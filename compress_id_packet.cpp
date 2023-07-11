#include "header.h"
#include "json.hpp"
using namespace std;
using json = nlohmann::json;


//
void change_list(json* packet_id_list_json){
  json packet_id_newlist_json;
  while(packet_id_list_json->size()!=0){

  }
}

//
int main(){
    json packet_id_list_json;   // 原list
    
    for(int i=0; i<10; i++) {  // 测试数据
        packet_id_list_json.push_back((uint32_t)i);
    }
    for(int i=11; i<100; i++) {
        packet_id_list_json.push_back((uint32_t)i);
    }
    for(int i=100; i<101; i++) {
        packet_id_list_json.push_back((uint32_t)i);
    }
    for(int i=102; i<10000; i++) {
        packet_id_list_json.push_back((uint32_t)i);
    }
    packet_id_list_json.push_back((uint32_t)12345);
    

    // 处理原list的部分-----------------------------------------
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

    // 处理原list的部分-----------------------------------------
    cout<< pack;// print查看
    return 0;
}

