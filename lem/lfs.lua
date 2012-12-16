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

local lfs = require 'lem.lfs.core'

function lfs.lock(file, ...)
	return file:lock(...)
end

function lfs.unlock(file, ...)
	return file:lock('u', ...)
end

function lfs.setmode()
	return 'binary'
end

do
	local setmetatable, remove, link = setmetatable, lfs.remove, lfs.link

	local Lock = { __index = true, free = true }
	Lock.__index = Lock

	function Lock:free()
		return remove(self.filename)
	end

	function lfs.lock_dir(path)
		local filename = path .. '/lockfile.lfs'
		local ok, err = link('lock', filename, true)
		if not ok then return ok, err end

		return setmetatable({ filename = filename }, Lock)
	end
end

return lfs

-- vim: ts=2 sw=2 noet:
