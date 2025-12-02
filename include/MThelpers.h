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

void add_donation_to_charity(message_t * msg, uint64_t * total_donor_amnt);
uint8_t get_charity_info(message_t * msg);
void copy_charity(message_t * msg, charity_t * charity);
void write_max_donations(message_t * msg, uint64_t maxDonations[]);
void join_all_threads(list_t * thread_list);
void update_highest_dono(uint64_t dono, uint64_t max_donos[], int size);
//void ltostr(long num, char str[]);
//void log_msg(int log_fd, const char msg[]);
#endif
