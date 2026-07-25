# shared_SL_tui.py
# curses-based frontend for the Shared Shopping List demo app

# (c) 2026-07-24 <christian.tschudin@unibas.ch> - MIT license

import asyncio
import curses
import hashlib
import os
import time

class TUI(): 
    def __init__(self, stdscr, args):
        self.args = args
        self.stdscr = stdscr

        self.shopping_list = {}  # {"item_text" : (-)timestamp}
        self.current_txt = None  # current item text, or None
        
        self.input = ''
        self.input_offs = 0

        self.stdscr.leaveok(True)
        self.stdscr.nodelay(True)
        curses.update_lines_cols()
        curses.curs_set(1)
        self.outscr = curses.newwin(curses.LINES-4, curses.COLS-2, 1, 1)
        self.outscr.scrollok(1)
        self.inpscr = curses.newwin(3, curses.COLS, curses.LINES - 3, 0)

        self.SSLtag = hashlib.sha1(b"simple-shopping-list-20260701").digest()

    async def run(self, mlan):
        self.mlan = mlan
        asyncio.create_task(self._wheel())
        asyncio.create_task(self._sync())

        self._redraw_display()

        while True: # keyboard loop
            try:
                char = self.stdscr.get_wch()
                if char == curses.ERR:
                    await asyncio.sleep(0.1)
                elif char == '\x03': # ctrl-c
                    break
            except curses.error:
                await asyncio.sleep(0.1)
                continue
            if char == curses.KEY_RESIZE:
                self._redraw_display()
            else:
                self._handle_char(char)

    def update(self, item): # upcall - we must react to a memoryLAN update
        exp, txt = self._from_mempage(item)
        if exp == None:  # not for this app
            return
        chg = False

        if txt in self.shopping_list: # merge based on timestamps
            ts = self.shopping_list[txt]
            if abs(exp) > abs(ts): # they were more recent, hence win
                self.shopping_list[txt] = exp
                chg = True
        elif exp > 0: # if unknown, only merge undeleted entries
                      # (we risk to believe outdated stuff, but store less)
            self.shopping_list[txt] = exp
            chg = True
        if chg:
            self._adjust_current_txt(0)

    # internal methods -----------------------------------------------------

    def _to_mempage(self, txt):
        ts = self.shopping_list[txt]
        return self.SSLtag + int(ts).to_bytes(4, signed=True) + txt.encode()

    def _from_mempage(self, mpg):
        if mpg[:len(self.SSLtag)] != self.SSLtag:
            return None, None # not for us
        mpg = mpg[len(self.SSLtag):]
        return int.from_bytes(mpg[:4], signed=True), mpg[4:].decode()

    async def _sync(self): # randomized app-level sync
        while True:
            if len(self.shopping_list) > 0:
                # pick random entry, push it to the memoryLAN
                lst = list(self.shopping_list.keys())
                txt = lst[int.from_bytes(os.urandom(2)) % len(lst)]
                self.mlan.add(self._to_mempage(txt))

            await asyncio.sleep(1)
            
        
    async def _wheel(self): # eye candy (lifeness of app, refresh debug info)
        w = '|/-\\'
        pos = 0
        while True:
            self.stdscr.addstr(0,0,w[pos] + ' ')
            self.stdscr.refresh()
            self.inpscr.refresh() # refocusing the cursor to input field
            pos = (pos+1) % len(w)

            if self.args.debug:
                self.outscr.addstr(curses.LINES-12, curses.COLS-12, 'memoryLAN')
                self.outscr.addstr(curses.LINES-11, curses.COLS-12, 'cache:')
                for i in range(6):
                    self.outscr.addstr(curses.LINES - 10 + i,
                                       curses.COLS - 20 - 4, ' ' * 20)
                for i,m in enumerate(list(self.mlan.cache)):
                    ts, m = self._from_mempage(m)
                    if m == None: continue
                    if ts < 0: # is a deleted entry
                        m = '(-) ' + m
                    self.outscr.addstr(curses.LINES - 10 + i,
                                       curses.COLS - len(m) - 4, m)
                self.outscr.refresh()

            await asyncio.sleep(.2)

    def _redraw_display(self):
        curses.update_lines_cols()

        self.stdscr.resize(curses.LINES,curses.COLS)
        self.stdscr.erase()
        self.stdscr.box()

        s = self.args.ticket
        s = f"ticket= {s if len(s) < 30 else s[0:30]+'...'} "
        s = f" Shared Shopping List ({s}) "
        self.stdscr.addstr(0, (curses.COLS - len(s)) // 2, s)

        self.outscr.resize(curses.LINES-4, curses.COLS-2)
        self.outscr.mvwin(1,1)
        self.outscr.erase()
        if len(self.shopping_list) == 0:
            self.outscr.addstr("enter a new item to the list")
        self.outscr.move(0,0)

        self.inpscr.resize(3, curses.COLS)
        self.inpscr.mvwin(curses.LINES - 3, 0)
        self.inpscr.addstr(1, 1, '> ' + self.input)
        self.inpscr.clrtoeol()
        self.inpscr.box()
        s = " ^ or v to select, ^D=delete, ^C=exit "
        self.inpscr.addstr(2, curses.COLS-len(s)-2, s)
        self.inpscr.move(1, 3 + len(self.input))

        self.stdscr.refresh()
        self.inpscr.refresh()

        lst = self.shopping_list.keys()
        lst = sorted([ k for k in lst if self.shopping_list[k] > 0 ])
        for t in lst:
            self.outscr.addstr(">> " if t == self.current_txt else "   ")
            if self.args.debug:
                exp = abs(self.shopping_list[t]) - int(time.time())
                t += f"   age={exp}"
            self.outscr.addstr(f"{t}\n")

        if self.args.debug:
            lst = self.shopping_list.keys()
            lst = sorted([ k for k in lst if self.shopping_list[k] < 0 ])
            if len(lst) > 0:
                self.outscr.addstr("DELETED:\n")
                for t in lst:
                    exp = abs(self.shopping_list[t]) - int(time.time())
                    self.outscr.addstr(f"   {t} age={exp}\n")

        self.outscr.refresh()
    
    def _adjust_current_txt(self, delta):
        if len(self.shopping_list) == 0:
            self.current_txt = None
        else:
            lst = self.shopping_list.keys()
            lst = sorted([ k for k in lst if self.shopping_list[k] > 0])
            if len(lst) == 1:
                self.current_txt = lst[0]
            else:
                if self.current_txt in lst:
                    ndx = lst.index(self.current_txt) + delta
                    if ndx < 0: ndx = 0
                    elif ndx >= len(lst): ndx = len(lst) - 1
                    self.current_txt = lst[ndx]
                elif len(lst) > 0: # find entry closest to the deleted one
                    c = lst[-1]
                    for k in reversed(lst):
                        if self.current_txt > k:
                            break
                        c = k
                    self.current_txt = c
                else:
                    self.current_txt = None
        self._redraw_display()
            
    def _handle_char(self, c):
        x = self.inpscr.getyx()[1] - 3
        mx = self.inpscr.getmaxyx()[1] - 3  # available width for input
        txt = self.input[self.input_offs:]

        if c == '\n' and len(txt) > 0: # newline --> new item
            self.inpscr.move(1, 3)
            self.inpscr.clrtoeol()
            self.inpscr.move(1, 3)
            if len(self.input) == 0:
                return
            txt = self.input
            self.input = ''
            self.input_offs = 0

            self.shopping_list[txt] = int(time.time())
            # create a 'this-is-new' memory page, disseminate
            mpg = self._to_mempage(txt)
            self.mlan.add(mpg)
            self.current_txt = txt
            self._redraw_display()
            return
        
        # handle special keys
        if c == curses.KEY_UP:
            self._adjust_current_txt(-1)
            return
        if c == curses.KEY_DOWN:
            self._adjust_current_txt(+1)
            return
        if c == curses.KEY_BACKSPACE:  c = '\x08'
        if c == curses.KEY_LEFT:       c = '\x02' # ctrl-b
        if c == curses.KEY_RIGHT:      c = '\x06' # ctrl-f
        if type(c) == int: # ignore other special keys
            return

        # control characters
        if c == '\x12': # ctrl-r (refresh)
            self._redraw_display()
            return
        if c == '\x04': # ctrl-d (delete)
            if self.current_txt == None:
                return
            # record as a toombstone
            self.shopping_list[self.current_txt] = - int(time.time())
            # create a 'this-is-deleted' memory page, disseminate
            mpg = self._to_mempage(self.current_txt)
            self.mlan.add(mpg)
            self._adjust_current_txt(0)

        # all following branches are for input line editing :-(
        if c == '\x01': # ctrl-a (home)
            self.input_offs = 0
            self.inpscr.move(1, 3)
            visible = self.input[:mx]
            self.inpscr.addstr(1, 3, visible + ' ' * (mx - len(visible)))
            self.inpscr.move(1, 3)

        elif c == '\x02': # ctrl-b (left)
            if x > 0:
                self.inpscr.move(1, 3 + x - 1)
            elif self.input_offs > 0:
                self.input_offs -= 1
                visible = self.input[self.input_offs:self.input_offs+mx]
                self.inpscr.addstr(1, 3, visible + ' ' * (mx - len(visible)))
                self.inpscr.move(1, 3)

        elif c == '\x05': # ctrl-e (end)
            if len(self.input) > mx:
                self.input_offs = len(self.input) - mx
            else:
                self.input_offs = 0
            visible = self.input[self.input_offs:]
            self.inpscr.addstr(1, 3, visible)
            self.inpscr.move(1, 3 + len(visible))

        elif c == '\x06': # ctrl-f (right)
            if x < len(self.input) - self.input_offs:
                if x < mx - 1:
                    self.inpscr.move(1, 3 + x + 1)
                else:
                    if self.input_offs + mx < len(self.input):
                        self.input_offs += 1
                        visible = self.input[self.input_offs:self.input_offs+mx]
                        self.inpscr.addstr(1, 3, visible)
                        self.inpscr.move(1, 3 + mx - 1)
        
        elif c == '\x08' or ord(c) == 127: # bs or delete-left
            pos = self.input_offs + x
            if pos > 0:
                self.input = self.input[:pos-1] + self.input[pos:]
                if x > 0:
                    self.inpscr.move(1, 3 + x - 1)
                else:
                    if self.input_offs > 0:
                        self.input_offs -= 1
                    self.inpscr.move(1, 3)
                visible = self.input[self.input_offs:self.input_offs+mx]
                self.inpscr.addstr(1, 3, visible + ' ' * (mx - len(visible)))

        elif ord(c) >= 32: # regular character
            pos = self.input_offs + x
            self.input = self.input[:pos] + c + self.input[pos:]

            # adjust horizontal scroll offset if needed
            if x >= mx - 1:
                self.input_offs += 1
                visible = self.input[self.input_offs:self.input_offs+mx]
                self.inpscr.addstr(1, 3, visible)
                self.inpscr.move(1, 3 + mx - 1)
            else:
                visible = self.input[self.input_offs:self.input_offs+mx]
                self.inpscr.addstr(1, 3, visible + ' ' * (mx - len(visible)))
                self.inpscr.move(1, 3 + x + 1)

        self.inpscr.refresh()

    pass

# eof
