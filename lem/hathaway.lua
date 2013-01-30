--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011-2013 Emil Renner Berthing
-- Copyright 2012 Asbjørn Sloth Tønnesen
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

local format = string.format

local httpserv = require 'lem.http.server'

local M = {}

function M.debug() end

do
	local lookup = {}
	M.lookup = lookup

	function M.GET(path, handler)
		local entry = lookup[path]
		if entry then
			entry['HEAD'] = handler
			entry['GET'] = handler
		else
			entry = {
				['HEAD'] = handler,
				['GET'] = handler,
			}
			lookup[path] = entry
		end
	end

	do
		local function static_setter(method)
			return function(path, handler)
				local entry = lookup[path]
				if entry then
					entry[method] = handler
				else
					lookup[path] = { [method] = handler }
				end
			end
		end

		M.POST    = static_setter('POST')
		M.PUT     = static_setter('PUT')
		M.DELETE  = static_setter('DELETE')
		M.OPTIONS = static_setter('OPTIONS')
	end

	function M.GETM(pattern, handler)
		local i = 1
		while true do
			local entry = lookup[i]
			if entry == nil then
				lookup[i] = { pattern,
					['GET'] = handler,
					['HEAD'] = handler
				}
				break
			end
			if entry[1] == pattern then
				entry['GET'] = handler
				entry['HEAD'] = handler
				break
			end
			i = i + 1
		end
	end

	do
		local function match_setter(method)
			return function(pattern, handler)
				local i = 1
				while true do
					local entry = lookup[i]
					if entry == nil then
						lookup[i] = { pattern, [method] = handler }
						break
					end
					if entry[1] == pattern then
						entry[method] = handler
						break
					end
					i = i + 1
				end
			end
		end

		M.POSTM    = match_setter('POST')
		M.PUTM     = match_setter('PUT')
		M.DELETEM  = match_setter('DELETE')
		M.OPTIONSM = match_setter('OPTIONS')
	end

	local function check_match(entry, req, res, ok, ...)
		if not ok then return false end
		local handler = entry[req.method]
		if handler then
			handler(req, res, ok, ...)
		else
			httpserv.method_not_allowed(req, res)
		end
		return true
	end

	local function handler(req, res)
		local method, path = req.method, req.path
		M.debug('info', format("%s %s HTTP/%s", method, req.uri, req.version))
		local entry = lookup[path]
		if entry then
			local handler = entry[method]
			if handler then
				handler(req, res)
			else
				httpserv.method_not_allowed(req, res)
			end
		else
			local i = 0
			repeat
				i = i + 1
				local entry = lookup[i]
				if not entry then
					httpserv.not_found(req, res)
					break
				end
			until check_match(entry, req, res, path:match(entry[1]))
		end
	end

	function M.Hathaway(host, port)
		local server, err
		if port then
			server, err = httpserv.new(host, port, handler)
		else
			server, err = httpserv.new(host, handler)
		end
		if not server then M.debug('new', err) return nil, err end

		M.server = server
		server.debug = M.debug

		local ok, err = server:run()
		if not ok and err ~= 'interrupted' then
			M.debug('run', err)
			return nil, err
		end
		return true
	end
end

function M.import(env)
	if not env then
		env = _G
	end

	env.GET      = M.GET
	env.POST     = M.POST
	env.PUT      = M.PUT
	env.DELETE   = M.DELETE
	env.OPTIONS  = M.OPTIONS
	env.GETM     = M.GETM
	env.POSTM    = M.POSTM
	env.PUTM     = M.PUTM
	env.DELETEM  = M.DELETEM
	env.OPTIONSM = M.OPTIONSM
	env.Hathaway = M.Hathaway
end

return M

-- vim: ts=2 sw=2 noet:
