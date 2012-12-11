#!bin/lem
--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011-2012 Emil Renner Berthing
--
-- LEM is free software: you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation, either version 3 of
-- the License, or (at your option) any later version.
--
-- LEM is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with LEM.  If not, see <http://www.gnu.org/licenses/>.
--

local utils = require 'lem.utils'
local io    = require 'lem.io'
local queue = require 'lem.io.queue'

local exit   = false
local ticker = utils.newsleeper()
local stdout = queue.wrap(io.stdout)

do
	local format = string.format
	function queue.Stream:printf(...)
		return self:write(format(...))
	end
end

-- this function just reads lines from a
-- stream and prints them to stdout
local function poll(stream, name)
	repeat
		local line, err = stream:read('*l')
		if not line then
			stdout:printf('%s: %s\n', name, err)
			break
		end

		stdout:printf('%s: %s\n', name, line)
	until line == 'quit'

	exit = true
	ticker:wakeup()
end

-- type 'mkfifo pipe' to create a named pipe for this script
-- and do 'cat > pipe' (in another terminal) to write to it
local pipe = assert(io.open(arg[1] or 'pipe', 'r'))

-- spawn coroutines to read from stdin and the pipe
utils.spawn(poll, io.stdin, 'stdin')
utils.spawn(poll, pipe, 'pipe')

do
	--local out = io.stderr
	local out = stdout
	local sound

	repeat
		if sound == 'tick\n' then
			sound = 'tock\n'
		else
			sound = 'tick\n'
		end
		out:write(sound)
		ticker:sleep(1.0)
	until exit
end

io.stdin:close()
pipe:close()

-- vim: syntax=lua ts=2 sw=2 noet:
