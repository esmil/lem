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

print 'Entered test.lua'

local utils = require 'lem.utils'

local function sleep(n)
   utils.newsleeper():sleep(n)
end

print 'Saying "Fee!" in 1 second'
utils.spawn(function()
   sleep(1)
   print 'Fee!'
end)

print 'Saying "Fo!" in 3 seconds'
utils.spawn(function()
   sleep(3)
   print 'Fo!'
end)

print 'Sleeping for 2 seconds, then saying  "Fi!" before the script ends'
sleep(2)
print 'Fi!'

-- vim: syntax=lua ts=3 sw=3 et:
