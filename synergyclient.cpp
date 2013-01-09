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

static unsigned char helloresp[] = {
//%2i%2i%s
'S', 'y', 'n', 'e', 'r', 'g', 'y', 0, 0x01, 0, 0x03, 0, 0, 0, 0x07, 'a', 'n', 'd', 'r', 'o', 'i', 'd'
};
static unsigned char dinf[] = {
//info.m_x, info.m_y, info.m_w, info.m_h, 0, info.m_mx, info.m_my);
    'D', 'I', 'N', 'F', 0, 0, 0, 0, 0x1d, 0x10, 0x04, 0x38, 0, 0, 0x07, 0x80, 0x02, 0x1c
};
static struct {
    int command;
    int param[10];
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
    //memdump(iobuffer, len, (char *)"Tx");
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

enum {CMD_SYNERGY, CMD_QINF, CMD_CCLP, CMD_DCLP, CMD_DSOP,
    CMD_CALV, CMD_CIAK, CMD_CROP, CMD_CNOP, CMD_CINN, CMD_COUT, CMD_DMMV,
    CMD_DMDN, CMD_DMUP, CMD_DKDN, CMD_DKUP,
    };

static COMMANDINFO commands[] = {
    { CMD_SYNERGY, "Synergy", "22S"},
    { CMD_QINF,    "QINF", NULL},
    { CMD_CCLP,    "CCLP", NULL},
    { CMD_DCLP,    "DCLP", NULL},
    { CMD_DSOP,    "DSOP", NULL},
    { CMD_CALV,    "CALV", NULL},
    { CMD_CIAK,    "CIAK", NULL},
    { CMD_CROP,    "CROP", NULL},
    { CMD_CNOP,    "CNOP", NULL},
    { CMD_CINN,    "CINN", "2242"},
    { CMD_COUT,    "COUT", NULL},
    { CMD_DMMV,    "DMMV", "22"},
    { CMD_DMDN,    "DMDN", NULL},
    { CMD_DMUP,    "DMUP", NULL},
    { CMD_DKDN,    "DKDN", NULL},
    { CMD_DKUP,    "DKUP", NULL},
    { -1, NULL, NULL} };

static int readdata(void)
{
    int pindex = 0;
//static struct {
    //char *name;
    //int param[10];
//} indication;
    char *p = (char *)iobuffer;
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
    //memdump(iobuffer, rc, (char *)"Rx");

    while (cinfo->name && strncmp(p, cinfo->name, strlen(cinfo->name)) )
        cinfo++;
    indication.command = cinfo->command;
printf ("command %d rc %d\n", cinfo->command, rc);
    switch(cinfo->command) {
    case CMD_SYNERGY:
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
    default: {
        static char bbb[5];
        memcpy (bbb, iobuffer, 4);
        printf("unknown %d '%s' ", cinfo->command, bbb);
        rc -= sizeof(int) + 4;
        if (rc > 0)
            memdump(&iobuffer[sizeof(int) + 4], rc, (char *)"Rx");
        else
            printf("\n");
        break;
        }
    case CMD_DMMV:
    case CMD_CINN:
    case CMD_COUT:
    case CMD_DCLP:
    case CMD_DSOP:
    case CMD_CROP:
    case CMD_DMDN:
    case CMD_DMUP:
    case CMD_DKDN:
    case CMD_DKUP:
        break;
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
