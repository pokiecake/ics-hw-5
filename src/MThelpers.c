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

//!!Remove during testing!!
#define DEBUG

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
	node_t * cur = list->head;
	int i;
	for (i = 0; cur != NULL; i ++, cur = cur->next) {
		int err = pthread_tryjoin_np((pthread_t)(cur->data), NULL);
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
extern charity_t charities[5];
void add_donation_to_charity(message_t * msg, uint64_t * total_donor_amnt) {
	uint8_t charity_i = msg->msgdata.donation.charity;
	uint64_t amnt = msg->msgdata.donation.amount;
	charity_t * req_charity = charities + charity_i;	
	//Add donation to charity
	req_charity->totalDonationAmt += amnt;
	*total_donor_amnt += amnt;
	//Check and change top donation
	if (req_charity->topDonation < amnt) {
		req_charity->topDonation = amnt;
	}
	//increment num of donations
	req_charity->numDonations++;
	#ifdef DEBUG
		printf("\t**Charity %d Stats**\t\n", charity_i);
		printf("\tTotal amount: %lu\n", req_charity->totalDonationAmt);
		printf("\tTop dono: %lu\n", req_charity->topDonation);
		printf("\tNum donations: %u\n", req_charity->numDonations);
	#endif
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

void join_all_threads(list_t * thread_list) {
	node_t * cur = thread_list->head;
	while(cur != NULL) {
		pthread_join(*((pthread_t *)(cur->data)), NULL);
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
