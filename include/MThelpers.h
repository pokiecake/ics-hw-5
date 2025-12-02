#ifndef HELPERS_H
#define HELPERS_H

#include "linkedlist.h"

// INSERT FUNCTION DECLARATIONS HERE
list_t * init_T_List();
int t_list_comparator(const void * left, const void * right);

void t_list_printer(void * data, void * fp);

void t_list_deleter(void * data);
#endif
