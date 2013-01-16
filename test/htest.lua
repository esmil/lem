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

local utils    = require 'lem.utils'
local io       = require 'lem.io'
local hathaway = require 'lem.hathaway'
hathaway.import()

GET('/', function(req, res)
	print(req.client:getpeer())
	res.status = 302
	res.headers['Location'] = '/dump'
end)

GET('/hello', function(req, res)
	res.headers['Content-Type'] = 'text/plain'
	res:add('Hello, World!\n')
end)

GET('/self', function(req, res)
	res.headers['Content-Type'] = 'text/plain'
	res.file = arg[0]
end)

GET('/dump', function(req, res)
	local accept = req.headers['accept']
	if accept and accept:match('application/xhtml%+xml') then
		res.headers['Content-Type'] = 'application/xhtml+xml'
	else
		res.headers['Content-Type'] = 'text/html'
	end
	res:add([[
<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
  <title>Hathaway HTTP dump</title>
  <style type="text/css">
    th { text-align:left; }
  </style>
</head>
<body>

<h2>Request</h2>
<table>
  <tr><th>Method:</th><td>%s</td></tr>
  <tr><th>Uri:</th><td>%s</td></tr>
  <tr><th>Version:</th><td>%s</td></tr>
</table>

<h2>Headers</h2>
<table>
]], req.method or '', req.uri or '', req.version)

	for k, v in pairs(req.headers) do
		res:add('  <tr><th>%s</th><td>%s</td></tr>\n', k, v)
	end

	res:add([[
</table>

<h2>Body</h2>
<form action="/form" method="POST" accept-charset="UTF-8">
  <p>
    <textarea name="text" cols="80" rows="25"></textarea><br />
    <input type="submit" value="Submit" />
  </p>
</form>

<form action="/quit" method="post">
  <p>
    <input type="hidden" name="quit" value="secret" />
    <input type="submit" value="Quit" />
  </p>
</form>

</body>
</html>
]])
end)

local function urldecode(str)
	return str:gsub('+', ' '):gsub('%%(%x%x)', function (str)
		return string.char(tonumber(str, 16))
	end)
end

local function parseform(str)
	local t = {}
	for k, v in str:gmatch('([^&]+)=([^&]*)') do
		t[urldecode(k)] = urldecode(v)
	end
	return t
end

POST('/form', function(req, res)
	res.headers['Content-Type'] = 'text/plain'
	local body =req:body()
	res:add("You sent:\n%s\n", body)
	res:add('{\n')
	for k, v in pairs(parseform(body)) do
		res:add("  ['%s'] = '%s'\n", k, v)
	end
	res:add('}\n')
end)

GET('/close', function(req, res)
	res.headers['Content-Type'] = 'text/plain'
	res.headers['Connection'] = 'close'
	res:add('This connection should close\n')
end)

POST('/quit', function(req, res)
	local body = req:body()

	res.headers['Content-Type'] = 'text/plain'

	if body == 'quit=secret' then
		res:add("Bye o/\n")
		hathaway.server:close()
	else
		res:add("You didn't supply the right value...\n")
	end
end)

GETM('^/hello/([^/]+)$', function(req, res, name)
	res.headers['Content-Type'] = 'text/plain'
	res:add('Hello, %s!\n', name)
end)

hathaway.debug = print
if arg[1] == 'socket' then
	local sock = assert(io.unix.listen('socket', 666))
	Hathaway(sock)
else
	Hathaway('*', arg[1] or '8080')
end
utils.exit(0) -- otherwise open connections will keep us running

-- vim: syntax=lua ts=2 sw=2 noet:
