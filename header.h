
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
  std::queue<uint32_t> recent_packet_id;
};