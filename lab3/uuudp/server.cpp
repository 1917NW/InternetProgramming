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



struct packet_head
{
    u_short sum;//У��� 16λ
    u_short datasize;//���������ݳ��� 16λ
    char flags; // ��־λ
    unsigned int seq; //���к�

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
    if (h.flags == SYN)
        printf("�յ���������\n");

    h.flags = 0 | SYN | ACK;
    sendto(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, sizeof(clntAdr));
    printf("�������˷���ڶ�������\n");


    u_long mode = 1;
    //�޸�Ϊ������ģʽ
    ioctlsocket(servSock, FIONBIO, &mode);
    clock_t start = clock();
    while (recvfrom(servSock, (char*)&h, sizeof(h), 0, (SOCKADDR*)&clntAdr, &clntAdrSz) <= 0)
    {
        if (clock() - start > MAX_TIME)
        {
            //��ʱ�Ͽ�����
            closesocket(servSock);
            return -1;
        }
    }
    //�Ļ�����ģʽ
    mode = 0;
    ioctlsocket(servSock, FIONBIO, &mode);
    
    if (h.flags == (0 | ACK))
        printf("�ɹ���������\n");

    return 1;
}

//server : ���صĵ�ַ��Ϣ
//client : �ͻ��˵ĵ�ַ��Ϣ
//clntAdrSz : ��ſͻ��˵�ַ��Ϣ��С�ĵ�ַ
//buf : ��Ž������ݵĻ�����
//�������Կͻ��˵�һ�����ݷ��ͣ����ҷ��뵽buf���棬���ؽ��յ������ݳ���
int recv_data(SOCKET& sockServ, SOCKADDR_IN& ClientAddr, int& ClientAddrLen, char* buf)
{
    long int file_size = 0;//�ļ�����

    packet_head header;
    char* Buffer = new char[MAXSIZE + sizeof(header)];
    int seq = 0;
    int index = 0;

    //�ȴ��������ݰ����������ݰ����д���
    while (1)
    {
        int length = recvfrom(sockServ, Buffer, sizeof(header) + MAXSIZE, 0, (sockaddr*)&ClientAddr, &ClientAddrLen);
        memcpy(&header, Buffer, sizeof(header));

        //�жϱ��η����Ƿ��ǽ���
        if (header.flags == OVER && cksum((u_short*)&header, sizeof(header)) == 0)
        {
            cout << "Accept Over" << endl;
            break;
        }

        if (header.flags == 0 && cksum((u_short*)Buffer, length - sizeof(header)))
        {
            //�жϽ��յ��İ��Ƿ��ظ���
            if (header.seq != seq)
            {
                //�ظ�����������һ�����кź�ACK
                header.flags = ACK;
                header.datasize = 0;
                header.seq = (seq - 1);
                header.sum = 0;
                u_short temp = cksum((u_short*)&header, sizeof(header));
                header.sum = temp;
                memcpy(Buffer, &header, sizeof(header));
                sendto(sockServ, Buffer, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << "Packet Duplicate  Send ACK:" << (int)header.seq << " SEQ:" << (int)header.seq << endl;
                continue;
            }
            //���յ���ȷ�İ�
            else {
                //ȡ��Buffer�е�����
                cout << "Accept Data " << length - sizeof(header) << " bytes Flag:" << int(header.flags) << " SEQ : " << int(header.seq) << " SUM:" << int(header.sum) << endl;

                //�����ݷ��뵽���ݻ���������
                memcpy(buf + file_size, Buffer + sizeof(header), length - sizeof(header));
                file_size = file_size + int(header.datasize);

                //���͸ð������кź�ACK
                header.flags = ACK;
                header.datasize = 0;
                header.seq = seq;
                header.sum = 0;
                u_short temp1 = cksum((u_short*)&header, sizeof(header));
                header.sum = temp1;
                sendto(sockServ, (char*)&header, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);
                cout << "Send ACK " << " SEQ:" << (int)header.seq << endl;
                seq++;
            }
        }
    }

    //����OVER��Ϣ
    header.flags = OVER;
    header.sum = 0;
    u_short temp = cksum((u_short*)&header, sizeof(header));
    header.sum = temp;
    sendto(sockServ, (char*)&header, sizeof(header), 0, (sockaddr*)&ClientAddr, ClientAddrLen);

    return file_size;
}

int disconnect(SOCKET& servSock, SOCKADDR_IN& clntAdr, int& clntAdrSz)
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
    connect(server, client_addr, clntAdrSz);

    //������Ϣ
    ofstream fout;
    char* name = new char[30];
    long long fileSize;

    //�����ļ���
    int name_len = recv_data(server, client_addr, clntAdrSz, name);
    
    //�����ļ�����
    recv_data(server, client_addr, clntAdrSz, (char*)&fileSize);
    cout << fileSize << endl;

    //�����ļ�����
    char* data = new char[fileSize];
    int data_len = recv_data(server, client_addr, clntAdrSz, data);
    
    //�Ͽ�����
    disconnect(server, client_addr, clntAdrSz);
    
    
    //д�뱾���ļ�
    name[name_len] = '\0';
    fout.open(name, ofstream::binary);
    fout.write(data, data_len);
    fout.close();
    
    while (1)
        ;
}
