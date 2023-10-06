// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class LocalizableMessageBlueprint : ModuleRules
	{
		public LocalizableMessageBlueprint(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"CoreUObject",
					"Engine",
					"LocalizableMessage",
					"StructUtils"
				});
		}
	}
}
