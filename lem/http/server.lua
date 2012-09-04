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

local setmetatable = setmetatable
local tostring = tostring
local tonumber = tonumber
local pairs = pairs
local type = type
local date = os.date
local format = string.format
local concat = table.concat
local remove = table.remove

local io = require 'lem.io'
require 'lem.http'

local M = {}

local status_string = {
	[100] = '100 Continue',
	[101] = '101 Switching Protocols',
	[102] = '102 Processing',                     -- WebDAV

	[200] = '200 OK',
	[201] = '201 Created',
	[202] = '202 Accepted',
	[203] = '203 Non-Authoritative Information',
	[204] = '204 No Content',
	[205] = '205 Reset Content',
	[206] = '206 Partial Content',
	[207] = '207 Multi-Status',                   -- WebDAV

	[300] = '300 Multiple Choices',
	[301] = '301 Moved Permanently',
	[302] = '302 Found',
	[303] = '303 See Other',
	[304] = '304 Not Modified',
	[305] = '305 Use Proxy',
	[306] = '306 Switch Proxy',
	[307] = '307 Temporary Redirect',

	[400] = '400 Bad Request',
	[401] = '401 Unauthorized',
	[402] = '402 Payment Required',
	[403] = '403 Forbidden',
	[404] = '404 Not Found',
	[405] = '405 Method Not Allowed',
	-- ...
	[417] = '417 Expectation Failed',

	[500] = '500 Internal Server Error',
	[501] = '501 Not Implemented',
	-- ...
	[505] = '505 HTTP Version Not Supported',
	-- ...
}
M.status_string = status_string

function M.not_found(req, res)
	if req.headers['Expect'] ~= '100-continue' then
		req:body()
	end

	res.status = 404
	res.headers['Content-Type'] = 'text/html; charset=UTF-8'
	res:add([[
<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
<title>Not Found</title>
</head>
<body>
<h1>Not found</h1>
</body>
</html>
]])
end

do
	local function htmlerror(num, text)
		local str = format([[
<?xml version="1.0" encoding="UTF-8"?>
<html xmlns="http://www.w3.org/1999/xhtml" xml:lang="en">
<head>
<title>%s</title>
</head>
<body>
<h1>%s</h1>
</body>
</html>
]], text, text)
		return function(req, res)
			res.status = num
			res.headers['Content-Type'] = 'text/html; charset=UTF-8'
			res.headers['Connection'] = 'close'
			res:add(str)
		end
	end

	M.method_not_allowed = htmlerror(405, 'Method Not Allowed')
	M.expectation_failed = htmlerror(417, 'Expectation Failed')
	M.version_not_supported = htmlerror(505, 'HTTP Version Not Supported')
end

function M.debug() end

do
	local gsub, char, tonumber = string.gsub, string.char, tonumber

	local function tochar(str)
		return char(tonumber(str, 16))
	end

	function M.urldecode(str)
		return gsub(gsub(str, '+', ' '), '%%(%x%x)', tochar)
	end
end

do
	local Response = {}
	Response.__index = Response
	M.Response = Response

	function new_response(req)
		local n = 0
		return setmetatable({
			headers = {},
			status  = 200,
			version = req.version,
			add     = function(self, ...)
				n = n + 1
				self[n] = format(...)
			end
		}, Response)
	end

	local function check_match(entry, req, res, ok, ...)
		if not ok then return false end
		local handler = entry[req.method]
		if handler then
			handler(req, res, ok, ...)
		else
			M.method_not_allowed(req, res)
		end
		return true
	end

	local urldecode = M.urldecode

	local function handleHTTP(self, client)
		repeat
			local req, err = client:read('HTTPRequest')
			if not req then self.debug('read', err) break end
			local method, uri, version = req.method, req.uri, req.version

			req.path = urldecode(uri:match('^([^?]*)'))

			local res = new_response(req)

			if version ~= '1.0' and version ~= '1.1' then
				M.version_not_supported(req, res)
				version = '1.1'
			else
				local expect = req.headers['Expect']
				if expect and expect ~= '100-continue' then
					M.expectation_failed(req, res)
				else
					self.handler(req, res)
				end
			end

			local headers = res.headers
			local file, close = res.file, false
			if type(file) == 'string' then
				file, err = io.open(file)
				if file then
					close = true
				else
					self.debug('open', err)
					res = new_response(req)
					headers = res.headers
					M.not_found(req, res)
				end
			end

			if res.status == 200 and #res == 0 and res.file == nil then
				res.status = 204
			elseif headers['Content-Length'] == nil then
				local len
				if file then
					len = file:size()
				else
					len = 0
					for i = 1, #res do
						len = len + #res[i]
					end
				end

				headers['Content-Length'] = len
			end

			if headers['Date'] == nil then
				headers['Date'] = date('!%a, %d %b %Y %T GMT')
			end

			if headers['Server'] == nil then
				headers['Server'] = 'Hathaway/0.1 LEM/0.3'
			end

			local robe, i = {}, 1
			do
				local status = res.status
				if type(status) == 'number' then
					status = status_string[status]
				end

				robe[1] = format('HTTP/%s %s\r\n', version, status)
			end

			for k, v in pairs(headers) do
				i = i + 1
				robe[i] = format('%s: %s\r\n', k, tostring(v))
			end

			i = i + 1
			robe[i] = '\r\n'

			local ok, err = client:cork()
			if not ok then self.debug('cork', err) break end

			local ok, err = client:write(concat(robe))
			if not ok then self.debug('write', err) break end

			if method ~= 'HEAD' then
				if file then
					ok, err = client:sendfile(file, headers['Content-Length'])
					if close then file:close() end
				else
					ok, err = client:write(concat(res))
				end
				if not ok then self.debug('write', err) break end
			end

			local ok, err = client:uncork()
			if not ok then self.debug('uncork', err) break end

		until version == '1.0'
		   or req.headers['Connection'] == 'close'
		   or headers['Connection'] == 'close'

		client:close()
	end

	local Server = {}
	Server.__index = Server
	M.Server = Server

	function Server:run(handler)
		return self.socket:autospawn(function(...) return handleHTTP(self, ...) end)
	end

	local type, setmetatable = type, setmetatable

	function M.new(host, port, handler)
		local socket, err
		if type(host) == 'string' then
			socket, err = io.tcp.listen(host, port)
			if not socket then
				return nil, err
			end
		else
			socket = host
			handler = port
		end

		return setmetatable({
			socket = socket,
			handler = handler,
			debug = M.debug
		}, Server)
	end
end

return M

-- vim: ts=2 sw=2 noet:
