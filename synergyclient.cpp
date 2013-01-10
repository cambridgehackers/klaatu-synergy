/*
 Permission is hereby granted, free of charge, to any person obtaining a
 copy of this software and associated documentation files (the "Software"),
 to deal in the Software without restriction, including without limitation
 the rights to use, copy, modify, merge, publish, distribute, sublicense,
 and/or sell copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included
 in all copies or substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 DEALINGS IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>
#include <stdarg.h>

#define VERSION_MAJOR 1
#define VERSION_MINOR 3
#define BUFSIZE     100000    /* size of buffer sent */
#define COMMANDS() \
    CC(CALV, NULL) CC(CBYE, NULL) CC(CCLP, "14") CC(CIAK, NULL) \
    CC(CINN, "2242") CC(CNOP, NULL) CC(COUT, NULL) CC(CROP, NULL) \
    CC(CSEC, "1") CC(DCLP, "14S") CC(DINF, "2222222") \
    CC(DKDN, "222") CC(DKRP, "2222") CC(DKUP, "222") \
    CC(DMDN, "1") CC(DMMV, "22") CC(DMUP, "1") CC(DMWM, "22") \
    CC(DSOP, "S") CC(QINF, NULL) CC(Synergy, "22S")

typedef struct {
    int command;
    const char *name;
    const char *params;
} COMMANDINFO;

typedef struct {
    int command;
    int count;
    int remain;
    unsigned char *str;
    int param[10];
} INDICATIONINFO;

static unsigned char iobuffer[BUFSIZE];
static int socketfd;
static int trace = 0;
static INDICATIONINFO indication;

#define CC(A,B) CMD_##A,
enum {CMD_NONE, COMMANDS() };
#undef CC
#define CC(A,B) {CMD_##A, #A, B},
static COMMANDINFO commands[] = { COMMANDS() { CMD_NONE, NULL, NULL} };
#undef CC

static void memdump(unsigned char *p, int len, const char *title)
{
    int i = 0;
    while (len > 0) {
        if (!(i & 0xf)) {
            if (i > 0)
                printf("\n");
            printf("%s: ",title);
        }
        printf("%02x ", *p++);
        i++;
        len--;
    }
    printf("\n");
}

static void senddata(int command, ...)
{
    int rc;
    char *p = (char *)iobuffer + sizeof(int);
    const char *param = commands[command-1].params;
    va_list args;
    va_start(args, command);
    strcpy(p, commands[command-1].name);
    p += strlen(commands[command-1].name);
    while (param && *param) {
        int val = 0;
        char *ptr = NULL;
        int pch = *param++;
        int count = pch - '0';
        if(pch == 'S') {
            ptr = va_arg(args, char *);
            val = strlen(ptr);
            count = 4;
        }
        else
            val = va_arg(args, int);
        switch(pch) {
        case '1':
            val <<= 8;
        case '2':
            val <<= 16;
        case '4': case 'S':
            while (count--) {
                *p++ = (val >> 24);
                val <<= 8;
            }
            break;
        default:
            printf ("bad case in params: %s\n", commands[command-1].params);
            break;
        }
        if (ptr) {
            strcpy(p, ptr);
            p += strlen(ptr);
        }
    }
    va_end(args);
    int len = p - (char *)iobuffer;
    *(int *)iobuffer = htonl(len - sizeof(int));
    if (trace)
        memdump(iobuffer, len, "Tx");
    if ( (rc = write(socketfd, iobuffer, len)) < 0 ) {
        perror("client: writing on socket stream");
        exit(1);
    }
}

static int readdata(void)
{
    int pindex = 0;
    char *pdata = (char *)iobuffer;
    COMMANDINFO *cinfo = commands;
    int len = 0;
    int rc = read(socketfd, &len, sizeof(len));
    if (!rc)
        return -1;
    len = ntohl(len);
    if ( rc < 0 || (rc = read(socketfd, iobuffer, len)) < 0 ) {
        printf("readdata: reading socket stream");
        exit(1);
    }
    if (!rc)
        return -1;
    if (rc != len)
        printf ("readdata: error in read len %d rc %d\n", len, rc);
    if (trace)
        memdump(iobuffer, rc, "Rx");

    while (cinfo->name && strncmp((char *)iobuffer, cinfo->name, strlen(cinfo->name)) )
        cinfo++;
    indication.command = cinfo->command;
    indication.count = 0;
    const char *param = cinfo->params;
    unsigned char *datap = iobuffer;
    if (cinfo->name)
        datap += strlen(cinfo->name);
    while (param && *param && datap < iobuffer + len) {
        int val = 0;
        int pch = *param++;
        switch(pch) {
        case '4': case 'S':
            val = (val << 8) | *datap++;
            val = (val << 8) | *datap++;
        case '2':
            val = (val << 8) | *datap++;
        case '1':
            indication.param[indication.count++] = (val << 8) | *datap++;
            break;
        default:
            printf ("bad case in params: %s\n", cinfo->params);
            break;
        }
    }
    indication.str = datap;
    indication.remain = iobuffer + len - datap;
    return 0;
}

int main(int argc, char **argv)
{
    struct sockaddr_in server;

    server.sin_family = AF_INET;
    server.sin_port = htons(24800);
    //socketfd = socket(AF_INET, SOCK_STREAM, 0); 
    socketfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP); 
    struct hostent *h = gethostbyname("127.0.0.1");
    if ( socketfd < 0 || !h ) {
        fprintf(stderr,"synergyclient: cannot initialize\n");
        exit(2);
    }
    memcpy(&server.sin_addr, h->h_addr, h->h_length);
    if (connect(socketfd, (const sockaddr *)&server, sizeof(server)) < 0 ) {
        perror("synergyclient: error in connect");
        exit(1);
    }
    printf("synergyclient: connected\n");
    while (!readdata()) {
        if (indication.command && indication.command != CMD_CALV) {
            printf ("command %s :", commands[indication.command - 1].name);
            for (int i = 0; i < indication.count; i++)
                printf("0x%x=%d., ", indication.param[i], indication.param[i]);
            printf("\n");
            if (indication.remain)
                memdump(indication.str, indication.remain, "REM");
        }
        switch(indication.command) {
        case CMD_Synergy:
            senddata(CMD_Synergy, VERSION_MAJOR, VERSION_MINOR, "android");
            break;
        case CMD_QINF:
            senddata(CMD_DINF, 0/* x */, 0/* y */,
                7440/* w */, 1080/* h */,
                0/* 0 */, 1920/* mx */, 540/* my */);
            break;
        case CMD_CIAK:
        case CMD_CALV:
            senddata(CMD_CALV);
            senddata(CMD_CNOP);
            break;
        case CMD_NONE: {
            char bbb[5];
            memcpy (bbb, iobuffer, 4);
            printf("unknown '%s'\n", bbb);
            break;
            }
        default:
            printf("undef '%s'\n", commands[indication.command - 1].name);
            break;
        case CMD_CCLP: case CMD_CINN: case CMD_COUT: case CMD_CROP: case CMD_CSEC:
        case CMD_DCLP: case CMD_DKDN: case CMD_DKRP: case CMD_DKUP:
        case CMD_DMDN: case CMD_DMMV: case CMD_DMUP: case CMD_DMWM: case CMD_DSOP:
            break;
        case CMD_CBYE:
            return 1;
        }
    }
    close(socketfd);
    return 0;
}
#ifdef FORANDROID
//#include <string.h>
//#include <stdlib.h>
//#include <unistd.h>
//#include <fcntl.h>
//#include <stdio.h>
//#include <sys/ioctl.h>
//#include <sys/mman.h>
//#include <sys/types.h>
//#include <time.h>
//#include <linux/fb.h>
//#include <linux/kd.h>
//#include <android/log.h>
#include <linux/input.h>

#define TAG "Synergy"
#define devicename "/dev/input/event3"
static int inputfd = -1;

static void initialize()
{
    struct uinput_dev dev;
int mouse = 0;
int keyboard = 1;
    
    inputfd = open(devicename, O_RDWR);
    if (inputfd < 0) {
        printf("Can't open input device:%s \n", devicename);
        return -1;
    }
    memset(&dev, 0, sizeof(dev));
    strcpy(dev.name, "Synergy");
    dev.id.bustype = 0x0003;// BUS_USB;
    dev.id.vendor  = 0x0000;
    dev.id.product = 0x0000;
    dev.id.version = 0x0000;
    if (write(inputfd, &dev, sizeof(dev)) < 0) {
        printf("Can't write device information");
        close(inputfd);
        return -1;
    }
    if (mouse) {
        ioctl(inputfd, UI_SET_EVBIT, EV_REL);
        for (int aux = REL_X; aux <= REL_MISC; aux++)
            ioctl(inputfd, UI_SET_RELBIT, aux);
    }
    if (keyboard) {
        ioctl(inputfd, UI_SET_EVBIT, EV_KEY);
        ioctl(inputfd, UI_SET_EVBIT, EV_LED);
        ioctl(inputfd, UI_SET_EVBIT, EV_REP);
        for (int aux = KEY_RESERVED; aux <= KEY_UNKNOWN; aux++)
            ioctl(inputfd, UI_SET_KEYBIT, aux);
        //for (int aux = LED_NUML; aux <= LED_MISC; aux++)
        //    ioctl(inputfd, UI_SET_LEDBIT, aux);
    }
    if (mouse) {
        ioctl(inputfd, UI_SET_EVBIT, EV_KEY);
        for (int aux = BTN_LEFT; aux <= BTN_BACK; aux++)
            ioctl(inputfd, UI_SET_KEYBIT, aux);
    }
    ioctl(inputfd, UI_DEV_CREATE);
    printf("intCreate success: %d\n",  inputfd);
}

void send_input_event(uint16_t type, uint16_t code, int32_t value)
{
    struct uinput_event event;

    printf("intSendEvent call (%d,%d,%d,%d)\n", type, code, value);
    if (inputfd < 0)
        return;
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.code = code;
    event.value = value;
    int len = write(inputfd, &event, sizeof(event));
    printf("intSendEvent done:%d\n",len);
} 
#endif
