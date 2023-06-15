import socket
import time

UDP_IP = "162.105.85.188"  # 目标 IP
# UDP_PORT = 30002     # 目标端口
# UDP_PORT = 31516     # 目标端口
UDP_PORT = 31539     # 目标端口
MESSAGE = b"Hello, World!" * 80   # 发送的数据，这里发送了 "Hello, World!" 10 次

sock = socket.socket(socket.AF_INET,  # Internet
                     socket.SOCK_DGRAM)  # UDP

while True:
    sock.sendto(MESSAGE, (UDP_IP, UDP_PORT))  # 发送数据
    print('send...')
    time.sleep(1)  # 休眠 1 秒钟
