环境：Windows系统 & visual studio 2022

chatServer和chatClient为C/C++版本的客户端-服务器-客户端程序(多人聊天程序)

server和Client为C/C++版本的服务器-客户端聊天程序(单人聊天程序)


可执行程序在Debug文件夹下


我本人在编程的过程中遇到的问题：

1.#inet_addr 需要被替换为inet_pton#

解决方案：

右击解决方案视图中的解决方案->选中属性->常规->SDL检查，将SDL检查的选项设置为否

2.#使用vscode编程可能会报错#

可能的解决方案：

在文件夹中的.vscode/task.json的的args中添加"-lwsock32"
