#define _GNU_SOURCE
#include "server.h"
#include "protocol.h"
#include "PChelpers.h"
#include <pthread.h>


//TODO vars for PC buffer
/**********************DECLARE ALL LOCKS HERE BETWEEN THES LINES FOR MANUAL GRADING*************/
sem_t mutex_stat, mutex_stat_w, mutex_job_slots, mutex_job_items, mutex_job;
sem_t mutex_charities[5];
/***********************************************************************************************/

// Global variables, statistics collected since server start-up
int clientCnt;  // # of client connections made, Updated by the main thread
uint64_t maxDonations[3];  // 3 highest total donations amounts (sum of all donations to all  
                           // charities in one connection), updated by client threads
                           // index 0 is the highest total donation
charity_t charities[5];    // Global variable, one charity per index

// Global variable for job buffer (connection between client and job threads)
job_t buffer[5];

int logfile_fd;

void init_logfilefd();
void init_mutexes();
void init_sigint_handler();
void sigint_handler(int sig);
void * thread_prod();
void * thread_cons();

volatile sig_atomic_t sigint;

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
    if (argc != 4) {
        fprintf(stderr, USAGE_MSG_MT);
        exit(EXIT_FAILURE);
    }
    unsigned int port_number = atoi(argv[1]);
    char *log_filename = argv[2];
	int num_job_threads = atoi(argv[3]);


    // INSERT SERVER INITIALIZATION CODE HERE
	//Open log file
	init_logfilefd(log_filename);
	//mutex initialization
	init_mutexes();
	//sigaction sigint handler
	init_sigint_handler();
	
	//thread id list
	list_t * prod_thread_list = init_T_List();
	pthread_t cons_thread_arr[num_job_threads];

	//spawn job threads
	int i;
	for (int i = 0; i < num_job_threads; i++) {
		Pthread_create(cons_thread_arr + i, NULL, thread_cons, NULL);
	}

    // Initiate server socket for listening
    int listen_fd = socket_listen_init(port_number);
    printf("Currently listening on port: %d.\n", port_number);
    int client_fd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while(1) {
        // Wait and Accept the connection from client
        client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
		//TODO loop when a signal is received
		//TODO kill when sigint is received
        if (client_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }

        // INSERT SERVER ACTIONS FOR CONNECTED CLIENT CODE HERE
		join_threads(prod_thread_list);
		//spawn prod thread
		pthread_t tid;
		int * arg = malloc(sizeof(int));
		*arg = client_fd;
		Pthread_create(&tid, NULL, thread_prod, arg);
		//increment client count
		clientCnt++;
		//insert thread into list
		InsertAtHead(prod_thread_list, &tid);
    }

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

void * thread_prod() {

	return NULL;
}

void * thread_cons() {
	
	return NULL;
}


void init_logfilefd(char * log_filename) {
	//Open log file
	logfile_fd = Open(log_filename, O_WRONLY | O_CREAT);
	Ftruncate(logfile_fd);
}

void init_mutexes() {
	//mutex initialization
	init_mutex(&mutex_stat);
	init_mutex(&mutex_stat_w);
	init_mutex(&mutex_job_slots);
	init_mutex(&mutex_job_items);
	init_mutex(&mutex_job);
}

void init_sigint_handler() {
	//sigaction sigint handler
	struct sigaction myaction = {{0}};
	myaction.sa_handler = &sigint_handler;
	Sigaction(SIGINT, &myaction);
}

void sigint_handler(int sig) {
	sigint = 1;
}


