// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class JsonSerialization : ModuleRules
{
	public JsonSerialization(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Json"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Serialization",
			}
		);
	}
}
