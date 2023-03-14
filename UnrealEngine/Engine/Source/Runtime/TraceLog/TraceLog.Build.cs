// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.IO;

public class TraceLog : ModuleRules
{
	public TraceLog(ReadOnlyTargetRules Target) : base(Target)
	{
		bRequiresImplementModule = false;
		PublicIncludePathModuleNames.Add("Core");

		UnsafeTypeCastWarningLevel = WarningLevel.Error;
	}

	// used by platform extension derived classes. probably should become a project setting at some point!
	protected void EnableTraceByDefault(ReadOnlyTargetRules Target)
	{
		if (Target.Configuration != UnrealTargetConfiguration.Shipping && Target.Type != TargetType.Program)
		{
			foreach (String Definition in Target.GlobalDefinitions)
			{
				if (Definition.Contains("UE_TRACE_ENABLED"))
				{
					// Define already set in Target.GlobalDefinitions
					return;
				}
			}
			PublicDefinitions.Add("UE_TRACE_ENABLED=1");
		}
	}
}
