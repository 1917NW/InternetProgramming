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
const char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const char FIN = 0x4;//FIN = 1 ACK = 0
const char FIN_ACK = 0x5;
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


/*
1.把伪首部添加到UDP上；
2.计算初始时是需要将检验和字段添零的；
3.把所有位划分为16位（2字节）的字
4.把所有16位的字相加，如果遇到进位，则将高于16字节的进位部分的值加到最低位上，举例，0xBB5E+0xFCED=0x1 B84B，则将1放到最低位，得到结果是0xB84C
5.将所有字相加得到的结果应该为一个16位的数，将该数取反则可以得到检验和checksum。 */

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
    //八位，使用后四位，排列是FIN ACK SYN 
    unsigned char SEQ = 0;
    //八位，传输的序列号，0~255，超过后mod
    HEADER() {
        sum = 0;//校验和 16位
        datasize = 0;//所包含数据长度 16位
        flag = 0;
        //八位，使用后三位，排列是FIN ACK SYN 
        SEQ = 0;
    }
};

int Connect(SOCKET& sock, SOCKADDR_IN& servAdr)//三次握手建立连接
{
    SOCKADDR_IN from_adr;
    int from_sz;

    from_sz = sizeof(from_adr);

    packet_head h;
    h.first();
    printf("第一次握手：发送连接请求,等待回应\n");
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));

    int strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | SYN | ACK))
        printf("收到回应，开始建立连接...\n");


    h.flags = 0 | ACK;
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("第三次握手\n");
    return 1;
}


void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{

    //发送包
    HEADER header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.SEQ = unsigned char(order);//序列号
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, sizeof(header) + len);
    u_short check = cksum((u_short*)buffer, sizeof(header) + len);//计算校验和
    header.sum = check;
    memcpy(buffer, &header, sizeof(header));
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送

    cout << "Send message " << len << " bytes!" << " flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
    


    clock_t start = clock();//记录发送时间
    
    //接收回应信息，并进行处理
    while (1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                //超时重传(超时重传必须使用非阻塞模式,每调用一次recvfrom就去检测一下缓冲区,然后去计算时间差)
                header.datasize = len;
                header.SEQ = u_char(order);//序列号
                header.flag = u_char(0x0);
                memcpy(buffer, &header, sizeof(header));
                memcpy(buffer + sizeof(header), message, sizeof(header) + len);
                u_short check = cksum((u_short*)buffer, sizeof(header) + len);//计算校验和
                header.sum = check;

                
                memcpy(buffer, &header, sizeof(header));
                sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//发送
                cout << "TIME OUT! ReSend message " << len << " bytes! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;
                clock_t start = clock();//记录发送时间
            }
        }

        memcpy(&header, buffer, sizeof(header));//缓冲区接收到信息，读取
        u_short check = cksum((u_short*)&header, sizeof(header));

        //检查回应消息
        if (header.SEQ == u_short(order) && header.flag == ACK)
        {
            cout << "Send has been confirmed! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;
            break;
        }
        else
        {
            //错误重发&乱序重发
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
        send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, i == packagenum - 1 ? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum);
        
        seqnum++;
        if (seqnum > 255)
        {
            seqnum = seqnum - 256;
        }
    }

    //文件发送结束，发送一条over消息
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    header.flag = OVER;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;

    memcpy(Buffer, &header, sizeof(header));
    sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End!" << endl;

    //等待服务器端发送over消息
    clock_t start = clock();
    while (1)
    {
        u_long mode = 1;
        //修改为非阻塞模式
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, Buffer, MAXSIZE-1, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                char* Buffer = new char[sizeof(header)];
                header.flag = OVER;
                header.sum = 0;
                u_short temp = cksum((u_short*)&header, sizeof(header));
                header.sum = temp;
                memcpy(Buffer, &header, sizeof(header));
                sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
                cout << "Time Out! ReSend End!" << endl;
                start = clock();
            }
        }
        memcpy(&header, Buffer, sizeof(header));//缓冲区接收到信息，读取

        u_short check = cksum((u_short*)&header, sizeof(header));
        if (header.flag == OVER)
        {
            cout << "对方已成功接收文件!" << endl;
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



int disConnect(SOCKET& sock, SOCKADDR_IN& servAdr)
{
    SOCKADDR_IN from_adr;
    int from_sz = sizeof(from_adr);

    packet_head h;
    h.flags = 0 | FIN;
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("发送断开连接请求,等待回应\n");


    int strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | ACK))
        printf("收到回应，开始建立连接...\n");

    strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | FIN))
        printf("收到服务器断开请求，开始断开连接...\n");

    h.flags = 0 | ACK;
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
    

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(server_addr);

    //建立连接
    if (Connect(sock, server_addr) == -1)
    {
        return 0;
    }

    //读取文件到内存
    ifstream fin;
    string filename;
    cout << "请输入文件名称" << endl;
    cin >> filename;
    fin.open(filename.c_str(), ifstream::binary);//以二进制方式打开文件
    char* buffer = new char[10000000];

    fin.read(buffer, 10000000);
    int bytes = fin.gcount();
        
    //发送文件到服务器
    send(sock, server_addr, len, (char*)(filename.c_str()), filename.length());
    clock_t start = clock();
    send(sock, server_addr, len, buffer, bytes);
    clock_t end = clock();
    cout << "传输总时间为:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "吞吐率为:" << ((float)bytes) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;
   

    //关闭连接
    disConnect(sock, server_addr);
    Sleep(10000);
}

