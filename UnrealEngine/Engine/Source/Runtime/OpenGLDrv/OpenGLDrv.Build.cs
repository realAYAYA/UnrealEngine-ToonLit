// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

[SupportedPlatforms("Linux", "Android", "LinuxArm64")]
[SupportedPlatformGroups("Windows")]
public class OpenGLDrv : ModuleRules
{
	public OpenGLDrv(ReadOnlyTargetRules Target) : base(Target)
	{
		IWYUSupport = IWYUSupport.None;

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

		if (Target.Platform == UnrealTargetPlatform.Android)
		{
			PrivateDependencyModuleNames.Add("detex");
			PrivateDependencyModuleNames.Add("ArmlibGPUInfo");
		}

		if (Target.Platform == UnrealTargetPlatform.Android)
        {
            // for Swappy
            PublicDefinitions.Add("USE_ANDROID_OPENGL_SWAPPY=1");

            PrivateDependencyModuleNames.Add("GoogleGameSDK");
			PrivateIncludePathModuleNames.Add("Launch");
		}

        if (!Target.IsInPlatformGroup(UnrealPlatformGroup.Windows)
			&& !Target.IsInPlatformGroup(UnrealPlatformGroup.IOS)
			&& Target.Platform != UnrealTargetPlatform.Android
			&& !Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrecompileForTargets = PrecompileTargetsType.None;
		}

		PublicDefinitions.Add(Target.Platform == UnrealTargetPlatform.Android ? "USE_ANDROID_OPENGL=1" : "USE_ANDROID_OPENGL=0");
	}
}
