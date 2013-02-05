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

local utils  = require 'lem.utils'
local io     = require 'lem.io'
local client = require 'lem.http.client'

local write, format = io.write, string.format
local function printf(...)
	return write(format(...))
end

local url      = arg[1] or 'http://ompldr.org/vODFnOA/Birgit%20Lystager%20-%20Birger.mp3'
local filename = arg[2] or 'birger.mp3'
local stop     = false

utils.spawn(function()
	local c = client.new()

	assert(c:download(url, filename))
	assert(c:close())
	stop = true
end)

local sleeper = utils.newsleeper()
repeat
	write('.')
	sleeper:sleep(0.001)
until stop

-- vim: set ts=2 sw=2 noet:
