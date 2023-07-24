// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System.Collections.Generic;
using AutomationTool;
using UnrealBuildTool;
using System.IO;
//using System.Linq;
using EpicGames.Core;

namespace AutomationScripts
{
	public class WrangleContentForDebugging : BuildCommand
	{
		public override void ExecuteBuild()
		{
			LogInformation("======= WrangleContentForDebugging - Start =======");

			if (ParseParam("usage"))
			{
				LogInformation("Arguments : \n -platform=<platform> \n -ProjectFilePath=<path/ProjectName.uproject> Mandatory. Path to a path to the roject you want to Wrangle Debug Data to. \n -SourcePackage=<path/PackageName.ipa> Optional. Specify the .ipa to wrangle data for debug from. When not specified, [ProjectPath]/Build/[IOS|TVOS]/[ProjectName].ipa will be used.");
			}

			FileReference ProjectFile = new FileReference(ParseParamValue("ProjectFilePath=", null));
			//ProjectFile = ParseProjectParam();
			var Params = new ProjectParams
			(
				Command: this,
				RawProjectPath: ProjectFile
			);
			var SC = Project.CreateDeploymentContext(Params, false);

			//Project newProject;
			SC[0].StageTargetPlatform.PrepareForDebugging(ParseParamValue("SourcePackage=", null), ParseParamValue("ProjectFilePath=", null), ParseParamValue("platform=", null));

			LogInformation("======= WrangleContentForDebugging - Done ========");
		}
	}
}