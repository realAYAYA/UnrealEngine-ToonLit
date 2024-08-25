# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import flow.cmd
from pathlib import Path

#-------------------------------------------------------------------------------
class _Shell(object):
    def _add_clink_lua(self):
        channel = self.get_channel()
        system = channel.get_system()
        working_dir = system.get_working_dir()

        lua_path = working_dir + "complete.clink.lua"
        if not system.is_path_stale(lua_path):
            return

        cmd_tree = system.get_command_tree()
        tree_root = cmd_tree.get_root_node()

        lua = open(lua_path, "w")
        lua.write("local commands = {")
        for name,_ in tree_root.read_children():
            lua.write(f"'{name}',")
        lua.write("}\n")

        run_py_path = os.path.abspath(__file__ + "/../../core/system/run.py")
        cmd = rf'"{sys.executable}" -Xutf8 -Esu "{run_py_path}" "{working_dir}/manifest" $complete --daemon'
        cmd = cmd.replace("\\", "/")
        lua.write(f"local py_server_cmd = '{cmd}'")

        clink_lua = _get_clink_lua()
        lua.write(clink_lua)
        lua.close()

    def register_shells(self, registrar):
        registrar.add("cmd", lambda x: self)
        return super().register_shells(registrar)

    def boot_shell(self, env, cookie):
        self._add_clink_lua()

        channel = self.get_channel()
        system = channel.get_system()
        working_dir = system.get_working_dir()
        env["CLINK_PATH"] = working_dir
        env["DIRCMD"] = "/ogen"

        if prompt := env.get("FLOW_PROMPT", None):
            prompt = prompt.replace("\n", "\r")
            env["FLOW_PROMPT"] = prompt

        try: os.makedirs(os.path.dirname(cookie))
        except: pass

        thefuzz_path = Path(__file__).parent / "cmd_thefuzz.bat"
        thefuzz_path = thefuzz_path.resolve()

        with open(cookie, "wt") as out:
            print(f"cd /d \"{os.getcwd()}\"", file=out)
            for key, value in env.read_changes():
                value = value or ""
                print(fr'set "{key}={value}"', file=out)

            print(rf'doskey ff=fd $B fzf $B clip', file=out)
            print(rf'doskey qq="{thefuzz_path}" history', file=out)
            print(rf'doskey de="{thefuzz_path}" explore $*', file=out)
            print(rf'doskey dc="{thefuzz_path}" chdir $*', file=out)
            print(rf'doskey dc.="{thefuzz_path}" chdir .', file=out)

            print("clink_x64.exe inject", file=out)
            print("$tip.exe", file=out)

            if user_script := getattr(self, "_user_script", None):
                print(rf'call "{user_script}"', file=out)


#-------------------------------------------------------------------------------
class Cmd(_Shell):
    def run(self, env):
        if os.name != "nt":
            return super().run(env)

        # Give the user a convenient way to add customisations in to the session
        self.print_info("Customisation")
        home_dir = Path(self.get_home_dir())
        tags = (
            flow.cmd.text.grey("none"),
            flow.cmd.text.white("okay"),
        )

        # These are really a no-op. It is here to hint to the user how they can
        # add their own channels
        candidate = home_dir / "channels"
        found = candidate.is_dir()
        print(tags[int(found)], candidate / "*")

        # Indicate to the user that there's a way to hook the boot process too
        candidate = home_dir / "hooks/boot.bat"
        found = candidate.is_file()
        print(tags[int(found)], candidate)

        # This is run just before the prompt is shown
        candidate = home_dir / "hooks/startup.bat"
        found = candidate.is_file()
        print(tags[int(found)], candidate)
        if found:
            self._user_script = candidate

        print()
        return super().run(env)


#-------------------------------------------------------------------------------
def _get_clink_lua():
    return r"""
local py_server = io.popen2(py_server_cmd)

for _, tree_root in ipairs(commands) do
    local arg_handler = function(index, line)
        py_server:write("\x01\n")
        py_server:write(tree_root.."\n")
        local n = line:getwordcount()
        for i = 2, n do
            py_server:write(line:getword(i).."\n")
        end

        local delim = line:getwordinfo(n).delim
        if delim ~= " " then
            py_server:write("\x03"..delim.."\n")
        end
        py_server:write("\x02\n")

        local ret = {}
        while true do
            match = py_server:read()
            if match == nil then return end
            if match == "\x01" then return ret end
            if match == "\x02" then return end
            table.insert(ret, match)
        end
    end

    clink.argmatcher(tree_root)
        :loop(1)
        :setflagprefix("-")
        :addarg(arg_handler)
        :addflags(arg_handler)
end

local cmd_generator = clink.generator(0)
function cmd_generator:generate(line_state, match_builder)
    if line_state:getwordcount() ~= 1 then return end
    if line_state:getword(1) ~= "." then return end

    match_builder:setprefixincluded()
    for _, command in ipairs(commands) do
        if command:sub(1, 1) == "." then
            match_builder:addmatch(command)
        end
    end
    return true
end

function cmd_generator:getprefixlength(line_state)
    if line_state:getwordcount() ~= 1 then return end
    local prefix = line_state:getword(1):sub(1, 1)
    if prefix == "." then return 1 end
    return 0
end

local cliff_prompt = clink.promptfilter(-493)
function cliff_prompt:filter(prompt)
    prompt_override = os.getenv("FLOW_PROMPT")
    if prompt_override == nil then
        return prompt
    end

    py_server:write("\x01\n")
    py_server:write("$\n")
    py_server:write(prompt_override)
    py_server:write("\n\x02\n")

    prompt = nil
    while true do
        ret = py_server:read()
        if ret == nil then return end
        if ret == "\x01" then return prompt end
        if ret == "\x02" then return end
        if not prompt then
            prompt = ret
        else
            prompt = prompt.."\n"..ret
        end
    end
end
"""
