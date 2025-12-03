#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "RWhelpers.h"
#include "linkedlist.h"

/**********************DECLARE ALL LOCKS HERE BETWEEN THES LINES FOR MANUAL GRADING*************/
sem_t mutex_sigint, mutex_stat, mutex_stat_w, mutex_char, mutex_char_w, mutex_dlog;
/***********************************************************************************************/

// Global variables, statistics collected since server start-up
int clientCnt;  // # of client connections made, Updated by the main thread
uint64_t maxDonations[3];  // 3 highest total donations amounts (sum of all donations to all  
                           // charities in one connection), updated by client threads
                           // index 0 is the highest total donation
charity_t charities[5]; // Global variable, one charity per index

volatile int sigintflag = 0;

void sigint_handler(int sig) {
	P(&mutex_sigint);
	sigintflag = 1;
	V(&mutex_sigint);
}
void * writer_thread(void * arg);
void * reader_thread(void * arg);

int main(int argc, char *argv[]) {

    // Arg parsing
    int opt;
    while ((opt = getopt(argc, argv, "h")) != -1) {
        switch (opt) {
            case 'h':
                fprintf(stderr, USAGE_MSG_RW);
                exit(EXIT_FAILURE);
        }
    }

    // 3 positional arguments necessary
    if (argc != 4) {
        fprintf(stderr, USAGE_MSG_RW);
        exit(EXIT_FAILURE);
    }
    unsigned int r_port_number = atoi(argv[1]);
    unsigned int w_port_number = atoi(argv[2]);
    char *log_filename = argv[3];


    // INSERT SERVER INITIALIZATION CODE HERE
	//clientCnt = 0;
	//Open log file
	int log_file_fd = Open(log_filename, O_WRONLY | O_CREAT);
	Ftruncate(log_file_fd);
	//sigaction sigint handler
	struct sigaction myaction = {{0}};
	myaction.sa_handler = &sigint_handler;
	Sigaction(SIGINT, &myaction);

	//mutex initialization
	init_mutex(&mutex_sigint);
	init_mutex(&mutex_stat);
	init_mutex(&mutex_stat_w);
	init_mutex(&mutex_char);
	init_mutex(&mutex_char_w);
	init_mutex(&mutex_dlog);

	//thread id list
	list_t * thread_list = init_T_List();
	
    // CREATE WRITER THREAD HERE
	pthread_t writer_tid;
	int * writer_fds = malloc(sizeof(int) * 1);
	writer_fds[0] = log_file_fd;
	Pthread_create(&writer_tid, NULL, writer_thread, writer_fds);

    // Initiate server socket for listening for reader clients
    int reader_listen_fd = socket_listen_init(r_port_number); 
    printf("Listening for readers on port %d.\n", r_port_number);

    int reader_fd;
    struct sockaddr_in client_addr;
    unsigned int client_addr_len = sizeof(client_addr);

    while(1) {
        // Wait and Accept the connection from client
        reader_fd = accept(reader_listen_fd, (SA*)&client_addr, &client_addr_len);
		//loop until sig int is called
		//join all threads on sig int
        if (reader_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }
        
        // INSERT SERVER ACTIONS FOR CONNECTED READER CLIENT CODE HERE
		//check for terminated threads and join

		//spawn new reader thread
		pthread_t reader_tid;
		//give file descriptors as payload
		int * fds = malloc(sizeof(int) * 2);
		fds[0] = reader_fd;
		fds[1] = log_file_fd;
		Pthread_create(&reader_tid, NULL, reader_thread, fds);
		//insert into list
		InsertAtHead(thread_list, &reader_thread);
    }

    close(reader_listen_fd);
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

void * writer_thread(void * arg) {
	int * fds = (int *)arg;
	int logfilefd = fds[0];
	free(arg);
	char * logmsg = "Writer thread initialized\n";
	write(logfilefd, logmsg, strlen(logmsg));
	
	return NULL;
}

void * reader_thread(void * arg) {
	int * fds = (int *)arg;
	int clientfd = fds[0];
	int logfilefd = fds[1];
	free(arg);
	char * logmsg = "Reader thread initialized\n";
	write(logfilefd, logmsg, strlen(logmsg));

	return NULL;
}


