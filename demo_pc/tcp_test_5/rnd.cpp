#include "stdafx.h"
#include "rnd.h"

//-- Based on George Marsaglia "A very fast and very good random number generator"
//-- http://groups.google.com/group/sci.math/msg/fa034083b193adf0?hl=en&lr&ie=UTF-8&pli=1


//----------------------------------------------------------------------------
// multiply-with-carry generator x(n) = a*x(n-1) + carry mod 2^32.
// period = (a*2^31)-1


//static unsigned int a=1791398085;
//-- Choose a value for a from this list
 //  1791398085 1929682203 1683268614 1965537969 1675393560
 //  1967773755 1517746329 1447497129 1655692410 1606218150
 //  2051013963 1075433238 1557985959 1781943330 1893513180
 //  1631296680 2131995753 2083801278 1873196400 1554115554

//static unsigned int x=30903, c=0, ah, al;


//----------------------------------------------------------------------------
void mwc_init(RNDMWC * mwc, unsigned int xi, unsigned int ci)
{
   mwc->a = 1791398085;
   mwc->x = 30903;
   mwc->c = 0;

   if(xi != 0)
     mwc->x = xi;
   mwc->c = ci;
   mwc->ah = mwc->a >> 16;
   mwc->al = mwc->a & 0xFFFF;
}

//----------------------------------------------------------------------------
unsigned int mwc_rand(RNDMWC * mwc)
{
   unsigned int xh = mwc->x >> 16;
   unsigned int xl = mwc->x & 0xFFFF;

   mwc->x = mwc->x * mwc->a + mwc->c;
   mwc->c = xh * mwc->ah + ((xh * mwc->al) >> 16) + ((xl * mwc->ah) >> 16);
   if(xl * mwc->al >= ~mwc->c + 1)
      mwc->c++;  //-- thanks to Don Mitchell for this correction
   return mwc->x;
}

