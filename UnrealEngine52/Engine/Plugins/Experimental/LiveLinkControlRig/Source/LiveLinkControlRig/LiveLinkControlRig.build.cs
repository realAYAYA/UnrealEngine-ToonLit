// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LiveLinkControlRig : ModuleRules
	{
		public LiveLinkControlRig(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"LiveLinkInterface",
			});

			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"InputCore",
				"Media",
				"Projects",
				"SlateCore",
				"Slate",
				"RigVM",
				"ControlRig",
			});
		}
	}
}
