// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DerivedDataCache : ModuleRules
{
	public DerivedDataCache(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.Add("Zen");

		// Dependencies for "S3" and "HTTP" backends
		PrivateDependencyModuleNames.AddRange(new string[] { "SSL", "Json", "Zen" });
		PrivateIncludePathModuleNames.AddRange(new string[] { "DesktopPlatform", "Zen"});
		AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		// Platform-specific opt-in
		PrivateDefinitions.Add($"WITH_HTTP_DDC_BACKEND=1");
		PrivateDefinitions.Add($"WITH_S3_DDC_BACKEND={(Target.Platform == UnrealTargetPlatform.Win64 ? 1 : 0)}");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
