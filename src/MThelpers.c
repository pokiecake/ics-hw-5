#define _GNU_SOURCE
#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "MThelpers.h"
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>

//!!Remove during testing!!
//#define DEBUG

list_t * init_T_List() {
	list_t * list = CreateList(&t_list_comparator, &t_list_printer, &t_list_deleter);
	return list;	
}

int t_list_comparator(const void * left, const void * right) {
	pthread_t * tid1 = (pthread_t *)left;
	pthread_t * tid2 = (pthread_t *)right;
//add comparator
	return 0;
}

void t_list_printer(void * data, void * fp) {
	pthread_t * tid = (pthread_t *)data;
	fprintf(fp, "%ln", tid);
}

void t_list_deleter(void * data) {
	free(data);
}
void join_threads(list_t * list) {
	node_t * cur = list->head, * next;
	int i;
	for (i = 0; cur != NULL; i ++, cur = next) {
		next = cur->next;
		pthread_t tid = *((pthread_t *)(cur->data));
		int err = pthread_tryjoin_np(tid, NULL);
		if (err == 0) {
			RemoveByIndex(list, i);
			i--; //removing the element will modify the following positions by -1
		}
	}
}

void P(sem_t * lock) {
	sem_wait(lock);
}

void V(sem_t * lock) {
	sem_post(lock);
}

int add_donation_to_charity(message_t * msg, uint64_t * total_donor_amnt, charity_t charities[], uint8_t max_charities) {
	uint8_t charity_i = msg->msgdata.donation.charity;
	uint64_t amnt = msg->msgdata.donation.amount;
	if (charity_i >= max_charities || charity_i < 0) {
		return -1;
	}
	// if (charity_i > max_charities || charity_i < 0 || amnt < 0 || amnt > 255) {
	// 	return -1;
	// }
	charity_t * req_charity = charities + charity_i;	
	//Add donation to charity
	req_charity->totalDonationAmt += amnt;
	*total_donor_amnt += amnt;
	// if (amnt > *total_donor_amnt) {
	// 	*total_donor_amnt = amnt;
	// }
	//Check and change top donation
	if (req_charity->topDonation < amnt) {
		req_charity->topDonation = amnt;
	}
	//increment num of donations
	req_charity->numDonations++;
	// #ifdef DEBUG
	// 	printf("\t**Charity %d Stats**\t\n", charity_i);
	// 	printf("\tTotal amount: %lu\n", req_charity->totalDonationAmt);
	// 	printf("\tTop dono: %lu\n", req_charity->topDonation);
	// 	printf("\tNum donations: %u\n", req_charity->numDonations);
	// #endif
	return 0;
}

uint8_t get_charity_info(message_t * msg) {
	return msg->msgdata.donation.charity;
}

void copy_charity(message_t * msg, charity_t * charity) {
	charity_t * dest = &(msg->msgdata.charityInfo);
	dest->totalDonationAmt = charity->totalDonationAmt;
	dest->topDonation = charity->topDonation;
	dest->numDonations = charity->numDonations;
}

void write_max_donations(message_t * msg, uint64_t maxDonations[]) {
	uint64_t * dest = msg->msgdata.maxDonations;
	int i;
	for (i = 0; i < 3; i++) {
		dest[i] = maxDonations[i];
	}
}

void kill_all_threads(list_t * thread_list) {
	node_t * cur = thread_list->head;
	while(cur != NULL) {
		pthread_t tid = *((pthread_t *)(cur->data));
		pthread_kill(tid, SIGINT); //possible problems with dead threads
		pthread_join(tid, NULL);
		cur = cur->next;
	}
	DeleteList(thread_list);
}

void update_highest_dono(uint64_t dono, uint64_t max_donos[], int size) {
	uint64_t cur = dono;
	int i;
	for (i = 0; i < size; i++) {
		if (max_donos[i] < dono) {
			uint64_t temp_cur = cur;
			cur = max_donos[i];
			max_donos[i] = temp_cur;
		}
	}
}

void print_all_charities(charity_t charities[], uint32_t size) {
	uint32_t i;
	for (i = 0; i < size; i++) {
		charity_t * cur_charity = charities + i;
		fprintf(stdout, "%u, %u, %lu, %lu\n", i, cur_charity->numDonations, cur_charity->topDonation, cur_charity->totalDonationAmt);
	}
}

//assumes max donations is size 3
void print_statistics(int clientCnt, uint64_t maxDonations[]) {
	fprintf(stderr, "%d\n", clientCnt);
	int i;
	fprintf(stderr, "%lu, %lu, %lu\n", maxDonations[0], maxDonations[1], maxDonations[2]);
	
}

void send_err_msg(int log_fd, int client_fd, sem_t * mutex_dlog, message_t * msg) {
	msg->msgtype = ERROR;
	write(client_fd, msg, sizeof(message_t));
	#ifdef DEBUG
		printf("holding file lock");
	#endif
	P(mutex_dlog);
	#ifdef DEBUG
		printf("writing to file");
	#endif
	//char msg[] = "%d CINFO %u\n";
	char * log_msg;
	asprintf(&log_msg, "%d ERROR\n", client_fd);
	write(log_fd, log_msg, strlen(log_msg));
	//fprintf(log_fd, "%d CINFO %u\n", clientfd, req_charity_i);
	V(mutex_dlog);
	free(log_msg);
}

void Ftruncate(int log_file_fd) {
	if (ftruncate(log_file_fd, 0) == -1) {
		fprintf(stderr, "Failed to truncate log file\n");
		exit(1);
	}
}

void * Calloc(size_t nelem, size_t elsize) {
	void * ret = calloc(nelem, elsize);
	if (errno != 0) {
		fprintf(stderr, "ERROR: Calloc set errno to %d\n", errno);
		exit(1);
	}
	return ret;
}

void Sigaction(int signal, const struct sigaction * act) {
	if (sigaction(signal, act, NULL) == -1) {
		printf("signal handler failed to install\n");
		exit(1);
	}
}

// void ltostr(long num, char str[], int size) {
// 	int i;
// 	for (i = size - 1; i >= 0; --i) {
// 		int digit = num % 10;
// 		str[i] = '0' + digit;
// 		num /= 10;
// 	}
// }

// void log_msg(int log_fd, const char msg[]) {
// 	fprintf(log_fd, msg);
// }
