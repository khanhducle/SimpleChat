/*
 * Author: Khanh Le
 */
#ifndef LIBRARY_H
#define LIBRARY_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h> //ntohs - htons

#define MAX_BUF 4000
#define MAX_NAME 250
#define MAX_MSG 1000
typedef uint32_t UInt;
typedef uint16_t UShort;
typedef uint8_t UChar;

#define FLG_C2S_INIT 1
#define FLG_S2C_GOOD_INIT 2
#define FLG_S2C_BAD_INIT 3
#define FLG_C2S_BRDCST 4
#define FLG_C2S_MSG 5
#define FLG_NONUSED 6
#define FLG_S2C_NONEXIST_HANDLE 7
#define FLG_C2S_EXIT 8
#define FLG_S2C_ACK_EXIT 9
#define FLG_C2S_LST_REQ 10
#define FLG_S2C_ACK_LST_REQ 11
#define FLG_S2C_LST_SEND 12

typedef struct {
   UShort pktLen;
   UChar flag;
} __attribute__((packed)) Header;

inline void Print(char *buf, int len) {
   int i;
   printf("bufLen = %d\n", len);
   for (i = 0; i < len; i++) {
      if (('a' <= buf[i] && buf[i] <= 'z') ||
          ('A' <= buf[i] && buf[i] <= 'Z') ||
          ('0' <= buf[i] && buf[i] <= '9'))
         printf("|%c", buf[i]);
      else
         printf("|%02X", buf[i]);
   }
   if(buf[i] == '\0')
      printf("|NULL");
   printf("\n"); 
}

int ReadName(char *buf, char **seekPtr, char *name) {
   memcpy(name, *seekPtr + 1, (*seekPtr)[0]);
   name[(uint8_t)(*seekPtr)[0]] = '\0';
   *seekPtr += 1 + (*seekPtr)[0];
   return 1 + buf[0];
}

int PackHeader(char **buf, int flag) {
   UShort len = sizeof(Header) + strlen(*buf) + 1;
   Header hdr;
   if (flag == FLG_C2S_BRDCST || flag == FLG_C2S_MSG) {
      hdr.pktLen = htons(len);
      hdr.flag = flag;
   }
   else {
      hdr.pktLen = htons(len - 1); // -1 to get rid of NULL
      hdr.flag = flag;
   }
   *buf = realloc(*buf, len);
   memmove(*buf + sizeof(Header), *buf, strlen(*buf) + 1);
   memcpy(*buf, &hdr, sizeof(Header));
   if (flag == FLG_C2S_BRDCST || flag == FLG_C2S_MSG) {
      return len;
   }
   else {
      return len - 1;
   }
}

void PackName(char **buf, char *name) {
   int len = 1 + strlen(name) + strlen(*buf) + 1; //name length + NULL
   *buf = realloc(*buf, len);
   memmove(*buf + 1 + strlen(name), *buf, strlen(*buf) + 1);
   memcpy(*buf + 1, name, strlen(name));
   (*buf)[0] = strlen(name);
}

void PackMessage(char **buf, char *msg) {
   int len = strlen(msg) + strlen(*buf) + 1;   
   *buf = realloc(*buf, len);
   memmove(*buf + strlen(msg), *buf, strlen(*buf) + 1);
   memcpy(*buf, msg, strlen(msg));
}

void PackNumber(char **buf, int *num, int bytes) {
   int len = strlen(*buf) + bytes + 1;
   *buf = realloc(*buf, len);
   memmove(*buf + bytes, *buf, strlen(*buf) + 1);
   memcpy(*buf, num, bytes);
}


inline int SendData(int sockfd, char *buffer, int length) {
   int sent = send(sockfd, buffer, length, 0);
   if (sent < 0) {
      perror("send call\n");
      exit(1);
   }
   return sent;
}

inline int ReceiveData(int sockfd, char *buffer, int length) {
   int received = recv(sockfd, buffer, length, 0);
   if (received < 0) {
      perror("recv call\n");
      exit(1);
   }
   return received;
}

int UnpackHeader(int client_sk, int *flag, int *payload) {
   Header hdr;
   int recv = ReceiveData(client_sk, (char *)&hdr, sizeof(Header));
   *flag = hdr.flag;
   *payload = ntohs(hdr.pktLen) - sizeof(Header);
   return recv;
}

int UnpackPayload(int client_sk, char **buf, int payload) {
   int recv;
   *buf = calloc(payload, sizeof(char));
   recv = ReceiveData(client_sk, *buf, payload);
   return recv;
}

#endif

