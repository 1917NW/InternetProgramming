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

//定义用户名
char name[16];
int main() {
	//1.确定协议版本
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);
	

	//2.创建套接字
	sSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sSocket == SOCKET_ERROR) {
		printf("创建socket失败");
		return -2;
	}

	//3.确定服务器的协议地址
	SOCKADDR_IN addr = { 0 };
	addr.sin_family = AF_INET;
	addr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	addr.sin_port = htons(9527);

	//4.连接
	int r = connect(sSocket, (sockaddr*)&addr, sizeof(addr));
	if (-1 == r) {
		printf("连接服务器失败");
		return -2;
	}
	printf("连接服务器成功\n");

	printf("请输入你的名字:");
	fgets(name, 16, stdin);
	char* p = strchr(name, '\n');
	if (p != NULL)
		*p = '\0';

	

	//5.通信
	
	//创建一个线程用来发送信息
	CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)Client_send, NULL, NULL, NULL);
	
	//主线程用来接收信息
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
				cout << "对方已结束聊天" << endl;
				break;
			}
			else {
				cout << "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";
				cout << ">>接收消息,消息如下:" << endl;
				buf[r] = 0;
				cout << buf << endl;
				cout << endl;
				cout << ">>";

			}
		}
	}
}

void Client_send() {
		
		//报文
		char buf[512];
		while (1)
		{
			//输入信息
			char msg[512];

			//消息发送时间
			time_t t = time(0);
			char tmp[64];
			cout << endl;

			//输入聊天信息
			printf(">>");
			fgets(msg, 512, stdin);
			char* p = strchr(msg, '\n');
			if (p != NULL)
				*p = '\0';

			//判断聊天结束
			if (strcmp(msg, "depart") == 0) {
				cout << "聊天已结束!" << endl;
				break;
			}

			cout << ">>发送消息,消息如下:" << endl;

			//处理时间
			strftime(tmp, sizeof(tmp), "%Y/%m/%d %X %A ", localtime(&t));
			
			//消息整合处理
			strcpy(buf, "发送人:");
			strcat(buf, name);
			strcat(buf, "\n");
			strcat(buf, "消息:");
			strcat(buf, msg);
			strcat(buf, "\n发送时间:");
			strcat(buf, tmp);
			
			cout << buf << endl;

			//发送信息
			send(sSocket, buf, strlen(buf) + 1, 0);
		}


}