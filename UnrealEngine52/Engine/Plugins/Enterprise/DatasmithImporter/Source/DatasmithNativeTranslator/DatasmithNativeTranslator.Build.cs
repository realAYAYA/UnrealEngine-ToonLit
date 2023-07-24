// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class DatasmithNativeTranslator : ModuleRules
    {
        public DatasmithNativeTranslator(ReadOnlyTargetRules Target) : base(Target)
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
                    "DatasmithCore",
                    "DatasmithTranslator",
                    "MeshDescription",
                    "RawMesh",
                    "StaticMeshDescription",
                }
            );
        }
    }
}
