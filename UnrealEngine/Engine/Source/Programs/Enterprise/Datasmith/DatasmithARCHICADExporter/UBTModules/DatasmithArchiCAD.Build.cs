// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	[SupportedPlatforms("Win64", "Mac")]
	public class DatasmithArchiCAD : ModuleRules
	{
		public DatasmithArchiCAD(ReadOnlyTargetRules Target)
			: base(Target)
		{
			PublicIncludePaths.Add("Runtime/Launch/Public");

			PrivateIncludePaths.Add("Runtime/Launch/Private");

			PrivateDependencyModuleNames.Add("Core");
			//PrivateDependencyModuleNames.Add("Projects");
		}
	}
}
