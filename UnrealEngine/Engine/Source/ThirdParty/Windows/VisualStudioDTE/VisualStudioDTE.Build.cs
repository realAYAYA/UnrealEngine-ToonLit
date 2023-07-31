// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Win32;
using System.IO;
using EpicGames.Core;
using UnrealBuildTool;

namespace UnrealBuildTool.Rules
{
	public class VisualStudioDTE : ModuleRules
	{
		public VisualStudioDTE(ReadOnlyTargetRules Target) : base(Target)
		{
			Type = ModuleType.External;

			PublicIncludePaths.Add(ModuleDirectory);

			if (Target.Platform != UnrealBuildTool.UnrealTargetPlatform.Win64 ||
				Target.WindowsPlatform.Compiler == WindowsCompiler.Clang ||
				Target.StaticAnalyzer == StaticAnalyzer.PVSStudio)
			{
				PublicDefinitions.Add("WITH_VISUALSTUDIO_DTE=0");
			}
			else
			{
				// In order to support building the plugin on build machines (which may not have the IDE installed), allow using an OLB rather than registered component.
				string DteOlbPath;
				if (TryGetDteOlbPath(out DteOlbPath))
				{
					TypeLibraries.Add(new TypeLibrary(DteOlbPath, "lcid(\"0\") raw_interfaces_only named_guids", "dte80a.tlh"));
					PublicDefinitions.Add("WITH_VISUALSTUDIO_DTE=1");
				}
				else
				{
					Log.TraceWarningOnce("Unable to find Visual Studio SDK. Editor integration will be disabled");
					PublicDefinitions.Add("WITH_VISUALSTUDIO_DTE=0");
				}
			}
		}

		bool TryGetDteOlbPath(out string OutDteOlbPath)
		{
			// Check AutoSDK for the type library
			string AutoSdkDir = AutoSdkDirectory;
			if (AutoSdkDir != null)
			{
				string AutoSdkDteOlbPath = Path.Combine(AutoSdkDir, "Win64", "VisualStudioDTE", "dte80a.olb");
				if (File.Exists(AutoSdkDteOlbPath))
				{
					OutDteOlbPath = AutoSdkDteOlbPath;
					return true;
				}
			}

			// Look in the registry for the appropriate type library
			string RegistryPath = Registry.GetValue("HKEY_CLASSES_ROOT\\TypeLib\\{80CC9F66-E7D8-4DDD-85B6-D9E6CD0E93E2}\\8.0\\0\\win32", null, null) as string;
			if (RegistryPath != null && File.Exists(RegistryPath))
			{
				OutDteOlbPath = RegistryPath;
				return true;
			}

			// Fail
			OutDteOlbPath = null;
			return false;
		}
	}
}
