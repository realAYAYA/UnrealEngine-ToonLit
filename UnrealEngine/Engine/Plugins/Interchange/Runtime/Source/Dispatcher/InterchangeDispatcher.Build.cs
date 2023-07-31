// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public class InterchangeDispatcher : ModuleRules
	{
		public InterchangeDispatcher(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Sockets",
				}
			);

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Json",
				}
			);

		}
	}
}
