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
void add_donation_to_charity(message_t * msg) {
	uint8_t charity_i = msg->msgdata.donation.charity;
	uint64_t amnt = msg->msgdata.donation.amount;
	charity_t * req_charity = &charities[charity_i];	
	//Add donation to charity
	req_charity->totalDonationAmt += amnt;
	//Check and change top donation
	if (req_charity->topDonation < amnt) {
		req_charity->topDonation = amnt;
	}
	//increment num of donations
	req_charity->numDonations ++;
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
