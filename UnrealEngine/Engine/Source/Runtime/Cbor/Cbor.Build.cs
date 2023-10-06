// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Cbor : ModuleRules
	{
		public Cbor(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
			);
			UnsafeTypeCastWarningLevel = WarningLevel.Error;
		}
    }
}
