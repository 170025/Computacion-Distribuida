#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <pthread.h>

#define BUFFER 4096

void errexit(const char *msg){
    perror(msg);
    exit(1);
}

int connectsock(const char *host, const char *service, const char *transport){
    struct hostent *hp;
    struct servent *sp;
    struct protoent *pp;
    struct sockaddr_in sin;
    int sock, type, protocol;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;

    if ((hp = gethostbyname(host)))
        memcpy(&sin.sin_addr, hp->h_addr, hp->h_length);
    else
        errexit("gethostbyname");

    if ((sp = getservbyname(service, transport)) == NULL)
        sin.sin_port = htons((u_short)atoi(service));
    else
        sin.sin_port = sp->s_port;

    if ((pp = getprotobyname(transport)) == NULL)
        protocol = 0;
    else
        protocol = pp->p_proto;

    if (strcmp(transport, "udp") == 0)
        type = SOCK_DGRAM;
    else
        type = SOCK_STREAM;

    sock = socket(PF_INET, type, protocol);
    if (sock < 0)
        errexit("socket");

    if (connect(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
        errexit("connect");

    return sock;
}

int connectTCP(const char *host, int port){
    int sockfd;
    struct sockaddr_in servaddr;
    struct hostent *hp;

    if((hp = gethostbyname(host)) == NULL)
        errexit("gethostbyname");

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd < 0)
        errexit("socket");

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    memcpy(&servaddr.sin_addr, hp->h_addr, hp->h_length);
    servaddr.sin_port = htons(port);

    if(connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        errexit("connectTCP");

    return sockfd;
}

void read_response(int sock){
    char buffer[BUFFER];
    memset(buffer,0,sizeof(buffer));
    int n = read(sock, buffer, sizeof(buffer)-1);
    if(n > 0) printf("%s", buffer);
}

void send_cmd(int sock, const char *cmd){
    write(sock, cmd, strlen(cmd));
    write(sock, "\r\n", 2);
}

int open_pasv(int control_socket, char *ip_out){
    char buffer[BUFFER];
    memset(buffer,0,sizeof(buffer));
    read(control_socket, buffer, sizeof(buffer)-1);
    printf("%s", buffer);
    int h1,h2,h3,h4,p1,p2;
    char *p = strchr(buffer,'(');
    if(!p) return -1;
    sscanf(p+1,"%d,%d,%d,%d,%d,%d",&h1,&h2,&h3,&h4,&p1,&p2);
    sprintf(ip_out,"%d.%d.%d.%d",h1,h2,h3,h4);
    return p1*256 + p2;
}

int setup_port(int control_socket){
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    getsockname(sockfd, (struct sockaddr *)&addr, &len);
    int port = ntohs(addr.sin_port);
    listen(sockfd, 1);

    char ip[64];
    gethostname(ip, sizeof(ip));
    struct hostent *h = gethostbyname(ip);
    unsigned char *b = (unsigned char*)h->h_addr;

    char cmd[128];
    sprintf(cmd,"PORT %d,%d,%d,%d,%d,%d",b[0],b[1],b[2],b[3],port/256,port%256);
    send_cmd(control_socket,cmd);
    read_response(control_socket);
    return sockfd;
}

void *thread_retr(void *arg){
    int *params = (int*)arg;
    int control = params[0];
    char filename[128];
    strcpy(filename,(char*)(params+1));
    char ip[64];
    send_cmd(control,"PASV");
    int port = open_pasv(control,ip);
    int data = connectTCP(ip,port);
    char cmd[128];
    snprintf(cmd,sizeof(cmd),"RETR %s",filename);
    send_cmd(control,cmd);
    read_response(control);
    int file = open(filename,O_WRONLY|O_CREAT|O_TRUNC,0666);
    char buff[BUFFER];
    int n;
    while((n = read(data,buff,sizeof(buff))) > 0)
        write(file,buff,n);
    close(file);
    close(data);
    read_response(control);
    return NULL;
}

void *thread_stor(void *arg){
    int *params = (int*)arg;
    int control = params[0];
    char filename[128];
    strcpy(filename,(char*)(params+1));
    char ip[64];
    send_cmd(control,"PASV");
    int port = open_pasv(control,ip);
    int data = connectTCP(ip,port);
    int file = open(filename,O_RDONLY);
    if(file<0) return NULL;
    char cmd[128];
    snprintf(cmd,sizeof(cmd),"STOR %s",filename);
    send_cmd(control,cmd);
    read_response(control);
    char buff[BUFFER];
    int n;
    while((n = read(file,buff,sizeof(buff))) > 0)
        write(data,buff,n);
    close(file);
    close(data);
    read_response(control);
    return NULL;
}

void do_list(int control_socket){
    char ip[64];
    send_cmd(control_socket,"PASV");
    int data_port = open_pasv(control_socket, ip);
    int datasock = connectTCP(ip, data_port);
    send_cmd(control_socket,"LIST");
    read_response(control_socket);
    char buff[BUFFER];
    int n;
    while((n = read(datasock,buff,sizeof(buff))) > 0)
        write(1,buff,n);
    close(datasock);
    read_response(control_socket);
}

int main(int argc, char *argv[]){
    if(argc != 3){
        printf("Uso: %s <IP> <PORT>\n",argv[0]);
        return 1;
    }

    int control_socket = connectTCP(argv[1], atoi(argv[2]));
    read_response(control_socket);

    char line[256];

    while(1){
        printf("ftp> ");
        fflush(stdout);

        if(!fgets(line,sizeof(line),stdin)) break;
        line[strcspn(line,"\n")] = 0;

        if(strncasecmp(line,"QUIT",4)==0){
            send_cmd(control_socket,"QUIT");
            read_response(control_socket);
            break;
        }
        else if(strncasecmp(line,"USER",4)==0){
            send_cmd(control_socket,line);
            read_response(control_socket);
        }
        else if(strncasecmp(line,"PASS",4)==0){
            send_cmd(control_socket,line);
            read_response(control_socket);
        }
        else if(strcasecmp(line,"PWD")==0){
            send_cmd(control_socket,"PWD");
            read_response(control_socket);
        }
        else if(strncasecmp(line,"CWD",3)==0){
            send_cmd(control_socket,line);
            read_response(control_socket);
        }
        else if(strncasecmp(line,"MKD",3)==0){
            send_cmd(control_socket,line);
            read_response(control_socket);
        }
        else if(strncasecmp(line,"DELE",4)==0){
            send_cmd(control_socket,line);
            read_response(control_socket);
        }
        else if(strcasecmp(line,"LIST")==0){
            do_list(control_socket);
        }
        else if(strncasecmp(line,"RETR ",5)==0){
            pthread_t th;
            int *args = malloc(256);
            args[0] = control_socket;
            strcpy((char*)(args+1), line+5);
            pthread_create(&th,NULL,thread_retr,args);
            pthread_detach(th);
        }
        else if(strncasecmp(line,"STOR ",5)==0){
            pthread_t th;
            int *args = malloc(256);
            args[0] = control_socket;
            strcpy((char*)(args+1), line+5);
            pthread_create(&th,NULL,thread_stor,args);
            pthread_detach(th);
        }
        else{
            printf("Comando no reconocido.\n");
        }
    }

    close(control_socket);
    return 0;
}
