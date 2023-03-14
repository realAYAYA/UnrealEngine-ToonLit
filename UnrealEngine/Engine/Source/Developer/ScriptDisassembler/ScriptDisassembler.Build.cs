// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ScriptDisassembler : ModuleRules
	{
		public ScriptDisassembler(ReadOnlyTargetRules Target) : base(Target)
        {
			bRequiresImplementModule = false;

			PublicDependencyModuleNames.AddRange(new string[]
			{
				"Core",
				"CoreUObject"
			});
        }
	}
}
