#include "protocol.cpp"
#include <iostream>


int pbm(const char * filename, int width, int height, void* data){
    FILE *fp;
    if (!(fp = fopen(filename,"wb"))) return 0;
    fprintf(fp, "P5\n%d %d\n%d\n", width, height, 0xFF);
    
    fwrite((uint8_t *) data, 1, width * height, fp);
    return 1;
}


int main(){
    // 1. Connect to device
    char cDeviceName[] = "/dev/sg1";
    
    Protocol fp;
    std::cout << "Connecting to device...." << std::endl;
    if(fp.connect(cDeviceName) == CONNECT_OK){
        std::cout << "Connected!" << std::endl;
        std::cout << "Device Info:" << std::endl;
        std::cout << " - FW Ver.: " << std::hex << fp.deviceInfo.FirmwareVersion << std::endl 
                  << " - S/N: ";
        for(int i=0; i < 16; i++) std::cout << (int)fp.deviceInfo.DeviceSerialNumber[i];
        std::cout << std::endl;

        std::cout << "Module Info:" << std::endl
                  << " - Sensor: " << fp.moduleInfo.sensor << std::endl
                  << " - Engine ver.: " << fp.moduleInfo.engineVer << std::endl << std::dec
                  << " - Raw image area (w x h): " << (int)fp.moduleInfo.rawImgWidth << " * " << (int)fp.moduleInfo.rawImgHeight << std::endl
                  << " - Image area (w x h): " << (int)fp.moduleInfo.imgWidth << " * " << (int)fp.moduleInfo.imgHeight << std::endl
                  << " - Record count: " << (int)fp.moduleInfo.enrollCnt << " / " << (int)fp.moduleInfo.maxEnrollCnt << std::endl
                  << " - Template size: " << (int)fp.moduleInfo.templateSize << std::endl;
    }else{
        printf("Connect error\n");
        printf("Open error, errno=%d (%s)\n", errno, strerror(errno));
        return 1;
    }
    fp.setLed(true);
    std::cout << "Press your fingerprint" << std::endl;
    int a = 0;
    while(1) {
        a++;
        sleep(0.5);
        if(fp.isFingerPressed()){
            std::cout << "Found ya!" << std::endl;
            uint8_t rawImage[fp.moduleInfo.rawImgWidth * fp.moduleInfo.rawImgHeight];
            fp.getRawImage(&rawImage);
            pbm("./fingerprint_raw.pbm", fp.moduleInfo.rawImgWidth, fp.moduleInfo.rawImgHeight, rawImage);

            uint8_t image[fp.moduleInfo.imgWidth * fp.moduleInfo.imgHeight];
            fp.getImage(&image);
            pbm("./fingerprint.pbm", fp.moduleInfo.imgWidth, fp.moduleInfo.imgHeight, image);
            
            std::cout << "Wrote fingerprint images to fingerprint.pbm and fingerprint_raw.pbm" << std::endl;
            fp.setLed(false);
            break;
        }
    }
    return 0;
}
