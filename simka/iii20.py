#!/usr/bin/python3
# -*- mode: Python; coding: utf-8 -*-
"""
~/simka/iii6term.py
Reënact six Triple-III vector display terminals.
"""
import tkinter as tk
from tkinter import font
from tkinter import *
import socket
import threading
import iii_tk as iii
import lester_keyboard as lkb
import pdb
import os
import sys

# global
root=None
servsock=None
usersock=None
iiilabstr=[
    "III-20 Display Room east",
    "III-21 hand-eye south",
    "III-22 Display Room middle",
    "III-23 hand-eye north",
    "III-24 Music Room",
    "III-25 Display Room west",
    ]

def one_pane(myline):
    root = tk.Tk()
    root.geometry("1440x1440+1690+600") # middle panel
    root.title(f"III-2{myline-0o20} terminal")
    root.tk_focusFollowsMouse()
    root.configure(bg="#404040");
    (ox,oy)=(32,32)
    root.frame = tk.Frame( root,
                           width=128,height=128,
                           highlightthicknes=0,
                           highlightcolor="gold",
                           highlightbackground="gray",
                           background="#404040", )
    root.frame.pack(side="top")
    root.canvas = tk.Canvas( root,
                             width=(1024+2*ox), height=(1024+2*oy),
                             takefocus=1,
                             highlightthicknes=4,
                             highlightcolor="#AA0",
                             highlightbackground="gray",
                             background="black",
    )
    root.canvas.pack(side="top")
    root.button = tk.Button( root,
                             text="Quit",
                             font = font.Font(size=30,weight='bold'),
                             fg="#822",
                             bg="gray",
                             activeforeground="white",
                             activebackground="darkred",
                             command=root.quit)
    root.button.pack(side="top")
    root.can= [ root.canvas, ]
    root.canvas.kb = myline - 0o20
    root.canvas.ln = myline
    iii.k_scale = 1.0
    return root

def six_panes():
    global root
    root = Tk()
    root.geometry("1440x2500+0+0")
    root.tk_focusFollowsMouse()
    root.configure(bg="darkgray");
    root.title("III six terminals");
    root.title("III Sexplex");
    button = Button( root, text="Quit",
                     font = font.Font(size=40,weight='bold'),
                     fg="darkred",command=root.quit )
    button.pack(side="bottom")
                 
    left  = Frame(root, borderwidth=0, relief="solid")
    right = Frame(root, borderwidth=0, relief="solid")

    box = [0]*6
    can = [0]*6
    label = [0]*6

    for i in range(0,6):
        box[i] = Frame( (left,right)[i>=3], borderwidth=2, relief="solid")
        label[i] = Label(box[i],
                         text= iiilabstr[i],
                         font=("Ubuntu Mono",30))
        can[i] = Canvas(
            box[i],
            width=680,height=680,
            takefocus=1,
            highlightthicknes=4,
            highlightcolor="gold",
            highlightbackground="gray",
            background='black',
            name="iii2"+str(i)
        )
        # can[i].dkb_line_num = i
        can[i].kb = i
        can[i].ln = i + 0o20
        can[i].pack(side="top", padx=0, pady=0)
        box[i].pack(expand=True, fill="both", padx=0, pady=0)
        label[i].pack(pady=10)

        left.pack(side="left",  expand=True, fill="both")
        right.pack(side="right", expand=True, fill="both")
    root.can = can
    k_scale = (640.0/1024.0)
    return root       

class sock_service( threading.Thread ):
    def __init__( me ):
        threading.Thread.__init__( me )
    def run( me ):
        global tick,root,servsock,usersock
        print("sim-III-service for six terminals at well-known-port #1974")
        servsock = socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        servsock.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
        servsock.bind(('',1974))
        servsock.listen(5)
        try:
            while True:
                print("Accept connections. Waiting at well-know-port #1974...")
                (usersock,a) = servsock.accept()
                print("Connected ",a,usersock)
                lkb.usersock = usersock
                iii.usersock = usersock
                usersock.sendall("iii_mask=07700;".encode())
                kid2 = iii.Triple_III_display_service()
                kid2.setDaemon(True)
                kid2.start()
        finally:
            servsock.close()
            print("iii6term display server thread exiting")            

def main():
    global root
    # root= six_panes()
    myname = sys.argv[0][2:-3]
    myline = 0o20 + int(myname[-1])
    print(f'myname="{myname}" myline={myline}')
    root = {
        "iii20" : one_pane,
        "iii21" : one_pane,
        "iii22" : one_pane,
        "iii23" : one_pane,
        "iii24" : one_pane,
        "iii25" : one_pane,
        "iii6term" : six_panes,
        }[myname](myline)
    III = iii.III_App()
    LKB = lkb.LKB_App()    
    kid = sock_service()
    kid.setDaemon(True)
    kid.start()
    iii.root = root
    iii.tk = root
    for canv in root.can:
        canv.bind("<Any-KeyPress>",   LKB.keyhit)
        canv.bind("<Any-KeyRelease>", LKB.keyrelease)
        iii.test_pattern(canv)
    try:
        root.mainloop()
    finally:
        if servsock is not None:
           servsock.close()
        if iii.usersock is not None:
            iii.usersock.close()

if __name__ == "__main__" :
    main()
