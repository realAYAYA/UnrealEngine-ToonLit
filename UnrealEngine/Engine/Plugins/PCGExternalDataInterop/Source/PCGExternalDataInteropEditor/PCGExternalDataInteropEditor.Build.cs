// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PCGExternalDataInteropEditor : ModuleRules
	{
		public PCGExternalDataInteropEditor(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"PCG"
				});

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"PCGEditor",
						"PCGExternalDataInterop",
						"UnrealEd",
						"AlembicLib",
						"AlembicLibrary"
					}
				);
			}
		}
	}
}
