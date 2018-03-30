/*
 * Author: Khanh Le
 */
#include "networks.h"
#include "library.h"
#include <unistd.h>

#define DELIMITER ' '

typedef enum {
   INUSE_HANDLE,
   TERMINATED_SERVER,
   NONEXIST_HANDLE,
   INVALID_COMMAND
} Error;

void PrintPrompt() {
   printf("$: ");
   fflush(stdout);
}

void PrintError(Error errorType, char *name) {
   switch(errorType) {
      case INUSE_HANDLE:
         printf("Handle already in use: %s\n", name);
         exit(1);
      case TERMINATED_SERVER:
         printf("Server Terminated\n");
         break;
      case NONEXIST_HANDLE:
         printf("Client with handle %s does not exist\n", name);
         PrintPrompt();
         break;
      case INVALID_COMMAND:
         printf("Invalid Command\n");
         break;
   }
}

void PrintMessage(char *name, char *msg) {
   printf("\n%s: %s\n", name, msg);
   PrintPrompt();
}

//-----------------------------------------------------------

void SendMessage(char *clientName, int sockfd, 
 int numClients, char **client, char* msg) {
   char *buf = calloc(1, sizeof(char));
   int bufLen, i = 0;
   PackMessage(&buf, msg);
   while (i < numClients) {
      PackName(&buf, client[i]);
      i++;
   }
   PackNumber(&buf, &numClients, 1);
   PackName(&buf, clientName); 
   bufLen = PackHeader(&buf, FLG_C2S_MSG);
   SendData(sockfd, buf, bufLen); // +1 for text need MULL character
   free(buf);
}

void SendBroadcast(char *clientName, int sockfd, char *msg) {
   char *buf = calloc(1, sizeof(char));
   int bufLen = 0;
   PackMessage(&buf, msg);
   PackName(&buf, clientName);
   bufLen = PackHeader(&buf, FLG_C2S_BRDCST);
   SendData(sockfd, buf, bufLen);
   free(buf);
}

void SendListRequest(char *clientName, int sockfd) {
   char *buf = calloc(1, sizeof(char));
   int bufLen = 0;
   PackName(&buf, clientName);
   bufLen = PackHeader(&buf, FLG_C2S_LST_REQ);
   SendData(sockfd, buf, bufLen);
   free(buf);
}

void SendExit(char *clientName, int sockfd) {
   char *buf = calloc(1, sizeof(char));
   int bufLen = 0;
   PackName(&buf, clientName);
   bufLen = PackHeader(&buf, FLG_C2S_EXIT);
   SendData(sockfd, buf, bufLen);
   free(buf);
}

void HandleCommand(char *clientName, int sockfd, char *cmd, 
 int numClients, char **client, char* msg) {
   if (!strcmp(cmd, "%m") || !strcmp(cmd, "%M")) {
     SendMessage(clientName, sockfd, numClients, client, msg);
   }
   else if (!strcmp(cmd, "%b") || !strcmp(cmd, "%B")) {
      SendBroadcast(clientName, sockfd, msg);
   }
   else if (!strcmp(cmd, "%l") || !strcmp(cmd, "%L")) {
      SendListRequest(clientName, sockfd);
   }
   else if (!strcmp(cmd, "%e") || !strcmp(cmd, "%E")) {
      SendExit(clientName, sockfd);
   }
   else {
      PrintError(INVALID_COMMAND, NULL); 
   }
}

int IsValid(char *cmd) {
   if (strcmp(cmd, "%b") && !strcmp(cmd, "%B") && !strcmp(cmd, "%l") &&
    strcmp(cmd, "%L") && !strcmp(cmd, "%e") && !strcmp(cmd, "%e")) {
      PrintError(INVALID_COMMAND, NULL);
      free(cmd);
      return 0;
   }
   return 1;
}

void HandleClientStdin(char *clientName, int sockfd) {
   char buf[MAX_BUF] = {'\0'}, tempBuf[MAX_BUF] = {'\0'};
   int numBytes = 0, idx = 0, temp, count = 0, numClients = 0; 
   int n = read(0, buf, MAX_BUF);
   char *delim = " ", *cmd = NULL, *token = NULL, *sendBuf = NULL;
   char **client = NULL;
   buf[n - 1] = '\0';
   strcpy(tempBuf, buf);
   if ((cmd = strtok(buf, delim)) == NULL || !IsValid(cmd)) {
      return;
   }
   idx += strlen(cmd) + 1;
   if (!strcmp(cmd, "%m") || !strcmp(cmd, "%M")) {
      token = strtok(NULL, delim);
      numClients = 1;
      if (sscanf(token, "%d%n", &temp, &temp) && !token[temp]) {
         idx += strlen(token) + 1;
         numClients = atoi(token);
         if (token == NULL) {
            PrintError(INVALID_COMMAND, NULL);
            return;
         }
         token = strtok(NULL, delim);
      }
      client = calloc(numClients, sizeof(char *));
      while (count < numClients) {
         if (token != NULL) {
            idx += strlen(token) + 1;
            client[count] = calloc(strlen(token) + 1, sizeof(char));
            strcpy(client[count++], token);
            token = strtok(NULL, delim);
         }
      }
      if (count < numClients) {
         PrintError(INVALID_COMMAND, NULL);
         return;
      }
   }
   strcpy(tempBuf, tempBuf + idx);
   do {
      numBytes = (strlen(tempBuf) < MAX_MSG) ? strlen(tempBuf) : MAX_MSG;
      sendBuf = calloc(numBytes + 1, sizeof(char));
      memcpy(sendBuf, tempBuf, numBytes);
      HandleCommand(clientName, sockfd, cmd, numClients, client, sendBuf);
      strcpy(tempBuf, tempBuf + numBytes);
      free(sendBuf);
   } while (strlen(tempBuf));

   count = numClients;
   while (count--) {
      free(client[count]);
   }
   free(client);
}

void RecvBroadcast(char **buf, int payload) {
   char *seekPtr = *buf;
   char senderName[MAX_NAME + 1];
   ReadName(*buf, &seekPtr, senderName);
   PrintMessage(senderName, seekPtr);
}

void RecvMessage(char **buf, int payload) {
   char senderName[MAX_NAME + 1] = {0};
   char clientName[MAX_NAME + 1] = {0};
   char *seekPtr = *buf;
   int numClients = 0;
   payload -= ReadName(*buf, &seekPtr, senderName);
   numClients = seekPtr[0];
   seekPtr++;;
   payload--;
   while (numClients--) {
      payload -= ReadName(*buf, &seekPtr, clientName);
   }
   PrintMessage(senderName, seekPtr); 
}

void RecvErrorMessenger(char **buf, int payload) {
   char clientName[MAX_NAME + 1] = {0};
   char *seekPtr = *buf;
   ReadName(*buf, &seekPtr, clientName);
   PrintError(NONEXIST_HANDLE, clientName);
}

int RecvAckList(char **buf, int payload) {
   int num = ntohl((*buf)[0] | ((int)(*buf)[1] << 8) |
    ((*buf)[2] << 16) | ((int)(*buf)[3] << 24));
   return num;
}

void RecvList(char **buf, int payload) {
   char clientName[MAX_NAME + 1] = {0};
   char *seekPtr = *buf;
   ReadName(*buf, &seekPtr, clientName);
   printf("  %s\n", clientName);
}

void HandleClientSocket(char *clientName, int sk) {
   char *buf = NULL;
   int flag = 0, payload = 0;
   static int flagRecv = 0;
   if (!UnpackHeader(sk, &flag, &payload)) {
      PrintError(TERMINATED_SERVER, NULL);
      exit(1);
   }
  
   static int numClients = 0;
   switch(flag) {
      case FLG_S2C_GOOD_INIT:
         // good packet, do nothing
         break;
      case FLG_S2C_BAD_INIT:
         PrintError(INUSE_HANDLE, clientName);
         break;
      case FLG_C2S_BRDCST:
         if (UnpackPayload(sk, &buf, payload)) {
            RecvBroadcast(&buf, payload);
         } 
         break;
      case FLG_C2S_MSG:
         if (UnpackPayload(sk, &buf, payload)) {
            RecvMessage(&buf, payload);
         } 
         break;
      case FLG_S2C_NONEXIST_HANDLE:
         if (UnpackPayload(sk, &buf, payload)) {
            RecvErrorMessenger(&buf, payload);
         } 
         break;
      case FLG_S2C_ACK_EXIT:
         free(buf);
         exit(0);
      case FLG_S2C_ACK_LST_REQ:
         if (UnpackPayload(sk, &buf, payload)) {
            numClients = RecvAckList(&buf, payload);
         } 
         break;
      case FLG_S2C_LST_SEND:
         if (flagRecv == 0) {
             printf("\nNumber of clients: %d\n", numClients);
             flagRecv = 1;
         }
         if (UnpackPayload(sk, &buf, payload)) {
            RecvList(&buf, payload);
            numClients--;
         }
         if (numClients == 0) {
            PrintPrompt();
            flagRecv = 0;
         }
         break;
   }
   free(buf);
}

void Run(char *clientName, int sockfd) {
   fd_set readfds;  // set of fds that need to monitor 
   PrintPrompt();
   do {
      FD_ZERO(&readfds);       
      FD_SET(sockfd, &readfds);
      FD_SET(0, &readfds);
      if (select(sockfd + 1, &readfds, NULL, NULL, NULL) < 0) {
         perror("select() error\n");
         exit(1);
      }
      if (FD_ISSET(sockfd, &readfds)) {
         HandleClientSocket(clientName, sockfd);
      }
      else if (FD_ISSET(0, &readfds)) {
         HandleClientStdin(clientName, sockfd);
         PrintPrompt();
      }
   } while(1);
}

void SetupConnection(char *clientName, int sockfd) {
   char *buf = calloc(sizeof(UChar), 1);
   UShort bufLen = 0;
   PackName(&buf, clientName);
   bufLen = PackHeader(&buf, FLG_C2S_INIT);
   SendData(sockfd, buf, bufLen);
   free(buf);
   HandleClientSocket(clientName, sockfd);
}

int main(int argc, char **argv) {
   int sockfd = 0;      // socket descriptor
   char *clientName = argv[1];
   char *hostName = argv[2];
   char *portNum = argv[3];
   
   sockfd = tcpClientSetup(hostName, portNum);
   SetupConnection(clientName, sockfd);
   Run(clientName, sockfd);

   return 0;
}
