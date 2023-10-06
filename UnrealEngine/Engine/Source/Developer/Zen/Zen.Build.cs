// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class Zen : ModuleRules
{
	public Zen(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new string[] { "Core", "Sockets", "SSL", "Json" });
		PrivateIncludePathModuleNames.Add("DesktopPlatform");
		PrivateIncludePathModuleNames.Add("Analytics");
		
		AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
