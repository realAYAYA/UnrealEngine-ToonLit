// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace UnrealBuildTool.Rules
{
	public class SecuritySandbox : ModuleRules
	{
		public SecuritySandbox(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"ApplicationCore"
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"DeveloperSettings"
				}
			);
		}
	}
}
