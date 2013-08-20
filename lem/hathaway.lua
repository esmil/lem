--
-- This file is part of LEM, a Lua Event Machine.
-- Copyright 2011-2013 Emil Renner Berthing
-- Copyright 2012-2013 Asbjørn Sloth Tønnesen
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
local httpresp = require 'lem.http.response'

local M = {}

function M.debug() end

local Hathaway = {}
Hathaway.__index = Hathaway
M.Hathaway = Hathaway

function Hathaway:GET(path, handler)
	local entry = self.lookup[path]
	if entry then
		entry['HEAD'] = handler
		entry['GET'] = handler
	else
		entry = {
			['HEAD'] = handler,
			['GET'] = handler,
		}
		self.lookup[path] = entry
	end
end

do
	local function static_setter(method)
		return function(self, path, handler)
			local entry = self.lookup[path]
			if entry then
				entry[method] = handler
			else
				self.lookup[path] = { [method] = handler }
			end
		end
	end

	Hathaway.POST    = static_setter('POST')
	Hathaway.PUT     = static_setter('PUT')
	Hathaway.DELETE  = static_setter('DELETE')
	Hathaway.OPTIONS = static_setter('OPTIONS')
end

function Hathaway:GETM(pattern, handler)
	local i = 1
	while true do
		local entry = self.lookup[i]
		if entry == nil then
			self.lookup[i] = { pattern,
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
		return function(self, pattern, handler)
			local i = 1
			while true do
				local entry = self.lookup[i]
				if entry == nil then
					self.lookup[i] = { pattern, [method] = handler }
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

	Hathaway.POSTM    = match_setter('POST')
	Hathaway.PUTM     = match_setter('PUT')
	Hathaway.DELETEM  = match_setter('DELETE')
	Hathaway.OPTIONSM = match_setter('OPTIONS')
end

local function check_match(entry, req, res, ok, ...)
	if not ok then return false end
	local handler = entry[req.method]
	if handler then
		handler(req, res, ok, ...)
	else
		httpresp.method_not_allowed(req, res)
	end
	return true
end

local function handle(self, req, res)
	local method, path = req.method, req.path
	self.debug('info', format("%s %s HTTP/%s", method, req.uri, req.version))
	local lookup = self.lookup
	local entry = lookup[path]
	if entry then
		local handler = entry[method]
		if handler then
			handler(req, res)
		else
			httpresp.method_not_allowed(req, res)
		end
	else
		local i = 0
		repeat
			i = i + 1
			local entry = lookup[i]
			if not entry then
				httpresp.not_found(req, res)
				break
			end
		until check_match(entry, req, res, path:match(entry[1]))
	end
end
Hathaway.handle = handle

function Hathaway:run(host, port)
	local server, err
	if port then
		server, err = httpserv.new(host, port, self.handler)
	else
		server, err = httpserv.new(host, self.handler)
	end
	if not server then self.debug('new', err) return nil, err end

	self.server = server
	server.debug = self.debug

	local ok, err = server:run()
	if not ok and err ~= 'interrupted' then
		self.debug('run', err)
		return nil, err
	end
	return true
end

function Hathaway:import(env)
	if not env then
		env = _G
	end

	env.GET      = function(...) self:GET(...) end
	env.POST     = function(...) self:POST(...) end
	env.PUT      = function(...) self:PUT(...) end
	env.DELETE   = function(...) self:DELETE(...) end
	env.OPTIONS  = function(...) self:OPTIONS(...) end
	env.GETM     = function(...) self:GETM(...) end
	env.POSTM    = function(...) self:POSTM(...) end
	env.PUTM     = function(...) self:PUTM(...) end
	env.DELETEM  = function(...) self:DELETEM(...) end
	env.OPTIONSM = function(...) self:OPTIONSM(...) end
	env.Hathaway = function(...) self:run(...) end
end

local function new()
	local self = {
		lookup = {},
		debug = M.debug
	}
	self.handler = function(...) return handle(self, ...) end
	return setmetatable(self, Hathaway)
end
M.new = new

function M.import(...)
	return new():import(...)
end

return M

-- vim: ts=2 sw=2 noet:
