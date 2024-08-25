# Copyright Epic Games, Inc. All Rights Reserved.

import sys
import unrealcmd
import subprocess

#-------------------------------------------------------------------------------
class Auth(unrealcmd.Cmd):
    """ Authorize oneself for cloud-based DDC """
    service     = unrealcmd.Arg("", "Name of the service to authorize with")
    extraargs   = unrealcmd.Arg([str], "Authorisation tool arguments")
    query       = unrealcmd.Opt(False, "Only check the current status")

    def _lookup_service_name(self):
        if getattr(self, "_service_name", None):
            return

        self.print_info("Discovering service name")
        print("Config path: Engine/StorageServers/Default")

        config = self.get_unreal_context().get_config()
        default = config.get("Engine", "StorageServers", "Default")
        if value := default.OAuthProviderIdentifier:
            print("found;", value)
            self._service_name = value
        else:
            cloud = config.get("Engine", "StorageServers", "Cloud")
            if value := cloud.OAuthProviderIdentifier:
                self._service_name = value

    def main(self):
        ue_context = self.get_unreal_context()
        engine = ue_context.get_engine()

        bin_type = "win-x64"
        if sys.platform == "darwin": bin_type = "osx-x64"
        elif sys.platform == "linux": bin_type = "linux-x64"

        bin_path = engine.get_dir()
        bin_path /= f"Binaries/DotNET/OidcToken/{bin_type}/OidcToken.exe"
        if not bin_path.is_file():
            raise FileNotFoundError(f"Unable to find '{bin_path}'")

        self._service_name = self.args.service
        self._lookup_service_name()
        if not self._service_name:
            raise ValueError("Unable to discover service name")

        args = (
            "--Service=" + str(self._service_name),
        )

        if self.args.query:
            args = (*args, "--Mode=Query")

        if project := ue_context.get_project():
            args = (*args, "--Project=" + str(project.get_path()))

        args = (*args, *self.args.extraargs)

        cmd = self.get_exec_context().create_runnable(bin_path, *args)
        result = cmd.run()
        return result
