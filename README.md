# 局域网聊天室
在Linux下实现局域网聊天室，并且具备登录注册、群聊私发等功能

# 服务器
服务器端使用的数据库是SQLite3，这需要另行下载
#编译聊天室的服务器
gcc server.c sqlite3.c -ldl -lpthread -o server

# 客户端
客户端的界面的依赖库为ncurses库函数
#编译聊天室的客户端
gcc client.c -lpthread -o client -lncursesw
