// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class HordeExecutor : ModuleRules
	{
		public HordeExecutor(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
				"Core",
				"CoreUObject",
				"Settings",
				"HTTP",
				"RemoteExecution",
				}
			);
		}
	}
}
