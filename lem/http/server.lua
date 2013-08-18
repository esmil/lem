--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011-2013 Emil Renner Berthing
-- Copyright 2013      Halfdan Mouritzen
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

local setmetatable = setmetatable
local tostring = tostring
local tonumber = tonumber
local pairs = pairs
local type = type
local date = os.date
local format = string.format
local concat = table.concat
local remove = table.remove

local io       = require 'lem.io'
                 require 'lem.http'
local response = require 'lem.http.response'

local M = {}

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

local function check_match(entry, req, res, ok, ...)
	if not ok then return false end
	local handler = entry[req.method]
	if handler then
		handler(req, res, ok, ...)
	else
		response.method_not_allowed(req, res)
	end
	return true
end

do
	local urldecode = M.urldecode
	local newresponse = response.new

	local function handleHTTP(self, client)
		repeat
			local req, err = client:read('HTTPRequest')
			if not req then self.debug('read', err) break end
			local method, uri, version = req.method, req.uri, req.version

			req.path = urldecode(uri:match('^([^?]*)'))

			local res = newresponse(req)

			if version ~= '1.0' and version ~= '1.1' then
				response.version_not_supported(req, res)
				version = '1.1'
			else
				local expect = req.headers['expect']
				if expect and expect ~= '100-continue' then
					response.expectation_failed(req, res)
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
					res = rewresponse(req)
					headers = res.headers
					response.not_found(req, res)
				end
			end

			if not res.status then
				if #res == 0 and file == nil then
					res.status = 204
				else
					res.status = 200
				end
			end

			if headers['Content-Length'] == nil and res.status ~= 204 then
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

			if req.headers['connection'] == 'close' and headers['Connection'] == nil then
				headers['Connection'] = 'close'
			end

			local rope = {}
			do
				local status = res.status
				if type(status) == 'number' then
					status = response.status_string[status]
				end

				rope[1] = format('HTTP/%s %s\r\n', version, status)
			end

			res:appendheader(rope, 1)

			client:cork()

			local ok, err = client:write(concat(rope))
			if not ok then self.debug('write', err) break end

			if method ~= 'HEAD' then
				if file then
					ok, err = client:sendfile(file, headers['Content-Length'])
					if close then file:close() end
				else
					local body = concat(res)
					if #body > 0 then
						ok, err = client:write(body)
					end
				end
				if not ok then self.debug('write', err) break end
			end

			client:uncork()

		until version == '1.0'
		   or headers['Connection'] == 'close'

		client:close()
	end

	local Server = {}
	Server.__index = Server
	M.Server = Server

	function Server:run()
		return self.socket:autospawn(function(...) return handleHTTP(self, ...) end)
	end

	function Server:close()
		return self.socket:close()
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
