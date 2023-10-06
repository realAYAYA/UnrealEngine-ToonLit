// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public abstract class DatasmithSolidworksBase : ModuleRules
	{
		public DatasmithSolidworksBase(ReadOnlyTargetRules Target) : base(Target)
		{
			bUseRTTI = true;

			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatasmithFacadeCSharp",
					"Projects"
				}
			);

			PrivateIncludePathModuleNames.Add("Launch");
		}

		public abstract string GetVersion();
	}

	[SupportedPlatforms("Win64")]
	public class DatasmithSolidworks2021 : DatasmithSolidworksBase
	{
		public DatasmithSolidworks2021(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetVersion()
		{
			return "SOLIDWORKS 2021";
		}
	}
}
