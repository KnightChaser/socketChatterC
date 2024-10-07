// client.c
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <stdbool.h>

#define MAX_MESSAGE_BYTES 1024

void *receiveMessages(void *args) {
    int socket = *(int *)args;
    char buffer[MAX_MESSAGE_BYTES];
    int messageLength;

    while ((messageLength = recv(socket, buffer, MAX_MESSAGE_BYTES, 0)) > 0) {
        buffer[messageLength] = '\0';
        printf("%s", buffer);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    int clientSocket;
    struct sockaddr_in serverAddress;
    char username[50];
    char message[MAX_MESSAGE_BYTES];
    char full_message[MAX_MESSAGE_BYTES + 50];
    pthread_t receiverThread;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0) {
        perror("Socket creation failed");
        return 1;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = inet_addr("127.0.0.1");
    serverAddress.sin_port = htons(8080);

    if (connect(clientSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        perror("Connection failed");
        close(clientSocket);
        return 1;
    }

    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    username[strlen(username) - 1] = '\0';

    srand(time(NULL));
    int randomHex = rand() % 0xFFFFFF + 1;
    snprintf(username + strlen(username), sizeof(username) - strlen(username), "#%06X", randomHex);
    printf("Connected as %s\n", username);

    pthread_create(&receiverThread, NULL, receiveMessages, &clientSocket);
    pthread_detach(receiverThread);  // Detach to free resources automatically

    printf("TYPE [ENTER] BEFORE TYPING A MESSAGE IF \"YOU: \" DOESN'T APPEAR\n");

    while (true) {
        printf("You: ");
        fgets(message, sizeof(message), stdin);
        if (strcmp(message, "!exit\n") == 0)
            break;

        // Send the message with the username
        snprintf(full_message, sizeof(full_message), "%s: %.1022s", username, message);
        if (send(clientSocket, full_message, strlen(full_message), 0) < 0) {
            perror("Send failed");
            break;
        }
    }

    close(clientSocket);
    return 0;
}
