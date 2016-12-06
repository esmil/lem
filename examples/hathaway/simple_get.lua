-- Require the hathaway library
local hathaway = require 'lem.hathaway'

-- Import functions as GET and GETM
hathaway.import()

-- A simple get route
GET('/', function(req, res)
	res:add('Hello world')
end)

-- A get route with a pattern
GETM('^/greet/(.*)$', function(req, res, name)
	res:add('Hello '..name)
end)

-- Start to serve
Hathaway('*', arg[1] or 8080)
