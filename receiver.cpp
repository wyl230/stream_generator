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
#include <climits>

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


int send_thread(int port, long package_num, int delay, int packageSize);  // 发送线程，需要指定端口和发送包数
int recv_thread(int port, int package_size);
void data_generate(my_package *pack);
void server_init();
void report_delay(uint32_t max_delay,uint32_t min_delay,uint32_t avg_delay,uint32_t avg_speed);

int package_size = 2048, package_speed, delay, report_interval;
long package_num;
int server_port, video_out;
int control_port;
string client_address;  // 初始发送参数，在程序开始时指定
string control_address;  // 初始发送参数，在程序开始时指定
char datagram[2048];
my_package pack;

long long int total_bytes = 0;
long long int recent_bytes = 0;


int main(int argc, char *argv[]) {
  server_init();
  cout << "please input client_address & control_address" << endl;
  client_address = argv[1];
  control_address = argv[2];
  cout << client_address << endl;
  // client address 
  struct hostent *client_host;
  client_host = gethostbyname(client_address.c_str());
  if (client_host == NULL) {
    cout << "gethostbyname error" << endl;
    return 0;
  }
  client_address = inet_ntoa(*(struct in_addr *)client_host->h_addr_list[0]);
  cout << "client" << client_address << endl;
  // control address
  struct hostent *control_host;
  control_host = gethostbyname(client_address.c_str());
  if (control_host == NULL) {
    cout << "gethostbyname error" << endl;
    return 0;
  }
  client_address = inet_ntoa(*(struct in_addr *)control_host->h_addr_list[0]);
  cout << "control: " << control_address << endl;
  // end

  cout << "server port" << server_port << endl;
  delay = 1e9 / package_speed;
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
  control_port = j["control_port"];
  server_port = j["server_port"];
  report_interval = j["report_interval"];
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
  uint32_t cnt_package = 0,recent_package = 0;
  uint32_t total_delay;
  timespec delay_a, delay_b,delay_c;
  uint32_t max_delay=0,min_delay=INT_MAX,avg_delay=0, recent_delay=0,avg_speed=0;
  delay_a = {0, 0};
  delay_b = {0, 0};
  delay_c = {0, 0};
  int error = 0;
  // 开始发送
  for (int i = 0;; i++) {
    // sprintf(sendBuf, "%d", i);
    // 固定化包间间隔
    memset(buffer, 0, sizeof(buffer));
    readLen = recvfrom(recv_socket, buffer, package_size+sizeof(my_package), 0, (sockaddr *)&sender_addr, &sender_addrLen);

    recent_bytes += readLen - sizeof(my_package);

    // 从buffer中读取时间戳
    my_package* ptr = reinterpret_cast<my_package*>(buffer); 

    std::tm tm = *std::localtime(&ptr->timestamp.tv_sec);
    std::cout << "timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(9) << std::setfill('0') << ptr->timestamp.tv_nsec << std::endl;

    // 计算延迟 ns
    auto get_time_diff = [](timespec t1, timespec t2)->double {
      return (t2.tv_sec - t1.tv_sec) * 1e9 + (t2.tv_nsec - t1.tv_nsec);
    };

    timespec now = {};  // 生成新的 timestamp
    clock_gettime(CLOCK_REALTIME, &now);
    double time_diff = get_time_diff(ptr->timestamp, now);

    cout << "time_diff: " << time_diff << endl;

    recent_delay = time_diff / 1000;
    total_delay += recent_delay;
    max_delay = max(max_delay, recent_delay);
    min_delay = min(min_delay, recent_delay);

    cout << "delays: " << max_delay << " | " << min_delay << endl;

    strncpy(datagram, buffer + sizeof(my_package), readLen - sizeof(my_package));
    error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
    if (error == -1) {
      perror("sendto");
      cout <<"sendto() error occurred at package "<< endl;
    }
    clock_gettime(CLOCK_MONOTONIC, &delay_a);
    ++recent_package;
    ++cnt_package;

    if(delay_a.tv_sec - delay_c.tv_sec >= 2)
    {
        uint32_t avg_delay = total_delay / recent_package;
        uint32_t avg_speed = recent_bytes / ((delay_a.tv_sec-delay_c.tv_sec) + (delay_a.tv_nsec - delay_c.tv_nsec) / 1e9) * 8;

        cout << max_delay << " | " << min_delay << " | avg: " << avg_delay << " speed:  " << avg_speed << "bps" <<  endl;

        report_delay(max_delay, min_delay, avg_delay, avg_speed);
        avg_delay = 0;
        min_delay = INT_MAX;
        max_delay = 0;
        recent_package = 0;
        recent_bytes = 0;
        total_delay = 0;
        delay_c = delay_a;


      static int cnt = 0;
      cout << "sending: " << cnt++ << endl;
      delay_c = delay_a;
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



void report_delay(uint32_t max_delay,uint32_t min_delay,uint32_t avg_delay,uint32_t avg_speed)
{
  //消息准备
  char data[32];
  memset(data,0,sizeof(data));
  control_message* temp=(control_message*)data;
  temp->max_delay=max_delay;temp->min_delay=min_delay;temp->avg_delay=avg_delay,temp->avg_speed=avg_speed;
  //发送准备
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
  target_addr.sin_port = htons(control_port);
  target_addr.sin_addr.s_addr = inet_addr(control_address.c_str());

  sendto(my_socket, data, sizeof(data), 0,
                   (sockaddr *)&target_addr, sizeof(target_addr));
}