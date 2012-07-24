#ifndef NETWORKING_H
#define NETWORKING_H

#include <stdbool.h>
// port numbers are unused as of now
#define PUSH_PORT 5558
#define CHAT_PORT 5559

#define CHATMSG_PREFIX 	"chat"
#define DEBUGMSG_PREFIX "debug"
#define CTRLMSG_PREFIX 	"ctrl"
#define JOINMSG 	"join"
#define QUITMSG 	"quit"

bool init_zmq();
void cleanup_zmq();
bool zmq_python_up();

/* currently unused stuff
typedef enum Msg_dest Msg_dest;
enum Msg_dest
{
	MSG_DEST_NONE,
	MSG_DEST_PARTY,
	MSG_DEST_GLOBAL,
	MSG_DEST_PRIVATE
};

typedef enum Msg_type Msg_type;
enum Msg_type
{
	MSG_TYPE_CONTROL,
	MSG_TYPE_CHAT,
	MSG_TYPE_DEBUG
};
*/

typedef struct Message Message;
struct Message
{
	/*
	char *from;
	Msg_dest dest;
	Msg_type type;
	*/
	char *body;
	size_t body_len;
};

Message create_chat_msg(char *body, size_t len);
Message create_ctrl_msg(char *body, size_t len);
Message create_join_msg();
Message wrap_as_partymsg(Message msg);
bool send_msg(Message msg);
void del_msg(Message msg);
void send_join_msg();
void send_quit_msg();

bool zmq_sendmsg(char *buf, int buf_len);

char *try_recv_msg(void *sock);

#endif //NETWORKING_H
