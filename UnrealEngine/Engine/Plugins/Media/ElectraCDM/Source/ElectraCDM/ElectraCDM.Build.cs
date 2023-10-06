// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class ElectraCDM : ModuleRules
	{
		public ElectraCDM(ReadOnlyTargetRules Target) : base(Target)
		{
			IWYUSupport = IWYUSupport.None; // Disabled because of third party code
			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"Json"
				});
		}
	}
}
