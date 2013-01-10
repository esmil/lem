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

local parsers = require 'lem.parsers.core'

local type   = type
local error  = error

local lookup = parsers.lookup
local available = lookup.available
lookup.available = nil
local target = lookup.target
lookup.target = nil

function parsers.newreader(readp)
	return function(self, fmt, ...)
		if fmt == nil then
			return readp(self, available)
		end
		if type(fmt) == 'number' then
			return readp(self, target, fmt)
		end
		local parser = lookup[fmt]
		if parser == nil then
			error('invalid format', 2)
		end
		return readp(self, parser, ...)
	end
end

return parsers

-- vim: ts=2 sw=2 noet:
