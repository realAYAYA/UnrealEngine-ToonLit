// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ProxyTableUncooked : ModuleRules
	{
		public ProxyTableUncooked(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(new string[] {"ProxyTable"});
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Chooser",
					"Engine",
					"AssetTools",
					"UnrealEd",
					"AssetDefinition",
					"GameplayTags",
					"StructUtils",
					"BlueprintGraph",
					"KismetCompiler"
					// ... add private dependencies that you statically link with here ...
				}
			);
		}
	}
}
