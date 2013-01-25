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
local io    = require 'lem.io'
local http  = require 'lem.http'

local write, format = io.write, string.format
local function printf(...)
	return write(format(...))
end

local domain, port = arg[1] or 'www.google.com', 'http'
local running = 0

local function get(n, close)
	running = running + 1

	local req
	if close then
		req = 'GET / HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n'
	else
		req = 'GET / HTTP/1.1\r\nHost: %s\r\n\r\n'
	end

	local conn = assert(io.tcp.connect(domain, port))
	assert(conn:write(req:format(domain)))
	local res = assert(conn:read('HTTPResponse'))

	printf('\n%d: HTTP/%s %d %s\n', n, res.version, res.status, res.text)
	for k, v in pairs(res.headers) do
		printf('%d: %s: %s\n', n, k, v)
	end

	local body = assert(res:body())
	printf('\n%d: #body = %d\n', n, #body)

	conn:close()
	running = running - 1
end

for i = 1, 2 do
	utils.spawn(get, i, (i % 2) == 0)
end

local sleeper = utils.newsleeper()
repeat
	write('.')
	sleeper:sleep(0.001)
until running == 0

-- vim: set ts=2 sw=2 noet:
