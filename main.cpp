#include "app_include.h"

#define WR_BUF_SIZE 128
#define RD_BUF_SIZE 128

static void usage_printf(char *prg)
{
    fprintf(stderr,
        "Usage: %s [<can-interface>] [Options] <can-msg>\n"
        "<can-msg> can consist of up to 8 bytes given as a space separated list\n"
        "Options:\n"
        " -d, --netdev name like ifconfig -a shows\n"
        " -f  --hex file    hex file path\n"
        " -h, --help        this help\n",
        prg);
}

uint16_t ASC2Hex(char *p_ascii)
{
    uint16_t result, h_byte, l_byte;

    result = 0;
    h_byte = 0;
    l_byte = 0;

    if (*p_ascii >= '0' && *p_ascii <= '9')
        h_byte = *p_ascii - '0';
    else if (*p_ascii >= 'A' && *p_ascii <= 'F')
        h_byte = *p_ascii - 'A' + 10;
    else if (*p_ascii >= 'a' && *p_ascii <= 'f')
        h_byte = *p_ascii - 'a' + 10;

    if (*(p_ascii + 1) >= '0' && *(p_ascii + 1) <= '9')
        l_byte = *(p_ascii + 1) - '0';
    else if (*(p_ascii + 1) >= 'A' && *(p_ascii + 1) <= 'F')
        l_byte = *(p_ascii + 1) - 'A' + 10;
    else if (*(p_ascii + 1) >= 'a' && *(p_ascii + 1) <= 'f')
        l_byte = *(p_ascii +1 ) - 'a' + 10;

    result = (h_byte << 4) + l_byte;
    return result;
}

int main(int argc, char *argv[])
{
    //    QApplication a(argc, argv);
    //    MainWindow w;
    //    w.show();
    
    //    return a.exec();

    int fd_can = 0, fd_hex = 0, ret = 0, i = 0;
    struct can_frame frame_rx;
    char *dev_name;
    char *hex_path;
    char buffer[RD_BUF_SIZE] = {0};
    char wdata[WR_BUF_SIZE] = {0};
    uint16_t length = 0, type = 0;
    uint32_t address = 0, addr_l = 0, addr_h = 0, base_addr = 0, can_id = 0, can_id_rx = 0;

    struct option long_options[] = {
        { "help",   no_argument,        0, 'h' },
        { "dev",    required_argument,  0, 'd' },
        { "file",   required_argument,  0, 'f' },
    };

    while ((ret = getopt_long(argc, argv, "hd:f:", long_options, NULL)) != -1) {
        switch (ret) {
        case 'h':
            usage_printf(basename(argv[0]));
            exit(0);
        case 'd':
            dev_name = optarg;
            break;
        case 'f':
            hex_path = optarg;
            break;
        default:
            fprintf(stderr, "Unknown option %c\n", ret);
            exit(EXIT_FAILURE);
            break;
        }
    }

    // open stream file to read hex file line by line
    if ((fd_hex = open(hex_path, O_RDONLY)) == -1){
        printf("hex file open failed\n");
        exit(EXIT_FAILURE);
    }
    FILE *fp = fdopen(fd_hex, "r");

    Gpio_Init();

    fd_can = CAN_Init(dev_name, BITRATE);
    if (fd_can <= 0) {
        perror("CAN_Init() fail\n");
        exit(EXIT_FAILURE);
    }

//    do {
//        can_id = 0xaaaaaaaa;
//        wdata[0] = 0xdb;
//        wdata[1] = 0x24;
//        ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 2, 5);
//        printf("CAN_SendFrame() send %d bytes for test, id: %x, byte0: %x, byte1: %x\n", ret, can_id, wdata[0], wdata[1]);
//        sleep(1);
//    } while (1);

    // read charge board status, bl or app
    WRITE_ID_CMD(can_id, CMD_PING);
    WRITE_ID_DEST(can_id, CANID_CHARGE);
    WRITE_ID_SRC(can_id, CANID_MB);
    can_id |= CAN_EFF_FLAG;
    printf("can_id is %x\n", can_id);
    wdata[0] = 0x11;
    wdata[1] = 0x22;
    ret = 1;
    do {
        ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 2, 5);
        printf("send %d bytes to check cb status\n", ret);
        CAN_RecvFrame(fd_can, &frame_rx, 500);
        can_id_rx = frame_rx.can_id & CAN_EFF_MASK;
        if (((stCanId*)&can_id_rx)->Target == CANID_MB &&
                ((stCanId*)&can_id_rx)->CmdNum == CMD_PING &&
                frame_rx.data[0] == 0xa5)
            break;
    } while (1);
    printf("cb is in bootloader\n");

    // if in bl, request to erase app flash area
    do {
        WRITE_ID_CMD(can_id, CMD_ERASE);
        ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5);
        printf("send %d bytes to erase flash\n", ret);
        CAN_RecvFrame(fd_can, &frame_rx, 5);
        can_id_rx = frame_rx.can_id & CAN_EFF_MASK;
        if (((stCanId*)&can_id_rx)->Target == CANID_MB &&
                ((stCanId*)&can_id_rx)->CmdNum == CMD_ERASE &&
                frame_rx.data[0] == 0x02)
            break;
    } while (1);
    printf("app flash erased\n");

    do {
        fgets(buffer, WR_BUF_SIZE, fp);
        if(ferror(fp)) {
            printf("Error read line from hex file\n");
            goto PROGRAM_FAIL;
        }

        // read header ":"
        if (*buffer != 58) {
            printf("line sof is not right\n");
            goto PROGRAM_FAIL;
        }

        // read length
        length = ASC2Hex(&buffer[1]);
        printf("length is %d, ", length);

        // read address
        addr_h = ASC2Hex(&buffer[3]);
        addr_l = ASC2Hex(&buffer[5]);
        address = (addr_h << 8) + addr_l + base_addr;
        printf("address is %x, ", address);

        // read  type
        type = ASC2Hex(&buffer[7]);
        printf("type is %d\n", type);

        switch(type) {
        case 0x00:
            // send start address and data length first
            WRITE_ID_CMD(can_id, CMD_DLD);
            WRITE_MSG_LONG(wdata, address);
            WRITE_MSG_LONG(wdata + 4, length);
            ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 8, 5);
            printf("send %d bytes DLD command \n", ret);
            ret = CAN_RecvFrame(fd_can, &frame_rx, 5);

            // send data
            for (i = 0; i < length; i++) {
                wdata[i] = ASC2Hex(buffer + 2*i + 9);
            }
            for (i = 0; i < length; ) {
                if (length - i >= 8) {
                    ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)&wdata[i], 8, 5);
                    i += 8;
                    printf("send %d bytes to program flash\n", ret);
                    ret = CAN_RecvFrame(fd_can, &frame_rx, 5);
                }else if (length - i == 4) {
                    ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)&wdata[i], 4, 5);
                    i += 4;
                    printf("send %d bytes to program flash\n", ret);
                    ret = CAN_RecvFrame(fd_can, &frame_rx, 5);
                }
            }
            break;
        case 0x01:
            WRITE_ID_CMD(can_id, CMD_JUMPTOAPP);
            ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5);
            break;
        case 0x02:
            break;
        case 0x03:
            break;
        case 0x04:
            memcpy(wdata, buffer+9, 4);
            wdata[4] = '\0';
            base_addr = atoi(wdata) << 16;
            break;
        default : break;
        }
    } while (feof(fp));
    printf("reach eof of hex\n");

    fclose(fp);
    close(fd_hex);
    CAN_DeInit(fd_can);
    close(fd_can);
    exit(EXIT_SUCCESS);

PROGRAM_FAIL:
    CAN_DeInit(fd_can);
    clearerr(fp);
    fclose(fp);
    close(fd_hex);
    close(fd_can);
    exit(EXIT_FAILURE);
}
