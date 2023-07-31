// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
    public class DatasmithGLTFTranslator : ModuleRules
    {
        public DatasmithGLTFTranslator(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Analytics",
                    "Core",
                    "CoreUObject",
                    "DatasmithCore",
                    "Engine",
                    "Json",
                    "MeshDescription",
                    "RawMesh",
                    "Slate",
                    "SlateCore",
                }
            );

			if(Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.Add("MessageLog");
			}

            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "DatasmithContent",
                    "DatasmithTranslator",
                    "GLTFCore",
                }
            );
        }
    }
}
