#include "data_receiver_and_sender.h"

#include "header.h"

using namespace std;
#include "json.hpp"
using json = nlohmann::json;

class ReceiverEnd {
  public:

  ReceiverEnd(string client_address, string control_address):client_address(client_address), control_address(control_address) {}

  void param_config() {
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

  void start() {
    param_config();
    DataReceiverAndSender data_receiver_and_sender(server_port, control_address, control_port, client_address, video_out);


  }

  int package_size = 2048, package_speed, delay, report_interval;
  long package_num;
  int server_port, video_out;
  int control_port;
  string client_address;  // 初始发送参数，在程序开始时指定
  string control_address;  // 初始发送参数，在程序开始时指定
  char datagram[2048];
  my_package pack;
  string duplex_server_address, duplex_client_address;
  int duplex_server_port, duplex_client_port;
};
int main(int argc, char *argv[]) {
  string client_address = argv[1]; // if video, 发给vlc
  string control_address = argv[2]; // 发流信息
  
  ReceiverEnd(client_address, control_address);
}