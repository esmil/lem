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

do
	local type = type
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
	local _write, stdout = io.Stream.write, io.stdout

	function io.write(str)
		return _write(stdout, str)
	end
end

return io

-- vim: ts=2 sw=2 noet:
