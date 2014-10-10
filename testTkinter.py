#!/usr/bin/python
from Tkinter import *
import zmq
from PIL import Image, ImageTk
import cStringIO, time
import subprocess
import tkFont

sub_port = 5929
pub_port = 5930

context = zmq.Context()
socket_sub = context.socket(zmq.SUB)
socket_sub.bind("tcp://*:%s" % sub_port)
socket_sub.setsockopt(zmq.SUBSCRIBE, '')
socket_pub = context.socket(zmq.PUB)
socket_pub.connect("tcp://localhost:%s" % pub_port)

recording = False

def record_button_press():
  global recording
  if not recording:
    # start recording
    socket_pub.send("record", zmq.NOBLOCK)
    rec_button.configure(text = "Stop", bg="red", activebackground='red')
  else:
    # stop recording
    socket_pub.send("stop", zmq.NOBLOCK)
    rec_button.configure(text = "Rec", bg="green", activebackground='green')
  recording = not recording
  print 'button clicked'

def close():
  global proc
  global master
  proc = subprocess.Popen("sudo killall test", shell=True)
  time.sleep(1)
  master.quit()

master = Tk()
canvas = Canvas(master, width=300, height=180)
canvas.grid(row=0, column=0)
canv_image = Image.open('/home/pi/pi-car-log/xkcd.png')
canv_image.thumbnail((240,150))
canv_photo = ImageTk.PhotoImage(canv_image)
canv_img_item = canvas.create_image(0,0,anchor=NW,image=canv_photo)
helv15= tkFont.Font(family='Helvetica', size=15, weight='bold')
rec_button = Button(master, text = "Rec", font=helv15,
    command=record_button_press)
rec_button.configure(width=5,height=4,bg='green',activebackground='green')
rec_button.place(relx=0.90,rely=0.2,anchor="c")
close_button = Button(master, text = "Exit", font=helv15,
    command=close)
close_button.configure(width=5)
close_button.place(relx=0.2,rely=0.96,anchor="s")

def check_socket(socket):
    try: 
        data = socket.recv(flags=zmq.NOBLOCK)
        return data
    except zmq.ZMQError as e:
        if "Resource temporarily unavailable" in e:
            return None

def getData():
    global canv_photo
    global canv_image
    global proc
    data = check_socket(socket_sub)
    if data is not None:
        print len(data)
        canv_image = Image.open(cStringIO.StringIO(data))
        canv_image.thumbnail((240,150))
        canv_photo = ImageTk.PhotoImage(canv_image)
        canvas.itemconfig(canv_img_item, image=canv_photo)
    master.after(50, getData)


def callback():
    print "click!"


master.protocol("WM_DELETE_WINDOW", close)
master.after(50, getData)

proc = subprocess.Popen("sudo /home/pi/pi-car-log/libuvc/bld/test", shell=True)

mainloop()
