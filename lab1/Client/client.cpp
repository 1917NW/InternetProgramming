#include<winsock2.h>
#include<stdio.h>
#pragma comment(lib,"ws2_32.lib")
#include<windows.h>

int main() {
	//1.确定协议版本
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	//if(LOBYTE(wsaData.wVersion)!=2 || HIBYTE(wsaData.wVersion))

	//2.创建套接字
	SOCKET sSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sSocket == SOCKET_ERROR) {
		printf("创建socket失败");
		return -2;
	}

	//3.确定服务器的协议地址
	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(10086);

	//4.连接
	int r = connect(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (-1 == r) {
		printf("连接服务器失败");
		return -2;
	}
	printf("连接服务器成功\n");

	//5.通信
	char buff[256];
	while (1) {
		memset(buff, 0, 256);
		printf("发送：");
		scanf("%s", buff);
		send(sSocket, buff, strlen(buff), NULL);
	}
}