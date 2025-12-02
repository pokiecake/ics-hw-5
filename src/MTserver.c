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

/**********************DECLARE ALL LOCKS HERE BETWEEN THES LINES FOR MANUAL GRADING*************/

/***********************************************************************************************/

// Global variables, statistics collected since server start-up
int clientCnt;  // # of client connections made, Updated by the main thread
uint64_t maxDonations[3];  // 3 highest total donations amounts (sum of all donations to all
                           // charities in one connection), updated by client threads
                           // index 0 is the highest total donation
charity_t charities[5]; // Global variable, one charity per index

volatile int sigint = 0;

void sigint_handler(int sig) {
	sigint = 1;
}
// synchronization locks
sem_t mutex_stat, mutex_stat_w, mutex_char, mutex_char_w, mutex_dlog;
int stat_r_cnt = 0, char_r_cnt = 0;

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
	int log_file_fd = open(log_filename, O_RDWR | O_CREAT);
	if (log_file_fd == -1) {
		fprintf(stderr, "File could not open\n");
		exit(2);
	}
	// signal handler
	struct sigaction myaction = {{0}};
	myaction.sa_handler = sigint_handler;

	if (sigaction(SIGINT, &myaction, NULL) == -1) {
		printf("signal handler failed to install\n");
	}
    // Initiate server socket for listening
    int listen_fd = socket_listen_init(port_number);
    printf("Currently listening on port: %d.\n", port_number);
    int client_fd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while(1) {
        // Wait and Accept the connection from client
		#ifdef DEBUG
			printf("Waiting for client...", client_fd);
		#endif
        client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }
		#ifdef DEBUG
			printf("Listening on fd %d", client_fd);
		#endif
        // INSERT SERVER ACTIONS FOR CONNECTED CLIENT CODE HERE
		// join on all threads
		join_threads(thread_list);	
		//block sigint?
		//create thread
		pthread_t spawned_tid;
		int * arg = malloc(sizeof(int));
		*arg = client_fd;
		pthread_create(&spawned_tid, NULL, thread, arg);
		//add thread id to linked list
		InsertAtHead(thread_list, (void *)spawned_tid);

		//unblock and check sigint
    }
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
	int clientfd = *((int *)arg);
	free(arg);
	message_t * message = calloc(1, MSG_SIZE); 
	#ifndef DEBUG
		printf("Thread created, handling client %d", clientfd);
	#endif
	//read loop
	while (true) {
		read(clientfd, message, MSG_SIZE);
		uint8_t msgtype = message->msgtype;
		int ret; //error code for write
		switch(msgtype) {
			//Donate
			case 0x00:
				//hold lock
				P(&mutex_char_w);	
				//Update donation amnt
				add_donation_to_charity(message);
				//release lock
				V(&mutex_char_w);
				//send msg !! 
				ret = write(clientfd, message, MSG_SIZE);
				if (ret < 0) {
					//error, sending failed
				}
				//log file !!
				break;
			//CINFO
			case 0x01:
				//hold charity lock
				P(&mutex_char);
				char_r_cnt ++;
				if (char_r_cnt == 1) {
					P(&mutex_char_w);
				}
				V(&mutex_char);
				//read data into buffer
				uint8_t req_charity_i = get_charity_info(message); //todo!!
				charity_t * req_charity = charities + req_charity_i;
				//What the fuck am I supposed to send?
				//release lock
				P(&mutex_char);
				char_r_cnt --;
				if (char_r_cnt == 0) {
					V(&mutex_char_w);
				}
				V(&mutex_char);
				//send msg w/ charity info
				ret = write(clientfd, message, MSG_SIZE);
				if (ret < 0) {
					//error, sending failed
				}
				//log file
				break;
			//TOP
			case 0x02:
				//hold server lock
				P(&mutex_stat);
				stat_r_cnt ++;
				if (stat_r_cnt == 1) {
					P(&mutex_stat_w);
				}
				V(&mutex_stat);
				//read donation amounts
				write_max_donations(message, maxDonations); //todo!!
				//release server lock
				P(&mutex_stat);
				stat_r_cnt --;
				if (stat_r_cnt == 0) {
					V(&mutex_stat_w);
				}
				V(&mutex_stat);
				//send msg
				//log file
				break;
			//LOGOUT
			case 0x03:
				//close socket
				close(clientfd);
				//log file
				//update max donations
				break;
			//ERROR
			case 0xFF:
				//send msg
				//log file
				break;
		}
	}
	return NULL;
}
