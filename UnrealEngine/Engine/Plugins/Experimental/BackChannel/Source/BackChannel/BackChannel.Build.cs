// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class BackChannel : ModuleRules
{
	public BackChannel( ReadOnlyTargetRules Target ) : base( Target )
	{
		PublicIncludePaths.AddRange(
			new string[] {
				System.IO.Path.Combine(ModuleDirectory, ".."),
				System.IO.Path.Combine(ModuleDirectory, "Public")
			}
		);

		PrivateDependencyModuleNames.AddRange
			(
			new string[] {
				"Core",
				"CoreUObject",
				"Engine",
				"Sockets",
				"Networking"
			}
		);
	}
}