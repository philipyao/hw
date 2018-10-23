#ifndef __COMMON_H__
#define __COMMON_H__

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

#define MAX_COMMAND_LENGTH 16
#define AUTOMATED_FILENAME 512
#define SWS 8
#define RWS 8
typedef unsigned char uchar_t;

//System configuration information
struct SysConfig_t
{
    float drop_prob;
    float corrupt_prob;
    unsigned char automated;
    char automated_file[AUTOMATED_FILENAME];
};
typedef struct SysConfig_t  SysConfig;

//Command line input information
struct Cmd_t
{
    uint16_t src_id;
    uint16_t dst_id;
    char * message;
};
typedef struct Cmd_t Cmd;

//Linked list information
enum LLtype 
{
    llt_string,
    llt_frame,
    llt_integer,
    llt_head
} LLtype;

struct LLnode_t
{
    struct LLnode_t * prev;
    struct LLnode_t * next;
    enum LLtype type;

    void * value;
};
typedef struct LLnode_t LLnode;

#define MSG_TYPE_FLAG_ACK 0x01			// 00000001
#define MSG_TYPE_FLAG_DATA 0x02			// 00000010

//Remember, your frame can be AT MOST 64 bytes!
//64 - sizeof(header) - sizeof(crc) = 52
//64 -  11            -  1          = 52
#define FRAME_PAYLOAD_SIZE 52

//header size: 11
#pragma pack(1)
struct Header_t
{
	// frame type: data message or ack message
	unsigned char flags;

	unsigned char seq_no;
	unsigned char ack_no;

	// from id: from which sender or receiver
	int from_id;
	// target id: the corresponding recv_id/send_id the message is intended for
    int tar_id;	
};
typedef struct Header_t Header;

struct Frame_t
{
	Header header;
    char data[FRAME_PAYLOAD_SIZE];
	char crc;	//crc appended
};
typedef struct Frame_t Frame;
#pragma pack()


typedef struct RecvWindow_t {
	//recv queue (based on circular array)
	struct recvQ_slot {
		int received;
		struct Frame_t * frame;
	} recvQ[RWS];

	//next frame expected
	unsigned char nfe;
} RecvWindow;

//Receiver and sender data structures
struct Receiver_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_framelist_head
    // 4) recv_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;
    LLnode * input_framelist_head;
    
    int recv_id;

	//send window for every sender
	RecvWindow * recv_window_array;
};


typedef struct SendWindow_t {
	//send queue (based on circular array)
	struct sendQ_slot {
		//event associate with send timeout
		struct timeval *timeout;	
		struct Frame_t *frame;
	} sendQ[SWS];

	//last acknowledgement received
	unsigned char lar;
	//last frame sent
	unsigned char lfs;
} SendWindow;

struct Sender_t
{
    //DO NOT CHANGE:
    // 1) buffer_mutex
    // 2) buffer_cv
    // 3) input_cmdlist_head
    // 4) input_framelist_head
    // 5) send_id
    pthread_mutex_t buffer_mutex;
    pthread_cond_t buffer_cv;    
    LLnode * input_cmdlist_head;
    LLnode * input_framelist_head;
    int send_id;

	//send window for every receiver
	SendWindow * send_window_array;
};

enum SendFrame_DstType 
{
    ReceiverDst,
    SenderDst
} SendFrame_DstType ;

typedef struct Sender_t Sender;
typedef struct Receiver_t Receiver;


#define MAX_FRAME_SIZE 64

//Declare global variables here
//DO NOT CHANGE: 
//   1) glb_senders_array
//   2) glb_receivers_array
//   3) glb_senders_array_length
//   4) glb_receivers_array_length
//   5) glb_sysconfig
//   6) CORRUPTION_BITS
Sender * glb_senders_array;
Receiver * glb_receivers_array;
int glb_senders_array_length;
int glb_receivers_array_length;
SysConfig glb_sysconfig;
int CORRUPTION_BITS;
#endif 
