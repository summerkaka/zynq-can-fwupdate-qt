#ifndef APP_INCLUDE_H
#define APP_INCLUDE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <getopt.h>
#include <libgen.h>
#include <unistd.h>

#include <net/if.h>
#include <linux/can.h>
#include <linux/can/raw.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>



#include "can_app.h"
#include "can_if.h"
#include "gpio.h"
#include "libsocketcan.h"

//#include "mainwindow.h"
//#include <QApplication>
//#include <QMessageBox>

#define USE_QTGUI  0
#define ZC702
//#define HMBIRD_MB


#ifdef __cplusplus
}
#endif

#endif // APP_INCLUDE_H
