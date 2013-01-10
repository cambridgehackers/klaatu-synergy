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

#define BUFSIZE     1024    /* size of buffer sent */
static unsigned char iobuffer[BUFSIZE];
static int socketfd;
static int trace;
#define VERSION_MAJOR 1
#define VERSION_MINOR 3

static unsigned char helloresp[] = {
    'S', 'y', 'n', 'e', 'r', 'g', 'y',
        0, VERSION_MAJOR,
        0, VERSION_MINOR,
        0, 0, 0, 0x07,
        'a', 'n', 'd', 'r', 'o', 'i', 'd'
};
static unsigned char dinf[] = {
    'D', 'I', 'N', 'F',
        0, 0, /* x */
        0, 0, /* y */
        0x1d, 0x10, /* w */
        0x04, 0x38, /* h */
        0, 0, /* 0 */
        0x07, 0x80, /* mx */
        0x02, 0x1c /* my */
};
static struct {
    int command;
    int count;
    union {
        int val;
        unsigned char *ptr;
    } param[10];
} indication;

void memdump(unsigned char *p, int len, char *title)
{
int i;

    i = 0;
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

static void senddata(const unsigned char *data, int len)
{
    int rc;
    *(int *)iobuffer = htonl(len);
    memcpy(&iobuffer[sizeof(int)], data, len);
    len += sizeof(int);
    if (trace)
        memdump(iobuffer, len, (char *)"Tx");
    if ( (rc = write(socketfd, iobuffer, len)) < 0 ) {
        perror("client: writing on socket stream");
        exit(1);
    }
    //printf("[%s:%d] after write %d\n", __FUNCTION__, __LINE__, rc);
}

typedef struct {
    int command;
    const char *name;
    const char *params;
} COMMANDINFO;

#define COMMANDS() \
    CC( CALV, NULL) \
    CC( CBYE, NULL) \
    CC( CCLP, "14") \
    CC( CIAK, NULL) \
    CC( CINN, "2242") \
    CC( CNOP, NULL) \
    CC( COUT, NULL) \
    CC( CROP, NULL) \
    CC( CSEC, "1") \
    CC( DCLP, "14S") \
    CC( DINF, "2222222") \
    CC( DKDN, "222") \
    CC( DKRP, "2222") \
    CC( DKUP, "222") \
    CC( DMDN, "1") \
    CC( DMMV, "22") \
    CC( DMUP, "1") \
    CC( DMWM, "22") \
    CC( DSOP, "S") \
    CC( QINF, NULL) \
    CC( Synergy, "22S")

#define CC(A,B) CMD_##A,
enum {CMD_NONE, COMMANDS() };
#undef CC
#define CC(A,B) {CMD_##A, #A, B},
static COMMANDINFO commands[] = { COMMANDS() { CMD_NONE, NULL, NULL} };
#undef CC

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
//printf ("len %x rc %x\n", len, rc);
    if ( rc < 0 || (rc = read(socketfd, iobuffer, len)) < 0 ) {
        printf("readdata: reading socket stream");
        exit(1);
    }
    if (!rc)
        return -1;
    if (rc != len) {
        printf ("******len %d rc %d\n", len, rc);
        memdump(iobuffer, rc, (char *)"Rx");
    }
    if (trace)
        memdump(iobuffer, rc, (char *)"Rx");

    while (cinfo->name && strncmp((char *)iobuffer, cinfo->name, strlen(cinfo->name)) )
        cinfo++;
    indication.command = cinfo->command;
    indication.count = 0;
    const char *param = cinfo->params;
    if (param) {
        unsigned char *datap = iobuffer + strlen(cinfo->name);
        while (*param) {
            int val = 0;
            int pch = *param++;
            switch(pch) {
            case 'S':
            case '4':
                val = (val << 8) | *datap++;
                val = (val << 8) | *datap++;
            case '2':
                val = (val << 8) | *datap++;
            case '1':
                indication.param[indication.count++].val = (val << 8) | *datap++;
                if (pch == 'S')
                    indication.param[indication.count++].ptr = datap;
                break;
            default:
                printf ("bad case in params: %s\n", cinfo->params);
                break;
            }
        }
    }
    if (cinfo->command && cinfo->command != CMD_CALV) {
        printf ("command %s rc %d count %d\n", commands[cinfo->command - 1].name, rc, indication.count);
        for (int i = 0; i < indication.count; i++)
            printf("param[%d]=%x\n", i, indication.param[i].val);
    }
    switch(cinfo->command) {
    case CMD_Synergy:
        senddata(helloresp, sizeof(helloresp));
        break;
    case CMD_QINF:
        senddata(dinf, sizeof(dinf));
        break;
    case CMD_CIAK:
    case CMD_CALV:
        senddata((unsigned char *)"CALV", 4);
        senddata((unsigned char *)"CNOP", 4);
        break;
    case CMD_NONE: {
        char bbb[5];
        memcpy (bbb, iobuffer, 4);
        printf("unknown '%s' ", bbb);
        goto dump_packet;
        }
    default:
        printf("undef '%s' ", commands[cinfo->command - 1].name);
dump_packet:
        if (rc > 4)
            memdump(&iobuffer[4], rc - 4, (char *)"Rx");
        else
            printf("\n");
        break;
    case CMD_CCLP:
    case CMD_CINN:
    case CMD_COUT:
    case CMD_CROP:
    case CMD_CSEC:
    case CMD_DCLP:
    case CMD_DKDN:
    case CMD_DKRP:
    case CMD_DKUP:
    case CMD_DMDN:
    case CMD_DMMV:
    case CMD_DMUP:
    case CMD_DMWM:
    case CMD_DSOP:
        break;
    case CMD_CBYE:
        return 1;
    }
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
    while (!readdata())
        ;
    close(socketfd);
    return 0;
}
