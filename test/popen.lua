#!bin/lem
--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2013 Emil Renner Berthing
--
-- LEM is free software: you can redistribute it and/or modify it
-- under the terms of the GNU Lesser General Public License as
-- published by the Free Software Foundation, either version 3 of
-- the License, or (at your option) any later version.
--
-- LEM is distributed in the hope that it will be useful, but
-- WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU Lesser General Public License for more details.
--
-- You should have received a copy of the GNU Lesser General Public
-- License along with LEM.  If not, see <http://www.gnu.org/licenses/>.
--

package.path = '?.lua'
package.cpath = '?.so'

local utils = require 'lem.utils'
local io    = require 'lem.io'

local write, format = io.write, string.format

local p = assert(io.popen('telnet localhost 8080', 'rw'))

assert(p:write('GET / HTTP/1.0'))
assert(p:write('\n'))
assert(p:write('\n'))

local n = 0
for line in p:lines() do
	n = n+1
	write(format('%4d: %s\n', n, line))
end

-- vim: syntax=lua ts=2 sw=2 noet:
