// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PlatformCryptoOpenSSL : ModuleRules
	{
		public PlatformCryptoOpenSSL(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"PlatformCryptoTypes",
				}
				);

			AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
		}
	}
}
