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
local io    = require 'lem.io'
local http  = require 'lem.http'

local format = string.format
local concat = table.concat

local done = false

utils.spawn(function()
	local conn = assert(io.tcp.connect('www.google.dk', 80))
	--local conn = assert(io.tcp.connect('127.0.0.1', 8080))

	print('\nConnected.')

	for i = 1, 2 do
		--assert(conn:write('GET / HTTP/1.1\r\nHost: www.google.dk\r\nConnection: close\r\n\r\n'))
		assert(conn:write('GET / HTTP/1.1\r\nHost: www.google.dk\r\n\r\n'))

		local res = assert(conn:read('HTTPResponse'))

		print(format('\nHTTP/%s %d %s', res.version, res.status, res.text))
		for k, v in pairs(res.headers) do
			print(format('%s: %s', k, v))
		end

		--print(format('\n#body = %d', #assert(conn:read('*a'))))
		print(format('\n#body = %d', #assert(res:body())))
	end

	done = true
end)

local write, yield = io.write, utils.yield
local sleeper = utils.newsleeper()
repeat
	write('.')
	--yield()
	sleeper:sleep(0.001)
until done

-- vim: set ts=2 sw=2 noet:
