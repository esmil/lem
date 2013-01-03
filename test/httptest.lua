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
local http  = require 'lem.http'

local write, format = io.write, string.format
local function printf(...)
	return write(format(...))
end

local domain, port = 'www.google.com', 'http'
--local domain, port = 'localhost', '8080'
local done = false

utils.spawn(function()
	local conn, name, port = assert(io.tcp.connect(domain, port))

	printf('\nConnected to %s:%u\n', name, port)

	for i = 1, 2 do
		--assert(conn:write('GET / HTTP/1.1\r\nHost: '..domain..'\r\nConnection: close\r\n\r\n'))
		assert(conn:write('GET / HTTP/1.1\r\nHost: '..domain..'\r\n\r\n'))

		local res = assert(conn:read('HTTPResponse'))

		printf('\nHTTP/%s %d %s\n', res.version, res.status, res.text)
		for k, v in pairs(res.headers) do
			printf('%s: %s\n', k, v)
		end

		local body = assert(res:body())
		printf('\n#body = %d\n', #body)
		--write(body, '\n')
	end

	done = true
end)

local yield = utils.yield
local sleeper = utils.newsleeper()
repeat
	write('.')
	--yield()
	sleeper:sleep(0.001)
until done

-- vim: set ts=2 sw=2 noet:
