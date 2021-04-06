#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
 
typedef struct {
  char mask;    /* char data will be bitwise AND with this */
  char lead;    /* start bytes of current char in utf-8 encoded character */
  uint32_t beg; /* beginning of codepoint range */
  uint32_t end; /* end of codepoint range */
  int bits_stored; /* the number of bits from the codepoint that fits in char */
}utf_t;
 
utf_t * utf[] = {
  /*             mask        lead        beg      end       bits */
  [0] = &(utf_t){0b00111111, 0b10000000, 0,       0,        6    },
  [1] = &(utf_t){0b01111111, 0b00000000, 0000,    0177,     7    },
  [2] = &(utf_t){0b00011111, 0b11000000, 0200,    03777,    5    },
  [3] = &(utf_t){0b00001111, 0b11100000, 04000,   0177777,  4    },
  [4] = &(utf_t){0b00000111, 0b11110000, 0200000, 04177777, 3    },
  &(utf_t){0},
};
 
/* All lengths are in bytes */
int codepoint_len(const uint32_t cp); /* len of associated utf-8 char */
int utf8_len(const char ch);          /* len of utf-8 encoded char */
 
char *to_utf8(const uint32_t cp);
uint32_t to_cp(const char chr[4]);
 
int codepoint_len(const uint32_t cp)
{
  int len = 0;
  for(utf_t **u = utf; *u; ++u) {
    if((cp >= (*u)->beg) && (cp <= (*u)->end)) {
      break;
    }
    ++len;
  }
  if(len > 4) /* Out of bounds */
    exit(1);
 
  return len;
}
 
int utf8_len(const char ch)
{
  int len = 0;
  for(utf_t **u = utf; *u; ++u) {
    if((ch & ~(*u)->mask) == (*u)->lead) {
      break;
    }
    ++len;
  }
  if(len > 4) { /* Malformed leading byte */
    exit(1);
  }
  return len;
}
 
char *to_utf8(const uint32_t cp)
{
  static char ret[5];
  const int bytes = codepoint_len(cp);
 
  int shift = utf[0]->bits_stored * (bytes - 1);
  ret[0] = (cp >> shift & utf[bytes]->mask) | utf[bytes]->lead;
  shift -= utf[0]->bits_stored;
  for(int i = 1; i < bytes; ++i) {
    ret[i] = (cp >> shift & utf[0]->mask) | utf[0]->lead;
    shift -= utf[0]->bits_stored;
  }
  ret[bytes] = '\0';
  return ret;
}
 
uint32_t to_cp(const char chr[4])
{
  int bytes = utf8_len(*chr);
  int shift = utf[0]->bits_stored * (bytes - 1);
  uint32_t codep = (*chr++ & utf[bytes]->mask) << shift;
 
  for(int i = 1; i < bytes; ++i, ++chr) {
    shift -= utf[0]->bits_stored;
    codep |= ((char)*chr & utf[0]->mask) << shift;
  }
 
  return codep;
}
 
int main(void)
{
  const uint32_t *in, input[] = {
    // 0x0041, 0x00f6, 0x0416, 0x20ac, 0x1d11e, 0x2193,
    0x2193, // ↓ down arrow
    0x03b1, // α alpha
    0x03b2, // β beta
    0x2227, // ∧ boolean AND
    0x00ac, // ¬ boolean NOT
    0x03b5, // ε epsilon
    0x03c0, // π pi
    0x03bb, // λ lambda          
    //         011,    012,    013,    014,    015,   // TAB, LF, VT, FF, CR as white space
    0x21E5, // ht ⇥ 011 horizontal tab
    0x21B4, // lf ↴ 012 linefeed
    0x2913, // vt ⤓ 013 vertical tab
    0x21ca, // ff ⇊ 014 formfeed
    0x21b5, // cr ↵ 015 carriage return          
    0x221e, // ∞ infinity
    0x2202, // ∂ partial differential
    0x2282, 0x2283, 0x2229, 0x222a, // ⊂ ⊃ ∩ ∪
    0x2200, 0x2203, 0x2297, 0x2194, // ∀ ∃ ⊗ ↔
    0x005f, 0x2192, 0x007e, 0x2260, // _ → ~ ≠
    0x2264, 0x2265, 0x2261, 0x2228, // ≤ ≥ ≡ ∨
    040,041,042,043,044,045,046,047,
    050,051,052,053,054,055,056,057,
    060,061,062,063,064,065,066,067,
    070,071,072,073,074,075,076,077,
    0100,0101,0102,0103,0104,0105,0106,0107,
    0110,0111,0112,0113,0114,0115,0116,0117,
    0120,0121,0122,0123,0124,0125,0126,0127,
    0130,0131,0132,0133,0134,0135,         
    0x2191, // ↑ up arrow
    0x2190, // ← left arrow
    0140,0141,0142,0143,0144,0145,0146,0147,
    0150,0151,0152,0153,0154,0155,0156,0157,
    0160,0161,0162,0163,0164,0165,0166,0167,
    0170,0171,0172,
    0x7b,     // { left curly bracket
    0x7c,     // | vertical bar
    0xa7,     // SAIL altmode § character
    0x7d,          // } right curly bracket ASCII octal 0175, but SAIL code 0176 !
    0x2408 ,        // Backspace

    0x3196, // Top ㆖
          
    0x0};
 
  printf("Character  Unicode  UTF-8 encoding (hex)\n");
  printf("----------------------------------------\n");
 
  char *utf8;
  uint32_t codepoint;
  for(in = input; *in; ++in) {
    utf8 = to_utf8(*in);
    codepoint = to_cp(utf8);
    printf("%s          U+%-7.4x", utf8, codepoint);
 
    for(int i = 0; utf8[i] && i < 4; ++i) {
      printf("%hhx ", utf8[i]);
    }
    printf("\n");
  }
  return 0;
}
