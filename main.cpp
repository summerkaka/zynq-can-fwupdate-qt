#include "app_include.h"

#define WR_BUF_SIZE 128
#define RD_BUF_SIZE 128
#define FLASHFLAGADDR 0x0801fc00

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

static uint16_t ASC2Hex(const char *p_ascii)
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

    int fd_can = 0, fd_hex = 0, ret = 0, line_num;
    uint8_t send_dlc = 0;
    struct can_frame frame_rx;
    uint8_t target_id = 0;
    char can_device_name[] = "can0";
    char *dev_name;
    char *hex_path;
    char buffer[RD_BUF_SIZE] = {0};
    char wdata[WR_BUF_SIZE] = {0};
    uint32_t address = 0, addr_l = 0, addr_h = 0, base_addr = 0, can_id = 0, length = 0, type = 0, i = 0;
    uint32_t flash_start = 0, flash_end = 0;
    struct option long_options[] = {
        { "help",   no_argument,        0, 'h' },
        { "dev",    required_argument,  0, 'd' },
        { "file",   required_argument,  0, 'f' },
        { "target", required_argument,  0, 't' }
    };

    dev_name = can_device_name;

    while ((ret = getopt_long(argc, argv, "hd:f:t:", long_options, NULL)) != -1) {
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
        case 't':
            if (strcmp(optarg, "cb") == 0) {
                target_id = CANID_CHARGE;
            }else if (strcmp(optarg, "pump") == 0) {
                target_id = CANID_PUMP;
            }
            break;
        default:
            fprintf(stderr, "Unknown option %c\n", ret);
            exit(EXIT_FAILURE);
            break;
        }
    }
    if (target_id == 0) {
        perror("fw update target is not input\n");
        exit(EXIT_FAILURE);
    }

    // open stream file to read hex file line by line
    if ((fd_hex = open(hex_path, O_RDONLY)) == -1){
        perror("hex file open failed\n");
        exit(EXIT_FAILURE);
    }
    FILE *fp = fdopen(fd_hex, "r");

    Gpio_Init();

    fd_can = CAN_Init(dev_name, BITRATE);
    if (fd_can <= 0) {
        perror("CAN_Init() fail\n");
        exit(EXIT_FAILURE);
    }

    // read charge board status, request to jump to bl if it's in app
    WRITE_ID_DEST(can_id, target_id);
    WRITE_ID_SRC(can_id, CANID_MB);
    can_id |= IDE_FLAG;
    can_id |= CAN_EFF_FLAG;
    ret = 0;
    i = 0;
    do {
        WRITE_ID_CMD(can_id, CMD_PING);
        if (CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5) > 0)
            printf("send request to check cb status\n");
        else {
            perror("fail to check cb status\n");
            goto PROGRAM_FAIL;
        }
        if (CAN_RecvFrame(fd_can, &frame_rx, 5) < 0 ||
                ((stCanId*)&frame_rx.can_id)->Target != CANID_MB ||
                ((stCanId*)&frame_rx.can_id)->CmdNum != CMD_PING) {
            perror("no response to check status\n");
            goto PROGRAM_FAIL;
        }
        if (frame_rx.data[0] == 0xa5) {
            printf("cb is in bootloader\n");
            break;
        }else
            printf("cb is in app mode\n");

        WRITE_ID_CMD(can_id, CMD_JUMPTOBL);
        if (CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5) < 0) {
            perror("fail to send request to jump to bl\n");
            goto PROGRAM_FAIL;
        }else
            printf("request sent to jump to bl\n");
        if (CAN_RecvFrame(fd_can, &frame_rx, 5) < 0 ||
                ((stCanId*)&frame_rx.can_id)->Target != CANID_MB ||
                ((stCanId*)&frame_rx.can_id)->CmdNum != CMD_JUMPTOBL) {
            perror("no response to jump to bl\n");
            goto PROGRAM_FAIL;
        }else {
            i++;
            printf("cb is jumping to bl......\n");
            sleep(5);
        }
    } while (i < 3);
    if (i >= 3) {
        printf("fail to jump to bl\n");
        goto PROGRAM_FAIL;
    }

    // ask target MCU flash area of application, hex file base_address is compared with the area
    WRITE_ID_CMD(can_id, CMD_ASK_APPAREA);
    if (CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5) > 0) {
        printf("send command to ask app flash area\n");
        ret = CAN_RecvFrame(fd_can, &frame_rx, 20);
        if (ret > 0 &&
                ((stCanId*)&frame_rx.can_id)->Target == CANID_MB &&
                ((stCanId*)&frame_rx.can_id)->CmdNum == CMD_ASK_APPAREA) {
            for (i = 0; i < 8; i++)
                printf("%x  ", *(&frame_rx.data[0] + i));
            printf("\n");
            printf("%x  ", *(&frame_rx.data[0]) << 24);
            printf("%x  ", *(&frame_rx.data[0] + 1) << 16);
            printf("%x  ", *(&frame_rx.data[0] + 2) << 8);
            printf("\n");
            flash_start = GetLongH(frame_rx.data);
            flash_end = GetLongH(&frame_rx.data[4]);
            printf("flash start address: %x, end address: %x\n", flash_start, flash_end);
        }else {
            printf("fail to get flash area\n");
        }
    }else {
        perror("fail to send command to get flash area\n");
        goto PROGRAM_FAIL;
    }

    // send 'program start' to tell mcu to release flash protection.
    WRITE_ID_CMD(can_id, CMD_PROGRAM_START);
    i = 0;
    do {
        ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 2, 5);
        printf("send %d bytes to unlock and erase flash\n", ret);
        if (CAN_RecvFrame(fd_can, &frame_rx, 5000) > 0 &&
                ((stCanId*)&frame_rx.can_id)->Target == CANID_MB &&
                ((stCanId*)&frame_rx.can_id)->CmdNum == CMD_PROGRAM_START &&
                frame_rx.data[0] == 0x00)
            break;
        else {
            i++;
            usleep(5000);
        }
    } while (i < 3);
    if (i < 3) {
        printf("flash is prepared\n");
    }else {
        printf("cb no respond to start program\n");
        goto PROGRAM_FAIL;
    }

    // read hex file line by line to program flash
    line_num = 0;
    do {
        fgets(buffer, WR_BUF_SIZE, fp);
        printf("line %d: %s", line_num++, buffer);
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
        sscanf(&buffer[1], "%2x", &length);
        printf("length is %d, ", length);

        // read address
        sscanf(&buffer[3], "%4x", &address);
        address += base_addr;
        printf("address is %x, ", address);

        // read  type
        sscanf(&buffer[7], "%2x", &type);
        printf("type is %d\n", type);

        switch(type) {
        case 0x00:
            // send start address and data length first
            if (address < flash_start || address > flash_end) {
                printf("exceed app flash area, wrong hex file\n");
                goto PROGRAM_FAIL;
            }
            WRITE_ID_CMD(can_id, CMD_DLD);
            WriteLongH(wdata, address);
            WriteLongH(wdata + 4, length);
            if (CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 8, 5) > 0)
                printf("send DLD command, length is %d, address is %x\n", length, address);
            else {
                perror("fail to send DLD command");
                goto PROGRAM_FAIL;
            }
            if (CAN_RecvFrame(fd_can, &frame_rx, 5) > 0) {
                if (((stCanId*)&frame_rx.can_id)->Target != CANID_MB ||
                        ((stCanId*)&frame_rx.can_id)->CmdNum != CMD_DLD ||
                        frame_rx.data[0] != 0x00) {
                    perror("no ack to DLD command\n");
                    goto PROGRAM_FAIL;
                }
            }

            // send data
            WRITE_ID_CMD(can_id, CMD_SENDDATA);
            for (i = 0; i < length; i++) {
//                wdata[i] = ASC2Hex(buffer + 2*i + 9);
                sscanf(&buffer[9] + 2*i, "%2x", (uint32_t*)&wdata[i]);
            }

            for (i = 0; i < length; ) {
                send_dlc = (length - i >= 8) ? 8 : length - i;
                    ret = CAN_SendFrame(fd_can, can_id, (const uint8_t *)&wdata[i], send_dlc, 5);
                if (ret > 0)
                    i += send_dlc; 
                if (CAN_RecvFrame(fd_can, &frame_rx, 20) < 0 ) {
                    printf("fail to program flash\n");
                    goto PROGRAM_FAIL;
                }else if (((stCanId*)&frame_rx.can_id)->Target != CANID_MB ||
                          ((stCanId*)&frame_rx.can_id)->CmdNum != CMD_SENDDATA ||
                          frame_rx.data[0] != 0x00) {
                    printf("fail to program flash\n");
                    goto PROGRAM_FAIL;
                }else
                    printf("%d bytes has been programmed to flash\n", send_dlc);
            }
            break;
        case 0x01:
            WRITE_ID_CMD(can_id, CMD_WRITECRC);
            WriteLongL(wdata, 0x12345678);
            CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 4, 5);
            ret = CAN_RecvFrame(fd_can, &frame_rx, 10);
            if (ret > 0 &&
                    ((stCanId*)&frame_rx.can_id)->Target == CANID_MB &&
                    ((stCanId*)&frame_rx.can_id)->CmdNum == CMD_WRITECRC &&
                    frame_rx.data[0] == 0x00) {
                printf("write CRC to MCU flash succeed \n");
            }else {
                printf("fail to write CRC into flash, \n");
                goto PROGRAM_FAIL;
            }
            WRITE_ID_CMD(can_id, CMD_PROGRAM_END);
            if (CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5) > 0) {
                printf("send command to end flash program\n");
                ret = CAN_RecvFrame(fd_can, &frame_rx, 20);
                if (ret > 0 &&
                        ((stCanId*)&frame_rx.can_id)->Target == CANID_MB &&
                        ((stCanId*)&frame_rx.can_id)->CmdNum == CMD_PROGRAM_END &&
                        frame_rx.data[0] == 0x00) {
                    printf("target flash is locked...\n");
                }else {
                    perror("target lock flash fail\n");
                    goto PROGRAM_FAIL;
                }
            }else {
                perror("fail to send command to lock flash\n");
                goto PROGRAM_FAIL;
            }
            WRITE_ID_CMD(can_id, CMD_JUMPTOAPP);
            if (CAN_SendFrame(fd_can, can_id, (const uint8_t *)wdata, 0, 5) > 0) {
                printf("send command to jump to application\n");
                ret = CAN_RecvFrame(fd_can, &frame_rx, 20);
                if (ret > 0 &&
                        ((stCanId*)&frame_rx.can_id)->Target == CANID_MB &&
                        ((stCanId*)&frame_rx.can_id)->CmdNum == CMD_JUMPTOAPP &&
                        frame_rx.data[0] == 0x00) {
                    printf("cb is jumping to application...\n");
                }else {
                    printf("target fail to jump to app\n");
                }
            }else {
                perror("fail to send command to jump to app\n");
                goto PROGRAM_FAIL;
            }
            break;
        case 0x02:
            break;
        case 0x03:
            break;
        case 0x04:
            snprintf(wdata, 3, buffer+9);
            addr_h = atoi(wdata);
            snprintf(wdata, 3, buffer+11);
            addr_l = atoi(wdata);
            base_addr = ((addr_h << 8) + addr_l) << 16;
            printf("base_address is %x\n", base_addr);
            // todo compare with 0x0800 0000
//            if (base_addr < flash_start || base_addr > flash_end) {
//                printf("wrong hex file\n");
//                goto PROGRAM_FAIL;
//            }
            break;
        case 0x05:
            break;
        default : break;
        }
    } while (!feof(fp) && type != 0x01);
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
