#include "UDPSocket.h"

class DataReceiverAndSender : public UDPSocket {
    public:
        DataReceiverAndSender(int port, std::string destination_address, int destination_port, std::string out_address, int out_port);
        void run() override;
        void send_out();

    private:
        std::string destination_address;
        std::string out_address;
        int destination_port;
        int out_port;
};