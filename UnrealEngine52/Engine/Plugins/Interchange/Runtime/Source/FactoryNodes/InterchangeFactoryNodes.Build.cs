// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class InterchangeFactoryNodes : ModuleRules
	{
		public InterchangeFactoryNodes(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CinematicCamera",
					"Core",
					"CoreUObject",
					"Engine",
					"InterchangeCore",
					"InterchangeNodes",
					"TextureUtilitiesCommon",
					"VariantManagerContent",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"LevelSequence",
				}
			);
		}
	}
}
