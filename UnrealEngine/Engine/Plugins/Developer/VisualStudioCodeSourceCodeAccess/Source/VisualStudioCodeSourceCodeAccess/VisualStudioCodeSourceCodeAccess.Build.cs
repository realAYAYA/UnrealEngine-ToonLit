// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using Microsoft.Win32;

namespace UnrealBuildTool.Rules
{
	public class VisualStudioCodeSourceCodeAccess : ModuleRules
	{
        public VisualStudioCodeSourceCodeAccess(ReadOnlyTargetRules Target) : base(Target)
		{
            PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"Core",
					"SourceCodeAccess",
					"DesktopPlatform",
				}
			);

			if (Target.bBuildEditor)
			{
				PrivateDependencyModuleNames.Add("HotReload");
			}

			bool bHasVisualStudioDTE;
			try
			{
				// Interrogate the Win32 registry
				string DTEKey = null;
				switch (Target.WindowsPlatform.Compiler)
				{
					case WindowsCompiler.VisualStudio2019:
						DTEKey = "VisualStudio.DTE.16.0";
						break;
					case WindowsCompiler.VisualStudio2022:
						DTEKey = "VisualStudio.DTE.17.0";
						break;
					default:
						throw new Exception("Unknown visual studio version when mapping to DTEKey: " +
						                    Target.WindowsPlatform.Compiler.ToString());
				}
				bHasVisualStudioDTE = RegistryKey.OpenBaseKey(RegistryHive.ClassesRoot, RegistryView.Registry32).OpenSubKey(DTEKey) != null;
			}
			catch
			{
				bHasVisualStudioDTE = false;
			}

			if (bHasVisualStudioDTE)
			{
				PublicDefinitions.Add("VSACCESSOR_HAS_DTE=1");
			}
			else
			{
				PublicDefinitions.Add("VSACCESSOR_HAS_DTE=0");
			}

			bBuildLocallyWithSNDBS = true;

			ShortName = "VSCSCA";
		}
	}
}
