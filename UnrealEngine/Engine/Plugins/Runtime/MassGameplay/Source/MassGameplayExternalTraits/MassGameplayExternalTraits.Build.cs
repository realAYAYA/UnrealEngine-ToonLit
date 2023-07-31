// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class MassGameplayExternalTraits : ModuleRules
	{
		public MassGameplayExternalTraits(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicIncludePaths.AddRange(
				new string[] { 
					ModuleDirectory + "/Public" 
				}
			);

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"MassEntity",
				}
			);
		}
	}
}
