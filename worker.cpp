#include <iostream>
#include <cstring>
#include <string>
#include <cassert> 
#include <string_view>
#include <sstream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include "CurlEasyPtr.h" 
using namespace std;
using namespace std::literals;
/// Worker process that receives a list of URLs and reports the result
/// Example:
///    ./worker localhost 4242
/// The worker then contacts the leader process on "localhost" port "4242" for work


// 错误处理函数
void error(const char* msg) {
    perror(msg);
    exit(1);
}

int connectToCoordinator(const char* hostname, const char* port) {
    struct addrinfo hints, *res = nullptr, *p;
    int status;
    int sockfd = -1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // 不指定是 IPv4 或 IPv6
    hints.ai_socktype = SOCK_STREAM; // TCP stream sockets

    if ((status = getaddrinfo(hostname, port, &hints, &res)) != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        return -1;
    }

    // 尝试连接到第一个可用的结果
    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("worker: socket");
            continue;
        }

        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("worker: connect");
            sockfd = -1; // 这一步很重要
            continue;
        }

        break; // 如果我们到这里，说明我们已经成功连接
    }

    if (p == NULL) {
        std::cerr << "worker: failed to connect\n";
        sockfd = -2;
    }

    if (res != nullptr) {
        freeaddrinfo(res); // 确保在任何退出点释放资源
    }


    return sockfd;
}


ssize_t processUrl(const std::string& url) {
    // std::cout<<"传进来的url："<<std::endl;
    // std::cout<<url<<std::endl;
    // 使用 CurlEasyPtr 来下载并处理 CSV 文件

    auto curlSetup = CurlGlobalSetup();

    size_t result = 0;

    // Download the file list
    auto curl = CurlEasyPtr::easyInit();

    curl.setUrl(url);
    // Download the CSV file
    auto csvData = curl.performToStringStream();
    // std::cout << "Processing data from CSV file..." << std::endl;

    // Check each row in the CSV data
    for (std::string row; std::getline(csvData, row, '\n');) {
        auto rowStream = std::stringstream(std::move(row));

        // Check the URL in the second column
        unsigned columnIndex = 0;
        for (std::string column; std::getline(rowStream, column, '\t'); ++columnIndex) {
            // column 0 is id, 1 is URL
            if (columnIndex == 1) {
                // std::cout<<"url in csv:"<<std::endl;
                // std::cout<<column<<std::endl;
                // Check if URL is "google.ru"
                auto pos = column.find("://"sv);
                if (pos != std::string::npos) {
                    std::string_view afterProtocol = std::string_view(column).substr(pos + 3);
                    if (afterProtocol.starts_with("google.ru/")) {
                        ++result;
                    }
                }
                break;
            }
        }
    }
    // std::cout<<"result from single csv："<<result<<std::endl;

    return result;
}


int main(int argc, char* argv[]) {
    if (argc != 3) {
      std::cerr << "Usage: " << argv[0] << " <host> <port>" << std::endl;
      return 1;
    }

   // TODO:
   //    1. connect to coordinator specified by host and port
   //       getaddrinfo(), connect(), see: https://beej.us/guide/bgnet/html/#system-calls-or-bust
   //    2. receive work from coordinator
   //       recv(), matching the coordinator's send() work
   //    3. process work
   //       see coordinator.cpp
   //    4. report result
   //       send(), matching the coordinator's recv()
   //    5. repeat
    // std::cout<<"22";
    // std::cout.flush();
    // 连接到协调器
    int sockfd = connectToCoordinator(argv[1], argv[2]);
    if (sockfd < 0) {
        // 连接失败
      return 1;
    }else{
    //   printf("连接成功！\n");
    }
    // std::cout<<"33";
    // std::cout.flush();
    char buffer[5000];
    std::string currentData;
    size_t results = 0;
    bool receivedEnd = false; // 用于标记是否接收到结束标志

    while (true) {
        memset(buffer, 0, sizeof(buffer));
        ssize_t n = recv(sockfd, buffer, sizeof(buffer) - 1, 0); // 使用recv代替read
        // std::cout<<n<<std::endl;
        // std::cout.flush();
        if (n < 0) {
            error("ERROR receiving from socket");
            return 1;
        } else if (n == 0) {
            std::cout<<"无数据";
            // std::cout.flush();            
            break; // 无数据，断开连接
        }
        // std::cout<<"55";
        // std::cout.flush();            
        buffer[n] = '\0'; // 确保以空字符结束
        string url(buffer);
        // std::cout<<"一次传过来的url："<< std::endl;
        // std::cout<<url<< std::endl;
        // std::cout<<"----"<< std::endl;
        currentData.append(buffer, n);

        // 查找换行符
        size_t newlinePos;
        // size_t results=0;
        while ((newlinePos = currentData.find('\n')) != std::string::npos) {
            std::string url2 = currentData.substr(0, newlinePos); // 提取URL
            // std::cout<<"分割成单个url, 开始计数, 处理后的："<< std::endl;
            // std::cout<<url2<< std::endl;
            
            if (url2 != "END"){
            // std::cout<<"results after END:"<<results<< std::endl;
            results += processUrl(url2); // 累加处理URL的结果
            currentData = currentData.substr(newlinePos + 1); // 移除已处理的URL
            } else if (url2 == "END") {
                receivedEnd = true; // 设置接收到结束标志的标志
                // std::cout << "接收到结束标志，结束接收" << std::endl;
                break; // 接收到结束标志，结束接收
            }
        }

    // std::cout<<"results："<<results<< std::endl;
    // std::string response = std::to_string(results);
    // std::cout<<"response"<<response<< std::endl;
    if (receivedEnd) {
        break;
        // if (send(sockfd, response.c_str(), response.length(), 0) < 0) {
        //     error("ERROR sending to socket");
        // }   

        // std::cout<<"close"<< std::endl;
        // close(sockfd);
    }

    }
    // std::cout<<"results_end："<<results<< std::endl;
    // 报告结果回协调器
    std::string response = std::to_string(results);
    if (send(sockfd, response.c_str(), response.length(), 0) < 0) {
        error("ERROR sending to socket");
    }   

    // std::cout<<"close"<< std::endl;
    close(sockfd);
    return 0;
}

