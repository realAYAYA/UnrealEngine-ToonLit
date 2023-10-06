// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;

namespace UnrealBuildTool.Rules
{
	public abstract class DatasmithRevitBase : ModuleRules
	{
		public DatasmithRevitBase(ReadOnlyTargetRules Target)
			: base(Target)
		{
			bUseRTTI = true;
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatasmithExporter",
					"DatasmithFacadeCSharp",
					"Projects"
				}
			);

			PrivateIncludePathModuleNames.Add("Launch");
		}

		public abstract string GetRevitVersion();
	}

	[SupportedPlatforms("Win64")]
	public class DatasmithRevit2018 : DatasmithRevitBase
	{
		public DatasmithRevit2018(ReadOnlyTargetRules Target)
			: base(Target)
		{
		}

		public override string GetRevitVersion()
		{
			return "2018";
		}
	}
}
