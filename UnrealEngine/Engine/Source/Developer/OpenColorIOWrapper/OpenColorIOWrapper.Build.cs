// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class OpenColorIOWrapper : ModuleRules
	{
		public OpenColorIOWrapper(ReadOnlyTargetRules Target) : base(Target)
		{
			PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

			PublicIncludePathModuleNames.AddRange(new string[]
			{
				"ColorManagement", // for ColorManagementDefines.h
			});

			PrivateIncludePathModuleNames.AddRange(new string[]
			{
				"Engine", // for TextureDefines.h
			});

			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ColorManagement",
				"ImageCore",
				"OpenColorIOLib",
			});

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
			});

			bool bIsPlatformSupported = false;

			if (Target.IsInPlatformGroup(UnrealPlatformGroup.Windows) ||
				Target.IsInPlatformGroup(UnrealPlatformGroup.Unix) ||
				Target.Platform == UnrealTargetPlatform.Mac)
			{
				// Mirror WITH_OCIO_LIB coverage, which suffices for editor use cases.
				bIsPlatformSupported = true;
			}

			PublicDefinitions.Add("WITH_OCIO=" + (bIsPlatformSupported ? "1" : "0"));
		}
	}
}
