// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InternationalizationSettings : ModuleRules
	{
		public InternationalizationSettings(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"InputCore",
					"Engine",
					"Slate",
					"SlateCore",
					"AppFramework",
					"PropertyEditor",
					"Localization",
				}
			);

			PrivateIncludePathModuleNames.AddRange(
				new string[] {
				}
			);

			DynamicallyLoadedModuleNames.AddRange(
				new string[] {
					"SettingsEditor"
				}
			);
		}
	}
}
