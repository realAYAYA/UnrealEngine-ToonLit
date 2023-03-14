// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class RivermaxMediaFactory : ModuleRules
	{
		public RivermaxMediaFactory(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Media",
					"MediaAssets",
					"Projects",
					"RivermaxMedia"
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
				});
		}
	}
}
