// 网页 23100 C 23200 S
// 短消息 C1：23100 C2：23200 S-C1:23300 S-C2:23400
#include "header.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;
#define RECEIVER_ADDRESS "127.0.0.1"  // 目的地址

string send_to_address = "162.105.85.120";
bool should_print_log = false;

void print_log(string s) {
  if(!should_print_log) {
    return;
  }
  cout << s << endl;
}

json compress_packet_id_list(json& packet_id_list_json) {
  sort(packet_id_list_json.begin(), packet_id_list_json.end());
  json pack;                  // 新list
  uint32_t val = packet_id_list_json[0];
  pack.push_back(val);
  for (int i = 1; i<packet_id_list_json.size(); i++){
      uint32_t val_new = packet_id_list_json[i];
      if (val_new == val + 1){
          //puts("lianxxu");
      }
      else {
          pack.push_back(uint32_t(-1));    // 分隔符 -1
          pack.push_back(uint32_t(val));
          pack.push_back(uint32_t(val_new));
      }
      val = val_new;
  }
  pack.push_back(uint32_t(-1));           // 分隔符 -1
  pack.push_back(uint32_t(val));
  return pack;
}

string generateRandomString() {
  const int length = 64;
  const string letters = "0123456789"
                          "abcdefghijklmnopqrstuvwxyz"
                          "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
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
    last_time = get_current_time();
  }

  timespec last_time;
  int package_size = 48048, package_speed, delay, report_interval;
  long package_num;
  int server_port, video_out;
  int control_port;
  string client_address;  // 初始发送参数，在程序开始时指定
  string control_address;  // 初始发送参数，在程序开始时指定
  char datagram[48048];
  my_package pack;
  string duplex_server_address, duplex_client_address;
  int duplex_server_port, duplex_client_port;

  unordered_map<int, msg_for_each_stream> flow_msg;

  string real_address(string address) {
    struct hostent *host;
    host = gethostbyname(address.c_str());
    if (host == NULL) {
      cout << "gethostbyname error: " << address << endl;
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
    send_to_address = j["send_to_address"];
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
    should_print_log = j["should_print_log"];
    
    srcFile.close();
    return;
  }

  void set_header() {
    memset(datagram, 0, 10000);  // zero out the packet buffer
    char *data = datagram;
    my_package *temp = (my_package *)data;
    temp->source_user_id = pack.source_user_id;
    temp->source_module_id = pack.source_module_id;
    temp->dest_user_id = pack.dest_user_id;
    return;
  }

  int set_bufsize(int recv_size, int send_size, int sock_) {
      if (setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, (char*)&recv_size, sizeof(int)) == -1) return -1;
      return setsockopt(sock_, SOL_SOCKET, SO_RCVBUF, (char*)&send_size, sizeof(int));
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

  void print_head_msg(my_package* ptr) {
    if(!should_print_log) 
      return;
    cout << "head: ";
    cout << ptr->source_id << " "; 
    cout << ptr->destination_ip << " "; 
    cout << ptr->tunnel_id << " "; 
    cout << ptr->source_module_id << " ";
    cout << ptr->source_user_id << " ";
    cout << ptr->dest_user_id << " ";
    cout << ptr->flow_id << " ";
    cout << ptr->service_id << " ";
    cout << ptr->qos_id << " ";
    cout << ptr->packet_id << " ";
    cout << ptr->timestamp.tv_sec << "." << ptr->timestamp.tv_nsec << " ";
    cout << ptr->ext_flag << endl;
  }

  msg_for_each_stream single_msg_init(uint32_t delay_us, int readLen, uint32_t packet_id, bool timeout = false) {
    msg_for_each_stream msg;
    msg.packet_num = 1;
    msg.sum_delay = delay_us;
    msg.max_delay = delay_us;
    msg.min_delay = delay_us;
    msg.byte_num = readLen; // 算上包头
    if(!timeout) {
      msg.max_packet_id = packet_id;
      msg.total_packet_num = 1;
    }
    clock_gettime(CLOCK_REALTIME, &msg.last_min_max_delay_record);
    return msg;
  }

  void update_single_msg(uint32_t flow_id, uint32_t delay_us, uint32_t packet_id, int readLen, int colored) {
    auto &msg = flow_msg[flow_id];
    msg.packet_num++;
    msg.sum_delay += delay_us;
    msg.max_delay = max(delay_us, msg.max_delay);
    msg.min_delay = min(delay_us, msg.min_delay);
    msg.byte_num += readLen; //  - sizeof(my_package); // no 包头
    msg.max_packet_id = max(msg.max_packet_id, packet_id);
    if(should_print_log) {
      cout << "max_packet_id" << msg.max_packet_id << endl;
    }
    msg.total_packet_num++;
  }

  void adjust_packet_id_queue(my_package* ptr) {
    cur_num_of_packet_id_allowed++; // 如果超过了，就缩减10% 
    // 添加id序列
    auto &msg = flow_msg[ptr->flow_id];
    msg.recent_packet_id.push(ptr->packet_id);
    if(cur_num_of_packet_id_allowed > 
    max_num_of_packet_id_allowed) {
      if(should_print_log) {
        cout << "cur_num > max_num" << endl;
      }
      auto pop_num = msg.recent_packet_id.size() / 10;
      pop_num = pop_num >= 3 ? pop_num : 0; // 
      for(auto i = 0; i < pop_num; ++i) {
        if(!msg.recent_packet_id.empty()) {
          msg.recent_packet_id.pop();
          cur_num_of_packet_id_allowed -= pop_num;
        }
      }
    }
  }

  timespec get_current_time() {
    timespec now = {};  // 生成新的 timestamp
    clock_gettime(CLOCK_REALTIME, &now);
    return now;
  }

  void print_timestamp(my_package* ptr) {
    if(!should_print_log) return;
    std::tm tm = *std::localtime(&ptr->timestamp.tv_sec);
    std::cout << "timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(9) << std::setfill('0') << ptr->timestamp.tv_nsec << std::endl;
  }

  void print_msg(my_package* ptr) {
    if(!should_print_log) return;
    auto &msg = flow_msg[ptr->flow_id];
    cout << "业务id: " << ptr->flow_id << ": delays: max: " << msg.max_delay << ", min: " << msg.min_delay << ", byte_num: " << msg.byte_num << ", packet_num: " << msg.packet_num << endl;

  }

  void update_colored_packet_num(my_package* ptr) {
    
  }

  void update_flow_msg(my_package* ptr, int readLen) {
    // 计算延迟 ns
    auto get_time_diff = [](timespec t1, timespec t2)->double {
      return (t2.tv_sec - t1.tv_sec) * 1e9 + (t2.tv_nsec - t1.tv_nsec);
    };

    timespec now = get_current_time();

    double time_diff = get_time_diff(ptr->timestamp, now);

    uint32_t delay_us = time_diff / 1000;
    if(should_print_log) {
      cout << "delay us: " << delay_us << endl;
    }
    // if(delay_us > 1e8) {
    //   delay_us = 0;
    // }

    if(!flow_msg.count(ptr->flow_id)) { // 没有，则初始化
      cout << "单条业务流初始化" << endl;
      flow_msg[ptr->flow_id] = single_msg_init(delay_us, readLen, ptr->packet_id);
    } else { // 有了，则更新
      update_single_msg(ptr->flow_id, delay_us, ptr->packet_id, readLen, ptr->ext_flag); 
    }

    adjust_packet_id_queue(ptr);
    if(should_print_log) {
      cout << "now" << now.tv_sec << " || msg: " << flow_msg[ptr->flow_id].last_min_max_delay_record.tv_sec << endl;
      cout << "time diff ai" << get_time_diff(flow_msg[ptr->flow_id].last_min_max_delay_record, now) << endl;
      print_msg(ptr);
    }
    if(flow_msg.count(ptr->flow_id) && get_time_diff(last_time, get_current_time()) > 2e9) {
      last_time = get_current_time();
      // 报告，超过一定时间了，除去最大id和包总数，全部初始化(所有id都要初始化)
      // 并设置时间
      report_flow_message(); 
      cout << "msg reinit" << endl;
      // for(auto it = flow_msg.begin(); it != flow_msg.end(); ++it) {
      //   int key = it->first;
      //   auto& value = it->second;
      //   value = single_msg_init(delay_us, readLen, ptr->packet_id);
      // }
      flow_msg.clear();
      cur_num_of_packet_id_allowed = 0;
      // cout << "count: " << flow_msg.count(ptr->flow_id);
    }
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

    set_bufsize(10 * 1024 * 1024, 10*1024*1024, recv_socket);
    // 接收准备
    sockaddr_in sender_addr;
    socklen_t sender_addrLen = sizeof(sender_addr);
    char buffer[package_size];
    int readLen = 0;
    int error = 0;
    // 开始发送
    for (int k = 0; ; k++) {
      // 固定化包间间隔
      memset(buffer, 0, sizeof(buffer));
      readLen = recvfrom(recv_socket, buffer, package_size+sizeof(my_package), 0, (sockaddr *)&sender_addr, &sender_addrLen);
      // cout << "read len " << readLen << endl;

      memmove(datagram, &buffer[sizeof(my_package)], readLen - sizeof(my_package));

      // 从buffer中读取时间戳
      my_package* ptr = reinterpret_cast<my_package*>(buffer); 

      print_head_msg(ptr);
      print_timestamp(ptr);
      update_flow_msg(ptr, readLen);

      // cout << ptr->tunnel_id << " send type " << endl;
      switch(ptr->tunnel_id) {
        case 3: { // 短消息
          cout << "短消息" << endl;
          receive_web_short_msg(ptr->source_module_id == 100 ? true : false, my_socket, readLen);
          break;
        }
        case 4: { // 纯转发
          // cout << "纯转发 | 视频流" << endl;
          // cout << "readLen: " << readLen << endl;
          receive_video_stream(my_socket, target_addr, readLen);
          break;
        }
        case 5: { // ip电话
          receive_ip_phone(ptr->source_module_id == 100 ? true : false, my_socket, readLen);
          break;
        }
        case 6: { // 网页
          receive_web(ptr->source_module_id == 100 ? true : false, my_socket, readLen, false);
          break;
        }
        case 11: {
          receive_tencent_video(ptr->source_module_id == 100 ? true : false, my_socket, readLen, ptr->tunnel_id);
        } break;
        case 12: {
          receive_tencent_video(ptr->source_module_id == 100 ? true : false, my_socket, readLen, ptr->tunnel_id);
        } break;
        case 13: {
          receive_tencent_video(ptr->source_module_id == 100 ? true : false, my_socket, readLen, ptr->tunnel_id);
        } break;
        default: break;
      }
    }
    return 0;
  }

  void report_flow_message() {
    json j;
    for(auto it = flow_msg.begin(); it != flow_msg.end(); ++it) {
      int key = it->first;
      msg_for_each_stream value = it->second;
      auto id_queue = value.recent_packet_id;
      if(should_print_log) {
        cout << id_queue.size() << "size" << endl;
      }
      json packet_id_list_json;
      while (!id_queue.empty()) {
          packet_id_list_json.push_back(id_queue.front());
          if(should_print_log) {
            cout << id_queue.front() << " q " << endl;
          }
          id_queue.pop();
      }

      // compress packet id 
      packet_id_list_json = compress_packet_id_list(packet_id_list_json);

      // end
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
        {"id_list", packet_id_list_json},
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

  void receive_ip_phone(bool from_client, const int& my_socket, const int& readLen) {
    if(from_client) { // from client
      short_send_to(my_socket, readLen, "162.105.85.70", 52700, "ip phone: from client. sendto: 52700");
    } else { // from server
      short_send_to(my_socket, readLen, "162.105.85.70", 52701, "ip phone：from server. sendto: 52701");
    }
  }

  void receive_web(bool from_client, const int& my_socket, const int& readLen, const bool web_log) {
    if(from_client) {
      short_send_to(my_socket, readLen, real_address("real-data-back"), 23101, "网页: from client. sendto: 23101", web_log);
    } else { // from server
      short_send_to(my_socket, readLen, real_address("real-data-back"), 23201, "网页: from server. sendto: 23201", web_log);
    }
  }

  void receive_web_short_msg(bool from_client, const int& my_socket, const int& readLen) {
    if(from_client) {
      short_send_to(my_socket, readLen, "162.105.85.70", 23100, "短消息");
    } else {
      short_send_to(my_socket, readLen, "162.105.85.70", 23200, "短消息");
    }
  }

  void receive_tencent_video(bool from_client, const int& my_socket, const int& readLen, const int type, bool print_msg = false) {
    switch(type) {
      case 11: {
        if(from_client) {
          short_send_to(my_socket, readLen, "162.105.85.70", 52701, "腾讯会议: from client.");
        } else {
          short_send_to(my_socket, readLen, "162.105.85.110", 52700, "腾讯会议: from server");
        }
        break;
      }
      case 12: {
        if(from_client) {
          short_send_to(my_socket, readLen, "162.105.85.188", 52700, "腾讯会议: from client.");
        } else {
          short_send_to(my_socket, readLen, "162.105.85.58", 52700, "腾讯会议: from server");
        }
        break;
      }
    }
  } 

  void receive_video_stream(const int& my_socket, const sockaddr_in& target_addr, const int& readLen, bool print_msg = false) {
    if(should_print_log) {
      cout << "视频发送" << endl;
    }
    auto error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
    if (error == -1) {
      perror("sendto");
      cout <<"sendto() error occurred at package "<< endl;
    }
  }

  void short_send_to(const int& my_socket, const int& readLen, string target_address, int target_port, const string& msg, bool print_msg = true) {
    if(print_msg)
      cout << msg << "| " << target_address << ":" << target_port << endl;
    sockaddr_in duplex_target_addr = get_sockaddr_in(target_address, target_port);
    auto error = sendto(my_socket, datagram, readLen - sizeof(my_package), 0, (sockaddr *)&duplex_target_addr, sizeof(duplex_target_addr));
    if (error == -1) { perror("sendto"); cout <<"sendto() error occurred at package "<< endl; }
  }

};

int main(int argc, char *argv[]) {
  Receiver receiver(argv[1], argv[2]);
  receiver.start();
}
