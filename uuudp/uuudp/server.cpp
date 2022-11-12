#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include<Windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;


const int MAXSIZE = 1024;//���仺������󳤶�
const char SYN = 0x1; 
const char ACK = 0x2;
const char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const char FIN = 0x4;//FIN = 1 ACK = 0
const char FIN_ACK = 0x5;//FIN = 1 ACK = 0
const char OVER = 0x7;//������־
double MAX_TIME = 0.5 * CLOCKS_PER_SEC;

struct packet_head {
    char flags; //��־λ
    int length; //���ݳ���
    int send_seq; //�������к�
    int ack_seq; //ȷ�����к�
    int check_sum; //У��ͣ�ֻУ��packet_head

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
    u_short sum = 0;//У��� 16λ
    u_short datasize = 0;//���������ݳ��� 16λ
    unsigned char flag = 0;
    //��λ��ʹ�ú���λ��������FIN ACK SYN 
    unsigned char SEQ = 0;
    //��λ����������кţ�0~255��������mod
    HEADER() {
        sum = 0;//У��� 16λ
        datasize = 0;//���������ݳ��� 16λ
        flag = 0;
        //��λ��ʹ�ú���λ��������FIN ACK SYN 
        SEQ = 0;
    }
};

int Connect(SOCKET& servSock, SOCKADDR_IN& clntAdr, int& clntAdrSz)
{ 
    packet_head h;

    int strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == SYN)
        printf("�յ���������\n");

    h.flags = 0 | SYN | ACK;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("����������\n");

    strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == (0 | ACK))
        printf("�ɹ���������\n");
    return 1;
}

int RecvMessage(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* message)
{
    long int all = 0;//�ļ�����
    HEADER header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;

    //�ȴ��������ݰ����������ݰ����д���
    while (1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
        memcpy(&header, Buffer, sizeof(header));

        //�жϱ��η����Ƿ��ǽ���
        if (header.flag == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "�ļ��������" << endl;
            break;
        }

        
        if (header.flag == unsigned char(0) && cksum((u_short*)Buffer, length - sizeof(header)))
        {
            //�жϽ��յ��İ��Ƿ���Ԥ�ڵİ�
            if (seq != int(header.SEQ))
            {
                //˵���������⣬����ACK
                header.flag = ACK;
                header.datasize = 0;
                header.SEQ = (unsigned char)seq;
                header.sum = 0;
                u_short temp = cksum((u_short*)&header, sizeof(header));
                header.sum = temp;
                memcpy(Buffer, &header, sizeof(header));

                //�ط��ð���ACK
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << "Send to Clinet ACK:" << (int)header.SEQ << " SEQ:" << (int)header.SEQ << endl;
                continue;//���������ݰ�
            }

            seq = int(header.SEQ);
            if (seq > 255)
            {
                seq = seq - 256;
            }

            //ȡ��buffer�е�����
            cout << "Send message " << length - sizeof(header) << " bytes!Flag:" << int(header.flag) << " SEQ : " << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
            char* temp = new char[length - sizeof(header)];

            memcpy(temp, Buffer + sizeof(header), length - sizeof(header));
            memcpy(message + all, temp, length - sizeof(header));
            all = all + int(header.datasize);

            //����ACK
            header.flag = ACK;
            header.datasize = 0;
            header.SEQ = (unsigned char)seq;
            header.sum = 0;
            u_short temp1 = cksum((u_short*)&header, sizeof(header));
            header.sum = temp1;
            memcpy(Buffer, &header, sizeof(header));
            //�ط��ð���ACK
            sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
            cout << "Send to Clinet ACK:" << (int)header.SEQ << " SEQ:" << (int)header.SEQ << endl;
            seq++;
            if (seq > 255)
            {
                seq = seq - 256;
            }
        }
    }

    //����OVER��Ϣ
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
    printf("�յ��ͻ��˶Ͽ�����\n");

    h.flags = 0 | ACK;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("����ȷ����Ϣ\n");

    h.flags = 0 | FIN;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("���ͷ������˶Ͽ�����\n");

    strLen = recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz);
    if (h.flags == ACK)
        printf("�������Ͽ�����...\n");
    return 1;
}


int main()
{
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2, 2), &wsadata);

    SOCKADDR_IN server_addr,client_addr;
    SOCKET server;
    


    server = socket(AF_INET, SOCK_DGRAM, 0);

    //���÷������ĵ�ַ
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(9527);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    //�������������ַ��Ϣ
    bind(server, (SOCKADDR*)&server_addr, sizeof(server_addr));

    int clntAdrSz = sizeof(client_addr);
    
    //��������
    Connect(server, client_addr, clntAdrSz);


    ofstream fout;
    char* name = new char[20];
    char* data = new char[100000000];

    //��������
    int namelen = RecvMessage(server, client_addr, clntAdrSz, name);
    int datalen = RecvMessage(server, client_addr, clntAdrSz, data);
    
    disConnect(server, client_addr, clntAdrSz);
    
    
    //д�뱾���ļ�
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

