#include <stdio.h> 
#include <unistd.h>
#include <string.h>
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <netdb.h>        
#include <stdlib.h>
#include <pthread.h>
#include <sys/time.h>
#include <errno.h>
#include <syslog.h>

#define PORT 4007

/****查看与添加对讲机的静态路由****/
int run_prepared()
{
	FILE *fp1 = NULL;
	FILE *fp2 = NULL;
	FILE *fp3 = NULL;
	char buf2[20],buf1[20];
	char command[100];
	static char file_buf1[560],file_buf2[560];
	char *same_file1=NULL;
	char *same_file2=NULL;
	
	bzero(&buf1,sizeof(buf1));
	bzero(&buf2,sizeof(buf2));
	bzero(&command,sizeof(command));
	bzero(&file_buf1,sizeof(file_buf1));
	bzero(&file_buf2,sizeof(file_buf2));

	snprintf(buf1 , sizeof(buf1) , "192.168.6.2");
//	printf("buf1=%s\n",buf1);
	
	/*使用"ifconfig"以确认对讲机的usb是否连接好*/
	if((fp1 = popen("ifconfig", "r")) == NULL)
		printf("failed to use 'ifconfig'\n");
        fread(file_buf1, sizeof(file_buf1), 5, fp1); 
//	printf("file_buf1=%s\n",file_buf1);
	same_file2=strstr(file_buf1,buf1);
	if(same_file2 == NULL)
	{
		printf("Without radio route.\n");
		return (0);
	}

	/*增加对讲机的Network ID*/
	snprintf(buf2 , sizeof(buf2) , "12.0.0.0");
//	printf("buf2=%s\n",buf2);
	if(NULL == (fp2 = popen("route -n", "r")))
		printf("failed to use 'route -n'\n");
        fread(file_buf2, sizeof(file_buf2),8, fp2);
//	printf("file_buf2=%s\n",file_buf2);
	same_file1=strstr(file_buf2,buf2);
	if(same_file1)
	{
//		printf("same=%s\n",same_file1);
		printf("Netmask was already prepared.\n");
		pclose(fp2);
		return (0);
	}
	else
	{
//		printf("command:'sudo route add -net 12.0.0.0 netmask 255.0.0.0 gw 192.168.6.1'\n");
		snprintf(command , sizeof(command) , "sudo route add -net 12.0.0.0 netmask 255.0.0.0 gw 192.168.6.1");
		if(NULL == (fp3 = popen(command , "r")))
			printf("fp3 failed\n");
	}

	pclose(fp1);
	pclose(fp2);
	pclose(fp3);
	printf("Netmask was already prepared.\n");
	return 0;
}

/****TMS app 发送端函数****/
int client_thread()
{
	int cfd,len;
	struct sockaddr_in server;
	struct sockaddr_in client;
	if ((cfd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
	{  
	    printf("socket() error\n"); 
	    exit(1); 
	} 
	len=sizeof(struct sockaddr);

	bzero(&server,sizeof(server));
	server.sin_family = AF_INET; 
	server.sin_port = htons(PORT); 
	server.sin_addr.s_addr = inet_addr("12.0.3.235");
//	server.sin_addr.s_addr = inet_addr("12.0.3.233");
 
	while (1)
	{
		char tms[280];
		char mas[137];
		int i,j; 
	
		bzero(tms,sizeof(tms));
		bzero(mas,sizeof(mas));
		
		for(i=0;i<137;i++)
		{
			scanf("%[^\n]",&mas[i]);
			if(mas[i]=='\0')break;
		}
		if(i<126)
		{
			tms[0]=0x00;
			tms[1]=2*i+0x04;
		}
		else
		{
			tms[0]=2*i+0x04-0xff;
			tms[1]=0xff;
		}
	
		tms[2]=0xe0;
		tms[3]=0x00;
		tms[4]=0x81;
		tms[5]=0x04;

		/****对讲机支持中文，一个字符占两个字节****/
		for(j=0;mas[j]!='\0';j++)
		{
			*(tms+2*j+6)=*(mas+j);
			*(tms+2*j+7)=0x00;
		}	

		sendto(cfd, tms,2*i+6,0,(struct sockaddr *)&server,sizeof(struct sockaddr));

		/*接收确认已发送成功的信号*/
		char send_ack[140];
		bzero(send_ack,sizeof(send_ack));
		recvfrom(cfd,send_ack,140,0,(struct sockaddr *)&client,(socklen_t *)&len);
		getchar();
	}
	close(cfd); 
  	return 0;
}

/****TMS app 接收端函数****/
int server_thread()
{
	int sfd,l;
	int len;
	struct sockaddr_in server;
	struct sockaddr_in client;

	if ((sfd=socket(AF_INET, SOCK_DGRAM, 0))==-1)
	{  
	    printf("socket() error\n"); 
	    exit(1); 
	} 
	len=sizeof(struct sockaddr_in);

	bzero(&server,sizeof(server));
	server.sin_family = AF_INET; 
	server.sin_port = htons(PORT); 
  	server.sin_addr.s_addr = htonl (INADDR_ANY); 
	if(l=bind(sfd, (struct sockaddr *)&server,sizeof(struct sockaddr)))
    	{ 
        	perror("Bind error.");
        	exit(1); 
    	}    

	while(1)
	{	
		char recv[280];
		int num,k;
		bzero(recv,sizeof(recv));
		
		/*接收对讲机发来的信息*/
		num=recvfrom(sfd,recv,280,0,(struct sockaddr *)&client,&len);
//		printf("num=%d\n",num);
		/*确认已接收信息成功*/
		char recv_ack[6];
		bzero(recv_ack,sizeof(recv_ack));

		recv_ack[0]=0x00;
		recv_ack[1]=0x04;
		recv_ack[2]=0x9f;
		recv_ack[3]=0x00;
		recv_ack[4]=recv[4] & 0x3f;//和00111111相与，以确定Sequence Number
		recv_ack[5]=recv[4] & 0xc0;//和11000000相与，以确定Sequence Number是否溢出

		if(num>6)
		{
			printf("Received:");	
			for(k=9;k<280;k++)
			{
				printf("%c",recv[k]);
			}	
			printf("\n");	
			sendto(sfd, recv_ack,6,0,(struct sockaddr *)&client,sizeof(struct sockaddr));
		}
	}
	close(sfd); 
    	return (0);
}

int main()
{
	int send_file,recv_flie;
	pthread_t thread_1,thread_2;
	void *tret;

	run_prepared();

	/****创建两个线程，一个发送，一个接收****/
	send_file=pthread_create(&thread_1,NULL,(void *)client_thread,NULL);
	recv_flie=pthread_create(&thread_2,NULL,(void *)server_thread,NULL);

	/****结束线程****/
	send_file=pthread_join(thread_1,NULL);
	if(send_file!=0)
		printf("Can't join thread 1.");
	recv_flie=pthread_join(thread_2,NULL);
	if(recv_flie!=0)
		printf("Can't join thread 2.");

	return (0);	
}
