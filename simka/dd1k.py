#!/usr/bin/python3
# Filename: simka/dd_tk.py      -*- mode:Python; coding:utf-8 -*-

import tkinter as tk
from tkinter import font
from tkinter import *
import socket
import threading
import yaml
import pdb

# globals
root = None
canvas = None
tk = None
foo = None # analog TV signals 1 to 7.
bar = None # crossbar switch
pixel = None

# P1 phosphor was a saturated green with persistance ( BlueP1 is a Farbrication ).
Green="#80FF80"
GreenP1="#AFFF80"
Blue="#0090FF"
BlueP1="#80AFFF"
Black="#202020"
offBlack="#444"
Brown="#402020"
servsock=None
usersock=None
textitem={}

'''
Generate Data Disc instructions
using macro-like python functions.
'''
def TEXTWORD(w): # pack 0 to 5 characters of text into PDP10 word
    word=0
    for c in w[0:5]: # make sure argument width is five or fewer.
        word = (word<<7) | sailcode[c]  # sailcode is like ord(c) but decodes SAIL-WAITS characters from unicode into SAIL 7-bit
    return (word<<1)|1 # return DDD text command PDP10 word
def DPYSTR(s):
    s += "\0"*(5-len(s)%5) # pad right with nulls (not strictly necessary)
    words = [ s[p:p+5] for p in range(0,len(s),5) ]
    return [ TEXTWORD(w) for w in words ]
sailcode={
    "↓":1,"α":2,"β":3,"∧":4,"¬":5,"ε":6,"π":7,
    "λ":0o10,"\t":0o11,"\n":0o12,"\013":0o13,"\f":0o14,"\r":0o15,"∞":0o16,"∂":0o17,
    "⊂":0o20,"⊃":0o21,"∩":0o22,"∪":0o23,"∀":0o24,"∃":0o25,"⊗":0o26,"↔":0o27,
    "_":0o30,"→":0o31,"~":0o32,"≠":0o33,"≤":0o34,"≥":0o35,"≡":0o36,"∨":0o37,    
    "↑":0o136,"←":0o137,    
    "{":0o173,"|":0o174,"§":0o175,"}":0o176,"\b":0o177,
}
sailchar={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",
    0o10:"λ",0o11:"\t",0o12:"\n",0o13:"\013",0o14:"\f",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",
}
# Load execution does the table completion
sailcode[chr(0)]=0
for x in range(0o40,0o136):
    sailcode[chr(x)]=x
    sailchar[x]=chr(x)
for x in range(0o140,0o173):
    sailcode[chr(x)]=x
    sailchar[x]=chr(x)
# omit NUL and TAB.
# VT is  integral \u222B
# FF is plusminus \u00B1
# RUBOUT is tilde
sail_decode_for_DDD={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",
    0o10:"λ",0o11:"",0o12:"\n",0o13:"∫",0o14:"±",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",0o177:"~"
}
"""
# The data disc terminal available banner "Take Me I'm Yours!" has some lowercase letters.
# Display color blue until replaced by client monitor updates in P31 green.
        moveto( x=2, y=348 )
        ␣␣␣␣␣␣␣␣␣␣␣␣␣DD␣JBS,TCOR␣␣R,RCOR␣UCOR␣␣NL␣DSKQ\r\n
        ␣␣␣␣␣␣␣␣␣␣␣␣␣␣0␣␣␣0,0␣␣␣␣␣0,0␣␣␣178K␣␣␣␣%␣␣0D␣␣JUL␣26␣␣FRIDAY␣␣15:07\r\n
        ␣\r\n"
        ␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣Take␣Me␣I'm␣Yours!\r\n
"""
DDD_example=(
    0,
    0o036044461214,
    0o004064103454,
    0o201004020101,
    0o201004020101,
    0o201004042211,
    0o202250251531,
    0o522071751101,
    0o202445451207,
    0o476444052607,
    0o476444020235,
    0o461010451627,
    0o504321200001,
    0o201004020101,
    0o201004020101,
    0o201004020141,
    0o201004030131,
    0o301004020101,
    0o201405430101,
    0o201006133561,
    0o455004020101,
    0o225004030211,
    0o201011252631,
    0o201446620101,
    0o432451142203,
    0o544000000001,
    0o201006134165,
    0o305521505001,
    0o004010023334,
    0o200321200001,
    0o000000000001,
    0o201004020101,
    0o201004020101,
    0o201004020101,
    0o201004020101,
    0o201004020101,
    0o201004020101,
    0o201004052303,
    0o657124046713,
    0o202224766501,
    0o547376571347,
    0o204321200001,
    0,
)
DDD_Font = None

def nlz(word):
    "number of leading zeroes in a 36-bit PDP10 word"
    w = word & 0xFFFFFFFFF
    n = (36,'{0:036b}'.format(w).index('1'))[w>0]
    return n

def vprint(*args,**kwargs):
    "verbose debug bamboo print statements"
    # print(*args,**kwargs)
    pass

class display_service( threading.Thread ):
    def __init__(me):
        threading.Thread.__init__( me )

    def run( me ):
        global tick,root,usersock
        global foo,bar
        for canv in root.can:
            canv.delete( canv.tag )
            convert_ddd_into_tk( canv, DDD_example, BlueP1 ) # Take me I'm yours!
            msgstr=""
        while True:
            data = usersock.recv( 125000 )
            if not data:
                usersock.close()
                usersock = None
                break
            msg = data.decode()
            
            # Inspect YAML message received.
            # print("\nReceived YAML msg length {:d}. bytes head='{}'".format(len(msg),msg[:40]))
            # continue # from HERE when changing the message format
        
            # Parse YAML commands sent from simka
            for document in list(filter(None,msg.split("---"))):
                namval = yaml.safe_load(document)
                # print("\nReceive YAML namval= {}".format(namval))
                for k in namval:
                    val = namval[k]
                    if k[:4]=='VDS_':
                        w = int(val,8)
                        kb = int(k[4:],8)
                        tv = kb + 0o20
                        foo[kb] = w & 7
                        oldbar= bar[kb]
                        bar[kb] = w >> 4
                        # The one and only KB#6 on line TV#26 most likely VDS crossbar from DD#0
                        if oldbar!=bar[kb] and kb-6 in range(1):
                            text="Terminal TV#{:o} keyboard#{:o} crossbar hex {:08X} atv# {:2o}".format( tv, kb, bar[kb], foo[kb] )
                            root.lab.configure(text=text)
                            # pdb.set_trace()
                            root.can[kb-6].delete( 'all' )
                            root.can[kb-6].create_image(0,1,anchor=NW,image=root.can[kb-6].pixel,tag=root.can[kb-6].tag)
                            print("VDS tv={:0} kb#{:o} cross bar=0x{:08x} foo={}".format(tv,kb,bar[kb],foo[kb]))
                    elif k=='DD_code':
                        buf = [int(x,8) for x in val.split()] # From octal PDP10 words to Python integer list
                        # print("DD buf={}".format(buf)) # decimal buffer list is not much help
                        for canv in root.can:
                            convert_ddd_into_tk( canv, buf, Green )
        
        print("DDD display service thread finished")
        for canv in root.can:
            canv.delete( canv.tag )
            test_pattern(canv)
            
def Execute_text( canvas, Fill, column, line, high, wide, linebuffer ):
    """
    Data Disc Execute command - display the line buffer text
    for single height text: at column 1 to 85 width 6 bits
    """
    global textitem # cache
    # new text over writing old
    if(column==2 and len(linebuffer)>1):
        key = "T {}.{}".format(3,line)
        try:
            item = textitem[key]
            canvas.delete(item)
            del textitem[key]
            # pdb.set_trace()
        except KeyError:
            pass
   
    (x00,y00) = (10,10)
    x0 = column * (6,12)[wide]
    key = "T {}.{}".format(column,line)
    remark=""
    try:
        item = textitem[key]        
        if canvas.itemcget(item,'text')==linebuffer :
            remark = "repeat"
            pass # ignore exact repetitions
        else:
            canvas.itemconfigure( item, text=linebuffer )
            remark = f"at {key} old item {item} update text '{linebuffer}'"
    except KeyError:
        textitem[key] = canvas.create_text(
            (x00+  x0)*2,
            (y00+line)*2,
            text=linebuffer,
            font=DDD_Font[(1,2)[high]],
            fill=Fill,
            anchor="nw",
            tags = canvas.tag )
        remark = f"at {key} NEW text item '{linebuffer}'"
    if remark: print(" "*13 + remark )
                         
bitrow={}    
def Execute_graphics( canvas, Fill, column, line, bits ):
    """
    Data Disc Execute command - the line buffer data is written to bit map image,
    as bit raster graphics:   at column 1 to 64 width 8 bits
    """
    global bitrow # cache
    (x00,y00) = (10,10)
    z0= 0
    z9= len(bits)
    # pdb.set_trace()
    gtag="G {}.{}".format(column,line)
    try:
        row = bitrow[gtag]
        if row==bits: # same as before
            return
    except KeyError:
        pass
    bitrow[gtag] = bits # update cache
    rowstr = "{ "+bits.replace('0',' #444 #444').replace('1', ' #FFF #FFF')+" }" # offblack and white pixel values
    canvas.pixel.put(rowstr,to=(2*(8*column+x00),2*(line+y00)  ))
    canvas.pixel.put(rowstr,to=(2*(8*column+x00),2*(line+y00)+1))

def convert_ddd_into_tk( canvas, DDbuf, Fill ):
    global textitem
    (     x00,  y00  ) = ( 10,10 ) # offset DD raster within canvas
    (  column,  line ) = (  1, 0 ) # DD beam starting coordinates
    ( holine, loline ) = (  0, 0 )
    ( high, wide, gmode ) = (False, False, False)
    (dd,kb,ln,tag) = (canvas.dd, canvas.kb, canvas.ln, canvas.tag)
    i = 1
    n = len( DDbuf )
    vprint(f"\nnew DDbuf length {n}")
    while i < n:
        word = DDbuf[i]
        # vprint("_%4d / %013o  " % (i, word),)
        
        if word & 1: # Text word
            msg=""
            while word & 1:
                b1 = word >> 29 & 0o177
                b2 = word >> 22 & 0o177
                b3 = word >> 15 & 0o177
                b4 = word >>  8 & 0o177
                b5 = word >>  1 & 0o177
                msg += "".join(sail_decode_for_DDD.get(x,chr(x)) for x in (b1,b2,b3,b4,b5))
                i += 1
                if (i >= n) or (DDbuf[i] & 1 != 1) :
                    i -= 1
                    break
                word = DDbuf[i]
            vprint("{}           TEXT( column={:3d} line={:3d} '{}' )".
                   format(tag, column, line, msg.
                          replace(' ','␣').
                          replace('\n','↴').
                          replace('\r','↵') ))
            linebuffer=""
            for snip in list(filter(None,re.split(r'(\r|\n)',msg))):
                if snip=='\r':
                    if linebuffer: Execute_text( canvas, Fill, column, (line & -2), high, wide, linebuffer )
                    linebuffer=""
                    column = 2
                elif snip=='\n':
                    if linebuffer: Execute_text( canvas, Fill, column, (line & -2), high, wide, linebuffer )
                    linebuffer=""
                    line += (12,24)[ high ]
                elif len(snip):
                    linebuffer = snip
            if len(linebuffer):
                # print("EOL text linebuffer='{}'".format(linebuffer))
                Execute_text( canvas, Fill, column, (line & -2), high, wide, linebuffer )
                column += len(linebuffer)
                        
        elif (word & 0o17)==2 : # Graphics word
            bits=""
            while (i<n) and (word & 0o17)==2:
                bits += bin(1<<32 | word>>4)[3:]                 # 32 bits
                i += 1
                if (i >= n) or (DDbuf[i] & 0o17 != 2) :
                    i -= 1
                    break
                word = DDbuf[i]
                
        elif (word & 7)==4: # Triple command word, unpack three operations
            (op1,op2,op3,ops) = ((word>>9)&7,     (word>>6)&7,     (word>>3)&7,      (word>>3)&0o777)
            (d1,d2,d3) =        ((word>>28)&0xFF, (word>>20)&0xFF, (word>>12)&0xFF,)            

            if ops in [ 0o121, 0o123 ] :
                if bar[kb] & (1<<(31-d2)) == 0 : # VDS crossbar omits this DD# channel
                    return                    
                # vprint("{} {} DD#{:o} Canvas ln#{:o} dd#{:o} kb#{:o} crossbar hex_{:08x} foo_{}".format(
                #    tag,("select","erase")[d1==0o17],d2, ln, dd, kb, bar[kb], foo[kb]))
            
            if op1==1 :
                gmode= bool(d1 & 0o01)
                high= (d1 & 0o40 == 0)
                wide= bool(d1 & 0o10)
                if d1==0o17 : # erase
                    vprint("\nERASE screen\n")
                    canvas.delete( 'all' )
                    canvas.pixel = PhotoImage(data="R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7")
                    canvas.create_image(0,1,anchor=NW,image=canvas.pixel,tag=canvas.tag)
                    textitem={} # cache
                    bitrow={} # cache
            elif op1==0:
                if gmode:
                    Execute_graphics( canvas, Fill, column, line, bits )
                    
            if op2==1 :
                gmode= bool(d2 & 0o01)
                high= (d2 & 0o40 == 0)
                wide= bool(d2 & 0o10)
                
            if op3==1 :
                gmode= bool(d3 & 0o01)
                high= (d3 & 0o40 == 0)
                wide= bool(d3 & 0o10)
            elif op3==3:
                column= d3
                # vprint("{} set column={}".format(tag,column))

            remark=""
            if ops in ( 0o121, 0o123, 0o033, 0o333 ):
                pass
            elif ops == 0o345 :
                column = d1
                holine = d2
                loline = d3
                line = (holine<<4) | loline                
            elif ops == 0o444 :
                holine= d3
                line = (holine<<4) | loline
                # vprint("{} set holine={}".format(tag,holine))
            elif ops == 0o503 :
                loline= d1
                line = (holine<<4) | loline
                # vprint("{} set           loline={} line={}".format(tag,       loline,line))                
                Execute_graphics( canvas, Fill, column, line, bits )                
                column= d3
                line = (holine<<4) | loline
                # vprint("{} set holine={} loline={} line={}".format(tag,holine,loline,line))
            else:
                remark = "Could_be_incomplete_? "+"="*33

            vprint("{} commands: {:o}/{:03o} {:o}/{:03o} {:o}/{:03o} Results {} column={:d} line={:d} {}".format(
                tag,op1,d1, op2,d2, op3,d3, "gG"[gmode]+"hH"[high]+"wW"[wide], column, line, remark ))

        i+=1    
    
class DDD_App:
    def __init__(self):
        pass

    # 160x48
    sail_four_month = """
..........................................................................*........................*...........................................................
.................................................*........................*........................*........................*..................................
.................................................*........................*........................*........................**.................................
........................................********.*.******........********.*.******........********.*.******........********.*.******...........................
.......................................*..******.*.*******.....**..******.*.*******.....**..******.*.*******.....**..******.*.*******..........................
......................................*..******..*..*****....**...******..*..******...**...******..*..******...**...******..**.*****...........................
.....................................*...........*.........**.............*.........**.............*.........**.............*.*....*...........................
................................*...*...********.*.********......********.*.********......********.*.********......********.*.*******..........................
...............................***.*.....*******.*.********.......*******.*.********.......*******.*.********.......*******.*.********.........................
.............................*..*.*.....*******..*..******.......*******..*..******.......*******..*..******.......*******..*..******.*........................
............................*...**.*.............*........................*........................*........................**....*....*.......................
..........................*.....*...*...********.*.*******.......********.*.*******.......********.*.*******.......********.*.*******...*......................
.........................*......*....*...*******.*.********.......*******.*.********.......*******.*.********.......*******.*.********...*.....................
.......................*........*.....*.******...*...*****.......******...*...*****.......******...*...*****.......******...*...******....*....................
......................*.........**....**.........*........................*........................*........................*....*....*....*...................
....................*...........*.*....***.*****.*.********.....*********.*.********.....*********.*.********.....*********.*.********.*....*..................
...................*............*..*...****.****.*.*********.....********.*.*********.....********.*.*********.....********.*.*********.*....*.................
.................*..............*...*...****.***.*.*********.....********.*.*********.....********.*.*********.....********.*.*********..*....*................
................*...............*....*..*****....*....*****.....******....*....*****.....******....*....*****.....******....*....*****....*....*...............
...............*.**.............*....**.******...*........................*........................*........................*.........*...**....*..............
...............*..***...........*.....***.*****..*.**********..**********.*.**********..**********.*.**********..**********.*.**********...**....*.............
..............*....****.........**....****.*****.*.***********..*********.*.***********..*********.*.***********..*********.*.***********...***...*............
..............*....******.......*.*....****.******..**********..*********.*.***********..*********.*.***********..*********.*.************..****...*...........
.............*......*******.....*..*...*****.....*.....******..******.....*.....******..*******....*.....******..*******....*.....******..*..*****..*..........
.............*.....**********...*...**.*******...*........................*........................*........................*..............*..*****..*.........
............*......************.*....**.*******..*.***********.**********.*.***********.**********.*.***********.**********.*.***********...*..******.*........
...........*......*************.*....***.*******.*.************.*********.*.************.*********.*.************.*********.*.************..**..******.*.......
...........*......*************.*.....***.......**.*************.********.*.*************.********.*.*************.********.*.*************..**..********......
..........*......**************.*....*****.*****.*.**************.*******.*.**************.*******.*.**************.*******.*.**************..**.*********.....
.........*......***************.*...*.*****.****.*.**************.*******.*.**************.*******.*.**************.*******.*.**************..***.*********....
.........*....*****************.*..*.*******.***.*.***************.******.*.***************.******.*.***************.******.*.***************.****.*********...
........*...*******************.*.*..********.**.*.***************.******.*.***************.******.*.***************.******.*.***************.*****.*********..
.......*.**********************.**..**********...*.....***********.**.....*.....***********.**.....*.....***********.**.....*.....***********.******.***.*****.
......*************************.*...***********..*.........******.........*.........******.........*.........******.........*.........******.********.......***
..........*.....................*..*****....****.*.............**.........*.............**.........*.............**.........*.............**.****.****...***...
.........*......................*..**.........****........................*........................*........................*...............***.......***......
........**********..............*.*.............**........................*........................*........................*.............**.......***.........
.......***********..............**...............*........................*........................*........................*...........**......***............
........*************************................*........................*........................*........................*************************..........
.........*****************************************************************************************************************************************.............
...........**************************************....**..**.**.*...*******.***.**..**.***.*.....*.**.**...**************************************...............
.............************************************.****.**.*.**.*.**.******..*..*.**.*..**.***.***.**.*.*****************************************...............
............*************************************...**.**.*.**.*..********.*.*.*.**.*.*.*.***.***....**..**************************************................
...........**************************************.****.**.*.**.*.*.*******.***.*.**.*.**..***.***.**.****.*************************************................
..........***************************************.*****..***..**.**.******.***.**..**.***.***.***.**.*...*************************************.................
..........************************************************************************************************************************************.................
...............................................................................................................................................................
...............................................................................................................................................................
"""
    available = """
             DD JBS,TCOR  R,RCOR UCOR  NL DSKQ
              0   0,0     0,0   178K    %  0D  JUL 26  FRIDAY  15:07
 
                                 Take Me I'm Yours!
"""
def test_pattern(canvas):
    '''
    Data Disc bit raster 480. lines by 512. columns
    standard character font, each glyph is  6 x 12.
    double size font,        each glyph is 12 x 24.
    graphics bits font,      each glyph is  1 x 1.
    
    CR sets x  =  2.
    LF sets y += 12.

    177 TB displays T/B u2409 ␉
    177 LF displays L/F u240a ␊
    177 CR displays C/R u240D ␍

    ai(2,348)
    dpystr(
    "␣␣␣␣␣␣␣␣␣␣␣␣␣DD␣JBS,TCOR␣␣R,RCOR␣UCOR␣␣NL␣DSKQ\r\n"
    "␣␣␣␣␣␣␣␣␣␣␣␣␣␣0␣␣␣0,0␣␣␣␣␣0,0␣␣␣178K␣␣␣␣%␣␣0D␣␣JUL␣26␣␣FRIDAY····␣␣15:07\r\n"
    "␣\r\n"
    "␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣␣Take␣Me␣I'm␣Yours!\r\n"
    )
    # Screen 480 x 512 pixel origin (0,0) is at upper left, North West corner of raster.
    # ------------------------------------------
    # SIZE =0  480 x 512     1       1
    # SIZE =1  40 x  85     12      25
    # SIZE =2  20 x  42     14      28
    # ------------------------------------------
    # characters per line   :  512, 85, 42,
    #      lines per screen :  480, 42, 20,
'''
    global app,pixel
    global DDD_Font
    canvas.pixel = PhotoImage(data="R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7")
    canvas.create_image(0,1,anchor=NW,image=canvas.pixel,tag=canvas.tag)
    DDD_Font = [ # size negative is height in canvas pixels
        font.Font(family='Ubuntu Mono',size=0- 1*2), # for pixel bits as text characters (not adequate)
        font.Font(family='Ubuntu Mono',size=1-12*2), #  85 cpl
        font.Font(family='Ubuntu Mono',size=0-24*2), #  42 cpl
    ]
    # unicode Full Block u2588 ( → → → "█" ← ← ← hard to see unicode glyph ) 
    # I.Canvas screen ruler for chosing font sizes to simulate DDD size table.
    r0="         1         2         3         4         5         6         7         8     "
    r1="␣␣␣␣␣␣␣␣␣1␣␣␣␣␣␣␣␣␣2␣␣␣␣␣␣␣␣␣3␣␣␣␣␣␣␣␣␣4␣␣␣␣␣␣␣␣␣5␣␣␣␣␣␣␣␣␣6␣␣␣␣␣␣␣␣␣7␣␣␣␣␣␣␣␣␣8␣␣␣␣␣"
    r2="1234567890123456789012345678901234567890123456789012345678901234567890123456789012345"
    r3="ABCDEFGHIJKLMNOPQRSTUVWXYZ ≤ π ε ↓ ∩ ¬ ≠ ≡ ∞ ← → ↔ ∃ ∀ ⊗ ∂ ∧ ∪ ≥ ⊂ ↑ λ ∨ β ⊃ α"
    r4="abcdefghijklmnopqrstuvwxyz ~ ! @ # $ % ^ & * ( ) _ + { } | : ? > < , . / ; ' \\ \""
    forty_lines = '\n'.join(["line "+str(i) for i in range(40)])    
    (dd,kb,ln,tag) = (canvas.dd, canvas.kb, canvas.ln, canvas.tag)
    (ox,oy) = (16,16)
    if 0:canvas.create_rectangle( ox-1, oy-1, ox+512*2 +1, oy+480*2 +1, fill=None, outline="orange",width=1,tags=tag)
    if 0:canvas.create_rectangle(  8-1,  8-1,    512*2+31,    480*2+31, fill=None, outline="cyan",  width=2,tags=tag)
    if 1:canvas.create_text(ox+2,oy+16*12*2,text=r0[:85]+"\n"+r2[:85]+"\n"+r3[:85]+"\n"+r4[:85],font=DDD_Font[1],fill="gray",anchor="nw",tags=tag)
    if 1:canvas.create_text(ox+2,oy+20*12*2,text=r0[:42]+"\n"+r2[:42],                          font=DDD_Font[2],fill="gray",anchor="nw",tags=tag)
    for i in list(range(0,16))+list(range(24,40)):
        canvas.create_text(ox+2*2,oy+i*12*2,text="line "+str(i), font=DDD_Font[1],fill="gray",anchor="nw",tags=tag)       
    for i in list(range(0,8))+list(range(12,20)):
        canvas.create_text(ox+100*2,oy+i*24*2,text="line "+str(i), font=DDD_Font[2],fill="gray",anchor="nw",tags=tag)    
    glyph = app.sail_four_month
    y=0
    (x00,y00) = (200,300)
    if 0:canvas.create_rectangle(
             x00*2,         y00*2,
            (x00+160+2)*2, (y00+48+2)*2,
            fill=None,outline="red",width=2,tags=tag)
    for ln in glyph.splitlines():
        y+=1
        if bool(ln):
            rowstr = "{ "+ln.replace('.',' #444 #444').replace('*', ' #888 #888')+" }"
            canvas.pixel.put( rowstr, to=(2*x00,2*(y00+y)  ) )
            canvas.pixel.put( rowstr, to=(2*x00,2*(y00+y)+1) )

def main():
    """
    Stanford Data Disc central Display Processor reënactment for July 1974 system configuration.
    Display a Data Disc test pattern with the four mast sail boat glyph.

    Worry about the font sizes and the intra line spacing.
    Worry about origin (0,0) offset and frame border width.
    Worry about good looking background and frame coloring.

    Experiment with scaling of the 480x512 raster up by a factor of 2.
    Note yet again a unit pixel tk exploit that is used to plot bit-graphics on the 512x480 raster x2.
    """
    global app, DDD_font
    global root,canvas
    global servsock,usersock
    global bar,foo,pixel
    bar = [1<<(31-9)]*64
    foo = [0]*64    
    root = Tk()
    root.geometry('1100x1100+170+670')
    root.tk_focusFollowsMouse()
    root["bg"] = offBlack
    root.title("DD test pattern");
    Button( root, text="Quit",
            font = font.Font(size=30,weight='bold'),
            fg="darkred",
            bg="white",
            activeforeground="white",
            activebackground="darkred",
            command=root.quit ).pack(side="bottom")
    canvas = Canvas( root,
                     width =(2*512+32),
                     height=(2*480+32),
                     takefocus=1,
                     highlightthicknes= 4,
                     highlightcolor="gold",
                     highlightbackground="gray",
                     background=offBlack,
                     name="tv26")    
    canvas.pack( side="top", padx=0, pady=16 )
    canvas.dd = 0
    canvas.kb = 6
    canvas.ln = 0o26
    canvas.tv = 0o26
    canvas.tag = "DD{:o}".format(canvas.dd)    
    app = DDD_App()
    test_pattern(canvas)    
    try:
        root.mainloop()
    finally:
        if servsock is not None:
            servsock.close()
        if usersock is not None:
            usersock.close()

if __name__ == "__main__" :
    main()

