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

local utils = require 'lem.utils'
local io    = require 'lem.io'
local lfs   = require 'lem.lfs'

local testdir  = 'testdir'
local testfile = 'testfile'

local spawnticker, stopticker
do
	local write, yield = io.write, utils.yield
	local stop = true

	local function ticker()
		stop = false
		repeat
			write('.')
			yield()
		until stop
	end

	function spawnticker()
		utils.spawn(ticker)
	end

	function stopticker()
		stop = true
	end
end

local sleeper = utils.newsleeper()
local attr

io.write('Current directory: '.. lfs.currentdir() ..'\n')

spawnticker()

for name in lfs.dir('.') do
	io.write('\n"' .. name .. '"\n')
end

io.write('\nCreating testdir\n')
assert(lfs.mkdir(testdir))
io.write('\nLocking directory\n')
local lock = assert(lfs.lock_dir(testdir))
io.write('\nGetting attributes\n')
attr = assert(lfs.attributes(testdir))
io.write('\nAttributes:\n')
for k, v in pairs(attr) do
	print(k, v)
end
io.write('\nUnlocking testdir\n')
assert(lock:free())
io.write('\nRemoving testdir\n')
assert(lfs.rmdir(testdir))
io.write('\nCreating testfile\n')
local file = assert(io.open(testfile, 'w'))
io.write('\nLocking testfile\n')
assert(lfs.lock(file, 'w'))
io.write('\nGetting change time\n')
attr = assert(lfs.attributes(testfile, 'change'))
io.write('\nChange time: ' .. attr .. '\n')
stopticker()
io.write('\nSleeping two seconds..\n')
sleeper:sleep(2)
spawnticker()
io.write('\nTouching testfile\n')
assert(lfs.touch(testfile))
io.write('\nGetting change time\n')
attr = assert(lfs.attributes(testfile, 'change'))
io.write('\nChange time: ' .. attr .. '\n')
io.write('\nRenaming testfile\n')
assert(lfs.rename(testfile, testfile..'2'))
io.write('\nRemoving testfile\n')
assert(lfs.remove(testfile..'2'))
io.write('\nDone\n')
stopticker()

-- vim: syntax=lua ts=2 sw=2 noet:
