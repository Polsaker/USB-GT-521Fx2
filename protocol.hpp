#pragma once
#include <stdint.h>

// Header Of Cmd and Ack Packets
#define STX1				0x55	//Header1 
#define STX2				0xAA	//Header2

// Header Of Data Packet
#define STX3				0x5A	//Header1 
#define STX4				0xA5	//Header2

#define USB_BLOCK_SIZE      65536
#define IMAGE_SIZE_MAX		640*480

enum ConnectResult {
    CONNECT_OK = 0,
    CONNECT_ERR_OPEN = 0x1000,
    CONNECT_ERR_INVALID_DEVICE
};

enum CmdResult {
    CMD_OK = 0,
    CMD_ERR,
    CMD_ERR_CHECKSUM,
    CMD_ERR_HEADER,
    CMD_READ_ERR
};

typedef struct _devinfo {
    uint32_t FirmwareVersion;
    uint32_t IsoAreaMaxSize;
    uint8_t DeviceSerialNumber[16];
} devinfo;

typedef struct _moduleinfo {
	uint8_t		    sensor[12];
	uint8_t		    engineVer[12];
	uint16_t		rawImgWidth;
	uint16_t		rawImgHeight;
    uint16_t		imgHeight;
	uint16_t		imgWidth;
	uint16_t		maxEnrollCnt;
	uint16_t		enrollCnt;
	uint16_t		templateSize;
	
} moduleinfo;

class Protocol {
    public: 
        devinfo deviceInfo;
        moduleinfo moduleInfo;

        Protocol();
        ConnectResult connect(const char* device);

        CmdResult checkUsb();
        CmdResult setLed(bool on);
        CmdResult getImage(void*);
        CmdResult getRawImage(void*);
        bool isFingerPressed();

    private:
        int fd;
        typedef struct {
            uint8_t 	head1;
            uint8_t 	head2;
            uint16_t	deviceID;
            int		    params;
            uint16_t	command;
            uint16_t 	checksum;
        } Packet;

        enum Commands {
            CMD_OPEN                = 0x01,
            CMD_CLOSE               = 0x02,
            CMD_USB_CHECK           = 0x03,
            CMD_CHG_BAUDRATE        = 0x04,
            CMD_MODULE_INFO         = 0x06,

            CMD_CMOS_LED            = 0x12,

            CMD_IS_PRESS_FINGER     = 0x26,

            CMD_CAPTURE				= 0x60,
            CMD_GET_IMAGE			= 0x62,
	        CMD_GET_RAWIMAGE		= 0x63
        };

        CmdResult _open();
        CmdResult getModuleInfo();

        Packet send_command(Commands command, int params);

        uint16_t calculate_checksum(Packet*);

        int scsi_write(Packet*);
        Packet scsi_read();
        int scsi_read(void* pkt, int size);
        int scsi_read_all(uint8_t* buffer, int size);
        bool read_data(void* buffer, int size);
        
        CmdResult validatePacket(Packet);
};
