#ifndef DEFINED_RTUART_H
#define DEFINED_RTUART_H

struct timespec;
struct rtuart_callbacks;
struct rtuart;
struct rtuart_buffer;


void rtuart_destroy (struct rtuart * uart);

/*
 * Assign callbacks to this uart. The userpointer can be used by the callbacks.
 * It can be queried with rtuart_callback_user_ptr.
 */
void   rtuart_assign_callbacks(struct rtuart *, struct rtuart_callbacks *, void * userptr);
void * rtuart_callback_user_ptr(struct rtuart *);

/*
 * Sets the baudrate.
 */
void rtuart_set_baudrate(struct rtuart *, const unsigned long baudrate);

/*
 * Gets the baudrate.
 */
void rtuart_get_baudrate(struct rtuart *, unsigned long * baudrate);

/*
 * Sets the format containing databits, parity and number of stopbits.
 */
void rtuart_set_format(struct rtuart *, const int databits, const char parity, const char stopbits);

typedef enum {
  RTUART_HANDSHAKE_OFF   = 0,
  RTUART_HANDSHAKE_RTSCTS = 1,
  RTUART_HANDSHAKE_DSRDTR = 2,
  RTUART_HANDSHAKE_RTSCTS_DSRDTR = 3
} rtuart_handshake_t;

/*
 * Sets the handshake method.
 */
void rtuart_set_handshake(struct rtuart *, rtuart_handshake_t);

/*
 * Sets the controls in to_set.
 */
void rtuart_set_control(struct rtuart *, const unsigned long to_set);
/*
 * Clears back the controls in to_clr.
 */
void rtuart_clr_control(struct rtuart *, const unsigned long to_clr);
/*
 * Gets the controls.
 */
void rtuart_get_control(struct rtuart *, unsigned long  * ctrl);


/*
 * Sets the notifiers in set_notify mask.
 */
void rtuart_set_notify(struct rtuart *, const unsigned long set_notify);
/*
 * Clears the notifiers in clear_notify mask.
 */
void rtuart_clr_notify(struct rtuart *, const unsigned long clear_notify);
/*
 * Gets the current notifier mask.
 */
void rtuart_get_notify(struct rtuart *, unsigned long  * notify_mask);

/*
 * When start is called while a transmision is ongoing.
 */
void rtuart_tx_start  (struct rtuart *, struct rtuart_buffer *);
void rtuart_tx_stop   (struct rtuart *, struct rtuart_buffer *, int * allready_written);
int  rtuart_tx_written(struct rtuart *, struct rtuart_buffer *);

/*
 * Starts a new transmision. the buffer must be available until the transmision is completed.
 */
void rtuart_rx_start (struct rtuart *, struct rtuart_buffer *);

/*
 * stops an ongoing receiption and returns the number of octets
 * allready filled by the receiver. It ensures that after the call
 * returns no more data is written to the buffer.
 * If callback rx_end was allready called for this buffer
 * or there is no such buffer this funtion returns an error (-1).
 */
long rtuart_rx_stop(struct rtuart *, struct rtuart_buffer *);

/*
 * 
 * The time after a synbol was received until rx_timeout is called.
 * That means that rx_timeout is called not later than timeout after
 * the reception of a symbol. If timeout is to small and can not be
 * achived it is set to the next possible value. (some uarts have a
 * hardware timeout mechanism and on some uarts the bimber of bits
 * or characters for the timeout can be configured.
 * This is more acurate and has a smaller overhead than a timer.
 * But is the timeout is to big, a timer will be used.
 */
void rtuart_rx_set_timeout(struct rtuart *, struct timespec * timeout);



struct rtuart_callbacks
{
  /*
   * This callback is invoked right before the transmitter starts to
   * send out a transmission buffer.
   */
  void (*tx_start) (struct rtuart *, struct rtuart_buffer * buffer);

  /*
   * This callback is invoked when a transmission buffer has been completely
   * read by the driver.
   */
  void (*tx_end1) (struct rtuart *, struct rtuart_buffer * buffer);

  /*
   * This callback is invoked when a transmission has physically completed.
   */
  void (*tx_end2) (struct rtuart *, struct rtuart_buffer * buffer);

  /*
   * This callback is invoked on a change of the inputs (CTS, DSR, RI, DCD).
   */
  void (*input_change) (struct rtuart *,
			unsigned long eventmask,
			unsigned long values);

  /*
   * This callback is invoked on a receive error, the errors mask is passed
   * as parameter.
   */
  void (*rx_err) (struct rtuart *, unsigned long errmask);

  /*
   * This callback is invoked when a character is received but the application
   * was not ready to receive it, the character is passed as parameter.
   */
  void (*rx_char) (struct rtuart *, const unsigned short c);

  /*
   * This callback is invoked when a receive buffer has completely been filled.
   * The buffer is removed from the list of active buffers. If this function is
   * not defined, a default function is called, that removed the memory
   * in this buffer to prevent memory leaks.
   */
  void (*rx_end) (struct rtuart *, struct rtuart_buffer *);

  /*
   * there have no characters been received for some time.
   * This is called at most once per buffer. The buffer remains
   * the top element in the active buffer list. This must be done
   * explicitly by a call to rtuart_rx_stop.
   */
  void (*rx_timeout) (struct rtuart *, struct rtuart_buffer *);

  /*
   * Is called when a triggerpoint has been passed.
   * A trigger point can be used to initiate an end of buffer.
   * The trigger point that completes a buffer can be changed
   * while the buffer is active, e.g. if the one trigger is
   * set to the payload size and a callback adjusts the
   * end-of-buffer trigger (eob_trigger).
   */
  void (*rx_trigger)(struct rtuart *,
		     struct rtuart_buffer *,
		     const unsigned long triggermask
		     );
};

/*
 * Set a position for a trigger to be reached.
 * position = -1 disables the trigger.
 * if triggerno is -1, and position is -1, all triggers are reset.
 * (implemented by a memset(triggers, 0xff, sizeof(triggers)); which sets all triggers to -1. )
 */
int rtuart_buffer_set_trigger(struct rtuart_buffer *, int triggerno, int position);


#define RTUART_MAX_TRIGGERS (32)

struct rtuart_buffer
{
	/* the virtual address of the buffer. */
	unsigned char * data;

	/* the number of octets allocated. */
	unsigned long size;

	/*
	 * The number of octets that contain valid data.
	 * Receiver: For receive buffers this is filled in by the
	 * rtuart driver.
	 * Transmitter: For transmit buffers this has to be filled
	 * by the client driver.
	 * This number shall be smaller or equal to size.
	 */
	unsigned long validcount;

	/*
	 * Receive: number of octets really written to the buffer.
	 * Transmit: number of octets allready written to the transmitter.
	 * This number shall be smaller or equal to validcount.
	 */
	unsigned long transfered;
  
	/*
	 * Each bit in this mask represents one trigger.
	 * Bit 0 represents the first trigger (triggers[0]).
	 * If the bit is set, the corresponding trigger
	 * is set, it is active. The uart driver clears the bit,
	 * once rx_trigger has been called for that trigger.
	 * The client driver has to set this mask.
	 */
	unsigned long triggermask;

	/*
	 * rx_trigger is called if the number of octets received is
	 * higher or equal that of a trigger. A triger value of 0
	 * disables a trigger. That means that a memset or zalloc
	 * can be used to have uninitialized triggers.
	 */
	unsigned int trigger[RTUART_MAX_TRIGGERS];
};

#endif
