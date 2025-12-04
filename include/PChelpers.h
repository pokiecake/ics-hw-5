#ifndef HELPERS_H
#define HELPERS_H

#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>
#include "linkedlist.h"
// INSERT FUNCTION DECLARATIONS HERE

int add_donation_to_charity(message_t * msg, charity_t charities[], uint8_t max_charities);
void join_threads(list_t * list);
uint8_t get_charity_info(message_t * msg);
void copy_charity(message_t * msg, charity_t * charity);
void write_max_donations(message_t * msg, uint64_t maxDonations[]);
void update_high_low_charities(message_t * msg, charity_t charities[], size_t);
void set_high_low_charities(message_t * msg, uint64_t high, uint64_t low, uint8_t high_ci, uint8_t low_ci);

void kill_all_threads(list_t * thread_list, pthread_t writer_tid);
void update_highest_dono(uint64_t dono, uint64_t max_donos[], int size);
void print_all_charities(charity_t charities[], uint32_t size);

//assumes max donations is size 3
void print_statistics(int clientCnt, uint64_t maxDonations[]);

void send_err_msg(int log_fd, int client_fd, sem_t * mutex_dlog, message_t * msg);

/* Initializes a binary mutex lock
 * @param mutex a pointer to a sem_t mutex
 */
void init_mutex(sem_t * mutex);

void P(sem_t * lock);
void V(sem_t * lock);
void Ftruncate(int log_file_fd);
int Open(char * log_filename, int flags);
//no need for old action for our purposes
void Sigaction(int signal, const struct sigaction * act);
void Pthread_create(pthread_t * tid, const pthread_attr_t *__restrict attr, void * (start_routine)(void *) , void * arg);
//list t functions
list_t * init_T_List();
int t_list_comparator(const void * left, const void * right);
void t_list_printer(void * data, void * fp);
void t_list_deleter(void * data);
void * Calloc(size_t nelem, size_t elsize);

#endif