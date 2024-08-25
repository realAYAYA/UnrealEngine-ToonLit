// Copyright Epic Games, Inc. All Rights Reserved.

namespace UnrealBuildTool.Rules
{
	public class NetworkFileSystem : ModuleRules
	{
		public NetworkFileSystem(ReadOnlyTargetRules Target) : base(Target)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"CoreUObject",
					"Engine",
					"Projects",
					"SandboxFile",
					"TargetPlatform",
					"DesktopPlatform",
					"CookOnTheFly",
					"CookOnTheFlyNetServer"
				});

			PublicDependencyModuleNames.AddRange(
				new string[] {
					"Core",
					"Sockets",
				});

			if ((Target.Platform == UnrealTargetPlatform.Win64) ||
				(Target.Platform == UnrealTargetPlatform.Mac))
			{
				AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL", "libWebSockets", "zlib");
				PublicDefinitions.Add("ENABLE_HTTP_FOR_NFS=1");
				PrivateDependencyModuleNames.Add("SSL");
			}
			else
			{
				PublicDefinitions.Add("ENABLE_HTTP_FOR_NFS=0");
			}

			PrecompileForTargets = PrecompileTargetsType.Editor;

			UnsafeTypeCastWarningLevel = WarningLevel.Error;
		}
	}
}
