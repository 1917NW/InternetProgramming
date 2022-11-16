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
double MAX_TIME = 1 * CLOCKS_PER_SEC;
string Flags[] = { "ZERO","SYN","ACK","SYN | ACK","FIN","OVER" };



u_short cksum(u_short* message, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
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
    unsigned int seq; //序列号

    packet_head() {
        sum = 0;
        datasize = 0;
        flags = 0;
        seq = 0;
    }
};

int connect(SOCKET& servSock, SOCKADDR_IN& clntAdr, int& clntAdrSz)
{
    packet_head h;

    int strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == SYN && cksum((u_short*)&h, sizeof(h)) == 0)
        printf("收到连接请求\n");

    h.flags = 0 | SYN | ACK;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("服务器端发起第二次握手\n");


    u_long mode = 1;
    //修改为非阻塞模式
    ioctlsocket(servSock, FIONBIO, &mode);
    clock_t start = clock();
    while (recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz) <= 0)
    {
        if (clock() - start > MAX_TIME)
        {
            //超时断开连接
            closesocket(servSock);
            return -1;
        }
    }
    //改回阻塞模式
    mode = 0;
    ioctlsocket(servSock, FIONBIO, &mode);

    if (h.flags == (0 | ACK) && cksum((u_short*)&h, sizeof(h)) == 0)
        printf("成功建立连接\n");

    return 1;
}

//server : 本地的地址信息
//client : 客户端的地址信息
//clntAdrSz : 存放客户端地址信息大小的地址
//buf : 存放接收数据的缓冲区
//接送来自客户端的一次数据发送，并且放入到buf里面，返回接收到的数据长度
int recv_data(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* buf)
{
    long int file_size = 0;//文件长度

    packet_head header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;

    //等待接收数据包，并对数据包进行处理
    while (1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
        memcpy(&header, Buffer, sizeof(header));

        //判断本次发送是否是结束
        if (header.flags == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "Accept Over" << endl;
            break;
        }

        
        if (header.flags == 0 && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            //判断接收到的包是否重复包
            if (header.seq != seq)
            {
                //重复包，发送上一个序列号和ACK
                cout << "WRONG PAG !  ACK:" << " SEQ:" << (int)header.seq << endl;
                header.flags = ACK;
                header.datasize = 0;
                header.seq = seq-1;
                header.sum = 0;
                header.sum = cksum((u_short*)&header, sizeof(header));
                memcpy(Buffer, &header, sizeof(header));
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << "Packet Duplicate !  Send ACK:" <<  " SEQ:" << (int)header.seq <<endl;
                continue;
            }
            //接收到正确的包
            else {
                //取出Buffer中的内容
                cout << "Accept Data " << length - sizeof(header) << " bytes "  << " SEQ : " << int(header.seq) << " SUM:" << int(header.sum) << endl;

                //把数据放入到数据缓冲区里面
                memcpy(buf + file_size, Buffer + sizeof(header), length - sizeof(header));
                file_size = file_size + int(header.datasize);

                seq = header.seq;
                //发送该包的序列号和ACK
                header.flags = ACK;
                header.datasize = 0;
                header.seq = seq;
                header.sum = 0;
                header.sum = cksum((u_short*)&header, sizeof(header));
                sendto(sockServ, (char*)&header, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << "Send ACK " << " SEQ:" << (int)header.seq << endl;
                seq++;
            }
        }
    }

    //发送OVER信息
    header.flags = OVER;
    header.datasize = 0;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));
    sendto(sockServ, (char*)&header, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);

    return file_size;
}

int disconnect(SOCKET& servSock, SOCKADDR_IN& clntAdr, int& clntAdrSz)
{
    packet_head h;
    int strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == FIN && cksum((u_short*)&h, sizeof(h)) == 0)
        printf("收到客户端断开请求\n");

    h.flags = 0 | ACK;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("发送确认消息\n");

    h.flags = 0 | FIN;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("发送服务器端断开请求\n");
    printf("服务器断开连接...\n");
    return 1;
}


int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN server_addr, client_addr;
    SOCKET server;



    server = socket(AF_INET, SOCK_DGRAM, 0);

    //设置服务器的地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9528);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //给服务器分配地址信息
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));

    int clntAdrSz = sizeof(client_addr);

    //建立连接
    connect(server, client_addr, clntAdrSz);

    //分配信息
    ofstream fout;
    char* name = new char[30];
    long long fileSize;

    //接收文件名
    int name_len = recv_data(server, client_addr, clntAdrSz, name);

    //接收文件长度
    recv_data(server, client_addr, clntAdrSz, (char*)&fileSize);
    cout << fileSize << endl;

    //接收文件数据
    char* data = new char[fileSize];
    int data_len = recv_data(server, client_addr, clntAdrSz, data);

    //断开连接
    disconnect(server, client_addr, clntAdrSz);


    //写入本地文件
    name[name_len] = '\0';
    fout.open(name, ios::out|ios::binary);
    fout.write(data, data_len);
    cout << "文件写入完毕" << endl;
    fout.close();

    while (1)
        ;
}
