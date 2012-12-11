#!bin/lem
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

print('Entered ' .. arg[0])

local utils = require 'lem.utils'
local io    = require 'lem.io'

local conn = assert(io.tcp_connect('127.0.0.1', arg[1] or 8080))

for i = 1, 10 do
	assert(conn:write('ping\n'))

	local line, err = conn:read('*l')
	if not line then
		if err == 'closed' then
			print("Server closed connection")
			return
		end

		error(err)
	end

	print("Server answered: '" .. line .. "'")
end

conn:write('quit\n')

print('Exiting ' .. arg[0])

-- vim: syntax=lua ts=2 sw=2 noet:
