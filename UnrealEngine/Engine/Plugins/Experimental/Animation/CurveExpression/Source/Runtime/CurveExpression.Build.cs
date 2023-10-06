// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class CurveExpression : ModuleRules
{
	public CurveExpression(ReadOnlyTargetRules Target) : base(Target)
	{
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"AnimationCore",
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
            {
			}
		);
	}
}
