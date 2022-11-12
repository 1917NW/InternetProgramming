#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include<Windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int MAXSIZE = 1024;//���仺������󳤶�
const char SYN = 0x1; //SYN = 1 ACK = 0
const char ACK = 0x2;//SYN = 0, ACK = 1��FIN = 0
const char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const char FIN = 0x4;//FIN = 1 ACK = 0
const char FIN_ACK = 0x5;
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


/*
1.��α�ײ���ӵ�UDP�ϣ�
2.�����ʼʱ����Ҫ��������ֶ�����ģ�
3.������λ����Ϊ16λ��2�ֽڣ�����
4.������16λ������ӣ����������λ���򽫸���16�ֽڵĽ�λ���ֵ�ֵ�ӵ����λ�ϣ�������0xBB5E+0xFCED=0x1 B84B����1�ŵ����λ���õ������0xB84C
5.����������ӵõ��Ľ��Ӧ��Ϊһ��16λ������������ȡ������Եõ������checksum�� */

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

int Connect(SOCKET& sock, SOCKADDR_IN& servAdr)//�������ֽ�������
{
    SOCKADDR_IN from_adr;
    int from_sz;

    from_sz = sizeof(from_adr);

    packet_head h;
    h.first();
    printf("��һ�����֣�������������,�ȴ���Ӧ\n");
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));

    int strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | SYN | ACK))
        printf("�յ���Ӧ����ʼ��������...\n");


    h.flags = 0 | ACK;
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("����������\n");
    return 1;
}


void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{

    //���Ͱ�
    HEADER header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.SEQ = unsigned char(order);//���к�
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, sizeof(header) + len);
    u_short check = cksum((u_short*)buffer, sizeof(header) + len);//����У���
    header.sum = check;
    memcpy(buffer, &header, sizeof(header));
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//����

    cout << "Send message " << len << " bytes!" << " flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << " SUM:" << int(header.sum) << endl;
    


    clock_t start = clock();//��¼����ʱ��
    
    //���ջ�Ӧ��Ϣ�������д���
    while (1)
    {
        u_long mode = 1;
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                //��ʱ�ش�(��ʱ�ش�����ʹ�÷�����ģʽ,ÿ����һ��recvfrom��ȥ���һ�»�����,Ȼ��ȥ����ʱ���)
                header.datasize = len;
                header.SEQ = u_char(order);//���к�
                header.flag = u_char(0x0);
                memcpy(buffer, &header, sizeof(header));
                memcpy(buffer + sizeof(header), message, sizeof(header) + len);
                u_short check = cksum((u_short*)buffer, sizeof(header) + len);//����У���
                header.sum = check;

                
                memcpy(buffer, &header, sizeof(header));
                sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//����
                cout << "TIME OUT! ReSend message " << len << " bytes! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;
                clock_t start = clock();//��¼����ʱ��
            }
        }

        memcpy(&header, buffer, sizeof(header));//���������յ���Ϣ����ȡ
        u_short check = cksum((u_short*)&header, sizeof(header));

        //����Ӧ��Ϣ
        if (header.SEQ == u_short(order) && header.flag == ACK)
        {
            cout << "Send has been confirmed! Flag:" << int(header.flag) << " SEQ:" << int(header.SEQ) << endl;
            break;
        }
        else
        {
            //�����ط�&�����ط�
            continue;
        }
    }

    //�Ļ�����ģʽ
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

    //�ļ����ͽ���������һ��over��Ϣ
    HEADER header;
    char* Buffer = new char[sizeof(header)];
    header.flag = OVER;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;

    memcpy(Buffer, &header, sizeof(header));
    sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End!" << endl;

    //�ȴ��������˷���over��Ϣ
    clock_t start = clock();
    while (1)
    {
        u_long mode = 1;
        //�޸�Ϊ������ģʽ
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
        memcpy(&header, Buffer, sizeof(header));//���������յ���Ϣ����ȡ

        u_short check = cksum((u_short*)&header, sizeof(header));
        if (header.flag == OVER)
        {
            cout << "�Է��ѳɹ������ļ�!" << endl;
            break;
        }
        else
        {
            continue;
        }
    }

    //�Ļ�����ģʽ
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
    printf("���ͶϿ���������,�ȴ���Ӧ\n");


    int strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | ACK))
        printf("�յ���Ӧ����ʼ��������...\n");

    strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | FIN))
        printf("�յ��������Ͽ����󣬿�ʼ�Ͽ�����...\n");

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

    //���÷������ĵ�ַ
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(9527);
    

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(server_addr);

    //��������
    if (Connect(sock, server_addr) == -1)
    {
        return 0;
    }

    //��ȡ�ļ����ڴ�
    ifstream fin;
    string filename;
    cout << "�������ļ�����" << endl;
    cin >> filename;
    fin.open(filename.c_str(), ifstream::binary);//�Զ����Ʒ�ʽ���ļ�
    char* buffer = new char[10000000];

    fin.read(buffer, 10000000);
    int bytes = fin.gcount();
        
    //�����ļ���������
    send(sock, server_addr, len, (char*)(filename.c_str()), filename.length());
    clock_t start = clock();
    send(sock, server_addr, len, buffer, bytes);
    clock_t end = clock();
    cout << "������ʱ��Ϊ:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "������Ϊ:" << ((float)bytes) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;
   

    //�ر�����
    disConnect(sock, server_addr);
    Sleep(10000);
}

