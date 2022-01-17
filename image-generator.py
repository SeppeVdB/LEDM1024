from tkinter import *
from bitstring import BitArray


class Grid:

    def __init__(self, master, bamount, bsize, bspace, con, coff, bg):
        self.con = con
        self.coff = coff
        self.side_length = bamount*(bsize+2*bspace)+1
        self.bamount = bamount
        self.data = BitArray()

        self.canvas = Canvas(master, bg=bg, height=self.side_length, width=self.side_length)
        self.canvas.grid(row=0, sticky=NW)
        for i in range(self.bamount):
            for j in range(self.bamount):
                self.canvas.create_rectangle(bspace+j*(2*bspace+bsize)+2, bspace+i*(2*bspace+bsize)+2,
                                bspace+bsize+j*(2*bspace+bsize)+2, bspace+bsize+i*(2*bspace+bsize)+2,
                                fill=self.coff, tags="%dx%dy" % (j, i))
        self.canvas.bind("<Button-1>", self.change_color)

    def change_color(self, event):
        if self.canvas.find_withtag(CURRENT):
            if self.canvas.itemcget(CURRENT, 'fill') == self.coff: self.canvas.itemconfig(CURRENT, fill=self.con)
            else: self.canvas.itemconfig(CURRENT, fill=self.coff)

    def save(self):
        self.data = BitArray([0 if self.canvas.itemcget("%dx%dy" % (j, i), 'fill') == self.coff else 1
                              for i in range(self.bamount) for j in range(self.bamount)])
        print('{', end='')
        for i in range(63):
            print('0b', end='')
            print(self.data[i*16:i*16+16].bin, end=', ')
        print('0b', end='')
        print(self.data[1008:1024].bin, end='};\n')

    def clear(self):
        for i in range(self.bamount):
            for j in range(self.bamount):
                self.canvas.itemconfig("%dx%dy" % (i, j), fill=self.coff)

    def up(self):
        first_row = [self.canvas.itemcget("%dx0y" % i, 'fill') for i in range(self.bamount)]

        for i in range(self.bamount-1):
            for j in range(self.bamount):
                self.canvas.itemconfig("%dx%dy" % (j, i), fill=self.canvas.itemcget("%dx%dy" % (j, i+1), 'fill'))

        for i in range(self.bamount): self.canvas.itemconfig("%dx31y" % i, fill=first_row[i])

    def down(self):
        last_row = [self.canvas.itemcget("%dx31y" % i, 'fill') for i in range(self.bamount)]

        for i in range(self.bamount, 0, -1):
            for j in range(self.bamount):
                self.canvas.itemconfig("%dx%dy" % (j, i), fill=self.canvas.itemcget("%dx%dy" % (j, i-1), 'fill'))

        for i in range(self.bamount): self.canvas.itemconfig("%dx0y" % i, fill=last_row[i])

    def left(self):
        first_column = [self.canvas.itemcget("0x%dy" % i, 'fill') for i in range(self.bamount)]

        for i in range(self.bamount-1):
            for j in range(self.bamount):
                self.canvas.itemconfig("%dx%dy" % (i, j), fill=self.canvas.itemcget("%dx%dy" % (i+1, j), 'fill'))

        for i in range(self.bamount): self.canvas.itemconfig("31x%dy" % i, fill=first_column[i])

    def right(self):
        last_column = [self.canvas.itemcget("31x%dy" % i, 'fill') for i in range(self.bamount)]

        for i in range(self.bamount, 0, -1):
            for j in range(self.bamount):
                self.canvas.itemconfig("%dx%dy" % (i, j), fill=self.canvas.itemcget("%dx%dy" % (i-1, j), 'fill'))

        for i in range(self.bamount): self.canvas.itemconfig("0x%dy" % i, fill=last_column[i])


class Window(Frame):

    def __init__(self, master=None):
        Frame.__init__(self, master)
        self.master = master
        self.init_window()

    def init_window(self):
        self.master.title('LED Matrix')
        self.pack(fill=BOTH, expand=1)

        frame_1 = Frame(root)
        frame_1.pack(side=LEFT, fill=BOTH)
        frame_2 = Frame(root)
        frame_2.pack(side=LEFT, fill=BOTH)

        self.grid = Grid(master=frame_1, bamount=32, bsize=20, bspace=2, con="blue", coff="white", bg="grey")

        menu = Menu(self.master)
        self.master.config(menu=menu)

        file = Menu(menu)
        file.add_command(label='Open File')
        file.add_command(label='Save')
        file.add_command(label='Save As...')
        file.add_separator()
        file.add_command(label='Exit', command=self.client_exit)
        menu.add_cascade(label='File', menu=file)

        edit = Menu(menu)
        edit.add_command(label='Clear', command=self.grid.clear)
        edit.add_separator()
        edit.add_command(label='Scroll Up', command=self.grid.up)
        edit.add_command(label='Scroll Down', command=self.grid.down)
        edit.add_command(label='Scroll Left', command=self.grid.left)
        edit.add_command(label='Scroll Right', command=self.grid.right)
        menu.add_cascade(label='Edit', menu=edit)

        frame = Menu(menu)
        frame.add_command(label='Previous Frame')
        frame.add_command(label='Next Frame')
        frame.add_command(label='First Frame')
        frame.add_command(label='Last Frame')
        frame.add_separator()
        frame.add_command(label='Append New Frame')
        frame.add_command(label='Insert New Frame (After)')
        frame.add_command(label='Insert New Frame (Before)')
        menu.add_cascade(label='Frame', menu=frame)

        Button(master=frame_2, text="▲", command=self.grid.up, font='tkfixedfont').grid(row=4, column=2, sticky=NE+SW)
        Button(master=frame_2, text="▼", command=self.grid.down, font='tkfixedfont').grid(row=6, column=2, sticky=NE+SW)
        Button(master=frame_2, text="◄", command=self.grid.left, font='tkfixedfont').grid(row=5, column=1, sticky=NE+SW)
        Button(master=frame_2, text="►", command=self.grid.right, font='tkfixedfont').grid(row=5, column=3, sticky=NE+SW)

        Button(master=frame_2, text="save to bits", command=self.grid.save).grid(row=2, column=0, columnspan=5, sticky=W)
        Button(master=frame_2, text="clear screen", command=self.grid.clear).grid(row=1, column=0, columnspan=5, sticky=W)

        col_count, row_count = frame_2.grid_size()
        for col in range(col_count):
            frame_2.grid_columnconfigure(col, minsize=30)
        for row in range(row_count):
            frame_2.grid_rowconfigure(row, minsize=30)

    @staticmethod
    def client_exit():
        exit()


if __name__ == '__main__':
    root = Tk()

    app = Window(root)

    root.mainloop()
