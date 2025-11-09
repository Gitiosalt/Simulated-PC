#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <WinSock2.h>
#include <Windows.h>
#include <conio.h>
#include <math.h>
#include <errno.h>
#include <Commdlg.h>
#include <time.h>
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996)//允许使用一些非安全函数
#pragma warning(disable:6031)//有些函数可以不用接收返回值比如getchar(),scanf()等等

#define SERVER_IP "192.168.8.3"
#define SERVER_PORT 8889
#define BUFFER_SIZE 1024  

WSADATA wsaData;
SOCKET ArmSocket;
SOCKADDR_IN ServerAddr = { 0 };

uint32_t sendBuf[BUFFER_SIZE] = { 0 };               // 发送缓冲区
uint32_t recvBuf[BUFFER_SIZE] = { 0 };               // 接收缓冲区

int totalRecv = 0;                  // 已接收的总字节数
int expectedCount = 100;            // 假设期望接收100个32位数据（与发送数量对应）
int expectedTotal = expectedCount * sizeof(uint32_t);  // 期望总字节数

#define END_FLAG 0  // 终止交互的标识

void socket_init()
{
    WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("Winsock初始化失败！错误代码：%d\n", WSAGetLastError());
        exit(1);
    }
    // 再检查版本
    if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
        printf("不支持的Winsock版本！\n");
        WSACleanup();
        exit(1);
    }
    printf("确认协议版本信息成功！\n");

    ArmSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ArmSocket == SOCKET_ERROR) {
        printf("客户端socket创建失败：%d\n", GetLastError());
        WSACleanup();
        exit(1);
    }
    printf("客户端socket创建成功！\n");

    //设置服务器地址信息
    ServerAddr.sin_family = AF_INET;                        // IPv4 地址族
    ServerAddr.sin_port = htons(SERVER_PORT);               // 端口号（主机字节序转网络字节序）
    ServerAddr.sin_addr.S_un.S_addr = inet_addr(SERVER_IP); // IP 地址（字符串转网络字节序）
}

// 发送一个32位整数（确保完整发送）
int sendInt(SOCKET sock, uint32_t data) {
    int totalSent = 0;
    int left = sizeof(uint32_t);
    char* buf = (char*)&data;
    while (totalSent < left) {
        int sent = send(sock, buf + totalSent, left - totalSent, 0);
        if (sent == SOCKET_ERROR) {
            return -1;
        }
        totalSent += sent;
    }
    return 0;
}

// 接收一个32位整数（确保完整接收）
int recvInt(SOCKET sock, uint32_t* data) {
    int totalRecv = 0;
    int left = sizeof(uint32_t);
    char* buf = (char*)data;
    while (totalRecv < left) {
        int recvLen = recv(sock, buf + totalRecv, left - totalRecv, 0);
        if (recvLen == SOCKET_ERROR || recvLen == 0) {
            return -1;
        }
        totalRecv += recvLen;
    }
    return 0;
}


int main()
{
    socket_init();
   //连接服务器
    int r;
    r = connect(ArmSocket, (LPSOCKADDR)&ServerAddr, sizeof(ServerAddr));
    if (r == -1)
    {
        closesocket(ArmSocket);
        for (int j = 0; j < 3; j++)//最大重连3次
        {
            socket_init();
            connect(ArmSocket, (LPSOCKADDR)&ServerAddr, sizeof(ServerAddr));
            if (r != -1) break;
            printf("连接服务器失败：%d \n", GetLastError());
            closesocket(ArmSocket);
            WSACleanup();
            return -1;
        }
    }
    while (1) {
        uint32_t n;
        printf("\n等待服务器发送指令...\n");
        if (recvInt(ArmSocket, &n) == -1) {
            printf("接收指令失败！\n");
            return 0;
        }

        // 若指令为0，终止交互
        if (n == END_FLAG) {
            printf("客户端终止交互\n");
            //关闭连接并清理资源
            closesocket(ArmSocket);  // 关闭客户端 Socket
            WSACleanup();               // 清理 Winsock 环境
            printf("连接已关闭\n");
            system("pause");  // 暂停窗口，方便查看输出
            return 0;
        }
        printf("收到服务器指令：需要发送 %d 条数据\n", n);
        expectedCount = n;

        // 发送数据到服务器
        uint32_t sendData;              // 要发送的32位数据
        for (int SendCount = 0; SendCount <= expectedCount; SendCount++) {
            sendData = 1 << (SendCount % 32);
            int sendLen = send(ArmSocket, (char*)&sendData, sizeof(uint32_t), 0);
            if (sendLen == SOCKET_ERROR) {
                printf("发送数据失败！错误代码：%d\n", WSAGetLastError());
                closesocket(ArmSocket);
                WSACleanup();
                return 1;
            }
        }
    }


    ////////////////////////////////////////////////
    //// 循环接收，直到收满预期数据或连接关闭
    //while (totalRecv < expectedTotal) {
    //    // 计算剩余接收空间（避免缓冲区溢出）
    //    int remaining = expectedTotal - totalRecv;
    //    // 每次接收一个32位数据（或剩余全部）
    //    int recvLen = recv(ArmSocket,
    //        (char*)recvBuf + totalRecv,  // 从缓冲区当前位置继续接收
    //        remaining,
    //        0);

    //    if (recvLen == SOCKET_ERROR) {
    //        printf("接收数据失败！错误代码：%d\n", WSAGetLastError());
    //        closesocket(ArmSocket);
    //        WSACleanup();
    //        return 1;
    //    }
    //    else if (recvLen == 0) {
    //        printf("服务器已关闭连接，仅接收了%d字节（预期%d字节）\n", totalRecv, expectedTotal);
    //        break;
    //    }

    //    totalRecv += recvLen;
    //    printf("已接收%d字节，累计接收%d字节\n", recvLen, totalRecv);
    //}

    //// 打印接收的32位数据（按每个32位解析）
    //if (totalRecv > 0) {
    //    int actualCount = totalRecv / sizeof(uint32_t);  // 实际接收的32位数据个数
    //    printf("接收完成，共%d字节，包含%d个32位数据：\n", totalRecv, actualCount);
    //    for (int i = 0; i < actualCount; i++) {
    //        printf("第%d个数据：0x%08X\n", i + 1, recvBuf[i]);
    //    }
    //}

    ////关闭连接并清理资源
    //closesocket(ArmSocket);  // 关闭客户端 Socket
    //WSACleanup();               // 清理 Winsock 环境
    //printf("连接已关闭\n");

    //system("pause");  // 暂停窗口，方便查看输出
    //return 0;


}

