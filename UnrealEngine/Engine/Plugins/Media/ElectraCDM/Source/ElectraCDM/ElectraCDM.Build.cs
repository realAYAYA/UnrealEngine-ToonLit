// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ElectraCDM : ModuleRules
	{
		public ElectraCDM(ReadOnlyTargetRules Target) : base(Target)
		{
			bEnforceIWYU = false; // Disabled because of third party code
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Json"
				});
		}
	}
}
