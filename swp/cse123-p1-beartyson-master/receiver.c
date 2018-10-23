#include "receiver.h"

static void send_ack_message(int from_id, int to_id, unsigned char ack_no, 
							 LLnode ** outgoing_frames_head_ptr) {
	// acknowledge the new received seqno (nfe-1) to the sender
    Frame * ack_frame = (Frame *) malloc (sizeof(Frame));
	ack_frame->header.flags |= MSG_TYPE_FLAG_ACK;
	ack_frame->header.seq_no = 0;
	ack_frame->header.ack_no = ack_no;
	ack_frame->header.from_id = from_id;
	ack_frame->header.tar_id = to_id;

    //Convert the message to the outgoing_charbuf and add to sending list
    char * outgoing_charbuf = convert_frame_to_char(ack_frame);
	//calc CRC
	append_crc(outgoing_charbuf, sizeof(Frame));
    ll_append_node(outgoing_frames_head_ptr,
    			   outgoing_charbuf);

	free(ack_frame);
	ack_frame = NULL;

	fprintf(stderr, "send_ack_message: ack_no %d\n", ack_no);
}

void init_receiver(Receiver * receiver,
                   int id)
{
    receiver->recv_id = id;
    receiver->input_framelist_head = NULL;

	//malloc one window for each sender
	receiver->recv_window_array = malloc(sizeof(RecvWindow) *  glb_senders_array_length); 
	int i;
	for (i = 0; i < glb_senders_array_length; ++i) {
		RecvWindow * wnd = &receiver->recv_window_array[i];
		wnd->nfe = 1;
	}
}


void handle_incoming_msgs(Receiver * receiver,
                          LLnode ** outgoing_frames_head_ptr)
{
    // Suggested steps for handling incoming frames
    //    1) Dequeue the Frame from the sender->input_framelist_head
    //    2) Convert the char * buffer to a Frame data type
    //    3) Check whether the frame is corrupted
    //    4) Check whether the frame is for this receiver
    //    5) Do sliding window protocol for sender/receiver pair

    int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
    while (incoming_msgs_length > 0)
    {
        //Pop a node off the front of the link list and update the count
        LLnode * ll_inmsg_node = ll_pop_node(&receiver->input_framelist_head);
        incoming_msgs_length = ll_get_length(receiver->input_framelist_head);

        //DUMMY CODE: Print the raw_char_buf
        //NOTE: You should not blindly print messages!
        //      Ask yourself: Is this message really for me?
        //                    Is this message corrupted?
        //                    Is this an old, retransmitted message?           
        char * raw_char_buf = (char *) ll_inmsg_node->value;

		//the check length = sizeof(Frame)
		if (is_corrupted(raw_char_buf, sizeof(Frame)) > 0) {
			fprintf(stderr, "ERR: corrupted message\n");
			//drop the message
        	free(raw_char_buf);
        	free(ll_inmsg_node);
			continue;
		}

        Frame * inframe = convert_char_to_frame(raw_char_buf);
        
        //Free raw_char_buf
        free(raw_char_buf);
        

		//check message type: data
		if ((inframe->header.flags & MSG_TYPE_FLAG_DATA) == 0) {
			fprintf(stderr, "ERR: recv invalid message type %d\n", inframe->header.flags);
        	free(ll_inmsg_node);
			continue;
		}

		if (inframe->header.tar_id != receiver->recv_id) {
        	free(ll_inmsg_node);
			continue;
		}

		int from_id = inframe->header.from_id;

        if (from_id < 0 || from_id >= glb_senders_array_length) {
			fprintf(stderr, "ERR: recv invalid from_id %d\n", from_id);
        	free(ll_inmsg_node);
			continue;
		}
 	    RecvWindow * wnd = &receiver->recv_window_array[from_id];
		unsigned char laf = wnd->nfe + RWS - 1;

        //fprintf(stderr, "nfe: %d, laf: %d\n", wnd->nfe, laf);

		unsigned char seq_no = inframe->header.seq_no;
		//make sure seqno is in window
		if ((wnd->nfe <= laf && seq_no >= wnd->nfe && seq_no <= laf) || 
			(wnd->nfe > laf && (seq_no >= wnd->nfe || seq_no <= laf))) {
        	//fprintf(stderr, "message in recv window\n");
			//just put the incoming frame in window cache
			struct recvQ_slot * slot = &wnd->recvQ[seq_no % RWS];
			if (slot->received == 1) {
				send_ack_message(receiver->recv_id, from_id, wnd->nfe - 1, outgoing_frames_head_ptr);
				continue;
			}
			slot->received = 1;
			slot->frame = inframe;

			//if the frame is not NFE, it means not successive frame arrived, namely disorder. we just put it in cache and make NO ack
			//if the frame is exactly what we expected, then it's time to slide the window NFE to the largest consecutive frame received,
			//and the new (nfe-1) value should be the ack number to the sender
			if (seq_no == wnd->nfe) {
        		//fprintf(stderr, "seq_no equals to nfe, slide the window\n");
				//slide as long as the slot is received
				do {
					//the passed slot should be make non-received
					slot->received = 0;	
        			printf("<RECV_%d>:[%s]\n", receiver->recv_id, slot->frame->data);
        			fprintf(stderr, "<RECV_%d>:[%s]\n", receiver->recv_id, slot->frame->data);
					free(slot->frame);
					slot->frame = NULL;
					//auto wrap to 0 where it reached 255
					++wnd->nfe;
					slot = &wnd->recvQ[wnd->nfe % RWS];
				} while(slot->received > 0);
        		//fprintf(stderr, "updated: nfe %d\n", wnd->nfe);

				// acknowledge the new received seqno (nfe-1) to the sender
				send_ack_message(receiver->recv_id, from_id, wnd->nfe - 1, outgoing_frames_head_ptr);
			}
		} else {
			// seq_no not in window: just send ACK with number NFE -1
			// if previous ack was lost and sender retransmited the message, this ack will be useful
			
			//send_ack_message(receiver->recv_id, from_id, wnd->nfe - 1, outgoing_frames_head_ptr);
		}

        free(ll_inmsg_node);
    }
}

void * run_receiver(void * input_receiver)
{    
    struct timespec   time_spec;
    struct timeval    curr_timeval;
    const int WAIT_SEC_TIME = 0;
    const long WAIT_USEC_TIME = 100000;
    Receiver * receiver = (Receiver *) input_receiver;
    LLnode * outgoing_frames_head;


    //This incomplete receiver thread, at a high level, loops as follows:
    //1. Determine the next time the thread should wake up if there is nothing in the incoming queue(s)
    //2. Grab the mutex protecting the input_msg queue
    //3. Dequeues messages from the input_msg queue and prints them
    //4. Releases the lock
    //5. Sends out any outgoing messages

    pthread_cond_init(&receiver->buffer_cv, NULL);
    pthread_mutex_init(&receiver->buffer_mutex, NULL);

    while(1)
    {    
        //NOTE: Add outgoing messages to the outgoing_frames_head pointer
        outgoing_frames_head = NULL;
        gettimeofday(&curr_timeval, 
                     NULL);

        //Either timeout or get woken up because you've received a datagram
        //NOTE: You don't really need to do anything here, but it might be useful for debugging purposes to have the receivers periodically wakeup and print info
        time_spec.tv_sec  = curr_timeval.tv_sec;
        time_spec.tv_nsec = curr_timeval.tv_usec * 1000;
        time_spec.tv_sec += WAIT_SEC_TIME;
        time_spec.tv_nsec += WAIT_USEC_TIME * 1000;
        if (time_spec.tv_nsec >= 1000000000)
        {
            time_spec.tv_sec++;
            time_spec.tv_nsec -= 1000000000;
        }

        //*****************************************************************************************
        //NOTE: Anything that involves dequeing from the input frames should go 
        //      between the mutex lock and unlock, because other threads CAN/WILL access these structures
        //*****************************************************************************************
        pthread_mutex_lock(&receiver->buffer_mutex);

        //Check whether anything arrived
        int incoming_msgs_length = ll_get_length(receiver->input_framelist_head);
        if (incoming_msgs_length == 0)
        {
            //Nothing has arrived, do a timed wait on the condition variable (which releases the mutex). Again, you don't really need to do the timed wait.
            //A signal on the condition variable will wake up the thread and reacquire the lock
            pthread_cond_timedwait(&receiver->buffer_cv, 
                                   &receiver->buffer_mutex,
                                   &time_spec);
        }

        handle_incoming_msgs(receiver,
                             &outgoing_frames_head);

        pthread_mutex_unlock(&receiver->buffer_mutex);
        
        //CHANGE THIS AT YOUR OWN RISK!
        //Send out all the frames user has appended to the outgoing_frames list
        int ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        while(ll_outgoing_frame_length > 0)
        {
            LLnode * ll_outframe_node = ll_pop_node(&outgoing_frames_head);
            char * char_buf = (char *) ll_outframe_node->value;
            
            //The following function frees the memory for the char_buf object
            send_msg_to_senders(char_buf);

            //Free up the ll_outframe_node
            free(ll_outframe_node);

            ll_outgoing_frame_length = ll_get_length(outgoing_frames_head);
        }
    }
    pthread_exit(NULL);

}
