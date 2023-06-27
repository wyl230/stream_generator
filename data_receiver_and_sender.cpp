#include "data_receiver_and_sender.h"

DataReceiverAndSender::DataReceiverAndSender(int port, std::string destination_address, int destination_port, std::string out_address, int out_port)

    : UDPSocket(port), destination_address(destination_address), destination_port(destination_port), out_address(out_address), out_port(out_port) {}

void DataReceiverAndSender::run() {
    while (true) {
        std::string message = recv();
        std::cout << "Received message: " << message << std::endl;
        // TODO: 对接收到的数据进行处理，并整理成需要发送给另一个程序的格式
        std::string formatted_message = "Formatted message";
        send(formatted_message, destination_address, destination_port);
    }
}

void DataReceiverAndSender::send_out() { // 发送到k8s外部

}