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

local io = require 'lem.io.core'

local type   = type
local assert = assert
local error  = error

do
	local parsers = io.parsers
	local parser_available = parsers.available
	parsers.available = nil
	local parser_target = parsers.target
	parsers.target = nil

	function io.reader(readp)
		return function(self, fmt, ...)
			if fmt == nil then
				return readp(self, parser_available)
			end
			if type(fmt) == 'number' then
				return readp(self, parser_target, fmt)
			end
			local parser = parsers[fmt]
			if parser == nil then
				error('invalid format', 2)
			end
			return readp(self, parser, ...)
		end
	end

	io.Stream.read = io.reader(io.Stream.readp)
	io.File.read = io.reader(io.File.readp)
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

return io

-- vim: ts=2 sw=2 noet:
