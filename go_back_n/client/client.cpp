#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include<Windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int MAXSIZE = 1024;//传输缓冲区最大长度
const int dup_count_MAX = 50;//最大重传次数
const char SYN = 0x1; //SYN = 1 ACK = 0
const char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const char FIN = 0x4;//FIN = 1 ACK = 0
const char OVER = 0x5;//结束标志
double MAX_TIME = 1* CLOCKS_PER_SEC;
string Flags[] = { "ZERO","SYN","ACK","SYN | ACK","FIN","OVER" };

//记录重传变量
int dup_count = 0;

//窗口大小
int windows = 10;



u_short cksum(u_short* message, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);

    //最后一位补零
    memset(buf, 0, size + 1);
    memcpy(buf, message, size);
    u_long sum = 0;
    while (count--) {
        sum += *buf++;
        if (sum & 0xffff0000) {
            sum &= 0xffff;
            sum++;
        }
    }
    return ~(sum & 0xffff);
}


struct packet_head
{
    u_short sum;//校验和 16位
    u_short datasize;//所包含数据长度 16位
    char flags; // 标志位
     int seq; //序列号

    packet_head() {
        sum = 0;
        datasize = 0;
        flags = 0;
        seq = 0;
    }
};

int connect(SOCKET& sock, SOCKADDR_IN& servAdr)//三次握手建立连接
{
    SOCKADDR_IN from_adr;
    int from_sz;

    from_sz = sizeof(from_adr);

    packet_head h;
    h.datasize = 0;
    h.flags = SYN;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    printf("第一次握手：发送连接请求,等待回应\n");
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));

    u_long mode = 1;
    //修改为非阻塞模式
    ioctlsocket(sock, FIONBIO, &mode);
    clock_t start = clock();
    while (recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz) <= 0)
    {
        if (clock() - start > 5 * MAX_TIME)
        {
            //超时断开连接
            closesocket(sock);
            return -1;
        }
    }
    //改回阻塞模式

    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);

    if (h.flags == (0 | SYN | ACK) && cksum((u_short*)&h, sizeof(h)) == 0)
        printf("收到回应，开始建立连接...\n");
    else
        return -1;


    h.flags = 0 | ACK;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("客户端发起第三次握手\n");
    return 1;
}


void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{


    //设置包的头部
    packet_head header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.seq = order;//序列号
    header.flags = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));

    //设置udp数据包
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, len);
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送

    cout << "Send data " << len << " bytes " << " flags:" << Flags[(header.flags)] << " SEQ:" << int(header.seq) << " SUM:" << int(header.sum) << endl;

    
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
    //包号为[0:packagenum-1]
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    
    //seqnum为[0:packagenum-1]
    int seqnum = 0;

    //窗口的头部包
    //head前面的包(包含head指向的包)都已经发送
    int base = -1;

    packet_head h_buffer;
    //接下来要发送的包
    int nextseqnum = 0;

    int index = 0;
    clock_t start;
    cout << "总共要发送的包"<<packagenum << endl;
    while (base < packagenum - 1) {
        if (nextseqnum - base < windows && nextseqnum != packagenum)
        {   
            int data_len = nextseqnum == packagenum - 1 ? len - (packagenum - 1) * MAXSIZE : MAXSIZE;
            send_package(socketClient, servAddr, servAddrlen, message + nextseqnum * MAXSIZE, data_len, nextseqnum);
            start = clock();
            nextseqnum++;
        }

        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        if (recvfrom(socketClient, (char*)&h_buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen)>0) {
            //收到响应包后对响应包进行检查
            
            if (cksum((u_short*)&h_buffer, sizeof(h_buffer))!=0 || h_buffer.flags!=ACK) {
                
                cout << (cksum((u_short*)&h_buffer, sizeof(h_buffer)) != 0) << endl;
                cout << (h_buffer.flags != ACK) << endl;
               
                nextseqnum = base + 1;
                cout << "响应包损坏，开始重传" << endl;
               
                continue;
            }
            else {
                //更新base

               
                if ((h_buffer.seq > base) && (h_buffer.seq < nextseqnum)) {
                    base = int(h_buffer.seq);
                    cout << "Send has been confirmed! Flags " << Flags[h_buffer.flags] << " SEQ:" << int(h_buffer.seq) << endl;
                }
                
                //更新时间
                start = clock();
            }
        }
        else {
            if (clock() - start > MAX_TIME) {
                nextseqnum = base + 1;
                cout << "时间超时，开始重传" << endl;
            }
        }
        mode = 0;
        ioctlsocket(socketClient, FIONBIO, &mode);

    }


    

    //文件发送结束，发送over消息
    packet_head header;
    char* Buffer = new char[sizeof(header)];
    header.flags = OVER;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));

    sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End" << endl;

    //设置over重发缓冲区
    packet_head header_copy = header;


    //等待服务器端发送over消息
    start = clock();
    dup_count = 0;
    while (1)
    {
        u_long mode = 1;
        //修改为非阻塞模式
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                dup_count++;
                //重发超过一定次数就关闭连接
                if (dup_count >= dup_count_MAX)
                    closesocket(socketClient);
                //超时重发
                sendto(socketClient, (char*)&header_copy, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "Time Out! ReSend End!" << endl;
                start = clock();
            }
        }


        if (header.flags == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "Send Over" << endl;
            break;
        }
        else
        {
            continue;
        }
    }

    //改回阻塞模式
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);
}



int disconnect(SOCKET& sock, SOCKADDR_IN& servAdr)
{
    SOCKADDR_IN from_adr;
    int from_sz = sizeof(from_adr);
    int strLen;
    packet_head h;
    h.flags = 0 | FIN;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("发送断开连接请求,等待回应\n");

    packet_head h_buffer;
    clock_t start = clock();
    dup_count = 0;

    u_long mode = 1;
    //修改为非阻塞模式
    ioctlsocket(sock, FIONBIO, &mode);
    while (recvfrom(sock, (char*)&h_buffer, sizeof(h_buffer), 0, (SOCKADDR*)&from_adr, &from_sz) <= 0)
    {
        if (clock() - start > MAX_TIME)
        {
            dup_count++;
            if (dup_count >= dup_count_MAX)
                closesocket(sock);
            //超时重发
            sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
            cout << "Time Out! ReSend End!" << endl;
            start = clock();
        }
    }
    if (h_buffer.flags == (0 | ACK)) {
        printf("收到回应，开始建立连接...\n");
    }
    //修改为阻塞模式
    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);



    strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | FIN))
        printf("收到服务器断开请求，开始断开连接...\n");

    h.flags = 0 | ACK;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    closesocket(sock);
    return 1;
}


int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN server_addr;
    SOCKET sock;

    //设置服务器的地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(9527);

    //设置客户端套接字
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(server_addr);

    //建立连接
    if (connect(sock, server_addr) == -1)
    {
        return 0;
    }

    //读取文件到内存
    ifstream fin;
    string fileName;
    cout << "请输入文件名:" << endl;
    cin >> fileName;
    fin.open(fileName.c_str(), ios::in | ios::binary);

    //获取文件大小
    fin.seekg(0, ios::end);
    long long fileSize = (int)fin.tellg();
    fin.seekg(0, ios::beg);

    //分配内存缓冲区，将文件读到内存
    char* buffer = new char[fileSize];
    fin.read(buffer, fileSize);
    long long bytes = fin.gcount();

    //发送文件到服务器
    //1.发送文件名
    send(sock, server_addr, len, (char*)(fileName.c_str()), fileName.length());

    //2.发送文件大小
    send(sock, server_addr, len, (char*)(&fileSize), sizeof(fileSize));

    //3.发送文件数据
    clock_t start = clock();
    send(sock, server_addr, len, buffer, bytes);
    clock_t end = clock();

    //关闭连接
    disconnect(sock, server_addr);

    cout << "传输总时间为:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "吞吐率为:" << ((float)bytes) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;

    while (1)
        ;
}