#include "data_receiver_and_sender.h"
#include "json.hpp"
#include <iostream>
using namespace std;
using json = nlohmann::json;

DataReceiverAndSender::DataReceiverAndSender(int port, std::string destination_address, std::string out_address)

  : UDPSocket(port), destination_address(destination_address), out_address(out_address) {
    cout << "client address: " << out_address << endl;
    cout << "control address: " << destination_address << endl;
    last_time = get_current_time();
  }

std::string real_address(std::string address) {
  struct hostent *host;
  host = gethostbyname(address.c_str());
  if (host == NULL) {
    cout << "gethostbyname error" << endl;
    return "gethostbyname error";
  }
  return inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
}

timespec DataReceiverAndSender::get_current_time() {
  timespec now = {};  // 生成新的 timestamp
  clock_gettime(CLOCK_REALTIME, &now);
  return now;
}


void DataReceiverAndSender::param_config() {
  ifstream srcFile("./init.json", ios::binary);
  if (!srcFile.is_open()) {
    cout << "Fail to open src.json" << endl;
    return;
  }
  json j;
  srcFile >> j;
  pack.source_user_id = j["source_id"];
  // pack.source_module_id = j["source_module_id"];
  pack.dest_user_id = j["dest_id"];
  package_num = j["package_num"];
  package_speed = j["package_speed"];
  control_port = j["control_port"];
  server_port = j["server_port"];
  report_interval = j["report_interval"];
  video_out = j["video_out"];

  duplex_client_address = j["duplex_client_address"];
  duplex_server_address = j["duplex_server_address"];
  duplex_client_port = j["duplex_client_port"];
  duplex_server_port = j["duplex_server_port"];
  
  srcFile.close();
  return;
}


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