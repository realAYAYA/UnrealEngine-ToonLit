// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public abstract class DatasmithNavisworksBase : ModuleRules
	{
		public DatasmithNavisworksBase(ReadOnlyTargetRules Target)
			: base(Target)
		{
			bUseRTTI = true;
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatasmithExporter",
				}
			);

			PrivateIncludePaths.AddRange(
				new string[]
				{
					"Runtime/Launch/Public",
					"Runtime/Launch/Private",
					ModuleDirectory
				}
			);
		}

		public abstract string GetNavisworksVersion();
	}

	[SupportedPlatforms("Win64")]
	public class DatasmithNavisworks2020 : DatasmithNavisworksBase
	{
		public DatasmithNavisworks2020(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetNavisworksVersion()
		{
			return "2020";
		}
	}
}
