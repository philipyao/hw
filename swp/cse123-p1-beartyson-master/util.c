#include "util.h"

char get_bit(char byte, int pos) 
{
	if (pos >= 0 && pos < 8) {
		return (byte >> pos) & 0x01;
	}
	return 0;
}

char crc8(char * array, int array_len) 
{
	char poly = 0x07;	//00000111
	char crc = array[0];
    int i, j;
	for (i = 1; i < array_len; ++i) {
		char next_byte = array[i];
		for (j = 7; j >= 0; --j) {
			if (get_bit(crc, 7) == 0) {
				crc = crc << 1;
				crc = crc | get_bit(next_byte, j);
			} else {
				crc = crc << 1;
				crc = crc | get_bit(next_byte, j);
				crc = crc ^ poly;
			}
		}
	}
	return crc;
}

//Linked list functions
int ll_get_length(LLnode * head)
{
    LLnode * tmp;
    int count = 1;
    if (head == NULL)
        return 0;
    else
    {
        tmp = head->next;
        while (tmp != head)
        {
            count++;
            tmp = tmp->next;
        }
        return count;
    }
}

void ll_append_node(LLnode ** head_ptr, 
                    void * value)
{
    LLnode * prev_last_node;
    LLnode * new_node;
    LLnode * head;

    if (head_ptr == NULL)
    {
        return;
    }
    
    //Init the value pntr
    head = (*head_ptr);
    new_node = (LLnode *) malloc(sizeof(LLnode));
    new_node->value = value;

    //The list is empty, no node is currently present
    if (head == NULL)
    {
        (*head_ptr) = new_node;
        new_node->prev = new_node;
        new_node->next = new_node;
    }
    else
    {
        //Node exists by itself
        prev_last_node = head->prev;
        head->prev = new_node;
        prev_last_node->next = new_node;
        new_node->next = head;
        new_node->prev = prev_last_node;
    }
}


LLnode * ll_pop_node(LLnode ** head_ptr)
{
    LLnode * last_node;
    LLnode * new_head;
    LLnode * prev_head;

    prev_head = (*head_ptr);
    if (prev_head == NULL)
    {
        return NULL;
    }
    last_node = prev_head->prev;
    new_head = prev_head->next;

    //We are about to set the head ptr to nothing because there is only one thing in list
    if (last_node == prev_head)
    {
        (*head_ptr) = NULL;
        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
    else
    {
        (*head_ptr) = new_head;
        last_node->next = new_head;
        new_head->prev = last_node;

        prev_head->next = NULL;
        prev_head->prev = NULL;
        return prev_head;
    }
}

void ll_destroy_node(LLnode * node)
{
    if (node->type == llt_string)
    {
        free((char *) node->value);
    }
    free(node);
}

void ll_split_head_if_necessary(LLnode ** head_ptr, size_t cut_size) {
	if (head_ptr == NULL) {
		return;
	}
	LLnode * head = *head_ptr;
	if (head == NULL) {
		return;
	}
	Cmd * head_cmd = (Cmd *)head->value;
	char * msg = head_cmd->message;
	if (strlen(msg) < cut_size) {
		return;
	}

	size_t i;
	LLnode *curr, *next;
	Cmd *next_cmd;
	curr = head;
	for (i = cut_size; i < strlen(msg); i+= cut_size) {
		next_cmd = (Cmd *)malloc(sizeof(Cmd));
		//one extra byte for NULL character
		char * cmd_msg = (char *)malloc((cut_size + 1) * sizeof(char));
		memset(cmd_msg, 0, (cut_size + 1) * sizeof(char));
		strncpy(cmd_msg, msg + i, cut_size);
        next_cmd->src_id = head_cmd->src_id;
        next_cmd->dst_id = head_cmd->dst_id;
        next_cmd->message = cmd_msg;

		next = (LLnode *)malloc(sizeof(LLnode));
		next->value = (void *)next_cmd;
		//put the node just after current node
		next->prev = curr;
		next->next = curr->next;
		curr->next = next;
		next->next->prev = next;

		curr = next;
	}
	msg[cut_size] = '\0';
}

void calculate_timeout(struct timeval* timeout) {
	gettimeofday(timeout,NULL); 
	timeout->tv_usec += 100000;	//add 0.1s
	if (timeout->tv_usec >= 1000000) {
		timeout->tv_usec -= 1000000;
		timeout->tv_sec += 1; 
	}
}

//Compute the difference in usec for two timeval objects
long timeval_usecdiff(struct timeval *start_time, 
                      struct timeval *finish_time)
{
  long usec;
  usec=(finish_time->tv_sec - start_time->tv_sec)*1000000;
  usec+=(finish_time->tv_usec- start_time->tv_usec);
  return usec;
}


//Print out messages entered by the user
void print_cmd(Cmd * cmd)
{
    fprintf(stderr, "src=%d, dst=%d, message=%s\n", 
           cmd->src_id,
           cmd->dst_id,
           cmd->message);
}


char * convert_frame_to_char(Frame * frame)
{
	//one more byte for appending crc
	//and initialize it to zero (the last byte)
    char * char_buffer = (char *) malloc(MAX_FRAME_SIZE);
    memset(char_buffer,
           0,
           MAX_FRAME_SIZE);
	//copy header
	memcpy(char_buffer, 
		   &frame->header, 
           sizeof(frame->header));
	//copy payload
    memcpy(char_buffer + sizeof(frame->header), 
           frame->data,
           FRAME_PAYLOAD_SIZE);
    return char_buffer;
}


Frame * convert_char_to_frame(char * char_buf)
{
    Frame * frame = (Frame *) malloc(sizeof(Frame));
    memset(frame->data,
           0,
           sizeof(char)*sizeof(frame->data));
    memcpy(&frame->header, 
		   char_buf,
		   sizeof(frame->header));
    memcpy(frame->data, 
           char_buf + sizeof(frame->header),
           sizeof(char)*sizeof(frame->data));
    return frame;
}


void append_crc(char * array, int array_len) 
{
	char crc = crc8(array, array_len - 1);
	array[array_len - 1] = crc;
}

int is_corrupted(char * array, int array_len) 
{
	char crc = crc8(array, array_len - 1);
	if (crc != array[array_len - 1]) {
		return 1;
	}
	return 0;
}
