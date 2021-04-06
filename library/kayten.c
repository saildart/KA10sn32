/* path /data/KA.Ten.lib/kayten.c
 * ======================================================================================================= 110
 *                      K       A       Y               T       E       N
 * ======================================================================================================= 110
 *                      gcc -g -m64 -c kayten.c
123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789..123456789.
// Bruce Guenther Baumgart, copyright©2019-2020, license GPLv3.
 */
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> 

#include "kayten.h"

// PDP-10 KA Priority Interrupt, PI,
// lookup tables for bit and level
// where "highest priority" is numeric value 1
//  and  "lowest priority"  is numeric value 7
const uint16 pi_bit[8] = { 0, 0100, 0040, 0020, 0010, 0004, 0002, 0001 };
const uint16 pi_level[128] = {
 0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4,
 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
 };

opcode_attributes_t op_attr[ 768 ]; // indexed by narrow op code 713. < (512+256) = 768.
void init_opcode_attributes(){
#include "attributes.c"
}
char *op_string[]={
#include "user-opcode-strings.c"
#include "narrow_code_uppercase_strings.c"
};
#include "opname_using_big_case_stmt.c"

int verbose=0;
int quiet=0;
int bamboo=0;

d10 **vm; // 2^18 PDP-10 words of virtual memory READ permitted.
d10 **vw; // 2^18 PDP-10 words of virtual memory WRITE permitted.
d10   *m; // 2^18 PDP-10 words of physical memory which in 1974 was 2usec magnetic core.

// PDP-10 sixbit character decoding tables (and the mod64 xx encoding table)
// SIXBIT tables
//           1         2         3         4         5         6     Decimal Ruler
//  1234567890123456789012345678901234567890123456789012345678901234
//  0       1       2       3       4       5       6       7      7 Octal Ruler
//  0123456701234567012345670123456701234567012345670123456701234567
//   !"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\]^_ SIXBIT character values
//                              omit comma for bare csv fields
//                                   ↓ omit dot for FILNAM fields
//           Backwhack ↓             ↓ ↓                                       Backwhack ↓
char *sixbit_table= " !\"" "#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ" "[\\]^_"; // actual SIXBIT decode
char *sixbit_fname= " ! "  "#$%&'()*+_-_/0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ" "___^_"; // SAIL filenames sanity
char *sixbit_uname= " B "  "#$%APLRX+Y-DZ0123456789CSN=MW@abcdefghijklmnopqrstuvwxyz" "IQOV_"; // unix filenames sanity
char *sixbit_ppn  = " __"  "_____________0123456789_______ABCDEFGHIJKLMNOPQRSTUVWXYZ" "_____"; // Project Programmer SAFE
char *sixbit_sail = "_ab"  "cdefghijklmno0123456789pqrstuvABCDEFGHIJKLMNOPQRSTUVWXYZ" "wxyz-"; // base64 for SAIL bit exact CSV

char *sixbit_mod64= "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"; // modulo 64 naive URL safe
char *sixbit_xx   = "+-0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
char *sixbit_rfc  = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz-_"; // rfc3548 filename URL safe
char *sixbit4648  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"; // rfc4648 pad with =
char *sixURL4648  = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"; // rfc4648 pad with = URL safe

char *
sixbit_word_into_ascii(char *p0,d10 d)
{
  pdp10_word_t w = (pdp10_word_t)d;
  char *p=p0;
  *p++ = (w.sixbit.c1 + 040);
  *p++ = (w.sixbit.c2 + 040);
  *p++ = (w.sixbit.c3 + 040);
  *p++ = (w.sixbit.c4 + 040);
  *p++ = (w.sixbit.c5 + 040);
  *p++ = (w.sixbit.c6 + 040);
  *p++= 0;
  return p0;
}
char *
sixbit_halfword_into_ascii(char *p0,int halfword)
{
  char *p=p0;
  *p++ = ((halfword >> 12) & 077) + 040;
  *p++ = ((halfword >> 6) & 077) + 040;
  *p++ = (halfword & 077) + 040;
  *p++ = 0;
  return p0;
}

// 'C' utf8 string for each 7-bit SAIL ASCII character code.
// used to convert SAIL7 into 'C' printable
char *SAIL7CODE_utf8[]={
  "\\0",  "↓",   "α",   "β",   "∧",   "¬",   "ε",   "π", //     007 
  "λ",   "\\t",  "\\n",  "\\v",  "\\f",  "\\r",  "∞",   "∂", //     017 
  "⊂",   "⊃",   "∩",   "∪",   "∀",   "∃",   "⊗",   "↔", //     027 
  "_",   "→",   "~",   "≠",   "≤",   "≥",   "≡",   "∨", //     037 
  " ","!","\"","#","$","%","&","'",     // 040 to 047
  "(",")","*","+",",","-",".","/",      // 050 to 057
  "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?", // 060 to 077
  "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z", // 0100 to 0132
  "[",   "\\",  "]",   "↑",   "←",   "`", // 0140
  "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", // 0141 to 0172
  "{",   "|",   "§",   "}",   "\b",// 0177 Stanford rubout character
};
char *SAIL7CODE_look[]={
  "",  "↓",  "α",  "β",  "∧",  "¬",  "ε",  "π", //     007 
  "λ",  "\t",  "\n",  "\v",  "\f",  "\r",  "∞",   "∂", //     017 
  "⊂",   "⊃",   "∩",   "∪",   "∀",   "∃",   "⊗",   "↔", //     027 
  "_",   "→",   "~",   "≠",   "≤",   "≥",   "≡",   "∨", //     037 
  " ","!","\"","#","$","%","&","'",     // 040 to 047
  "(",")","*","+",",","-",".","/",      // 050 to 057
  "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?", // 060 to 077
  "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z", // 0100 to 0132
  "[",   "\\",  "]",   "↑",   "←",   "`", // 0140
  "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", // 0141 to 0172
  "{",   "|",   "§",  "}",  "\b",// 0177 Stanford rubout character
};
char *SAIL7CODE_iii[]={
  "",  "↓",  "α",  "β",  "∧",  "¬",  "ε",  "π", "λ",
  "",   // The III processor ignored tabs.
  "\n",
  "∫", // VT displays as integral U222B
  "±", // FF displays as plusminus U00B1
  "\r",  "∞",   "∂", //     017 
  "⊂",   "⊃",   "∩",   "∪",   "∀",   "∃",   "⊗",   "↔", //     027 
  "_",   "→",   "~",   "≠",   "≤",   "≥",   "≡",   "∨", //     037 
  " ","!","\"","#","$","%","&","'",     // 040 to 047
  "(",")","*","+",",","-",".","/",      // 050 to 057
  "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?", // 060 to 077
  "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z", // 0100 to 0132
  "[",   "\\",  "]",   "↑",   "←",   "`", // 0140
  "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", // 0141 to 0172
  "{",   "|",   "§",  "}",
  "~",  // the III processor displays 0177 as tilde !
};
char *SAIL7CODE_lpt[]={
  "·", // LPT_000 center dot U00B7
  "↓",  "α",  "β",  "∧",  "¬",  "ε",  "π", "λ",         // 001 to 010
  "γ", // LPT_011 TB prints U03B3 small gamma
  "δ", // LPT_012 LF prints U03B4 small delta
  "∫", // LPT_013 VT prints U222B integral sign
  "±", // LPT_014 FF prints U00B1 plus-minus sign
  "⊕", // LPT_015 CR prints U2295 circle-plus sign
  "∞",   "∂", // 016, 017 
  "⊂",   "⊃",   "∩",   "∪",   "∀",   "∃",   "⊗",   "↔", //     027 
  "_",   "→",   "~",   "≠",   "≤",   "≥",   "≡",   "∨", //     037 
  " ","!","\"","#","$","%","&","'",     // 040 to 047
  "(",")","*","+",",","-",".","/",      // 050 to 057
  "0","1","2","3","4","5","6","7","8","9",":",";","<","=",">","?", // 060 to 077
  "@","A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z", // 0100 to 0132
  "[",   "\\",  "]",   "↑",   "←",   "`", // 0140
  "a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", // 0141 to 0172
  "{",   "|",   "§",  "}",
  "\\", // the LPT prints backslash
};

// Lester keyboard keycodes into SAIL7BIT ASCII
char pro_dkb[]={
    0,0,0,0,                // 000 /  .... not generated by keyboard
    0141, 0101, 0034, 0034, // 001 /  'a'	'A'    '\034'  '\034'
    0142, 0102, 0007, 0007, // 002 /  'b'	'B'    '\007'  '\007'
    0143, 0103, 0006, 0006, // 003 /  'c'	'C'    '\006'  '\006'
    0144, 0104, 0074, 0074, // 004 /  'd'	'D'	  '<'	  '<'
    0145, 0105, 0100, 0100, // 005 /  'e'	'E'	  '@'	  '@'
    0146, 0106, 0076, 0076, // 006 /  'f'	'F'	  '>'	  '>'
    0147, 0107, 0033, 0033, // 007 /  'g'	'G'    '\033'  '\033'
    0150, 0110, 0075, 0075, // 010 /  'h'	'H'	  '='	  '='
    0151, 0111, 0047, 0047, // 011 /  'i'	'I'	  '''	  '''
    0152, 0112, 0137, 0137, // 012 /  'j'	'J'	'_'	'_'
    0153, 0113, 0031, 0031, // 013 /  'k'	'K'    '\031'  '\031'
    0154, 0114, 0027, 0027, // 014 /  'l'	'L'    '\027'  '\027'
    0155, 0115, 0025, 0025, // 015 /  'm'	'M'    '\025'  '\025'
    0156, 0116, 0024, 0024, // 016 /  'n'	'N'    '\024'  '\024'
    0157, 0117, 0173, 0173, // 017 /  'o'	'O'	  '{'	  '{'
    0160, 0120, 0176, 0176, // 020 /  'p'	'P'	  '}'	  '}'
    0161, 0121, 0004, 0004, // 021 /  'q'	'Q'    '\004'  '\004'
    0162, 0122, 0043, 0043, // 022 /  'r'	'R'	  '#'	  '#'
    0163, 0123, 0035, 0035, // 023 /  's'	'S'    '\035'  '\035'
    0164, 0124, 0046, 0046, // 024 /  't'	'T'	  '&'	  '&'
    0165, 0125, 0140, 0140, // 025 /  'u'	'U'	  '`'	  '`'
    0166, 0126, 0010, 0010, // 026 /  'v'	'V'    '\010'  '\010'
    0167, 0127, 0037, 0037, // 027 /  'w'	'W'    '\037'  '\037'
    0170, 0130, 0003, 0003, // 030 /  'x'	'X'    '\003'  '\003'
    0171, 0131, 0042, 0042, // 031 /  'y'	'Y'	  '"'	  '"'
    0172, 0132, 0002, 0002, // 032 /  'z'	'Z'    '\002'  '\002'
    0015, 0015, 0015, 0015, // 033 / RETURN
    0134, 0134, 0016, 0016, // 034 /  '\'	'\'	 '\016'	 '\016'
    0012, 0012, 0012, 0012, // 035 / LINE feed
    0,0,0,0,                // 036 / .... not generated by keyboard
    0,0,0,0,                // 037 / .... not generated by keyboard
    0040, 0040, 0040, 0040, // 040 / SPACEBAR
    0,0,0,0,                // 041 / BREAK
    0,0,0,0,                // 042 / ESCAPE
    0000, 0000, 0000, 0000, // 043 / CALL 0600 0600 0600 0600
    0,0,0,0,                // 044 / CLEAR
    0011, 0011, 0011, 0011, // 045 / TAB
    0014, 0014, 0014, 0014, // 046 / FORM
    0013, 0013, 0013, 0013, // 047 / VT
    0050, 0050, 0133, 0133, // 050 /  '('	'('	  '['	  '['
    0051, 0051, 0135, 0135, // 051 /  ')'	')'	  ']'	  ']'
    0052, 0052, 0026, 0026, // 052 /  '*'	'*'	 '\026'	 '\026'
    0053, 0053, 0174, 0174, // 053 /  '+'	'+'	  '|'	  '|'
    0054, 0054, 0041, 0041, // 054 /  ','	','	  '!'	  '!'
    0055, 0055, 0005, 0005, // 055 /  '-'	'-'	 '\005'	 '\005'
    0056, 0056, 0077, 0077, // 056 /  '.'	'.'	  '?'	  '?'
    0057, 0057, 0017, 0017, // 057 /  '/'	'/'	 '\017'	 '\017'
    0060, 0060, 0060, 0060, // 060 /  '0'	'0'	  '0'	  '0'
    0061, 0061, 0036, 0036, // 061 /  '1'	'1'    '\036'  '\036'
    0062, 0062, 0022, 0022, // 062 /  '2'	'2'    '\022'  '\022'
    0063, 0063, 0023, 0023, // 063 /  '3'	'3'    '\023'  '\023'
    0064, 0064, 0020, 0020, // 064 /  '4'	'4'    '\020'  '\020'
    0065, 0065, 0021, 0021, // 065 /  '5'	'5'    '\021'  '\021'
    0066, 0066, 0044, 0044, // 066 /  '6'	'6'	  '$'	  '$'
    0067, 0067, 0045, 0045, // 067 /  '7'	'7'	  '%'	  '%'
    0070, 0070, 0032, 0032, // 070 /  '8'	'8'    '\032'  '\032'
    0071, 0071, 0030, 0030, // 071 /  '9'	'9'    '\030'  '\030'
    0072, 0072, 0001, 0001, // 072 /  ':'	':'    '\001'  '\001'
    0073, 0073, 0136, 0136, // 073 /  ';'	';'	  '^'	  '^'
    0177, 0177, 0177, 0177, // 074 / BACKSPACE
    0175, 0175, 0175, 0175, // 075 / 0175 Stanford ALT, not ASCII '}'
    0,0,0,0,                // 076 / .... not generated by keyboard
    0,0,0,0,                // 077 / .... not generated by keyboard
};
// SAIL7BIT ASCII into Lester Keyboard Keycodes for DKB keyboard scan hardware
// shift 0100 = 64.
//  top  0200
// ctrl  0400
// meta 01000
int anti_dkb[]={
  0000, 0272, 0232, 0230,  0136, 0255, 0203, 0202, // null ↓ α β   ^ ¬ ε π
  0226, 0045, 0035, 0047,  0046, 0033, 0234, 0257, // λ tab lf vt  ff cr ∞ ∂
  0264, 0265, 0263, 0262,  0216, 0215, 0252, 0214, //  ⊂ ⊃ ∩ ∪     ∀ ∃ ⊗ ↔
  0271, 0213, 0270, 0207,  0201, 0223, 0261, 0227, //  _ → ~ ≠     ≤ ≥ ≡ ∨
  040,  0254, 0251, 0222,  0266, 0267, 0224, 0211, //  ␣ ! " #     $ % & ' 
  0050, 0051, 0052, 0053,  0054, 0055, 0056, 0057, //  ( ) * +     , - . /
  0060, 0061, 0062, 0063,  0064, 0065, 0066, 0067, // 0123  4567
  0070, 0071, 0072, 0073,  0204, 0210, 0206, 0256, // 89:;  <=>?
  //  @ ABCDEFGHIJKLMNOPQRSTUVWXYZ                                                     [    \    ]    ↑    ←
  0205, 65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90, 0250,0034,0251,0273,0212,
  //  ` abcdefghijklmnopqrstuvwxyz                                                     {    |    §    }    ⌫
  0225, 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,          0217,0253,0075,0220,0074,
};

int
load_dump_file(core_image_t *C,char *dmp_pathname){
  int n,fd,p,q;
  a10 pc;
  struct stat statbuf;

  // Initialize 768. slots of op_attr[] array.
  // Lookup on narrow op code enums, narr,
  init_opcode_attributes();
  
  // Input a core image from data8 DMP file
  fd = open(dmp_pathname,O_RDONLY);
  if (fd<0){
    ERROR("ERROR: open file %s failed.\n",dmp_pathname);
    exit(1);
  }
  stat(dmp_pathname,&statbuf);
  n = statbuf.st_size;
  // JOBSYM slot is at 0116 the DMP files skip 074 pdp-10 words
  if ( (n/8) <= 32){
    ERROR("file is '%s' too short.\n",dmp_pathname);
    exit(0);
  }
  C->word_count = 074 + n/8;
  VERBOSE("READ   dmp_pathname %s filesize=%d bytes into %d. PDP10 words\n",dmp_pathname,n,C->word_count);
  C->memory =   calloc( C->word_count, 8 );
  C->psymstr =  calloc( C->word_count, 8 );
  C->symbolic = calloc( C->word_count, sizeof(char*));
  C->disassembled = (dasmline *)calloc( C->word_count, sizeof(dasmline)); // space for disassembly text
  n = read( fd, C->memory+074, statbuf.st_size );
  close(fd);
  
  // Initialize scratch memory for execution at TOY user level
#if 0
  m  = (void *)C->memory;
  vm = calloc( C->word_count, 8 );
  vw = calloc( C->word_count, 8 );
#else
  {
    // NOTE local word_count
    int word_count = 1<<18; // The 18. bit address space has 262144. PDP-10 words.
    m  = calloc( word_count, 8);  // actual memory for dynamic content which may change by execution
    vm = calloc( word_count, 8 ); // virtual user read addresses or NULL pointer
    vw = calloc( word_count, 8 ); // virtual user write addresses of NULL pointer
  }
#endif
 
  C->ddt =              C->memory[074].half.right;
  C->raid_version =    (C->memory[074].half.left >> 5);
  C->start_address =   C->memory[0120].half.right;
  C->first_unused  =   C->memory[0120].half.left;
  C->sym_address_low = C->memory[0116].half.right;
  C->sym_count =      -C->memory[0116].sign.left;
  C->sym_address_high = C->sym_address_low + C->sym_count;  
  
  VERBOSE("Disassemble \"%s\" %6d. words, user=%d\n", dmp_pathname, C->word_count, C->user );
  //    VERBOSE("    JOBDAT / %012llo \n", C->memory[0120].full.word );
  // VERBOSE("    JOBSA  / %012llo \n", (uint64)C->memory[0120].full.word );
  if(C->ddt){
    VERBOSE("     %10s at %06o \n", (C->raid_version ? "RAID" : "DDT"), C->ddt);
  }else{
    VERBOSE("Neither DDT nor RAID is present.\n");
  }    
  VERBOSE(  "   Start Address %06o \n", C->start_address );
  if(C->first_unused >= C->sym_address_high){
    VERBOSE("   Symbols Address %06o \n", C->sym_address_low );
    VERBOSE("   Symbols count   %6d.\n",  C->sym_count );
    VERBOSE("   Symbols  Top    %06o \n", C->sym_address_high);
  }else{
    C->sym_address_low = 0;
    C->sym_count = 0;
    VERBOSE("   No Symbol Table\n");
  }
  VERBOSE("   Start Address %06o \n", C->start_address );
  VERBOSE("   First unused  %06o \n", C->first_unused  );  
  
  //  m[0121] = (word_count+074)+1;    // JOBFF First Free (unused) location
  C->memory[044].half.right = ( C->memory[0121].half.right | 0777 );
  pc = C->memory[0120].half.right; // start address from JOBDAT slot JOBSA
  VERBOSE("start address is %06o\n",pc);

  VERBOSE("JOBREL %06o %06o\n", C->memory[ 044].half.left, C->memory[ 044].half.right );
  VERBOSE("JOBFF  %06o %06o\n", C->memory[0121].half.left, C->memory[0121].half.right );
  VERBOSE("JOBHRL %06o %06o\n", C->memory[0115].half.left, C->memory[0115].half.right );
  VERBOSE("HILOC  %06o %06o\n", C->memory[0135].half.left, C->memory[0135].half.right );
 
  // fetch the memory boundaries from user JOBDAT locations
  C->lower_bot = 0;
  C->lower_top = C->memory[044].half.right; // right side of JOBREL
  
  // upper segment when JOBHRL is non-zero
  if(C->memory[0115].half.right){
    C->upper_bot = 0400000;
    C->upper_top = C->memory[0115].half.right;
    C->upper_segment_read_only = (SIGN & C->memory[0115].full.word) ? TRUE : FALSE;
  }else{
    C->upper_bot = 0;
    C->upper_top = 0;
    C->upper_segment_read_only = 0;
  }
  // USER address space is coincidant with physical memory array for the Toy.Ten
  // however unallocated core will cause Memory-Protection interrupt 
  // lower segment (Toy.Ten includes the Accumulators)
  for(q=0, p=C->lower_bot; p<=C->lower_top; p++,q++){
    m[q] = C->memory[p].full.word; // 36-bits from DMP image into low order of d10 word
    vm[q] = &m[p]; // read pointer
    vw[q] = &m[p]; // write pointer
  }  
  return 1;
}

// do easy disassembly decoding markup
void
memory_scan(core_image_t *C){
  uint32 ma;
  uint32 ea;
  uint32 lo = 0140;
  uint32 hi = C->sym_address_low ? C->sym_address_low : C->first_unused;
  // JOBDAT area
  for (ma=074; ma<0140; ma++){
    C->memory[ma].flow.narr = n_zero;    
  }
  for (ma=lo; ma<hi; ma++){
    int narr = narrow_opcode( C->user, C->memory+ma );
    C->memory[ma].flow.narr = narr;
    switch(narr){
    case n_init:
      C->memory[ma+1].flow.narr = n_zero;
      C->memory[ma+1].flow.sixbit = 1; // 'device'
      C->memory[ma+2].flow.narr = n_zero;
      C->memory[ma+2].flow.octal = 1; // OBUF,,IBUF
      ma += 2; // skip 2 words
      break;
    case n_lookup:
      ea = C->memory[ma].half.right;
      if(!( ea+3 < hi )) break;
      C->memory[ea+0].flow.narr = n_zero;
      C->memory[ea+0].flow.sixbit = 1; // 'filnam'
      C->memory[ea+1].flow.narr = n_zero;
      C->memory[ea+1].flow.sixbit = 1; // 'ext'
      C->memory[ea+2].flow.narr = n_zero;
      C->memory[ea+2].flow.octal = 1; // ignored
      C->memory[ea+3].flow.narr = n_zero;
      C->memory[ea+3].flow.sixbit = 1; // 'ppn'
      break;
    }
  }
}

void
dasm_mark(){
};

void
dasm_make( core_image_t *C, FILE *o ){
  uint32 ma;
  uint32 lo = 074;
  uint32 hi = C->sym_address_low ? C->sym_address_low : C->first_unused;
  pdp10_word_t w;
  int narr;
  
  uint op,a,i,x,y,l,r;
  char tagstr[12]; // label tag:                        post fix colon          or empty
  char  a_str[12]; // accumulator register field        post fix comma          or empty
  char  i_str[12]; // indirect addressing bit           at-sign                 or empty
  char  x_str[12]; // index register field              parentheses (embedded)  or empty
  char  y_str[12]; // address field                     symbolic or octal       or empty  
  char line_str[128];
  
  for (ma=lo; ma<hi; ma++){
    // narrow opcode pre decode word format
    w = C->memory[ma];
    narr = w.flow.narr;
    // is there a symbolic tag: for this ma location ?
    if( C->symbolic[ma] ){
      sprintf(tagstr,"%s:",C->symbolic[ma]);
    } else {
      tagstr[0] = 0;
    }
    if (narr == n_zero ){ 
      if(TRUE || w.flow.octal){ // default line string is octal
        sprintf( line_str,
                 "%06o / %06o %06o "  // 22  addr / octal left right
                 " %7.7s ",           //  8  TAG:
                 ma, w.half.left, w.half.right, tagstr ); // octal dump tag
      }
      if(w.flow.asciz){
        sprintf( line_str,
                 "%06o / %06o %06o "  // 22  addr / octal left right
                 "%7.7s "            //  8  TAG:
                 "\"%s%s%s%s%s\"", //  7  SAIL 7-bit ASCII string literal delimited by double quotes
                 ma, w.half.left, w.half.right, tagstr,  // octal dump tag
                 SAIL7CODE_utf8[w.seven.a1],
                 SAIL7CODE_utf8[w.seven.a2],
                 SAIL7CODE_utf8[w.seven.a3],
                 SAIL7CODE_utf8[w.seven.a4],
                 SAIL7CODE_utf8[w.seven.a5] );
      }
      if(w.flow.sixbit){
        sprintf( line_str,
                 "%06o / %06o %06o "  // 22  addr / octal left right
                 "%7.7s "            //  8  TAG:
                 "'%c%c%c%c%c%c'", //  7  SAIL 6-bit ASCII string literal delimited by single quotes
                 ma, w.half.left, w.half.right, tagstr,  // octal dump tag
                 sixbit_fname[w.sixbit.c1],
                 sixbit_fname[w.sixbit.c2],
                 sixbit_fname[w.sixbit.c3],
                 sixbit_fname[w.sixbit.c4],
                 sixbit_fname[w.sixbit.c5],
                 sixbit_fname[w.sixbit.c6] );
      }
      if(o)fprintf(o,"%s\n",line_str);
      strncpy( C->disassembled[ma], line_str, sizeof(dasmline));
      // fprintf(stderr,"ASCII literal word %s\n",line_str);
      continue;
    }
    op= w.instruction.op;
    a = w.flow.no_a ? 0 : w.instruction.a; // AC field may be used for operation decoding
    i = w.instruction.i;
    x = w.instruction.x;
    y = w.flow.no_y ? 0 : w.instruction.y; // Y address field may be used for operation decoding
    
    l = w.half.left;
    r = w.half.right;

    // ac Accumulator or device code
    if(a){
      sprintf(a_str,"%o,",a);
    }else
      a_str[0]=0; // empty
    
    // xr Index Register
    if(x){
      sprintf(x_str,"(%o)",x);
    }else
      x_str[0]=0; // empty

    // indirect bit representation
    sprintf(i_str, i ? "@" : "");
    
    // is there a symbolic tag: for this y address ?
    if( y < C->sym_address_low && y >= 0140
        &&  C->symbolic[y] ){
      sprintf(y_str,"%s",C->symbolic[y]);
    } else {
      if(y<0777000)
        sprintf(y_str,"%6o",y);
      else
        sprintf(y_str,"-%o",-w.sign.right);
    }
    if(w.flow.no_y || (!y && op_attr[narr].omiteazero)){
      y_str[0]=0; // empty y field
    }
    // negative constant integer is NOT a conso instruction
    if( l==0777777 && w.sign.right<0){
      sprintf(y_str,"-%o",-w.sign.right);
      narr=0;
      a_str[0]=0;
      i_str[0]=0;
      x_str[0]=0;
    }
    sprintf( line_str,                               // width
             "%06o / %06o %06o "                     // 23      addr / octal left right
             "%7.7s "                                //  8      TAG:
             "%-6.6s "                               //  7      opcode
             "%5.5s"  "%1.1s"  "%6.6s"  "%s",        // 12+3    A I Y X
             ma, w.half.left, w.half.right, tagstr,  // octal dump tag
             narr ? op_string[narr] : "",            // opcode
             a_str,   i_str,   y_str,   x_str );     // operands
    if(o)fprintf(o,"%s\n",line_str);
    strncpy( C->disassembled[ma], line_str, sizeof(dasmline));
    // mark attributes of this instruction
    C->memory[ma].flow.stepper = op_attr[narr].skip;
    C->memory[ma].flow.jumper  = op_attr[narr].jump;
    // mark operand attributes
    if( !i && !x ){ // when NO address modification
      switch(narr){
      case n_outstr:
        // fprintf(stderr,"mark OUTSTR operand %0o as ASCII ",y);
        { uint y0=y;
          int b;
          do {
            w = C->memory[y0];
            C->memory[y0++].flow.asciz=1; // mark ASCIZ words
            b = w.seven.a1 && w.seven.a2 && w.seven.a3 && w.seven.a4 && w.seven.a5 ;

            // fprintf(stderr,"b=%d y0=%06o ",b,y0);
          } while ( // until the ZERO byte is found.
                   w.seven.a1 &&
                   w.seven.a2 &&
                   w.seven.a3 &&
                   w.seven.a4 &&
                   w.seven.a5 );
          C->memory[y0-1].flow.stopper=1; // ZERO byte terminates string.
        }
        // fprintf(stderr,"\n");
        break;
      default:
        break;
      } // switch
    } // no address modification
  } // memory scan loop
} // dasm_make

void
dasm_write( core_image_t *C, FILE *o ){
  uint32 ma;
  uint32 lo = 074; // decimal 60.
  uint32 hi = C->sym_address_low ? C->sym_address_low : C->first_unused;
  pdp10_word_t w;
  uint y;

  if(!o)return; // do nothing
  for (ma=lo; ma<hi; ma++){
    fprintf(o,"%s",C->disassembled[ma]);
    if(0) // disabled
    if( C->memory[ma].flow.narr == n_outstr ){
      w = C->memory[ma];
      y = w.instruction.y;
      if(!w.instruction.i && !w.instruction.x){
        char *p,str[16];
        fprintf(o,";\"");
        do{
          bzero(str,16);
          strncpy(str, C->disassembled[y++]+31, 10); // each SAIL7 characters could be two bytes wide.
          p = rindex(str,'"');
          if(p) *p='\0'; // omit final double quote
          fprintf(o,"%s",str);
        }while(!strstr(str,"\\0")); // The representation for NUL is '\' then '0'
        fprintf(o,"\"");        
      }
    }
    fprintf(o,"\n");
  }  
}

void
read_symbol_table(core_image_t *C){
  uint32 ma;
  char *symp = C->psymstr;

  // adhoc JOBDAT
  C->symbolic[0000020]="JOBDAC";
  C->symbolic[0000023]="JOBDPD";
  C->symbolic[0000027]="JOBDPG";
  C->symbolic[0000040]="JOBUUO";
  C->symbolic[0000042]="JOBERR";
  C->symbolic[0000043]="JOBENB";
  C->symbolic[0000044]="JOBPDL";
  C->symbolic[0000044]="JOBREL";
  C->symbolic[0000045]="JOBBLT";
  C->symbolic[0000071]="JOBINT";
  C->symbolic[0000072]="JOBHCU";
  C->symbolic[0000072]="JOBPRT";
  C->symbolic[0000073]="JOBPC";
  C->symbolic[0000073]="JOBSAV";
  C->symbolic[0000074]="JOBDDT";
  C->symbolic[0000075]="JOBJDA";
  C->symbolic[0000114]="JOBPFI";
  C->symbolic[0000115]="JOBHRL";
  C->symbolic[0000116]="JOBSYM";
  C->symbolic[0000117]="JOBUSY";
  C->symbolic[0000120]="JOBSA";
  C->symbolic[0000121]="JOBFF";
  C->symbolic[0000122]="JOBS41";
  C->symbolic[0000123]="JOBEXM";
  C->symbolic[0000124]="JOBREN";
  C->symbolic[0000125]="JOBAPR";
  C->symbolic[0000126]="JOBCNI";
  C->symbolic[0000127]="JOBTPC";
  C->symbolic[0000130]="JOBOPC";
  C->symbolic[0000131]="JOBCHN";
  C->symbolic[0000132]="JOBFDV";
  C->symbolic[0000133]="JOBCOR";
  C->symbolic[0000134]="HINAME";
  C->symbolic[0000135]="HILOC";
  C->symbolic[0000140]="JOBDA";
  
  if(!strcmp(C->jobname,"LESCAL")){ // adhoc symbols for LESCAL
      C->symbolic[000140]="POP0";
    C->symbolic[000142]="PREP";
    C->symbolic[000160]="PRIP";
    C->symbolic[000173]="PIN";
    C->symbolic[000201]="POUT";
    C->symbolic[000211]="CATER"; 
    C->symbolic[000423]="START";
  }
  if(!strcmp(C->jobname,"DPYCLK")){ // adhoc symbols for DPYCLK
      C->symbolic[000140]="PI";
    C->symbolic[000141]="TIME";
    C->symbolic[000142]="DATE";
    C->symbolic[000143]="S";
    C->symbolic[000146]="GO";
    C->symbolic[000203]="HOUR";
    C->symbolic[000236]="MINUTE";
    C->symbolic[000272]="SECOND";
    C->symbolic[000330]="D";
    C->symbolic[000334]="A";
    C->symbolic[000377]="C"; 
    C->symbolic[000342]="DPY";
    C->symbolic[000377]="MONTHS"; 
    C->symbolic[000413]="READ";

    C->symbolic[000416]="FOO";
    C->symbolic[000420]="FOO1";
    C->symbolic[000422]="FOO2"; 
    C->symbolic[000424]="DPY1";
    C->symbolic[000432]="DPY2";
    C->symbolic[000440]="POINT1"; 
    C->symbolic[000441]="POINT2";
  
    C->symbolic[000500]="COS";
    C->symbolic[000505]="SIN";
  }
  // in core DMP file symbols
  if(!(C->first_unused >= C->sym_address_high)){
    VERBOSE("No Symbol Table in DMP\n");
    return;
  }  
  VERBOSE("sym_address_low=%06o sym_address_high=%06o sym_count=%d.\n",
          C->sym_address_low,
          C->sym_address_high,
          C->sym_count );
  //
  for (ma=C->sym_address_low; ma<C->sym_address_high; ma+=2){
    char *p,strbuf[12];
    int n, xxxx, addr;
    pdp10_word_t mb;
    pdp10_word_t mb1;
    uint64 w;
    int c1,c2,c3,c4,c5,c6;
    //
    mb  = C->memory[ma];
    mb1 = C->memory[ma+1];
    xxxx = mb.symbol.type; // symbol type 4 bits
    w    = mb.symbol.text; // RAD50 text in 32 bits
    addr = mb1.half.right; // 18-bit address
    if( !xxxx || !w ){ // not a program label tag
      continue;
    }
    // decode Radix 050 into ASCII. Unicode NOT needed here.
#define RADTAB " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_$%"
    c6 = RADTAB[w % 40];w=w/40;
    c5 = RADTAB[w % 40];w=w/40;
    c4 = RADTAB[w % 40];w=w/40;
    c3 = RADTAB[w % 40];w=w/40;
    c2 = RADTAB[w % 40];w=w/40;
    c1 = RADTAB[w % 40];w=w/40;
#undef RADTAB      
    sprintf(strbuf,"%c%c%c%c%c%c",c1,c2,c3,c4,c5,c6); // RAD50      
    // omit leading spaces
    for(p=strbuf;*p==' ';p++)
      ;
    n = strlen(p);
    // tag candidate
    if (   addr > 0
           && addr < C->sym_address_low
           && ( addr > 0140 || !strncmp("JOB",p,3))
           //           && mb1.half.left==0
           ){
      C->symbolic[addr] = strcpy( symp, p );
      symp += n+1;
    }
    // noisy diagnostic message
    if(verbose>=2){
      fprintf(stderr,"#%5d %c%c%c%c symbol %6.6s:=%012llo %06o %06o %s\n",
              (1+ ma - C->sym_address_low),
              xxxx & 0x8 ? 'x' : '.',
              xxxx & 0x4 ? 'x' : '.',
              xxxx & 0x2 ? 'x' : '.',
              xxxx & 0x1 ? 'x' : '.',
              strbuf,
              (d10)mb.full.word,
              (a10)mb1.half.left,
              (a10)mb1.half.right,
              addr < C->sym_address_low && C->symbolic[addr] ? C->symbolic[addr] : ""
              );
    }
  }
}

void fatal(const char *msg)
{
    perror(msg);
    exit(0);
}
#define THEEND 0165332
char *labtab[THEEND+1];
char *urltab[THEEND+1];

// URL table, machine address --> FILENAME#line
void read_urltab(){
  FILE *Table;
  int ma,cnt=0;
  char *str;
  // int j;
  Table=fopen("/data/do.run.run./DATA/urltab","r");
  if(!Table)fatal("fopen DATA/urltab");
  //    for(i=0;i<9;i++){
  //          j= fscanf(Table,"0%6o,%m[A-Z0-9#],\n",&ma,&str);
  str = calloc(1,20);
  //      j= fscanf(Table,"0%6o,%[A-Z0-9#],\n",&ma,str);
  while(2==fscanf(Table,"0%6o,%[A-Z0-9#],\n",&ma,str)){
    urltab[ma]=str;
    str=calloc(1,20);
    cnt++;
    //      fprintf(stderr,"init urltab i=%d ma=%o str='%s'\n",cnt,ma,urltab[ma]);
  }
  fclose(Table);
  VERBOSE("initialized urltab %5d lines\n",cnt);
}

// Label table, machine address --> symbolic label tag
// translate 'FAIL' symbolic characters '$'  '%'  '.'
//      into  'C'   symbolic lowercase  'd'  'p'  '_'
void read_labtab(){
  FILE *Table;
  int ma,cnt=0;
  char *str;
  Table=fopen("/data/do.run.run./DATA/labtab","r");
  if(!Table)fatal("fopen DATA/labtab");
  str=calloc(1,8);
  while(2==fscanf(Table,"0%6o,%[A-Z0-9dp_],\n",&ma,str)){
    labtab[ma]=str;
    cnt++;
    str=calloc(1,8);
  }
  fclose(Table);
  VERBOSE("initialized labtab %5d lines\n",cnt);
}

void
iso_date(char *dest,int sysdate,int systime){
  int day,month,year,hour,minute,second;
  int dpm[]={0,31,28,31, 30,31,30, 31,31,30, 31,30,31}; // days per month
  // 123456789.1234
  // 00-00-00 00:00
  sprintf(dest,"%-20s","null");
  sprintf(dest,"_%06d_%06d_",sysdate,systime);

  // preview
  day   =  sysdate % 31 + 1;
  month = (sysdate / 31) % 12 + 1;  
  year  = (sysdate / 31) / 12 + 1964;
  hour  = systime / 60;
  minute= systime % 60;
  /* bamboo
    if(sysdate>0 && ((sysdate < (1968-1964)*372) ||  (sysdate > (1991-1964)*372) || (systime > 1440 )))
    {
        fprintf( stderr,"%7d / sysdate=%d systime=%d ",record,sysdate,systime);
        fprintf( stderr,"iso_date defect year %d month %d day %d hour %d minute %d\n",year,month,day,hour,minute);
    }  
  */
  if(sysdate < (1968-1964)*372) return; // below floor
  if(sysdate > (1991-1964)*372) return; // above ceiling
  if(systime > 1440 ) return;           // over the wall
  
  // Note 31 times 12 is 372.
  //    sysdate = (year - 1964)*372 + (month-1)*31 + (day-1)
  //    systime = hour*60 + minute
  day   =  sysdate % 31 + 1;
  month = (sysdate / 31) % 12 + 1;  
  year  = (sysdate / 31) / 12 + 1964;

  // Day of month too big ?
  if( day > ((month!=2) ? dpm[month] : (year%4 == 0) ? 29 : 28)) return; // bad day-of-month

  hour = systime / 60;
  minute = systime % 60;
  second = 0;

  // Hour two big ?
  if(hour >= 24){ hour=23; minute=59; } // fake it
  
  // Check for the 23-hour Day Light Savings Spring Forward dates
  // increment one hour for times between 02:00 AM and 02:59 AM inclusive
  switch(sysdate){
  case   920: //  24-Apr-1966
  case  1310: //  30-Apr-1967
  case  1692: //  28-Apr-1968
  case  2075: //  27-Apr-1969
  case  2458: //  26-Apr-1970
  case  2841: //  25-Apr-1971
  case  3230: //  30-Apr-1972
  case  3613: //  29-Apr-1973
  case  3878: //   6-Jan-1974 Nixon oil crisis Daylight-Savings-Time
  case  4311: //  23-Feb-1975 ditto (but Ford was president in 1975)
  case  4761: //  25-Apr-1976
  case  5144: //  24-Apr-1977
  case  5534: //  30-Apr-1978
  case  5917: //  29-Apr-1979
  case  6299: //  27-Apr-1980
  case  6682: //  26-Apr-1981
  case  7065: //  25-Apr-1982
  case  7448: //  24-Apr-1983
  case  7837: //  29-Apr-1984
  case  8220: //  28-Apr-1985
  case  8603: //  27-Apr-1986
  case  8965: //   5-Apr-1987
  case  9347: //   3-Apr-1988
  case  9730: //   2-Apr-1989
  case 10113: //   1-Apr-1990
    if(hour==2){ // 2 AM clocks SPRING forward ... for GNU/linux that hour is INVALID.
      hour++; // work around for the impossible datetime stamp values
    }
    break;
  default: break;
  }
  // No adjustment for leap seconds.
  // ================================================================================
  // From 1972 to 1991 there were seventeen positive LEAP SECONDS
  // which occured at GMT 00:00:00 on 1 January 1972 to 1980, 1988, 1990, 1991.
  //           and at GMT 00:00:00 on 1 July 1972, 1982, 1983 and 1985.
  // There were no LEAP SECOND in 1984, 1986, 1987 and 1989.
  //    On 1 January 1972 the difference (TAI-UTC) was 10 seconds, and
  //    on 1 January 1991 the difference (TAI-UTC) was 26 seconds.
  // Although NTP Network-Time-Protocol overlaps the SAILDART epoch,
  // the computer date time was normally set from somebody's wristwatch
  // after a building power failure. The PDP10 clock interrupt was driven
  // at 60 hertz by powerline cycles, the Petit electronic clock likely
  // had a crystal oscillator but was set by software based on a wristwatch reading.
  // =================================================================================
  // No adjustment for leap seconds.
  sprintf( dest,"%4d-%02d-%02dT%02d:%02d:%02d",year,month,day,hour,minute,second );
}
