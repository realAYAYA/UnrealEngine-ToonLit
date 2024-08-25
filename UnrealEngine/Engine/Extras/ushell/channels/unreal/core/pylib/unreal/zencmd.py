# Copyright Epic Games, Inc. All Rights Reserved.

import os
import sys
import shutil
import unreal
import unrealcmd
import unreal.cmdline
import http.client
import json
import hashlib

#-------------------------------------------------------------------------------
class ZenBaseCmd(unrealcmd.Cmd):
    def perform_project_completion(self, prefix, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            conn.request("GET", "/prj", headers=headers)
            response = conn.getresponse()
            projects = json.loads(response.read().decode())
        except socket.timeout:
            return

        for project in projects:
            yield project['Id']

    def perform_oplog_completion(self, prefix, project_filter, hostname="localhost", port=8558):
        headers = {"Accept":"application/json"}
        conn = http.client.HTTPConnection(hostname, port=port, timeout=0.1)
        projects = []
        try:
            if project_filter:
                conn.request("GET", f"/prj/{project_filter}", headers=headers)
                response = conn.getresponse()
                projects.append(json.loads(response.read().decode()))
            else:
                conn.request("GET", "/prj", headers=headers)
                response = conn.getresponse()
                projects = json.loads(response.read().decode())
        except socket.timeout:
            return

        for project in projects:
            for oplog in project['oplogs']:
                yield oplog['id']

    def get_project_id_from_project(self, project):
        if not project:
            self.print_error("An active project is required for this operation")

        normalized_project_path = str(project.get_path()).replace("\\","/")
        project_id = project.get_name() + "." + hashlib.md5(normalized_project_path.encode('utf-8')).hexdigest()[:8]
        return project_id;


    def get_project_or_default(self, explicit_project):
        if explicit_project:
            return explicit_project

        ue_context = self.get_unreal_context()
        return self.get_project_id_from_project(ue_context.get_project())


#-------------------------------------------------------------------------------
class ZenUETargetBaseCmd(ZenBaseCmd):
    variant = unrealcmd.Arg("development", "Build variant to launch")
    runargs  = unrealcmd.Arg([str], "Additional arguments to pass to the process being launched")
    build    = unrealcmd.Opt(True, "Build the target prior to running it")

    def _build(self, target, variant, platform):
        if target.get_type() == unreal.TargetType.EDITOR:
            build_cmd = ("_build", "editor", variant)
        else:
            platform = platform or unreal.Platform.get_host()
            build_cmd = ("_build", "target", target.get_name(), platform, variant)

        import subprocess
        ret = subprocess.run(build_cmd)
        if ret.returncode:
            raise SystemExit(ret.returncode)

    def get_binary_path(self, target, platform=None):
        ue_context = self.get_unreal_context()

        if isinstance(target, str):
            target = ue_context.get_target_by_name(target)
        else:
            target = ue_context.get_target_by_type(target)

        if self.args.build:
            self._build(target, self.args.variant, platform)

        variant = unreal.Variant.parse(self.args.variant)
        if build := target.get_build(variant, platform):
            if build := build.get_binary_path():
                return build

        raise EnvironmentError(f"No {self.args.variant} build found for target '{target.get_name()}'")

    def run_ue_program_target(self, target_name, cmdargs = None):
        binary_path = self.get_binary_path(target_name)

        final_args = []
        if cmdargs: final_args += cmdargs
        if self.args.runargs: final_args += (*unreal.cmdline.read_ueified(*self.args.runargs),)

        launch_kwargs = {}
        if not sys.stdout.isatty():
            launch_kwargs = { "silent" : True }
        exec_context = self.get_exec_context()
        runnable = exec_context.create_runnable(str(binary_path), *final_args);
        runnable.launch(**launch_kwargs)
        return True if runnable.is_gui() else runnable.wait()


#-------------------------------------------------------------------------------
class ZenUtilityBaseCmd(ZenBaseCmd):
    zenargs  = unrealcmd.Arg([str], "Additional arguments to pass to zen utility")

    def get_zen_utility_command(self, cmdargs):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        platform = unreal.Platform.get_host()
        exe_filename = "zen"
        if platform == "Win64":
            exe_filename = exe_filename + ".exe"

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/{platform}/{exe_filename}"
        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        final_args = []
        if cmdargs: final_args += cmdargs
        if self.args.zenargs: final_args += self.args.zenargs

        return bin_path, final_args


    def run_zen_utility(self, cmdargs):
        bin_path, final_args = self.get_zen_utility_command(cmdargs)

        cmd = self.get_exec_context().create_runnable(bin_path, *final_args)
        return cmd.run()