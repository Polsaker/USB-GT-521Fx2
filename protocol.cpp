#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <scsi/sg.h>

#include <iostream>

#include "protocol.hpp"

Protocol::Protocol(){}

uint16_t Protocol::calculate_checksum(Protocol::Packet* pkt){
	uint16_t wChkSum = 0;
	uint8_t* pBuf = (uint8_t*) pkt;
	for(int i = 0; i < (sizeof(Protocol::Packet) - 2); i++){
		wChkSum += pBuf[i];
    }
	return wChkSum;
}

Protocol::Packet Protocol::send_command(Protocol::Commands cmd, int params){
    Protocol::Packet pkt;
    int nSentBytes;

    pkt.head1 = (uint8_t) 0x55;
	pkt.head2 = (uint8_t) 0xAA;
	pkt.deviceID = 1;
	pkt.command = cmd;
	pkt.params = params;
	pkt.checksum = Protocol::calculate_checksum(&pkt);

    // Send command...
    if(Protocol::scsi_write(&pkt) < 0){
        // TODO: Handle ioctl error
    }

    Protocol::Packet reslt = Protocol::scsi_read();
    // TODO: Check if we actually got an ACK packet
    return reslt;
}

int Protocol::scsi_write(Protocol::Packet* pkt){
    //printf(" - Packet WRITE:\n  - HD1: %x\n  - HD2: %x\n  - CMD: %x\n  - PAR: %x\n  - CHK: %x\n\n", pkt->head1, pkt->head2, pkt->command, pkt->params, pkt->checksum);
    // TODO: Make a function to build the sg_io_hdr_t var and dedupe code
    sg_io_hdr_t *io_hdr = (sg_io_hdr_t *) malloc(sizeof(sg_io_hdr_t));
	memset(io_hdr, 0, sizeof(sg_io_hdr_t));

    // Magic values
    unsigned char Cdb[16] = {0xEF, 0xFE};

    io_hdr->interface_id = 'S';
    io_hdr->dxfer_direction = SG_DXFER_TO_DEV;
    io_hdr->timeout = 1000;

    io_hdr->cmdp = Cdb;
    io_hdr->cmd_len = 10;

    io_hdr->dxferp = pkt;
	io_hdr->dxfer_len = sizeof(Protocol::Packet);

    return ioctl(this->fd, SG_IO, io_hdr);
}

bool Protocol::read_data(void* buffer, int size) {
    uint8_t Buf[4],*cmdBuffer;
    int nReceivedBytes;

    cmdBuffer = new uint8_t[size + 6]; // HEADER(2) + DEV_ID(2) + CHKSUM(2) = 6

    nReceivedBytes = scsi_read_all(cmdBuffer, size + 6);

    if(nReceivedBytes != size +6){
        return false;
    }

    memcpy(Buf, cmdBuffer, 4);
	memcpy(buffer, cmdBuffer+4, size);

    if(Buf[0] != 0x5A || Buf[1] != 0xA5){
        return false;
    }
    return true;
}

int Protocol::scsi_read_all(uint8_t* buffer, int size){
    int sizeTemp = size, sizeReal;
    uint8_t* dataTmp = (uint8_t*) buffer;

    while(sizeTemp){
        sizeReal = sizeTemp;
		if (sizeReal > USB_BLOCK_SIZE)
			sizeReal = USB_BLOCK_SIZE;
        
        if(Protocol::scsi_read(dataTmp, sizeReal) < 0)
            break;
        
        dataTmp += sizeReal;
		sizeTemp -= sizeReal;
    }

    if (sizeTemp == 0)
		return size;
    return 0;
}

int Protocol::scsi_read(void* pkt, int size){
    sg_io_hdr_t *io_hdr = (sg_io_hdr_t *) malloc(sizeof(sg_io_hdr_t));
	memset(io_hdr, 0, sizeof(sg_io_hdr_t));

    // Magic values
    unsigned char Cdb[16] = {0xEF, 0xFF};

    io_hdr->interface_id = 'S';
    io_hdr->dxfer_direction = SG_DXFER_FROM_DEV;
    io_hdr->timeout = 1000;

    io_hdr->cmdp = Cdb;
    io_hdr->cmd_len = 10;

    io_hdr->dxferp = pkt;
	io_hdr->dxfer_len = size;
    int f = ioctl(this->fd, SG_IO, io_hdr);
    return f;
}

Protocol::Packet Protocol::scsi_read(){
    Protocol::Packet pkt;
    
    scsi_read(&pkt, sizeof(Protocol::Packet));
    //printf(" - Packet RECV:\n  - HD1: %x\n  - HD2: %x\n  - CMD: %x\n  - PAR: %x\n  - CHK: %x\n\n", pkt.head1, pkt.head2, pkt.command, pkt.params, pkt.checksum);
    return pkt;
}

CmdResult Protocol::validatePacket(Protocol::Packet pkt){
    if(pkt.head1 != 0x55 || pkt.head2 != 0xAA)
        return CmdResult::CMD_ERR_HEADER;

    if(pkt.checksum != Protocol::calculate_checksum(&pkt))
        return CmdResult::CMD_ERR_CHECKSUM;

    return CmdResult::CMD_OK;
}

ConnectResult Protocol::connect(const char* device){
    this->fd = open(device, O_RDWR);

    if (this->fd < 0){
        return CONNECT_ERR_OPEN;
    }

    if(Protocol::checkUsb() != CMD_OK){
        return CONNECT_ERR_INVALID_DEVICE;
    }

    _open();
    getModuleInfo();

    return CONNECT_OK;
}

CmdResult Protocol::checkUsb(){
    return validatePacket(send_command(Commands::CMD_USB_CHECK, 1));;
}

CmdResult Protocol::setLed(bool on){
    return validatePacket(send_command(Commands::CMD_CMOS_LED, on));
}

bool Protocol::isFingerPressed(){
    Protocol::Packet pkt = send_command(Commands::CMD_IS_PRESS_FINGER, 1);
    return pkt.params == 0;
}

CmdResult Protocol::_open(){
    Protocol::Packet pkt = send_command(Commands::CMD_OPEN, 1);

    if(read_data(&deviceInfo, sizeof(devinfo)))
        return CMD_OK;
    
    return CMD_READ_ERR;
}

/* Fetch module info. Required to receive images. */
CmdResult Protocol::getModuleInfo(){
    Protocol::Packet pkt = send_command(Commands::CMD_MODULE_INFO, 1);

    if(read_data(&moduleInfo, sizeof(moduleInfo)))
        return CMD_OK;
    
    return CMD_READ_ERR;
}

CmdResult Protocol::getRawImage(void* buffer){
    Protocol::Packet pkt = send_command(Commands::CMD_GET_RAWIMAGE, 0);
    if(read_data(buffer, moduleInfo.rawImgHeight * moduleInfo.rawImgWidth))
        return CMD_OK;
    
    return CMD_READ_ERR;
}


CmdResult Protocol::getImage(void* buffer){
    send_command(Commands::CMD_CAPTURE, 1);
    Protocol::Packet pkt = send_command(Commands::CMD_GET_IMAGE, 0);
    if(read_data(buffer, moduleInfo.imgHeight * moduleInfo.imgWidth))
        return CMD_OK;
    
    return CMD_READ_ERR;
}