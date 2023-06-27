
#include "UDPSocket.h"

UDPSocket::UDPSocket(int port) {
    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket creation failed");
    }
    memset(&serveraddr, 0, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    serveraddr.sin_addr.s_addr = INADDR_ANY;
    serveraddr.sin_port = htons(port);

    if (bind(sockfd, (const struct sockaddr *)&serveraddr, sizeof(serveraddr)) < 0) {
        perror("bind failed");
    }
}

std::string UDPSocket::recv() {
    char buffer[MAXLINE];
    memset(buffer, 0, MAXLINE);

    socklen_t len = sizeof(clientaddr);
    int n = recvfrom(sockfd, buffer, MAXLINE, MSG_WAITALL, (struct sockaddr *)&clientaddr, &len);
    if (n < 0) {
        perror("recvfrom failed");
    }
    return std::string(buffer);
}

void UDPSocket::send(std::string message, std::string address, int port) {
    char buffer[MAXLINE];
    memset(buffer, 0, MAXLINE);
    strcpy(buffer, message.c_str());

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr.s_addr = inet_addr(address.c_str());

    if (sendto(sockfd, buffer, strlen(buffer), MSG_CONFIRM, (const struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
        perror("sendto failed");
    }
}
