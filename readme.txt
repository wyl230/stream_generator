VLC推流：推流地址为服务器地址，port为NodePort video_in的nodePort(30001)
sender:接收IP为0.0.0.0 port为NodePort video_in 的targetPort(8000)；发送IP为seu-ue-svc，端口为9001
receiver：接收IP为0.0.0.0 port为9002,；发送IP为目标主机的IP，端口为3000
VLC拉流：接收IP为0.0.0.0,port为约定好的3000
