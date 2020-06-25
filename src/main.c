#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <stdlib.h>
#include <arpa/inet.h>
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
#include <unistd.h>
#include <stdlib.h>
#include "linkedlist.h"
#include "main.h"
//so the main function can know which thread to unlock the mutex to
int currentThread=0;
int threadsBegun=0;
void *connection_handler(struct thrd_args *args);
char *codetotext(int code);
char *HttpResponse(char *type,int status);
int footer(int *sock);
void nextThread();
pthread_mutex_t locks[THREADS];
//open and memory map files.
struct viewfile getfile(char *name)
{
	int file;
	struct stat s;
	struct viewfile file_s;
	char *path=malloc(strlen(WEBROOT)+strlen(name));
	memcpy(path,WEBROOT,sizeof(WEBROOT));
	memcpy(path+strlen(WEBROOT),name,strlen(name));
	file=open(path,O_RDONLY);
	fstat(file,&s);
	file_s.size = s.st_size;
	file_s.data=mmap(0,file_s.size,PROT_READ,MAP_PRIVATE,file,0);
	close(file);
	free(path);
	return file_s;
}
//400 bad request
int badRequest(int sock)
{
	struct viewfile file=getfile("400.html");
	char *res=HttpResponse("text/html",400);
	write(sock,res,strlen(res));
	free(res);
	write(sock,file.data,file.size);
	close(sock);
	munmap(file.data,file.size);
	return 0;
}

//404 not found
int notFound(int sock)
{
	struct viewfile file=getfile("404.html");
	char *res=HttpResponse("text/html",404);
	write(sock,res,strlen(res));
	free(res);
	write(sock,file.data,file.size);
	close(sock);
	munmap(file.data,file.size);
	return 0;
}
//write functions for each url path you want to create."
//main page
int *main_page(struct conn_data *info)
{
	struct viewfile file=getfile("index.html");
	char *res = HttpResponse("text/html",200);
	write(info->sock,res,strlen(res));
	free(res);
	write(info->sock,file.data,file.size);
	close(info->sock);
	munmap(file.data,file.size);
	return 0;
}
//for webserver:port/numbers/!cgi/amount of numbers.
int *numbers(struct conn_data *info)
{
	char *rest=info->url;
	char *token;
	char *res=HttpResponse("text/plain",200);
	char num[500];
	int len;
	token=strtok_r(info->cgi,"/",&rest);
	write(info->sock,res,strlen(res));
	free(res);
	for(int i=0;i<atoi(token);i++)
	{
		len=sprintf(num,"%d\n",i);
		printf("%s\n",num);
		write(info->sock,num,len);
	}
	close(info->sock);
	return 0;
}
int main()
{
	//head of linked list.
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
		//thread typedef
  		pthread_t thread;
		//stuff to send to threads, pointer to head of linked list and their number
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

        struct sockaddr_in server, client;
        socket_desc=socket(AF_INET,SOCK_STREAM,0);
	//make the server reuse the address.
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
		printf("accept failed");
}
void nextThread()
{
	if(currentThread+1>=THREADS)
		currentThread=0;
	else
		currentThread++;
}
int getPost(struct conn_data *info)
{
	char *startofbound=strstr(info->header,"boundary=");
	if(startofbound==NULL)
	{
		printf("POST data invalid at start\n");
		return 1;
	}

	char *endofbound=strchr(startofbound,'\n');
	if(endofbound==NULL)
	{
		endofbound=strchr(startofbound,'\r');
		if(endofbound==NULL)
		{
			printf("POST data invalid at end\n");
			return 1;
		}
	}

	char *bound=malloc((endofbound-startofbound)+1);
	memcpy(bound,startofbound+9,endofbound-startofbound);
	bound[(endofbound-startofbound)+1]=0;
	char *body=malloc(info->body_size);
	memcpy(body,info->body,info->body_size);
	char *token=strtok(body,bound);
	while(token!=NULL)
	{
		puts(token);
		token=strtok(token,bound);
	}
	free(bound);
	free(body);
	return 0;
}


void *connection_handler(struct thrd_args *args)
{
	int mythread=args->thrd_id;
	struct listNode **head=args->head;
	int *socket_desc;
	char chunk[CHUNK_SIZE];
	int size_recv;
	char *content_length;
//	char *crtonl;
	char *header=NULL;
	char *cl;
	int cl_chars=0;
        struct conn_data *info=malloc(sizeof(struct conn_data));
	char *rest=header;
	info->client_message=malloc(MAX_RECV);
	struct timeval timeout;
	timeout.tv_sec=TIMEOUT_SECONDS;
	timeout.tv_usec=TIMEOUT_MICROSECONDS;
	struct url_s urls[] = {
		{"/",main_page},
		{"/numbers",numbers}
	};
	free(args);
	while(1)
	{
		if(currentThread==mythread && *head!=NULL && threadsBegun==1)
			
		{
			socket_desc=(*head)->data;
			delFirst(&(*head));
			nextThread();
			printf("thread: %d\n",mythread);
			info->sock=*(int *)socket_desc;
			if(setsockopt(info->sock,SOL_SOCKET,SO_RCVTIMEO,(char *)&timeout,sizeof(timeout))<0)
				printf("setsockopt timeout failed.\n");
			memset(info->client_message,0,MAX_RECV);
			info->client_message_len=0;
			info->header_end=NULL;
			info->header=NULL;
			info->client_message_len=0;
			info->header_len=0;
			info->body_size=0;
			info->get_d=NULL;
			while (1)
			{
				memset(chunk,0,CHUNK_SIZE);
				if ((size_recv=recv(info->sock,chunk,CHUNK_SIZE,0)) <1)
					break;
				else
				{
					info->client_message_len+=size_recv;
					if (info->client_message_len>=MAX_RECV)
					{
						badRequest(info->sock);
						goto done;
					}
					memcpy(info->client_message+(info->client_message_len-size_recv),chunk,size_recv);
					if ((info->header_end=strstr(info->client_message,"\r\n\r\n")+1))
					{
						info->header_len=(((long unsigned)info->header_end-(long unsigned)info->client_message));
						info->header=info->client_message;
						//put a null terminator at the end of the header of the message
						info->header_end=0;
						if( !!(cl=strstr(info->header,"Content-Length: ") ))
						{
							cl+=strlen("Content-Length: ");
							if((cl_chars= (strstr(cl,"\n")-cl))<30)
							{
								content_length=malloc(cl_chars);
								memcpy(content_length,cl,cl_chars);
								info->content_length=atoi(content_length);
								free(content_length);
								if(info->header_len+info->content_length>=info->client_message_len)
									break;
							}
							else
							{
								badRequest(info->sock);
								goto done;
							}
						}
						else
							break;
					}
				}
			}
			//prevent buffer overflow
			info->client_message[info->client_message_len+1]=0;
			if(info->header==NULL)
			{
				printf("the client didnt send a header\n");
				badRequest(info->sock);
				goto done;
			}
			if(info->client_message_len<=0)
			{
       				printf("the client didnt send any data.\n");
				badRequest(info->sock);
				goto done;
			}
			
//			while((crtonl=strchr(info->client_message,'\r')))
//			{
//				memcpy(crtonl,"\n",1);
//				crtonl++;
//			}
			header=malloc(info->header_len);
			memcpy(header,info->header,info->header_len);
       			info->type=strtok_r(header," ",&rest);
       			info->url=strtok_r(NULL," ",&rest);
       			info->http_v=strtok_r(NULL," ",&rest);
			info->get_d=strchr(info->url,'?');
			info->body=info->header_end+1;
			info->body_size=info->client_message_len-info->header_len;
			if (!info->url || !info->type || !info->http_v)
			{
				badRequest(info->sock);
				goto done;
			}
			if(info->get_d)
			{
				memset(info->get_d,0,1);
				info->get_d++;
			}
			info->cgi=strstr(info->url,"!cgi/");
			if(info->cgi)
			{
				memset(info->cgi,0,5);
				info->cgi+=5;
			}
			//remove the trailing slash.
			if(strcmp(info->url+strlen(info->url)-1,"/")==0 && strlen(info->url)>1)
				memset(info->url+strlen(info->url)-1,0,1);
			//iterate over urls in array of structs.
			for (long unsigned int urll=0;urll<sizeof(urls)/sizeof(urls[0]);urll++)
			{
				//check if the url defined in the current struct is the same as the url path.
				if (strcmp(urls[urll].path,info->url)==0)
				{
					//the url matches one in the array of structs, run the function
					urls[urll].urlfunc(info);
					goto done;
				}
			}
			notFound(info->sock);
			done:
			free(socket_desc);
			if (header!=NULL)
				free(header);



		}
		pthread_mutex_lock(&locks[mythread]);
	}
}
//http response

char *HttpResponse(char *type,int status)
{
	//get the response text from the number
	char *code=codetotext(status);
	//get the size of the mimetype string
	int mimeSize=strlen(type);
	//get the string size of the response text
	int codeSize=strlen(code);
	char *message_template=
        "HTTP/1.1 %s\n"
        "Server: Ewebserver/1.0\n"
        "Content-Type: %s\r\n\r\n";
	//get the size of the message template
	int messageTemplateSize=strlen(message_template);
	char *message;
	//malloc just enough memory in the heap for the response message
	message=malloc(messageTemplateSize+codeSize+mimeSize);
	//merge everything into the malloced message string
	sprintf(message,message_template,code,type);
	//return the malloced message string
	return message;
}
int advertisement(int *sock)
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
