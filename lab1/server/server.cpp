#include<winsock2.h>
#include<stdio.h>
#pragma comment(lib,"ws2_32.lib")
#include<windows.h>

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

	//3.ȷ����������Э���ַ
	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(10086);

	//4.��
	int r = bind(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (r == -1) {
		printf("��ʧ��");
		closesocket(sSocket);
		WSACleanup();
		return -2;
	}

	printf("�󶨳ɹ�\n");
	r = listen(sSocket, 10);

	SOCKET cSocket = accept(sSocket, NULL, NULL);
	if (SOCKET_ERROR == cSocket) {
		printf("����������");
		closesocket(sSocket);
		WSACleanup();
		return -3;
	}
	printf("�пͻ��������Ϸ�������!\n");
	
	//ͨ��
	char buff[256] = { 0 };

	while (1) {
		r = recv(cSocket, buff, 255, NULL);
		if (r > 0) {
			buff[r] = 0;
			printf(">>%s\n", buff);
		}
	}
}