#!/usr/bin/python3

from tkinter import *

root = Tk()
root.geometry("2700x2300+0+0")
root.tk_focusFollowsMouse()
button = Button( root, text="Quit", fg="darkred", command=root.quit )
button.pack(side="bottom")
                 
left  = Frame(root, borderwidth=0, relief="solid")
right = Frame(root, borderwidth=0, relief="solid")

box = [0]*8
can = [0]*8
label = [0]*8

for i in range(0,4):
    box[i] = Frame( (left,right)[i>=2], borderwidth=2, relief="solid")
    label[i] = Label(box[i],text="III2"+str(i))
    can[i] = Canvas(
        box[i],
        width=1024,height=1024,
        takefocus=1,
        highlightthicknes=10,
        highlightcolor="gold",
        highlightbackground="gray"
    )
    can[i].pack(side="top",expand=True)
    can[i].configure(background='black')
    box[i].pack(expand=True, fill="both", padx=10, pady=10)
    label[i].pack()

left.pack(side="left",  expand=True, fill="both")
right.pack(side="right", expand=True, fill="both")
root.mainloop()
