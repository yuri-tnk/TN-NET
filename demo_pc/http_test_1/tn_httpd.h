#ifndef _TN_HTTPD_H_
#define _TN_HTTPD_H_

#define HTTPD_STATUS_OK                      200
#define HTTPD_STATUS_MOVED_PERMANENTLY       301
#define HTTPD_STATUS_MOVED_TEMPORARILY       302
#define HTTPD_STATUS_NOT_MODIFIED            304
#define HTTPD_STATUS_BAD_REQUEST             400
#define HTTPD_STATUS_NOT_AUTHORIZED          401
#define HTTPD_STATUS_FORBIDDEN               403
#define HTTPD_STATUS_NOT_FOUND               404
#define HTTPD_STATUS_METHOD_NOT_ALLOWED      405
#define HTTPD_STATUS_SYSTEM_ERROR            500
#define HTTPD_STATUS_NOT_IMPLEMENTED         501


#define HTTPD_GET                             10
#define HTTPD_HEAD                            11
#define HTTPD_POST                            12
#define HTTPD_RX_MSG_HDR                      13
#define HTTPD_RX_MSG_BODY                     14
#define HTTPD_EOHDR                           15
#define HTTPD_EOBODY                          16

#define ERR_EINVAL                            -1
#define ERR_EPIPE                             -2
#define ERR_RES_NOT_FOUND                     -3
#define ERR_HANDLER_NOT_FOUND                 -4


#define HTTPD_RX_BUF_SIZE    512
#define HTTPD_TX_BUF_SIZE    512
#define HTTPD_URL_BUF_SIZE    32

typedef struct _HTTPDRESENTRY
{
   char res_full_name[HTTPD_URL_BUF_SIZE];
   const unsigned char * res_data;
   int res_len;
}HTTPDRESENTRY;

typedef struct _MIMEINFO
{
   const char * ext;
   const char * mime_type;
}MIMEINFO;

typedef struct _HTTPDREPLACEFILES
{
   char res_full_name[HTTPD_URL_BUF_SIZE];
   int  file_start_ind;
}HTTPDREPLACEFILES;

typedef struct _HTTPDREPLACEBLOCK
{
   const unsigned char * block_data;
   int  block_len;
   int  var_num;
}HTTPDREPLACEBLOCK;

//--------------------------------

typedef int (*httpd_handler_func)(void * hti);

typedef struct _HTTPDHANDLER
{
   httpd_handler_func handler_func;
  /* const*/ char * handler_func_name;

}HTTPDHANDLER;

typedef struct _HTTPDINFO
{
   int s;
   int method;
   int status;

   int msg_body_len;
   int fSendChunks;
   char * mime_type;
   int send_nbytes;

   int port;
   unsigned char ip_addr[4];

   HTTPDHANDLER *  g_handlers;
   HTTPDRESENTRY * g_resources;
   MIMEINFO * g_mime;
   HTTPDREPLACEFILES * g_replace_files;
   HTTPDREPLACEBLOCK * g_replace_blocks;

   char rx_buf[HTTPD_RX_BUF_SIZE];
   char tx_buf[HTTPD_TX_BUF_SIZE];
   char url_buf[HTTPD_URL_BUF_SIZE];

}HTTPDINFO;


int s_scmp(const char * str1, char * str2, int count, int maxlen);
int ch_hex_to_bin(int ch);

int httpd_init(HTTPDINFO * hti);
char * httpd_file_ext(HTTPDINFO * hti);
int httpd_main_func(HTTPDINFO * hti);
int httpd_line_processing(HTTPDINFO * hti,
                          char * line_start_ptr,
                          int line_data_len);

int httpd_rx_hdr_proc(HTTPDINFO * hti, int rx_bytes);
int httpd_rx_body_proc(HTTPDINFO * hti, int rx_bytes);
char * httpd_get_URL(HTTPDINFO * hti, char * ptr);
int httpd_proc_handler_func(HTTPDINFO * hti);
int httpd_processing_GET(HTTPDINFO * hti);
int httpd_processing_POST(HTTPDINFO * hti);
int httpd_send(HTTPDINFO * hti, char * buf, int nbytes);
int httpd_send_chunks(HTTPDINFO * hti, char * buf, int nbytes);
int httpd_end_chunks(HTTPDINFO * hti);
char * httpd_find_mime_type(HTTPDINFO * hti, char *ext);
int httpd_send_res(HTTPDINFO * hti, char * res_data, int res_len);
int httpd_format_header(HTTPDINFO * hti);
int httpd_send_error(HTTPDINFO * hti, int errcode);
int httpd_send_replaceable_res(HTTPDINFO * hti, int start_ind);
void  httpd_form_name_val_proc(char * buf, int buf_len);


int httpd_get_replaceable_val(HTTPDINFO * hti, char * data_buf, int var_num);
void httpd_set_form_value(char * name_start, int name_len,
                                              char * val_start, int val_len);


#endif

