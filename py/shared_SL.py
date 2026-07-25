#!/usr/bin/env python3

# shared_SL.py
# "shared shopping list" demo app, running over MemoryLAN over UDP multicast

# (c) 2026-07-24 <christian.tschudin@unibas.ch> - MIT license

import asyncio
import curses
import memoryLAN
from shared_SL_tui import TUI

# ---------------------------------------------------------------------------

async def backend(args):
    # create and start local memoryLAN switch
    mlan = memoryLAN.Switch()
    await mlan.begin(args.ticket, lambda mpg: args.tui.update(mpg), args.error)

    # start frontend
    await args.tui.run(mlan) # provide access to the mlan layer

def main(stdscr, args):
    args.tui = TUI(stdscr, args)  # create frontend
    try:
        asyncio.run(backend(args))
    except KeyboardInterrupt:
        pass

# ---------------------------------------------------------------------------
if __name__ == "__main__":
    import argparse
    default_ticket = 'udp:230.4.4.1:5007'

    parser = argparse.ArgumentParser()
    parser.add_argument('-debug', action='store_true')
    parser.add_argument('-error', type=float, default=0, help='error rate')
    parser.add_argument('-ticket', type=str, default=default_ticket,
                        help=f"multicast group e.g., '{default_ticket}'")
    args = parser.parse_args()

    curses.wrapper(lambda stdscr:main(stdscr, args))

# eof
