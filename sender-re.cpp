// sender中打上时间戳
// tunnel_id 用来当作send_type

#include "header.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;
// #define RECEIVER_ADDRESS "127.0.0.1"  // 目的地址
bool should_print_log = true;
int print_cnt = 0;
int loss_rate = 0;

uint32_t global_packet_id = 0;
int recv_thread(int port, int package_size);
void client_init();

void data_generate(char *package_head);
int package_size = 20480, package_speed, delay, report_interval;
long package_num;
int client_port, video_in;
bool auto_send = false;
bool send_to_core_net = false;
int send_type = 0; // 1 恒比特 2 变比特 
int control_port, server_port;// client:接入网 server：本pod接收程序 control；控制程序
char datagram[20480];
my_package pack;
string client_address;
string json_file_name;

string duplex_server_address, duplex_client_address;
int duplex_server_port, duplex_client_port;

void print_head_msg(my_package* ptr) {
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
  client_address = argv[1];
  global_packet_id = stoi(string(argv[2]));
  client_init();
  /* global_packet_id = stoi(string(argv[2])); */
  // std::ifstream ifs("packet_id.json");
  // json jf = json::parse(ifs);
  // global_packet_id = jf[to_string(pack.flow_id).c_str()];

  cout << "client address:" << client_address << endl;
  cout << server_port << endl;
  cout << "接收开始" << endl;
  recv_thread(server_port,package_size);
}

//把字符串的ip转换成uint32_t
inline uint32_t str_to_ip(const std::string& ip_str) {
    struct in_addr addr;
    inet_pton(AF_INET, ip_str.c_str(), &addr);
    return htonl(addr.s_addr);
    // return ntohl(addr.s_addr);
}

void client_init() {
  ifstream srcFile("./init.json", ios::binary);
  if (!srcFile.is_open()) {
    cout << "Fail to open src.json" << endl;
    return;
  }
  json j;
  srcFile >> j;
  loss_rate = j["loss_rate"];
  should_print_log = j["should_print_log"];
  pack.source_user_id = j["source_id"];
  pack.source_module_id = j["source_module_id"];
  pack.tunnel_id = j["send_type"];
  pack.dest_user_id = j["dest_id"];
  pack.source_id = pack.source_user_id;
  pack.destination_ip = str_to_ip(string("192.168.") + to_string(pack.dest_user_id) + string(".1"));
  pack.flow_id = j["flow_id"];
  package_num = j["package_num"];
  package_speed = j["package_speed"];
  control_port = j["control_port"];
  server_port = j["server_port"];
  client_port = j["client_port"];
  report_interval = j["report_interval"];
  video_in = j["video_in"];
  duplex_client_address = j["duplex_client_address"];
  duplex_server_address = j["duplex_server_address"];
  duplex_client_port = j["duplex_client_port"];
  duplex_server_port = j["duplex_server_port"];
  auto_send = j["auto_send"];
  send_type = j["send_type"];
  srcFile.close();

  if(pack.source_user_id == 33000 || pack.dest_user_id == 33000) {
    send_to_core_net = true;
  }
  return;
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
void write_to_head(my_package *ptr) {
  timespec now = {};  // 生成新的 timestamp
  clock_gettime(CLOCK_REALTIME, &now);
  ptr->timestamp = now;  // 将 timestamp 赋值为新的值
  ptr->source_user_id = pack.source_user_id;
  ptr->source_module_id = pack.source_module_id;
  ptr->tunnel_id = pack.tunnel_id;
  ptr->dest_user_id = pack.dest_user_id;
  ptr->flow_id = pack.flow_id;
  ptr->packet_id = global_packet_id++;
  cout << "global_packet_id: " << global_packet_id << ptr->packet_id << endl;
  cout << ptr->destination_ip << "ip | " << ptr->source_id << endl;

  std::tm tm = *std::localtime(&ptr->timestamp.tv_sec);
  if(should_print_log && print_cnt++ % 100 == 0) {
    std::cout << print_cnt << " timestamp: " << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << "." << std::setw(9) << std::setfill('0') << ptr->timestamp.tv_nsec << std::endl;
  }
}


int recv_thread(int port, int package_size) {
  int num = 0;
  // socket初始化
  int recv_socket;
  sockaddr_in recv_addr, sender_addr;

  if(send_type == 3 || send_type == 6 || (send_type >= 11 && send_type <= 13)) {
    auto port = (pack.source_module_id == 100 ? duplex_client_port : duplex_server_port);
    recv_socket = get_init_socket("0.0.0.0", port);
  } else if (send_type == 4) {
    recv_socket = get_init_socket("0.0.0.0", video_in);
  }

  int my_socket = get_init_socket("0.0.0.0", -1);
  sockaddr_in target_addr = get_sockaddr_in(real_address(client_address).c_str(), client_port);

  // 接收准备
  socklen_t sender_addrLen = sizeof(sender_addr);
  char buffer[package_size];
  char package_head[package_size+sizeof(my_package)];
  int readLen = 0;
  uint32_t cnt_package = 0,recent_package=0;
  uint32_t total_delay;
  timespec delay_a, delay_c;
  uint32_t max_delay=0,min_delay=INT_MAX,avg_delay=0, recent_delay=0,avg_speed=0;
  delay_a = {0, 0};
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
    } else if(send_type == 4) {
      // 视频流
      readLen = recvfrom(recv_socket, buffer, package_size, 0, (sockaddr *)&sender_addr, &sender_addrLen);
      strcpy(package_head + sizeof(my_package), buffer);
    } else if(send_type == 3) {
      // 短消息
      cout << "recv port: " << (pack.source_module_id == 100 ? duplex_client_port : duplex_server_port) << endl;
      readLen = recvfrom(recv_socket, buffer, package_size, 0, (sockaddr *)&sender_addr, &sender_addrLen);
      strcpy(package_head + sizeof(my_package), buffer);
      cout << "received: 短消息 " << readLen << endl;
    } else if(send_type == 6) { // 双向业务，接收转发来的client
      cout << "recv port: " << (pack.source_module_id == 100 ? duplex_client_port : duplex_server_port) << endl;

      readLen = recvfrom(recv_socket, buffer, package_size, 0, (sockaddr *)&sender_addr, &sender_addrLen);

      strcpy(package_head + sizeof(my_package), buffer);
      cout << "received: 网页流 " << readLen << endl;
    } else if(send_type == 10) {
      return -1;
    } else { // 接受真正的视频流
      readLen = recvfrom(recv_socket, buffer, package_size, 0, (sockaddr *)&sender_addr, &sender_addrLen);
      strcpy(package_head + sizeof(my_package), buffer);
    }
    //转发视频流

    // package_head
    // 加上时间戳
    my_package* ptr = reinterpret_cast<my_package*>(package_head);  // 将其强制转换为my_package指针类型

    write_to_head(ptr);

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
      if(should_print_log) {
        print_head_msg(ptr);
      }
      static uint32_t cnt = 0;
      if(cnt++ % 100 >= loss_rate) {
        sendto(my_socket, package_head, readLen+sizeof(my_package), 0, (sockaddr *)&target_addr, sizeof(target_addr));
      }
    }
    // 写入packet_id到文件中 json格式 [id: packet_id]
    // static int cnt = 0;
    // if(cnt++ % 100 == 0) {
    //   std::ifstream ifs("packet_id.json");
    //   json jf = json::parse(ifs);

    //   jf[to_string(pack.flow_id).c_str()] = global_packet_id;
    //   std::ofstream file("packet_id.json");
    //   file << jf;
    // }

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
  temp->source_id = pack.source_id;
  temp->destination_ip = pack.destination_ip;
  temp->source_user_id = pack.source_user_id;
  temp->dest_user_id = pack.dest_user_id;
  temp->source_module_id = pack.source_module_id;
  temp->tunnel_id = pack.tunnel_id;
  return;
}
