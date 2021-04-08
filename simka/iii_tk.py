#!/usr/bin/python3
# -*- mode:Python; coding:utf-8 -*-
# Filename: simka/iii_tk.py

import tkinter as tk
from tkinter import font
from tkinter import *

import socket
import threading
import yaml
import pdb

# Globals
usersock=None
root = None
tk = None

Green="#80FF80"
Blue="#0090FF"
Black="#404040"
Black="#202020"
offBlack="#404040"
Brown="#402020"

(ox,oy)=(15,15)
glass = 0
paper = None

'''
Simplified Triple-III command word "macro-like" python functions.
Just provide graphics commands 6=vector 16=dot 32=invisible 64=absolute
Omit all the short vector, subroutine, control transfer and bit testing logic commands.
'''
def AV(x,y):    return (x&2047)<<25 | (y&2047)<<14 | 6 | 64     # LINE visible absolute
def AI(x,y):    return AV(x,y) | 32                             # MOVE
def RV(x,y):    return (x&2047)<<25 | (y&2047)<<14 | 6          # line visible
def RI(x,y):    return AV(x,y) | 32                             # move
def BRT(brt):   return (brt&7)<<11 | 32 | 6                     # brightness 1 to 7. 0 no change.
def SIZ(siz):   return (siz&7)<<8  | 32 | 6                     # font size  1 to 7. 0 no change.
def ADOT(x,y):  return AV(x,y) | 16                             # absolute display dot endpoint
def RDOT(x,y):  return RV(x,y) | 16                             # relative display dot endpoint
def TEXTWORD(w):                                                # pack 0 to 5 characters of text into PDP10 word
    word=0
    for c in w[0:5]: # damn sure width is five characters
        word = (word<<7) | sailcode[c]
    return (word<<1)|1 # return III text command PDP10 word
def DPYSTR(s):
    s += "\0"*(5-len(s)%5) # pad right with nulls (not strictly necessary)
    words = [ s[p:p+5] for p in range(0,len(s),5) ]
    return [ TEXTWORD(w) for w in words ]

sailcode={
    # sailcode is like ord(c) it converts SAIL-WAITS characters from unicode into SAIL 7-bit integers
    "↓":1,"α":2,"β":3,"∧":4,"¬":5,"ε":6,"π":7,
    "λ":0o10,"\t":0o11,"\n":0o12,"\013":0o13,"\f":0o14,"\r":0o15,"∞":0o16,"∂":0o17,
    "⊂":0o20,"⊃":0o21,"∩":0o22,"∪":0o23,"∀":0o24,"∃":0o25,"⊗":0o26,"↔":0o27,
    "_":0o30,"→":0o31,"~":0o32,"≠":0o33,"≤":0o34,"≥":0o35,"≡":0o36,"∨":0o37,    
    "↑":0o136,"←":0o137,    
    "{":0o173,"|":0o174,"§":0o175,"}":0o176,"\b":0o177,
}
sailchar={
    # sailcode is like chr(i) it converts SAIL 7-bit integers into unicode SAIL-WAITS characters
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",
    0o10:"λ",0o11:"\t",0o12:"\n",0o13:"\013",0o14:"\f",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",
}
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
sail_decode_for_III={
    0:"",1:"↓",2:"α",3:"β",4:"∧",5:"¬",6:"ε",7:"π",
    0o10:"λ",0o11:"",0o12:"\n",0o13:"∫",0o14:"±",0o15:"\r",0o16:"∞",0o17:"∂",
    0o20:"⊂",0o21:"⊃",0o22:"∩",0o23:"∪",0o24:"∀",0o25:"∃",0o26:"⊗",0o27:"↔",
    0o30:"_",0o31:"→",0o32:"~",0o33:"≠",0o34:"≤",0o35:"≥",0o36:"≡",0o37:"∨",
    0o136:"↑",0o137:"←",0o173:"{",0o174:"|",0o175:"§",0o176:"}",0o177:"~"
}
# The Triple-III terminal available banner "TAKE ME I'M YOURS!" was all uppercase.
III_example=(1,
             0o770000000010, # select all six display consoles
             0o640000012146, # AI(-384,0) BRT(2) SIZ(4)
             0o522031342501,0o466124044517,0o465013147653,0o512464100001,
             # display square
             AI(-511,-511),
             AV( 511,-511),
             AV( 511, 511),
             AV(-511, 511),
             AV(-511,-511))
# 1024x1024 character width and height
I_char_dx = [0,  8, 12,    14,    16, 24,    32,  48   ]
I_char_dy = [0, 16, 25,    28,    32, 48,    64, 102   ]
III_BRT = [ "#000", "#0B0", "#0F0", "#6F6", "#8F8", "#AFA", "#DFD", "#EFE",]
III_Font_S = None
III_Font_M = None
III_Font = None


def vprint(*args,**kwargs):
    # print(*args,**kwargs)
    pass

class Triple_III_display_service( threading.Thread ):
    def __init__(me):
        threading.Thread.__init__( me )
        # paper.delete('1.0',END)
    def run( me ):
        global tick,root,usersock
        global paper
        for canv in root.can:
            canv.delete(f'III#{canv.ln:o}')
            convert_iii_into_tk( canv, III_example, Blue ) # Take me I'm yours!
            msgstr=""
        while True:
            data = usersock.recv( 125000 )
            if not data:
                usersock.close()
                usersock = None
                break
            msg = data.decode()
            
            # Inspect YAML message received.
            n = len(msg)
            if 0:print(f"Received YAML msg length {n:d}.")
            if 0:print(f"\nReceived YAML msg length {n:d}. bytes msg'{msg}'")
            if 0:print("\nReceived YAML msg length {:d}. bytes head='{}' tail='{}'".
                  format(len(msg),msg[:40],msg[-40:]))
            # continue # from HERE when changing the message format
            # Parse YAML commands sent from simka
            
            for document in list(filter(None,msg.split("---"))):
                # print(f"\ndocument={document}")
                namval = yaml.safe_load(document)
                # print(f"\nnamval={namval}")
                for k in namval:
                    val = namval[k]
                    if k[:8]=='III_code':            
                        buf = [int(x,8) for x in val.split()] # octal words
                        for canv in root.can:
                            convert_iii_into_tk( canv, buf, Green )
        
        print("III display service thread finished")
        for canv in root.can:
            test_pattern(canv)

k_scale = (640.0/1024.0)
def k(xy):
    global k_scale
    return int( k_scale * xy + 0.5 )

def convert_iii_into_tk( canvas, III_buffer, Fill ):
    global glass
    n = len( III_buffer )
    brt = 0
    siz = 0
    III_Font[0] = III_Font[2]
    (  ox,  oy ) = (    30,     30 ) # offset the displayed square inside its frame
    ( xxx, yyy ) = (ox+512, oy+512 ) # Middle of the III screen. The "origin" of the (x,y) coordinates.
    (  x0,  y0 ) = (    0,       0 ) # beam starting coordinates
    line_num = canvas.ln
    kb = canvas.kb
    beam = False
    glass = III_buffer[0]
    tag = f"III#{line_num:o}"
    canvas.delete( tag )
    # print(f'canvas delete tag={tag} glass#{glass}')
    # time.sleep(5)
    i = 1
    # pdb.set_trace()
    while i < n:
        word = III_buffer[i]
        vprint("III_%d / %013o  " % (i, word),)
        if word & 1:
            msg=""
            while word & 1:
                b1 = word >> 29 & 0o177
                b2 = word >> 22 & 0o177
                b3 = word >> 15 & 0o177
                b4 = word >>  8 & 0o177
                b5 = word >>  1 & 0o177
                msg += "".join(sail_decode_for_III.get(x,chr(x)) for x in (b1,b2,b3,b4,b5))
                i += 1
                if (i >= n) or (III_buffer[i] & 1 != 1) :
                    i -= 1
                    break
                word = III_buffer[i]
                # vprint( "TXT_%d / %013o  " % (i, word),)
            vprint("DPYSTR(asciz|"+msg+"|)")            
            for snip in re.split(r'(\r|\n)',msg):
                if snip=='\r':
                    x0 = -512
                elif snip=='\n':
                    y0 = y0-I_char_dy[siz]
                elif len(snip):                
                    if(beam):canvas.create_text(k(xxx+x0+7),k(yyy-y0), text=snip, font=III_Font[siz], fill=Fill, anchor="sw", tags=tag)
                    x0 += len(snip) * I_char_dx[siz] 
        elif (word & 0o17) in (4,0o12,0o14,0o16,): # JSR-JMS-SAVE, TEST, RESTORE, NOP
            pass
        elif (word & 0o17) in (8,):
            select_mask = (word>>30)&63 # only the six high order PDP10 bits
            beam = bool(select_mask & (0o40 >> kb)) # bits numbered left to right
            vprint("SELECT beam={} line={} mask={:o}".format(beam, kb, select_mask))
        elif (word & 0o17) in (2,6,): # Vectors short or long
            x = word >> (36-11) & 0o3777
            y = word >> (36-22) & 0o3777
            brt = word >>11 & 7
            siz = word >>8  & 7
            if siz:
                III_Font[0]= III_Font[siz]
                I_char_dx[0]= I_char_dx[siz]
                I_char_dy[0]= I_char_dy[siz]
            if brt>0 or siz>0:
                brtsiz = ",BRT=%o,SIZ=%o" % (brt,siz);
            else:
                brtsiz = ""
                
            op = word & 0o177
                
            if x & 0o2000: x |= ~0o3777 # sign extend
            if y & 0o2000: y |= ~0o3777
            
            if op==0o106: # Absolute Visible
                vprint("LINE(%d,%d%s)" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x,y)         # to position now
                if(beam):canvas.create_line(k(xxx+x1),k(yyy-y1),k(xxx+x0),k(yyy-y0),fill=Fill,tags=tag)
                
            elif op==0o006: # relative Visible
                vprint("line(%d,%d%s)" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x0+x,y0+y)   # to position
                if(beam):canvas.create_line(k(xxx+x1),k(yyy-y1),k(xxx+x0),k(yyy-y0),fill=Fill,tags=tag)
                
            elif op==0o126 or op==0o166: # absolute DOT endpoint
                vprint("LINE(%d,%d%s)dot" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x,y)         # to position now
                if(beam):canvas.create_oval(k(xxx+x0-1),k(yyy-y0-1),k(xxx+x0+1),k(yyy-y0+1),fill=Fill,tags=tag)

            elif op==0o026 or op==0o066: # relative DOT endpoint
                vprint("line(%d,%d%s)dot" %(x,y,brtsiz))
                (x1,y1) = (x0,y0)       # from here
                (x0,y0) = (x0+x,y0+y)   # to position
                if(beam):canvas.create_oval(k( xxx+x0-1),k( yyy-y0-1),k(xxx+x0+1),k(yyy-y0+1),fill=Fill,tags=tag)
                
            elif op==0o146:
                vprint("MOVE(%d,%d%s)" %(x,y,brtsiz))
                (x0,y0) = (x,y) # beam position Absolute Invisible
                
            elif op==0o046:
                vprint("move(%d,%d%s)" %(x,y,brtsiz))
                (x0,y0) = (x0+x,y0+y) # beam position Relative Invisible
            else:
                vprint("III opcode %03o" %(op & 0o177))
        i+=1
    
class III_App:
    def __init__(self):
        pass

def test_pattern(canvas):
    '''    
    Screen 1024 x 1024 pixel origin 0,0 in the center +X goes right +Y goes up.
    SIZE =0 no_change     DX      DY
     ------------------------------------------
    SIZE =1  64 x 128      8      16
    SIZE =2  41 x  85     12      25
    SIZE =3  36 x  73     14      28
    SIZE =4  32 x  64     16      32
    SIZE =5  21 x  42     24      48
    SIZE =6  16 x  32     32      64
    SIZE =7  10 x  21     48     102
    ------------------------------------------
    characters per line   : 128, 85, 73, 64, 42, 32, 21
         lines per screen :  64, 42, 36, 32, 21, 16, 10
    '''
    global III_Font
    global III_Font_S
    global III_Font_M
    global k_scale
    III_Font_M = [
        None,
        font.Font(family='Ubuntu Mono',size=-16),
        font.Font(family='Ubuntu Mono',size=-24),
        font.Font(family='Ubuntu Mono',size=-28),    
        font.Font(family='Ubuntu Mono',size=-32),
        font.Font(family='Ubuntu Mono',size=-48),
        font.Font(family='Ubuntu Mono',size=-64),
        font.Font(family='Ubuntu Mono',size=-98),
    ]
    III_Font_S = [ # size negative is Pixels
        None,
        font.Font(family='Ubuntu Mono',size=-10), # 128 cpl
        font.Font(family='Ubuntu Mono',size=-15), #  85 = -14 ( but 80 = -15 looks better and works ok )
        font.Font(family='Ubuntu Mono',size=-17), #  73
        font.Font(family='Ubuntu Mono',size=-20), #  64 cpl
        font.Font(family='Ubuntu Mono',size=-30), #  42
        font.Font(family='Ubuntu Mono',size=-40), #  32
        font.Font(family='Ubuntu Mono',size=-60), #  21
    ]
    III_Font = [ III_Font_S , III_Font_M ][bool(k_scale==1.0)] # small or medium ?
    fill="gray"
    # I.Canvas screen ruler for chosing font sizes to simulate III size table.
    r0="         1         2         3         4         5         6         7         8         9        10        11        12        13"
    r1="␣␣␣␣␣␣␣␣␣1␣␣␣␣␣␣␣␣␣2␣␣␣␣␣␣␣␣␣3␣␣␣␣␣␣␣␣␣4␣␣␣␣␣␣␣␣␣5␣␣␣␣␣␣␣␣␣6␣␣␣␣␣␣␣␣␣7␣␣␣␣␣␣␣␣␣8␣␣␣␣␣␣␣␣␣9␣␣␣␣␣␣␣100␣␣␣␣␣␣␣110␣␣␣␣␣␣␣120␣␣␣␣␣␣␣130"
    r2="1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890"
    r3="ABCDEFGHIJKLMNOPQRSTUVWXYZ ≤ π ε ↓ ∩ ¬ ≠ ≡ ∞ ← → ↔ ∃ ∀ ⊗ ∂ ∧ ∪ ≥ ⊂ ↑ λ ∨ α ⊃"
    r4="abcdefghijklmnopqrstuvwxyz ~ ! @ # $ % ^ & * ( ) _ + { } | : ? > < , . / ; ' \\ \" β"
    beam = True
    tag = f'III#{canvas.ln:o}'
    (ox,oy) = [ (15,15) , (32,32) ][bool(k_scale==1.0)] # small or medium ?
    if 0:canvas.create_rectangle(5,5,682,682,fill=None,outline="orange",width=2,tags=tag)    
    if 0:canvas.create_rectangle(ox,oy,ox+k(1024),ox+k(1024),fill=Brown,outline="red",width=2,tags=tag)    
    canvas.create_text(ox+k(350),oy+k(450),text=f"III-{canvas.ln:o}",font=III_Font[7],fill="gray",anchor="nw",tags=tag)
    # canvas.create_text(50,220,text=f"Test pattern III#{i:o}",fill="gray",anchor="nw", font=("Ubuntu Mono",40),tags=tag)    
    if(1):
        canvas.create_text(ox,oy+k( 20),text=r0[:128]+"\n"+r2[:128]+"\n"+r3+"\n"+r4,    font=III_Font[1],fill="gray",anchor="nw",tags=tag)    
        canvas.create_text(ox,oy+k(120),text=r0[:85]+"\n"+r2[:85]+"\n"+r3[:85]+"\n"+r4[:85],   font=III_Font[2],fill="gray",anchor="nw",tags=tag)
    if(0):
        canvas.create_text(ox,oy+k(250),text=r0[:73]+"\n"+r2[:73],                      font=III_Font[3],fill="gray",anchor="nw",tags=tag)
        canvas.create_text(ox,oy+k(450),text=r0[:64]+"\n"+r2[:64]+"\n"+r3[:64],         font=III_Font[4],fill="gray",anchor="nw",tags=tag)
        canvas.create_text(ox,oy+k(600),text=r0[:42]+"\n"+r2[:42],                      font=III_Font[5],fill="gray",anchor="nw",tags=tag)
        canvas.create_text(ox,oy+k(720),text=r0[:32]+"\n"+r2[:32],                      font=III_Font[6],fill="gray",anchor="nw",tags=tag)
    if(1):
        canvas.create_text(ox,oy+k(850),text=r0[:21]+"\n"+r2[:21],                      font=III_Font[7],fill="gray",anchor="nw",tags=tag)
