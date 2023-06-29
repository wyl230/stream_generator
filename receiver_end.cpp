#include "data_receiver_and_sender.h"

#include "header.h"

using namespace std;
#include "json.hpp"
using json = nlohmann::json;

int get_server_port() {
  ifstream srcFile("./init.json", ios::binary);
  if (!srcFile.is_open()) {
    cout << "Fail to open src.json" << endl;
    return;
  }
  json j;
  srcFile >> j;
  int server_port = j["server_port"];
  srcFile.close();
  return server_port;
}

int main(int argc, char *argv[]) {
  string client_address = argv[1]; // if video, 发给vlc
  string control_address = argv[2]; // 发流信息
  
  DataReceiverAndSender data_receiver_and_sender(get_server_port(), control_address, client_address);
}