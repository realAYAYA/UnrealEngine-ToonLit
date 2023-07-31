// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class USDStage : ModuleRules
	{
		public USDStage(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"USDSchemas" // Has to be a public dependency because the stage actor has an FUsdInfoCache member
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"AnimationCore",
					"CinematicCamera",
					"ControlRig",
					"Core",
					"CoreUObject",
					"Engine",
					"HairStrandsCore",
					"LevelSequence",
					"LiveLinkComponents", // For tracking edits to LiveLinkComponentController properties and writing to USD
					"LiveLinkInterface",
					"MeshDescription",
					"MovieScene",
					"MovieSceneTracks",
					"Niagara",	// Needed by GroomComponent.h
					"Projects", // So that we can use the plugin manager to find out our content dir and cook the base materials
					"Slate",
					"SlateCore",
					"StaticMeshDescription",
					"UnrealUSDWrapper",
					"USDClasses",
					"USDUtilities",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"ControlRigDeveloper",
						"DeveloperToolSettings",
						"EditorStyle", // For the font style on the stage actor customization
						"InputCore", // For keyboard control on the widget in the stage actor customization
						"LevelSequenceEditor",
						"MovieSceneTools",
						"PropertyEditor", // For the stage actor's details customization
						"Sequencer",
						"UnrealEd",
					}
				);
			}
		}
	}
}
