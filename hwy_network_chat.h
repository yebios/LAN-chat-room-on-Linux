#ifndef __HWY_NETWORK_CHAT_H__
#define __HWY_NETWORK_CHAT_H__

//聊天室中能注册登陆的最大人员数量
#define  PEOPLE_NUM_MAX   10
//聊天信息（发送结构体）长度
#define  CHAT_STRUCT_SIZE (POSITION_CONTENT+128)
//客户端最大连接数量
#define  CLIENT_MAX       3

#define  POSITION_SELFID  0
#define  POSITION_NAME    (4+POSITION_SELFID)
#define  POSITION_DESTID  (4+POSITION_NAME)
#define  POSITION_TIME    (4+POSITION_DESTID)
#define  POSITION_CONTENT (26+POSITION_TIME)

//聊天室中人员身份信息
typedef struct 
{
  char id[4];	 //登陆帐号
  char name[4];	 //昵称
  char passwd[8];//登陆密码
}hwy_people_t;

//聊天消息之发送的格式
typedef struct
{
  char self_id[4];	//自身ID
  char name[4];		//自身昵称
  char dest_id[4];	//目标ID
  char time[26];	//发送时间
  char content[128];	//发送内容
}hwy_send_msg_t;

//聊天消息之接收的格式
typedef struct
{
  char who[4];		//发送者
  char time[26];	//发送时间
  char content[128];	//发送内容
}hwy_recv_msg_t;



#endif
