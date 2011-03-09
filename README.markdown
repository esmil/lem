A Lua Event Machine
===================


About
-----

The Lua Event Machine is basically a [Lua][] interpreter with a built-in
[libev][] main loop.

All Lua code is run in coroutines so that modules can suspend the currently
running code, register callbacks with the event loop and wait for events
to happen before resuming the coroutine.

This allows libraries to be written such that calls appear to be blocking,
while still allowing other Lua coroutines to run. One just have to remember
that all global variables (and variables shared in closures) may be changed
by other coroutines when calling functions which might suspend the currently
running coroutine for a while.

This also allows you to write libraries which automatically spawn
new coroutines and run Lua functions to handle incoming events.

[Lua]: http://www.lua.org/
[libev]: http://libev.schmorp.de/

Installation
------------

Get the sources and do

    $ make install

This will install the `lem` binary to `/usr/local/bin`, a utility
library to `/usr/local/lib/lua/5.1/lem/utils.so` and some C headers
to `/usr/local/include/`.

Use

    $ make clean
    $ make PREFIX=/install/path install

to change the install path.

Both the Lua 5.1.4 and libev 4.03 sources are included so having Lua or
libev installed on your system is not required to build the Lua Event Machine.

You can, however, link against a shared Lua or [LuaJIT][] library by
setting the LUA variable to point to the installed interpreter. E.g:

    $ make clean
    $ make PREFIX=/install/path LUA=/path/to/lua/or/luajit2 install

[luajit]: http://luajit.org/luajit.html

Usage
-----

The `lem` interpreter will behave just like the normal standalone Lua
interpreter except there is no built-in REPL. You can indeed run
all your normal Lua scripts using it. Type

    $ lem myscript.lua

to run `myscript.lua` or make the script executable and add a hash-bang
header as in

    #!/usr/bin/env lem

    local utils = require 'lem.utils'

    (etc.)

Just like the normal stand-alone interpreter command line arguments
are stored in the global table `arg` where `arg[-1]` is the interpreter,
`arg[0]` is the script name and normal arguments begin at `arg[1]`.

Running Lua scripts in the Lua Event Machine however, will allow you
to load the lem modules, which will fail in the normal interpreter.

The Utility Library
-------------------

The Lua Event Machine comes with a small utility library which contains
some basic building blocks for spawning new coroutines and synchronizing
between running coroutines.

The library is imported using

    local utils = require 'lem.utils'

This sets `utils` to a table with the following functions.

* __utils.spawn(func, ...)__

  This function schedules the function `func` to be run in a new coroutine.
  Any excess arguments will be given as arguments to `func`.

* __utils.yield()__

  This function suspends the currently running coroutine, but immediately
  schedules it to be run again. This will let any other coroutines scheduled
  to run get their turn before this coroutine continues.

* __utils.exit([status])__

  The function will stop the main loop and exit the Lua Event Machine.
  The only difference between this function and `os.exit()` is that this
  function will let any garbage collection metafunctions run before the
  program exits.

  If `status` is supplied this will be the exit status of program, otherwise
  `EXIT_SUCCESS` is used.

* __utils.sleeper()__

  This function returns a new sleeper object.

* __sleeper:sleep([seconds])__

  This method suspends the current coroutine.
  If `seconds` is given the method will return `nil, 'timeout'` after
  that many seconds.

  If `seconds` is zero or negative this method will behave as `utils.yield()`
  except it will still return `nil, 'timeout'`.

  If another coroutine is already sleeping on this object the method will
  return `nil, 'busy'`.

  The timeout should have at least milliseconds resolution, but since
  other coroutines could be running, and even more coroutines scheduled
  for running when the timeout occurs, no guarantees can be made as to
  exactly how long time the coroutine will be suspended.

* __sleeper:wakeup(...)__

  This method wakes up any coroutine sleeping on the sleeper object.

  Any arguments given to this method will be returned by the `sleeper:sleep()`
  method called by the sleeping coroutine.

  Returns `true` on succes and `nil, 'not sleeping'` if no coroutine is
  currently sleeping on the object.

* __utils.timer(seconds, func)__

  This method will schedule the function `func` to be run in a new coroutine
  after `seconds` seconds and return a new timer object.

  If `seconds` is zero or negative this method shall behave as `utils.spawn()`
  except it will still return a timer object.

* __timer:cancel()__

  This method cancels the timer.
  Returns `true` on success and `nil, 'expired'` if the coroutine has already
  been scheduled to run.


License
-------

The Lua Event Machine is free software. It is distributed under the terms
of the [GNU General Public License][gpl].

[gpl]: http://www.fsf.org/licensing/licenses/gpl.html


Contact
-------

Please send bug reports, patches, feature requests, praise and general gossip
to me, Emil Renner Berthing <esmil@mailme.dk>.
