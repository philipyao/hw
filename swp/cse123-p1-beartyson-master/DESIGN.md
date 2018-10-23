## Message Frame


```c
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
    char crc;   //crc appended                                                                                                                                                          
};                                                                                                                                                                                      
typedef struct Frame_t Frame;                                                                                                                                                           
#pragma pack()  
```

In the above Frame structure, header holds important fields:

* **flags**. I use the lowest bit in flags to indicate the message type, which should be data(0) or ack(1).
* **seq_no**. sequence number. If the message is from sender to receiver, this field make sense. when initialized, it is 1, and increases by 1 every time when a new data message be sent. since it's type is `unsigned char`, it will wrap to 0 automatically when reach 255.
* **ack_no**. acknowledgement number. Similar to seq_no, it make sense if the message is acknowledgement from receiver to sender.

* **from_id**. from id. Indicate the message source. By this id, I can locate the corresponding sliding window when processing the message.
* **tar_id**. target id. Indicate the message destination/target. Since the underlying communication is broadcast to each receiver or sender, I can use this id to check whether the message is really for some target.

The size of header is 11, and the last byte reserved for crc, so the size left for data is `64 - 11 - 1 = 52`. message length greater than 52 will be split to multiple messages.



## Sliding window

```c
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
```

I maintain one **SendWindow** respectively for each receiver in the Sender_t, which supports muliple receivers. In the SendWindow structure, LAR and LFS act as edge position of the send sliding window.

The **sendQ** will cache all frames sent to the receiver with a future timeout. In every loop I will check whether these frames are timeout or not. If timeout, the related frame will be retransmitted to the receiver. If the ack for the frame arrives, it will be removed from the cache. Since I implement the queue by circular array, it's easy to add or remove element by adding LAR or LFS.



## Main functions

##### ll_split_head_if_necessary

In `handle_input_cmds` procedure, I use `ll_split_head_if_necessary` to split message if the cmd message is longer than the payload size.

Before each pop of  the head, it should check the size, and split it up to multiple message just after the head.

##### calculate_timeout

Every time we send a message, we just cache it in the send queue, and attach a expire time with it, so that we can check the timeout if the message be droped or corrupted.

#####  append_crc

Before a frame be sent, a CRC value should be append to the final char* buffer. We use the last byte of Frame to store the CRC value so that the overall size of buffer would not exceed MAX_FRAME_SIZE.

