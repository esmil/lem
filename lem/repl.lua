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

local load = load
if _VERSION == 'Lua 5.1' then
	load = loadstring
end

local format = string.format
local concat = table.concat
local tostring = tostring
local select = select

local function repl(name, ins, outs)
	name = '=' .. name

	local function onreturn(ok, ...)
		if not ok then
			local ok, err = outs:write(format("%s\n", select(1, ...)))
			if not ok then return nil, err end
			return true
		end

		local args = select('#', ...)
		if args == 0 then return true end

		local rstr
		do
			local t = { ... }
			for i = 1, args - 1 do
				t[i] = tostring(t[i])
			end
			t[args] = tostring(t[args])..'\n'

			rstr = concat(t, '\t')
		end

		local ok, err = outs:write(rstr)
		if not ok then return nil, err end

		return true
	end

	while true do
		local res, err = outs:write('> ')
		if not res then return nil, err end

		res, err = ins:read('*l')
		if not res then return nil, err end

		local line = res:gsub('^=', 'return ')

		while true do
			res, err = load(line, name)
			if res then
				res, err = onreturn(pcall(res))
				if not res then return nil, err end
				break
			end

			if not err:match("<eof>") then
				res, err = outs:write(format("%s\n", err))
				if not res then return nil, err end
				break
			end

			res, err = outs:write('>> ')
			if not res then return nil, err end

			res, err = ins:read('*l')
			if not res then return nil, err end

			line = line .. res
		end
	end
end

-- if not run directly just return the module table
if not arg or arg[0] then
	return { repl = repl }
end

local io = require 'lem.io'

io.stdout:write([[
A Lua Event Machine 0.3  Copyright 2011-2013 Emil Renner Berthing
]])

local _, err = repl('stdin', io.stdin, io.stdout)
print(err or '')

-- vim: ts=2 sw=2 noet:
