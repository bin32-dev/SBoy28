os.print("[lua] init.lua started")

if fs.exists("2:/lua/test.lua") then
  os.print("[lua] loading 2:/lua/test.lua")
  dofile("2:/lua/test.lua")
else
  os.print("[lua] test.lua not found")
end

fs.write("2:/lua/output.txt", "hello from lua\n")
local data = fs.read("2:/lua/output.txt")
if data then
  os.print("[lua] read back: " .. data)
end
