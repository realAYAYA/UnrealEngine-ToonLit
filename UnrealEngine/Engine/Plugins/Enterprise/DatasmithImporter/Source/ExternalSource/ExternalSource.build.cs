// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ExternalSource : ModuleRules
	{
		public ExternalSource(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DatasmithTranslator",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DatasmithCore",
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"Engine",
						"UnrealEd",
					}
				);
			}
		}
	}
}
