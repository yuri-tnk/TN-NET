
#define P1_26_MASK  (1<<26)
#define P1_28_MASK  (1<<28)
#define P1_29_MASK  (1<<29)

int tn_snprintf( char *outStr, int maxLen, const char *fmt, ... );
void dbg_send(char * buf);
void dbg_send_int(char * buf);
extern char dbg_buf[];
void dbg_pin_on(int pin_mask);
void dbg_pin_off(int pin_mask);




