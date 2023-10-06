# Copyright Epic Games, Inc. All Rights Reserved.

import os
import unreal

#-------------------------------------------------------------------------------
class Prompt(object):
    def prompt(self, context):
        try:
            self._get_ue_branch(context)
        except EnvironmentError:
            self._get_git_branch(context)

        context.TITLE = context.UE_BRANCH + " | " + context.UE_PROJECT
        super().prompt(context)

    def _get_ue_branch(self, context):
        try:
            session = self.get_noticeboard(self.Noticeboard.SESSION)
            ue_context = unreal.Context(session["uproject"] or ".")

            branch_full = ue_context.get_engine().get_info().get("BranchName", "UnknownBranch")
            branch_name = ue_context.get_name()
            if project := ue_context.get_project():
                project_name = project.get_name()

                if ue_context.get_type() == unreal.ContextType.FOREIGN:
                    branch_name = ue_context.get_engine().get_dir().parent.name
                    project_name += "(F)"
            else:
                project_name = "-"
        except EnvironmentError:
            raise
        except Exception as e:
            branch_full = "Error"
            branch_name = "Error"
            project_name = str(e) + "\nError"

        context.UE_BRANCH = branch_name
        context.UE_PROJECT = project_name
        context.UE_BRANCH_FULL = branch_full

    def _get_git_branch(self, context):
        from pathlib import Path
        for git_root in (Path(os.getcwd()) / "x").parents:
            if (git_root / ".git").is_dir():
                break
        else:
            context.UE_BRANCH = "NoBranch"
            context.UE_PROJECT = "NoProject"
            return

        context.UE_BRANCH = git_root.stem
        context.UE_PROJECT = "nobranch"
        with (git_root / ".git/HEAD").open("rt") as file:
            for line in file.readlines():
                line = line.strip()
                if line.startswith("ref: "):
                    context.UE_PROJECT = line[5:].split("/")[-1]
                else:
                    context.UE_PROJECT = line[:6]
                break
