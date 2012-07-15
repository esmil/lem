--
-- This file is part of lem-streams.
-- Copyright 2011 Emil Renner Berthing
--
-- lem-streams is free software: you can redistribute it and/or
-- modify it under the terms of the GNU General Public License as
-- published by the Free Software Foundation, either version 3 of
-- the License, or (at your option) any later version.
--
-- lem-streams is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with lem-streams.  If not, see <http://www.gnu.org/licenses/>.
--

local utils = require 'lem.utils'

local setmetatable = setmetatable
local thisthread, suspend, resume
	= utils.thisthread, utils.suspend, utils.resume

local QOStream = {}
QOStream.__index = QOStream

function QOStream:closed(...)
	return self.stream:closed(...)
end

function QOStream:interrupt(...)
	return self.stream:interrupt(...)
end

function QOStream:close(...)
	return self.stream:close(...)
end

function QOStream:write(...)
	local nxt = self.next
	if nxt == 0 then
		nxt = 1
		self.next = 1
	else
		local me = nxt

		self[me] = thisthread()
		nxt = #self+1
		self.next = nxt
		suspend()
		self[me] = nil
	end

	local ok, err = self.stream:write(...)

	nxt = self[nxt]
	if nxt then
		resume(nxt)
	else
		self.next = 0
	end

	if not ok then return nil, err end
	return ok
end

local function wrap(stream, ...)
	if not stream then return stream, ... end
	return setmetatable({ stream = stream, next = 0 }, QOStream)
end

return {
	QOStream = QOStream,
	wrap = wrap,
}

-- vim: set ts=2 sw=2 noet:
