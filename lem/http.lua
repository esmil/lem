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

local http    = require 'lem.http.core'
local parsers = require 'lem.parsers'

parsers.lookup['HTTPRequest'] = http.HTTPRequest
http.HTTPRequest = nil
parsers.lookup['HTTPResponse'] = http.HTTPResponse
http.HTTPResponse = nil

return http

-- vim: ts=2 sw=2 noet:
