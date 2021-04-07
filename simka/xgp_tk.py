#!/usr/bin/python3
# Filename: simka/xgp_tk.py -*- mode:Python; coding:utf-8 -*-

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
pixel = None
Black="#202020"
offblack="#444"
Brown="#402020"
servsock=None
usersock=None

def main():
    
    root = Tk()
    root.tk_focusFollowsMouse()
    root.title("XGP Xerox Graphics Printer");
    root["bg"]=offblack
    
    canvas = Canvas( root,
                     width=(1700+32), height=(2200+32),
                     takefocus=1,
                     highlightthicknes=4,
                     highlightcolor="#aa0", # dull yellow
                     highlightbackground="gray",
                     background=offblack,
    );
    canvas.pack( side="top", padx=0, pady=0 )
    
    Button( root, text="Quit",
            font = font.Font(size=30,weight='bold'),
            fg="darkred",bg="white",
            activeforeground="white",
            activebackground="darkred",
            command=root.quit ).pack(side="bottom")
    try:
        root.mainloop()
    finally:
        if servsock is not None:
            servsock.close()
        if usersock is not None:
            usersock.close()

if __name__ == "__main__" :
    main()
