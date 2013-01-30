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

local format, write = string.format, io.write

local n = 0

if not arg[1] then
   io.stderr:write("I need a file..\n")
   utils.exit(1)
end

---[[
local file, err = io.streamfile(arg[1])
--local file, err = io.open(arg[1])

if not file then
	io.stderr:write(format("Error opening '%s': %s\n", arg[1], err))
	utils.exit(1)
end

for line in file:lines() do
   n = n+1
end
--[=[
--]]
for line in io.lines(arg[1]) do
	n = n+1
end
--]=]

write(format('%d lines\n', n))

-- vim: syntax=lua ts=2 sw=2 noet:
