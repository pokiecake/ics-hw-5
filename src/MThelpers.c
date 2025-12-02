#include "server.h"
#include "protocol.h"
#include <pthread.h>
#include <signal.h>
#include "MThelpers.h"

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
