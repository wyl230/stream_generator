#include <arpa/inet.h>
#include <map>
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
  uint32_t  insId=0;
  uint32_t  unused3=0;
};
// 包数，字节总数，延迟总和，最大时延，最小时延，丢包率
struct msg_for_each_stream {
  uint32_t packet_num;
  uint32_t byte_num;
  uint32_t sum_delay;
  uint32_t max_delay;
  uint32_t min_delay;
  uint32_t loss_rate;
  timespec last_min_max_delay_record;
};

class Receiver {
public:
  Receiver(string client_address, string control_address):client_address(client_address), control_address(control_address) {
    cout << "client address: " << client_address << endl;
    cout << "control address: " << control_address << endl;
  }

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

  map<int, msg_for_each_stream> flow_msg;
  
  string real_address(string address) {
    struct hostent *host;
    host = gethostbyname(address.c_str());
    if (host == NULL) {
      cout << "gethostbyname error" << endl;
      return "gethostbyname error";
    }
    return inet_ntoa(*(struct in_addr *)host->h_addr_list[0]);
  }

  void param_config() {
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

  void set_header() {
    memset(datagram, 0, 2048);  // zero out the packet buffer
    char *data = datagram;
    my_package *temp = (my_package *)data;
    temp->source_user_id = pack.source_user_id;
    temp->dest_user_id = pack.dest_user_id;
    return;
  }

  void start() {
    param_config();
    cout << "server port" << server_port << endl;

    client_address = real_address(client_address);
    cout << "client: " << client_address << endl;
    control_address = real_address(control_address);
    cout << "control: " << client_address << endl;

    set_header();

    cout << "传输开始" << endl;
    std::thread send_t(&Receiver::send_thread, this, 0, package_num, delay, package_size);
    while (1) {
      sleep(1);
    }
  }

  int get_init_socket(string address, int port) {
    int my_socket;
    sockaddr_in my_addr;
    my_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if(port == -1) {
      return my_socket;
    }
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    my_addr.sin_addr.s_addr = inet_addr(address.c_str());
    // 绑定端口
    bind(my_socket, (sockaddr *)&my_addr, sizeof(my_addr));
    return my_socket;
  }

  sockaddr_in get_sockaddr_in(string address, int port) {
    sockaddr_in target_addr;
    target_addr.sin_family = AF_INET;
    target_addr.sin_port = htons(port);
    target_addr.sin_addr.s_addr = inet_addr(client_address.c_str());
    return target_addr;
  }

  int send_thread(int port, long package_num, int delay, int packageSize) {
    // 初始化socket
    int my_socket = get_init_socket("0.0.0.0", 2222);
    // 指定目标
    sockaddr_in target_addr = get_sockaddr_in(client_address, video_out);

    int recv_socket = get_init_socket("0.0.0.0", -1);
    auto recv_addr = get_sockaddr_in("0.0.0.0", server_port);
    bind(recv_socket, (sockaddr *)&recv_addr, sizeof(recv_addr));

    // 接收准备
    sockaddr_in sender_addr;
    socklen_t sender_addrLen = sizeof(sender_addr);
    char buffer[package_size];
    int readLen = 0;
    uint32_t cnt_package = 0,recent_package = 0;
    uint32_t total_delay;
    timespec delay_a, delay_b,delay_c;
    uint32_t max_delay = 0, min_delay = INT_MAX, avg_delay = 0, recent_delay = 0,avg_speed = 0;
    delay_a = {0, 0};
    delay_b = {0, 0};
    delay_c = {0, 0};
    int error = 0;
    // 开始发送
    for (int k = 0; ; k++) {
      // sprintf(sendBuf, "%d", i);
      // 固定化包间间隔
      memset(buffer, 0, sizeof(buffer));
      readLen = recvfrom(recv_socket, buffer, package_size+sizeof(my_package), 0, (sockaddr *)&sender_addr, &sender_addrLen);

      recent_bytes += readLen - sizeof(my_package);

      for(int i = 0;i < readLen - sizeof(my_package); i++) {
        datagram[i] = buffer[i + sizeof(my_package)];
      }

      // 从buffer中读取时间戳
      my_package* ptr = reinterpret_cast<my_package*>(buffer); 

      cout << ptr->tunnel_id << endl;
      cout << ptr->source_module_id << endl;
      cout << ptr->source_user_id << endl;
      cout << ptr->dest_user_id << endl;
      cout << ptr->flow_id << endl;
      cout << ptr->service_id << endl;
      cout << ptr->qos_id << endl;
      cout << ptr->packet_id << endl;
      cout << ptr->timestamp.tv_sec << "." << ptr->timestamp.tv_nsec << endl;
      cout << ptr->ext_flag << endl;



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


      uint32_t delay_us = time_diff / 1000;

      if(flow_msg.count(ptr->flow_id) && get_time_diff(flow_msg[ptr->flow_id].last_min_max_delay_record, now) < 1e9) {
        auto &msg = flow_msg[ptr->flow_id];
        msg.packet_num++;
        msg.sum_delay += delay_us;
        msg.max_delay = max(delay_us, msg.max_delay);
        msg.min_delay = min(delay_us, msg.min_delay);
        msg.byte_num += readLen; // 算上包头

        double time_diff_ms = get_time_diff(msg.last_min_max_delay_record, now) / 1000000;
        cout << "time_diff_ms record: " << time_diff_ms << endl;
      } else {
        msg_for_each_stream msg;
        msg.packet_num = 1;
        msg.sum_delay = delay_us;
        msg.max_delay = delay_us;
        msg.min_delay = delay_us;
        msg.byte_num = readLen; // 算上包头
        clock_gettime(CLOCK_REALTIME, &msg.last_min_max_delay_record);

        flow_msg[ptr->flow_id] = msg;
      }

      auto &msg = flow_msg[ptr->flow_id];
      cout << "now: delays: " << msg.max_delay << " | " << msg.min_delay << '|' << msg.byte_num << '|' << msg.packet_num << endl;

      // strncpy(datagram, buffer + sizeof(my_package), readLen - sizeof(my_package));
      // 发送给VLC 仅此端口为视频业务
      if(ptr->flow_id == 23023) {
        error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
        if (error == -1) {
          perror("sendto");
          cout <<"sendto() error occurred at package "<< endl;
        }
      }
      clock_gettime(CLOCK_MONOTONIC, &delay_a);
      ++recent_package;
      ++cnt_package;

      if(delay_a.tv_sec - delay_c.tv_sec >= 2) {
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

  void report_delay(uint32_t max_delay,uint32_t min_delay,uint32_t avg_delay,uint32_t avg_speed)
  // 这里每0.5秒报告一次，报告消息为 json格式 
  {
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

    json j;
    for(auto it = flow_msg.begin(); it != flow_msg.end(); ++it) {
      int key = it->first;
      msg_for_each_stream value = it->second;
      json j2 = {
        {"packet_num", value.packet_num},
        {"byte_num", value.byte_num},
        {"sum_delay", value.sum_delay},
        {"max_delay", value.max_delay},
        {"min_delay", value.min_delay},
        {"loss_rate", value.loss_rate},
        {"last_min_max_delay_record", value.last_min_max_delay_record.tv_sec}
      };
      j[to_string(key)] = j2;
    }
    cout << j.dump() << endl;
    // cout << j.dump(4) << endl;
    string message = j.dump();

    sendto(my_socket, message.c_str(), message.length(), 0, (sockaddr *)&target_addr, sizeof(target_addr));
  }

};

// ----------------

int main(int argc, char *argv[]) {
  Receiver receiver(argv[1], argv[2]);
  receiver.start();
}