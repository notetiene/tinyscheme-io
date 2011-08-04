#/* 
# * Base64 decoder
# * Copyright (c) 2011 A. Carl Douglas
# *
# * To compile the test program:
# *   make -f b64.c
# */

# /* Makefile

b64: b64.c
	gcc -Wall -ansi -o b64 b64.c -DBASE64_TEST

clean:
	rm b64

define c_source
# */

#include <stdio.h>
#include <string.h>

/* subtract 40 from base64 ASCII representation to get index in this table */
static unsigned char decode_tab[] = {
  -1, -1, -1, 62, -1, -1, -1, 63, 
  52, 53, 54, 55, 56, 57, 58, 59, 
  60, 61, -1, -1, -1, -1, -1, -1,
  -1,  0,  1,  2,  3,  4,  5,  6,
   7,  8,  9, 10, 11, 12, 13, 14, 
  15, 16, 17, 18, 19, 20, 21, 22, 
  23, 24, 25, -1, -1, -1, -1, -1,
  -1, 26, 27, 28, 29, 30, 31, 32,
  33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48,
  49, 50, 51, -1, -1, -1, -1, -1 
};


/*
 * dst should be 3/4 the size of src
 */
void 
base64_decode(char *src, char *dst) {
  unsigned char           *ptr = (unsigned char *)src;
  unsigned char           *out = (unsigned char *)dst;
  int i;
  for (; *ptr && (*ptr != '='); ) {
    unsigned char blk[4] = { 0 };
    for (i = 0; *ptr && (*ptr != '=') && (i < 4); i++, ptr++) {
      blk[i] = decode_tab[*ptr-40];
      switch(i){
        case 1: *out++ = ((blk[0]&0x3f) << 2) | ((blk[1]&0x30) >> 4); break;
        case 2: *out++ = ((blk[1]&0x0f) << 4) | ((blk[2]&0x3c) >> 2); break;
        case 3: *out++ = ((blk[2]&0x03) << 6) | ((blk[3]&0x3f) >> 0); break;
      }
    }
  }
  *out   = '\0';
}

#ifdef BASE64_TEST
int main (int argc, char *argv[]) {

   char *tst[] = {
    "TWFu",
    "cGxlYXN1cmUu",
    "bGVhc3VyZS4=",
    "ZWFzdXJlLg==",
    "YXN1cmUu",
    "c3VyZS4="
  };
   char *exp[] = {
    "Man",
    "pleasure.",
    "leasure.",
    "easure.",
    "asure.",
    "sure."
  };
  int i;
  for (i = 0; i < 6; i++) {
    char res[64];
    base64_decode(tst[i], res);
    printf("'%s' = '%s' : %s\n",
        tst[i], res, strcmp(res, exp[i]) == 0 ? "PASS" : "FAIL");
  }

  return 0;
}
#endif

/* back to makefile
endef
#*/

