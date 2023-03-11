#include <sys/types.h>
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pthread.h>
#include "hwy_network_chat.h"
#include <stdlib.h>
#include <ncurses.h>
#include <locale.h>
#include <semaphore.h>

//定义三个窗口，在线用户，历史消息，输入框
WINDOW *windowOnline,*windowChat,*windowInput;

//程序中的线程用信号量来进行控制
sem_t sem_id1,sem_id2,sem_id3,sem_id4,sem_id5;

//窗口的长宽以及位置信息
typedef struct _WIN_struct {
    int startx, starty;
    int height, width;
} WIN;

WIN winOnline;      /* 在线用户 */
WIN winChat;   		/* 历史消息 */
WIN winInput;       /* 输入框 */

//窗口初始化
void init_windows() 		
{
    setlocale(LC_ALL,"");	//显示中文
    initscr();
    start_color();
    cbreak();
    refresh();                                   
}

//窗口创建函数
WINDOW *create_newwin(int height, int width, int starty, int startx) {
    WINDOW *local_win;
    local_win = newwin(height, width, starty, startx);
    box(local_win, 0, 0);
    wrefresh(local_win);
    return local_win;
}

//结构体win的初始化，也是对应窗口的大小及位置的设置
void initWin(WIN *p_win, int height, int width, int starty, int startx) {
    p_win->height = height;
    p_win->width = width;
    p_win->starty = starty;
    p_win->startx = startx;
}

//ncurses库中的函数都必须进行同步控制，否则大概率出错
//在线用户对应函数
void *threadfunc_Online(void *p) 
{
    sem_wait(&sem_id1);
    windowOnline = create_newwin(winOnline.height, winOnline.width, winOnline.starty, winOnline.startx);
    mvwprintw(windowOnline,1,1,"在线用户\n");
    wrefresh(windowOnline);
    sem_post(&sem_id2);
}

//历史消息对应函数
void *threadfunc_chat(void *p) 
{
    sem_wait(&sem_id3);
    windowChat = create_newwin(winChat.height, winChat.width, winChat.starty, winChat.startx);
    mvwprintw(windowChat,1,1,"历史消息\n");
    wrefresh(windowChat);
    sem_post(&sem_id4);
}

//输入框对应函数
void *threadfunc_input(void *p) 
{
    sem_wait(&sem_id2);
    windowInput = create_newwin(winInput.height, winInput.width, winInput.starty, winInput.startx);
    mvwprintw(windowInput,1,1,"输入：\n");
    wrefresh(windowInput);
    sem_post(&sem_id3);
}

//登录者帐号信息
static hwy_people_t client_id;

/*
 * 功能：建立网络通信的初始化工作,包括创建套接字和绑定可用端口号
 * 输入参数：无
 * 输出参数：成功返回用于网络通信的套接字，失败返回-1
 */
int sock_init(void)
{
  int ret;
  int portnum;//端口号
  //创建套接字--设置通信为IPv4的TCP通信
  int sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd == -1)
  {
    perror("socket failed");
    return -1;
  }
  
  //配置网络结构体
  /*从50001开始，查找主机中尚未被占用的端口号，然后绑定
   */
  struct sockaddr_in myaddr;
  portnum = 50001;
  while(1){
    if(portnum > 65535){
      printf("bind error because of no port number available!\n");
      return -1;
    }
    myaddr.sin_port = htons(portnum++);       //应用层端口号
    myaddr.sin_family = AF_INET;          //IPv4协议
    myaddr.sin_addr.s_addr = inet_addr("0.0.0.0");//通信对象的IP地址，0.0.0.0表示所有IP地址
    //绑定套接字和结构体----主机自检端口号是否重复，IP是否准确
    ret = bind(sockfd,(struct sockaddr *)&myaddr,sizeof(myaddr));
    if(ret == -1)
    {
      continue;
    }
    else{
      wprintw(windowChat," bind successfully!\n");
      break;
    }
  }
  return sockfd;
}

/*
 *功能：连接服务器
 *输入参数：套接字
 *输出参数：连接成功与否的标志，成功返回1，失败返回0
 * */
int sock_client(int sockfd)
{
  //连接服务器
  struct sockaddr_in srcaddr;
  srcaddr.sin_port = htons(50000);//服务器的应用层端口号
  srcaddr.sin_family = AF_INET;//服务器的IPv4协议
  srcaddr.sin_addr.s_addr = inet_addr("127.0.0.1");//服务器的IP地址-本地主机

  int ret = connect(sockfd,(struct sockaddr*)&srcaddr,sizeof(srcaddr));
  if(ret == -1)
  {
    perror("connect failed");
    return -1;
  }
  wprintw(windowChat," connect OK\n");
  return 0; 
}



/*
 *功能：验证登录或注册帐号信息
 *输入：客户端套接字，登录/注册选择
 *输出：登录成功1，注册成功2，任意失败-1
 * */
int check_id(int sockfd,int choice)
{
  char ID_info[17];//存储帐号信息以及登录/注册选择
  int i;
  char status[16];//存储登录/注册状态
  int ret;

  wprintw(windowChat," check ID information!\n");
  wrefresh(windowChat);
  memset(ID_info,0,17);
  //登录，携带ID 和密码
  if(1 == choice){
    ID_info[0]='1';
    memcpy(ID_info+1,client_id.id,4);
    memcpy(ID_info+9,client_id.passwd,8);
  }

  //注册，携带昵称和密码
  else {
    ID_info[0]='2';
    memcpy(ID_info+5,client_id.name,4);
    memcpy(ID_info+9,client_id.passwd,8);
  }

  //发送帐号信息给服务器端进行验证
  for(i=0;i<16;i++){
  	if(ID_info[i] == '\0'){
		ID_info[i] = '/';
	}
  }
  ID_info[i] = '\0';
  ret = send(sockfd, ID_info, strlen(ID_info), 0);
  if(-1 == ret){
    perror("send id_info error!");
	return -1;
  }

  //接收帐号验证信息
  memset(status,0,16);
  ret = recv(sockfd,status,16,0);
  if(-1 == ret){
    perror("recv id_info error!");
	return -1;
  }
  
  if(memcmp(status,"successfully!",13)==0){
    //登录成功
    //printf("login successfully!\n");
	send(sockfd,"ok",3,0);
	ret = recv(sockfd,client_id.name,4,0);
	if(-1 == ret){
		perror("recv ack_id_info error");
		return -1;
	}
	return 1;
  }
  else if(memcmp(status,"sign up",7)==0){
    //注册成功
    //printf("sign up successfully!\n");
	send(sockfd,"ok",3,0);
	ret = recv(sockfd,client_id.id,4,0);
	if(-1 == ret){
		perror("recv ack_id_info error");
		return -1;
	}
	return 2;
  }
  
  else 
  {
    printf("login or sign up error!\n");
	return -1;
  }
}


/*
 *功能：登录界面
 *输入：客户端套接字
 *输出：成功0，失败-1
 *注意：登录失败可以重新登录，其他失败会退出
 */
int hwy_login(int sockfd )
{
  int choice;//1--代表登录，2代表注册
  int login_status;//登录状态,-1代表失败，1代表成功
  int signup_status;//注册状态，-1代表失败，2代表成功

  while(1){
    //waddstr(windowChat," 1------------login\n 2------------sign up\n");
    mvwprintw(windowChat,2,1," 1------------login\n 2------------sign up\n");
    wrefresh(windowChat);
    mvwscanw(windowInput,2,1,"%d",&choice);
    wredrawln(windowInput,2,8);
	wrefresh(windowInput);
    if(1 == choice){
      //登录，输入ID
      wprintw(windowChat," ID:\n");
      wrefresh(windowChat);
      mvwscanw(windowInput,2,1,"%s",client_id.id); 
      wredrawln(windowInput,2,8);
	  wrefresh(windowInput);
	  //输入密码
      wprintw(windowChat," passwd:\n");
      wrefresh(windowChat);
      mvwscanw(windowInput,2,1,"%s",client_id.passwd);
	  wredrawln(windowInput,2,8);
	  wrefresh(windowInput);
	  
      login_status=check_id(sockfd,choice);
	  if(1 == login_status){
	  	//登录成功,进入聊天室
		wprintw(windowChat," 欢迎登录聊天室～%s\n",client_id.name);
		box(windowChat,0,0);
		wrefresh(windowChat);
		break;
	  }
	  else 
      	continue;
    }
    else if(2 == choice){
	  //注册，输入昵称
      wprintw(windowChat," name:\n");
      wrefresh(windowChat);
	  mvwscanw(windowInput,2,1,"%s",client_id.name);
	  wredrawln(windowInput,2,8);
	  wrefresh(windowInput);
	  //输入密码
	  wprintw(windowChat," passwd:\n");
	  wrefresh(windowChat);
	  mvwscanw(windowInput,2,1,"%s",client_id.passwd);
	  wredrawln(windowInput,2,8);
	  wrefresh(windowInput);
	  
	  signup_status = check_id(sockfd,choice);
	  if(2 == signup_status){
	  	//注册成功，返回登录界面
		wprintw(windowChat," 注册成功\n");
		wprintw(windowChat," 你的帐号为：%s\n 请重新登录\n",client_id.id);
		box(windowChat,0,0);
		wrefresh(windowChat);
		continue;
	  }
	  else {
	  	//注册失败
		return -1;
	  }
    }
    else {
      wprintw(windowChat," 错误！请输入正确数值！\n");
      box(windowChat,0,0);
      wrefresh(windowChat);
      continue;
    }
  }
  return 0;
}


/*
 *功能：获取聊天具体内容
 *输入：保存聊天内容的指针
 *输出：无
 * */
//获取聊天具体内容
void get_send_content(char get_send_buffer[CHAT_STRUCT_SIZE])
{
	  int i;
	  char dest[4];//目标帐号
	  char time_buf[26];//时间
	  time_t t;
	  time(&t);
	  memset(get_send_buffer,0,CHAT_STRUCT_SIZE);
	  //发送者
	  memcpy(get_send_buffer+POSITION_SELFID,client_id.id,4);
	  memcpy(get_send_buffer+POSITION_NAME,client_id.name,4);
	  //接收者
	  mvwprintw(windowInput,2,1,"你要发给谁？\n");
	  mvwscanw(windowInput,3,1,"%s",get_send_buffer+POSITION_DESTID);
	  //发送内容
	  mvwprintw(windowInput,4,1,"你要发什么？\n");
	  mvwscanw(windowInput,5,1,"%s",get_send_buffer+POSITION_CONTENT);
	  //重置输入框
	  wredrawln(windowInput,2,8);
	  box(windowInput,0,0);
	  wrefresh(windowInput);
	  //发送时间
	  memcpy(get_send_buffer+POSITION_TIME,ctime_r(&t,time_buf),26);  
	  for(i=0;i<POSITION_CONTENT;i++){
	  	if(get_send_buffer[i] == '\0')
			get_send_buffer[i] = '/';
	  }
}


/*
 *功能：处理接收信息的子线程处理函数
 *
 * */
void *pthread_recv_func (void *arg)
{
  sem_wait(&sem_id4);
  int sockfd = *(int *)arg;
  int ret;
  int i;
  char OnlineUser[20] = {0};
  
  hwy_recv_msg_t hwy_msg;//谁发的，什么时候发，发了什么
  char recv_buffer[CHAT_STRUCT_SIZE];//接收内容缓冲区
  wprintw(windowChat," 现在可以聊天了～\n");
  wrefresh(windowChat);
  sem_destroy(&sem_id4);
  sem_post(&sem_id5);
  while(1){
    memset(recv_buffer,0,CHAT_STRUCT_SIZE);
    ret=recv(sockfd, recv_buffer,CHAT_STRUCT_SIZE, 0);
    if(ret == -1){
      wprintw(windowChat," client received error!\n");
      return;
    }
    else {
		for(i=0;i<POSITION_CONTENT;i++){
			if(recv_buffer[i]== '/')
				recv_buffer[i]= '\0';
		}
		//对接收的信息进行处理
		memcpy(hwy_msg.who,recv_buffer+POSITION_NAME,4);
		memcpy(hwy_msg.time,recv_buffer+POSITION_TIME,26);
		memcpy(hwy_msg.content,recv_buffer+POSITION_CONTENT,128);
		//从上线提醒中获得用户id并更新在线用户列表
		if(strstr(hwy_msg.content,"在线") != NULL)
		{	memcpy(hwy_msg.who,recv_buffer+POSITION_CONTENT,strlen(hwy_msg.content)-7);
			wprintw(windowOnline," %s\n",hwy_msg.who);
			box(windowOnline,0,0);
			wrefresh(windowOnline);
		}
        wprintw(windowChat," %s:%s\n%s",hwy_msg.who,hwy_msg.content,hwy_msg.time);
        scrollok(windowChat,TRUE);
        box(windowChat,0,0);
        wrefresh(windowChat);
	}
  }

}

void *pthread_send_func (void *arg)
{
  sem_wait(&sem_id5);
  int sockfd = *(int *)arg;
  char send_buffer[CHAT_STRUCT_SIZE];//发送内容缓冲区
  sem_destroy(&sem_id5);
  while(1){
    get_send_content(send_buffer);
	//输入bye退出聊天室
	if(memcmp(send_buffer+POSITION_CONTENT,"bye",3)== 0 ){
		wprintw(windowChat," 你已下线\n");
    	send(sockfd, send_buffer, strlen(send_buffer), 0);
		close(sockfd);
		endwin();
		exit(0);
	}

    send(sockfd, send_buffer, strlen(send_buffer), 0);
  }
}


int main(int argc,char *argv[])
{
	  init_windows();
	  //不同窗口的大小位置的设置
	  initWin(&winOnline, LINES, COLS*0.20, 0 , 0);
	  initWin(&winChat, LINES*0.50, COLS*0.80, 0, COLS*0.2);
	  initWin(&winInput, LINES*0.50, COLS*0.80, LINES*0.50, COLS*0.20);
	  
	  //信号量的初始化
	  sem_init(&sem_id1,0,1);
	  sem_init(&sem_id2,0,0);
	  sem_init(&sem_id3,0,0);
	  sem_init(&sem_id4,0,0);
	  sem_init(&sem_id5,0,0);
	  
	  //三个单纯用来创建窗口
	  pthread_t tid1,tid2,pidOnline,pidChat,pidInput;
	  
	  pthread_create(&pidOnline, NULL, threadfunc_Online, NULL);
	  pthread_create(&pidInput, NULL, threadfunc_input, NULL);
	  pthread_create(&pidChat, NULL, threadfunc_chat, NULL);
	  
	  int sockfd = sock_init();
	  if(sockfd == -1){
		printf("sock_init error!\n");
		return -1;
	  }
	  int ret = sock_client(sockfd);
	  if(-1 == ret){
		printf("sock_client error!\n");
		return -1;
	  }
	  
	  ret = hwy_login(sockfd);
	  if(ret == -1){
		perror("hwy_login function error!");
		return -1;
	  }
	  
	  //一个线程负责接收信息，一个线程负责发送信息
	  pthread_create(&tid2,0,pthread_recv_func,&sockfd);
	  pthread_create(&tid1,0,pthread_send_func,&sockfd);
	  
	  while(1){
	  	sleep(9);
	  }

	  close(sockfd);
	  return 0;
}	


