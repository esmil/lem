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

local io    = require 'lem.io'
require 'lem.http'

local client = {}

local Client = {}
Client.__index = Client
client.Client = Client

function client.new()
	return setmetatable({
		proto = false,
		domain = false,
		conn = false,
	}, Client)
end

local req_get = "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: keep-alive\r\n\r\n"
--local req_get = "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n"

local function close(self)
	local c = self.conn
	if c then
		self.conn = false
		return c:close()
	end
	return true
end
Client.close = close

local function fail(self, err)
	self.proto = false
	close(self)
	return nil, err
end

function Client:get(url)
	local proto, domain, uri = url:match('([a-zA-Z0-9]+)://([a-zA-Z0-9.]+)(/.*)')
	if not proto then
		error('Invalid URL', 2)
	end

	local c, err
	local req = req_get:format(uri, domain)
	local res
	if proto == self.proto and domain == self.domain then
		c = self.conn
		if c:write(req) then
			res = c:read('HTTPResponse')
		end
	end

	if not res then
		c = self.conn
		if c then
			c:close()
		end

		if proto == 'http' then
			c, err = io.tcp.connect(domain, '80')
		elseif proto == 'https' then
			local ssl = self.ssl
			if not ssl then
				error('No ssl context defined', 2)
			end
			c, err = ssl:connect(domain, '443')
		else
			error('Unknown protocol', 2)
		end
		if not c then return fail(self, err) end

		local ok
		ok, err = c:write(req)
		if not ok then return fail(self, err) end

		res, err = c:read('HTTPResponse')
		if not res then return fail(self, err) end
	end

	local body
	body, err = res:body()
	if not body then return fail(self, err) end

	self.proto = proto
	self.domain = domain
	self.conn = c
	res.body = body
	return res
end

function Client:download(url, filename)
	local res, err = self:get(url)
	if not res then return res, err end

	local file
	file, err = io.open(filename, 'w')
	if not file then return file, err end

	local ok
	ok, err = file:write(res.body)
	if not ok then return ok, err end
	ok, err = file:close()
	if not ok then return ok, err end

	return true
end

return client

-- vim: set ts=2 sw=2 noet:
