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

	//4.����
	int r = connect(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (-1 == r) {
		printf("���ӷ�����ʧ��");
		return -2;
	}
	printf("���ӷ������ɹ�\n");

	//5.ͨ��
	char buff[256];
	while (1) {
		memset(buff, 0, 256);
		printf("���ͣ�");
		scanf("%s", buff);
		send(sSocket, buff, strlen(buff), NULL);
	}
}