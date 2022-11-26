#include <iostream>
#include <WINSOCK2.h>
#include <time.h>
#include <fstream>
#include<Windows.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

const int MAXSIZE = 1024;//���仺������󳤶�
const int dup_count_MAX = 50;//����ش�����
const char SYN = 0x1; //SYN = 1 ACK = 0
const char ACK = 0x2;//SYN = 0, ACK = 1��FIN = 0
const char FIN = 0x4;//FIN = 1 ACK = 0
const char OVER = 0x5;//������־
double MAX_TIME = 1* CLOCKS_PER_SEC;
string Flags[] = { "ZERO","SYN","ACK","SYN | ACK","FIN","OVER" };

//��¼�ش�����
int dup_count = 0;

//���ڴ�С
int windows = 10;



u_short cksum(u_short* message, int size) {
    int count = (size + 1) / 2;
    u_short* buf = (u_short*)malloc(size + 1);

    //���һλ����
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
    u_short sum;//У��� 16λ
    u_short datasize;//���������ݳ��� 16λ
    char flags; // ��־λ
     int seq; //���к�

    packet_head() {
        sum = 0;
        datasize = 0;
        flags = 0;
        seq = 0;
    }
};

int connect(SOCKET& sock, SOCKADDR_IN& servAdr)//�������ֽ�������
{
    SOCKADDR_IN from_adr;
    int from_sz;

    from_sz = sizeof(from_adr);

    packet_head h;
    h.datasize = 0;
    h.flags = SYN;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    printf("��һ�����֣�������������,�ȴ���Ӧ\n");
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));

    u_long mode = 1;
    //�޸�Ϊ������ģʽ
    ioctlsocket(sock, FIONBIO, &mode);
    clock_t start = clock();
    while (recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz) <= 0)
    {
        if (clock() - start > 5 * MAX_TIME)
        {
            //��ʱ�Ͽ�����
            closesocket(sock);
            return -1;
        }
    }
    //�Ļ�����ģʽ

    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);

    if (h.flags == (0 | SYN | ACK) && cksum((u_short*)&h, sizeof(h)) == 0)
        printf("�յ���Ӧ����ʼ��������...\n");
    else
        return -1;


    h.flags = 0 | ACK;
    h.sum = 0;
    h.sum = cksum((u_short*)&h, sizeof(h));
    sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
    printf("�ͻ��˷������������\n");
    return 1;
}


void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{


    //���ð���ͷ��
    packet_head header;
    char* buffer = new char[MAXSIZE + sizeof(header)];
    header.datasize = len;
    header.seq = order;//���к�
    header.flags = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));

    //����udp���ݰ�
    memcpy(buffer, &header, sizeof(header));
    memcpy(buffer + sizeof(header), message, len);
    sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//����

    cout << "Send data " << len << " bytes " << " flags:" << Flags[(header.flags)] << " SEQ:" << int(header.seq) << " SUM:" << int(header.sum) << endl;

    
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
    //����Ϊ[0:packagenum-1]
    int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
    
    //seqnumΪ[0:packagenum-1]
    int seqnum = 0;

    //���ڵ�ͷ����
    //headǰ��İ�(����headָ��İ�)���Ѿ�����
    int base = -1;

    packet_head h_buffer;
    //������Ҫ���͵İ�
    int nextseqnum = 0;

    int index = 0;
    clock_t start;
    cout << "�ܹ�Ҫ���͵İ�"<<packagenum << endl;
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
            //�յ���Ӧ�������Ӧ�����м��
            
            if (cksum((u_short*)&h_buffer, sizeof(h_buffer))!=0 || h_buffer.flags!=ACK) {
                
                cout << (cksum((u_short*)&h_buffer, sizeof(h_buffer)) != 0) << endl;
                cout << (h_buffer.flags != ACK) << endl;
               
                nextseqnum = base + 1;
                cout << "��Ӧ���𻵣���ʼ�ش�" << endl;
               
                continue;
            }
            else {
                //����base

               
                if ((h_buffer.seq > base) && (h_buffer.seq < nextseqnum)) {
                    base = int(h_buffer.seq);
                    cout << "Send has been confirmed! Flags " << Flags[h_buffer.flags] << " SEQ:" << int(h_buffer.seq) << endl;
                }
                
                //����ʱ��
                start = clock();
            }
        }
        else {
            if (clock() - start > MAX_TIME) {
                nextseqnum = base + 1;
                cout << "ʱ�䳬ʱ����ʼ�ش�" << endl;
            }
        }
        mode = 0;
        ioctlsocket(socketClient, FIONBIO, &mode);

    }


    

    //�ļ����ͽ���������over��Ϣ
    packet_head header;
    char* Buffer = new char[sizeof(header)];
    header.flags = OVER;
    header.sum = 0;
    header.sum = cksum((u_short*)&header, sizeof(header));

    sendto(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
    cout << "Send End" << endl;

    //����over�ط�������
    packet_head header_copy = header;


    //�ȴ��������˷���over��Ϣ
    start = clock();
    dup_count = 0;
    while (1)
    {
        u_long mode = 1;
        //�޸�Ϊ������ģʽ
        ioctlsocket(socketClient, FIONBIO, &mode);
        while (recvfrom(socketClient, (char*)&header, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0)
        {
            if (clock() - start > MAX_TIME)
            {
                dup_count++;
                //�ط�����һ�������͹ر�����
                if (dup_count >= dup_count_MAX)
                    closesocket(socketClient);
                //��ʱ�ط�
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

    //�Ļ�����ģʽ
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
    printf("���ͶϿ���������,�ȴ���Ӧ\n");

    packet_head h_buffer;
    clock_t start = clock();
    dup_count = 0;

    u_long mode = 1;
    //�޸�Ϊ������ģʽ
    ioctlsocket(sock, FIONBIO, &mode);
    while (recvfrom(sock, (char*)&h_buffer, sizeof(h_buffer), 0, (SOCKADDR*)&from_adr, &from_sz) <= 0)
    {
        if (clock() - start > MAX_TIME)
        {
            dup_count++;
            if (dup_count >= dup_count_MAX)
                closesocket(sock);
            //��ʱ�ط�
            sendto(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&servAdr, sizeof(servAdr));
            cout << "Time Out! ReSend End!" << endl;
            start = clock();
        }
    }
    if (h_buffer.flags == (0 | ACK)) {
        printf("�յ���Ӧ����ʼ��������...\n");
    }
    //�޸�Ϊ����ģʽ
    mode = 0;
    ioctlsocket(sock, FIONBIO, &mode);



    strLen = recvfrom(sock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&from_adr, &from_sz);
    if (h.flags == (0 | FIN))
        printf("�յ��������Ͽ����󣬿�ʼ�Ͽ�����...\n");

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

    //���÷������ĵ�ַ
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(9527);

    //���ÿͻ����׽���
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    int len = sizeof(server_addr);

    //��������
    if (connect(sock, server_addr) == -1)
    {
        return 0;
    }

    //��ȡ�ļ����ڴ�
    ifstream fin;
    string fileName;
    cout << "�������ļ���:" << endl;
    cin >> fileName;
    fin.open(fileName.c_str(), ios::in | ios::binary);

    //��ȡ�ļ���С
    fin.seekg(0, ios::end);
    long long fileSize = (int)fin.tellg();
    fin.seekg(0, ios::beg);

    //�����ڴ滺���������ļ������ڴ�
    char* buffer = new char[fileSize];
    fin.read(buffer, fileSize);
    long long bytes = fin.gcount();

    //�����ļ���������
    //1.�����ļ���
    send(sock, server_addr, len, (char*)(fileName.c_str()), fileName.length());

    //2.�����ļ���С
    send(sock, server_addr, len, (char*)(&fileSize), sizeof(fileSize));

    //3.�����ļ�����
    clock_t start = clock();
    send(sock, server_addr, len, buffer, bytes);
    clock_t end = clock();

    //�ر�����
    disconnect(sock, server_addr);

    cout << "������ʱ��Ϊ:" << (end - start) / CLOCKS_PER_SEC << "s" << endl;
    cout << "������Ϊ:" << ((float)bytes) / ((end - start) / CLOCKS_PER_SEC) << "byte/s" << endl;

    while (1)
        ;
}