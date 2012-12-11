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

local io   = require 'lem.io'
local http = require 'lem.http.core'

io.parsers['HTTPRequest'] = http.HTTPRequest
http.HTTPRequest = nil
io.parsers['HTTPResponse'] = http.HTTPResponse
http.HTTPResponse = nil

local tonumber = tonumber
local concat = table.concat

function http.Request:body()
	local len, body = self.headers['Content-Length'], ''
	if not len then return body end

	len = tonumber(len)
	if len <= 0 then return body end

	if self.headers['Expect'] == '100-continue' then
		local ok, err = self.ostream:send('HTTP/1.1 100 Continue\r\n\r\n')
		if not ok then return nil, err end
	end

	local err
	body, err = self.istream:read(len)
	if not body then return nil, err end

	return body
end

function http.Response:body_chunked()
	local istream = self.istream
	local t, n = {}, 0
	local line, err
	while true do
		line, err = istream:read('*l')
		if not line then return nil, err end

		local num = tonumber(line, 16)
		if not num then return nil, 'expectation failed' end
		if num == 0 then break end

		local data, err = istream:read(num)
		if not data then return nil, err end

		n = n + 1
		t[n] = data

		line, err = istream:read('*l')
		if not line then return nil, err end
	end

	line, err = istream:read('*l')
	if not line then return nil, err end

	return t
end

function http.Response:body()
	if self.headers['Transfer-Encoding'] == 'chunked' then
		return concat(self:body_chunked())
	end

	local num = self.headers['Content-Length']
	if not num then return nil, 'no content length specified' end

	num = tonumber(num)
	if not num then return nil, 'invalid content length' end

	return self.istream:read(num)
end

return http

-- vim: ts=2 sw=2 noet:
