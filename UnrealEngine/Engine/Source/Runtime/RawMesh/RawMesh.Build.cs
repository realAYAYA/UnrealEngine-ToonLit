// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RawMesh : ModuleRules
{
    public RawMesh(ReadOnlyTargetRules Target) : base(Target)
	{
        PublicIncludePaths.AddRange(new string[] {"Runtime/RawMesh/Public"});
        PrivateDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });
	}
}
