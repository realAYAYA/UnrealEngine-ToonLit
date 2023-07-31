// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class GLTFCore : ModuleRules
    {
        public GLTFCore(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
                    "CoreUObject",
                }
                );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Engine",
                    "MeshDescription",
					"StaticMeshDescription",
                    "Json",
                    "RenderCore",
                }
                );
        }
    }
}
