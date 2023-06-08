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
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

#include "json.hpp"
using namespace std;
using json = nlohmann::json;
#define RECEIVER_ADDRESS "127.0.0.1"  // 目的地址
//#define RECEIVER_ADDRESS "172.17.0.55" // 目的地址

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

int send_thread(int port, long package_num, int delay,
                int packageSize);  // 发送线程，需要指定端口和发送包数
int recv_thread(int port, int package_size);
void data_generate(my_package *pack);
void server_init();

int package_size = 2048, package_speed, delay;
long package_num;
int server_port,video_out;
string client_address;  // 初始发送参数，在程序开始时指定
char datagram[2048];
my_package pack;

int main(int argc, char *argv[]) {
  server_init();
  client_address = argv[1];
  cout << client_address << endl;
  struct hostent *host;
  host = gethostbyname(client_address.c_str());
  if (host == NULL) {
    cout << "gethostbyname error" << endl;
    return 0;
  }
  client_address = inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
  cout << client_address << endl;
  cout<<server_port<<endl;
  delay = 1000000000 / package_speed;
  sleep(1);
  data_generate(&pack);
  cout << "传输开始" << endl;
  std::thread send_t(send_thread, 0, package_num, delay, package_size);
  while (1) {
    sleep(1);
  }
}

void server_init() {
  ifstream srcFile("./init.json", ios::binary);
  if (!srcFile.is_open()) {
    cout << "Fail to open src.json" << endl;
    return;
  }
  json j;
  srcFile >> j;
  pack.source_user_id = j["source_id"];
  pack.dest_user_id = j["dest_id"];
  package_num = j["package_num"];
  package_speed = j["package_speed"];
  server_port = j["server_port"];
  video_out = j["video_out"];
  srcFile.close();
  return;
}
int send_thread(int port, long package_num, int delay, int packageSize) {
  // 初始化socket
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
  target_addr.sin_port = htons(video_out);
  target_addr.sin_addr.s_addr = inet_addr(client_address.c_str());
  int recv_socket;
  sockaddr_in recv_addr, sender_addr;
  recv_socket = socket(AF_INET, SOCK_DGRAM, 0);
  recv_addr.sin_family = AF_INET;
  recv_addr.sin_addr.s_addr = inet_addr("0.0.0.0");
  recv_addr.sin_port = htons(server_port);
  bind(recv_socket, (sockaddr *)&recv_addr, sizeof(recv_addr));
  // 接收准备
  socklen_t sender_addrLen = sizeof(sender_addr);
  char buffer[package_size];
  int readLen = 0;
  uint32_t cnt_package = 0,recent_package=0;
  uint32_t total_delay;
  timespec delay_a, delay_b,delay_c;
  delay_a = {0, 0};
  delay_b = {0, 0};
  delay_c = {0, 0};
  int error=0;
  // 开始发送
  for (int i = 0;; i++) {
    // sprintf(sendBuf, "%d", i);
    // 固定化包间间隔
    readLen=recvfrom(recv_socket, buffer, package_size+sizeof(my_package), 0, (sockaddr *)&sender_addr, &sender_addrLen);
    /* strncpy(datagram,buffer+sizeof(my_package),readLen-sizeof(my_package)); */

    for(int i=0;i<readLen-sizeof(my_package);i++) {
      datagram[i]=buffer[i+sizeof(my_package)];
    }

    error=sendto(my_socket, datagram,readLen-sizeof(my_package), 0,
                   (sockaddr *)&target_addr, sizeof(target_addr));
    /* cout<<"readLen"<< readLen << endl; */
    if (error == -1) {
      perror("sendto");
      cout <<"sendto() error occurred at package "<< endl;
    }
        clock_gettime(CLOCK_MONOTONIC, &delay_a);


            /* cout << "size of pack" << sizeof(my_package) << endl; */
    if(delay_a.tv_sec-delay_c.tv_sec>1)
    {
      cout<<"sending"<<endl;
      delay_c=delay_a;
    }
  }
  sleep(1);
  return 0;
}


void data_generate(my_package *pack) {
  memset(datagram, 0, 2048);  // zero out the packet buffer
  char *data = datagram;
  my_package *temp = (my_package *)data;
  temp->source_user_id = pack->source_user_id;
  temp->dest_user_id = pack->dest_user_id;
  return;
}

