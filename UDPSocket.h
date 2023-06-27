#include <iostream>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

class UDPSocket {
    public:
        UDPSocket(int port);
        std::string recv();
        void send(std::string message, std::string address, int port);
        virtual void run() = 0;

    protected:
        static const int MAXLINE = 1024;
        int sockfd;
        struct sockaddr_in serveraddr, clientaddr;
};
