// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class GeometryCacheStreamer : ModuleRules
	{
		public GeometryCacheStreamer(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"GeometryCache",
					"Slate",
					"SlateCore"
				}
			);
		}
	}
}
