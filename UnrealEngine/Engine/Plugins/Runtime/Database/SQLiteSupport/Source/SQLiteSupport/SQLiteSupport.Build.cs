// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class SQLiteSupport : ModuleRules
	{
		public SQLiteSupport(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"DatabaseSupport",
					"SQLiteCore",
				}
			);
		}
	}
}
