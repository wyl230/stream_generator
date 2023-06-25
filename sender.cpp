// sender中打上时间戳
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <algorithm>
#include <random>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <chrono>
#include <ctime>
#include <iostream>
#include <thread>
#include <vector>

#include "json.hpp"
using namespace std;
using json = nlohmann::json;
#define RECEIVER_ADDRESS "127.0.0.1"  // 目的地址
#define INT_MAX 99999999
//#define RECEIVER_ADDRESS "172.17.0.55" // 目的地址
bool print_log = true;
int print_cnt = 0;

struct my_package {
  uint32_t tunnel_id;
  uint32_t source_module_id;
  uint16_t source_user_id;
  uint16_t dest_user_id;
  uint32_t flow_id;
  uint32_t service_id;
  uint32_t qos_id;
  uint32_t packet_id;
  timespec timestamp;
  uint32_t ext_flag;
};

struct control_message {
  uint32_t  total_loss=0;
  uint32_t  recent_loss=0;
  uint32_t  max_delay;
  uint32_t  min_delay;
  uint32_t  avg_delay;
  uint32_t  avg_speed=0;
  uint32_t  unused2=0;
  uint32_t  unused3=0;
};

uint32_t global_packet_id = 0;
int recv_thread(int port, int package_size);
void client_init();

void data_generate(char *package_head);
int package_size = 2048, package_speed, delay, report_interval;
long package_num;
int client_port, video_in;
bool auto_send = false;
bool send_to_core_net = false;
int send_type = 0; // 1 恒比特 2 变比特 
int control_port, server_port;// client:接入网 server：本pod接收程序 control；控制程序
char datagram[2048];
my_package pack;
string client_address;


string real_address(string address) {
  struct hostent *host;
  host = gethostbyname(address.c_str());
  if (host == NULL) {
    cout << "gethostbyname error" << endl;
    return "gethostbyname error";
  }
  return inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
}

int main(int argc, char *argv[]) {
  client_init();
  client_address = argv[1];
  global_packet_id = stoi(string(argv[2]));
  /* global_packet_id = stoi(string(argv[2])); */
  // std::ifstream ifs("packet_id.json");
  // json jf = json::parse(ifs);
  // global_packet_id = jf[to_string(pack.flow_id).c_str()];

  cout << "client address:" << client_address << endl;
  cout << server_port << endl;
  cout << "接收开始" << endl;
  std::thread receive_t(recv_thread,server_port,package_size);
  while (1) {
    sleep(1);
  }
}

void client_init() {
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
  pack.flow_id = j["flow_id"];
  package_num = j["package_num"];
  package_speed = j["package_speed"];
  control_port = j["control_port"];
  server_port = j["server_port"];
  client_port = j["client_port"];
  report_interval = j["report_interval"];
  video_in = j["video_in"];
  auto_send = j["auto_send"];
  send_type = j["send_type"];
  puts("6");
  srcFile.close();

  if(pack.source_user_id == 33000 || pack.dest_user_id == 33000) {
    send_to_core_net = true;
  }
  puts("7");
  return;
}

int recv_thread(int port, int package_size) {
  int num = 0;
  // socket初始化
  int recv_socket;
  sockaddr_in recv_addr, sender_addr;
  recv_socket = socket(AF_INET, SOCK_DGRAM, 0);
  recv_addr.sin_family = AF_INET;
  recv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  recv_addr.sin_port = htons(video_in);
  bind(recv_socket, (sockaddr *)&recv_addr, sizeof(recv_addr));

  int my_socket;
  sockaddr_in target_addr, my_addr;
  my_socket = socket(AF_INET, SOCK_DGRAM, 0);
  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(2222);
  my_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  // 绑定端口
  bind(my_socket, (sockaddr *)&my_addr, sizeof(my_addr));
  // 指定目标
  target_addr.sin_family = AF_INET;
  target_addr.sin_port = htons(client_port);
  target_addr.sin_addr.s_addr = inet_addr(real_address(client_address).c_str());

  // 接收准备
  socklen_t sender_addrLen = sizeof(sender_addr);
  char buffer[package_size];
  char package_head[package_size+sizeof(my_package)];
  int readLen = 0;
  uint32_t cnt_package = 0,recent_package=0;
  uint32_t total_delay;
  timespec delay_a, delay_b,delay_c;
  uint32_t max_delay=0,min_delay=INT_MAX,avg_delay=0, recent_delay=0,avg_speed=0;
  delay_a = {0, 0};
  delay_b = {0, 0};
  delay_c = {0, 0};
  data_generate(package_head);
  while (true) {    
    // 音频：160Byte 20ms 模拟的是G.711 64kbps音频流 package_speed = 50
    // 视频：1200Byte 3-6ms 模拟的是 H.264 1080p 30fps的视频流
    if(send_type == 1) { // 恒比特流
      readLen = 160;
      memset(buffer, 1, 1230);
      std::this_thread::sleep_for(std::chrono::milliseconds(1000 / package_speed));
    } else if(send_type == 2) { // 变比特流
      std::random_device rd;  
      std::mt19937 gen(rd()); 
      std::uniform_int_distribution<> distrib(3, 6);
      // std::uniform_int_distribution<> distrib(1000/package_speed / 2, 1000/package_speed * 3 / 2);
      int delay_ms = distrib(gen);
      readLen = 1200;
      memset(buffer, 1, 1200);
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
    } else if(send_type == 10) {
      return -1;
    } else { // 接受真正的视频流
      readLen = recvfrom(recv_socket, buffer, package_size, 0, (sockaddr *)&sender_addr, &sender_addrLen);
      strcat(package_head,buffer); // something wrong
      for(int i = 0; i < sizeof(buffer); ++i) {
        package_head[i + sizeof(my_package)] = buffer[i];
      }
    }
    //转发视频流

    // package_head
    // 加上时间戳
    my_package* ptr = reinterpret_cast<my_package*>(package_head);  // 将其强制转换为my_package指针类型
    timespec now = {};  // 生成新的 timestamp
    clock_gettime(CLOCK_REALTIME, &now);
    ptr->timestamp = now;  // 将 timestamp 赋值为新的值
    ptr->source_user_id = pack.source_user_id;
    ptr->dest_user_id = pack.dest_user_id;
    ptr->flow_id = pack.flow_id;
    ptr->packet_id = global_packet_id++;


    std::tm tm = *std::localtime(&ptr->timestamp.tv_sec);
    if(print_log && print_cnt++ % 100 == 0) {
      std::cout << print_cnt << " timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(9) << std::setfill('0') << ptr->timestamp.tv_nsec << std::endl;
    }

    if(send_to_core_net) {
      cout << "发送到核心网，todo" << endl;
      sockaddr_in target_addr;
      target_addr.sin_family = AF_INET;
      target_addr.sin_port = htons(32001);
      target_addr.sin_addr.s_addr = inet_addr("11.0.8.1");

      auto error = sendto(my_socket, package_head, readLen+sizeof
      (my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
      if (error == -1) {
        perror("sendto");
        cout <<"核心网发送失败！ sendto() error occurred at package "<< endl;
      }
    } else {
      sendto(my_socket, package_head, readLen+sizeof(my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
    }
    // 写入packet_id到文件中 json格式 [id: packet_id]
    //
    //
    //
    /* static int cnt = 0; */
    /* if(cnt++ % 100 == 0) { */
    /*   std::ifstream ifs("packet_id.json"); */
    /*   json jf = json::parse(ifs); */
    /**/
    /*   jf[to_string(pack.flow_id).c_str()] = global_packet_id; */
    /*   std::ofstream file("packet_id.json"); */
    /*   file << jf; */
    /* } */

    //
    clock_gettime(CLOCK_MONOTONIC, &delay_a);
    if(delay_a.tv_sec-delay_c.tv_sec > 1) {
      static int cnt = 0;
      cout<< "sending " << cnt++ << endl;
      delay_c = delay_a;
    }
  }
  return 0;
}

void data_generate(char *package_head)
{
  my_package *temp=(my_package *)package_head;
  temp->source_user_id = pack.source_user_id;
  temp->dest_user_id = pack.dest_user_id;
  // temp->dest_user_id = pack.source_module_id;
  return;
}
