// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AnimNextTestSuite : ModuleRules
	{
		public AnimNextTestSuite(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"AnimNext",
					"StructUtils",
					"RigVM",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"UnrealEd",
						"AnimNextUncookedOnly",
						"AnimNextEditor",
						"PythonScriptPlugin",
						"RigVMDeveloper",
					}
				);
			}
		}
	}
}