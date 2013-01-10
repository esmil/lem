--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011-2013 Emil Renner Berthing
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

local utils  = require 'lem.utils'
local io     = require 'lem.io.core'

local type   = type
local assert = assert
local error  = error

do
	local parsers = require 'lem.parsers'

	io.Stream.read = parsers.newreader(io.Stream.readp)
	io.File.read   = parsers.newreader(io.File.readp)
end

do
	local stdin = io.stdin

	function io.input(file)
		if not file then return stdin end
		if type(file) == 'string' then
			stdin = assert(io.open(file))
		else
			stdin = file
		end
	end

	function io.read(...)
		return stdin:read(...)
	end
end

do
	local stdout = io.stdout

	function io.output(file)
		if not file then return stdout end
		if type(file) == 'string' then
			stdout = assert(io.open(file))
		else
			stdout = file
		end
	end

	function io.write(...)
		return stdout:write(...)
	end

	function io.close(file)
		if not file then file = stdout end
		return file:close()
	end
end

do
	local MultiServer = {}
	MultiServer.__index = MultiServer
	io.MultiServer = MultiServer

	function MultiServer:interrupt()
		return self[1]:interrupt()
	end

	function MultiServer:close()
		local rok, rerr = true
		for i = 1, #self do
			local ok, err = self[i]:close()
			if not ok then
				rok, rerr = ok, err
			end
		end
		return rok, rerr
	end


	local function autospawn(self, i, handler)
		local ok, err = self[i]:autospawn(handler)
		if self.running then
			self.running, self.ok, self.err = false, ok, err
		end
		for i = 1, #self do
			self[i]:interrupt()
		end
	end

	local spawn = utils.spawn

	function MultiServer:autospawn(handler)
		local n = #self

		self.running = true
		for i = 1, n-1 do
			spawn(autospawn, self, i, handler)
		end
		autospawn(self, n, handler)

		return self.ok, self.err
	end

	local setmetatable = setmetatable
	local listen4, listen6 = io.tcp.listen4, io.tcp.listen6

	function io.tcp.listen(host, port)
		if host:match(':') then
			return listen6(host, port)
		end

		local s6, err = listen6(host, port)
		if s6 then
			local s4 = listen4(host, port)
			if s4 then
				return setmetatable({ s6, s4 }, MultiServer)
			end
			return s6
		else
			return listen4(host, port)
		end
	end
end

return io

-- vim: ts=2 sw=2 noet:
