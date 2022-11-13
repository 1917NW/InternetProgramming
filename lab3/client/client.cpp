#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include<Windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int MAXSIZE = 1024;//传输缓冲区最大长度
const char SYN = 0x1; //SYN = 1 ACK = 0
const char ACK = 0x2;//SYN = 0, ACK = 1，FIN = 0
const char FIN = 0x4;//FIN = 1 ACK = 0
const char OVER = 0x5;//结束标志
double MAX_TIME = 0.5 * CLOCKS_PER_SEC;
string Flags[] = { "ZERO","SYN","ACK","SYN | ACK","FIN","OVER" };


/*
1.把伪首部添加到UDP上；
2.计算初始时是需要将检验和字段添零的；
3.把所有位划分为16位（2字节）的字
4.把所有16位的字相加，如果遇到进位，则将高于16字节的进位部分的值加到最低位上，举例，0xBB5E+0xFCED=0x1 B84B，则将1放到最低位，得到结果是0xB84C
5.将所有字相加得到的结果应该为一个16位的数，将该数取反则可以得到检验和checksum。 */

u_short cksum(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);

    //最后一位补零
    memset(buf, 0, size + 1);
    memcpy(buf, mes, size);
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
    unsigned int seq; //序列号

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
    header.sum= cksum((u_short*)&header, sizeof(header));
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, len);
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送

    cout << "Send data " << len << " bytes " << " flags:" << Flags[(header.flags)] << " SEQ:" << int(header.seq) << " SUM:" << int(header.sum) << endl;

    //分配重传缓冲区
    char* buffer_copy = new char[MAXSIZE + sizeof(header)];
    memcpy(buffer_copy, buffer, len + sizeof(header));

    //记录发送时间
    clock_t start = clock();

    //接收回应信息，并进行处理
    while (1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        //每调用一次recvfrom就去检测一下数据区
        while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                //超时重传(超时重传必须使用非阻塞模式,,然后计算时间差)
                sendto(socketClient, buffer_copy, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
                cout << "Time Out! ReSend Data " << len << " bytes  flags:" <<Flags[(header.flags)] << " SEQ:" << int(header.seq) << endl;

                //重新记录发送时间
                clock_t start = clock();
            }
        }

        memcpy(&header, buffer, sizeof(header));//缓冲区接收到信息，读取

        //检查回应消息
        if (header.seq == u_short(order) && header.flags == ACK  && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "ACK accepted  flags:" << Flags[(header.flags)] << " SEQ:" << int(header.seq) << endl;
            break;
        }
        else
        {
            //包损坏重发(停等机制没有乱序的问题)
            continue;
        }
    }

    //改回阻塞模式
    u_long mode = 0;
    ioctlsocket(socketClient, FIONBIO, &mode);
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    int seqnum = 0;
    for (int i = 0; i < packagenum; i++)
    {
        int data_length = i == packagenum - 1 ? len - (packagenum - 1) * MAXSIZE : MAXSIZE;
        send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, data_length, seqnum);
        seqnum++;
    }

    //文件发送结束，发送over消息
    packet_head header;
    char* Buffer = new char[sizeof(header)];
    header.flags = OVER;
    header.sum = 0;
    header.sum =cksum((u_short*)&header, sizeof(header));
    
    sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End" << endl;

    packet_head header_copy = header;
    //等待服务器端发送over消息
    clock_t start = clock();
    while (1)
    {
        u_long mode = 1;
        //修改为非阻塞模式
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
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

    packet_head h;
    h.flags = 0 | FIN;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("发送断开连接请求,等待回应\n");


    int strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | ACK))
        printf("收到回应，开始建立连接...\n");

    strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | FIN))
        printf("收到服务器断开请求，开始断开连接...\n");

    h.flags = 0 | ACK;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
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
    fin.open(fileName.c_str(), ifstream::binary);

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