
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdarg.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <pcre.h>
#include "linkedlist.h"
#include "main.h"
int currentThread=0;
int threadsBegun=0;
void *connection_handler(struct thrd_args *args);
char *codetotext(int code);
char *HttpResponse(char *type,int status);
int footer(int *sock);
void nextThread();

pthread_mutex_t locks[THREADS];
struct viewfile *getfile(char *name)
{
	int file;
	struct stat s;
	struct viewfile *file_s=malloc(sizeof(struct viewfile));
	char *path=malloc(strlen(WEBROOT)+strlen(name));
	memcpy(path,WEBROOT,sizeof(WEBROOT));
	memcpy(path+strlen(WEBROOT),(char *)name,strlen(name));
	file=open(path,O_RDONLY);
	fstat(file,&s);
	file_s->size = s.st_size;
	file_s->data=mmap(0,file_s->size,PROT_READ,MAP_PRIVATE,file,0);
	printf("%d\n",file_s->size);
	close(file);
	free(path);
	return file_s;
}

//url functions
//main page
int *main_page(struct conn_data info)
{
	struct viewfile *file=getfile("index.html");
	char *res = HttpResponse("text/html",200);
	write(info.sock,res,strlen(res));
	free(res);
	write(info.sock,file->data,file->size);
	close(info.sock);
	munmap(file->data,file->size);
	free(file);
	return 0;
}
//404 not found
int notFound(int sock)
{
	struct viewfile *file=getfile("404.html");
	char *res=HttpResponse("text/html",404);
	write(sock,res,strlen(res));
	free(res);
	write(sock,file->data,file->size);
	munmap(file->data,file->size);
	free(file);
	return 0;
}
//for webserver:port/numbers/!cgi/amount of numbers
int *numbers(struct conn_data info)
{
	char *rest=info.url;
	char *token;
	char *res=HttpResponse("text/plain",200);
	char num[500];
	int len;
	token=strtok_r(info.cgi,"/",&rest);
	write(info.sock,res,strlen(res));
	free(res);
	for(int i=0;i<atoi(token);i++)
	{
		len=sprintf(num,"%d\n",i);
		printf("%s\n",num);
		write(info.sock,num,strlen(num));
	}
	close(info.sock);
	return 0;
}
int *src(struct conn_data info)
{
	struct viewfile *file=getfile("ewebserver.tar.gz");
	char *res=HttpResponse("archive/gz",200);
	write(info.sock,res,strlen(res));
	write(info.sock,file->data,file->size);
	free(res);
	munmap(file->data,file->size);
	free(file);
	return 0;
}

int main()
{
	//head of linked list
	struct listNode *head=NULL;
	//initialize all the locks
	for(int lp;lp<THREADS;lp++)
	{
		if(pthread_mutex_init(&locks[lp], NULL)!=0)
		{
			printf("pthread mutex init failed\n");
			return 1;
		}
	}
	//start the thread pool
	for(int i=0;i<THREADS;i++)
	{
  		pthread_t thread;
		struct thrd_args *args=malloc(sizeof(struct thrd_args));
		args->head=&head;
		args->thrd_id=i;
		if (pthread_create(&thread,NULL,(void *)connection_handler,(void *)args)<0)
		{
			printf("could not create thread\n");
			return 1;
		}
	}
	threadsBegun=1;

	int socket_desc, new_socket,c, *new_sock;
	char *notresponding="the server is not responding at the moment.";

        struct sockaddr_in server, client;
        socket_desc=socket(AF_INET,SOCK_STREAM,0);
	if(setsockopt(socket_desc,SOL_SOCKET,SO_REUSEADDR,&(int){1},sizeof(int))<0)
		printf("setsockopt failed\n");
        if (socket_desc==-1)
        {
                printf("could not create socket\n");

        }
        server.sin_family=AF_INET;
        server.sin_addr.s_addr=INADDR_ANY;
        server.sin_port=htons(PORT);
        if (bind(socket_desc,(struct sockaddr *)&server, sizeof(server)) < 0)        {
                printf("failed to bind\n");
                return 1;
        }

        printf("succesfully bound to port.\n");
        listen(socket_desc,3);
        printf("listening\n");
        c=sizeof(struct sockaddr_in);
	wait:
        while( (new_socket=accept(socket_desc,(struct sockaddr *)&client,(socklen_t*)&c))>0 )
        {
//              client_ip=inet_ntoa(client.sin_addr);
                new_sock = (int *)malloc(sizeof(new_socket));
                *new_sock = new_socket;
		//add the socket file descriptor to the linked list so a thread can process it
		appendToList(&head,new_sock);
		//unlock the mutex of the current thread so it can process the 
		pthread_mutex_unlock(&locks[currentThread]);
	}
        if (new_socket<0)
        {
		printf("accept failed");
		//goto wait;
	}
        goto wait;
}
void nextThread()
{
	if(currentThread+1>=THREADS)
		currentThread=0;
	else
		currentThread++;
}
void *connection_handler(struct thrd_args *args)
{
	int mythread=args->thrd_id;
	struct listNode **head=args->head;
	int *socket_desc;
        char *token;
	char *data;
	char *trail;
        struct conn_data info;
	char *rest=info.client_message;
	struct url_s urls[] = {
		{"/",main_page},
		{"/numbers",numbers},
		{"/src.tar.gz",src}
	};
	free(args);
	while(1)
	{
		if(currentThread==mythread && *head!=NULL && threadsBegun==1)
		{
				socket_desc=(*head)->data;
				delFirst(&(*head));
				nextThread();
				printf("%d\n",mythread);
				info.sock=*(int *)socket_desc;
				if (recv(info.sock,info.client_message,9000,0)<4)
				{
        				printf("the client didnt send any data, or they sent too much\n");
					goto notFound;
				}

				token = strtok_r(info.client_message,"\n",&rest);
        			info.type=strtok_r(token," ",&rest);
        			info.url=strtok_r(NULL," ",&rest);
        			info.http_v=strtok_r(NULL," ",&rest);
				info.get_d=strchr(info.url,'?');
				if(info.get_d)
				{
					memset(info.get_d,0,1);
					info.get_d++;
				}
				info.cgi=strstr(info.url,"!cgi/");
				if(info.cgi)
				{
					memset(info.cgi,0,5);
					info.cgi+=5;
					printf("%s\n",info.cgi);
				}
				if(strcmp(info.url+strlen(info.url)-1,"/")==0 && strlen(info.url)>1)
					memset(info.url+strlen(info.url)-1,0,1);
				printf("%s\n",info.url);
				for (long unsigned int urll=0;urll<sizeof(urls)/sizeof(urls[0]);urll++)
				{
					if (strcmp(urls[urll].path,info.url)==0)
					{
						urls[urll].urlfunc(info);
						goto conn_close;
					}
				}
			notFound:
				notFound(info.sock);
			conn_close:
				close(info.sock);
				free(socket_desc);

			}	
		pthread_mutex_lock(&locks[mythread]);

	}
}
//http response

char *HttpResponse(char *type,int status)
{
	char *code=codetotext(status);
	int mimeSize=strlen(type);
	int codeSize=strlen(code);
	char *message_template=
        "HTTP/1.1 %s\n"
        "Server: Ewebserver/1.0\n"
        "Content-Type: %s\r\n\r\n";
	int messageTemplateSize=strlen(message_template);
	char *message;
	int changed;
	message=malloc(messageTemplateSize+codeSize+mimeSize+100);
	changed=sprintf(message,message_template,code,type);
	return message;
}
int footer(int *sock)
{
	char msg[]="<div style='align: center;'>powered by Ewebserver</div>\n";
	write(*sock,msg,strlen(msg));
	return 0;
}

//takes a code such as 200 or 404 and turns it into 200 OK or 404 Not Found
char *codetotext(int code)
{
	switch(code)
	{
		case 100:
		return "100 Continue";
		break;
		case 101:
		return "101 Switching Protocols";
		break;
		case 103:
		return "103 Early Hints";
		break;
		case 200:
		return "200 OK";
		break;
		case 201:
		return "201 Created";
		break;
		case 202:
		return "202 Accepted";
		break;
		case 203:
		return "203 Non-Authoritative Information";
		break;
		case 204:
		return "204 No Content";
		break;
		case 205:
		return "205 Reset Content";
		break;
		case 206:
		return "206 Partial Content";
		break;
		case 207:
		return "207 Multi-Status";
		break;
		case 208:
		return "208 Already Reported";
		break;
		case 226:
		return "226 IM Used";
		break;
		case 300:
		return "300 Multiple Choice";
		break;
		case 301:
		return "301 Moved Permanently";
		break;
		case 302:
		return "302 Found";
		break;
		case 303:
		return "303 See Other";
		break;
		case 304:
		return "304 Not Modified";
		break;
		case 307:
		return "307 Temporary Redirect";
		break;
		case 308:
		return "308 Permanent Redirect";
		break;
		case 400:
		return "400 Bad Request";
		break;
		case 401:
		return "401 Unauthorized";
		break;
		case 403:
		return "403 Forbidden";
		break;
		case 404:
		return "404 Not Found";
		break;
		case 405:
		return "405 Method Not Allowed";
		break;
		case 406:
		return "406 Not Acceptable";
		break;
		case 407:
		return "407 Proxy Authentication Required";
		break;
		case 408:
		return "408 Request Timeout";
		break;
		case 409:
		return "409 Conflict";
		break;
		case 410:
		return "410 Gone";
		break;
		case 411:
		return "411 Length Required";
		break;
		case 412:
		return "412 Precondition Failed";
		break;
		case 413:
		return "413 Payload Too Large";
		break;
		case 414:
		return "414 URI Too Long";
		break;
		case 415:
		return "415 Unsupported Media Type";
		break;
		case 416:
		return "416 Range Not Satisfiable";
		break;
		case 417:
		return "400 Expectation Failed";
		break;
		case 418:
		return "418 I'm a teapot";
		break;
		case 421:
		return "421 Misdirected Request";
		break;
		case 422:
		return "422 Unprocessable Entity";
		break;
		case 423:
		return "423 Locked";
		break;
		case 424:
		return "424 Failed Dependency";
		break;
		case 425:
		return "425 Too Early";
		break;
		case 426:
		return "426 Upgrade Required";
		break;
		case 428:
		return "428 Precondition Required";
		break;
		case 429:
		return "429 Too Many Requests";
		break;
		case 431:
		return "431 Request Header Fields Too Large";
		break;
		case 451:
		return "451 Unavailible For Legal Reasons";
		break;
		case 500:
		return "500 Internal Server Error";
		break;
		case 501:
		return "501 Not Implemented";
		break;
		case 502:
		return "502 Bad Gateway";
		break;
		case 510:
		return "510 Not Extended";
		break;
		case 511:
		return "511 Network Authentication Required";
		break;
		default:
		return "500 Internal Server Error";
		break;
	}
	
}
