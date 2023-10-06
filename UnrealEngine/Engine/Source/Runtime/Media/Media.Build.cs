// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class Media : ModuleRules
	{
		public Media(ReadOnlyTargetRules Target) : base(Target)
		{
			IWYUSupport = IWYUSupport.KeepAsIsForNow;

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"RenderCore",
					"ColorManagement",
				});

			PublicIncludePathModuleNames.AddRange(
				new string[] {
					"ColorManagement",
					"RenderCore",
				});
		}
	}
}
