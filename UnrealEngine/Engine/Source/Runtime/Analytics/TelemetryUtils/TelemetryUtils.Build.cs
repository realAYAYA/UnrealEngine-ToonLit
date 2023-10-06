// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class TelemetryUtils : ModuleRules
	{
		public TelemetryUtils(ReadOnlyTargetRules Target) : base(Target)
		{			
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
				}
				);
			UnsafeTypeCastWarningLevel = WarningLevel.Error;
			CppStandard = CppStandardVersion.Cpp20;
		}
	}
}
