from Tkinter import *
import zmq
from PIL import Image, ImageTk
import cStringIO

sub_port = 5929
pub_port = 5930

context = zmq.Context()
socket_sub = context.socket(zmq.SUB)
socket_sub.bind("tcp://*:%s" % sub_port)
socket_sub.setsockopt(zmq.SUBSCRIBE, '')

master = Tk()
canvas = Canvas(master, width=320, height=200)
canvas.grid(row=0, column=0)
canv_image = Image.open('xkcd.png')
canv_photo = ImageTk.PhotoImage(canv_image)
canv_img_item = canvas.create_image(0,0,anchor=NW,image=canv_photo)

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
    print 'hi'
    data = check_socket(socket_sub)
    if data is not None:
        print len(data)
        canv_image = Image.open(cStringIO.StringIO(data))
        canv_image = canv_image.resize((240,150))
        canv_photo = ImageTk.PhotoImage(canv_image)
        canvas.itemconfig(canv_img_item, image=canv_photo)
    master.after(50, getData)


def callback():
    print "click!"

"""
b = Button(master, text="OK", command=callback)
b.pack()
"""

master.after(50, getData)

mainloop()
