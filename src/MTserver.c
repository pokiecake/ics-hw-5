#define _GNU_SOURCE
#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "linkedlist.h"
#include "MThelpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>

/*TODO
!!server statistics should only have one mutex
*/

/**********************DECLARE ALL LOCKS HERE BETWEEN THES LINES FOR MANUAL GRADING*************/

//!!Remove during testing!!
//#define DEBUG
// synchronization locks
sem_t mutex_stat, mutex_stat_w, mutex_char, mutex_char_w, mutex_dlog, mutex_sigint;
/*
mutex_stat - lock for stat_r_cnt
mutex_stat_w - writers stick for server statistics
mutex_char - lock for char_r_cnt (readers count for charity stats)
mutex_char_w - writers stick for charity data structures
mutex_dlog - lock for log file
*/
int stat_r_cnt = 0, char_r_cnt = 0;
/*
stat_r_cnt = readers count for server statistics
char_r_cnt = readers count for charity statstics
*/
/***********************************************************************************************/

// Global variables, statistics collected since server start-up
int clientCnt;  // # of client connections made, Updated by the main thread
uint64_t maxDonations[3];  // 3 highest total donations amounts (sum of all donations to all
                           // charities in one connection), updated by client threads
                           // index 0 is the highest total donation
charity_t charities[5]; // Global variable, one charity per index

volatile int sigint = 0;

void sigint_handler(int sig) {
	#ifdef DEBUG
		write(STDOUT_FILENO, "sigint raised\n", 15);
	#endif
	P(&mutex_sigint);
	sigint = 1;
	V(&mutex_sigint);
}

int main(int argc, char *argv[]) {
	

    // Arg parsing
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stderr, USAGE_MSG_MT);
                exit(EXIT_FAILURE);
        }
    }

    // 3 positional arguments necessary
    if (argc != 3) {
        fprintf(stderr, USAGE_MSG_MT);
        exit(EXIT_FAILURE);
    }
    unsigned int port_number = atoi(argv[1]);
    char *log_filename = argv[2];
	sem_init(&mutex_stat, 0, 1);
	sem_init(&mutex_stat_w, 0, 1);
	sem_init(&mutex_char, 0, 1);
	sem_init(&mutex_char_w, 0, 1);
	sem_init(&mutex_dlog, 0, 1);

    // INSERT SERVER INITIALIZATION CODE HERE
	//initialize data structures
	clientCnt = 0;
	int i = 0;
	for (i = 0; i < 3; i++)
		maxDonations[i] = 0;
	
	// initialize thread linked list
	list_t * thread_list = init_T_List();		
	// initialize log file 
	int log_file_fd = open(log_filename, O_WRONLY | O_CREAT);
	//O_RDWR produces bugs??? (blocks file form RDWR)
	if (log_file_fd == -1) {
		fprintf(stderr, "File could not open\n");
		exit(2);
	}
	//remove previous logs 
	Ftruncate(log_file_fd);
	// signal handler
	struct sigaction myaction = {{0}};
	myaction.sa_handler = &sigint_handler;

	Sigaction(SIGINT, &myaction);
    // Initiate server socket for listening
    int listen_fd = socket_listen_init(port_number);
    printf("Currently listening on port: %d.\n", port_number);
    int client_fd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while(1) {
        // Wait and Accept the connection from client
		#ifdef DEBUG
			printf("Waiting for client...\n");
		#endif
        client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
		while (client_fd < 0 && sigint == 0) { //ignore signals that are not sigint
			client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
		}
		//printf("sigint flag raised: %d\n", sigint);
		if (sigint == 1) {
			#ifdef DEBUG
			fprintf(stderr, "ERROR: Sigint raised\n");
			#endif
			//kill all threads & join all threads
			kill_all_threads(thread_list);
			//make sure threads finish tasks and updating stats
			
			//close listening socket  !!should be done outside the loop
			//close(listen_fd);
			//close log file
			//close(log_file_fd);
			break;
		}
        if (client_fd < 0) {
			close(listen_fd); //for development
			close(log_file_fd); //for development
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }
		#ifdef DEBUG
			printf("Listening on fd %d\n", client_fd);
		#endif
        // INSERT SERVER ACTIONS FOR CONNECTED CLIENT CODE HERE
		//increment client count
		clientCnt++;
		// join on all threads
		join_threads(thread_list);	
		//!!Thread id probably not removed from list entirely
		//block sigint?
		//create thread
		pthread_t spawned_tid;
		int * arg = malloc(sizeof(int) * 2);
		*arg = client_fd;
		*(arg + 1) = log_file_fd;
		pthread_create(&spawned_tid, NULL, thread, arg);
		//add thread id to linked list
		InsertAtHead(thread_list, (void *)spawned_tid);

    }
	//final output
	print_all_charities(charities, 5);
	print_statistics(clientCnt, maxDonations);
	
	close(log_file_fd);

    close(listen_fd);
    return 0;
}

int socket_listen_init(int server_port){
    int sockfd;
    struct sockaddr_in servaddr;

    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully created\n");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    int opt = 1;

    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, (char *)&opt, sizeof(opt))<0)
    {
    	perror("setsockopt");exit(EXIT_FAILURE);
    }

    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (SA*)&servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed\n");
        exit(EXIT_FAILURE);
    }
    else
        printf("Socket successfully binded\n");

    // Now server is ready to listen and verification
    if ((listen(sockfd, 1)) != 0) {
        printf("Listen failed\n");
        exit(EXIT_FAILURE);
    }
    return sockfd;
}

const int MSG_SIZE = 32;
void * thread(void * arg) {
	//do we let the thread detach?
	int * fds = (int *)arg;
	int clientfd = fds[0];
	int log_fd = fds[1];
	free(arg);
	fds = NULL;
	message_t * message = Calloc(1, MSG_SIZE); 
	#ifdef DEBUG
		printf("Thread created, handling client %d\n", clientfd);
	#endif
	uint64_t max_amnt = 0;
	int logout = 0;
	//read loop
	while (logout != 1) {
		int ret_err; //error ret_errvalues for functions
		read(clientfd, message, MSG_SIZE);
		//error checking for read
		uint8_t msgtype = message->msgtype;
		char * log_msg; //message for log file
		switch(msgtype) {
			//Donate
			case DONATE:
				#ifdef DEBUG
					printf("%d handling Donate\n", clientfd);
				#endif
				//hold lock		
				P(&mutex_char_w);	
				//Update donation amnt
				ret_err = add_donation_to_charity(message, &max_amnt, charities, 5);
				//release lock
				V(&mutex_char_w);
				if (ret_err < 0) {
					send_err_msg(log_fd, clientfd, &mutex_dlog, message);
					//error, sending failed
					break;
				}
				//send msg !! 
				ret_err = write(clientfd, message, MSG_SIZE);
				if (ret_err < 0) {
					//error, sending failed
				}
				uint8_t charity_i = message->msgdata.donation.charity;
				uint64_t amnt = message->msgdata.donation.amount;
				//log file !!
				P(&mutex_dlog);
				log_msg;
				asprintf(&log_msg, "%d DONATE %u %lu\n", clientfd, charity_i, amnt);
				//char msg[] = "DONATE";
				// char charity_msg = 
				// ltostr(charity_i, charity_msg);
				// "%d DONATE %u %ul\n";
				write(log_fd, log_msg, strlen(log_msg));
				//fprintf(log_fd, "%d DONATE %u %ul\n", clientfd, charity_i, amnt);
				V(&mutex_dlog);
				free(log_msg);
				break;
			//CINFO
			case CINFO:
				#ifdef DEBUG
					printf("%d handling Cinfo\n", clientfd);
				#endif
				//read data into buffer
				uint8_t req_charity_i = get_charity_info(message); //todo!!
				//hold charity lock
				P(&mutex_char);
				char_r_cnt ++;
				if (char_r_cnt == 1) {
					P(&mutex_char_w);
				}
				V(&mutex_char);
				//charity_t * req_charity = charities + req_charity_i;
				message->msgdata.charityInfo = charities[req_charity_i];
				//release lock
				P(&mutex_char);
				char_r_cnt --;
				if (char_r_cnt == 0) {
					V(&mutex_char_w);
				}
				V(&mutex_char);
				//send msg w/ charity info
				ret_err= write(clientfd, message, MSG_SIZE);
				if (ret_err< 0) {
					//error, sending failed
					
				}
				//log file
				P(&mutex_dlog);
				//char msg[] = "%d CINFO %u\n";
				asprintf(&log_msg, "%d CINFO %u\n", clientfd, req_charity_i);
				write(log_fd, log_msg, strlen(log_msg));
				//fprintf(log_fd, "%d CINFO %u\n", clientfd, req_charity_i);
				V(&mutex_dlog);
				free(log_msg);
				break;
			//TOP
			case TOP:
				#ifdef DEBUG
					printf("%d handling Top\n", clientfd);
				#endif
				//hold server lock
				P(&mutex_stat);
				stat_r_cnt ++;
				if (stat_r_cnt == 1) {
					P(&mutex_stat_w);
				}
				V(&mutex_stat);
				#ifdef DEBUG
					printf("\t%d takes mutex_stat_w writers stick\n", clientfd);
				#endif
				//read donation amounts
				write_max_donations(message, maxDonations); //todo!!
				#ifdef DEBUG
					printf("\t%d copied max donations\n", clientfd);
				#endif
				//release server lock
				P(&mutex_stat);
				stat_r_cnt --;
				if (stat_r_cnt == 0) {
					V(&mutex_stat_w);
				}
				V(&mutex_stat);
				#ifdef DEBUG
					printf("\t%d released mutex_stat_w writers stick\n", clientfd);
				#endif
				//send msg
				write(clientfd, message, MSG_SIZE);
				#ifdef DEBUG
					printf("\t%d has written to client and preparting log\n", clientfd);
				#endif
				//log file
				P(&mutex_dlog);
				log_msg;
				asprintf(&log_msg, "%d TOP\n", clientfd);
				write(log_fd, log_msg, strlen(log_msg));
				//fprintf(log_fd, "%d TOP\n", clientfd);
				V(&mutex_dlog);
				free(log_msg);
				break;
			//LOGOUT
			case LOGOUT:
				#ifdef DEBUG
					printf("%d handling Logout\n", clientfd);
				#endif
				//close socket
				close(clientfd);
				//log file
				P(&mutex_dlog);
				log_msg;
				asprintf(&log_msg, "%d LOGOUT\n", clientfd);
				write(log_fd, log_msg, strlen(log_msg));
				//fprintf(log_fd, "%d LOGOUT\n", clientfd);
				V(&mutex_dlog);
				//update max donations 
				P(&mutex_stat_w);
				if (max_amnt > maxDonations[2]) {
					update_highest_dono(max_amnt, maxDonations, 3);
				}
				V(&mutex_stat_w);
				logout = 1;
				free(log_msg);
				break;
			case ERROR:
				message->msgtype = ERROR;
				send_err_msg(log_fd, clientfd, &mutex_dlog, message);
				break;
		}
	}

	#ifdef DEBUG
		printf("%d thread is exiting loop\n", clientfd);
	#endif
	
	free(message);
	return NULL;
}
