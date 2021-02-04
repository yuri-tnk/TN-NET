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

#include "stdafx.h"
#include <direct.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <windows.h>

#pragma warning(disable: 4996)

//-- The program errorcodes -------------

#define  ERR_NO_ERR              0
#define  ERR_EINVAL            (-1)
#define  ERR_MEMORY            (-2)
#define  ERR_FILEOPEN          (-3)
#define  ERR_FILEWRITE         (-4)
#define  ERR_FILEREAD          (-5)
#define  ERR_BIGLEN            (-6)
#define  ERR_GET_DIR           (-7)
#define  ERR_SET_DIR           (-8)
#define  ERR_NOT_FOUND         (-9)
#define  ERR_FIND_NEXT_ERR     (-10)
#define  ERR_RESTORE_DIR       (-11)

//----------------------------------------

#define  HTTPD_MAX_RES_NAME_LEN  31

#define  FILE_RD_BUF_SIZE  1024
#define  FILE_WR_BUF_SIZE  1024


const char g_str_torepl_prefix[] = "valuetoreplace_";
#define STR_TOREPLACE_PREFIX_LEN (sizeof(g_str_torepl_prefix)-1) //-- exclude trailing zero
#define STR_TOREPLACE_SUFFIX_LEN   5  // 00001
int g_replace_blocks_ind  = 0;


char * g_file_rd_buf = NULL;
char * g_file_wr_buf = NULL;

FILE * g_hHdrFile     = NULL;
FILE * g_hArrFile     = NULL;
FILE * g_hHdrReplFile = NULL;
FILE * g_hBlkReplFile = NULL;

char g_out_file_name[MAX_PATH + 4];
char g_base_dir_name[MAX_PATH + 4];

//--- Local functions prototypes ------

void GetAppPath(char * in_buf, char * buf);
int open_wk_files(char * out_file_name);
int close_wk_files(char * out_file_name);
int scan_files_in_dir(char * base_dir_name);
int add_header(FILE * hOutFile, char * res_name_buf, char * arr_name, int file_len);
int make_res_name(char * full_file_name, char * base_dir_name, char * res_name_buf);
void make_arr_name(char * res_name_buf, char * arr_name);
int file_to_c_arr(FILE * hOutFile, char * arr_name, char * full_file_name);
int add_as_ascii(FILE * hOutFile, char * arr_name, char * start_ptr, char * end_ptr, int last_crlf);
int dir_proc(const char * in_path, char * base_dir_name);
int fix_path(const char * in_path, char * out_path);
void append_file(FILE * file_to_append);

//----------------------------------------------------------------------------
int _tmain(int argc, _TCHAR* argv[])
{
   int rc = 0;

   if(argc != 2)
   {
      printf("Usage: htm_to_c.exe full_dir_name\n");
      return -1;
   }
   GetAppPath(argv[0], g_out_file_name);
   strcat(g_out_file_name, "http_resources.c");

   strcpy(g_base_dir_name, argv[1]);

   rc = open_wk_files(g_out_file_name);
   if(rc != 0)
      return -1;

   scan_files_in_dir(g_base_dir_name);

   rc = close_wk_files(g_out_file_name);
   if(rc < 0)
      return -1;
	return 0;
}

//--------------------------------------------------------------------------
int open_wk_files(char * out_file_name)
{
  //-- Open arrays file & headers file

   g_hArrFile = fopen(out_file_name, "wb+");
   if(g_hArrFile == NULL)
      exit(-1);

   strcat(out_file_name,"_");
   g_hHdrFile = fopen(out_file_name, "wb+");
   if(g_hHdrFile == NULL)
   {
      exit(-1);
   }

   strcat(out_file_name,"_");
   g_hHdrReplFile = fopen(out_file_name, "wb+");
   if(g_hHdrReplFile == NULL)
   {
      exit(-1);
   }

   strcat(out_file_name,"_");
   g_hBlkReplFile = fopen(out_file_name, "wb+");
   if(g_hBlkReplFile == NULL)
   {
      exit(-1);
   }

  //-- Allocate memory

   g_file_rd_buf = (char*)malloc(FILE_RD_BUF_SIZE);
   if(g_file_rd_buf == NULL)
   {
      exit(-1);
   }
   g_file_wr_buf = (char*)malloc(FILE_WR_BUF_SIZE);
   if(g_file_wr_buf == NULL)
   {
      exit(-1);
   }

  //-- Write start record to the arrays file

   strcpy(g_file_wr_buf, "//\r\n//  HTTP resourses\r\n//\r\n"
            "//  The file was created automatically by \"htm_to_c.exe\""
            " utility.\r\n//\r\n\r\n\r\n");
   int len = strlen(g_file_wr_buf);
   int res = fwrite(g_file_wr_buf, 1, len, g_hArrFile);
   if(res != len)
      exit(-1);

  //-- Write start header to the headers file

   strcpy(g_file_wr_buf,"const HTTPDRESENTRY g_http_resources[] =\r\n{\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, g_hHdrFile);
   if(res != len)
      exit(-1);

  //-- Write start header to the repl file

   strcpy(g_file_wr_buf,"const HTTPDREPLACEFILES g_http_replace_resources[] =\r\n{\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, g_hHdrReplFile);
   if(res != len)
      exit(-1);


 //-- Write start header to the block repl file

   strcpy(g_file_wr_buf,"const HTTPDREPLACEBLOCK g_http_replace_fileparts[] =\r\n{\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, g_hBlkReplFile);
   if(res != len)
      exit(-1);

   g_file_wr_buf[0] = '\0';
   return 0; //-- OK
}



//--------------------------------------------------------------------------
int close_wk_files(char * out_file_name)
{
   int rc = 0;

   //-- Final record to the headers file

   strcpy(g_file_wr_buf, "   {\"\", NULL, 0}\r\n};\r\n\r\n\r\n");
   int len = strlen(g_file_wr_buf);
   int res = fwrite(g_file_wr_buf, 1, len, g_hHdrFile);
   g_file_wr_buf[0] = '\0';
   if(res != len)
      exit(-1);

   //-- Final record to the headers replace file

   strcpy(g_file_wr_buf, "   {\"\", 0}\r\n};\r\n\r\n\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, g_hHdrReplFile);
   g_file_wr_buf[0] = '\0';
   if(res != len)
      exit(-1);

   //-- Final record to the headers block file

   strcpy(g_file_wr_buf, "\r\n};\r\n\r\n\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, g_hBlkReplFile);
   g_file_wr_buf[0] = '\0';
   if(res != len)
      exit(-1);

   //-- Append a headers file to the arrays file

   append_file(g_hHdrFile);
   append_file(g_hHdrReplFile);
   append_file(g_hBlkReplFile);


   //-- Free memory & close files

   free(g_file_rd_buf);
   free(g_file_wr_buf);

   //-- g_hBlkReplFile

   fclose(g_hBlkReplFile);
   ::DeleteFile(out_file_name);
   len = strlen(out_file_name);
   out_file_name[len-1] = '\0'; //-- Remove trailer '_'

   //-- g_hHdrReplFile

   fclose(g_hHdrReplFile);
   ::DeleteFile(out_file_name);
   len = strlen(out_file_name);
   out_file_name[len-1] = '\0'; //-- Remove trailer '_'

   //-- g_hHdrFile

   fclose(g_hHdrFile);
   ::DeleteFile(out_file_name); //-- g_hHdrFile

   //--
   rc = 0;
   if(rc < 0) //-- If Err - delete g_hArrFile file also
   {
      len = strlen(out_file_name);
      out_file_name[len-1] = '\0'; //-- Remove trailer '_'
      ::DeleteFile(out_file_name);
   }

   return rc;
}

//--------------------------------------------------------------------------
void append_file(FILE * file_to_append)
{
   int len;
   int rc;
   fseek(file_to_append, 0, SEEK_SET);
   for(;;)
   {
      len = fread(g_file_rd_buf, 1, FILE_RD_BUF_SIZE, file_to_append);
      if(len == 0)
         break;
      rc = fwrite(g_file_rd_buf, 1, len, g_hArrFile);
      if(rc != len)
         exit(-1);
   }
}

//--------------------------------------------------------------------------
int processing_file(char * full_file_name, char * base_dir_name)
{
   int rc = 0;
   int res;
   int replace_num;
   char res_name_buf[(HTTPD_MAX_RES_NAME_LEN +1)*2];
   char arr_name[(HTTPD_MAX_RES_NAME_LEN+1)*2];
   char arr_name_block[(HTTPD_MAX_RES_NAME_LEN+1)*2];
   char s_buf[STR_TOREPLACE_PREFIX_LEN + STR_TOREPLACE_SUFFIX_LEN + 8];

   FILE * hInFile = NULL;
   BOOL fFindToReplace = FALSE;

   rc = make_res_name(full_file_name, base_dir_name, res_name_buf);
   if(rc != 0)
      return rc;
   make_arr_name(res_name_buf, arr_name);

 //--- Ptr to the file ext(if any) -----

   int len = strlen(res_name_buf);
   char * ext = NULL;
   if(len)
   {
      len--;
      while(len)
      {
         if(res_name_buf[len] == '.')
         {
            ext = &res_name_buf[len + 1];
            break;
         }
         len--;
      }
   }

//-------------------------

   if(ext != NULL && (strncmp(ext, "htm", 3) == 0 || strncmp(ext, "HTM", 3) == 0
                || strncmp(ext, "css", 3) == 0 || strncmp(ext, "CSS", 3) == 0))
   {
      hInFile = fopen(full_file_name, "rb");
      if(hInFile == NULL)
      {
         printf("Could not open file: %s\n", full_file_name);
         exit(-1);
      }

 //--- Len of the file and read it all

      fseek(hInFile, 0, SEEK_END);
      int filelen = ftell(hInFile);
      fseek(hInFile, 0, SEEK_SET);
      if(filelen == 0)
         return -1;
      char * file_buf = (char*)malloc(filelen + 1);
      if(file_buf == NULL)
      {
         fclose(hInFile);
         exit(-1);
      }
      rc = fread(file_buf, 1, filelen, hInFile);
      if(rc != filelen)
      {
         fclose(hInFile);
         free(file_buf);
         exit(-1);
      }
      file_buf[filelen] = '\0';
 //---------------------------------------------
     //-- find the replace word

      int block_num = 0;
      char * start_ptr = file_buf;
      char * end_ptr = file_buf + filelen - 1;
      char * prev_start_ptr = file_buf;
      for(;;)
      {
         start_ptr = strstr(start_ptr, g_str_torepl_prefix);
         if(start_ptr == NULL)
            break;
         end_ptr = start_ptr;
         start_ptr += STR_TOREPLACE_PREFIX_LEN;
         memcpy(s_buf, start_ptr, STR_TOREPLACE_SUFFIX_LEN);
         s_buf[STR_TOREPLACE_SUFFIX_LEN] = '\0';
         replace_num = atoi(s_buf);
         start_ptr += STR_TOREPLACE_SUFFIX_LEN;

         if(fFindToReplace == FALSE)
         {

           //-- add to g_http_replace_resources

            sprintf(g_file_wr_buf, "   {\"/%s\", %d},\r\n",
                      res_name_buf, g_replace_blocks_ind);

            len = strlen(g_file_wr_buf);
            res = fwrite(g_file_wr_buf, 1, len, g_hHdrReplFile);
            g_file_wr_buf[0] = '\0';
            if(res != len)
               exit(-1);
         }
        //-- add as block arr

         strcpy(arr_name_block, arr_name);
         rc = strlen(arr_name_block);
         sprintf(arr_name_block + rc, "_%d", block_num);

         rc = add_as_ascii(g_hArrFile, arr_name_block, prev_start_ptr, end_ptr, FALSE);

        //-- add to 'g_http_replace_blocks'

         if(fFindToReplace == FALSE)
         {
            fFindToReplace = TRUE;

            if(g_replace_blocks_ind == 0)
               sprintf(g_file_wr_buf, "   {&%s[0], %d, %d}",
                         arr_name_block, rc, replace_num);
            else
               sprintf(g_file_wr_buf, ",\r\n   {&%s[0], %d, %d}",
                         arr_name_block, rc, replace_num);
         }
         else
            sprintf(g_file_wr_buf, ",\r\n   {&%s[0], %d, %d}",
                  arr_name_block, rc, replace_num);

         len = strlen(g_file_wr_buf);
         res = fwrite(g_file_wr_buf, 1, len, g_hBlkReplFile);
         g_file_wr_buf[0] = '\0';
         if(res != len)
            exit(-1);

        //--------------------------------------
         prev_start_ptr = start_ptr;
         g_replace_blocks_ind++;
         block_num++;
      }

      if(fFindToReplace == FALSE) //-- There are no the replace word
      {
         rc = add_as_ascii(g_hArrFile, arr_name, prev_start_ptr, end_ptr, TRUE);
         rc = add_header(g_hHdrFile, res_name_buf, arr_name, rc); // rc is a filelen
      }
      else
      {
     //-- add the remainder of the file to the 'g_http_replace_blocks'
     //-- the replace word number will be 0 here

         strcpy(arr_name_block, arr_name);
         rc = strlen(arr_name_block);
         sprintf(arr_name_block + rc, "_%d", block_num);

         end_ptr = file_buf + filelen - 1;

         rc = add_as_ascii(g_hArrFile, arr_name_block, prev_start_ptr, end_ptr, TRUE);

        //-- add to 'g_http_replace_blocks'

         sprintf(g_file_wr_buf, ",\r\n   {&%s[0], %d, 0}",
                                                    arr_name_block, rc);

         len = strlen(g_file_wr_buf);
         res = fwrite(g_file_wr_buf, 1, len, g_hBlkReplFile);
         g_file_wr_buf[0] = '\0';
         if(res != len)
            exit(-1);

         g_replace_blocks_ind++;

         strcpy(g_file_wr_buf, ",\r\n   {NULL, 0, 0}");
         len = strlen(g_file_wr_buf);
         res = fwrite(g_file_wr_buf, 1, len, g_hBlkReplFile);
         g_file_wr_buf[0] = '\0';
         if(res != len)
            exit(-1);

         g_replace_blocks_ind++;
      }
   }
   else
   {
      rc = file_to_c_arr(g_hArrFile, arr_name, full_file_name);
      if(rc < 0) //- err
         return rc;
      rc = add_header(g_hHdrFile, res_name_buf, arr_name, rc); // rc is a filelen
   }
   return rc;
}

//--------------------------------------------------------------------------
int add_header(FILE * hOutFile, char * res_name_buf, char * arr_name, int file_len)
{
   sprintf(g_file_wr_buf, "   {\"/%s\", &%s[0], %d},\r\n",
             res_name_buf, arr_name, file_len);

   int len = strlen(g_file_wr_buf);
   int res = fwrite(g_file_wr_buf, 1, len, hOutFile);
   g_file_wr_buf[0] = '\0';
   if(res != len)
      return ERR_FILEWRITE;
   return 0;
}
//--------------------------------------------------------------------------
int make_res_name(char * full_file_name, char * base_dir_name, char * res_name_buf)
{
   int len1 = strlen(base_dir_name);
   int len2 = strlen(full_file_name);
   int n_len = len2 - len1;
   if(n_len > HTTPD_MAX_RES_NAME_LEN)
      return ERR_BIGLEN;
   memmove(res_name_buf, &full_file_name[len1+1], n_len);
   res_name_buf[n_len] = '\0';
   char * ptr = res_name_buf;
   while(*ptr != '\0')
   {
      if(*ptr == '\\')
         *ptr = '/';
      ptr++;
   }
   return 0;
}

//--------------------------------------------------------------------------
void make_arr_name(char * res_name_buf, char * arr_name)
{
   int i =0;
   unsigned char ch;
   for(;;)
   {
      ch = res_name_buf[i];
      if(ch == '\0')
      {
         arr_name[i] = '\0';
         break;
      }

      if(isalnum(ch))
         arr_name[i] = ch;
      else
         arr_name[i] = '_';
      i++;
   }
}
//--------------------------------------------------------------------------
int file_to_c_arr(FILE * hOutFile, char * arr_name, char * full_file_name)
{
   int rc;
   int res;
   int len;
   int i;
   int fEOF = FALSE;
   char buf[(HTTPD_MAX_RES_NAME_LEN+1)*2];

#define NUM_SYM_IN_LINE 16

   if(arr_name == NULL || full_file_name == NULL || hOutFile == NULL)
      return ERR_EINVAL;

   FILE * hInFile = fopen(full_file_name, "rb");
   if(hInFile == NULL)
      return ERR_FILEOPEN;

//--- First line
   strcpy(g_file_wr_buf,"const unsigned char ");
   strcat(g_file_wr_buf, arr_name);
   strcat(g_file_wr_buf, "[] = \r\n{\r\n");

   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, hOutFile);
   if(res != len)
   {
      fclose(hInFile);
      return ERR_FILEWRITE;
   }
   g_file_wr_buf[0] = '\0';
   int filelen = 0;
   int sym_in_line_cnt = 0;
   for(;;)
   {
      rc = fread(g_file_rd_buf, 1, FILE_RD_BUF_SIZE, hInFile);
      if(rc != FILE_RD_BUF_SIZE) //-- Last reading
         fEOF = TRUE;
      filelen += rc;
      for(i = 0; i < rc; i++)
      {
         if(fEOF && i == rc - 1) //-- Last symbol in the file
            sprintf(buf, "0x%02X\r\n", (unsigned char)g_file_rd_buf[i]);
         else
         {
            if(sym_in_line_cnt < NUM_SYM_IN_LINE - 1)
               sprintf(buf, "0x%02X,", (unsigned char)g_file_rd_buf[i]);
            else
               sprintf(buf, "0x%02X,\r\n", (unsigned char)g_file_rd_buf[i]);
         }
         if(sym_in_line_cnt == 0)
            strcat(g_file_wr_buf, "   ");
         strcat(g_file_wr_buf, buf);

         sym_in_line_cnt++;
         if(sym_in_line_cnt >= NUM_SYM_IN_LINE)
         {
            sym_in_line_cnt = 0;
            len = strlen(g_file_wr_buf);
            res = fwrite(g_file_wr_buf, 1, len, hOutFile);
            g_file_wr_buf[0] = '\0';
            if(res != len)
            {
               fclose(hInFile);
               return ERR_FILEWRITE;
            }
         }
      }
      if(fEOF)
         break;
   }
   //-- Remainder(if any)
   len = strlen(g_file_wr_buf);
   if(len > 0)
   {
      res = fwrite(g_file_wr_buf, 1, len, hOutFile);
      g_file_wr_buf[0] = '\0';
      if(res != len)
      {
         fclose(hInFile);
         return ERR_FILEWRITE;
      }
   }

//--- Last_line

   strcpy(g_file_wr_buf,"};\r\n\r\n\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, hOutFile);
   if(res != len)
   {
      fclose(hInFile);
      return ERR_FILEWRITE;
   }

   fclose(hInFile);

   return filelen; //-- OK
}

//----------------------------------------------------------------------------
int add_as_ascii(FILE * hOutFile,
                 char * arr_name,
                 char * start_ptr,
                 char * end_ptr,
                 int last_crlf)
{
   int i;
   int len;
   int res;
   int tlen;
   char * prev_start_ptr;

//--- First line
   strcpy(g_file_wr_buf,"const unsigned char ");
   strcat(g_file_wr_buf, arr_name);
   strcat(g_file_wr_buf, "[] = \r\n\r\n");

   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, hOutFile);
   if(res != len)
   {
      printf("fwrite() failed.\n");
      exit(-1);
   }

//--- Body

   prev_start_ptr = start_ptr;
   tlen = 0;
   while(start_ptr < end_ptr)
   {
     //--
      if(*start_ptr == '\r' && *(start_ptr+1) == '\n')
      {
         len = 4;
         res = fwrite("   \"",1, len, hOutFile);
         if(res != len)
         {
            printf("fwrite() failed.\n");
            exit(-1);
         }
         len = start_ptr - prev_start_ptr;
         tlen += len;
//-------------------------------------------
         for(i=0; i < len; i++)
         {
            int ch = prev_start_ptr[i];
            if(ch == '\"')
               fputc('\\',hOutFile);
            fputc(ch, hOutFile);
         }
/*
         res = fwrite(prev_start_ptr, 1, len, hOutFile);
         if(res != len)
         {
            printf("fwrite() failed.\n");
            exit(-1);
         }
*/
//----------------------------------------------
         //if(last_crlf && end_ptr - start_ptr <= 2
         strcpy(g_file_wr_buf,"\\r\\n\"");
         strcat(g_file_wr_buf,"\r\n");
         len = strlen(g_file_wr_buf);
         res = fwrite(g_file_wr_buf, 1, len, hOutFile);
         if(res != len)
         {
            printf("fwrite() failed.\n");
            exit(-1);
         }
         tlen += 2;

         start_ptr++;
         prev_start_ptr = start_ptr;
         prev_start_ptr++;
      }
      start_ptr++;
   }
//--- remainder (if any)
   len = start_ptr - prev_start_ptr;
   if(len > 0)
   {
      res = fwrite("   \"",1, 4, hOutFile);
      if(res != 4)
      {
         printf("fwrite() failed.\n");
         exit(-1);
      }
//-------------------------------
      tlen += len;

      for(i=0; i < len; i++)
      {
          int ch = prev_start_ptr[i];
          if(ch == '\"')
             fputc('\\',hOutFile);
          fputc(ch, hOutFile);
      }
/*
      res = fwrite(prev_start_ptr, 1, len, hOutFile);
      if(res != len)
      {
         printf("fwrite() failed.\n");
         exit(-1);
      }
*/
//-------------------------------
      if(last_crlf)
      {
         strcpy(g_file_wr_buf,"\\r\\n\"");
         tlen += 2;
      }
      else
         strcpy(g_file_wr_buf,"\"");
      strcat(g_file_wr_buf,"\r\n");
      len = strlen(g_file_wr_buf);
      res = fwrite(g_file_wr_buf, 1, len, hOutFile);
      if(res != len)
      {
         printf("fwrite() failed.\n");
         exit(-1);
      }
   }
//--- Last_line

   strcpy(g_file_wr_buf,";\r\n\r\n\r\n");
   len = strlen(g_file_wr_buf);
   res = fwrite(g_file_wr_buf, 1, len, hOutFile);
   if(res != len)
   {
      exit(-1);
   }

   return tlen; //-- OK

}

//----------------------------------------------------------------------------
int scan_files_in_dir(char * base_dir_name)
{
   dir_proc(base_dir_name, base_dir_name);

   return 0;
}

//-----------------------------------------------------------------------------
void GetAppPath(char * in_buf, char * buf)
{
   int ind;
   int i;
   char * ptr;
   int len;

   ptr = in_buf;
   len = strlen(ptr);
   ind = 0;

   if(ptr[ind] == '\"')
   {
      ind++;
      while(ptr[ind] != '\"' && ind < len)
         ind++;
      while(ptr[ind] != '\\' && ind >= 0)
         ind--;
      for(i = 0; i < ind; i++)
         buf[i] = ptr[i+1];
      buf[i] = 0;
   }
   else
   {
      while(ptr[ind] != '\"' && ind < len)
         ind++;
      while(ptr[ind] != '\\' && ind >= 0)
         ind--;
      for(i = 0; i < ind; i++)
         buf[i] = ptr[i];
      buf[i] = 0;
   }
}

//------------------------------------------------------------------------------------------------------------------
int fix_path(const char * in_path, char * out_path)
{
   int i = 0;

   strcpy(out_path, in_path);

   while(in_path[i])
      i++;

   if(in_path[i-1] != '\\')
   {
      strcat(out_path,"\\");
      return 1;
   }
   return 0;
}

//------------------------------------------------------------------------------------------------------------------
int dir_proc(const char * in_path, char * base_dir_name)
{
   HANDLE fh;
   WIN32_FIND_DATA fd;        //-- XXX use malloc() here!
   char path[MAX_PATH];       //-- XXX use malloc() here!
   char tmp_path[MAX_PATH];   //-- XXX use malloc() here!

   fix_path(in_path, path);
   strcat(path, "*");

   fh = ::FindFirstFile((LPCSTR)path, &fd);
   if(fh != INVALID_HANDLE_VALUE)
   {
      do
      {
         if(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
         {
            if(strcmp(fd.cFileName,".") != 0 && strcmp(fd.cFileName, "..") != 0)
            {
               fix_path(in_path, tmp_path);
               strcat(tmp_path, fd.cFileName);
               fix_path(tmp_path, tmp_path);

           //-- Recursion

               dir_proc(tmp_path, base_dir_name);
            }
         }
         else
         {
           //--- Do file operation
            strcpy(tmp_path, in_path);
            fix_path(tmp_path, tmp_path);
            strcat(tmp_path, fd.cFileName);

            processing_file(tmp_path, base_dir_name);
         }
      }while(::FindNextFile(fh, &fd));
   }
   ::FindClose(fh);
   return 1;
}

//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------------------------


