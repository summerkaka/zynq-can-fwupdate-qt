#ifndef CAN_APP_H
#define CAN_APP_H


#define BITRATE         1000000

#define CANID_CHARGE    0x11
#define CANID_PUMP      0x12
#define CANID_MB        0x21

#define CMD_PING        0x00
#define CMD_DLD         0x01
#define CMD_SENDDATA    0x02
#define CMD_REQUEST     0x03
#define CMD_RESET       0x05
#define CMD_ASK_APPAREA 0xaa
#define CMD_JUMPTOAPP   0xab
#define CMD_JUMPTOBL    0xac
#define CMD_WRITECRC    0xad
#define CMD_PROGRAM_START   0xae
#define CMD_PROGRAM_END     0xaf

#define GetLongH(ptr)   (uint32_t)*(uint8_t *)(ptr) << 24 | \
                        (uint32_t)*((uint8_t *)(ptr) + 1) << 16 | \
                        (uint32_t)*((uint8_t *)(ptr) + 2) << 8  | \
                        (uint32_t)*((uint8_t *)(ptr) + 3)

#define GetLongL(ptr)   (uint32_t)*((uint8_t *)(ptr) + 3) << 24 | \
                        (uint32_t)*((uint8_t *)(ptr) + 2) << 16 | \
                        (uint32_t)*((uint8_t *)(ptr) + 1) << 8  | \
                        (uint32_t)*(ptr)

#define GetWordH(ptr)   (uint16_t)*(uint8_t *)(ptr) << 8  | \
                        (uint16_t)*((uint8_t *)(ptr) + 1)

#define GetWordL(ptr)   *((uint8_t *)(ptr) + 1) << 8  | \
                        *(uint8_t *)(ptr)

// ptr is h
#define WriteLongH(ptr, value)      do {*(uint8_t *)(ptr)       = (value) >> 24; \
                                        *(uint8_t *)((ptr) + 1) = (value) >> 16 & 0xff; \
                                        *(uint8_t *)((ptr) + 2) = (value) >> 8 & 0xff;  \
                                        *(uint8_t *)((ptr) + 3) = (value) & 0xff;       \
                                    } while (0)
// ptr is l
#define WriteLongL(ptr, value)      do {*(uint8_t *)((ptr) + 3) = (value) >> 24; \
                                        *(uint8_t *)((ptr) + 2) = (value) >> 16 & 0xff; \
                                        *(uint8_t *)((ptr) + 1) = (value) >> 8 & 0xff;  \
                                        *(uint8_t *)(ptr)       = (value) & 0xff;       \
                                    } while (0)
// ptr is h
#define WriteWordH(ptr, value)      do {*(uint8_t *)(ptr)       = (value) >> 8; \
                                        *(uint8_t *)((ptr) + 1) = (value);      \
                                    } while (0)
// ptr is l
#define WriteWordL(ptr, value)      do {*(uint8_t *)((ptr) + 1) = (value) >> 8; \
                                        *(uint8_t *)(ptr)       = (value);      \
                                    } while (0)


#define CMD_SHIFT   1
#define DEST_SHIFT  9
#define SRC_SHIFT   21

#define GET_ID_CMD(id)              (id & ((uint32_t)0xff << 1))

#define GET_ID_DEST(id)             (id & ((uint32_t)0xff << DEST_SHIFT))

#define GET_ID_SRC(id)              (id & ((uint32_t)0xff << 21))

#define WRITE_ID_CMD(id, value)     do {id &= ~((uint32_t)0xff << CMD_SHIFT);   \
                                        id |= value << CMD_SHIFT;               \
                                    } while (0)

#define WRITE_ID_DEST(id, value)    do {id &= ~((uint32_t)0xff << DEST_SHIFT);  \
                                        id |= value << DEST_SHIFT;              \
                                    } while (0)

#define WRITE_ID_SRC(id, value)     do {id &= ~((uint32_t)0xff << SRC_SHIFT);   \
                                        id |= value << SRC_SHIFT;               \
                                    } while (0)

#define IDE_FLAG    0x00080000u


typedef struct {
    uint32_t RTR        :1;  //0
    uint32_t CmdNum     :8;  //8:1
    uint32_t Target     :8;  //16:9
    uint32_t rsvd0      :2;  //18:17
    uint32_t IDE        :1;  //19
    uint32_t STR        :1;  //20
    uint32_t Src        :8;  //28:21
    uint32_t rsvd1      :3;  //31:29
} stCanId;

typedef union {
    uint32_t 	all;
    stCanId     field;
} tuCanId;




#endif // CAN_APP_H
