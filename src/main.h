
#ifndef MAIN_H
#define MAIN_H

#define PORT 8000
#define WEBROOT "./"
#define THREADS 200
struct conn_data
{
	char client_message[9000];
	char *type;
	char *url;
	char *http_v;
	char *get_d;
	char *body;
	int sock;
	char *cgi;
};

struct url_s
{
	char *path;
	int *(*urlfunc)(struct conn_data info);
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
