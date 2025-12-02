#ifndef HELPERS_H
#define HELPERS_H

#include "linkedlist.h"
#include <semaphore.h>

// INSERT FUNCTION DECLARATIONS HERE
list_t * init_T_List();
int t_list_comparator(const void * left, const void * right);

void t_list_printer(void * data, void * fp);

void t_list_deleter(void * data);
void join_threads(list_t * list);
void P(sem_t * lock);
void V(sem_t * lock);
void * thread (void *);

void add_donation_to_charity(message_t * msg);
charity_t * get_charity_info(message_t * msg);
void write_max_donations(message_t * msg, uint64_t maxDonations[3]);

#endif
