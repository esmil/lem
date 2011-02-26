--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011 Emil Renner Berthing
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

local utils   = require 'lem.utils'

local loadstring = loadstring
local format = string.format
local concat = table.concat
local tostring = tostring
local select = select

local function repl(done, name, ins, outs)
	if not outs then outs = ins end

	local getcode, onreturn, onerror

	name = '=' .. name

	function getcode()
		local res, err = outs:write('> ')
		if not res then return done(nil, err) end

		local line
		line, err = ins:read('*l')
		if not line then return done(nil, err) end

		line = line:gsub('^=', 'return ')

		while true do
			res, err = loadstring(line, name)
			if res then break end

			if not err:match("'<eof>'") then
				return onerror(err)
			end

			res, err = outs:write('>> ')
			if not res then return done(nil, err) end

			res, err = ins:read('*l')
			if not res then return done(nil, err) end

			line = line .. res
		end

		utils.sethandler(onerror)
		return onreturn(res())
	end

	function onreturn(...)
		utils.sethandler()
		local args = select('#', ...)
		if args == 0 then return getcode() end

		local rstr
		do
			local t, ti = { ... }, nil
			for i = 1, args - 1 do
				t[i] = tostring(t[i])
			end
			t[args] = tostring(t[args])..'\n'

			rstr = concat(t, '\t')
		end

		local ok, err = outs:write(rstr)
		if not ok then return done(nil, err) end

		return getcode()
	end

	function onerror(err)
		local ok, err = outs:write(format("%s\n", err))
		if not ok then return done(nil, err) end

		return getcode()
	end

	return getcode()
end

return {
	wait = function(name, ins, outs)
		local sleeper = utils.sleeper()
		local function done(...)
			return sleeper:wakeup(...)
		end

		utils.spawn(repl, done, name, ins, outs)

		return sleeper:sleep()
	end,

	go = function(name, ins, outs)
		local function done()
			return outs:write('\n')
		end
		return repl(done, name, ins, outs)
	end,
}

-- vim: syntax=lua ts=2 sw=2 noet:
