import tkinter

from multiprocessing import shared_memory
from multiprocessing .resource_tracker import unregister
import numpy as np
# import matplotlib.pyplot as plt
# Implement the default Matplotlib key bindings.
#from matplotlib.backend_bases import key_press_handler
from matplotlib.backends.backend_tkagg import (FigureCanvasTkAgg, NavigationToolbar2Tk)
from matplotlib.figure import Figure
from matplotlib.animation import FuncAnimation

existingshm=shared_memory.SharedMemory(name='shmRtt')
# for val in existingshm.buf:
#   print(val, end=" ")
c=np.ndarray((256,), dtype=np.uint32, buffer=existingshm.buf[12:])
#sBuf = np.ndarray(dtype=np.uint32)
sBuf = []

oldAdr = 0
cAdrInt = 0

# pTime = list(range(0, 256))
root = tkinter.Tk()
root.wm_title("rtt data trace")

def getSamples():
  global c
  global sBuf
  global ax
  global adrSel
  global oldAdr
  global cAdrInt
  global samplingStatus

  if ((cAdrInt != 0) and (True == samplingStatus) ):
    head = existingshm.buf[8]
    tail = existingshm.buf[9]
    if ( head > tail):
      sBuf.extend(c[tail:head])
    else:
      sBuf.extend(c[tail:])
      sBuf.extend(c[0:head])
    t1 = list(range(0, len(sBuf)))
    existingshm.buf[9] = head
    
    # print ("sBuf last val:", sBuf[-1], " size: ", len(sBuf))
    # print (" h:",head," t:", tail)
    
    ax.clear()
    ax.scatter(t1, sBuf)
    canvas.draw()
  
  #update address in mapped memory according to the user input in the tkinter Entry widget
  lAdr = adrSel.get()
  cAdrInt = 0
  if( 10 == len(lAdr) ): #0x11223344 - has length 11
    if( "0x" == lAdr[0:2]):
      cAdrInt = int(lAdr[2:], 16)
  if (cAdrInt != 0):
    if ( cAdrInt != oldAdr):
      #put data in little endian format
      existingshm.buf[3] = cAdrInt >> 24
      existingshm.buf[2] = (cAdrInt >> 16) & 0xFF
      existingshm.buf[1] = (cAdrInt >> 8) & 0xFF
      existingshm.buf[0] = cAdrInt & 0xFF
      oldAdr = cAdrInt
      print("adr changed to:",f'{cAdrInt:#x}')

  if ( len(sBuf) < 8000):
    root.after(10, getSamples)
  else:
    root.after(1000,getSamples)

fig = Figure(figsize=(5, 4), dpi=100)

ax = fig.add_subplot()
ax.set_xlabel("time [ms]")
ax.set_ylabel("val")
# for i,j in zip(range(0, c.size),c):
#   if ( 0 == i%10 ):
#     ax.annotate(str(j),xy=(i,j))

canvas = FigureCanvasTkAgg(fig, master=root)  # A tk.DrawingArea.
canvas.draw()

# pack_toolbar=False will make it easier to use a layout manager later on.
toolbar = NavigationToolbar2Tk(canvas, root, pack_toolbar=False)
toolbar.update()

# canvas.mpl_connect(
#     "key_press_event", lambda event: print(f"you pressed {event.key}"))
# canvas.mpl_connect("key_press_event", key_press_handler)

button_quit = tkinter.Button(master=root, text="Quit", command=root.destroy)

samplingStatus = False

def bStartStopPressed():
  global samplingStatus
  global button_startStop
  if ( False == samplingStatus ):
    samplingStatus = True
    button_startStop.configure(text='Stop')
  else:
    samplingStatus = False
    button_startStop.configure(text='Start')

def bClearGraphPressed():
  global sBuf
  sBuf = []

adrSel = tkinter.StringVar()
entry_addrSel = tkinter.Entry(master=root, textvariable=adrSel)

button_startStop = tkinter.Button(master=root, text="Start", command=bStartStopPressed)
button_clearGraph = tkinter.Button(master=root, text="ClearGraph", command=bClearGraphPressed)



# Packing order is important. Widgets are processed sequentially and if there
# is no space left, because the window is too small, they are not displayed.
# The canvas is rather flexible in its size, so we pack it last which makes
# sure the UI controls are displayed as long as possible.
button_quit.pack(side=tkinter.BOTTOM)
button_startStop.pack(side=tkinter.BOTTOM)
button_clearGraph.pack(side=tkinter.BOTTOM)
entry_addrSel.pack(side=tkinter.BOTTOM)
toolbar.pack(side=tkinter.BOTTOM, fill=tkinter.X)
canvas.get_tk_widget().pack(side=tkinter.TOP, fill=tkinter.BOTH, expand=True)

getSamples()
tkinter.mainloop()

# print(sBuf)

unregister(existingshm._name, 'shared_memory')