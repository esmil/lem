#!bin/lem
--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011-2012 Emil Renner Berthing
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
local queue = require 'lem.queue'

local consumers = 0
local function consumer(q, id)
	consumers = consumers + 1
	local sleeper = utils.newsleeper()
	for v in q:consume() do
		print(string.format('thread %d, n = %2d, received "%s"',
			id, q.n, tostring(v)))
		sleeper:sleep(0.04)
	end
	consumers = consumers - 1
end

local q, sleeper = queue.new(), utils.newsleeper()

print "One consumer:\n"
for i = 1, 5 do
	q:put(i)
end

utils.spawn(consumer, q, 1)
utils.yield()
assert(consumers == 1)

for i = 6, 10 do
	q:put(i)
	sleeper:sleep(0.1)
end

assert(q:empty())

assert(consumers == 1)
q:reset()
utils.yield()
assert(consumers == 0)

print "\nFive consumers:\n"

for i = 1, 10 do
	q:put(i)
end

for i = 1, 5 do
	utils.spawn(consumer, q, i)
end
utils.yield()
assert(consumers == 5)

for i = 11, 20 do
	q:put(i)
	sleeper:sleep(0.1)
end

assert(q:empty())
assert(consumers == 5)

q:signal(nil)
utils.yield()
assert(consumers == 0)

-- vim: syntax=lua ts=2 sw=2 noet:
