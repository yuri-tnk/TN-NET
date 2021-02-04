/*

Copyright © 2004, 2009 Yuri Tiomkin
All rights reserved.

Permission to use, copy, modify, and distribute this software in source
and binary forms and its documentation for any purpose and without fee
is hereby granted, provided that the above copyright notice appear
in all copies and that both that copyright notice and this permission
notice appear in supporting documentation.

THIS SOFTWARE IS PROVIDED BY THE YURI TIOMKIN AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL YURI TIOMKIN OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#include <stdlib.h>  //-- atoi
#include <string.h>
#include <ctype.h>  //-- toupper
#include "../tn_net/cpu_lpc23xx/LPC236X.h"

#include "../tnkernel/tn.h"

#include "../tn_net/tn_net_cfg.h"
#include "../tn_net/tn_net_types.h"
#include "../tn_net/tn_net_pcb.h"
#include "../tn_net/tn_net_mem.h"
#include "../tn_net/tn_ip.h"
#include "../tn_net/tn_net.h"
#include "../tn_net/errno.h"
#include "../tn_net/tn_mbuf.h"
#include "../tn_net/tn_net_utils.h"
#include "../tn_net/tn_netif.h"
#include "../tn_net/bsd_socket.h"
#include "../tn_net/tn_socket.h"

#include "../tn_net/tn_socket_func.h"
#include "../tn_net/dbg_func.h"

#include "types.h"
#include "tn_httpd.h"

//--- A project depended resourses ---------

#include "http_resources.c"

#define  FIELD1_MAX_LEN         31

char g_field0_value[FIELD1_MAX_LEN + 1] = "1234567890";

//--- A project depended handlers ----------

int httpd_handler_root(void * par);
int httpd_handler_form1_submit(void * par);

//------------------------------------------

const HTTPDHANDLER g_httpd_handlers[] =
{
   {httpd_handler_root, "/"},
   {httpd_handler_form1_submit, "/form1_submit"},
   {NULL, ""}
};

const MIMEINFO g_prj_mime_info[] =
{
   {"bmp",  "image/bmp"},
   {"css",  "text/css"},
   {"gif",  "image/gif"},
   {"jpeg", "image/jpeg"},
   {"jpg",  "image/jpeg"},
   {"js",   "application/x-javascript"},
   {"png",  "image/png"},
   {"htm",  "text/html"},
   {"html", "text/html"},
   {"ico",  "image/vnd.microsoft.icon"},
   {"", ""}
};

//----------------------------------------------------------------------------
int httpd_init(HTTPDINFO * hti)
{
   memset(hti, 0, sizeof(HTTPDINFO));

   hti->g_handlers        = (HTTPDHANDLER *)&g_httpd_handlers[0];
   hti->g_resources       = (HTTPDRESENTRY *)&g_http_resources[0];
   hti->g_mime            = (MIMEINFO *)&g_prj_mime_info[0];
   hti->g_replace_files   = (HTTPDREPLACEFILES *)&g_http_replace_resources[0];
   hti->g_replace_blocks  = (HTTPDREPLACEBLOCK *)&g_http_replace_fileparts[0];

   return 0;
}

//----------------------------------------------------------------------------
int httpd_main_func(HTTPDINFO * hti)
{
   int rc;
   int len;
   int line_data_len;
   int fEndOfReq;
   char * end_ptr;
   char * start_rx_ptr;
   char * ptr;
   char * line_start_ptr;
   char * prev_line_start_ptr;

   start_rx_ptr   = hti->rx_buf;
   line_start_ptr = hti->rx_buf;
   prev_line_start_ptr = hti->rx_buf;
   hti->method = 0;
   hti->msg_body_len = 0;
   fEndOfReq = FALSE;

   for(;;)
   {
      len = (int)((hti->rx_buf + HTTPD_RX_BUF_SIZE) - start_rx_ptr);

      rc = s_recv(hti->s, (unsigned char*)start_rx_ptr, len, 0);
      if(rc < 0) //-- SOCKET_ERROR
      {
         s_close(hti->s);
         break;
      }
      else if(rc == 0)
      {
         s_close(hti->s);
         break;
      }
      else
      {
         if(fEndOfReq == TRUE)
            continue;

         end_ptr = start_rx_ptr + rc;
         ptr     = start_rx_ptr;
         fEndOfReq = FALSE;

         for(;;)
         {
            if(ptr >= end_ptr)
               break;
            if(*ptr == '\n') //-- End of line
            {
               prev_line_start_ptr = line_start_ptr;
               line_start_ptr = ptr+1;

               line_data_len = ptr - prev_line_start_ptr + 1;
               if(line_data_len == 2) //-- an empty line end - EndOfReq
               {
                  fEndOfReq = TRUE;
                  break;
               }

               rc = httpd_line_processing(hti,
                                          prev_line_start_ptr,
                                          line_data_len);
               if(rc < 0) //-- Fatal error
               {
                  s_close(hti->s);
                  return 0;
               }
            }
            ptr++;
         }

    //-- If here - end of request or all received data processed

        //-- Shift the rx reminder(if any) to the start of the rx_buf

         len = end_ptr - line_start_ptr;
         if(len > 0)
            memmove(hti->rx_buf, line_start_ptr, len);
         line_start_ptr = hti->rx_buf;
         start_rx_ptr   = hti->rx_buf + len;

        //-- Here processing for GET/POST

         if(fEndOfReq)
         {
            if(hti->msg_body_len > 0) //-- Body exists
            {
             //-- Total msg body len MUST be <= RX_BUF_SIZE

               if(len > 0)
                  len = _min(hti->msg_body_len, len);
               if(len < hti->msg_body_len)
               {
                  len = hti->msg_body_len - len;
                  while(len)
                  {
                     rc = s_recv(hti->s,
                               (unsigned char*)start_rx_ptr,
                              (int)((hti->rx_buf + HTTPD_RX_BUF_SIZE)
                                                      - start_rx_ptr), 0);
                     if(rc < 0) //-- SOCKET_ERROR
                     {
                        //s_shutdown(hti->s, SHUT_RDWR);
                        s_close(hti->s);
                        return 0;
                     }
                     else if(rc == 0)
                     {
                        s_close(hti->s);
                        return 0;
                     }
                     len -= rc;
                     if(len <= 0)
                        break;
                     start_rx_ptr += rc;
                  }
               }
            }

            if(hti->method == HTTPD_GET)
            {
               rc = httpd_processing_GET(hti);
               if(rc == ERR_RES_NOT_FOUND)
                  httpd_send_error(hti, HTTPD_STATUS_NOT_FOUND);
            }
            else if(hti->method == HTTPD_POST && hti->msg_body_len > 0)
            {
               httpd_processing_POST(hti);
            }
            else  //-- Err
               httpd_send_error(hti, HTTPD_STATUS_BAD_REQUEST);

            hti->method = 0;

           // s_shutdown(hti->s, SHUT_WR);
            s_shutdown(hti->s, SHUT_RDWR);
         }
      }
    //-----
   }

   return 0;
}

//----------------------------------------------------------------------------
int ch_hex_to_bin(int ch)
{
   if((ch >= '0') && (ch <= '9'))
      return (ch - '0');
   if((ch >= 'A') && (ch <= 'F'))
      return (ch - 'A' + 10);
   if((ch >= 'a') && (ch <= 'f'))
      return (ch - 'a' + 10);
   return -1;
}

//----------------------------------------------------------------------------
int s_scmp(const char * str1, char * str2, int count, int maxlen)
{
   int i;
   int ch_str1;
   int ch_str2;

   if(count <= 0 || maxlen <= 0)
      return 0;

   count = _min(count, maxlen);

   for(i=0; i < count; i++)
   {
      ch_str1 = toupper(*str1);
      ch_str2 = toupper(*str2);
      if(ch_str1 == 0 || ch_str1 != ch_str2)
      {
         return (*(unsigned char *)str1 - *(unsigned char *)str2);
      }
      str1++;
      str2++;
   }
   return 0;
}

//----------------------------------------------------------------------------
char * httpd_file_ext(HTTPDINFO * hti)
{
   int len;
   char * ext;
   len = strlen(hti->url_buf);
   ext = NULL;
   if(len)
   {
      len--;
      while(len)
      {
         if(hti->url_buf[len] == '.')
         {
            ext = &hti->url_buf[len + 1];
            break;
         }
         len--;
      }
   }
   return ext;
}

//----------------------------------------------------------------------------
int httpd_line_processing(HTTPDINFO * hti,
                          char * line_start_ptr,
                          int line_data_len)
{
   if(s_scmp("GET ", line_start_ptr, 4, line_data_len) == 0)
   {
      hti->method = HTTPD_GET;
      httpd_get_URL(hti, line_start_ptr + 4);
   }
   else if(s_scmp("POST ", line_start_ptr, 5, line_data_len) == 0)
   {
      hti->method = HTTPD_POST;
      httpd_get_URL(hti, line_start_ptr + 5);
   }
   else if(s_scmp("Content-Length: ", line_start_ptr, 16, line_data_len) == 0)
   {
      *(line_start_ptr + line_data_len) = '\0';
      hti->msg_body_len = atoi(line_start_ptr + 16);
      if(hti->msg_body_len > HTTPD_RX_BUF_SIZE) //-- Err
      {
         httpd_send_error(hti, HTTPD_STATUS_BAD_REQUEST);
         return -1;
      }
   }

   return 0; //-- to continue operating
}

//----------------------------------------------------------------------------
char * httpd_get_URL(HTTPDINFO * hti, char * ptr)
{
   int ch;
   char * dst;

   dst = hti->url_buf;

   while((*ptr == '/') && (*(ptr+1) == '/'))
      ptr++;

   for(;;)
   {
      if(dst - hti->url_buf >= HTTPD_URL_BUF_SIZE)
         break;
      ch = *ptr;
      if(ch == ' ' || ch == '?' || ch == '\r')
         break;

      if(ch == '%') //-- Hex data
      {
         ptr++;
         ch = ch_hex_to_bin(*ptr++);
         if(ch == -1)
         {
            httpd_send_error(hti, HTTPD_STATUS_BAD_REQUEST);
            return NULL;
         }
         *dst = ch << 4;
         ch = ch_hex_to_bin(*ptr++);
         if(ch == -1)
         {
            httpd_send_error(hti, HTTPD_STATUS_BAD_REQUEST);
            return NULL;
         }
         *dst |= ch;
         dst++;
      }
      else
         *dst++ = *ptr++;
   }

   *dst = '\0';

   if(*hti->url_buf != '/')  //-- A leading slash must be first in the URL
   {
      httpd_send_error(hti, HTTPD_STATUS_BAD_REQUEST);
      return NULL;
   }
   return ptr;
}

//----------------------------------------------------------------------------
int httpd_proc_handler_func(HTTPDINFO * hti)
{
   HTTPDHANDLER * p_h;
   HTTPDHANDLER * h_func_arr;
   int i;
   int rc;

   if(hti == NULL)
      return ERR_EINVAL;
   h_func_arr = hti->g_handlers;
   if(h_func_arr == NULL)
      return ERR_EINVAL;

   rc = ERR_HANDLER_NOT_FOUND;
   for(i=0;;i++)
   {
      p_h = &h_func_arr[i];
      if(p_h)
      {
         if(p_h->handler_func == NULL)
            break;
         if(strcmp(hti->url_buf, p_h->handler_func_name) == 0)
         {
            if(p_h->handler_func)
            {
               rc = p_h->handler_func(hti);
               break;
            }
         }
      }
      else
         break;
   }
   return rc;
}

//----------------------------------------------------------------------------
int httpd_processing_GET(HTTPDINFO * hti)
{
   int rc;
   int i;
   HTTPDRESENTRY * p_res;
   HTTPDREPLACEFILES * prf;
   HTTPDRESENTRY * res_arr;

   if(hti == NULL)
      return ERR_EINVAL;
   res_arr = hti->g_resources;
   if(res_arr == NULL)
      return ERR_EINVAL;

   rc = httpd_proc_handler_func(hti);
   if(rc != ERR_HANDLER_NOT_FOUND)
      return rc;

//-------- Resources without replacement --------------------

   rc = ERR_RES_NOT_FOUND;
   for(i=0;;i++)
   {
      p_res = &res_arr[i];
      if(p_res)
      {
         if(p_res->res_full_name[0] == '\0')
            break;
         if(strcmp(hti->url_buf, p_res->res_full_name) == 0)
         {
            rc = httpd_send_res(hti, (char*)p_res->res_data, p_res->res_len);
            break;
         }
      }
      else
         break;
   }

   if(rc != ERR_RES_NOT_FOUND)
      return rc;

//-------- Resources with replacement --------------------

   for(i = 0; ;i++)
   {
      prf = &hti->g_replace_files[i];
      if(prf)
      {
         if(prf->res_full_name[0] == '\0')
            break;
         if(strcmp(hti->url_buf, prf->res_full_name) == 0)
         {
            rc = httpd_send_replaceable_res(hti, prf->file_start_ind);
            break;
         }
      }
      else
         break;
   }

   return rc;
}

//----------------------------------------------------------------------------
int httpd_processing_POST(HTTPDINFO * hti)
{
   int rc;

   if(hti == NULL)
      return ERR_EINVAL;

   rc = httpd_proc_handler_func(hti);
   if(rc == ERR_HANDLER_NOT_FOUND)
   {
      rc = httpd_send_error(hti, HTTPD_STATUS_NOT_FOUND);
   }
   return rc;
}

//----------------------------------------------------------------------------
int httpd_send(HTTPDINFO * hti, char * buf, int nbytes)
{
   int rc;
   int len;
   char * ptr;

   ptr = buf;
   while(nbytes)
   {
      len = _min(nbytes, HTTPD_TX_BUF_SIZE);

      rc = s_send(hti->s, (unsigned char*)ptr, len, 0);
      if(rc > 0)
      {
         nbytes -=len;
         ptr += len;
      }
      else if(rc == 0)
         return ERR_EPIPE;
      else
         return rc;
   }
   return 0; //-- OK
}

//----------------------------------------------------------------------------
int httpd_send_chunks(HTTPDINFO * hti, char * buf, int nbytes)
{
   int rc;
   char pre_buf[16];

  //--- Leader

   tn_snprintf(pre_buf, 15, "%x\r\n", nbytes);

   rc = s_send(hti->s, (unsigned char*)pre_buf, strlen(pre_buf), 0);
   if(rc == 0)
      return ERR_EPIPE;
   else if(rc < 0)
      return rc;

  //--- Data

   rc = httpd_send(hti, buf, nbytes);
   if(rc < 0) //-- Err
      return rc;

 //--- Trailer

   strcpy(pre_buf, "\r\n");
   rc = s_send(hti->s, (unsigned char*)pre_buf, 2, 0);
   if(rc == 0)
      return ERR_EPIPE;
   else if(rc < 0)
      return rc;

   return 0; //-- OK
}

//----------------------------------------------------------------------------
int httpd_end_chunks(HTTPDINFO * hti)
{
   int rc;
   char buf[8];

   strcpy(buf, "0\r\n\r\n");
   rc = httpd_send(hti, buf, 5);
   hti->fSendChunks = FALSE;

   return rc;
}

//----------------------------------------------------------------------------
char * httpd_find_mime_type(HTTPDINFO * hti, char * ext)
{
   MIMEINFO * p_mime;
   MIMEINFO * mime_arr;
   int i;

   if(hti == NULL || ext == NULL)
      return NULL;
   mime_arr = hti->g_mime;
   if(mime_arr == NULL)
      return NULL;

   for(i = 0; ;i++)
   {
      p_mime = &mime_arr[i];
      if(p_mime)
      {
         if(p_mime->ext[0] == '\0')
            break;
         if(strcmp(p_mime->ext, ext) == 0)
            return (char *)&p_mime->mime_type[0];
      }
      else
         break;
   }
   return NULL;
}

//----------------------------------------------------------------------------
int httpd_send_res(HTTPDINFO * hti,
                    char * res_data,
                    int res_len)
{
   int len;
   MIMEINFO * mime_arr;
   char * ext;

   if(hti == NULL)
      return ERR_EINVAL;

   mime_arr = hti->g_mime;
   if(mime_arr == NULL)
      return ERR_EINVAL;

   hti->status = HTTPD_STATUS_OK;
   hti->fSendChunks = FALSE;

 //-- Ptr to the file ext(if any), file name - in the 'hti->url'

   ext = httpd_file_ext(hti);
   if(ext == NULL)
      hti->mime_type = NULL;
   else
      hti->mime_type = httpd_find_mime_type(hti, ext);

   hti->send_nbytes  = res_len;
   len = httpd_format_header(hti);

  //-- Send header and data
   len = s_send(hti->s, (unsigned char*)hti->tx_buf, len, 0);
   if(len == 0)
      return ERR_EPIPE;
   else if(len < 0)
      return len;

   return httpd_send(hti, res_data, res_len);
}

//----------------------------------------------------------------------------
int httpd_send_replaceable_res(HTTPDINFO * hti, int start_ind)
{
   int len;
   int i;
   int rc = 0;
   MIMEINFO * mime_arr;
   HTTPDREPLACEBLOCK * prb; //g_replace_blocks;
   char * ext;

   if(hti == NULL)
      return ERR_EINVAL;

   mime_arr = hti->g_mime;
   if(mime_arr == NULL)
      return ERR_EINVAL;

   hti->status = HTTPD_STATUS_OK;
   hti->fSendChunks = TRUE;

 //--- Ptr to the file ext(if any), file name - in the 'hti->url'

   ext = httpd_file_ext(hti);
   if(ext == NULL)
      hti->mime_type = NULL;
   else
      hti->mime_type = httpd_find_mime_type(hti, ext);

   len = httpd_format_header(hti);

  //-- Send header

   len = s_send(hti->s, (unsigned char*)hti->tx_buf, len, 0);
   if(len == 0)
      return ERR_EPIPE;
   else if(len < 0)
      return len;

  //-- Send data

   for(i = start_ind; ;i++)
   {
      prb = &hti->g_replace_blocks[i];
      if(prb)
      {
         if(prb->block_data == NULL)
            break;
         else
         {
            rc = httpd_send_chunks(hti, (char*)prb->block_data, prb->block_len);
            if(rc < 0) //-- Error
            {
               break;
            }
            if(prb->var_num != 0) //-- We have some data to replace
            {
               rc = httpd_get_replaceable_val(hti, hti->tx_buf, prb->var_num);
               if(rc > 0) //-- OK
               {
                  rc = httpd_send_chunks(hti, hti->tx_buf, rc);
                  if(rc < 0)
                     break;
               }
            }
         }
      }
      else
         break;
   }

   rc = httpd_end_chunks(hti);

   return rc;
}

//----------------------------------------------------------------------------
int httpd_format_header(HTTPDINFO * hti)
{
   int len;

   tn_snprintf(hti->tx_buf, HTTPD_TX_BUF_SIZE-1, "HTTP/1.1 %d", hti->status);

   switch(hti->status)
   {
      case HTTPD_STATUS_NOT_FOUND:

         strcat(hti->tx_buf, " Not Found\r\n");
         len = strlen(hti->tx_buf);
         tn_snprintf(hti->tx_buf + len, (HTTPD_TX_BUF_SIZE-1) - len,
                     "Content-Length: %d\r\n", hti->send_nbytes);
         break;

      case HTTPD_STATUS_METHOD_NOT_ALLOWED:

         strcat(hti->tx_buf, " Method Not Allowed\r\n");
         break;

      default:

         strcat(hti->tx_buf, " OK\r\n");
         len = strlen(hti->tx_buf);
         if(hti->fSendChunks == FALSE)
            tn_snprintf(hti->tx_buf + len, (HTTPD_TX_BUF_SIZE-1) - len,
                    "Content-Length: %d\r\n",
                    hti->send_nbytes);
         break;
   }

   len = strlen(hti->tx_buf);
   tn_snprintf(hti->tx_buf + len, (HTTPD_TX_BUF_SIZE-1) - len,
                                    "Server: %s\r\n", "TN NET EmbWebSrv");

   strcat(hti->tx_buf,"Connection: ");

#ifdef HTTPD_USE_KEEPALIVE
   if(hti->fCloseSocket)
      strcat(hti->tx_buf, "close\r\n");
   else
      strcat(hti->tx_buf, "keep-alive\r\n");
#else
   strcat(hti->tx_buf, "close\r\n");
#endif

   if(hti->mime_type == NULL)
      hti->mime_type = (char *)"text/plain";

   len = strlen(hti->tx_buf);
   tn_snprintf(hti->tx_buf + len, (HTTPD_TX_BUF_SIZE-1) - len,
                                   "Content-Type: %s\r\n", hti->mime_type);

   if(hti->fSendChunks)
      strcat(hti->tx_buf, "Transfer-Encoding: chunked\r\n");

   strcat(hti->tx_buf, "Cache-Control: no-cache\r\n");
   strcat(hti->tx_buf, "\r\n"); //-- As the end of the header

   return strlen(hti->tx_buf);
}

//----------------------------------------------------------------------------
int httpd_send_error(HTTPDINFO * hti, int errcode)
{
   int len;
   int rc = 0;
   const char term_str[] = "</title></head>\r\n";

   hti->status = errcode;
   hti->fSendChunks = TRUE;
   hti->mime_type = "text/html";

   len = httpd_format_header(hti);
   rc = httpd_send(hti, hti->tx_buf, len);
   if(rc < 0)
      return rc;

 //---------------------------

   strcpy(hti->tx_buf, "<!DOCTYPE HTML PUBLIC \"-//IETF//DTD HTML 1.1//EN\">\r\n");
   strcat(hti->tx_buf,"<html><head><title>");

   switch(errcode)
   {
      case HTTPD_STATUS_NOT_AUTHORIZED:

         strcat(hti->tx_buf, "401 Not Authorized");
         strcat(hti->tx_buf, term_str);
         strcat(hti->tx_buf,
               "<body><p>Authorization required to access this URL.</p>\r\n");
         break;

      case HTTPD_STATUS_NOT_FOUND:

         strcat(hti->tx_buf, "404 Not Found");
         strcat(hti->tx_buf, term_str);
         len = strlen(hti->tx_buf);
         tn_snprintf(hti->tx_buf + rc, (HTTPD_TX_BUF_SIZE -1) - len,
                "<p>The requested URL: %s was not found on this server</p>\r\n",
                hti->url_buf);
         break;

      case HTTPD_STATUS_SYSTEM_ERROR:

         strcat(hti->tx_buf, "500 Server Error");
         strcat(hti->tx_buf, term_str);
         strcat(hti->tx_buf,
               "<p>The server encountered an unexpected condition that "
               "prevented it from fulfilling the request"
               " by the client</p>\r\n");
         break;

      case HTTPD_STATUS_NOT_IMPLEMENTED:

         strcat(hti->tx_buf,"501 Not Implemented");
         strcat(hti->tx_buf, term_str);
         strcat(hti->tx_buf,
                "<p>The method requested is not implemented</p>\r\n");
         break;

      default:

         strcat(hti->tx_buf,"400 Bad Request");
         strcat(hti->tx_buf, term_str);
         strcat(hti->tx_buf,
               "<p>Bad request - too big file posted?</p>\r\n");
         break;
   }

   len = strlen(hti->tx_buf);
   tn_snprintf(hti->tx_buf + len, (HTTPD_TX_BUF_SIZE-1) - len,
            "<hr>%s at %d.%d.%d.%d Port %d\r\n</body></html>\r\n",
            "TN NET EmbWebSrv",
            hti->ip_addr[0],
            hti->ip_addr[1],
            hti->ip_addr[2],
            hti->ip_addr[3],
            hti->port);

   len = strlen(hti->tx_buf);
   rc = httpd_send_chunks(hti, hti->tx_buf, len);
   if(rc <= 0)
      return rc;
   return httpd_end_chunks(hti);
}

//----------------------------------------------------------------------------
void  httpd_form_name_val_proc(char * buf, int buf_len)
{
   char * start_ptr;
   char * end_ptr;
   char * ptr;
   int name_len;
   char * name_start;
   int val_len;
   char * val_start;
   int ch;

   start_ptr  = buf;
   end_ptr    = buf + buf_len;
   ptr        = buf;
   name_len   = 0;
   name_start = NULL;
   val_len    = 0;
   val_start  = NULL;

   for(;;)
   {
      if(ptr >= end_ptr)
         break;
      ch = *ptr;
      if(ch == '\0' || ch == ' ')
         break;
      else if(ch == '=')
      {
         name_len = ptr - start_ptr;
         name_start = start_ptr;
         start_ptr = ptr + 1;
      }
      else if(ch == '&')
      {
         if(name_len != 0)
         {
            val_len = ptr - start_ptr;
            val_start = start_ptr;
            start_ptr = ptr + 1;

            httpd_set_form_value(name_start, name_len,
                                           val_start, val_len);
            name_len = 0;
         }
      }
      ptr++;
   }
 //--- Remainder(if any)
   if(name_len != 0)
   {
      val_len = ptr - start_ptr;
      val_start = start_ptr;

      httpd_set_form_value(name_start, name_len,
                                           val_start, val_len);
   }
}

//============================================================================
// A project(the example) depended functions & handlers
//============================================================================

//----------------------------------------------------------------------------
int httpd_get_replaceable_val(HTTPDINFO * hti, char * data_buf, int var_num)
{
   int rc = -1;

   if(var_num == 1)
   {
      strcpy(data_buf, g_field0_value);
      rc = strlen(data_buf);
   }
   else if(var_num == 2)
   {
      strcpy(data_buf, g_field0_value);
      rc = strlen(data_buf);
   }

   return rc;
}

//----------------------------------------------------------------------------
int httpd_handler_root(void * par)
{
   HTTPDINFO * hti  = (HTTPDINFO *)par;
  //-- Here we just redirect the first request to the "index.html" page

  //if(strcmp(hti->url_buf, "/") == 0)
   strcpy(hti->url_buf, "/index.html");
   return ERR_HANDLER_NOT_FOUND;
}

//----------------------------------------------------------------------------
int httpd_handler_form1_submit(void * par)
{
   HTTPDINFO * hti  = (HTTPDINFO *)par;

   httpd_form_name_val_proc(hti->rx_buf, hti->msg_body_len);

   //--- As responce - send a page with the form

   strcpy(hti->url_buf, "/index.html");
   return httpd_send_replaceable_res(hti, 0); //-- start_ind = 0 here
}
//----------------------------------------------------------------------------
void httpd_set_form_value(char * name_start, int name_len,
                          char * val_start, int val_len)
{
   int len;
   const char field0_name[] = "textfield0"; //-- For the example only

   if(name_start == NULL || name_len == 0 ||
               val_start == NULL || val_len == 0)
      return;

  //-- In general, here we should find the name in the list and set the value
  //-- But for the example, the search is simplified

   len = _min(name_len, sizeof(field0_name));
   if(s_scmp(field0_name, name_start, len, len) == 0)
   {
      len = _min(FIELD1_MAX_LEN, val_len);
      memcpy(g_field0_value, val_start, len);
      g_field0_value[len] = '\0';
   }
}

//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------
//----------------------------------------------------------------------------

