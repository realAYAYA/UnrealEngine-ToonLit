// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RSA : ModuleRules
{
	public RSA(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.Add("Core");

        if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows) ||
			Target.Platform == UnrealTargetPlatform.Mac ||
			Target.IsInPlatformGroup(UnrealPlatformGroup.Linux))
		{
			PrivateDependencyModuleNames.Add("OpenSSL");
			PrivateDefinitions.Add("RSA_USE_OPENSSL=1");
		}
		else
		{
			PrivateDefinitions.Add("RSA_USE_OPENSSL=0");
		}
	}
}
