// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using AutomationTool;
using UnrealBuildTool;

class FixupRedirects : BuildCommand
{
	public override void ExecuteBuild()
	{
		var ProjectName = ParseParamValue("project", "");

		var EditorExe = CombinePaths(CmdEnv.LocalRoot, @"Engine/Binaries/Win64/UnrealEditor-Cmd.exe");
		LogInformation("********** Running FixupRedirects: {0} -run=ResavePackages -unattended -nopause -buildmachine -fixupredirects -autocheckout -autocheckin -projectonly", EditorExe);
		RunCommandlet(GetCommandletProjectFile(ProjectName), EditorExe, "ResavePackages", "-unattended -nopause -buildmachine -fixupredirects -autocheckout -autocheckin -projectonly");
	}
}
