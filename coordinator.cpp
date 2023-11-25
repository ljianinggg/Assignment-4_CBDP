#include "CurlEasyPtr.h"

#include <iostream>
#include <sstream>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <netdb.h>
#include <vector>
#include <thread>
#include <mutex>
#include <map>

using namespace std::literals;
using namespace std;

mutex resultMutex;  // 用于保护共享的结果变量
int totalResult = 0; // 共享的总结果变量

// 错误处理函数
void error(const char* msg) {
    perror(msg);
    exit(1);
}
// 工作分发函数
void distributeWork(int workerSocket, const vector<string>& urls) {
    char buffer[256];
    ssize_t n;
    std::string resultString;

    for (size_t i = 0; i < urls.size(); ++i) {
        std::string urlWithNewline = urls[i] + "\n";
        // std::cout << "Sending message: " << urlWithNewline.c_str() << std::endl;

        if (send(workerSocket, urlWithNewline.c_str(), urlWithNewline.length(), 0) < 0) {
            error("ERROR sending to socket");
        }
    }

    // 发送结束标志
    std::string endFlag = "END\n";
    // std::cout << "Sending end flag: " << endFlag.c_str() << std::endl;
    if (send(workerSocket, endFlag.c_str(), endFlag.length(), 0) < 0) {
        error("ERROR sending end flag to socket");
    }

    int localTotalResult = 0; // 用于每个工作节点的局部结果

    while ((n = recv(workerSocket, buffer, 256, 0)) > 0) {
        buffer[n] = '\0';  // 确保字符串结束
        try {
            int receivedNumber = stoi(buffer);
            localTotalResult += receivedNumber; // 将字符串转换为整数并累加到局部结果中
        } catch (std::invalid_argument& e) {
            std::cout << "Invalid number received: " << buffer << std::endl;
        }
        memset(buffer, 0, 256); // 清空缓冲区
    }

    if (n < 0) {
        error("ERROR receiving from socket");
    }

    // 使用互斥锁更新总结果
    {
        lock_guard<mutex> lock(resultMutex);
        totalResult += localTotalResult;
    }
        
    close(workerSocket);
}


int main(int argc, char* argv[]) {
    // 检查命令行参数
    if (argc != 3) {
        cerr << "Usage: " << argv[0] << " <URL to csv list> <listen port>" << endl;
        return 1;
    }

    // Initialize socket communication
    int sockfd, newsockfd;
    struct addrinfo hints, *servinfo, *p;
    int rv;
    struct sockaddr_storage their_addr;
    socklen_t addr_size;
    // char s[INET6_ADDRSTRLEN];
    // 设置地址信息
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // 不限制地址类型
    hints.ai_socktype = SOCK_STREAM; // 流套接字
    hints.ai_flags = AI_PASSIVE; // 被动模式，用于绑定

    // 获取地址信息
    if ((rv = getaddrinfo(NULL, argv[2], &hints, &servinfo)) != 0) {
        cerr << "getaddrinfo: " << gai_strerror(rv) << endl;
        return 1;
    }

    // 创建和绑定套接字
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        int yes = 1;
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
            perror("setsockopt");
            close(sockfd);
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        break;
    }

   
    if (p == NULL) {
        cerr << "server: failed to bind" << endl;
        freeaddrinfo(servinfo);
        return 2;
    }
    // if (servinfo != nullptr) {
    //     freeaddrinfo(servinfo); // 确保在任何退出点释放资源
    // }

    
    freeaddrinfo(servinfo);

    // 监听套接字
    if (listen(sockfd, 10) == -1) {
        perror("listen");
        exit(1);
    }

    // Initialize CURL to download the CSV file list
    CurlEasyPtr curl = CurlEasyPtr::easyInit();
    curl.setUrl(argv[1]);
    stringstream fileList = curl.performToStringStream();

    // Process the file list and extract URLs
    string line;
    vector<string> urls;
    while (getline(fileList, line)) {
        urls.push_back(line);    // 将从fileList中读取的每一行（URL）添加到urls向量中
    }

    const int numWorkers = 4; // 假设有4个工作节点
    vector<thread> threads;
    vector<vector<string>> workerUrls(numWorkers);

    
    // 接受连接并分发工作
    size_t currentWorkerIndex = 0;

    addr_size = sizeof their_addr;

    for (int i = 0; i < numWorkers; ++i) {
        newsockfd = accept(sockfd, (struct sockaddr *)&their_addr, &addr_size);
        // std::cout<<newsockfd<< std::endl;
        // std::cout.flush();
        if (newsockfd < 0) 
            error("ERROR on accept");

        // 为每个新连接的工作节点创建一个URL集合，轮询分配URLs
        vector<string> assignedUrls;
        for (size_t i = 0; i < urls.size(); ++i) {
            if (i % numWorkers == currentWorkerIndex) {  // 假设currentWorkerIndex是工作节点的索引
                assignedUrls.push_back(urls[i]);
                // std::cout<<"assignedUrls"<<assignedUrls[i]<< std::endl;
            }
        }


        // 创建一个新线程来处理新连接的工作节点
        // std::cout<<"currentWorkerIndex 1:"<<currentWorkerIndex<< std::endl;
        threads.push_back(thread(distributeWork, newsockfd, assignedUrls));
        // 创建多个线程
        // for (int i = 0; i < numWorkers; ++i) {
        //     std::cout<<"currentWorkerIndex 1:"<<i<<std::endl;
        //     threads.push_back(thread(distributeWork, newsockfd, assignedUrls));
        // }
        // close(newsockfd);

        // std::cout << "Thread ID: " << t.get_id() << ", Joinable: " << t.joinable() << std::endl;
        
        
        currentWorkerIndex = (currentWorkerIndex + 1) % numWorkers; // 更新工作节点索引
        // std::cout<<"currentWorkerIndex 2:"<<currentWorkerIndex<< std::endl;


        std::this_thread::sleep_for(std::chrono::seconds(1));


        close(newsockfd);
            
        // if (t.joinable()) {
        //     std::cout<<"结束线程 "<< std::endl;
        //     t.join();
        // }

        // // 检查是否应该跳出循环（例如，当所有线程完成时）
        // allThreadsCompleted = true;
        // for (const auto& t : threads) {
        //     std::cout << "255/Thread ID: " << t.get_id() << ", Joinable: " << t.joinable() << std::endl;
        //     if (t.joinable()) {
        //         allThreadsCompleted = false;
        //         break;
        //     }
        // }

        // std::cout << "allThreadsCompleted: " << allThreadsCompleted  << std::endl;
        // if (allThreadsCompleted) {
        //     std::cout << "检查是否应该跳出循环 exit" << std::endl;
        //     shouldExit = true; // 设置标志以跳出循环
        // }

        // std::cout << "Still waiting for some threads to complete." << std::endl;

        // if (currentWorkerIndex == numWorkers - 1){
        //     break;
        // }


    }
    // std::cout<<"跳出循环";

    // 等待所有线程完成
    for (auto& t : threads) {
        // std::cout << "2,Thread ID: " << t.get_id() << ", Joinable: " << t.joinable() << std::endl;
        if (t.joinable()) {
            t.join();
        }
    }

    // 输出总结果
    cout << "Total Result = " << totalResult << endl;   
    std::cout<<"所有线程完成"<<std::endl;

    
    close(sockfd);
    return 0;
}
