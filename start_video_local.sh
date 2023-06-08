# 开启完之后，VLC分别对应 udp 23023和3000端口
./receiver 192.168.0.181 > log_receiver & 
./sender 0.0.0.0 >> log_sender & 
