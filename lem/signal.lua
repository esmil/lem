--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2013 Asbjørn Sloth Tønnesen
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

local core = require 'lem.signal.core'

local queues = {}

local function fire(signal, ...)
	local queue = queues[signal]
	for i = 1, #queue do
		queue[i](signal, ...)
	end
end

local install_signal_handler
do
	local installed = false
	function install_signal_handler()
		if not installed then
			core.sethandler(function(signal, ...)
				fire(signal, ...)
			end)
			installed = true
		end
	end
end

local signal_install, signal_uninstall
do
	local installed = {}

	function signal_install(sig)
		if installed[sig] then
			return true
		end

		install_signal_handler()

		local ret, err = core.watch(sig)
		if not ret then return nil, err end

		installed[sig] = true
		return true
	end

	function signal_uninstall(sig)
		if not installed[sig] then
			return true
		end

		local ret, err = core.unwatch(sig)
		if not ret then return nil, err end

		installed[sig] = nil
		return true
	end
end

local function lookup(signal)
	if type(signal) == 'string' then
		if string.sub(signal, 1, 3):upper() == 'SIG' then
			signal = string.sub(signal, 4)
		end
		return core.tonumber(signal:upper())
	else
		local ret = core.tostring(signal)
		if not ret then return nil end

		return 'SIG' .. ret
	end
end

local M = {}
M.lookup = lookup

function M.register(signal, func)
	assert(type(signal) == 'string', 'signal should be a string')
	local signum = lookup(signal)
	if not signum then return nil, 'unknown signal' end

	local queue = queues[signum]
	if queue == nil then
		queue = {}
		queues[signum] = queue
	end
	table.insert(queue, func)
	return signal_install(signum)
end

function M.unregister(signal, func)
	assert(type(signal) == 'string', 'signal should be a string')
	local signum = lookup(signal)
	if not signum then return nil, 'unknown signal' end

	local queue = queues[signum]
	if queue then
		for i = 1, #queue do
			if queue[i] == func then
				table.remove(queue, i)
			end
		end
		if #queue == 0 then
			return signal_uninstall(signum)
		end
	end
	return true
end

return M

-- vim: ts=2 sw=2 noet:
