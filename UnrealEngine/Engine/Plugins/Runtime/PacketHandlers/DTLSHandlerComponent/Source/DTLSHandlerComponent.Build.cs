// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DTLSHandlerComponent : ModuleRules
{
    public DTLSHandlerComponent(ReadOnlyTargetRules Target) : base(Target)
	{
        PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
                "CoreUObject",
				"NetCore",
                "PacketHandler",
                "Engine",
                "SSL",
			}
		);

        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}