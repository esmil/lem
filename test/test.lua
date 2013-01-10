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

local function sleep(n)
  utils.newsleeper():sleep(n)
end

local function timer(n, f, ...)
   utils.spawn(function(...)
      sleep(n)
      return f(...)
   end, ...)
end

--print('package.cpath = ', package.cpath)

print 'Saying booh in 2.5 seconds'

timer(2.5, function() print 'Booh!' end)

print 'Sleeping 5 seconds'

sleep(5)

print 'Done sleeping'

print 'Spawning new thread..'
utils.spawn(function(s) print(s) end, "I'm the new thread!")

print 'Yielding..'
utils.yield()

print 'Back.'

print 'Bye!'

-- vim: syntax=lua ts=3 sw=3 et:
