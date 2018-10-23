#include "sender.h"

void init_sender(Sender * sender, int id)
{
    sender->send_id = id;
    sender->input_cmdlist_head = NULL;
    sender->input_framelist_head = NULL;
	//malloc one window for each receiver
	sender->send_window_array = malloc(sizeof(SendWindow) *  glb_receivers_array_length); 
	int i;
	for (i = 0; i < glb_receivers_array_length; ++i) {
		SendWindow * wnd = &sender->send_window_array[i];
		wnd->lar = 0;
		wnd->lfs = 0;
	}
}

struct timeval * sender_get_next_expiring_timeval(Sender * sender)
{
    // You should fill in this function so that it returns the next timeout that should occur
    return NULL;
}


void handle_incoming_acks(Sender * sender,
                          LLnode ** outgoing_frames_head_ptr)
{
    // Suggested steps for handling incoming ACKs
    //    1) Dequeue the ACK from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this sender
    //    5) Do sliding window protocol for sender/receiver pair   

	int from_id;

    int incoming_acks_length = ll_get_length(sender->input_framelist_head);
    while (incoming_acks_length > 0)
    {
        //Pop a node off the front of the link list and update the count
        LLnode * ll_inack_node = ll_pop_node(&sender->input_framelist_head);
        incoming_acks_length = ll_get_length(sender->input_framelist_head);

        //      Ask yourself: Is this message really for me?
        //                    Is this message corrupted?
        //                    Is this an old, retransmitted message?           
        char * raw_char_buf = (char *) ll_inack_node->value;

		//the check length = sizeof(Frame)
		if (is_corrupted(raw_char_buf, sizeof(Frame)) > 0) {
			fprintf(stderr, "corrupt ack, drop it\n");
			//drop the message
        	free(raw_char_buf);
        	free(ll_inack_node);
			continue;
		}
        Frame * inframe = convert_char_to_frame(raw_char_buf);

        //Free raw_char_buf
        free(raw_char_buf);

		//check ack type
		if ((inframe->header.flags & MSG_TYPE_FLAG_ACK) == 0) {
			fprintf(stderr, "invalid header flags: %d, drop it\n", inframe->header.flags);
			//drop the message
        	free(ll_inack_node);
			continue;
		}

		//ack frame not for this sender, just ignore
		if (inframe->header.tar_id != sender->send_id) { 
			continue; 
		}

        
        from_id = inframe->header.from_id,
        fprintf(stderr, "<ACK_%d>: [%d]\n", sender->send_id, inframe->header.ack_no);

        if (from_id < 0 || from_id >= glb_receivers_array_length) {
			fprintf(stderr, "ERR: invalid from_id: %d\n", from_id);
        	free(ll_inack_node);
			continue;
		}
 	    SendWindow * wnd = &sender->send_window_array[from_id];
		unsigned char ack_no = inframe->header.ack_no;
		if ((wnd->lar <= wnd->lfs && ack_no > wnd->lar && ack_no <= wnd->lfs) || 
			(wnd->lar > wnd->lfs && (ack_no > wnd->lar || ack_no <= wnd->lfs))) {
			// slide the LAR to ack_no
			struct sendQ_slot *slot;
			do {
				++wnd->lar;
				slot = &wnd->sendQ[wnd->lar % SWS];
				free(slot->timeout);
				slot->timeout = NULL;
				free(slot->frame);
				slot->frame = NULL;
			} while(wnd->lar != ack_no);
			fprintf(stderr, "window updated: LAF=%d, LFS=%d\n", wnd->lar, wnd->lfs);
		} 

        free(ll_inack_node);
    }
}

int check_window_full(SendWindow * wnd) {
    if (wnd->lfs >= wnd->lar) {
		return ((wnd->lfs - wnd->lar) >= SWS) ? 1 : 0;
	} else {
		return ((256 - (wnd->lar - wnd->lfs)) >= SWS) ? 1 : 0;
	}
}

void handle_input_cmds(Sender * sender,
                       LLnode ** outgoing_frames_head_ptr)
{
    // Suggested steps for handling input cmd
    //    1) Dequeue the Cmd from sender->input_cmdlist_head
    //    2) Convert to Frame
    //    3) Set up the frame according to the sliding window protocol
    //    4) Compute CRC and add CRC to Frame

    int input_cmd_length = ll_get_length(sender->input_cmdlist_head);

	struct sendQ_slot * slot;

    //Recheck the command queue length to see if stdin_thread dumped a command on us
    input_cmd_length = ll_get_length(sender->input_cmdlist_head);
	if (input_cmd_length > 0) {
		//fprintf(stderr, "input_cmd_length: %d\n", input_cmd_length);
	}
    while (input_cmd_length > 0)
    {
		ll_split_head_if_necessary(&sender->input_cmdlist_head, FRAME_PAYLOAD_SIZE);
    	input_cmd_length = ll_get_length(sender->input_cmdlist_head);
		//fprintf(stderr, "after split: input_cmd_length: %d\n", input_cmd_length);

        //Pop a node off and update the input_cmd_length
        LLnode * ll_input_cmd_node = ll_pop_node(&sender->input_cmdlist_head);
        input_cmd_length = ll_get_length(sender->input_cmdlist_head);

        //Cast to Cmd type and free up the memory for the node
        Cmd * outgoing_cmd = (Cmd *) ll_input_cmd_node->value;
        free(ll_input_cmd_node);
            
		//fprintf(stderr, "after pop: input_cmd_length: %d, msg: %s\n", input_cmd_length, outgoing_cmd->message);

        //DUMMY CODE: Add the raw char buf to the outgoing_frames list
        //NOTE: You should not blindly send this message out!
        //      Ask yourself: Is this message actually going to the right receiver (recall that default behavior of send is to broadcast to all receivers)?
        //                    Does the receiver have enough space in in it's input queue to handle this message?
        //                    Were the previous messages sent to this receiver ACTUALLY delivered to the receiver?
        int msg_length = strlen(outgoing_cmd->message);
        if (msg_length > FRAME_PAYLOAD_SIZE)
        {
            //Do something about messages that exceed the frame size
            printf("<SEND_%d>: sending messages of length greater than %d is not implemented\n", sender->send_id, FRAME_PAYLOAD_SIZE);
        }
        else
        {
			//get the destination receiver "send window"
			uint16_t recv_id = outgoing_cmd->dst_id;
			if (recv_id >= (uint16_t)glb_receivers_array_length) {
				fprintf(stderr, "ERR: invalid recv_id %d\n", recv_id);
				continue;
			}
 	        SendWindow * wnd = &sender->send_window_array[recv_id];
			//check whether have enough space to put the frame in the send queue
			//SHOULD buffer less than 8 messages
            if (check_window_full(wnd) > 0) {
				fprintf(stderr, "send window for full, exit\n");
				break;
			}

            //This is probably ONLY one step you want
            Frame * outgoing_frame = (Frame *) malloc (sizeof(Frame));
			outgoing_frame->header.flags |= MSG_TYPE_FLAG_DATA;
			outgoing_frame->header.seq_no = ++wnd->lfs;
			outgoing_frame->header.from_id = sender->send_id;
			outgoing_frame->header.tar_id = (int)recv_id;
            strcpy(outgoing_frame->data, outgoing_cmd->message);
			fprintf(stderr, "send msg: seqno %d, msg %s\n", outgoing_frame->header.seq_no, outgoing_cmd->message);

			//cache the frame in the queue
            slot = &wnd->sendQ[outgoing_frame->header.seq_no % SWS];
			slot->frame = outgoing_frame;

    		struct timeval * ack_timeout = (struct timeval *)malloc(sizeof(struct timeval));
			calculate_timeout(ack_timeout);
            slot->timeout = ack_timeout;
       		//Convert the message to the outgoing_charbuf and add to sending list
       		char * outgoing_charbuf = convert_frame_to_char(outgoing_frame);
			//calc CRC
			append_crc(outgoing_charbuf, sizeof(Frame));
       		ll_append_node(outgoing_frames_head_ptr,
                           outgoing_charbuf);

            //At this point, we don't need the outgoing_cmd
            free(outgoing_cmd->message);
            free(outgoing_cmd);
        }
    }   
}


void handle_timedout_frames(Sender * sender,
                            LLnode ** outgoing_frames_head_ptr)
{
    // Suggested steps for handling timed out datagrams
    //    1) Iterate through the sliding window protocol information you maintain for each receiver
    //    2) Locate frames that are timed out and add them to the outgoing frames
    //    3) Update the next timeout field on the outgoing frames

	int i;
	unsigned char j;
    struct timeval    curr_timeval;
    for (i = 0; i < glb_receivers_array_length; ++i) {
        //get each receiver window
 	    SendWindow * wnd = &sender->send_window_array[i];
        gettimeofday(&curr_timeval, 
                     NULL);

        //re-transmit timeout frames
		if (wnd->lar == wnd->lfs) { continue; }

		unsigned char end = wnd->lfs + 1;
	    for (j = wnd->lar + 1; j != end; ++j) {
        	struct sendQ_slot * slot = &wnd->sendQ[j % SWS];
           	long diff = timeval_usecdiff(&curr_timeval, slot->timeout);
			if (diff <= 0) {
                fprintf(stderr, "timeout, retransmit msg: seqno %d, data %s\n", 
						slot->frame->header.seq_no, slot->frame->data);
				//add to sending list
       			char * outgoing_charbuf = convert_frame_to_char(slot->frame);
				append_crc(outgoing_charbuf, sizeof(Frame));
       			ll_append_node(outgoing_frames_head_ptr,
               				   outgoing_charbuf);
				//update new timeout
    			struct timeval * ack_timeout = (struct timeval *)malloc(sizeof(struct timeval));
				calculate_timeout(ack_timeout);
            	slot->timeout = ack_timeout;
			}
		}
	}
}


void * run_sender(void * input_sender)
{    
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Sender * sender = (Sender *) input_sender;    
    LLnode * outgoing_frames_head;
    struct timeval * expiring_timeval;
    long sleep_usec_time, sleep_sec_time;
    
    //This incomplete sender thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up
    //2. Grab the mutex protecting the input_cmd/inframe queues
    //3. Dequeues messages from the input queue and adds them to the outgoing_frames list
    //4. Releases the lock
    //5. Sends out the messages

    pthread_cond_init(&sender->buffer_cv, NULL);
    pthread_mutex_init(&sender->buffer_mutex, NULL);

    while(1)
    {    
        outgoing_frames_head = NULL;

        //Get the current time
        gettimeofday(&curr_timeval, 
                     NULL);

        //time_spec is a data structure used to specify when the thread should wake up
        //The time is specified as an ABSOLUTE (meaning, conceptually, you specify 9/23/2010 @ 1pm, wakeup)
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;

        //Check for the next event we should handle
        expiring_timeval = sender_get_next_expiring_timeval(sender);

        //Perform full on timeout
        if (expiring_timeval == NULL)
        {
            time_spec.tv_sec += WAIT_SEC_TIME;
            time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        }
        else
        {
            //Take the difference between the next event and the current time
            sleep_usec_time = timeval_usecdiff(&curr_timeval,
                                               expiring_timeval);

            //Sleep if the difference is positive
            if (sleep_usec_time > 0)
            {
                sleep_sec_time = sleep_usec_time/1000000;
                sleep_usec_time = sleep_usec_time % 1000000;   
                time_spec.tv_sec += sleep_sec_time;
                time_spec.tv_nsec += sleep_usec_time*1000;
            }   
        }

        //Check to make sure we didn't "overflow" the nanosecond field
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        
        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames or input commands should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&sender->buffer_mutex);

        //Check whether anything has arrived
        int input_cmd_length = ll_get_length(sender->input_cmdlist_head);
        int inframe_queue_length = ll_get_length(sender->input_framelist_head);
        
        //Nothing (cmd nor incoming frame) has arrived, so do a timed wait on the sender's condition variable (releases lock)
        //A signal on the condition variable will wakeup the thread and reaquire the lock
        if (input_cmd_length == 0 &&
            inframe_queue_length == 0)
        {
            
            pthread_cond_timedwait(&sender->buffer_cv, 
                                   &sender->buffer_mutex,
                                   &time_spec);
        }
        //Implement this
        handle_incoming_acks(sender,
                             &outgoing_frames_head);

        //Implement this
        handle_input_cmds(sender,
                          &outgoing_frames_head);

        pthread_mutex_unlock(&sender->buffer_mutex);


        //Implement this
        handle_timedout_frames(sender,
                               &outgoing_frames_head);

        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        
        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *)  ll_outframe_node->value;

            //Don't worry about freeing the char_buf, the following function does that
            send_msg_to_receivers(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);
    return 0;
}
