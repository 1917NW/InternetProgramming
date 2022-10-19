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

	//4.绑定
	int r = bind(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (r == -1) {
		printf("绑定失败");
		closesocket(sSocket);
		WSACleanup();
		return -2;
	}

	printf("绑定成功\n");
	r = listen(sSocket, 10);

	SOCKET cSocket = accept(sSocket, NULL, NULL);
	if (SOCKET_ERROR == cSocket) {
		printf("服务器崩溃");
		closesocket(sSocket);
		WSACleanup();
		return -3;
	}
	printf("有客户端连接上服务器了!\n");
	
	//通信
	char buff[256] = { 0 };

	while (1) {
		r = recv(cSocket, buff, 255, NULL);
		if (r > 0) {
			buff[r] = 0;
			printf(">>%s\n", buff);
		}
	}
}