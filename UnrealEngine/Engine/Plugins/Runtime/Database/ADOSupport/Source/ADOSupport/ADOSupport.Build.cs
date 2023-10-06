// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.IO;

namespace UnrealBuildTool.Rules
{
	public class ADOSupport: ModuleRules
	{
		public ADOSupport(ReadOnlyTargetRules Target) : base(Target)
		{
			AddEngineThirdPartyPrivateStaticDependencies(Target, "ADO");

			if (Target.Platform == UnrealTargetPlatform.Win64 &&
				Target.WindowsPlatform.Compiler != WindowsCompiler.Clang &&
				Target.StaticAnalyzer != StaticAnalyzer.PVSStudio)
			{
				string MsAdo15 = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.CommonProgramFiles), "System", "ADO", "msado15.dll");
				TypeLibraries.Add(new TypeLibrary(MsAdo15, "rename(\"EOF\", \"ADOEOF\")", "msado15.tlh"));
			}

			PublicDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"DatabaseSupport",
					// ... add other public dependencies that you statically link with here ...
				}
				);
		}
	}
}
