#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include<Windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;


const int MAXSIZE = 1024;//传输缓冲区最大长度
const char SYN = 0x1; 
const char ACK = 0x2;
const char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const char FIN = 0x4;//FIN = 1 ACK = 0
const char FIN_ACK = 0x5;//FIN = 1 ACK = 0
const char OVER = 0x7;//结束标志
double MAX_TIME = 0.5 * CLOCKS_PER_SEC;

struct packet_head {
    char flags; //标志位
    int length; //数据长度
    int send_seq; //发送序列号
    int ack_seq; //确认序列号
    int check_sum; //校验和，只校验packet_head

    packet_head() {
        flags = 0;

    }
    void packet_head_check_sum() {
        ;
    }
    void first() {
        flags = SYN;
    }
};

u_short cksum(u_short* mes, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);
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

struct HEADER
{
    u_short sum = 0;//校验和 16位
    u_short datasize = 0;//所包含数据长度 16位
    unsigned char flag = 0;
    //八位，使用后三位，排列是FIN ACK SYN 
    unsigned char SEQ = 0;
    //八位，传输的序列号，0~255，超过后mod
    HEADER() {
        sum = 0;//校验和 16位
        datasize = 0;//所包含数据长度 16位
        flag = 0;
        //八位，使用后四位，排列是FIN ACK SYN 
        SEQ = 0;
    }
};

int Connect(SOCKET& servSock, SOCKADDR_IN& clntAdr, int& clntAdrSz)
{ 
    packet_head h;

    int strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == SYN)
        printf("收到连接请求\n");

    h.flags = 0 | SYN | ACK;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("第三次握手\n");

    strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == (0 | ACK))
        printf("成功建立连接\n");
    return 1;
}

int RecvMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* message)
{
    long int all = 0;//文件长度
    HEADER header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;

    //等待接收数据包，并对数据包进行处理
    while (1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
        memcpy(&header, Buffer, sizeof(header));

        //判断本次发送是否是结束
        if (header.flag == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "文件接收完毕" << endl;
            break;
        }

        
        if (header.flag == unsigned char(0) && cksum((u_short*)Buffer, length - sizeof(header)))
        {
            //判断接收到的包是否是预期的包
            if (seq != int(header.SEQ))
            {
                //说明出了问题，返回ACK
                header.flag = ACK;
                header.datasize = 0;
                header.SEQ = (unsigned char)seq;
                header.sum = 0;
                u_short temp = cksum((u_short*)&header, sizeof(header));
                header.sum = temp;
                memcpy(Buffer, &header, sizeof(header));

                //重发该包的ACK
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << "Send to Clinet ACK:" << (int)header.SEQ << " SEQ:" << (int)header.SEQ << endl;
                continue;//丢弃该数据包
            }

            seq = int(header.SEQ);
            if (seq > 255)
            {
                seq = seq - 256;
            }

            //取出buffer中的内容
            cout << "Send message " << length - sizeof(header) << " bytes!Flag:" << int(header.flag) << " SEQ : " << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
            char* temp = new char[length - sizeof(header)];

            memcpy(temp, Buffer + sizeof(header), length - sizeof(header));
            memcpy(message + all, temp, length - sizeof(header));
            all = all + int(header.datasize);

            //返回ACK
            header.flag = ACK;
            header.datasize = 0;
            header.SEQ = (unsigned char)seq;
            header.sum = 0;
            u_short temp1 = cksum((u_short*)&header, sizeof(header));
            header.sum = temp1;
            memcpy(Buffer, &header, sizeof(header));
            //重发该包的ACK
            sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            cout << "Send to Clinet ACK:" << (int)header.SEQ << " SEQ:" << (int)header.SEQ << endl;
            seq++;
            if (seq > 255)
            {
                seq = seq - 256;
            }
        }
    }

    //发送OVER信息
    header.flag = OVER;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    memcpy(Buffer, &header, sizeof(header));
    if (sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen) == -1)
    {
        return -1;
    }

    return all;
}

int disConnect(SOCKET& servSock, SOCKADDR_IN& clntAdr, int& clntAdrSz)
{
    packet_head h;
    int strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if(h.flags==FIN)
    printf("收到客户端断开请求\n");

    h.flags = 0 | ACK;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("发送确认消息\n");

    h.flags = 0 | FIN;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("发送服务器端断开请求\n");

    strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == ACK)
        printf("服务器断开连接...\n");
    return 1;
}


int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN server_addr,client_addr;
    SOCKET server;
    


    server = socket(AF_INET, SOCK_DGRAM, 0);

    //设置服务器的地址
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9527);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //给服务器分配地址信息
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));

    int clntAdrSz = sizeof(client_addr);
    
    //建立连接
    Connect(server, client_addr, clntAdrSz);


    ofstream fout;
    char* name = new char[20];
    char* data = new char[100000000];

    //接收数据
    int namelen = RecvMessage(server, client_addr, clntAdrSz, name);
    int datalen = RecvMessage(server, client_addr, clntAdrSz, data);
    
    disConnect(server, client_addr, clntAdrSz);
    
    
    //写入本地文件
    string a;
    for (int i = 0; i < namelen; i++)
    {
        a = a + name[i];
    }
    fout.open(a.c_str(), ofstream::binary);
    fout.write(data, datalen);
    fout.close();
    
    Sleep(10000);
    
    
}

