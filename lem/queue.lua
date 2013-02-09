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

local utils = require 'lem.utils'

local M = {}

local Queue = {}
Queue.__index = Queue
M.Queue = Queue

local remove = table.remove
local thisthread, suspend, resume =
	utils.thisthread, utils.suspend, utils.resume

function Queue:put(v)
	local n = self.n + 1
	self.n = n

	if n >= 1 then
		self[n] = v
	else
		resume(remove(self, 1), v)
	end
end

function Queue:signal(...)
	local n = self.n
	if n < 0 then
		for i = 1, -n do
			resume(self[i], ...)
			self[i] = nil
		end
		self.n = 0
	end
end

function Queue:get()
	local n = self.n - 1
	self.n = n

	if n >= 0 then
		return remove(self, 1)
	end

	self[-n] = thisthread()
	return suspend()
end

function Queue:reset()
	self:signal(nil, 'reset')
	for i = self.n, 1, -1 do
		self[i] = nil
	end
	self.n = 0
end

local get = Queue.get
function Queue:consume()
	return get, self
end

function Queue:empty()
	return self.n <= 0
end

function M.new()
	return setmetatable({ n = 0 }, Queue)
end

return M

-- vim: ts=2 sw=2 noet:
