// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenImageDenoise : ModuleRules
	{
		public OpenImageDenoise(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"RenderCore",
					"Renderer",
					"RHI",
					"IntelOIDN"
				}
			);
			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("MessageLog");
			}
		}
	}
}
