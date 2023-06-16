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
#include <vector>
#include <queue>
#include <iostream>
#include <thread>
#include <vector>

#include "json.hpp"
using namespace std;
using json = nlohmann::json;
#define RECEIVER_ADDRESS "127.0.0.1"  // 目的地址

const int max_num_of_packet_id_allowed = 5000;
int cur_num_of_packet_id_allowed = 0;

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
  uint32_t max_packet_id;
  uint32_t total_packet_num;
  timespec last_min_max_delay_record;
  queue<uint32_t> recent_packet_id;
};


string generateRandomString() {
  const int length = 64;
  const string letters = "0123456789"
                          "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                          "!\"#$%&'()*+,-./:;<=>?@[\\]^_`{|}~";
  srand(time(NULL));
  string result;
  for (int i = 0; i < length; i++) {
      result += letters[rand() % letters.length()];
  }
  return result;
}

string receiver_id = generateRandomString();

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
  string duplex_server_address, duplex_client_address;
  int duplex_server_port, duplex_client_port;


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

  void set_header() {
    memset(datagram, 0, 2048);  // zero out the packet buffer
    char *data = datagram;
    my_package *temp = (my_package *)data;
    temp->source_user_id = pack.source_user_id;
    temp->source_module_id = pack.source_module_id;
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
    target_addr.sin_addr.s_addr = inet_addr(address.c_str());
    return target_addr;
  }

  int send_thread(int port, long package_num, int delay, int packageSize) {
    // 初始化socket
    int my_socket = get_init_socket("0.0.0.0", 2222);
    int duplex_socket = get_init_socket("0.0.0.0", 2233);
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
    timespec delay_a, delay_c;
    delay_a = {0, 0};
    delay_c = {0, 0};
    int error = 0;
    // 开始发送
    for (int k = 0; ; k++) {
      // sprintf(sendBuf, "%d", i);
      // 固定化包间间隔
      memset(buffer, 0, sizeof(buffer));
      readLen = recvfrom(recv_socket, buffer, package_size+sizeof(my_package), 0, (sockaddr *)&sender_addr, &sender_addrLen);

      for(int i = 0;i < readLen - sizeof(my_package); i++) {
        datagram[i] = buffer[i + sizeof(my_package)];
      }

      // 从buffer中读取时间戳
      my_package* ptr = reinterpret_cast<my_package*>(buffer); 

      cout << "head: " << ptr->tunnel_id << " " << ptr->source_module_id << " ";
      cout << ptr->source_user_id << " ";
      cout << ptr->source_module_id << " ";
      cout << ptr->dest_user_id << " ";
      cout << ptr->flow_id << " ";
      cout << ptr->service_id << " ";
      cout << ptr->qos_id << " ";
      cout << ptr->packet_id << " ";
      cout << ptr->timestamp.tv_sec << "." << ptr->timestamp.tv_nsec << " ";
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

      cout << "time_diff:(ns) " << time_diff << endl;

      uint32_t delay_us = time_diff / 1000;

      if(flow_msg.count(ptr->flow_id)) {
        auto &msg = flow_msg[ptr->flow_id];
        if(get_time_diff(flow_msg[ptr->flow_id].last_min_max_delay_record, now) < 2e9) {
          msg.packet_num++;
          msg.sum_delay += delay_us;
          msg.max_delay = max(delay_us, msg.max_delay);
          msg.min_delay = min(delay_us, msg.min_delay);
          msg.byte_num += readLen - sizeof(my_package); // no 包头
        } 
        msg.max_packet_id = max(msg.max_packet_id, ptr->packet_id);
        // cout << msg.max_packet_id << "???" << endl;
        msg.total_packet_num++;
      } else {
        msg_for_each_stream msg;
        msg.packet_num = 1;
        msg.sum_delay = delay_us;
        msg.max_delay = delay_us;
        msg.min_delay = delay_us;
        msg.byte_num = readLen; // 算上包头
        msg.max_packet_id = ptr->packet_id;
        msg.total_packet_num = 1;
        clock_gettime(CLOCK_REALTIME, &msg.last_min_max_delay_record);

        flow_msg[ptr->flow_id] = msg;
      }

      bool not_init_msg = false;

      if(!flow_msg.count(ptr->flow_id)) { // 没有，则初始化
        not_init_msg = true;
        msg_for_each_stream msg;
        msg.packet_num = 1;
        msg.sum_delay = delay_us;
        msg.max_delay = delay_us;
        msg.min_delay = delay_us;
        msg.byte_num = readLen; // 算上包头
        msg.max_packet_id = ptr->packet_id;
        msg.total_packet_num = 1;
        clock_gettime(CLOCK_REALTIME, &msg.last_min_max_delay_record);
        flow_msg[ptr->flow_id] = msg;
      } else { // 有了，则更新
        auto &msg = flow_msg[ptr->flow_id];
        msg.packet_num++;
        msg.sum_delay += delay_us;
        msg.max_delay = max(delay_us, msg.max_delay);
        msg.min_delay = min(delay_us, msg.min_delay);
        msg.byte_num += readLen - sizeof(my_package); // no 包头
        msg.max_packet_id = max(msg.max_packet_id, ptr->packet_id);
        cout << msg.max_packet_id << "???" << endl;
        msg.total_packet_num++;
      }

      cur_num_of_packet_id_allowed++; // 如果超过了，就缩减10% 
      // 添加id序列
      auto &msg = flow_msg[ptr->flow_id];
      msg.recent_packet_id.push(ptr->packet_id);
      if(cur_num_of_packet_id_allowed > 
      max_num_of_packet_id_allowed) {
        auto pop_num = msg.recent_packet_id.size() / 10;
        pop_num = pop_num >= 3 ? pop_num : 0; // 
        for(auto i = 0; i < pop_num; ++i) {
          msg.recent_packet_id.pop();
        }
        cur_num_of_packet_id_allowed -= pop_num;
      }


      if(flow_msg.count(ptr->flow_id) && get_time_diff(flow_msg[ptr->flow_id].last_min_max_delay_record, now) > 1e9) {
        // 超过一定时间了，除去最大id和包总数，全部初始化
        msg_for_each_stream msg;
        msg.packet_num = 1;
        msg.sum_delay = delay_us;
        msg.max_delay = delay_us;
        msg.min_delay = delay_us;
        msg.total_packet_num = 1;
        clock_gettime(CLOCK_REALTIME, &msg.last_min_max_delay_record);
        flow_msg[ptr->flow_id] = msg;
      }

      cout << "now: delays: " << msg.max_delay << " | " << msg.min_delay << '|' << msg.byte_num << '|' << msg.packet_num << endl;

      // strncpy(datagram, buffer + sizeof(my_package), readLen - sizeof(my_package));
      // 发送给VLC 仅此端口为视频业务
      if(ptr->flow_id == 23023) {
        cout << "视频发送" << endl;
        error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
        if (error == -1) {
          perror("sendto");
          cout <<"sendto() error occurred at package "<< endl;
        }
      }

      if(ptr->source_module_id == 100) {
        // 客户端发送给服务器
        cout << "客户端发送给服务器" << endl;
        sockaddr_in duplex_target_addr = get_sockaddr_in(real_address(duplex_client_address), duplex_client_port);
        error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&duplex_target_addr, sizeof(duplex_target_addr));
        if (error == -1) { perror("sendto"); cout <<"sendto() error occurred at package "<< endl; }
      } else if(ptr->source_module_id == 200) {
        // 服务器发送给客户端
        cout << "服务器发送给客户端" << endl;
        cout << duplex_client_address << " " << duplex_client_port << endl;
        sockaddr_in duplex_target_addr = get_sockaddr_in(real_address(duplex_server_address), duplex_server_port);
        error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&duplex_target_addr, sizeof(duplex_target_addr));
        if (error == -1) { perror("sendto"); cout <<"sendto() error occurred at package "<< endl; }
      }
      clock_gettime(CLOCK_MONOTONIC, &delay_a);

      if(delay_a.tv_sec - delay_c.tv_sec >= 1) {
        report_delay();
        delay_c = delay_a;

        static int cnt = 0;
        cout << "sending: " << cnt++ << endl;
        delay_c = delay_a;
      }
    }
    sleep(1);
    return 0;
  }

  void report_delay()
  {
    //发送准备
    json j;
    for(auto it = flow_msg.begin(); it != flow_msg.end(); ++it) {
      int key = it->first;
      msg_for_each_stream value = it->second;
      auto id_queue = value.recent_packet_id;
      json packet_id_list_json;
      while (!id_queue.empty()) {
          packet_id_list_json.push_back(id_queue.front());
          id_queue.pop();
      }

      json j2 = {
        {"packet_num", value.packet_num},
        {"byte_num", value.byte_num},
        {"sum_delay", value.sum_delay},
        {"max_delay", value.max_delay},
        {"min_delay", value.min_delay},
        {"loss_rate", value.loss_rate},
        {"last_min_max_delay_record", value.last_min_max_delay_record.tv_sec},
        {"max_packet_id", value.max_packet_id},
        {"total_packet_num", value.total_packet_num},
        {"pod_id", receiver_id},
        {"id_list", packet_id_list_json}
      };
      j[to_string(key)] = j2;
    }
    cout << j.dump() << endl;
    // cout << j.dump(4) << endl;
    string message = j.dump();

    auto my_socket = get_init_socket("0.0.0.0", 2222);
    auto target_addr = get_sockaddr_in(control_address, control_port);
    sendto(my_socket, message.c_str(), message.length(), 0, (sockaddr *)&target_addr, sizeof(target_addr));
  }

};

// ----------------

int main(int argc, char *argv[]) {
  Receiver receiver(argv[1], argv[2]);
  receiver.start();
}