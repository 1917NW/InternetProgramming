#include<winsock2.h>
#include<stdio.h>
#pragma comment(lib,"ws2_32.lib")
#include<windows.h>



#define NUM 1024
int count = 0;
SOCKET cSocket[NUM];
void communicate(int index);
int main() {
	//1.ȷ��Э��汾
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//if(LOBYTE(wsaData.wVersion)!=2 || HIBYTE(wsaData.wVersion))

	//2.�����׽���
	SOCKET sSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sSocket == SOCKET_ERROR) {
		printf("����socketʧ��");
		return -2;
	}
	printf("����socket�ɹ�\n");
	//3.ȷ����������Э���ַ
	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(9527);

	//4.��IP��ַ�Ͷ˿ں�
	int r = bind(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (r == -1) {
		printf("��ʧ��");
		closesocket(sSocket);
		WSACleanup();
		return -2;
	}
	printf("�󶨳ɹ�\n");

	//5.����(���������״̬)
	r = listen(sSocket, 10);
	printf("��ʼ����\n");
	for (int i = 0;i < NUM;i++) {
		cSocket[i] = accept(sSocket, NULL, NULL);
		if (SOCKET_ERROR == cSocket[i]) {
			printf("����������");
			closesocket(sSocket);
			WSACleanup();
			return -3;
		}
		count++;
		printf("�пͻ��������Ϸ�������!\n");

		//6.ͨ��
		CreateThread(NULL, NULL,
			(LPTHREAD_START_ROUTINE)communicate, (LPVOID)i, NULL, NULL);
	}
	


	
}

void communicate(int index) {
	char buf[512] = { 0 };
	int r;
	while (1) {
		r = recv(cSocket[index], buf, 512, NULL);
		if (r > 0) {
			buf[r] = 0;
			printf(">>����%d����Ϣ:\n%s\n",index, buf);

			
			for (int i = 0;i < count;i++) {
				//������index����Ϣת����������
				if(i!=index)
					send(cSocket[i], buf, strlen(buf), NULL);
			}
			memset(buf, 0, 512);
		}
	}
}