// Copyright Epic Games, Inc. All Rights Reserved.
using Microsoft.Win32;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class VisualStudioSourceCodeAccess : ModuleRules
	{
        public VisualStudioSourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SourceCodeAccess",
					"DesktopPlatform",
					"Projects",
					"Json",
					"VisualStudioSetup",
					"VisualStudioDTE"
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("HotReload");
			}

			bBuildLocallyWithSNDBS = true;
			
			ShortName = "VSSCA";
		}
	}
}
