#ifndef __UTIL_H__
#define __UTIL_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <unistd.h>
#include <netdb.h>
#include <math.h>
#include <sys/time.h>
#include "common.h"

//Linked list functions
int ll_get_length(LLnode *);
void ll_append_node(LLnode **, void *);
LLnode * ll_pop_node(LLnode **);
void ll_destroy_node(LLnode *);
void ll_split_head_if_necessary(LLnode ** head_ptr, size_t cut_size);

//Print functions
void print_cmd(Cmd *);

//Calculate default timeout time
void calculate_timeout(struct timeval * timeout);
//Time functions
long timeval_usecdiff(struct timeval *, 
                      struct timeval *);

char * convert_frame_to_char(Frame *);
Frame * convert_char_to_frame(char *);

// crc functions
void append_crc(char * array, int array_len);
int is_corrupted(char * array, int array_len);

#endif
