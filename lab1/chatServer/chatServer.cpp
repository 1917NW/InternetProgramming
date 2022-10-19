#include<winsock2.h>
#include<stdio.h>
#pragma comment(lib,"ws2_32.lib")
#include<windows.h>



#define NUM 1024
int count = 0;
SOCKET cSocket[NUM];
void communicate(int index);
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
	printf("创建socket成功\n");
	//3.确定服务器的协议地址
	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(9527);

	//4.绑定IP地址和端口号
	int r = bind(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (r == -1) {
		printf("绑定失败");
		closesocket(sSocket);
		WSACleanup();
		return -2;
	}
	printf("绑定成功\n");

	//5.监听(进入可连接状态)
	r = listen(sSocket, 10);
	printf("开始监听\n");
	for (int i = 0;i < NUM;i++) {
		cSocket[i] = accept(sSocket, NULL, NULL);
		if (SOCKET_ERROR == cSocket[i]) {
			printf("服务器崩溃");
			closesocket(sSocket);
			WSACleanup();
			return -3;
		}
		count++;
		printf("有客户端连接上服务器了!\n");

		//6.通信
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
			printf(">>来自%d的消息:\n%s\n",index, buf);

			
			for (int i = 0;i < count;i++) {
				//把来自index的消息转发给其他人
				if(i!=index)
					send(cSocket[i], buf, strlen(buf), NULL);
			}
			memset(buf, 0, 512);
		}
	}
}