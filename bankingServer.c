#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <sys/time.h>
#include <signal.h>

#define ERROR -1
#define SIZE 1024

typedef struct _Account{
	char *name;
	double balance;
	bool in_service;
	struct _Account *next;
} Account;


// server
void *session_process(void *port);
void *account_working(void *sock);
void split_commands(char *buffer, int sockfd, Account **current_user, bool *termination);
void one_query(char *first, int sockfd,  Account **current_user, bool *termination);
void two_query(char *first, char *second, int sockfd,  Account **current_user);
void create_user(char *name, int sockfd);
Account *serve_user(char *name, int sockfd);
void query_user (Account **current_user, int sockfd);
void diagnostic_display();
void *diagnostic_area(void *nope);
void alarm_handler(int signum);




pthread_mutex_t serving = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t create = PTHREAD_MUTEX_INITIALIZER;
int DISPLAY = 0;


// needs head, current, end for Account.
Account *acc_head;// first node of a linked list.
Account *acc_current;
Account *acc_end; // points to the end of list to add new user easily


int main (int args, char *argv[])
{
	pthread_t session, diagnosis;
	struct itimerval times;
	struct sigaction alaction;
	
	alaction.sa_handler = alarm_handler;
	sigemptyset(&alaction.sa_mask);
	alaction.sa_flags = 0;
	
	times.it_value.tv_sec = 15;
	times.it_value.tv_usec = 0;
	times.it_interval = times.it_value;
	
	if (setitimer(ITIMER_REAL, &times,NULL) == -1)
	{
		printf ("TIMER SET FAILED\n");
		exit(0);
	}

	if (sigaction(SIGALRM, &alaction, NULL) == -1)
	{
		printf ("SIG ACTION ERROR\n");
		exit(0);
	}
	acc_head = (Account *) malloc (sizeof (Account));
	acc_current = acc_head;
	acc_end = acc_head;
	// set dummy account to make it the head of the list
	
	
	if (pthread_create(&session,NULL, session_process, (void *) argv[1]) != 0)
	{
		printf ("Session thread Error\n");
		exit(0);
	}
	
	if (pthread_create(&diagnosis, NULL, diagnostic_area, NULL) != 0)
	{
		printf ("Diagnostic thread Error\n");
		exit(0);
	}
	
//	printf ("Printf\n");
	// waiting until the session ends.
	diagnostic_display();
	pthread_join(session, NULL);
	
	return 0;
	
}


void * session_process(void *port)
{
	struct sockaddr_in server, client;
	int sock, newsock;
	int len = sizeof(struct sockaddr);
	int *con;
	char *server_port = (char *)port;
	
	if ((sock = socket(AF_INET,SOCK_STREAM, 0)) == ERROR)
	{
		perror("socket");
		exit(-1);
	} 
	
	server.sin_family = AF_INET;
	server.sin_port = htons(atoi(server_port));
	server.sin_addr.s_addr = INADDR_ANY;
	bzero(&server.sin_zero, 8);
	
	if ((bind(sock, (struct sockaddr *)&server, len)) == ERROR)
	{
		perror("bind");
		exit(-1);
	}
	
	listen(sock, 5);
	
	
	while (newsock = accept(sock, (struct sockaddr *)&client, &len))
	{
		pthread_t thread;
		con = (int *)malloc (sizeof (int));
		*con = newsock;
		
		if (pthread_create(&thread, NULL, account_working, (void *)con) != 0)
		{
			perror("thread creation failed");
		}
		if (pthread_detach(thread) != 0)
		{
			perror("thread detach failed");
		}
	}
	
	pthread_exit(NULL);
}



// this function works for a client.
// this will be called every time the client is accepted.
void *account_working(void *sock)
{
	Account *current_user = NULL;
//	Account *current_user = NULL;
	int sockfd = *((int *)sock);
	// connection socket descripter.
	char buff[SIZE] = {0};
	// gets the message from the client.
	char buflen[20] = {0};
	// gets the length of the messages.
	int status = 0, reads = 0;
	int size = 0, tw = 20, n;
	// size of message.
	bool termination = false; // determine quit condition.
	char *done = "done";
	char *dsize = "4";
	printf ("Opened: %ld\n", pthread_self());
	
	while (1)
	{
	  size = recv(sockfd,buflen, tw,0);
	  if(size == 0){
			printf("A client closed by CTRL + C\n");
			close(sockfd);
			free(sock);
			pthread_exit(0);
		}
	  
/*
		if (strcmp(buflen, "bye") == 0)
		{
			printf ("the client terminated.\n");
			free(sock);
			pthread_exit(NULL);
		}
*/
//		while (size != tw)
//		{
//			size += read(sockfd, buflen + size, tw - size);
//		}
		// get the message size
		
		reads = atoi(buflen);
//		printf ("Size: %d\n", reads);
		status = recv(sockfd, buff, reads,0);

		while (status != reads)
		{
			status += recv(sockfd, buff + status, reads - status, 0);
		}
		buff[status] = '\0';
		
		printf ("Current input in Buffer: %s\n\n", buff);
		split_commands(buff, sockfd, &current_user, &termination);
		
		if (termination == true)
		{
			write(sockfd, dsize, strlen(dsize));
			write(sockfd, done, strlen(done));
			break;
		}
		
		bzero((char *)buff, SIZE);
		bzero((char *)buflen, 20);
		
		
		
//		sprintf (c,"%d",c1);

		status = 0;
		reads = 0;
		// reads whole messages from  a client.
	}
	
	close(sockfd);
	
}




// client에서 \n까지 같이 전송됨. 
void split_commands(char *buffer, int sockfd, Account **current_user, bool *termination)
{
	char *str;
	char *first, *second;
	char *er_msg = "Wrong Query\n";
	char buflen[20] = {0};
	int errlen = 0; // strlen of error message;
	int numlen = 0; // strlen of strlen of error message;
	
	str = strtok(buffer, " \n");
	
	
	if (str == NULL)
	{
		errlen = strlen(er_msg);
		sprintf(buflen, "%d", errlen);
		numlen = strlen(buflen);
		send(sockfd,buflen, numlen, 0);
		write(sockfd, er_msg, strlen(er_msg));
		return;
	}
	first = (char *) malloc (strlen(str) +1);
	strcpy(first, str);
	
	str = strtok(NULL, "\n");
	if (str == NULL)
	{
		// call one query func
//		printf ("GO INTO ONE QUERY\n");
		one_query(first, sockfd, current_user, termination);
		return;
	}
	
	second = (char *) malloc (strlen(str) + 1);
	strcpy(second, str);
	
	str = strtok(NULL, "\n");
	if (str == NULL)
	{
		// call two query func
		printf ("GO INTO TWO QUERY\n");
		two_query(first, second, sockfd, current_user);
		return;
	}
	else
	{
		errlen = strlen(er_msg);
		sprintf(buflen, "%d", errlen);
		numlen = strlen(buflen);
		send(sockfd,buflen, numlen, 0);
		write(sockfd, er_msg, strlen(er_msg));
		return;
	}
}


// one query
// needs sockfd to send a message when an error occurs
void one_query(char *first, int sockfd,  Account **current_user, bool *termination)
{
	char *end = "end";
	char *end_client = "\nServer: Your service is ended.\n";
	char *query = "query";
	char *quit = "quit";
	char *er_msg = "Wrong One Query\n";
	char *noexit = "\nServer: No serving users\n";
	char *quit_error = "\nServer: Service Session cannot quit the Customer Session\n";
	char buflen[20] = {0};
	int errlen = 0; // strlen of error message;
	int numlen = 0; // strlen of strlen of error message;
	
	if (!strcmp(end, first))
	{
		//ends
		if (*current_user == NULL)
		{
			//printf ("1\n");
			errlen = strlen(noexit);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			send(sockfd, noexit, strlen(noexit), 0);
			return;
		}
		else
		{
			// think weather mutex is needed or not
			errlen = strlen(end_client);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write(sockfd, end_client, strlen(end_client));
			(*current_user)->in_service = false;	
			(*current_user) = NULL;
		}
	}
	else if (!strcmp(query, first))
	{
		query_user(current_user, sockfd);
		// sends user's bank account;
		// call display_user_info function.
	}
	else if (!strcmp(quit, first))
	{
		if (*current_user != NULL)
		{
			printf ("2\n");
			errlen = strlen(quit_error);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			send(sockfd, quit_error, strlen(quit_error),0);
			return;
		}
		
		// free sockfd 
		*termination = true;
		return;
		//pthread_exit(NULL);
	}
	else
	{
//		printf ("3\n");
		errlen = strlen(er_msg);
		sprintf(buflen, "%d", errlen);
		numlen = strlen(buflen);
		send(sockfd, buflen, numlen, 0);
		send(sockfd, er_msg, strlen(er_msg),0);
//		printf ("SENDED\n");
		return;
	}
}

// two query 
void two_query(char *first, char *second, int sockfd,  Account **current_user)
{
	char *create = "create";
	char *serve = "serve";
	char *deposit = "deposit";
	char *deposit_S = "\nServer: deposit Sucessfull!\n";
	char *withdraw = "withdraw";	
	char *withdraw_S = "\nServer: withdraw Sucessfull!\n";
	char *er_msg = "\nServer: Wrong Two Query.";
	char *creation = "\nServer: Creation Successful!\n";
	char *create_error = "\nServer: Customer in service cannot create an account.";
	char *create_error2 = "\nServer: Same name of account already exits.";
	char *not_inservice_error = "\nServer: Cannot Deposit!\nUser is not in service.";
	char *deposit_error = "\nServer: Only Positive amount of money can be deposited.";
	char *withdraw_error = "\nServer: Use positive amount of money.";
	char *withdraw_error2 = "\nServer: Cannot withdraw more than balance.";
	char *serve_error = "\nServer: User is currently using account.";
	char *in_serve_error = "\nServer: the user you entered is currently being used by others.";
	char *in_serve_check = "\nServer: Bank is now serving your Account.";
	char *no_user = "\nServer: There is no user.";
	char *display;
	char *u_creation = " is created.\n";
	Account *check;
	double money = 0;
	char buflen[20] = {0};
	int errlen = 0; // strlen of error message;
	int numlen = 0; // strlen of strlen of error message;
	
	if (!strcmp(create, first))
	{
		// create user.
		if (*current_user != NULL)
		{
			printf ("CREATE ERROR\n");
			errlen = strlen(create_error);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write(sockfd, create_error, strlen(create_error));
			return;
		}
		else
		{
			/*errlen = strlen(creation);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write(sockfd, creation, strlen(creation));
			*/create_user(second, sockfd);
			display = (char *) malloc (strlen(second) + strlen(u_creation) + 1);
			strcpy(display, second);
			strcat (display, u_creation);
			
			return;
		}
	}
	else if (!strcmp(serve, first))
	{
		if (*current_user != NULL)
		{
			printf ("SERVE ERROR\n");
			errlen = strlen(serve_error);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write(sockfd, serve_error, strlen(serve_error));
			return;	
		}
		else
		{
			check = acc_head->next;
			while (check != NULL)
			{
				if (!strcmp(second, check->name))
				{
					//mutex
					pthread_mutex_lock(&serving);
					//printf ("is this ?\n");
					if (check->in_service == true)
					{
						printf ("IN SERVE ERROR\n");
						errlen = strlen(in_serve_error);
						sprintf(buflen, "%d", errlen);
						numlen = strlen(buflen);
						send(sockfd,buflen, numlen, 0);
						write(sockfd, in_serve_error, strlen(in_serve_error));
					}
					else
					{
						errlen = strlen(in_serve_check);
						sprintf(buflen, "%d", errlen);
						numlen = strlen(buflen);
						send(sockfd,buflen, numlen, 0);
						write(sockfd, in_serve_check, strlen(in_serve_check));
						*current_user = check;
						check->in_service = true;
					}
					//metex
					pthread_mutex_unlock(&serving);
					return;
				}
				check = check->next;
			}
			if (*current_user == NULL)
			{
				printf ("NO USER EXITS\n");
				errlen = strlen(no_user);
				sprintf(buflen, "%d", errlen);
				numlen = strlen(buflen);
				send(sockfd,buflen, numlen,0);
				write(sockfd, no_user, strlen(no_user));
				return;	
			}
		}
	}
	else if (!strcmp(deposit, first))
	{
		if (*current_user == NULL)
		{
			printf ("NOT IN SERVICE\n");
			errlen = strlen(not_inservice_error);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write (sockfd, not_inservice_error, strlen(not_inservice_error));
			return;
		}
		else
		{
			// deposit
			money = atof(second);
			if (money <= 0)
			{
				printf ("DEPOSIT\n");
				errlen = strlen(deposit_error);
				sprintf(buflen, "%d", errlen);
				numlen = strlen(buflen);
				send(sockfd,buflen, numlen, 0);
				write (sockfd, deposit_error, strlen(deposit_error));
				return;
			}

			errlen = strlen(deposit_S);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write(sockfd, deposit_S, strlen(deposit_S));
			(*current_user)->balance += money;
			return;
		}
	}
	else if (!strcmp(withdraw, first))
	{
		if (*current_user == NULL)
		{
			printf ("NOT IN SERVICE\n");
			errlen = strlen(not_inservice_error);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write (sockfd, not_inservice_error, strlen(not_inservice_error));
			return;
		}
		else
		{
			// withdraw.
			money = atof(second);
			if (money <= 0)
			{
				printf ("WITHDRAW ERROR\n");
				errlen = strlen(withdraw_error);
				sprintf(buflen, "%d", errlen);
				numlen = strlen(buflen);
				send(sockfd,buflen, numlen, 0);
				write (sockfd, withdraw_error, strlen(withdraw_error));
				return;
			}
			
			if ((*current_user)->balance < money)
			{
				printf ("WITHDRAW ERROR, amount exceeds your current deposit\n");
				errlen = strlen(withdraw_error2);
				sprintf(buflen, "%d", errlen);
				numlen = strlen(buflen);
				send(sockfd,buflen, numlen, 0);
				write (sockfd, withdraw_error2, strlen(withdraw_error2));
				return;
			}
			errlen = strlen(withdraw_S);
			sprintf(buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd,buflen, numlen, 0);
			write(sockfd, withdraw_S, strlen(withdraw_S));
			(*current_user)->balance -= money;
			return;
		}		
	}
	else
	{
		printf ("er_ms ERROR\n");
		errlen = strlen(er_msg);
		sprintf(buflen, "%d", errlen);
		numlen = strlen(buflen);
		send(sockfd,buflen, numlen, 0);
		write(sockfd, er_msg, strlen(er_msg));
		return;
	}
}

void create_user(char *name, int sockfd)
{
	Account *new_user;
	char *create_error2 = "\nServer: Same name of account already exits.";
	char buflen[20] = {0};
	int errlen = 0;
	int numlen =0;

	new_user = (Account *) malloc (sizeof (Account));
	

	//printf ("WHERE\n");
	//mutex
	pthread_mutex_lock(&create);
	acc_current = acc_head->next;
	while (acc_current != NULL)
	{
		if (!strcmp(acc_current->name, name))
		{
			printf ("CREATION ERROR\n");
			errlen = strlen(create_error2);
			sprintf (buflen, "%d", errlen);
			numlen = strlen(buflen);
			send(sockfd, buflen, numlen, 0);
			write (sockfd, create_error2, strlen(create_error2));
			free(new_user);
			pthread_mutex_unlock(&create);
			return;
		}
		acc_current = acc_current->next;
	}
	acc_end->next = new_user;
	acc_end = new_user;
	pthread_mutex_unlock(&create);
	//mutex;
	
	new_user->name = (char *) malloc (strlen(name) + 1);
	strcpy(new_user->name, name);
	new_user->balance = 0.0;
	new_user->in_service = false;
}


// serve user
Account *serve_user(char *name, int sockfd)
{
	Account *current;
	char *service_error = "\nServer: User is already in service.";
	char *service_error2 = "\nServer: Account does not exist in bank server. Create one before use";
	
	current = acc_head;
	while (current != NULL)
	{
		if (!strcmp(current->name, name))
		{
			if (current->in_service == true)
			{
				write (sockfd, service_error, strlen(service_error));
				return (Account *) NULL;
			}
			else
			{
				//mutex
				current->in_service = true;
				// mutex
			}
		}
		current = current->next;
	}
	if (current == NULL)
	{
		write(sockfd, service_error2, strlen(service_error2));
		return (Account *) NULL;
	}
	
	return current;
}
// user info
void query_user (Account **current_user, int sockfd)
{
	char *query_error = "\nServer: No user in the session.";
	char buflen[20] = {0};
	int errlen = 0; // strlen of error message;
	int numlen = 0; // strlen of strlen of error message;
	char balance[50] = {0};
	int msglen = 0;
	char *your = "\nServer: Your balance is ";
	char *msg;
	
	if (*current_user == NULL)
	{
		printf ("QUERY ERROR\n");
		errlen = strlen(query_error);
		sprintf(buflen, "%d", errlen);
		numlen = strlen(buflen);
		send(sockfd,buflen, numlen, 0);
		write (sockfd, query_error, strlen(query_error));
		return;
	}
	else
	{
		snprintf (balance, 20 ,"%.2f", (*current_user)->balance);
//		printf ("balance: %s\n", balance);
//		printf ("leng: %d\n", strlen(balance)); 
		msg = (char *)malloc (strlen(your) + strlen(balance) + 1);
		strcpy(msg, your);
		strcat(msg, balance);
		numlen = strlen(msg);
		sprintf (buflen, "%d", numlen);
		msglen = strlen(msg);
		send(sockfd, buflen, strlen(buflen), 0);
		send (sockfd, msg, strlen(msg), 0);
		free(msg);
		msg = NULL;
		return;	
	}
}

void alarm_handler(int signum)
{
	DISPLAY = 1;
}


void *diagnostic_area(void *nope)
{
	while (1)
	{
		if (DISPLAY == 1)
		{
			diagnostic_display();
			DISPLAY = 0;
		}
	}
}


void diagnostic_display()
{
	int num = 1;
	char* info;
	char *aname = "\t| Name: ";
	char *abalance = "\t| Balance: ";
	char *in_service = " Account | IN SERVICE\t";
	char *out_of_service = " Account | NOT IN SERVICE ";
	char number[5] = {0};
	char bal[30] = {0};
	acc_current = acc_head->next;
	char *diag = "\n\n\t\tBANK ACCOUNT DIAGNOSIS";
	char *star = "\n*****************************************************************\n\n";
	
	write(STDOUT_FILENO, diag, strlen(diag));
	write(STDOUT_FILENO, star, strlen(star));
	
	while (acc_current != NULL)
	{
		sprintf(number,"%.2d", num);
		sprintf (bal, "%.2f", acc_current->balance);
		if(acc_current->in_service == true)
		{
			info = (char *) malloc (strlen(aname)+strlen(abalance)+strlen(in_service)+strlen(acc_current->name)+strlen(number)+strlen(bal)+2);
			strcpy(info,number);
			strcat(info, in_service);
			strcat(info, aname);
			strcat(info, acc_current->name);
			strcat(info, abalance);
			strcat(info, bal);
			strcat(info, "\n");
		}
		else
		{
			info = (char *) malloc (strlen(aname)+strlen(abalance)+strlen(out_of_service)+strlen(acc_current->name)+strlen(number)+strlen(bal)+2);
			strcpy(info,number);
			strcat(info, out_of_service);
			strcat(info, aname);
			strcat(info, acc_current->name);
			strcat(info, abalance);
			strcat(info, bal);
			strcat(info, "\n");
		}
		write(STDOUT_FILENO,info, strlen(info));
		free(info);
		acc_current = acc_current->next;
		num++;
	}
	
	write(STDOUT_FILENO, star, strlen(star));
}



