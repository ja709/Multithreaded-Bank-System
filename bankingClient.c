#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <pthread.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define ERROR -1
#define SIZE 1024

void *write_thread(void *port);
void *read_thread(void *port);

int main (int args, char *argv[])
{

	struct sockaddr_in server;
	int sockfd;
	char input[1024];
	char num[20];
	char output[1024];
	int len, numlen;
	pthread_t reading, writing;
	struct hostent *server_host;
	
	if(args != 3){
		printf("please enter host name and port number\n");
		return -1;
	}
	
	if ((sockfd = socket(AF_INET,SOCK_STREAM, 0)) == ERROR)
	{
		perror("socket");
		exit(-1);
	}
	
	server_host = gethostbyname(argv[1]);
	if (server_host == NULL)
	{
		fprintf (stderr, "ERROR, no such host\n");
		exit(0);
	}
	
	bzero(&server, sizeof (struct sockaddr_in));
	server.sin_family = AF_INET;
	bcopy((char *)server_host->h_addr, (char *)&server.sin_addr.s_addr, server_host->h_length);
	server.sin_port = htons(atoi(argv[2]));

	while (1)
	{
		if(connect(sockfd, (struct sockaddr *) &server, sizeof (struct sockaddr)) < 0){
			printf("connection failed\nreconnect in every three seconds\n");
			
			printf("Reconnecting.\n\n");
			printf(".\n\n");
			sleep(1);

			printf("..\n\n");
			sleep(1);

			printf("...\n\n");
			sleep(1);
			continue;		
		} else {
			break;
		}
		break;	
	}

	if (pthread_create(&writing, NULL, write_thread, (void *) &sockfd) != 0)
	{
		printf ("Creat error\n");
		exit(0);
	}
	
	if (pthread_create(&reading, NULL, read_thread, (void *) &sockfd) != 0)
	{
		printf ("Creat error\n");
		exit(0);
	}
	
	pthread_join(reading, NULL);
	//pthread_join(writing, NULL);
	close(sockfd);
	printf("\ndisconnected from server.\n");
	return 0;	
}

// write to server
void *write_thread(void *port)
{
	int sockfd = *(int *)port;
	char input[1024];
	char num[20];
	char *first = NULL;
	char *second = NULL;
	char *whole = NULL;
	int len, numlen;
	int i = 0;
	
	while (1)
	{
		printf("\n*******************************************\n");
		printf("welcome to hello bank! How may I help you?\n\n");
		printf("--------------------------------------------\n");
		printf("create <accountname (char) >\nserve <accountname (char) >\ndeposit <amount (double)>\nwithdraw <amount (double) >\nquery\nend\nquit\n");	
		printf("--------------------------------------------\n");
		printf("\nEnter command and message: \n");
		
	

		fgets(input, SIZE, stdin);
		
		first = strtok(input, " \n");
		if (first == NULL)
		{
			printf ("Wrong Query!! 1\n");
			continue;
		}
	
		second = strtok(NULL, "\n");
		if (second == NULL)
		{
			if (strcmp(first, "quit") && strcmp(first, "end")&& strcmp(first, "query"))
			{
				printf ("Wrong Query!!\n");
				continue;
			}
			else
			{
				printf ("\n\nYou have requested: %s", input);
				len = strlen(input);
				sprintf(num, "%d", len);
				numlen = strlen(num);
				send(sockfd, num, numlen, 0);
				
				send(sockfd, input, len, 0);
			}
		}
		else
		{
			
			if (strcmp(first,"create") &&  strcmp(first,"serve") &&  strcmp(first,"deposit") && strcmp(first,"withdraw"))
			{
				printf ("Wrong Query!!\n");
				continue;
			}
			
			if (strlen(second) < 256)
			{
				whole = (char *) malloc (strlen(first) + strlen(second) + 2);
				strcpy(whole, first);
				strcat(whole, " ");
				strcat(whole, second);
				printf ("\n\nYou have requested2: %s", whole);
				len = strlen(whole);
				sprintf(num, "%d", len);
				numlen = strlen(num);
				send(sockfd, num, numlen, 0);
				
				send(sockfd, whole, len, 0);
				free(whole);
			}
			else
			{
				whole = (char *) malloc (strlen(first) + 257);
				strcpy(whole, first);
				strcat(whole, " ");
				strncat(whole, second, 255);
				printf ("\n\nYou have requested: %s", whole);
				len = strlen(whole);
				sprintf(num, "%d", len);
				numlen = strlen(num);
				send(sockfd, num, numlen, 0);
				
				send(sockfd, whole, len, 0);
				free(whole);
			}
		}
		
		

//		printf ("\n\nYou have requested: %s", input);
//		len = strlen(input);
//		sprintf(num, "%d", len);
//		numlen = strlen(num);
//		send(sockfd, num, numlen, 0);
//		
//		send(sockfd, input, len, 0);
		
		bzero((char *)num, sizeof (num));
		bzero((char *)input, sizeof(input));
		sleep(2);
	}
	free(&sockfd);
	pthread_exit(0);	
}

// read from server
void *read_thread(void *port)
{

	int sockfd = *(int *)port;
	char output[1024];
	char num[20];
	int status = 0, reads = 0, size = 0;
	char *done = "done";
	while (1)
	{
		size = recv(sockfd,num, 20,0);		
		reads = atoi(num);
		status = recv(sockfd, output, reads,0);
		if(output[0] == '\0'){
			printf("Bank Server is closed...\nConnection lost from Bank server\n");
			exit(0);
		}
		while (status != reads)
		{
			status += recv(sockfd, output + status, reads - status, 0);
		}
		output[status] = '\0';
		if (strcmp(output, done) == 0){
			pthread_exit(NULL);
		}
		printf ("%s\n",output);
		bzero((char *)output, SIZE);
		bzero((char *)num, 20);
		status = 0;
		reads = 0;
	}
	free(&sockfd);
	pthread_exit(0);
}


