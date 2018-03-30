/*
 * Author: Khanh Le
 */
#include "networks.h"
#include "library.h"
#include <arpa/inet.h>

typedef struct Client {
   int sk;
   char name[MAX_NAME + 1];
   struct Client *next;
} Client;

typedef struct ClientControl {
   int size;
   Client *head;
} ClientControl;

static ClientControl *clientCtrl = NULL;

void CreateClientControl() {
   clientCtrl = calloc(1, sizeof(ClientControl));
   clientCtrl->head = NULL;
}

void DeleteClientControl() {
   free(clientCtrl);
}

void AddClient(int client_sk) {
   Client *client = calloc(1, sizeof(Client));
   client->sk = client_sk;
   client->name[0] = '\0';
   client->next = clientCtrl->head;
   clientCtrl->head = client;
   client = NULL;
   clientCtrl->size++;
}

void DeleteClient(int client_sk) {
   Client *prev, *curr = clientCtrl->head;
   if (curr && curr->sk == client_sk) {
      clientCtrl->head = curr->next;
      clientCtrl->size--;
      free(curr);
      return;
   }
   while (curr && curr->sk != client_sk) {
      prev = curr;
      curr = curr->next;
   }
   
   if (curr) {
      prev->next = curr->next;
      free(curr);
   }
   clientCtrl->size--;
}

Client *SearchClientByName(char *name) {
   Client *temp = clientCtrl->head;
   
   while(temp) {
      if (strlen(name) == strlen(temp->name) && 
       !memcmp(temp->name, name, strlen(name))) {
         break;
      }
      temp = temp->next;
   }
   return temp;
}

Client *SearchClientBySocket(int client_sk) {
   Client *temp = clientCtrl->head;
   while (temp && temp->sk != client_sk) {
      temp = temp->next;
   }
   return temp;
}

void PrintClient(Client *client) {
   printf("%d - %s, ", client->sk, client->name);
}

void PrintAllClients() {
   int count = 0;
   Client *client = clientCtrl->head;
   while (count < clientCtrl->size) {
      PrintClient(client);
      client = client->next;
      count++;
   }
   printf("\n");
}

int GetPort(int argc, char **argv) {
   int temp, port = 0;
   char *arg;
   if (argc == 2) {
      arg = argv[1];
      if (sscanf(arg, "%d%n", &temp, &temp) && !arg[temp]) {
         port = atoi(arg);
      }
   }
   return port;
}
// ----------------------------------------------------------------------------


void HandleClientConnection(char **buf, int sk) {
   char *seekPtr = *buf;
   char *sendBuf = calloc(1, sizeof(char));
   char name[MAX_NAME + 1];
   Client *client = NULL;
   int bufLen = 0;
   ReadName(*buf, &seekPtr, name);
   if ((client = SearchClientByName(name)) == NULL) {
         client = SearchClientBySocket(sk);
         memcpy(client->name, name, strlen(name) + 1);
         bufLen = PackHeader(&sendBuf, FLG_S2C_GOOD_INIT);
      
   } else {
         bufLen = PackHeader(&sendBuf, FLG_S2C_BAD_INIT);
         DeleteClient(sk);
   }
   SendData(sk, sendBuf, bufLen);
   free(sendBuf);
}

void HandleClientMessage(char **buf, int sk, int payload) {
   char senderName[MAX_NAME + 1], clientName[MAX_NAME + 1];
   Client *client = NULL;
   int numClients = 0, goodBufLen = 0, badBufLen = 0;
   char *errBuf, *seekPtr = *buf;
   char *str = calloc(payload, sizeof(char));

   memcpy(str, *buf, payload);
   goodBufLen = PackHeader(&str, FLG_C2S_MSG);
   payload -= ReadName(*buf, &seekPtr, senderName); 
   numClients = seekPtr[0];
   seekPtr++;
   payload--; 
   while (numClients--) {
      payload -= ReadName(*buf, &seekPtr, clientName);
      if((client = SearchClientByName(clientName)) != NULL) {
         SendData(client->sk, str, goodBufLen);
      }
      else {
         errBuf = calloc(1, sizeof(char));
         PackName(&errBuf, clientName);
         badBufLen = PackHeader(&errBuf, FLG_S2C_NONEXIST_HANDLE);
         SendData(sk, errBuf, badBufLen);
         free(errBuf);
      }
   }
}

void HandleClientBroadcast(char **buf, int payload) {
   char *seekPtr = *buf;
   char senderName[MAX_NAME + 1];
   int bufLen = 0;
   Client *client = clientCtrl->head;
   ReadName(*buf, &seekPtr, senderName);
   bufLen = PackHeader(buf, FLG_C2S_BRDCST);
   while(client) {
      if (strcmp(client->name, senderName)) {
         SendData(client->sk, *buf, bufLen);
      }
      client = client->next; 
   }
}

void HandleClientExit(int client_sk) {
   char *buf = calloc(1, sizeof(char));
   int bufLen = 0;
   bufLen = PackHeader(&buf, FLG_S2C_ACK_EXIT);
   SendData(client_sk, buf, bufLen);
   DeleteClient(client_sk);
   free(buf);
}

// remember to handle if the list is empty;
// checl the while loop for the linkedlist
void HandleClientRequestList(int client_sk) {
   Client *client = clientCtrl->head;
   int numClients = htonl(clientCtrl->size);
   int len = sizeof(Header) + sizeof(int);
   char *buf = calloc(len, sizeof(char));
   Header hdr = {htons(len), FLG_S2C_ACK_LST_REQ};
   memcpy(buf, &hdr, sizeof(Header));
   memcpy(buf + sizeof(Header), &numClients, sizeof(uint32_t));
   SendData(client_sk, buf, len);
   free(buf); 

   while (client) {
      buf = calloc(1, sizeof(char));
      PackName(&buf, client->name);
      len = PackHeader(&buf, FLG_S2C_LST_SEND);
      SendData(client_sk, buf, len);
      free(buf);
      client = client->next;
   }
}


void HandleServerSocket(int server_sockfd, fd_set *readfds) {
   int client_sockfd = tcpAccept(server_sockfd);
   FD_SET(client_sockfd, readfds);
   AddClient(client_sockfd);
}

void HandleClientSocket(int client_sk) {
   char *buf = NULL;
   int flag = 0, payload = 0;   
            
   if (!UnpackHeader(client_sk, &flag, &payload)) {
      DeleteClient(client_sk);
      return;
   }

   switch(flag) {
      case FLG_C2S_INIT:
         if (UnpackPayload(client_sk, &buf, payload)) {
            HandleClientConnection(&buf, client_sk);
         }
         else {
            DeleteClient(client_sk);
         }
         break;
      case FLG_C2S_BRDCST: 
         if (UnpackPayload(client_sk, &buf, payload)) {
            HandleClientBroadcast(&buf, payload); 
         }
         else {
            DeleteClient(client_sk);
         }
         break;
      case FLG_C2S_MSG:
         if (UnpackPayload(client_sk, &buf, payload)) {
            HandleClientMessage(&buf, client_sk, payload);
         }
         else {
            DeleteClient(client_sk);
         }
         break;
      case FLG_C2S_EXIT:
         HandleClientExit(client_sk); 
         break;
      case FLG_C2S_LST_REQ:
         HandleClientRequestList(client_sk); 
         break;   
   }
   free(buf);
}

void SetupFds(int server_sk, fd_set *readfds) {
   FD_ZERO(readfds);
   FD_SET(server_sk, readfds);
   Client *client = clientCtrl->head;
   while (client) {
      FD_SET(client->sk, readfds);
      client = client->next;
   }
}

void Run(int server_sk) {
   fd_set readfds;  // set of fds that need to monitor 
   Client *client;
   while(1) {
      SetupFds(server_sk, &readfds);
      if (select(FD_SETSIZE, &readfds, NULL, NULL, NULL) < 0) {
         perror("select() error\n");
         exit(1);
      }
      if (FD_ISSET(server_sk, &readfds)) {
         HandleServerSocket(server_sk, &readfds);
      }
      else {
         client = clientCtrl->head;
         while(client) {
            if (FD_ISSET(client->sk, &readfds)) {
               HandleClientSocket(client->sk);
               break;
            }
            client = client->next; 
         } 
      }
   }
}

int main(int argc, char **argv) {
   int port = 0;
   int server_sockfd = 0;      // socket descriptor
   port = GetPort(argc, argv);
   server_sockfd = tcpServerSetup(port);
   CreateClientControl();
   Run(server_sockfd);
   DeleteClientControl();
   return 0;
}
