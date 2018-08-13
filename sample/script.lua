-- script.lua
-- Receives a table, returns the sum of its components.
local dump = require("dump")

io.write("The table the script received has:\n")

function my_script_system1(update_stage, pos, vel)
  print("my_script_system1", update_stage.dt)
end

function my_script_system2(update_stage, components)
  print("my_script_system2", update_stage.dt)
end

dump(foo)
dump(debug.getinfo(my_script_system))

x = 0
for i = 1, #foo do
  print(i, foo[i])
  x = x + foo[i]
end

io.write("Returning data back to C\n")

return x