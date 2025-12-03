#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "RWhelpers.h"

void init_mutex(sem_t * mutex) {
	if (sem_init(mutex, 0, 1) == -1) {
		fprintf(stderr, "ERROR: failed to initialize mutex\n");
		exit(1);
	}
}

void P(sem_t * lock) {
	if (sem_wait(lock) == -1) {
		fprintf(stderr, "ERROR: failed to hold onto lock\n");
		exit(1);
	}
}

void V(sem_t * lock) {
	if (sem_post(lock) == -1) {
		fprintf(stderr, "ERROR: failed to release lock\n");
		exit(1);
	}
}

void Ftruncate(int log_file_fd) {
	if (ftruncate(log_file_fd, 0) == -1) {
		fprintf(stderr, "ERROR: Failed to truncate log file\n");
		exit(1);
	}
}
//wrapper for open
int Open(char * log_filename, int flags) {
	int log_file_fd = open(log_filename, flags);
	//O_RDWR produces bugs??? (blocks file form RDWR)
	if (log_file_fd == -1) {
		fprintf(stderr, "File could not open\n");
		exit(2);
	}
	return log_file_fd;
}

void Sigaction(int signal, const struct sigaction * act) {
	if (sigaction(signal, act, NULL) == -1) {
		fprintf(stderr, "signal handler failed to install\n");
		exit(1);
	}
}

void Pthread_create(pthread_t * tid, const pthread_attr_t *__restrict attr, void * (start_routine)(void *) , void * arg) {
	if (pthread_create(tid, attr, start_routine, arg) == -1) {
		fprintf(stderr, "thread failed to create\n");
		exit(1);
	}
}

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