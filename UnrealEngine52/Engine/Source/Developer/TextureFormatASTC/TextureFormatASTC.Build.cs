// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class TextureFormatASTC : ModuleRules
{
	public TextureFormatASTC(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePathModuleNames.AddRange(new string[]
		{
			"DerivedDataCache",
			"Engine",
			"TargetPlatform",
			"TextureCompressor",
		});
		
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"ImageCore",
			"ImageWrapper",
			"TextureBuild",
			"TextureFormatIntelISPCTexComp",
		});

		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Win64/astcenc-sse2.exe");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Win64/astcenc-sse4.1.exe");
		//	RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Win64/astcenc-avx2.exe");
		}
		else if (Target.Platform == UnrealTargetPlatform.Mac)
		{
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Mac/astcenc-sse2");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Mac/astcenc-sse4.1");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Mac/astcenc-neon");
		}
		else if (Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Linux64/astcenc-sse2");
			RuntimeDependencies.Add("$(EngineDir)/Binaries/ThirdParty/ARM/Linux64/astcenc-sse4.1");
		}
	}
}
