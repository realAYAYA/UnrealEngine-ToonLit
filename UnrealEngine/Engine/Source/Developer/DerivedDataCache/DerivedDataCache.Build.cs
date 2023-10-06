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
		// NOTE: DesktopPlatform is needed for UE Cloud DDC interactive login.  It is set as an private include
		//		 dependency here, but not as a full dependency.  This will compile and link, but the absence of
		//		 DesktopPlatform module at runtime will prevent interactive login from working.  Adding DesktopPlatform
		//		 would bloat the size of multiple users of the DDC module who don't need UE Cloud DDC interactive login,
		//		 so we don't add it as a full dependency.  Build targets (particularly standalone programs) that use the
		//		 DDC module, should add a dependency on DesktopPlatform if they require interactive login.  This will
		//		 change in the future as the DDC module dependencies are refactored in the future.
		PrivateDependencyModuleNames.AddRange(new string[] { "SSL", "Json", "Zen" });
		PrivateIncludePathModuleNames.AddRange(new string[] { "DesktopPlatform", "Analytics" });
		AddEngineThirdPartyPrivateStaticDependencies(Target, "libcurl");
		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");

		// Platform-specific opt-in
		PrivateDefinitions.Add($"WITH_HTTP_DDC_BACKEND=1");
		PrivateDefinitions.Add($"WITH_S3_DDC_BACKEND={(Target.Platform == UnrealTargetPlatform.Win64 ? 1 : 0)}");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}
}
