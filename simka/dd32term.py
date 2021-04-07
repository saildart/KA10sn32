#!/usr/bin/python3
# -*- mode: Python; coding: utf-8 -*-
"""
~/simka/dd32term.py
Reënact 32 Data Disc raster display terminals.
"""
import tkinter as tk
from tkinter import font
from tkinter import *
import socket
import threading


import pdb

def _32_pane():
    root = Tk()
    root.geometry("1440x2540+0+0")
    root.tk_focusFollowsMouse()
    root.configure(bg="darkgray");
    button = Button( root, text="Quit",
                     font = font.Font(size=40,weight='bold'),
                     fg="darkred",
                     command=root.quit )
    button.pack(side="bottom")
    col = [0]*4
    for i in range(0,4):
        col[i]=Frame(root, borderwidth=0, relief="solid")
    box = [0]*32
    can = [0]*32
    label = [0]*32
    for i in range(0,32):
        box[i] = Frame( col[int(i/8)], borderwidth=2, relief="solid")
        label[i] = Label(box[i],text="Line#"+str(6+i))
        can[i] = Canvas(
            box[i],
#           width=256,height=240,
            width=264,height=248,
            takefocus=1,
            highlightthicknes=8,
            highlightcolor="gold",
            highlightbackground="gray",
            borderwidth=2,
        )
        can[i].dkb_line_num = i+6
        can[i].pack(side="top",expand=True)
        can[i].configure(background='black')
        can[i].create_text(100,100,text="canvas #"+str(i),fill="white",anchor="nw")
        box[i].pack(expand=True, fill="both", padx=0, pady=0)
        label[i].pack()
    col[0].pack(side="left",  expand=True, fill="both")
    col[1].pack(side="left",  expand=True, fill="both")
    col[2].pack(side="left",  expand=True, fill="both")
    col[3].pack(side="right", expand=True, fill="both")
    root.can = can
    return root

# global
root=None
servsock=None
usersock=None

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
                kid2 = iii.Triple_III_display_service()
                kid2.setDaemon(True)
                kid2.start()
        finally:
            servsock.close()
            print("iii6term display server thread exiting")            

def main():
    global root
    root= _32_pane()
    if(0):
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
    else:
        root.mainloop()
        

if __name__ == "__main__" :
    main()
