#include<winsock2.h>
#include<stdio.h>
#include<iostream>
#pragma comment(lib,"ws2_32.lib")
#include<windows.h>
#include<time.h>
#include<string.h>
using namespace std;
void Client_send();
SOCKET sSocket;

//�����û���
char name[16];
int main() {
	//1.ȷ��Э��汾
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	

	//2.�����׽���
	sSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sSocket == SOCKET_ERROR) {
		printf("����socketʧ��");
		return -2;
	}

	//3.ȷ����������Э���ַ
	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(9527);

	//4.����
	int r = connect(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (-1 == r) {
		printf("���ӷ�����ʧ��");
		return -2;
	}
	printf("���ӷ������ɹ�\n");

	printf("�������������:");
	fgets(name, 16, stdin);
	char* p = strchr(name, '\n');
	if (p != NULL)
		*p = '\0';

	

	//5.ͨ��
	
	//����һ���߳�����������Ϣ
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Client_send, NULL, NULL, NULL);
	
	//���߳�����������Ϣ
	char buff[256];
	while (1) {
		char buf[512];
		while (1)
		{
			int r = recv(sSocket, buf, 512, 0);
			if (r < 0) {
				break;
			}
			else if (r == 0) {
				cout << "ERROR_RECV";
			}
			else if (strcmp(buf, "End") == 0) {
				cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
				cout << "�Է��ѽ�������" << endl;
				break;
			}
			else {
				cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
				cout << ">>������Ϣ,��Ϣ����:" << endl;
				buf[r] = 0;
				cout << buf << endl;
				cout << endl;
				cout << ">>";

			}
		}
	}
}

void Client_send() {
		
		//����
		char buf[512];
		while (1)
		{
			//������Ϣ
			char msg[512];

			//��Ϣ����ʱ��
			time_t t = time(0);
			char tmp[64];
			cout << endl;

			//����������Ϣ
			printf(">>");
			fgets(msg, 512, stdin);
			char* p = strchr(msg, '\n');
			if (p != NULL)
				*p = '\0';

			//�ж��������
			if (strcmp(msg, "depart") == 0) {
				cout << "�����ѽ���!" << endl;
				break;
			}

			cout << ">>������Ϣ,��Ϣ����:" << endl;

			//����ʱ��
			strftime(tmp, sizeof(tmp), "%Y/%m/%d %X %A ", localtime(&t));
			
			//��Ϣ���ϴ���
			strcpy(buf, "������:");
			strcat(buf, name);
			strcat(buf, "\n");
			strcat(buf, "��Ϣ:");
			strcat(buf, msg);
			strcat(buf, "\n����ʱ��:");
			strcat(buf, tmp);
			
			cout << buf << endl;

			//������Ϣ
			send(sSocket, buf, strlen(buf) + 1, 0);
		}


}