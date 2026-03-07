os.print("Hello from Lua on SBoy28!")
os.console_write("Lua app started via runtime layer")

local win = os.window_create(200, 150, "My App")
os.print("window handle: " .. tostring(win))

local fd = os.fs_open("/home/test.txt")
os.print("file descriptor: " .. tostring(fd))

local pid = os.process_spawn("app")
os.print("spawn result: " .. tostring(pid))
