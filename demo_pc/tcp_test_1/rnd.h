#ifndef _RNDMWC_H_
#define _RNDMWC_H_
typedef struct _RNDMWC
{
   unsigned int a;
   unsigned int x;
   unsigned int c;
   unsigned int ah;
   unsigned int al;
}RNDMWC;

void mwc_init(RNDMWC * mwc, unsigned int xi, unsigned int ci);
unsigned int mwc_rand(RNDMWC * mwc);

#endif

