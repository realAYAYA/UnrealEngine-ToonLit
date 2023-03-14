// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class StructUtilsNodes : ModuleRules
	{
		public StructUtilsNodes(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"BlueprintGraph",
					"Core",
					"CoreUObject",
					"Engine",
					"KismetCompiler",
					"PropertyEditor",
					"StructUtils",
					"UnrealEd"
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
				}
			);

			PublicIncludePaths.AddRange(
				new string[]
				{
				}
			);
		}
	}
}