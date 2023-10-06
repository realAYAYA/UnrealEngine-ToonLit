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
			PublicIncludePathModuleNames.Add("Launch");
			PrivateDependencyModuleNames.Add("Core");
		}
	}
}
