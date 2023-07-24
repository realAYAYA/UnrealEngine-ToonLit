// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class CADInterfaces : ModuleRules
	{
		public CADInterfaces(ReadOnlyTargetRules Target) : base(Target)
		{
			bLegalToDistributeObjectCode = true;
			bUseUnity = false;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CADKernel",
					"CADLibrary",
					"CADTools",
					"DatasmithCore",
					"Json",
				}
			);

			// CAD library is only available if TechSoft is available too
			bool bHasTechSoft = System.Type.GetType("TechSoft") != null;

			if ((Target.Platform == UnrealTargetPlatform.Win64 || Target.Platform == UnrealTargetPlatform.Linux) && bHasTechSoft)
			{
				PublicDependencyModuleNames.Add("TechSoft");
			}
		}
	}
}
