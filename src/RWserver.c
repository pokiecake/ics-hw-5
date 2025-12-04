#define _GNU_SOURCE
#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "RWhelpers.h"
#include "linkedlist.h"

/**********************DECLARE ALL LOCKS HERE BETWEEN THES LINES FOR MANUAL GRADING*************/
sem_t mutex_stat, mutex_stat_w, mutex_char, mutex_char_w, mutex_dlog;
int stat_r_cnt, char_r_cnt;
/*
mutex_stat = readers lock for server statistsics
mutex_stat_w = writers lock for server statistics
mutex_char = readers lock for charity data structs
mutex_char_w = writers lock for charity data structs
mutex_dlog = mutex lock for log file

stat_r_cnt = readers count for server statistics
char_r_cnt = readers count for charity data structs
*/

/***********************************************************************************************/

// Global variables, statistics collected since server start-up
int clientCnt;  // # of client connections made, Updated by the main thread
uint64_t maxDonations[3];  // 3 highest total donations amounts (sum of all donations to all  
                           // charities in one connection), updated by client threads
                           // index 0 is the highest total donation
charity_t charities[5]; // Global variable, one charity per index

volatile int sigintflag = 0;

void sigint_handler(int sig) {
	//P(&mutex_sigint);
	sigintflag = 1;
	//V(&mutex_sigint);
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
	int logfilefd = Open(log_filename, O_WRONLY | O_CREAT);
	Ftruncate(logfilefd);
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
	int * writer_fds = malloc(sizeof(int) * 2);
	writer_fds[0] = logfilefd;
	writer_fds[1] = socket_listen_init(w_port_number);
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
		while (reader_fd < 0 && sigintflag == 0) {
			reader_fd = accept(reader_listen_fd, (SA*)&client_addr, &client_addr_len);
		}
		if (sigintflag == 1) {
			kill_all_threads(thread_list, writer_tid);
			break;
		}
		//join all threads on sig int
        if (reader_fd < 0) {
            printf("server acccept failed\n");
            exit(EXIT_FAILURE);
        }
        
        // INSERT SERVER ACTIONS FOR CONNECTED READER CLIENT CODE HERE
		clientCnt++;
		//check for terminated threads and join
		join_threads(thread_list);
		//spawn new reader thread
		pthread_t reader_tid;
		//give file descriptors as payload
		int * fds = malloc(sizeof(int) * 2);
		fds[0] = reader_fd;
		fds[1] = logfilefd;
		Pthread_create(&reader_tid, NULL, reader_thread, fds);
		//insert into list
		InsertAtHead(thread_list, &reader_thread);
    }
	print_statistics(clientCnt, maxDonations);
	print_all_charities(charities, 5);

	close(logfilefd);
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
	int listenfd = fds[1];
	free(arg);
	//char * logmsg = "Writer thread initialized\n";
	//write(logfilefd, logmsg, strlen(logmsg));
	message_t * msg = Calloc(1, sizeof(message_t));
	char * logmsg;
	uint64_t total_amnt = 0;
	struct sockaddr_in client_addr;
	unsigned int client_addr_len = sizeof(client_addr);
	int reterr;
	while (1) {
		int clientfd = accept(listenfd, (SA *)&client_addr, &client_addr_len);
		while(clientfd == -1 && sigintflag == 0) {
			clientfd = accept(listenfd, (SA *)&client_addr, &client_addr_len);
		}
		if (sigintflag == 1) {
			if (clientfd != -1) {
				close(clientfd);
			}
			break;
		}
		int logout = 0;
		while (logout == 0) {
			read(clientfd, msg, sizeof(message_t));
			while (errno == EINTR && sigintflag == 0) {
				read(clientfd, msg, sizeof(message_t));
			}
			if (errno == EINTR && sigintflag == 1) {
				close(clientfd);
				break;
			}
			uint8_t msgtype = msg->msgtype;
				
			switch(msgtype) {
				case DONATE:
					//lock charities
					P(&mutex_char_w);
					//add donation to charity
					reterr = add_donation_to_charity(msg, charities, 5); //TODO
					//release charities
					V(&mutex_char_w);
	
					if (reterr == -1) {
						
					}
					//add to local total
					total_amnt += msg->msgdata.donation.amount;
					//send msg to client
					write(clientfd, msg, sizeof(message_t));
					//lock log
					P(&mutex_dlog);
					//log file
					uint8_t charity_i = msg->msgdata.donation.charity;
					uint64_t amnt = msg->msgdata.donation.amount;
					asprintf(&logmsg, "%d DONATE %u %lu\n", clientfd, charity_i, amnt); //not thread safe?
					write(logfilefd, logmsg, strlen(logmsg));
					//release log
					V(&mutex_dlog);
					free(logmsg);
				break;
				case LOGOUT:
					close(clientfd);
					
					P(&mutex_stat_w);
					if (total_amnt > maxDonations[2]) {
						update_highest_dono(total_amnt, maxDonations, 3); //TODO
					}	
					V(&mutex_stat_w);
	
					P(&mutex_dlog);
					asprintf(&logmsg, "%d LOGOUT\n", clientfd);
					write(logfilefd, logmsg, strlen(logmsg));
					V(&mutex_dlog);
					free(logmsg);
					logout = 1;
				break;
				default:
					send_err_msg(logfilefd, clientfd, &mutex_dlog, msg);
					// write(clientfd, msg, sizeof(message_t));
					// P(&mutex_dlog);
					// asprintf(&logmsg, "%d ERROR\n", clientfd);
					// write(logfilefd, logmsg, sizeof(logmsg));
					// V(&mutex_dlog);
					// free(logmsg);
				break;
				}
		}
		if (sigintflag == 1) {
			break;
		}
	}
	free(msg);		
	close(listenfd);
	return NULL;
}

void * reader_thread(void * arg) {
	int * fds = (int *)arg;
	int clientfd = fds[0];
	int logfilefd = fds[1];
	free(arg);
	//char * logmsg = "Reader thread initialized\n";
	//write(logfilefd, logmsg, strlen(logmsg));
	message_t * msg = Calloc(1, sizeof(message_t));
	char * logmsg;
	int logout = 0;
	while (logout == 0) {
		int reterr;
		//get msg from client
		read(clientfd, msg, sizeof(message_t));

		//check sigint signal after block
		while (errno == EINTR && sigintflag == 0) {
			reterr = read(clientfd, msg, sizeof(message_t));
		}
		if (errno == EINTR && sigintflag == 1) {
			if (clientfd == -1) {
				close(clientfd);
			}
			break;
		}
		uint8_t msgtype = msg->msgtype;
		
		switch(msgtype) {
			case CINFO:
				//read data into buffer
				uint8_t req_charity_i = get_charity_info(msg);
				//hold charity lock
				P(&mutex_char);
				char_r_cnt ++;
				if (char_r_cnt == 1) {
					P(&mutex_char_w);
				}
				V(&mutex_char);
				//charity_t * req_charity = charities + req_charity_i;
				msg->msgdata.charityInfo = charities[req_charity_i];
				//release lock
				P(&mutex_char);
				char_r_cnt --;
				if (char_r_cnt == 0) {
					V(&mutex_char_w);
				}
				V(&mutex_char);
				//send msg w/ charity info
				reterr= write(clientfd, msg, sizeof(message_t));
				if (reterr< 0) {
					//error, sending failed
					
				}
				//log file
				P(&mutex_dlog);
				//char msg[] = "%d CINFO %u\n";
				asprintf(&logmsg, "%d CINFO %u\n", clientfd, req_charity_i);
				write(logfilefd, logmsg, strlen(logmsg));
				//fprintf(log_fd, "%d CINFO %u\n", clientfd, req_charity_i);
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
				write(clientfd, msg, sizeof(message_t));
				
				//hold log lock
				P(&mutex_dlog);
				//log file
				asprintf(&logmsg, "%d TOP\n", clientfd);
				write(logfilefd, logmsg, strlen(logmsg));
				//release lock
				V(&mutex_dlog);
				//free asprintf buffer
				free(logmsg);
			break;
			case LOGOUT:
				//close connection
				close(clientfd);

				//hold log lock
				P(&mutex_dlog);
				//log file
				asprintf(&logmsg, "%d LOGOUT\n", clientfd);
				write(logfilefd, logmsg, strlen(logmsg));
				//release lock
				V(&mutex_dlog);
				//free asprintf buffer
				free(logmsg);
				//set logout flag
				logout = 1;
			break;
			case STATS:
				P(&mutex_char);
				char_r_cnt++;
				if (char_r_cnt == 1) {
					P(&mutex_char_w);
				}
				V(&mutex_char);
				update_high_low_charities(msg, charities, 5);
				P(&mutex_char);
				char_r_cnt--;
				if (char_r_cnt == 0) {
					V(&mutex_char_w);
				}
				V(&mutex_char);
				
				write(clientfd, msg, sizeof(message_t));

				//create log msg
				asprintf(&logmsg, "%d STATS %u:%lu %u:%lu\n", clientfd, 
					msg->msgdata.stats.charityID_high,
					msg->msgdata.stats.amount_high,
					msg->msgdata.stats.charityID_low,
					msg->msgdata.stats.amount_low);
				//hold log lock
				P(&mutex_dlog);
				//log file
				write(logfilefd, logmsg, strlen(logmsg));
				//release lock
				V(&mutex_dlog);
			break;
			default: //ERROR
				send_err_msg(logfilefd, clientfd, &mutex_dlog, msg);
			break;
		}
		if (sigintflag == 1) {
			break;
		}
	}

	free(msg);

	return NULL;
}


