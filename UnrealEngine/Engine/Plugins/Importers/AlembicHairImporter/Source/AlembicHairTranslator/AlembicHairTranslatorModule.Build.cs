// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class AlembicHairTranslatorModule : ModuleRules
	{
        public AlembicHairTranslatorModule(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
                new string[]
                {
					"Imath",
                    "AlembicLib",
                    "Core",
                    "Engine",
                    "HairStrandsCore",
                    "HairStrandsEditor",
					"CoreUObject"
                });
        }
	}
}
