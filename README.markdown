A Lua Event Machine
===================


About
-----
The Lua Event Machine is an attempt to make multitasking easier.
It makes heavy use of Lua coroutines so code that does I/O
can be suspended until data is ready. This allows you write code
as if you're using blocking I/O, while still allowing code in other
coroutines to run when you'd otherwise wait for I/O.

Hovewer unlike traditional multithreading there is no need for locks since
only one coroutine is running at a given time and you know exactly
when control might switch to another coroutine. That is when you're
doing I/O.

Here is a minimal chat server:
```lua
local io = require 'lem.io'
local queue = require 'lem.io.queue'

local socket = assert(io.tcp.listen('*', '5555'))
local clients = {}

socket:autospawn(function(client)
  client:write('What is your name?\r\n')
  local name = client:read('*l')
  if not name then
    client:close()
    return
  end
  name = name:match('[^\r]*')

  local self = queue.wrap(client)
  clients[self] = true

  while true do
    local line = client:read('*l')
    if not line then break end

    for c, _ in pairs(clients) do
      if c ~= self then
        c:write(string.format('%s : %s\r\n', name, line))
      end
    end
  end

  clients[self] = nil
  client:close()
end)
```
Use `telnet <your ip> 5555` to connect to it.


How It Works
------------
LEM is basically a [Lua][] interpreter with a built-in
[libev][] main loop.

All Lua code is run in coroutines so that modules can suspend the currently
running code, register callbacks with the event loop and wait for events
to happen before resuming the coroutine.

This allows libraries to be written such that calls appear to be blocking,
while still allowing other Lua coroutines to run. It also allows you to write
libraries which automatically spawn new coroutines to handle incoming events.

For this to work properly LEM modules needs to use non-blocking I/O. However,
not all I/O can easily be done non-blocking. Filesystem operations is one example.
Therefore LEM also includes a thread pool and an API to easily push work to a
separate OS thread and receive an event when it's done.

[Lua]: http://www.lua.org/
[libev]: http://libev.schmorp.de/


Getting Started
---------------
Check out the sources

    $ git clone git://github.com/esmil/lem.git
    $ cd lem

and do

    $ ./configure --prefix=<your prefix>
    $ make

Now you can try out some of the test scripts.

    $ test/sleep.lua
    $ test/lfs.lua

If you're happy with the result run

    $ make install

to install the event machine to `<your prefix>`.

Both the Lua 5.2.1 and libev 4.11 sources are included so having Lua or
libev installed on your system is not required.
However if you already have a Lua 5.1 or 5.2 library installed the configure
script should find it and link against it.
Use `./configure --with-lua=builtin` or `./configure --with-lua=<pkg-config name>`
to use a specific version of Lua.
Eg. to build against the [LuaJIT][] library I do

    $ ./configure --with-lua=luajit

[LuaJIT]: http://luajit.org/luajit.html


Usage
-----
The `lem` interpreter will behave just like the normal standalone Lua
interpreter except it doesn't take any command line options yet.
All your normal Lua scripts should run with the LEM interpreter just fine. Type

    $ lem myscript.lua

to run `myscript.lua` or make the script executable and add a hash-bang
header as in

```lua
#!/usr/bin/env lem

local utils = require 'lem.utils'
local io    = require 'lem.io'

(etc.)
```

Just like the normal stand-alone interpreter LEM stores command line
arguments in the global table `arg` where `arg[-1]` is the interpreter,
`arg[0]` is the script name and normal arguments begin at `arg[1]`.

Running Lua scripts in the Lua Event Machine however, will allow you
to load the LEM modules, which will fail in the normal interpreter.


License
-------
The Lua Event Machine is free software. It is distributed under the terms
of the [GNU Lesser General Public License][lgpl].

[lgpl]: http://www.gnu.org/licenses/lgpl.html


Contact
-------
Please send bug reports, patches, feature requests, praise and general gossip
to me, Emil Renner Berthing <esmil@mailme.dk>.
