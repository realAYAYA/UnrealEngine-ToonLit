// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenCVLensDistortion : ModuleRules
	{
		public OpenCVLensDistortion(ReadOnlyTargetRules Target) : base(Target)
		{            
            PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"OpenCVHelper",
					"OpenCV",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Projects",
					"RenderCore",
					"RHI",
					"Projects",
				}
			);
		}
	}
}
