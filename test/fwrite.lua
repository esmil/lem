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

package.path = '?.lua'
package.cpath = '?.so'

local utils = require 'lem.utils'
local io    = require 'lem.io'

local write = io.write

print("Press enter to read '" .. (arg[1] or arg[0]) .. "'")
io.read()

local threads = 2
for i = 1, threads do
	utils.spawn(function(n)
		local file = assert(io.open('file' .. tostring(n) .. '.txt', 'w'))
		assert(getmetatable(file) == io.File, "Hmm...")

		print(tostring(n) .. '-1')
		assert(file:write('Hej', ' ', 'med', ' ', 'dig', '!\n'))
		print(tostring(n) .. '-2')
		assert(file:write(tostring(n)))
		print(tostring(n) .. '-3')
		assert(file:write('\n'))
		print(tostring(n) .. '-4')
		print(assert(file:seek("cur", -2)))
		print(tostring(n) .. '-5')
		assert(file:write('Dav!'))
		print(tostring(n) .. '-6')

		threads = threads - 1
	end, i)
end

local yield = utils.yield
while threads > 0 do
	write('.')
	yield()
end

print "\nDone. Press enter to continue."
io.read()

-- vim: set ts=2 sw=2 noet:
