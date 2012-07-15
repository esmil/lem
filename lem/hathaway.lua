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

local streams = require 'lem.streams'
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
	local lookup = {}
	M.lookup = lookup

	function M.GET(uri, handler)
		local path = lookup[uri]
		if path then
			path['HEAD'] = handler
			path['GET'] = handler
		else
			path = {
				['HEAD'] = handler,
				['GET'] = handler,
			}
			lookup[uri] = path
		end
	end

	do
		local function static_setter(method)
			return function(uri, handler)
				local path = lookup[uri]
				if path then
					path[method] = handler
				else
					lookup[uri] = { [method] = handler }
				end
			end
		end

		M.POST   = static_setter('POST')
		M.PUT    = static_setter('PUT')
		M.DELETE = static_setter('DELETE')
	end

	function M.GETM(pattern, handler)
		local i = 1
		while true do
			local entry = lookup[i]
			if entry == nil then
				lookup[i] = { pattern,
					['GET'] = handler,
					['HEAD'] = handler
				}
				break
			end
			if entry[1] == pattern then
				entry['GET'] = handler
				entry['HEAD'] = handler
				break
			end
			i = i + 1
		end
	end

	do
		local function match_setter(method)
			return function(pattern, handler)
				local i = 1
				while true do
					local entry = lookup[i]
					if entry == nil then
						lookup[i] = { pattern, [method] = handler }
						break
					end
					if entry[1] == pattern then
						entry[method] = handler
						break
					end
					i = i + 1
				end
			end
		end

		M.POSTM   = match_setter('POST')
		M.PUTM    = match_setter('PUT')
		M.DELETEM = match_setter('DELETE')
	end

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

	local function handler(istream, ostream)
		repeat
			local req, err = istream:read('HTTPRequest')
			if not req then M.debug(err) break end
			local method, uri, version = req.method, req.uri, req.version
			M.debug(format("%s %s HTTP/%s", method, uri, version))

			req.ostream = ostream
			local res = new_response(req)

			if version ~= '1.0' and version ~= '1.1' then
				M.version_not_supported(req, res)
				version = '1.1'
			else
				local expect = req.headers['Expect']
				if expect and expect ~= '100-continue' then
					M.expectation_failed(req, res)
				else
					local path = lookup[uri]
					if path then
						local handler = path[method]
						if handler then
							handler(req, res)
						else
							M.method_not_allowed(req, res)
						end
					else
						local i = 0
						repeat
							i = i + 1
							local entry = lookup[i]
							if not entry then
								M.not_found(req, res)
								break
							end
						until check_match(entry, req, res, uri:match(entry[1]))
					end
				end
			end

			local headers = res.headers
			local file, close = res.file, false
			if type(file) == 'string' then
				file, err = streams.sendfile(file)
				if file then
					close = true
				else
					M.debug(err)
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
				headers['Server'] = 'Hathaway/0.1 LEM/0.1'
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

			local ok, err = ostream:cork()
			if not ok then M.debug(err) break end

			local ok, err = ostream:write(concat(robe))
			if not ok then M.debug(err)	break end

			if method ~= 'HEAD' then
				if file then
					ok, err = ostream:sendfile(file)
					if close then file:close() end
				else
					ok, err = ostream:write(concat(res))
				end
				if not ok then M.debug(err)	break end
			end

			local ok, err = ostream:uncork()
			if not ok then M.debug(err) break end

		until version == '1.0'
		   or req.headers['Connection'] == 'close'
		   or headers['Connection'] == 'close'

		istream:close()
		ostream:close()
	end

	function M.Hathaway(address, port)
		local server, err = streams.tcp4_listen(address, port)
		if not server then M.debug(err) return nil, err end

		M.server = server

		local ok, err = server:autospawn(handler)
		if not ok and err ~= 'interrupted' then
			M.debug(err)
			return nil, err
		end
		return true
	end
end

function M.import(env)
	if not env then
		env = _G
	end

	env.GET      = M.GET
	env.POST     = M.POST
	env.PUT      = M.PUT
	env.DELETE   = M.DELETE
	env.GETM     = M.GETM
	env.POSTM    = M.POSTM
	env.PUTM     = M.PUTM
	env.DELETEM  = M.DELETEM
	env.Hathaway = M.Hathaway
end

return M

-- vim: ts=2 sw=2 noet:
