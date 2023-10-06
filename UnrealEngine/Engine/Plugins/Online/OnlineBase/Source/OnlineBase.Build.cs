// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class OnlineBase : ModuleRules
	{
		public OnlineBase(ReadOnlyTargetRules Target) : base(Target)
		{
			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Sockets"
				}
			);

			// NOTE:  OnlineBase cannot depend on Engine!
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject"
				}
			);
		}
	}
}