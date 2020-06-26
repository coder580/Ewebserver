
#ifndef MAIN_H
#define MAIN_H

#define TIMEOUT_SECONDS 0
#define TIMEOUT_MICROSECONDS 100000
#define CHUNK_SIZE 2048
#define MAX_RECV 1024
#define PORT 8000
#define WEBROOT "./"
#define THREADS 20
struct conn_data
{
	char *client_message;
	int content_length;
	int header_len;
	int client_message_len;
	char *header;
	char *header_end;
	char *type;
	char *url;
	char *http_v;
	char *get_d;
	char *body;
	int body_size;
	int sock;
	char *cgi;
	char *boundry;
};

struct url_s
{
	char *path;
	int *(*urlfunc)(struct conn_data *info);
};

struct viewfile
{
	int size;
	char *data;
};

struct thrd_args
{
	struct listNode **head;
	int thrd_id;
};

#endif
