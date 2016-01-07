#!bin/lem
--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2013 Asbjørn Sloth Tønnesen
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

local signal = require 'lem.signal'
local utils = require 'lem.utils'
local io = require 'lem.io'

local sleeper = utils.newsleeper()

assert(signal.register('sigchld', function(signum, ev)
	if ev.type == 'exited' then
		print('child ' .. ev.rpid .. ' ' .. ev.type .. ' with status ' .. ev.status)
	end
end))

local cmd = 'sleep 0.5'
print('starting child: `'..cmd..'`');
io.popen(cmd)

sleeper:sleep(1)

local function handler(signum)
	print('got ' .. signal.lookup(signum))
end

print('catch sigint')
assert(signal.register('SIGINT', handler))

sleeper:sleep(5)

print('restore sigint')
assert(signal.unregister('SIGINT', handler))

sleeper:sleep(5)

print('catch sigint')
assert(signal.register('SIGINT', handler))

sleeper:sleep(5)
print('exiting')

-- vim: ts=2 sw=2 noet:
