// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CryptoKeysOpenSSL : ModuleRules
{
	public CryptoKeysOpenSSL(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("CryptoKeys/Classes");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"OpenSSL"
			}
		);

		AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
	}
}