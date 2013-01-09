
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <unistd.h>

#define BUFSIZE     1024    /* size of buffer sent */
static unsigned char iobuf[BUFSIZE];

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

static unsigned char helloresp[] = {
//Synergy%2i%2i%s
'S', 'y', 'n', 'e', 'r', 'g', 'y', 0, 0x01, 0, 0x03, 0, 0, 0, 0x07, 'a', 'n', 'd', 'r', 'o', 'i', 'd'
};
static unsigned char dinf[] = {
//DINF%2i%2i%2i%2i%2i%2i%2i" kMsgDInfo, info.m_x, info.m_y, info.m_w, info.m_h, 0, info.m_mx, info.m_my);
    'D', 'I', 'N', 'F', 0, 0, 0, 0, 0x1d, 0x10, 0x04, 0x38, 0, 0, 0x07, 0x80, 0x02, 0x1c
};

static int sock;
static void senddata(const unsigned char *data, int len)
{
    int rc;
    *(int *)iobuf = htonl(len);
    memcpy(&iobuf[sizeof(int)], data, len);
    len += sizeof(int);
    //memdump(iobuf, len, (char *)"Tx");
    if ( (rc = write(sock, iobuf, len)) < 0 ) {
        perror("client: writing on socket stream");
        exit(1);
    }
    //printf("[%s:%d] after write %d\n", __FUNCTION__, __LINE__, rc);
}

int main(int argc, char **argv)
{
    struct sockaddr_in server;
    int rc;

    sock = socket(AF_INET, SOCK_STREAM, 0); 
    if ( sock < 0 ) {
        perror("socket");
        exit(1);
    }
    server.sin_family = AF_INET;
    struct hostent *h = gethostbyname("127.0.0.1");
    if ( !h ) {
        fprintf(stderr,"gethostbyname: cannot find host \n");
        exit(2);
    }
    memcpy(&server.sin_addr, h->h_addr, h->h_length);
    server.sin_port = htons(24800);
    if (connect(sock, (const sockaddr *)&server, sizeof(server)) < 0 ) {
        perror("connecting stream socket");
        exit(1);
    }
    printf("[%s:%d]connected\n", __FUNCTION__, __LINE__);

    while (1) {
        if ( (rc = read(sock,iobuf,sizeof(iobuf))) < 0 ) {
            perror("oRead: reading socket stream");
            exit(1);
        }
        //printf("[%s:%d] read %d\n", __FUNCTION__, __LINE__, rc);
        if (!rc)
            break;
        //memdump(iobuf, rc, (char *)"Rx");
        static char bbb[5];
        memcpy (bbb, &iobuf[sizeof(int)], 4);
        //printf ("in '%s'\n", bbb);
        if (!memcmp(iobuf+sizeof(int), "Syne", 4))
            senddata(helloresp, sizeof(helloresp));
        else if (!memcmp(iobuf+sizeof(int), "QINF", 4))
            senddata(dinf, sizeof(dinf));
        else if (!memcmp(iobuf+sizeof(int), "CIAK", 4)
         || !memcmp(iobuf+sizeof(int), "CALV", 4)) {
            senddata((unsigned char *)"CALV", 4);
            senddata((unsigned char *)"CNOP", 4);
        }
#if 0
        else if (!memcmp(iobuf+sizeof(int), "COUT", 4)) {
printf("[%s:%d]cout\n", __FUNCTION__, __LINE__);
        }
#endif
        else {
            printf("unknown '%s' ", bbb);
            rc -= sizeof(int) + 4;
            if (rc > 0)
                memdump(&iobuf[sizeof(int) + 4], rc, (char *)"Rx");
            else
                printf("\n");
        }
    }
    close(sock);
    printf("tcpclient finished ok\n");
    return 0;
}
