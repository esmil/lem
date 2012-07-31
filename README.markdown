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


The Stream Library
------------------

Import the module using something like

    local streams = require 'lem.streams'

This sets `streams` to a table with the following functions.

* __streams.open(path, [mode])__

  Opens a given `path` and returns a new stream object.
  This should be a path to a device, named pipe or named socket.
  The function does work on regular files, but non-blocking IO doesn't,
  so you might as well use `io.open()`.
  This is a property of POSIX, sorry.

  The `mode` argument supports the same strings as `io.open()`.
  If `mode` is not specified it defaults to `"r"`.

* __streams.popen(cmd, [mode])__

  This function works just like `io.popen()` except it returns a
  stream object (which can be read or written to with blocking the
  main loop).

* __streams.tcp_connect(address, port)__

  This function connects to the specified address and port
  over TCP and if successful returns a new stream object.

* __streams.tcp_listen(address, port)__

  This function creates a new server object which can be used to receive
  incoming TCP connections on the specified address and port.

* __streams.sendfile(path)__

  This function opens a file and returns a new object to be used by the
  `streams:sendfile()` method. The path should point to a regular file or
  `streams:sendfile()` will fail.

The above functions return either a new stream object, a new server object,
a new sendfile object or `nil` and an error message indicating what went wrong.
The metatable of those objects can be found under __streams.Stream__,
__streams.Server__ and __streams.SendFile__ respectively.

The following methods are available on streams.

* __stream:closed()__

  Returns `true` when the stream is closed, `false` otherwise.

* __stream:busy()__

  Returns `true` when another coroutine is waiting for IO on this stream,
  `false` otherwise.

* __stream:close()__

  Closes the stream. If the stream is busy, this also interrupts the IO
  action on the stream.

  Returns `true` on succes or otherwise `nil` followed by an error message.
  If the stream is already closed the error message will be `'already closed'`.

* __stream:interrupt()__

  Interrupt any coroutine waiting for IO on the stream.

  Returns `true` on success and `nil, 'not busy'` if no coroutine is waiting
  for connections on the server object.

* __stream:read([mode])__

  Read data from the stream. The `mode` argument can be one of the following:

    - a number: read the given number of bytes from the stream
    - "\*a": read all data from stream until the stream is closed
    - "\*l": read a line (read up to and including the next '\n' character)

  If there is not enough data immediately available the current coroutine will
  be suspended until there is.

  However if the method is called without the mode argument, it will return
  what is immediately available on the stream (up to a certain size limit).
  Only if there is no data immediately available will the current coroutine
  be suspended until there is.

  On success this method will return the data read from stream in a Lua string.
  Otherwise it will return `nil` followed by an error message.
  If another coroutine is waiting for IO on the stream the error message
  will be `'busy'`.
  If the stream was interrupted (eg. by another coroutine calling
  `stream:interrupt()`, or `stream:close()`) the error message will be
  `'interrupted'`.
  If the stream is closed either before calling the method or closed
  from the other end during the read the error message will be `'closed'`.

* __stream:write(data)__

  Write the given data, which must be a Lua string, to the stream.
  If the data cannot be immediately written to the stream the current
  coroutine will be suspended until all data is written.

  Returns `true` on success or otherwise `nil` followed by an error message.
  If another coroutine is waiting for IO on the stream the error message
  will be `'busy'`.
  If the stream was interrupted (eg. by another coroutine calling
  `stream:interrupt()`, or `stream:close()`) the error message will be
  `'interrupted'`.
  If the stream is closed either before calling the method or closed
  from the other end during the write the error message will be `'closed'`.

* __stream:sendfile(file, [offset])__

  Write the given file to the stream. This is more effektive than reading
  from a file and writing to the socket since the data doesn't have to go
  through userspace. It only works on socket streams though.

  The file must be a sendfile object as returned by `streams.sendfile()`.

  If the offset argument is given the transfer will begin at the given
  offset into the file.

  Returns `true` on success or otherwise `nil` followed by an error message.
  If another coroutine is waiting for IO on the stream the error message
  will be `'busy'`.
  If the stream was interrupted (eg. by another coroutine calling
  `stream:interrupt()`, or `stream:close()`) the error message will be
  `'interrupted'`.
  If the stream is closed either before calling the method or closed
  from the other end during the write the error message will be `'closed'`.
  If the file is closed the error message will be `'file closed'`.

* __stream:cork()__

  Sets the `TCP_CORK` attribute on the stream. This will of course fail
  on streams which are not TCP connections.

  Returns `true` on success or otherwise `nil` followed by an error message.
  If another coroutine is waiting for IO on the stream the error message
  will be `'busy'`.
  If the stream is closed the error message will be `'closed'`.

* __stream:uncork()__

  Removes the `TCP_CORK` attribute on the stream. This will of course fail
  on streams which are not TCP connections.

  Returns `true` on success or otherwise `nil` followed by an error message.
  If another coroutine is waiting for IO on the stream the error message
  will be `'busy'`.
  If the stream is closed the error message will be `'closed'`.


The following methods are available on server objects.

* __server:closed()__

  Returns `true` if the server is closed, `false` otherwise.

* __server:busy()__

  Returns `true` if another coroutine is listening on the server object,
  `false` otherwise.

* __server:close()__

  Closes the server object. If another coroutine is waiting for connections
  on the object it will be interrupted.

  Returns `true` on succes or otherwise `nil` followed by an error message.
  If the server is already closed the error message will be `'already closed'`.

* __server:interrupt()__

  Interrupt any coroutine waiting for new connections on the server object.

  Returns `true` on success and `nil, 'not busy'` if no coroutine is waiting
  for connections on the server object.

* __server:accept()__

  This method will get a stream object of a new incoming connection.
  If there are no incoming connections immediately available,
  the current coroutine will be suspended until there is.

  Returns a new stream object on succes or otherwise `nil` followed by an
  error message.
  If another coroutine is already waiting for new connections on the server
  object the error message will be `'busy'`.
  If the server was interrupted (eg. by another coroutine calling
  `server:interrupt()`, or `server:close()`) the error message will be
  `'interrupted'`.
  If the server object is closed the error message will be `'closed'`.

* __server:autospawn(handler)__

  This method will suspend the currently running coroutine while
  listening for new connections to the server object.
  When a new client connects it will automatically spawn a new
  coroutine running the `handler` function with a stream object
  for the new connection as first argument.

  If an error occurs the method will return `nil` followed by an error message.
  If another coroutine is already waiting for new connections on the server
  object the error message will be `'busy'`.
  If the server was interrupted (eg. by another coroutine calling
  `server:interrupt()`, or `server:close()`) the error message will be
  `'interrupted'`.
  If the server object is closed the error message will be `'closed'`.

The following methods are available on sendfile objects.

* __file:close()__

  Closes the gives file.

  Returns `true` on success or otherwise `nil` followed by an error message.

* __file:size()__

  Returns the size of the file.


License
-------

The Lua Event Machine is free software. It is distributed under the terms
of the [GNU General Public License][gpl].

[gpl]: http://www.fsf.org/licensing/licenses/gpl.html


Contact
-------

Please send bug reports, patches, feature requests, praise and general gossip
to me, Emil Renner Berthing <esmil@mailme.dk>.
