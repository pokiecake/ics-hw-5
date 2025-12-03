#ifndef HELPERS_H
#define HELPERS_H
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include "linkedlist.h"
// INSERT FUNCTION DECLARATIONS HERE

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
#endif