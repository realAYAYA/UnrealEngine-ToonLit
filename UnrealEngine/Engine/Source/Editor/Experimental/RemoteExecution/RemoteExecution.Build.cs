// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class RemoteExecution : ModuleRules
	{
		public RemoteExecution(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Settings",
					"CoreUObject",
				}
			);
		}
	}
}
