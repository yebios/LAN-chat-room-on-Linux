#include <sys/types.h>     
#include <sys/socket.h>
#include <stdio.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "hwy_network_chat.h"
#include "sqlite3.h"
#include <stdlib.h>


//id--fd结构体类型
typedef struct
{
	int client_fd;
	char client_id[4];
}client_id_to_fd;

//将用户帐号和占用的文件描述符一一对应起来，
//方便后续一对一通信
client_id_to_fd id_to_fd[CLIENT_MAX];

//数据库的连接指针
static sqlite3 *db = NULL;

/*
 *功能：建立网络通信的初始化工作,包括创建套接字和绑定可用端口号
 *输入：无
 *输出：成功返回套接字，失败返回-1
 * */
int sock_init(void )
{
  int ret;
  //创建套接字--设置通信为IPv4的TCP通信
  int sockfd = socket(AF_INET,SOCK_STREAM,0);
  if(sockfd == -1)
  {
    perror("socket failed");
    return -1;
  }

  //配置网络结构体
  struct sockaddr_in myaddr; 
  myaddr.sin_port = htons(50000);	//应用层端口号
  myaddr.sin_family = AF_INET;		//IPv4协议
  myaddr.sin_addr.s_addr = inet_addr("0.0.0.0");//通信对象的IP地址，0.0.0.0表示所有IP地址

  //绑定套接字和结构体----主机自检端口号是否重复，IP是否准确
  ret = bind(sockfd,(struct sockaddr *)&myaddr,sizeof(myaddr));
  if(ret == -1)
  {
    perror("bind failed");
    return -1;
  }
  
  return sockfd;
}

/*
 *功能：把服务器接收的信息发给所有人
 *输入：聊天信息具体内容
 *输出：无
 * */
void SendMsgToAll(char *msg)
{
  int i;
  for(i=0;i<CLIENT_MAX;i++){
    if(id_to_fd[i].client_fd != 0){
      printf("sendto%s\n",id_to_fd[i].client_id);
      printf("%s\n",msg);
      send(id_to_fd[i].client_fd,msg,strlen(msg),0);
    }
  }
}

/*
 * 功能：把服务器接收的消息发给指定的人
 * 输入：目标帐号所绑定的fd，具体聊天内容
 * 输出：无
 */
void SendMsgToSb(int destfd,char *msg)
{
  int i;
  for(i=0;i<CLIENT_MAX;i++){
    if(id_to_fd[i].client_fd == destfd ){
  		printf("sendto%s\n",id_to_fd[i].client_id);
  		printf("%s\n",msg);
  		send(destfd,msg,strlen(msg),0);
		break;
  	}
  }
}



//数据库初始化工作
//连接数据库，创建表格
void hwyDataBase_init(void )
{
  // 打开hwyhwy.db的数据库，如果数据库不存在，则创建并打开
  sqlite3_open_v2("hwyhwy.db",&db,
  				SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE,//可读可写可创建
				NULL);
  if(db==NULL)
  {
	perror("sqlite3_open_v2 faield\n");
	exit(-1);
  }
  
  //在数据库中创建表格
  //ID 昵称 密码，ID为主键
  char *errmsg = NULL;
  char *sql = "CREATE TABLE if not exists hwy_id_sheet(id text primary key,\
  				name text not null,passwd text not null);";
  sqlite3_exec(db, sql, NULL, NULL, &errmsg);
}


/*
 *功能：验证客户端传来的ID信息
 *输入：服务器accept后产生的套接字
 *输出：验证状态，1登录成功，2注册成功，其他失败
 * */
int check_recv_id(int fd )
{

  int ret;
  int i;
  hwy_people_t client_ID_info; //帐号信息结构体ID_info
  char recv_ID_info[17];//客户端传来的帐号信息其登录/注册选择
  char ack_ID_info[4];
  char ack_ok[3];
  char *errmsg = NULL;
  char sql[128];

  int nrow=0;
  int ncolumn=0;
  char **azResult=NULL;
  char status[16];
  
  hwy_send_msg_t hwy_C_SendMsg;	
	
	
  //接收ID_info
  memset(recv_ID_info,0,17);
  memset(ack_ID_info,0,4);
  memset(sql,0,128);
  memset(status,0,16);

  ret = recv(fd, recv_ID_info, 17, 0);
  if(-1 == ret){
    perror("recv error!");
    return -1;
  }

  //打印接收到的信息
  for(i=0;i<17;i++){
  	if(recv_ID_info[i] == '/')
		recv_ID_info[i] = '\0';
  }
  memcpy(client_ID_info.id,recv_ID_info+1,4);
  memcpy(client_ID_info.name,recv_ID_info+5,4);
  memcpy(client_ID_info.passwd,recv_ID_info+9,8);
  
  //登录,验证输入的ID和passwd是否正确
  if(recv_ID_info[0] == '1'){
	sprintf(sql,"select * from hwy_id_sheet where id = '%s' and passwd = '%s';",
			client_ID_info.id,client_ID_info.passwd);
	sqlite3_get_table(db,sql,&azResult,&nrow,&ncolumn,&errmsg);
	if(nrow == 0){
		//没有匹配项，登录验证失败
		strcpy(status,"login failed!");
		send(fd,status,strlen(status),0);
		return -1;
	}
	else {
		//登录验证成功
  		memset(status,0,16);
		strcpy(status,"successfully!");
		send(fd,status,strlen(status),0);
		recv(fd,ack_ok,3,0);
	
		//在这里绑定client_fd---client_id
		for(i=0;i<CLIENT_MAX;i++){
		   if(id_to_fd[i].client_fd == fd){
			memcpy(id_to_fd[i].client_id,client_ID_info.id,4);
			break;
			}
		}
		
		//发送用户昵称
	   	strcpy(ack_ID_info,azResult[4]);
		send(fd,ack_ID_info,strlen(ack_ID_info),0);
		
		return 1;
	}
  }

  //注册，根据昵称和密码注册、记录帐号信息，并返回帐号
  else {
    int j = 100;
	char *sql1 = "select * from hwy_id_sheet;";
	sqlite3_get_table(db,sql1,&azResult,&nrow,&ncolumn,&errmsg);
	j = j+ nrow;
	memset(ack_ID_info,0,4);
	sprintf(ack_ID_info,"%d",j);//---itoa
	memcpy(client_ID_info.id,ack_ID_info,4);

	sprintf(sql,"insert into hwy_id_sheet values('%s','%s','%s'); ",client_ID_info.id,
				client_ID_info.name,client_ID_info.passwd);
	ret = sqlite3_exec(db, sql, NULL, NULL, &errmsg);
	if(ret == SQLITE_OK){
		printf("注册成功\n");
		memset(status,0,16);
		strcpy(status,"sign up");
		send(fd,status,strlen(status),0);
		recv(fd,ack_ok,3,0);
		
		//发送用户帐号信息
		send(fd,ack_ID_info,strlen(ack_ID_info),0);
		return 2;
	}
	else {
		printf("注册失败\n");
		memset(status,0,16);
		strcpy(status,"sign up error");
		send(fd,status,strlen(status),0);
		return -1;
	}
  }
}

//每接收一个客户端的连接，便创建一个线程
void * thread_func (void *arg)
{
  int fd = *(int *)arg;
  int ret;
  int i,j;
  hwy_send_msg_t hwy_C_SendMsg;
  printf("pthread = %d\n",fd);
   
  char recv_buffer[CHAT_STRUCT_SIZE];
  
  //验证登录/注册信息
  while(1){ 
  	ret = check_recv_id(fd);
	printf("check_recv_id = %d\n",ret);
	if(ret == 1){
		//成功登录
		printf("登录成功\n");
    for(i = 0;i< CLIENT_MAX;i++){
      if(id_to_fd[i].client_fd == fd){
        memset(recv_buffer,0,CHAT_STRUCT_SIZE);
        strcpy(recv_buffer,"////root//////////////////////////////");
            for(j = 0;j< CLIENT_MAX;j++){
              if(id_to_fd[j].client_fd != 0){
                memcpy(recv_buffer+strlen(recv_buffer),id_to_fd[j].client_id,4);
                memcpy(recv_buffer+strlen(recv_buffer),"/",1);
              }
            }
        memcpy(recv_buffer+strlen(recv_buffer),"在线",10);
        SendMsgToAll(recv_buffer);
        memset(recv_buffer,0,CHAT_STRUCT_SIZE);
      }
    }
		break;
	}
	else if(ret == 2){
		//注册成功,需要重新登录
		continue;
	}
	else {
		//验证失败,服务器不退出
		continue;
	}
  }

  //登录成功，处理正常聊天的信息--接收与转发
  while(1){
    memset(recv_buffer,0,CHAT_STRUCT_SIZE);
    ret = recv(fd, recv_buffer, CHAT_STRUCT_SIZE, 0);
    if(-1 == ret){
      perror("recv error!");
      return;
    }
    else if(ret>0){
      printf("接收到的内容为：%s\n",recv_buffer);
      if(memcmp(recv_buffer+POSITION_DESTID,"999",3)== 0)
        SendMsgToAll(recv_buffer);
      if(memcmp(recv_buffer+POSITION_CONTENT,"bq1",3)== 0){
        memcpy(recv_buffer+POSITION_CONTENT,"(≧∇≦)ﾉ",20);
      }
      if(memcmp(recv_buffer+POSITION_CONTENT,"bq2",3)== 0){
        memcpy(recv_buffer+POSITION_CONTENT,"o(TヘTo)",20);
      }
      if(memcmp(recv_buffer+POSITION_CONTENT,"bq3",3)== 0){
        memcpy(recv_buffer+POSITION_CONTENT,"ˋ( ° ▽、° ) (o(￣▽￣///(斩!!)",45);
      }
      if(memcmp(recv_buffer+POSITION_CONTENT,"bq4",3)== 0){
        memcpy(recv_buffer+POSITION_CONTENT,"(* ￣︿￣)",20);
      }
      if(memcmp(recv_buffer+POSITION_CONTENT,"bq5",3)== 0){
        memcpy(recv_buffer+POSITION_CONTENT,"Σ(っ °Д °;)っ",20);
      }
      if(memcmp(recv_buffer+POSITION_CONTENT,"bq6",3)== 0){
        memcpy(recv_buffer+POSITION_CONTENT,"₍₍ ◝(　ﾟ∀ ﾟ )◟⁾⁾不送",45);
      }
      if(memcmp(recv_buffer+POSITION_CONTENT,"bye",3)== 0){
          for(i = 0;i< CLIENT_MAX;i++){
            if(id_to_fd[i].client_fd == fd){
              id_to_fd[i].client_fd=0;
              memset(recv_buffer,0,CHAT_STRUCT_SIZE);
              strcpy(recv_buffer,"////root//////////////////////////////");
                  for(j = 0;j< CLIENT_MAX;j++){
                    if(id_to_fd[j].client_fd != 0){
                      memcpy(recv_buffer+strlen(recv_buffer),id_to_fd[j].client_id,4);
                      memcpy(recv_buffer+strlen(recv_buffer),"/",1);
                    }
                  }
              memcpy(recv_buffer+strlen(recv_buffer),"在线",10);
              SendMsgToAll(recv_buffer);
            memset(recv_buffer,0,CHAT_STRUCT_SIZE);
            
          }
        }
      }
      else{
		for(i = 0;i< CLIENT_MAX;i++){
			if(memcmp(id_to_fd[i].client_id,recv_buffer+POSITION_DESTID,\
				3)== 0){
        		SendMsgToSb(id_to_fd[i].client_fd,recv_buffer);
				break;
			}
		}
	  
	  }
    }
  
  }  
}

void service(int sock_fd)
{
  printf("服务器启动...\n");
  listen(sock_fd,CLIENT_MAX);
  while(1){
    struct sockaddr_in clientaddr;
    int len = sizeof(clientaddr);
    int client_sock = accept(sock_fd,(struct sockaddr*)&clientaddr,&len);
    if(client_sock== -1)
    {
      printf("accept failed...\n");
      continue;
    }
    printf("accept OK!\n");
    int i;
    for(i=0;i<CLIENT_MAX;i++){
      if(id_to_fd[i].client_fd == 0){
        id_to_fd[i].client_fd = client_sock;
        printf("client fd = %d\n",client_sock);
        //有客户端连接之后，启动线程为此客户端服务
        pthread_t tid;
        pthread_create(&tid, 0,thread_func,&client_sock);
        break;
      }
    }
    if(i == CLIENT_MAX){
      char * str = "对不起，聊天室已满人！\n";
      send(client_sock,str,sizeof(str),0);
      close(client_sock);
    }

  }
  
}


int main(int argc,char *argv[])
{
  int sock_fd = sock_init();
  hwyDataBase_init();
  service(sock_fd);
  return 0;
}

