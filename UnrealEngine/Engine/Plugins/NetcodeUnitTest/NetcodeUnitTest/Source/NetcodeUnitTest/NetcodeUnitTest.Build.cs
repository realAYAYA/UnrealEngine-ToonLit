// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetcodeUnitTest : ModuleRules
	{
		public NetcodeUnitTest(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateIncludePaths.Add("NetcodeUnitTest/Private");

			PublicDependencyModuleNames.AddRange
			(
				new string[]
				{
					"Core",
					"CoreUObject",
					"Engine",
					"Sockets"
				}
			);

			PrivateDependencyModuleNames.AddRange
			(
				new string[]
				{
					"ApplicationCore",
					"EngineSettings",
					"InputCore",
					"PacketHandler",
					"RenderCore",
					"Slate",
					"SlateCore",
					"NetCore"
				}
			);

			if (Target.LinkType != TargetLinkType.Monolithic)
			{
				PrivateDependencyModuleNames.AddRange
				(
					new string[]
					{
						"StandaloneRenderer"
					}
				);
			}
		}
	}
}

