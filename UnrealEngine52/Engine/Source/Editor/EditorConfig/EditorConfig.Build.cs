// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class EditorConfig : ModuleRules
	{
        public EditorConfig(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
                new string[]
                {
                    "Core",
					"CoreUObject",
					"EditorSubsystem",
					"Engine",
					"Json",
					"UnrealEd",
				} );
        }
	}
}
