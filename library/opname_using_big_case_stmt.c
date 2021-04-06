// FILE: opname_using_big_case_stmt.c

// The WIDE opcode is 36 bits wide.
// Isolate instruction fields of the word
// return zero for all illegal and undefined operations
uint64 wide_opcode(int user,pdp10_word_t *w){
  uint64 fw,opword, uuword, opcode;
  uint32 op,ac,y;
  fw = w->full.word;
  opcode = ( fw & 0777000000000 );
  op = w->instruction.op;
  y =  w->instruction.y;
  ac =  w->instruction.a;
  opword = ( fw & 0777000777777 ); // omit AC, I and X fields for CALLI subcodes.
  uuword = ( fw & 0777740000000 ); // omit I, X and Y fields for IOT, UUO and flagged Jxxx
  if(user){
    switch(op){
    case 0047: return ((y>044 && y<0400000) || y>0400111) ? 0 : opword;         // CALLI
    case 0051: return uuword;                                                   // TTYUUO
    case 0254: return uuword; // JRST flag names: JRST HALT JRSTF JEN
    case 0255: return uuword; // JFCL flag names: JOV JCRY0 JCRY1 JCRY JFOV
    case 0702: return ac<=4   ? uuword : 0; // PPIOT
    case 0710: return ac<=4   ? uuword : 0; // MAIL
    case 0711: return ac<=016 ? uuword : 0; // PTYUUO
    case 0715: return ac<=4   ? uuword : 0; // PGIOT
    case 0723: return ac<=5   ? uuword : 0; // INTUUO
    case 0773: return ac==1 || ac==5 ? uuword : 0; // for <CONI PCLK,time> or <DATI PCLK,date>
    default:
      return(opcode);
    }
  }else{
    // Monitor
    if(op>=0700) opcode = ( fw & 0700340000000 ); // classic PDP-10 KA input-output opcodes
    // io_device = ( w & 0077400000000 ) >> 24; // SAIL 9-bit device code NOT needed here
    switch(op){
    case 0254: // JRST flag names: JRST HALT JRSTF JEN
    case 0255: // JFCL flag names: JOV JCRY0 JCRY1 JCRY JFOV
      return(uuword);
    default:
      return(opcode);
    }    
  }
}

/*
  Return a narrow enum < 999. for the operation found in 36-bit instruction.

  The narrow opcodes are a fictional USER API for a 10-bit opcode PDP-10

        narrow_enum.c

  The usual machine operation codes are 0. to 511.
  further code names are simply enumerated from 512. to 713. inclusive
  including the eight classic KA-ten PDP-6 I/O instructions
  as well as the almost 200. SAIL-WAITS-1974 named UUO / IOTS of the user API.
 */
int narrow_opcode(int user,pdp10_word_t *w){
  // uint64 fw;
  // uint64 opword, uuword, opcode;
  uint32 op,ac,y;
  // fw = w->full.word;
  // opcode = ( fw & 0777000000000 );
  op = w->instruction.op;
  ac = w->instruction.a;
  y  = w->instruction.y;
  // opword = ( fw & 0777000777777 ); // omit AC, I and X fields for CALLI subcodes.
  // uuword = ( fw & 0777740000000 ); // omit  Y, I and X fields for IOT, UUO and flagged Jxxx
  if(op>=0700){ // u_iot700
    w->flow.no_a = 1;
    // monitor mode the Classic PDP-10 KA input/output instructions
    if(!user){
      return 01000 + (ac & 7); // Narrow n_ blki datai blko datao cono coni consz conso
    }
    // user mode WAITS-1974 J17 Time Sharing System API interface UUO and IOT codes
    switch(op){
    case 0702: return ac<=4   ? ac + n_ppsel  : 0; // PPIOT     u_ppiot
    case 0710: return ac<=4   ? ac + n_send   : 0; // MAIL      u_send
    case 0711: return ac<=016 ? ac + n_ptyget : 0; // PTYUUO    u_ptyuuo
    case 0715: return ac<=4   ? ac + n_pgsel  : 0; // PGIOT     u_pgiot
    case 0723: return ac<=5   ? ac + n_intjen : 0; // INTUUO    u_intuuo
    case 0773: return ac==5 ? n_pctime : ac==1 ? n_pcdate : 0; // for <CONI PCLK,time> or <DATI PCLK,date>
    default: // the remaining WAITS user IOTs NOT using the AC field 
      w->flow.no_a = 0;     
      return ( 0701<=op && op<=0723 ? (op-0701)+520 : 0 );
    }
  }
  switch(op){
  case 0047: // u_calli
    w->flow.no_y = 1;    
    return y <= 044 ? n_reset + y                               // CALLI 000...
      : y >= 0400000 && y<= 0400111 ? n_spwbut + (y & 0177)     // CALLI 400...
      : 0 ;
  case 0051: // u_ttyuuo
    w->flow.no_a = 1;
    return ac + n_inchrw ; // TTYUUO
  case 0254: // u_jrst JRST flags. Op names: JRST HALT JRSTF JEN
    w->flow.no_a = 1;
    switch(ac){
    case 0: return op; // the common go to is JRST
    case 2: return n_jrstf;
    case 4: return n_halt;
    case 012: return n_jen;
    default:
      w->flow.no_a = 0;
      return n_jrst; // executible BUT not a recognized flag pattern
    }
  case 0255: // JFCL flags. Op names: JOV JCRY0 JCRY1 JCRY JFOV
    w->flow.no_a = 1;
    switch(ac){
    case 0:
      if(!y) w->flow.no_y = 1; // bare JFCL
      return n_jfcl;
    case 1: return n_jfov;
    case 2: return n_jcry1;
    case 4: return n_jcry0;
    case 6: return n_jcry;
    case 8: return n_jov;
    default:
      return n_jfcl; // executible BUT not a recognized flag pattern
    }
  default:
    return(op);
  }
}

char *opname_case(uint64 w); // Forward
char *opname(int user,uint64 w){
  uint64 opword, uuword, opcode;
  int op;
  opcode = ( w & 0777000000000 );
  op = opcode>>27;
  opword = ( w & 0777000777777 ); // omit AC, I and X fields for CALLI subcodes.
  uuword = ( w & 0777740000000 ); // omit I, X and Y fields for IOT, UUO and flagged Jxxx
  if(user){
    switch(op){
    case 0047: // CALLI
      return(opname_case(opword));
    case 0051: // TTYUUO
    case 0254: // JRST flag names: JRST HALT JRSTF JEN
    case 0255: // JFCL flag names: JOV JCRY0 JCRY1 JCRY JFOV
    case 0702: // PPIOT
    case 0710: // MAIL
    case 0711: // PTYUUO
    case 0715: // PGIOT
    case 0723: // INTUUO
      return(opname_case(uuword));
    default: return(opname_case(opcode));
    }
  }else{
    if(op>=0700) opcode = ( w & 0700340000000 ); // classic PDP-10 KA input-output opcodes
    // io_device = ( w & 0077400000000 ) >> 24; // SAIL 9-bit device code NOT needed here
    switch(op){
    case 0254: // JRST flag names: JRST HALT JRSTF JEN
    case 0255: // JFCL flag names: JOV JCRY0 JCRY1 JCRY JFOV
      return(opname_case(uuword));
    default:
      return(opname_case(opcode));
    }    
  }
}

/*
  See Appendix 8 on page 219 of the "UUO Manual" SAILON 55.3 December 1973
  by Martin Frost and others: REG, JAM, TED, BH, RPH, FW and TES.
  http://www.saildart.org/TABLE.PUB[DOC,ME]
  which is appended at the end of this file.
*/
char *opname_case(uint64 w){
  switch(w){
  case 0000000000000: return("ZERO");
  case 0001000000000: return("U001");
  case 0002000000000: return("U002");
  case 0003000000000: return("U003");
  case 0004000000000: return("U004");
  case 0005000000000: return("U005");
  case 0006000000000: return("U006");
  case 0007000000000: return("U007");
  case 0010000000000: return("U010");
  case 0011000000000: return("U011");
  case 0012000000000: return("U012");
  case 0013000000000: return("U013");
  case 0014000000000: return("U014");
  case 0015000000000: return("U015");
  case 0016000000000: return("U016");
  case 0017000000000: return("U017");
  case 0020000000000: return("U020");
  case 0021000000000: return("U021");
  case 0022000000000: return("U022");
  case 0023000000000: return("U023");
  case 0024000000000: return("U024");
  case 0025000000000: return("U025");
  case 0026000000000: return("U026");
  case 0027000000000: return("U027");
  case 0030000000000: return("U030");
  case 0031000000000: return("U031");
  case 0032000000000: return("U032");
  case 0033000000000: return("U033");
  case 0034000000000: return("U034");
  case 0035000000000: return("U035");
  case 0036000000000: return("U036");
  case 0037000000000: return("U037");
    //
  case 0040000000000: return("CALL");
  case 0041000000000: return("INIT");
  case 0042000000000: return("U042");
  case 0043000000000: return("U043");
  case 0044000000000: return("U044");
  case 0045000000000: return("U045");
  case 0046000000000: return("U046");
    // CALLI 0047
  case 0050000000000: return("OPEN");
    // TTYUUO 0051
  case 0052000000000: return("U052");
  case 0053000000000: return("U053");
  case 0054000000000: return("U054");
  case 0055000000000: return("RENAME");
  case 0056000000000: return("IN");
  case 0057000000000: return("OUT");
  case 0060000000000: return("SETSTS");
  case 0061000000000: return("STATO");
  case 0062000000000: return("STATUS");
  case 0063000000000: return("GETSTS");
  case 0064000000000: return("STATZ");
  case 0065000000000: return("INBUF");
  case 0066000000000: return("OUTBUF");
  case 0067000000000: return("CLOSE");
  case 0070000000000: return("RELEAS");
  case 0071000000000: return("MTAPE");
  case 0072000000000: return("UGETF");
  case 0073000000000: return("USETI");
  case 0074000000000: return("USETO");
  case 0075000000000: return("LOOKUP");
  case 0076000000000: return("ENTER");
  case 0077000000000: return("U077");
    //
  case 0100000000000: return("U100");
  case 0101000000000: return("U101");
  case 0102000000000: return("U102");
  case 0103000000000: return("U103");
  case 0104000000000: return("U104");
  case 0105000000000: return("U105");
  case 0106000000000: return("U106");
  case 0107000000000: return("U107");
  case 0110000000000: return("U110");
  case 0111000000000: return("U111");
  case 0112000000000: return("U112");
  case 0113000000000: return("U113");
  case 0114000000000: return("U114");
  case 0115000000000: return("U115");
  case 0116000000000: return("U116");
  case 0117000000000: return("U117");
  case 0120000000000: return("U120");
  case 0121000000000: return("U121");
  case 0122000000000: return("U122");
  case 0123000000000: return("U123");
  case 0124000000000: return("U124");
  case 0125000000000: return("U125");
  case 0126000000000: return("U126");
  case 0127000000000: return("U127");
  case 0130000000000: return("UFA");
  case 0131000000000: return("DFN");
  case 0132000000000: return("FSC");
  case 0133000000000: return("IBP");
  case 0134000000000: return("ILDB");
  case 0135000000000: return("LDB");
  case 0136000000000: return("IDPB");
  case 0137000000000: return("DPB");
  case 0140000000000: return("FAD");
  case 0141000000000: return("FADL");
  case 0142000000000: return("FADM");
  case 0143000000000: return("FADB");
  case 0144000000000: return("FADR");
  case 0145000000000: return("FADRI");
  case 0146000000000: return("FADRM");
  case 0147000000000: return("FADRB");
  case 0150000000000: return("FSB");
  case 0151000000000: return("FSBL");
  case 0152000000000: return("FSBM");
  case 0153000000000: return("FSBB");
  case 0154000000000: return("FSBR");
  case 0155000000000: return("FSBRI");
  case 0156000000000: return("FSBRM");
  case 0157000000000: return("FSBRB");
  case 0160000000000: return("FMP");
  case 0161000000000: return("FMPL");
  case 0162000000000: return("FMPM");
  case 0163000000000: return("FMPB");
  case 0164000000000: return("FMPR");
  case 0165000000000: return("FMPRI");
  case 0166000000000: return("FMPRM");
  case 0167000000000: return("FMPRB");
  case 0170000000000: return("FDV");
  case 0171000000000: return("FDVL");
  case 0172000000000: return("FDVM");
  case 0173000000000: return("FDVB");
  case 0174000000000: return("FDVR");
  case 0175000000000: return("FDVRI");
  case 0176000000000: return("FDVRM");
  case 0177000000000: return("FDVRB");
    // 200 FWT
  case 0200000000000: return("MOVE");
  case 0201000000000: return("MOVEI");
  case 0202000000000: return("MOVEM");
  case 0203000000000: return("MOVES");
  case 0204000000000: return("MOVS");
  case 0205000000000: return("MOVSI");
  case 0206000000000: return("MOVSM");
  case 0207000000000: return("MOVSS");
  case 0210000000000: return("MOVN");
  case 0211000000000: return("MOVNI");
  case 0212000000000: return("MOVNM");
  case 0213000000000: return("MOVNS");
  case 0214000000000: return("MOVM");
  case 0215000000000: return("MOVMI");
  case 0216000000000: return("MOVMM");
  case 0217000000000: return("MOVMS");
  case 0220000000000: return("IMUL");
  case 0221000000000: return("IMULI");
  case 0222000000000: return("IMULM");
  case 0223000000000: return("IMULB");
  case 0224000000000: return("MUL");
  case 0225000000000: return("MULI");
  case 0226000000000: return("MULM");
  case 0227000000000: return("MULB");
  case 0230000000000: return("IDIV");
  case 0231000000000: return("IDIVI");
  case 0232000000000: return("IDIVM");
  case 0233000000000: return("IDIVB");
  case 0234000000000: return("DIV");
  case 0235000000000: return("DIVI");
  case 0236000000000: return("DIVM");
  case 0237000000000: return("DIVB");
  case 0240000000000: return("ASH");
  case 0241000000000: return("ROT");
  case 0242000000000: return("LSH");
  case 0243000000000: return("JFFO");
  case 0244000000000: return("ASHC");
  case 0245000000000: return("ROTC");
  case 0246000000000: return("LSHC");
  case 0247000000000: return("KAFIX");
  case 0250000000000: return("EXCH");
  case 0251000000000: return("BLT");
  case 0252000000000: return("AOBJP");
  case 0253000000000: return("AOBJN");
  case 0254000000000: return("JRST");
  case 0254100000000: return("JRSTF");
  case 0254200000000: return("HALT");
  case 0254500000000: return("JEN");
  case 0255000000000: return("JFCL");
  case 0255740000000: return("JFCL17");
  case 0255040000000: return("JFOV");
  case 0255100000000: return("JCRY1");
  case 0255200000000: return("JCRY0");
  case 0255300000000: return("JCRY");
  case 0255400000000: return("JOV");
  case 0256000000000: return("XCT");
  case 0257000000000: return("CONS");
  case 0260000000000: return("PUSHJ");
  case 0261000000000: return("PUSH");
  case 0262000000000: return("POP");
  case 0263000000000: return("POPJ");
  case 0264000000000: return("JSR");
  case 0265000000000: return("JSP");
  case 0266000000000: return("JSA");
  case 0267000000000: return("JRA");
  case 0270000000000: return("ADD");
  case 0271000000000: return("ADDI");
  case 0272000000000: return("ADDM");
  case 0273000000000: return("ADDB");
  case 0274000000000: return("SUB");
  case 0275000000000: return("SUBI");
  case 0276000000000: return("SUBM");
  case 0277000000000: return("SUBB");
    // 300 CAI
  case 0300000000000: return("CAI");
  case 0301000000000: return("CAIL");
  case 0302000000000: return("CAIE");
  case 0303000000000: return("CAILE");
  case 0304000000000: return("CAIA");
  case 0305000000000: return("CAIGE");
  case 0306000000000: return("CAIN");
  case 0307000000000: return("CAIG");
  case 0310000000000: return("CAM");
  case 0311000000000: return("CAML");
  case 0312000000000: return("CAME");
  case 0313000000000: return("CAMLE");
  case 0314000000000: return("CAMA");
  case 0315000000000: return("CAMGE");
  case 0316000000000: return("CAMN");
  case 0317000000000: return("CAMG");
  case 0320000000000: return("JUMP");
  case 0321000000000: return("JUMPL");
  case 0322000000000: return("JUMPE");
  case 0323000000000: return("JUMPLE");
  case 0324000000000: return("JUMPA");
  case 0325000000000: return("JUMPGE");
  case 0326000000000: return("JUMPN");
  case 0327000000000: return("JUMPG");
  case 0330000000000: return("SKIP");
  case 0331000000000: return("SKIPL");
  case 0332000000000: return("SKIPE");
  case 0333000000000: return("SKIPLE");
  case 0334000000000: return("SKIPA");
  case 0335000000000: return("SKIPGE");
  case 0336000000000: return("SKIPN");
  case 0337000000000: return("SKIPG");
  case 0340000000000: return("AOJ");
  case 0341000000000: return("AOJL");
  case 0342000000000: return("AOJE");
  case 0343000000000: return("AOJLE");
  case 0344000000000: return("AOJA");
  case 0345000000000: return("AOJGE");
  case 0346000000000: return("AOJN");
  case 0347000000000: return("AOJG");
  case 0350000000000: return("AOS");
  case 0351000000000: return("AOSL");
  case 0352000000000: return("AOSE");
  case 0353000000000: return("AOSLE");
  case 0354000000000: return("AOSA");
  case 0355000000000: return("AOSGE");
  case 0356000000000: return("AOSN");
  case 0357000000000: return("AOSG");
  case 0360000000000: return("SOJ");
  case 0361000000000: return("SOJL");
  case 0362000000000: return("SOJE");
  case 0363000000000: return("SOJLE");
  case 0364000000000: return("SOJA");
  case 0365000000000: return("SOJGE");
  case 0366000000000: return("SOJN");
  case 0367000000000: return("SOJG");
  case 0370000000000: return("SOS");
  case 0371000000000: return("SOSL");
  case 0372000000000: return("SOSE");
  case 0373000000000: return("SOSLE");
  case 0374000000000: return("SOSA");
  case 0375000000000: return("SOSGE");
  case 0376000000000: return("SOSN");
  case 0377000000000: return("SOSG");
    // 400 BOOL
  case 0400000000000: return("SETZ");
  case 0401000000000: return("SETZI");
  case 0402000000000: return("SETZM");
  case 0403000000000: return("SETZB");
  case 0404000000000: return("AND");
  case 0405000000000: return("ANDI");
  case 0406000000000: return("ANDM");
  case 0407000000000: return("ANDB");
  case 0410000000000: return("ANDCA");
  case 0411000000000: return("ANDCAI");
  case 0412000000000: return("ANDCAM");
  case 0413000000000: return("ANDCAB");
  case 0414000000000: return("SETM");
  case 0415000000000: return("SETMI");
  case 0416000000000: return("SETMM");
  case 0417000000000: return("SETMB");
  case 0420000000000: return("ANDCM");
  case 0421000000000: return("ANDCMI");
  case 0422000000000: return("ANDCMM");
  case 0423000000000: return("ANDCMB");
  case 0424000000000: return("SETA");
  case 0425000000000: return("SETAI");
  case 0426000000000: return("SETAM");
  case 0427000000000: return("SETAB");
  case 0430000000000: return("XOR");
  case 0431000000000: return("XORI");
  case 0432000000000: return("XORM");
  case 0433000000000: return("XORB");
  case 0434000000000: return("IOR");
  case 0435000000000: return("IORI");
  case 0436000000000: return("IORM");
  case 0437000000000: return("IORB");
  case 0440000000000: return("ANDCB");
  case 0441000000000: return("ANDCBI");
  case 0442000000000: return("ANDCBM");
  case 0443000000000: return("ANDCBB");
  case 0444000000000: return("EQV");
  case 0445000000000: return("EQVI");
  case 0446000000000: return("EQVM");
  case 0447000000000: return("EQVB");
  case 0450000000000: return("SETCA");
  case 0451000000000: return("SETCAI");
  case 0452000000000: return("SETCAM");
  case 0453000000000: return("SETCAB");
  case 0454000000000: return("ORCA");
  case 0455000000000: return("ORCAI");
  case 0456000000000: return("ORCAM");
  case 0457000000000: return("ORCAB");
  case 0460000000000: return("SETCM");
  case 0461000000000: return("SETCMI");
  case 0462000000000: return("SETCMM");
  case 0463000000000: return("SETCMB");
  case 0464000000000: return("ORCM");
  case 0465000000000: return("ORCMI");
  case 0466000000000: return("ORCMM");
  case 0467000000000: return("ORCMB");
  case 0470000000000: return("ORCB");
  case 0471000000000: return("ORCBI");
  case 0472000000000: return("ORCBM");
  case 0473000000000: return("ORCBB");
  case 0474000000000: return("SETO");
  case 0475000000000: return("SETOI");
  case 0476000000000: return("SETOM");
  case 0477000000000: return("SETOB");
    // 500 HWT
  case 0500000000000: return("HLL");
  case 0501000000000: return("HLLI");
  case 0502000000000: return("HLLM");
  case 0503000000000: return("HLLS");
  case 0504000000000: return("HRL");
  case 0505000000000: return("HRLI");
  case 0506000000000: return("HRLM");
  case 0507000000000: return("HRLS");
  case 0510000000000: return("HLLZ");
  case 0511000000000: return("HLLZI");
  case 0512000000000: return("HLLZM");
  case 0513000000000: return("HLLZS");
  case 0514000000000: return("HRLZ");
  case 0515000000000: return("HRLZI");
  case 0516000000000: return("HRLZM");
  case 0517000000000: return("HRLZS");
  case 0520000000000: return("HLLO");
  case 0521000000000: return("HLLOI");
  case 0522000000000: return("HLLOM");
  case 0523000000000: return("HLLOS");
  case 0524000000000: return("HRLO");
  case 0525000000000: return("HRLOI");
  case 0526000000000: return("HRLOM");
  case 0527000000000: return("HRLOS");
  case 0530000000000: return("HLLE");
  case 0531000000000: return("HLLEI");
  case 0532000000000: return("HLLEM");
  case 0533000000000: return("HLLES");
  case 0534000000000: return("HRLE");
  case 0535000000000: return("HRLEI");
  case 0536000000000: return("HRLEM");
  case 0537000000000: return("HRLES");
  case 0540000000000: return("HRR");
  case 0541000000000: return("HRRI");
  case 0542000000000: return("HRRM");
  case 0543000000000: return("HRRS");
  case 0544000000000: return("HLR");
  case 0545000000000: return("HLRI");
  case 0546000000000: return("HLRM");
  case 0547000000000: return("HLRS");
  case 0550000000000: return("HRRZ");
  case 0551000000000: return("HRRZI");
  case 0552000000000: return("HRRZM");
  case 0553000000000: return("HRRZS");
  case 0554000000000: return("HLRZ");
  case 0555000000000: return("HLRZI");
  case 0556000000000: return("HLRZM");
  case 0557000000000: return("HLRZS");
  case 0560000000000: return("HRRO");
  case 0561000000000: return("HRROI");
  case 0562000000000: return("HRROM");
  case 0563000000000: return("HRROS");
  case 0564000000000: return("HLRO");
  case 0565000000000: return("HLROI");
  case 0566000000000: return("HLROM");
  case 0567000000000: return("HLROS");
  case 0570000000000: return("HRRE");
  case 0571000000000: return("HRREI");
  case 0572000000000: return("HRREM");
  case 0573000000000: return("HRRES");
  case 0574000000000: return("HLRE");
  case 0575000000000: return("HLREI");
  case 0576000000000: return("HLREM");
  case 0577000000000: return("HLRES");
    // 600 TAM
  case 0600000000000: return("TRN");
  case 0601000000000: return("TLN");
  case 0602000000000: return("TRNE");
  case 0603000000000: return("TLNE");
  case 0604000000000: return("TRNA");
  case 0605000000000: return("TLNA");
  case 0606000000000: return("TRNN");
  case 0607000000000: return("TLNN");
  case 0610000000000: return("TDN");
  case 0611000000000: return("TSN");
  case 0612000000000: return("TDNE");
  case 0613000000000: return("TSNE");
  case 0614000000000: return("TDNA");
  case 0615000000000: return("TSNA");
  case 0616000000000: return("TDNN");
  case 0617000000000: return("TSNN");
  case 0620000000000: return("TRZ");
  case 0621000000000: return("TLZ");
  case 0622000000000: return("TRZE");
  case 0623000000000: return("TLZE");
  case 0624000000000: return("TRZA");
  case 0625000000000: return("TLZA");
  case 0626000000000: return("TRZN");
  case 0627000000000: return("TLZN");
  case 0630000000000: return("TDZ");
  case 0631000000000: return("TSZ");
  case 0632000000000: return("TDZE");
  case 0633000000000: return("TSZE");
  case 0634000000000: return("TDZA");
  case 0635000000000: return("TSZA");
  case 0636000000000: return("TDZN");
  case 0637000000000: return("TSZN");
  case 0640000000000: return("TRC");
  case 0641000000000: return("TLC");
  case 0642000000000: return("TRCE");
  case 0643000000000: return("TLCE");
  case 0644000000000: return("TRCA");
  case 0645000000000: return("TLCA");
  case 0646000000000: return("TRCN");
  case 0647000000000: return("TLCN");
  case 0650000000000: return("TDC");
  case 0651000000000: return("TSC");
  case 0652000000000: return("TDCE");
  case 0653000000000: return("TSCE");
  case 0654000000000: return("TDCA");
  case 0655000000000: return("TSCA");
  case 0656000000000: return("TDCN");
  case 0657000000000: return("TSCN");
  case 0660000000000: return("TRO");
  case 0661000000000: return("TLO");
  case 0662000000000: return("TROE");
  case 0663000000000: return("TLOE");
  case 0664000000000: return("TROA");
  case 0665000000000: return("TLOA");
  case 0666000000000: return("TRON");
  case 0667000000000: return("TLON");
  case 0670000000000: return("TDO");
  case 0671000000000: return("TSO");
  case 0672000000000: return("TDOE");
  case 0673000000000: return("TSOE");
  case 0674000000000: return("TDOA");
  case 0675000000000: return("TSOA");
  case 0676000000000: return("TDON");
  case 0677000000000: return("TSON");
    // 700 IOT
  case 0700000000000: return("BLKI");
  case 0700040000000: return("DATAI");
  case 0700100000000: return("BLKO");
  case 0700140000000: return("DATAO");
  case 0700200000000: return("CONO");
  case 0700240000000: return("CONI");
  case 0700300000000: return("CONSZ");
  case 0700340000000: return("CONSO");
    // IOTs 701..723  19.                       // 520.
  case 0701000000000: return("DPYCLR");
    //  case 0702000000000: return("PPIOT");
  case 0703000000000: return("DPYOUT");
  case 0704000000000: return("UINBF");
  case 0705000000000: return("UOUTBF");
  case 0706000000000: return("FBREAD");
  case 0707000000000: return("FBWRT");
    //  case 0710000000000: return("MAIL");
    //  case 0711000000000: return("PTYUUO");
  case 0712000000000: return("POINTS");
  case 0713000000000: return("UPGMVE");
  case 0714000000000: return("UPGMVM");
    //  case 0715000000000: return("PGIOT");
  case 0716000000000: return("CHNSTS");
  case 0717000000000: return("CLKINT");
  case 0720000000000: return("INTMSK");
  case 0721000000000: return("IMSKST");
  case 0722000000000: return("IMSKCL");
    //  case 0723000000000: return("INTUUO");
    // CALLI 000..044 36.                       // 539.
  case 0047000000000: return("RESET");
  case 0047000000001: return("DDTIN");
  case 0047000000002: return("SETDDT");
  case 0047000000003: return("DDTOUT");
  case 0047000000004: return("DEVCHR");
  case 0047000000005: return("DDTGT");
  case 0047000000006: return("GETCHR");
  case 0047000000007: return("DDTRL");
  case 0047000000010: return("WAIT");
  case 0047000000011: return("CORE");
  case 0047000000012: return("EXIT");
  case 0047000000013: return("UTPCLR");
  case 0047000000014: return("DATE");
  case 0047000000015: return("LOGIN");
  case 0047000000016: return("APRENB");
  case 0047000000017: return("LOGOUT");
  case 0047000000020: return("SWITCH");
  case 0047000000021: return("REASSI");
  case 0047000000022: return("TIMER");
  case 0047000000023: return("MSTIME");
  case 0047000000024: return("GETPPN");
  case 0047000000025: return("TRPSET");
  case 0047000000026: return("TRPJEN");
  case 0047000000027: return("RUNTIM");
  case 0047000000030: return("PJOB");
  case 0047000000031: return("SLEEP");
  case 0047000000032: return("SETPOV");
  case 0047000000033: return("PEEK");
  case 0047000000034: return("GETLN");
  case 0047000000035: return("RUN");
  case 0047000000036: return("SETUWP");
  case 0047000000037: return("REMAP");
  case 0047000000040: return("GETSEG");
  case 0047000000041: return("GETTAB");
  case 0047000000042: return("SPY");
  case 0047000000043: return("SETNAM");
  case 0047000000044: return("TMPCOR");
    // CALLI 400 000..111  73.                  // 575.
  case 0047000400000: return("SPWBUT");
  case 0047000400001: return("CTLV");
  case 0047000400002: return("SETNAM");
  case 0047000400003: return("SPCWGO");
  case 0047000400004: return("SWAP");
  case 0047000400005: return("EIOTM");
  case 0047000400006: return("LIOTM");
  case 0047000400007: return("PNAME");
  case 0047000400010: return("UFBGET");
  case 0047000400011: return("UFBGIV");
  case 0047000400012: return("UFBCLR");
  case 0047000400013: return("JBTSTS");
  case 0047000400014: return("TTYIOS");
  case 0047000400015: return("CORE2");
  case 0047000400016: return("ATTSEG");
  case 0047000400017: return("DETSEG");
  case 0047000400020: return("SETPRO");
  case 0047000400021: return("SEGNUM");
  case 0047000400022: return("SEGSIZ");
  case 0047000400023: return("LINKUP");
  case 0047000400024: return("DISMIS");
  case 0047000400025: return("INTENB");
  case 0047000400026: return("INTORM");
  case 0047000400027: return("INTACM");
  case 0047000400030: return("INTENS");
  case 0047000400031: return("INTIIP");
  case 0047000400032: return("INTIRQ");
  case 0047000400033: return("INTGEN");
  case 0047000400034: return("UWAIT");
  case 0047000400035: return("DEBREA");
  case 0047000400036: return("SETNM2");
  case 0047000400037: return("SEGNAM");
  case 0047000400040: return("IWAIT");
  case 0047000400041: return("USKIP");
  case 0047000400042: return("BUFLEN");
  case 0047000400043: return("NAMEIN");
  case 0047000400044: return("SLEVEL");
  case 0047000400045: return("IENBW");
  case 0047000400046: return("RUNMSK");
  case 0047000400047: return("TTYMES");
  case 0047000400050: return("JOBRD");
  case 0047000400051: return("DEVUSE");
  case 0047000400052: return("SETPR2");
  case 0047000400053: return("GETPR2");
  case 0047000400054: return("RLEVEL");
  case 0047000400055: return("UFBPHY");
  case 0047000400056: return("UFBSKP");
  case 0047000400057: return("FBWAIT");
  case 0047000400060: return("UFBERR");
  case 0047000400061: return("WAKEME");
  case 0047000400062: return("GETNAM");
  case 0047000400063: return("SNEAKW");
  case 0047000400064: return("SNEAKS");
  case 0047000400065: return("GDPTIM");
  case 0047000400066: return("SETPRV");
  case 0047000400067: return("DDCHAN");
  case 0047000400070: return("VDSMAP");
  case 0047000400071: return("DSKPPN");
  case 0047000400072: return("DSKTIM");
  case 0047000400073: return("SETCRD");
  case 0047000400074: return("CALLIT");
  case 0047000400075: return("XGPUUO");
  case 0047000400076: return("LOCK");
  case 0047000400077: return("UNLOCK");
  case 0047000400100: return("DAYCNT");
  case 0047000400101: return("ACCTIM");
  case 0047000400102: return("UNPURE");
  case 0047000400103: return("XPARMS");
  case 0047000400104: return("DEVNUM");
  case 0047000400105: return("ACTCHR");
  case 0047000400106: return("UUOSIM");
  case 0047000400107: return("PPSPY");
  case 0047000400110: return("ADSMAP");
  case 0047000400111: return("BEEP");
    // INTUUO         6.
  case 0723000000000: return("INTJEN");
  case 0723040000000: return("IMSTW");
  case 0723100000000: return("IWKMSK");
  case 0723140000000: return("INTDMP");
  case 0723200000000: return("INTIPI");
  case 0723240000000: return("IMSKCR");
    // MAIL           6.
  case 0710000000000: return("SEND");
  case 0710040000000: return("WRCV");
  case 0710100000000: return("SRCV");
  case 0710140000000: return("SKPME");
  case 0710200000000: return("SKPHIM");
  case 0710240000000: return("SKPSEN");
    // PGIOT          5.
  case 0715000000000: return("PGSEL");
  case 0715040000000: return("PGACT");
  case 0715100000000: return("PGCLR");
  case 0715140000000: return("DDUPG");
  case 0715200000000: return("PGINFO");
    // PPIOT          8.
  case 0702000000000: return("PPSEL");
  case 0702040000000: return("PPACT");
  case 0702100000000: return("DPYPOS");
  case 0702140000000: return("DPYSIZ");
  case 0702200000000: return("PPREL");
  case 0702240000000: return("PPINFO");
  case 0702300000000: return("LEYPOS");
  case 0702340000000: return("PPHLD");
    // PTY            15.
  case 0711000000000: return("PTYGET");
  case 0711040000000: return("PTYREL");
  case 0711100000000: return("PTIFRE");
  case 0711140000000: return("PTOCNT");
  case 0711200000000: return("PTRD1S");
  case 0711240000000: return("PTRD1W");
  case 0711300000000: return("PTWR1S");
  case 0711340000000: return("PTWR1W");
  case 0711400000000: return("PTRDS");
  case 0711440000000: return("PTWRS7");
  case 0711500000000: return("PTWRS9");
  case 0711540000000: return("PTGETL");
  case 0711600000000: return("PTSETL");
  case 0711640000000: return("PTLOAD");
  case 0711700000000: return("PTJOBX");
    // TTY            16.
  case 0051000000000: return("INCHRW");
  case 0051040000000: return("OUTCHR");
  case 0051100000000: return("INCHRS");
  case 0051140000000: return("OUTSTR");
  case 0051200000000: return("INCHWL");
  case 0051240000000: return("INCHSL");
  case 0051300000000: return("GETLIN");
  case 0051340000000: return("SETLIN");
  case 0051400000000: return("RESCAN");
  case 0051440000000: return("CLRBFI");
  case 0051500000000: return("CLRBFO");
  case 0051540000000: return("INSKIP");
  case 0051600000000: return("INWAIT");
  case 0051640000000: return("SETACT");
  case 0051700000000: return("TTREAD");
  case 0051740000000: return("OUTFIV");
  default: return("???");
  }}

/*
  content of file at URL
  https://www.saildart.org/TABLE.PUB[DOC,ME]
  https://www.saildart.org/TABLE.PUB[CSP,DOC]
  https://www.saildart.org/sn/077089

  * * *

  COMMENT ⊗   VALID 00002 PAGES
  C REC  PAGE   DESCRIPTION
  C00001 00001
  C00002 00002	.PORTION NUMBERTABLE
  C00011 ENDMK
  C⊗;
  .PORTION NUMBERTABLE
  .PAGE FRAME 100 HIGH WCHARS WIDE
  .APP UUOs by Number,,UUOs by number
  .AREA UUOS_BY_NUMBER LINES 10 TO 100 IN 5 COLUMNS 11 WIDE
  .PLACE UUOS_BY_NUMBER
  .BEGIN NOFILL
  %4-----UUOs-----
  Opcode	Name
  040	CALL
  041	INIT
  043	SPCWAR
  047	CALLI
  050	OPEN
  051	TTYUUO
  055	RENAME
  056	IN
  057	OUT
  060	SETSTS
  061	STATO
  062	GETSTS
  063	STATZ
  064	INBUF
  065	OUTBUF
  066	INPUT
  067	OUTPUT
  070	CLOSE
  071	RELEAS
  072	MTAPE
  073	UGETF
  074	USETI
  075	USETO
  076	LOOKUP
  077	ENTER


  701	DPYCLR
  702	PPIOT
  703	DPYOUT
  704	UINBF
  705	UOUTBF
  706	FBREAD
  707	FBWRT
  710	MAIL
  711	PTYUUO
  712	POINTS
  713	UPGMVE
  714	UPGMVM
  715	PGIOT
  716	CHNSTS
  717	CLKINT
  720	INTMSK
  721	IMSKST
  722	IMSKCL
  723	INTUUO
  .SKIP TO COLUMN 2
  ----CALLIs----
  Number	Name
  0	RESET
  1	DDTIN
  2	SETDDT
  3	DDTOUT
  4	DEVCHR
  5	DDTGT
  6	GETCHR
  7	DDTRL
  10	WAIT
  11	CORE
  12	EXIT
  13	UTPCLR
  14	DATE
  15	LOGIN
  16	APRENB
  17	LOGOUT
  20	SWITCH
  21	REASSI
  22	TIMER
  23	MSTIME
  24	GETPPN
  25	TRPSET
  26	TRPJEN
  27	RUNTIM
  30	PJOB
  31	SLEEP
  32	SETPOV
  33	PEEK
  34	GETLN
  35	RUN
  36	SETUWP
  37	REMAP
  40	GETSEG
  41	GETTAB
  42	SPY
  43	SETNAM
  44	TMPCOR

  400000	SPWBUT
  400001	CTLV
  400002	SETNAM
  400003	SPCWGO
  400004	SWAP
  400005	EIOTM
  400006	LIOTM
  400007	PNAME
  .SKIP TO COLUMN 3
  ----CALLIs----
  Number	Name
  400010	UFBGET
  400011	UFBGIV
  400012	UFBCLR
  400013	JBTSTS
  400014	TTYIOS
  400015	CORE2
  400016	ATTSEG
  400017	DETSEG
  400020	SETPRO
  400021	SEGNUM
  400022	SEGSIZ
  400023	LINKUP
  400024	DISMIS
  400025	INTENB
  400026	INTORM
  400027	INTACM
  400030	INTENS
  400031	INTIIP
  400032	INTIRQ
  400033	INTGEN
  400034	UWAIT
  400035	DEBREA
  400036	SETNM2
  400037	SEGNAM
  400040	IWAIT
  400041	USKIP
  400042	BUFLEN
  400043	NAMEIN
  400044	SLEVEL
  400045	IENBW
  400046	RUNMSK
  400047	TTYMES
  400050	JOBRD
  400051	DEVUSE
  400052	SETPR2
  400053	GETPR2
  400054	RLEVEL
  400055	UFBPHY
  400056	UFBSKP
  400057	FBWAIT
  400060	UFBERR
  400061	WAKEME
  400062	GETNAM
  400063	SNEAKW
  400064	SNEAKS
  400065	GDPTIM
  .SKIP TO COLUMN 4
  ----CALLIs----
  Number	Name
  400066	SETPRV
  400067	DDCHAN
  400070	VDSMAP
  400071	DSKPPN
  400072	DSKTIM
  400073	SETCRD
  400074	CALLIT
  400075	XGPUUO
  400076	LOCK
  400077	UNLOCK
  400100	DAYCNT
  400101	ACCTIM
  400102	UNPURE
  400103	XPARMS
  400104	DEVNUM
  400105	ACTCHR
  400106	UUOSIM
  400107	PPSPY
  400110	ADSMAP
  400111	BEEP

  ---MAIL UUOs--
  Number	Name
  0,	SEND
  1,	WRCV
  2,	SRCV
  3,	SKPME
  4,	SKPHIM
  5,	SKPSEN

  ----INTUUOs---
  Number	Name
  0,	INTJEN
  1,	IMSTW
  2,	IWKMSK
  3,	INTDMP
  4,	INTIPI
  5,	IMSKCR

  ----PGIOTs----
  Number	Name
  0,	PGSEL
  1,	PGACT
  2,	PGCLR
  3,	DDUPG
  4,	PGINFO
  .SKIP TO COLUMN 5
  ----TTYUUOs---
  Number	Name
  0,	INCHRW
  1,	OUTCHR
  2,	INCHRS
  3,	OUTSTR
  4,	INCHWL
  5,	INCHSL
  6,	GETLIN
  7,	SETLIN
  10,	RESCAN
  11,	CLRBFI
  12,	CLRBFO
  13,	INSKIP
  14,	INWAIT
  15,	SETACT
  16,	TTREAD
  17,	OUTFIV

  ----PTYUUOs---
  Number	Name
  0,	PTYGET
  1,	PTYREL
  2,	PTIFRE
  3,	PTOCNT
  4,	PTRD1S
  5,	PTRD1W
  6,	PTWR1S
  7,	PTWR1W
  10,	PTRDS
  11,	PTWRS7
  12,	PTWRS9
  13,	PTGETL
  14,	PTSETL
  15,	PTLOAD
  16,	PTJOBX


  ----PPIOTs----
  Number	Name
  0,	PPSEL
  1,	PPACT
  2,	DPYPOS
  3,	DPYSIZ
  4,	PPREL
  5,	PPINFO
  6,	LEYPOS
  7,	PPHLD
  .END
*/
