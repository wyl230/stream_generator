
- sender.cpp 发流，添加包头
  - 如果是音频、视频流，先接收8000端口发来的数据包，再转发出去
  - 包长 cbr 160B，vbr 1200B
  - 包间隔 cbr 20ms，vbr 3~6ms
  - 某个特定的ip发送给核心网
- receiver.cpp 收流，发送业务流信息给real-interface
  - 发送


---
以下忽略
VLC推流：推流地址为服务器地址，port为NodePort video_in的nodePort(30001)
sender:接收IP为0.0.0.0 port为NodePort video_in 的targetPort(8000)；发送IP为seu-ue-svc，端口为9001
receiver：接收IP为0.0.0.0 port为9002,；发送IP为目标主机的IP，端口为3000
VLC拉流：接收IP为0.0.0.0,port为约定好的3000
