// server.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>

#define MAX_NUMBER_OF_CLIENTS 100
#define MAX_MESSAGE_BYTES     1024

int clients[MAX_NUMBER_OF_CLIENTS];
unsigned int numberOfCurrentClients = 0;
pthread_mutex_t clientsMutex = PTHREAD_MUTEX_INITIALIZER;

struct handleClientArgs {
    int clientFileDescriptor;
    struct sockaddr_in clientAddress;
};

void broadcastClientMessage(char *message, int senderFileDescriptor) {
    pthread_mutex_lock(&clientsMutex);
    for (int index = 0; index < numberOfCurrentClients; index++) {
        if (clients[index] != senderFileDescriptor) {
            if (send(clients[index], message, strlen(message), 0) < 0) {
                perror("Send failed, moving to the next client");
                continue;
            }
        }
    }
    pthread_mutex_unlock(&clientsMutex);  // Unlock after broadcast
}

void *handleClient(void *arg) {
    struct handleClientArgs *args = (struct handleClientArgs *)arg;
    int clientFileDescriptor = args->clientFileDescriptor;
    struct sockaddr_in clientAddress = args->clientAddress;

    const char* newClientJoinedMessage = "The client %s:%d has joined the chat\n";
    char newClientJoinedMessageBuffer[100];
    sprintf(newClientJoinedMessageBuffer, newClientJoinedMessage,
            inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
    broadcastClientMessage(newClientJoinedMessageBuffer, clientFileDescriptor);

    char messageBuffer[MAX_MESSAGE_BYTES];
    int messageLength;

    while ((messageLength = recv(clientFileDescriptor, messageBuffer, MAX_MESSAGE_BYTES, 0)) > 0) {
        messageBuffer[messageLength] = '\0';  // Properly terminate the string
        if (strcmp(messageBuffer, "!exit") == 0) {
            printf("The client with file descriptor %d requested to exit\n", clientFileDescriptor);
            const char* clientLeftMessage = "The client %s:%d has left the chat\n";
            char clientLeftMessageBuffer[100];
            sprintf(clientLeftMessageBuffer, clientLeftMessage,
                    inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
            broadcastClientMessage(clientLeftMessageBuffer, clientFileDescriptor);
            break;
        }
        printf("Received a message from %s:%d => %s",
               inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port), messageBuffer);
        broadcastClientMessage(messageBuffer, clientFileDescriptor);
    }

    close(clientFileDescriptor);
    pthread_mutex_lock(&clientsMutex);
    for (int index = 0; index < numberOfCurrentClients; index++) {
        if (clients[index] == clientFileDescriptor) {
            clients[index] = clients[numberOfCurrentClients - 1];   // Replace with the last client
            clients[numberOfCurrentClients - 1] = 0;                // Clear the last client
            numberOfCurrentClients--;
            break;
        }
    }
    pthread_mutex_unlock(&clientsMutex);

    free(arg);
    return NULL;
}

int main() {
    int serverFileDescriptor, clientFileDescriptor;
    struct sockaddr_in serverAddress, clientAddress;
    socklen_t clientAddressLength;
    pthread_t threadId;

    serverFileDescriptor = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFileDescriptor < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8080);

    // Allow the socket to be reused immediately after the server terminates
    int opt = 1;
    if (setsockopt(serverFileDescriptor, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(serverFileDescriptor);
        exit(EXIT_FAILURE);
    }

    // Bind the socket to the server address
    if (bind(serverFileDescriptor, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Binding failed");
        close(serverFileDescriptor);
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(serverFileDescriptor, 10) < 0) {  // Increased backlog size for more clients
        perror("Listening failed");
        close(serverFileDescriptor);
        exit(EXIT_FAILURE);
    }

    printf("Server is listening on port 8080\n");

    while (true) {
        clientAddressLength = sizeof(clientAddress);
        clientFileDescriptor = accept(serverFileDescriptor, (struct sockaddr *)&clientAddress, &clientAddressLength);
        if (clientFileDescriptor < 0) {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clientsMutex);
        if (numberOfCurrentClients < MAX_NUMBER_OF_CLIENTS) {
            clients[numberOfCurrentClients++] = clientFileDescriptor;
            struct handleClientArgs *args = malloc(sizeof(struct handleClientArgs));
            args->clientFileDescriptor = clientFileDescriptor;
            args->clientAddress = clientAddress;
            pthread_create(&threadId, NULL, handleClient, args);
            pthread_detach(threadId);  // Detach the thread to avoid memory leaks
        } else {
            fprintf(stderr, "Server full, connection refused: %s:%d\n",
                    inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port));
            close(clientFileDescriptor);
        }
        pthread_mutex_unlock(&clientsMutex);
    }

    close(serverFileDescriptor);
    return 0;
}
