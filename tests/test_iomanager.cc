#include "../src/iomanager/ioscheduler.h"
#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <cstring>
#include <cerrno>

using namespace colib;

char recv_data[4096];

const char data[] = "GET / HTTP/1.0\r\n\r\n"; // 请求报文

int sock; // 套接字描述符

// 从套接字读取数据并输出接收到的内容。
void func()
{
  recv(sock, recv_data, 4096, 0);
  std::cout << recv_data << std::endl
            << std::endl;
}

//发送 HTTP 请求数据到服务器。 
void func2()
{
  send(sock, data, sizeof(data), 0);
}

int main(int argc, char const *argv[])
{
  /*
   * 代码使用了 IOManager 来管理网络套接字的读写事件。
   * addEvent 将读写事件与回调函数关联，
   * IOManager 会在适当的时机调用这些回调。
   *
   * 目的是
   * 连接到指定的 HTTP 服务器，
   * 发送一个 GET 请求，并接收响应。
   * 使用 epoll 事件通知机制来实现非阻塞 I/O。
   *
   * 套接字被设置为非阻塞，
   * 这意味着 recv 和 send 操作不会阻塞线程，
   * 因此可以同时处理多个 I/O 操作。
   */
  // create iomanager
  IOManager manager(2);
  // create socket
  sock = socket(AF_INET, SOCK_STREAM, 0);
  // 设置服务器相关信息
  sockaddr_in server;
  server.sin_family = AF_INET;
  server.sin_port = htons(80); // HTTP 标准端口
  server.sin_addr.s_addr = inet_addr("103.235.46.96");
  // 设置为非阻塞
  fcntl(sock, F_SETFL, O_NONBLOCK);
  // 发起连接
  connect(sock, (struct sockaddr *)&server, sizeof(server));
  // 注册事件
  manager.addEvent(sock, IOManager::WRITE, &func2);
  manager.addEvent(sock, IOManager::READ, &func);

  std::cout << "event has been posted\n\n";

  return 0;
}