// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Win64", "Linux", "Android", "LinuxArm64")]
public class OpenGLDrv : ModuleRules
{
	public OpenGLDrv(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"RHI",
		});

		PrivateDependencyModuleNames.AddRange(new string[] {
			"CoreUObject",
			"ApplicationCore",
			"Engine",
			"RHICore",
			"RenderCore",
			"PreLoadScreen"
		});

		PrivateIncludePathModuleNames.Add("ImageWrapper");
		DynamicallyLoadedModuleNames.Add("ImageWrapper");

		PublicIncludePathModuleNames.Add("OpenGL");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenGL");

		if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "SDL2");
		}

		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateIncludePathModuleNames.Add("TaskGraph");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("detex");
		}

        if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // for Swappy
            PublicDefinitions.Add("USE_ANDROID_OPENGL_SWAPPY=1");

            PrivateDependencyModuleNames.Add("GoogleGameSDK");
			PrivateIncludePathModuleNames.Add("Launch");
		}

        if (Target.Platform != UnrealTargetPlatform.Win64
			&& Target.Platform != UnrealTargetPlatform.IOS && Target.Platform != UnrealTargetPlatform.Android
			&& !Target.IsInPlatformGroup(UnrealPlatformGroup.Linux)
			&& Target.Platform != UnrealTargetPlatform.TVOS)
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		PublicDefinitions.Add(Target.Platform == UnrealTargetPlatform.Android ? "USE_ANDROID_OPENGL=1" : "USE_ANDROID_OPENGL=0");
	}
}
