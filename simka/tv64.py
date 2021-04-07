#!/usr/bin/python3
# Filename: simka/ten_color_squares.py -*- mode: Python; coding: utf-8 -*-
'''
Create ten tkinter top level windows.
A keep-it-simple example,
`2020-07-24 bgbaumgart@mac.com'
'''
import tkinter as tk
from tkinter import font
from tkinter import *

root = tk.Tk()
root.title("root darkblue")
root.configure(bg="darkblue")
root.geometry("200x200+1500+1200")

window = tk.Toplevel()
window.title("darkgreen")
window.configure( bg="darkgreen")
window.geometry("200x200+1800+1200")

window = tk.Toplevel()
window.title("window darkred")
window.configure( bg="darkred")
window.geometry("200x200+2100+1200")

window = tk.Toplevel()
window.title("window #500")
window.configure( bg="#500")
window.geometry("200x200+2400+1200")

window = tk.Toplevel()
window.title("window #077")
window.configure( bg="#077")
window.geometry("200x200+1500+1500")

window = tk.Toplevel()
window.title("window #770")
window.configure( bg="#770")
window.geometry("200x200+1800+1500")

window = tk.Toplevel()
window.title("window #707")
window.configure( bg="#707")
window.geometry("200x200+2100+1500")


window = tk.Toplevel()
window.title("window #055")
window.configure( bg="#055")
window.geometry("200x200+1500+1800")

window = tk.Toplevel()
window.title("window #550")
window.configure( bg="#550")
window.geometry("200x200+1800+1800")

window = tk.Toplevel()
window.title("window #505")
window.configure( bg="#505")
window.geometry("200x200+2100+1800")

root.mainloop()
