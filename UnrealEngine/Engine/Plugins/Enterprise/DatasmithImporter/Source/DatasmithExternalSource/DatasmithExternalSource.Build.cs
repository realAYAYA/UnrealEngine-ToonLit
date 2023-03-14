// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
    public class DatasmithExternalSource : ModuleRules
    {
        public DatasmithExternalSource(ReadOnlyTargetRules Target) : base(Target)
        {
            PublicDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
					"DirectLinkExtension",
					"ExternalSource",
                    "DatasmithNativeTranslator",
                }
            );

            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "DatasmithCore",
                    "DatasmithTranslator",
					"Projects",
				}
			);

			if (Target.Type == TargetType.Editor)
			{
				PrivateDependencyModuleNames.AddRange(
					new string[]
					{
						"DesktopPlatform",
						"DirectLinkExtensionEditor",
						"Slate",
						"UnrealEd",
					}
				);
			}
		}
    }
}
