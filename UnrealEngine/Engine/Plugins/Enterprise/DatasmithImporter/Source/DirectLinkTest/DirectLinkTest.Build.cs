// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class DirectLinkTest : ModuleRules
	{
		public DirectLinkTest(ReadOnlyTargetRules Target) : base(Target)
		{
			// #ue_directlink_cleanup move to QAEnterprise
			PublicDependencyModuleNames.AddRange(new string[]{
				"Core",
				"DatasmithCore",
				"DatasmithTranslator",
				"DirectLink",
			});

			PrivateDependencyModuleNames.AddRange(new string[]{
				"CoreUObject",
				"Engine",
			});
		}
	}
}
