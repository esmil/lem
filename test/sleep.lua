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

local sum, coros, n = 0, 0, 0
local done = utils.newsleeper()

local function test(t)
	local sleeper = utils.newsleeper()
	local diff = utils.now()
	sleeper:sleep(t)
	diff = utils.now() - diff

	print(string.format('%fs, %fms off', diff, 1000*(diff - t)))
	sum = sum + math.abs(diff - t)
	n = n + 1
	if n == coros then done:wakeup() end
end

for t = 0, 3, 0.1 do
	coros = coros + 1
	utils.spawn(test, t)
end

done:sleep()

print(string.format('%fms off on average', 1000*sum / n))

-- vim: syntax=lua ts=2 sw=2 noet:
