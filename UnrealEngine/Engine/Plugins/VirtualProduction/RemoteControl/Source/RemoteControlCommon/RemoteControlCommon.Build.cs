// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

/** Shared functionality between RemoteControlUI and RemoteControlProtocolWidgets */
public class RemoteControlCommon : ModuleRules
{
	public RemoteControlCommon(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[] {
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[] {
				"ApplicationCore",
				"Core",
				"CoreUObject",
				"Engine",
				"InputCore"
			}
		);
		
		if (Target.bBuildEditor)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"PropertyEditor",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"AssetRegistry",
					"AssetTools",
					"BlueprintGraph",
					"DeveloperSettings",
					"EditorWidgets",
					
					"EditorSubsystem",
					"Projects",
					"PropertyEditor",
					"Slate",
					"SlateCore",
					"UnrealEd",
				}
			);
		}
    }
}
