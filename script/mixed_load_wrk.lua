-- mixed_load.lua
-- Simulate mixed CPU, I/O, and light routes

wrk.method = "GET"

-- Define request mix ratios
-- (e.g. 60% "/", 30% "/io", 10% "/cpu")
local routes = {
  {path = "/", weight = 0.33},
  {path = "/io", weight = 0.33},
  {path = "/cpu", weight = 0.34}
}

-- Build cumulative probabilities
local cumulative = {}
local total = 0
for i, route in ipairs(routes) do
  total = total + route.weight
  cumulative[i] = total
end

-- Function to pick next request path
function request()
  local r = math.random()
  for i, route in ipairs(routes) do
    if r <= cumulative[i] then
      return wrk.format(nil, route.path)
    end
  end
  return wrk.format(nil, "/")
end