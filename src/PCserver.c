#define _GNU_SOURCE
#include "server.h"
#include "protocol.h"
#include "PChelpers.h"
#include <pthread.h>

//TODO !!REMOVE DURING SUBMISSION
//#define DEBUG

/*TODO 
update usage statement
*/

/**********************DECLARE ALL LOCKS HERE BETWEEN THES LINES FOR MANUAL GRADING*************/
sem_t mutex_stat, mutex_stat_w, sem_job_slots, sem_job_items, mutex_job, mutex_dlog;
int stat_r_cnt;
sem_t mutex_charities[5];
/*
mutex_stat - lock for stat_r_cnt
mutex_stat_w - writers stick for server statistics
mutex_charities[5] - mutex for each charity
sem_job_slots - semaphore for # of available slots
sem_job_items - semaphore for # of items 
mutex_job - mutex lock for job_t buffer[]
mutex_dlog - lock for log file
*/
/***********************************************************************************************/
typedef struct {
	int size;
	int front;
	int rear;
} pcbuf;
// Global variables, statistics collected since server start-up
int clientCnt;  // # of client connections made, Updated by the main thread
uint64_t maxDonations[3];  // 3 highest total donations amounts (sum of all donations to all  
                           // charities in one connection), updated by client threads
                           // index 0 is the highest total donation
charity_t charities[5];    // Global variable, one charity per index

// Global variable for job buffer (connection between client and job threads)
job_t buffer[5];
pcbuf job_buffer;

int logfile_fd;
pthread_t mainthread_id;

void init_logfilefd();
void init_mutexes(int);
void init_sigint_handler();
void sigint_handler(int sig);
void * thread_prod(void *);
void * thread_cons(void *);

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
	int num_job_threads = atoi(argv[2]);
    char *log_filename = argv[3];


    // INSERT SERVER INITIALIZATION CODE HERE
	//Open log file
	init_logfilefd(log_filename);
	//mutex initialization
	init_mutexes(num_job_threads);
	//sigaction sigint handler
	init_sigint_handler();
	
	//thread id list
	list_t * prod_thread_list = init_T_List();
	pthread_t cons_thread_arr[num_job_threads];

	//initialize main thread tid
	mainthread_id = pthread_self();

	//initialize job buffer info
	job_buffer.size = num_job_threads; //front and rear are already initialized to 0

	//spawn job threads
	int i;
	for (int i = 0; i < num_job_threads; i++) {
		Pthread_create(&cons_thread_arr[i], NULL, thread_cons, NULL);
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
			#ifdef DEBUG
				printf("out of accept\n");
			#endif
		while (client_fd == -1 && sigint == 0) {
			#ifdef DEBUG
				printf("reconnecting\n");
			#endif
	        client_fd = accept(listen_fd, (SA*)&client_addr, &client_addr_len);
		}
		if (sigint == 1) {
		    close(listen_fd);
			#ifdef DEBUG
				printf("killing threads\n");
			#endif
			kill_all_threads(prod_thread_list, cons_thread_arr, num_job_threads);
			break;
		}
		//TODO loop when a signal is received
		//TODO kill when sigint is received
        if (client_fd < 0) {
		    close(listen_fd);
			close(logfile_fd);
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }

        // INSERT SERVER ACTIONS FOR CONNECTED CLIENT CODE HERE
		join_threads(prod_thread_list);
		//spawn prod thread
		pthread_t * reader_tid = malloc(sizeof(pthread_t));
		int * arg = malloc(sizeof(int));
		*arg = client_fd;
		Pthread_create(reader_tid, NULL, thread_prod, arg);
		//increment client count
		clientCnt++;
		//insert thread into list
		InsertAtHead(prod_thread_list, reader_tid);
    }

    //close(listen_fd);
	close(logfile_fd);
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

void * thread_prod(void * arg) {
	pthread_t self_tid = pthread_self();
	#ifdef DEBUG
		printf("prod %lu created\n", self_tid);
	#endif
	//take client fd
	int * fd = (int *)arg;
	int clientfd = fd[0];
	free(arg);
	
	//read msg from client
	message_t * msg = Calloc(1, sizeof(message_t));

	while (1) {
		
		int reterr = read(clientfd, msg, sizeof(message_t));
		
		//loop signals until sigint or error
		while (errno == EINTR && sigint == 0) {
			int reterr = read(clientfd, msg, sizeof(message_t));
		}
		if (sigint == 1) {
			return NULL;
		}
		if (reterr < 0 && errno != EINTR) {
			fprintf(stderr, "ERROR: Failed to read from client. Errno=%u\n", errno);
			exit(1);
		}
	
		//logout
		if (msg->msgtype == LOGOUT) {
			close(clientfd);
			P(&mutex_dlog);
			char * logmsg;
			asprintf(&logmsg, "%lu %u LOGOUT\n", mainthread_id, clientfd);
			write(logfile_fd, logmsg, strlen(logmsg));
			V(&mutex_dlog);
			return NULL;
		}
	
		job_t payload = {.fd = clientfd, .msg = *msg};
	
		#ifdef DEBUG
			printf("prod %lu checking slots\n", self_tid);
		#endif
		//hold a slot
		P(&sem_job_slots);
		if (sigint == 1) {
			return NULL;
		}
		#ifdef DEBUG
			printf("prod %lu holding buffer\n", self_tid);
		#endif
		//mutex job arr
		P(&mutex_job);
		if (sigint == 1) {
			V(&sem_job_slots);
			return NULL;
		}
		//put message in job arr
		buffer[((job_buffer.rear)++) % (job_buffer.size)] = payload;
	
		//release job arr
		#ifdef DEBUG
			printf("prod %lu releasing locks\n", self_tid);
		#endif
		V(&mutex_job);
		//announce item
		V(&sem_job_items);
		
	}
	return NULL;
}

void * thread_cons(void * arg) {
	pthread_t self_tid = pthread_self();
	char * logmsg;
	while (1) {
		#ifdef DEBUG
			printf("cons %lu checking items\n", self_tid);
		#endif
		int reterr;
		//hold items lock
		P(&sem_job_items);
		if (sigint == 1) {
			#ifdef DEBUG
				printf("cons %lu exiting\n", self_tid);
			#endif
			break;
		}
		#ifdef DEBUG
			printf("cons %lu holding onto buffer\n", self_tid);
		#endif
		//hold job arr
		P(&mutex_job);
		if (sigint == 1) {
			V(&sem_job_items);
			#ifdef DEBUG
				printf("cons %lu exiting\n", self_tid);
			#endif
			break;
		}
		#ifdef DEBUG
			printf("cons %lu found item\n", self_tid);
		#endif

		//take the payload from the job buffer
		job_t * payload = buffer + (job_buffer.front++) % job_buffer.size; //front points to the last used item
		
		//release lock
		V(&mutex_job);
		//anounce slot
		V(&sem_job_slots);
		
		int client_fd = payload->fd;
		message_t * msg = &(payload->msg);
		sem_t * char_lock;

		switch(msg->msgtype) {
			case DONATE:
				uint8_t charity_i = msg->msgdata.donation.charity;
				uint64_t amnt = msg->msgdata.donation.amount;
				if (charity_i >= 5 || charity_i < 0) {
					send_err_msg(logfile_fd, client_fd, &mutex_dlog, msg);
					break;
				}
				//get address of the lock for the requested charity
				char_lock = &mutex_charities[charity_i];
				//lock requested charity
				P(char_lock);
				//add donation to charity
				reterr = add_donation_to_charity(msg, charities, 5, maxDonations);
				//release requested charity
				V(char_lock);

				if (reterr == -1) {
					send_err_msg(logfile_fd, client_fd, &mutex_dlog, msg);
					break;
				}
			
				//send msg to client
				write(client_fd, msg, sizeof(message_t));

				//create log msg
				asprintf(&logmsg, "%lu %d DONATE %u %lu\n", self_tid, client_fd, charity_i, amnt);
				
				//lock log
				P(&mutex_dlog);
				//log file
				write(logfile_fd, logmsg, strlen(logmsg));
				//release log
				V(&mutex_dlog);
				free(logmsg);
			break;
			case CINFO:
				uint8_t req_charity_i = get_charity_info(msg);
				if (req_charity_i < 0 || req_charity_i > 4) {
					send_err_msg(logfile_fd, client_fd, &mutex_dlog, msg);
					break;
				}
				char_lock = &mutex_charities[req_charity_i];
				
				//lock requested charity
				P(char_lock);
				copy_charity(msg, &charities[req_charity_i]);
				//release requested charity
				V(char_lock);
				
				//send msg w/ charity info
				reterr= write(client_fd, msg, sizeof(message_t));
				if (reterr< 0) {
					//error, sending failed
					
				}
				//create log msg
				asprintf(&logmsg, "%d CINFO %u\n", client_fd, req_charity_i);
				
				//log file
				P(&mutex_dlog);
				write(logfile_fd, logmsg, strlen(logmsg));
				V(&mutex_dlog);
				
				free(logmsg);
			break;
			case TOP:
				//hold server lock
				P(&mutex_stat);
				stat_r_cnt ++;
				if (stat_r_cnt == 1) {
					P(&mutex_stat_w);
				}
				V(&mutex_stat);
				//read donation amounts
				write_max_donations(msg, maxDonations); //todo!!
				//release server lock
				P(&mutex_stat);
				stat_r_cnt --;
				if (stat_r_cnt == 0) {
					V(&mutex_stat_w);
				}
				V(&mutex_stat);
				//send msg
				write(client_fd, msg, sizeof(message_t));

				//create log msg
				asprintf(&logmsg, "%d TOP\n", client_fd);
				
				//log file
				P(&mutex_dlog);
				write(logfile_fd, logmsg, strlen(logmsg));
				V(&mutex_dlog);
				
				free(logmsg);
				break;
			break;
			default:
				msg->msgtype = ERROR;
				send_err_msg(logfile_fd, client_fd, &mutex_dlog, msg);
			break;
		}
		
	}
	return NULL;
}


void init_logfilefd(char * log_filename) {
	//Open log file
	logfile_fd = Open(log_filename, O_WRONLY | O_CREAT);
	Ftruncate(logfile_fd);
}

void init_mutexes(int slots_start) {
	//mutex initialization
	init_mutex(&mutex_stat);
	init_mutex(&mutex_stat_w);
	init_sem(&sem_job_slots, slots_start);
	init_sem(&sem_job_items, 0);
	init_mutex(&mutex_job);
	init_mutex(&mutex_dlog);
	for (int i = 0; i < 5; i++) {
		init_mutex(mutex_charities + i);	
	}
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


