#include <stdio.h>
#include <stdlib.h>
#include <iconv.h>
#include <string.h>

/*
     Generate a GB2312 -> UTF8 table.
     
     The output format is:
     
     gb2312 utf-8 unicode char-in-utf8
     ----------------------------------
     
e.g.  b0a1  e5958a 21834     啊

     Assume: sizeof int >= 32 bit

     Written by LiGong, 2010/1

*/



/*

Background:

  GB2312
  
  1) Divides characters into 94 block, numbered from 1 to 94
  2) Each block includes 94 character, numbered from 1 to 94
  3) block 1-9 are symbols, block 16-55 are first class characters
     block 56-87 are the second class characters, other blocks are
     reserved
  4) consists of 2 bytes: first byte is 0xa0 + block number
     second byte is 0xa0 + character number in this block

     e.g. 啊 is the first character of 16 block, encoded as 0xb0a1



  UTF-8
  
  1) The first byte indicates the length of bytes
     one byte: the highest bit is 0
     two bytes: 110xxxxx
     three bytes: 1110xxxx
     four bytes: 1111xxxx
     The rest bits of first bytes contains the Unicode value
     
  2) For the rest bytes, the first 2 bits are always 10
     The rest bits contains the Unicode value

*/


#define GB2312_SIZE 2
#define UTF8_MAX 6
#define GB2312_OFFSET 0xa0

/* C's idiom to clear the most right bit */
#define REMOVE_R1(c) (c & (c-1))


#define UTF8_2_MASK 0xe0 
#define UTF8_3_MASK 0xf0 
#define UTF8_4_MASK 0xf8 

#define IS_UTF8_N(c,n) ((c & (UTF8_ ## n ## _MASK) ) == REMOVE_R1( UTF8_ ## n ## _MASK))

#define IS_UTF8_1(c) ((c & 0x80) == 0)
#define IS_UTF8_2(c) IS_UTF8_N(c,2)
#define IS_UTF8_3(c) IS_UTF8_N(c,3)
#define IS_UTF8_4(c) IS_UTF8_N(c,4)

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef unsigned char GB2312[GB2312_SIZE];
typedef unsigned char UTF8[UTF8_MAX];

/* assume sizeof(int)>=4, which is long enough to take the UNICODE */
typedef unsigned int UNICODE; 


void encodeGB2312(int block, int pos, GB2312 gb);
int gb2utf8(iconv_t cd, GB2312 gb, UTF8 utf);
UNICODE utf2unicode(UTF8 utf);
void printOutput(GB2312 gb, UTF8 utf, UNICODE uni);
  
void err_sys(char *msg);


int main ()
{

  int k;
  iconv_t cd;

  int GB2312_region_block[][2] = {{1,9},{16,87}};
  
  if ( (cd = iconv_open("utf-8","gbk")) == NULL){
    err_sys("Fail to open iconv");
  }

  
  /* 
     For all possible gb2312 code, if it can be converted to utf8,
     output it
  */

  for (k=0; k<ARRAY_SIZE(GB2312_region_block); k++) {
    
    int start=GB2312_region_block[k][0];
    int end=GB2312_region_block[k][1];
    int i, block;

    for (block=start; block<=end; block++) {
      for (i=1; i<=94; i++) {

	GB2312 gb;
	UTF8 utf;
	
	encodeGB2312(block,i,gb);
	if (gb2utf8(cd,gb,utf) > 0) {
	  printOutput(gb,utf,utf2unicode(utf));
	}
      }
    }
  }

  iconv_close(cd);

  exit(0);
}
    


void encodeGB2312(int block, int pos, GB2312 gb)
{
  gb[0] = block + GB2312_OFFSET;
  gb[1] = pos + GB2312_OFFSET;
}


int gb2utf8(iconv_t cd, GB2312 gb, UTF8 utf)
{
  char *buf_in = gb;
  char *buf_out = utf;
  size_t buf_in_left = sizeof(GB2312);
  size_t buf_out_left = sizeof(UTF8);

  if (iconv(cd,&buf_in,&buf_in_left,&buf_out,&buf_out_left) < 0) {
    return -1;
  }

  return sizeof(UTF8) - buf_out_left;
}


int length_utf8(UTF8 utf)
{
  int len;
  char c = utf[0];
  
  if (IS_UTF8_1(c)) {      /* highest bit is zero, so it is 1 byte char */
    len = 1;
  } else if (IS_UTF8_2(c)) {
    len = 2;
  } else if (IS_UTF8_3(c)) {
    len = 3;
  } else if (IS_UTF8_4(c)) {
    len = 4;
  } else {
    err_sys("utf2unicode: invalid utf");
  }

  return len;
}

  
UNICODE utf2unicode(UTF8 utf)
{
  UNICODE uni;
  unsigned char c = utf[0];
  int len;
  unsigned char *p;

  /* check the first byte, figure out UTF8 length and the value stored in first byte */
  
  if (IS_UTF8_1(c)) {      /* highest bit is zero, so it is 1 byte char */
    return (UNICODE) c;
  } else if (IS_UTF8_2(c)) {
    len = 2;
    uni = c & ~UTF8_2_MASK;
  } else if (IS_UTF8_3(c)) {
    len = 3;
    uni = c & ~UTF8_3_MASK;
  } else if (IS_UTF8_4(c)) {
    len = 4;
    uni = c & ~UTF8_4_MASK;
  } else {
    err_sys("utf2unicode: invalid utf");
  }

  /* for the rest bytes, the first 2 bits are 10, the value is in the next 6 bits */
  for(p=&utf[1]; p<utf+len; p++) {
    uni = (uni << 6) | (*p & 0x3f);  /*0x3f = 0b00111111*/
  }

  return uni;
}
  
void err_sys(char *msg)
{
  perror(msg);
  exit(1);
}


void printOutput(GB2312 gb, UTF8 utf, UNICODE uni)
{
  int len = length_utf8(utf);
  int i;

  printf("%2x%2x ", gb[0], gb[1]);
  
  for(i=0; i<len; i++) {
    printf("%2x", utf[i]);
  }
  printf(" ");

  printf("%u ", (unsigned int) uni);

  if (fwrite(utf,len,1,stdout) != 1)
    printf("Write utf failue\n");

  printf("\n");
  
}

  
