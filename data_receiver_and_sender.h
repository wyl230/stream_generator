#include "UDPSocket.h"
#include "header.h"

class DataReceiverAndSender : public UDPSocket {
public:
  DataReceiverAndSender(int port, std::string destination_address, std::string out_address);
  void run() override;
  void send_out();

  timespec get_current_time();
  void param_config();

  std::string destination_address;
  std::string out_address;
  int destination_port;
  int out_port;

  timespec last_time;
  my_package pack;
  int package_size = 2048, package_speed, delay, report_interval;
  long package_num;
  int server_port, video_out;
  int control_port;
  std::string client_address;  // 初始发送参数，在程序开始时指定
  std::string control_address;  // 初始发送参数，在程序开始时指定
  char datagram[2048];
  std::string duplex_server_address, duplex_client_address;
  int duplex_server_port, duplex_client_port;

  std::map<int, msg_for_each_stream> flow_msg;
};