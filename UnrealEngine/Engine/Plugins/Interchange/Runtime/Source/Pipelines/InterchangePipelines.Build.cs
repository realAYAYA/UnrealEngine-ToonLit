// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InterchangePipelines : ModuleRules
	{
		public InterchangePipelines(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
					"InterchangeCommon",
					"InterchangeEngine",
					"InterchangeFactoryNodes",
					"InterchangeNodes"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"ApplicationCore",
					"CinematicCamera",
					"InterchangeImport",
					"DeveloperSettings",
					"GLTFCore",
					"InputCore",
					"Slate",
					"SlateCore",
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"MainFrame",
						"PhysicsUtilities",
						"TextureUtilitiesCommon",
						"UnrealEd",
					}
				);
			}
		}
	}
}
