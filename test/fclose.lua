#!bin/lem
--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2013 Ico Doornekamp
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

local done = false

utils.spawn(function()
	local s, now, format = utils.newsleeper(), utils.now, string.format
	local t1, t2 = now(), 0
	repeat
		s:sleep(0.1)
		t2 = now()
		print(format("Tick after %uus", (t2 - t1)*1000000))
		t1 = t2
	until done
end)

local file = assert(io.open('file.txt', 'w'))
local b = string.rep("a", 1024*1024)
for i = 1, 150 do
	file:write(b)
end
print("Closing file")
file:close()
print("Write done")

utils.newsleeper():sleep(1)
done = true

-- vim: set ts=2 sw=2 noet:
