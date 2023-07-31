// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class PixWinPlugin : ModuleRules
	{
		public PixWinPlugin(ReadOnlyTargetRules Target) : base(Target)
        {
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"ApplicationCore",
				"Core",
				"InputCore",
				"RenderCore",
				"InputDevice",
				"RHI",
			});

			if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
			{
				PrivateDependencyModuleNames.Add("WinPixEventRuntime");
				AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");
			}
		}
	}
}
