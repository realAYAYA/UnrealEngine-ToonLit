// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class CameraCalibrationCore : ModuleRules
	{
		public CameraCalibrationCore(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"CinematicCamera",
					"Core",
					"CoreUObject",
					"DeveloperSettings",
					"Engine",
					"LiveLinkComponents",
					"RenderCore",
					"RHI",
					"Slate",
					"SlateCore",
					"ProceduralMeshComponent",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
                {
					"LiveLinkInterface",
                    "Projects",
				}
			);

			if (Target.bBuildEditor == true)
			{
				PrivateDependencyModuleNames.Add("UnrealEd");
			}

			PrivateIncludePaths.AddRange(
				new string[] 
				{
            		//required for ScreenPass
					System.IO.Path.Combine(GetModuleDirectory("Renderer"), "Private"),
				}
			);
		}
	}
}
