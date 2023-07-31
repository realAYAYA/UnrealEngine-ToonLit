// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Diagnostics;
using UnrealBuildTool;
using EpicGames.Core;
using UnrealBuildBase;
using System.Runtime.Versioning;

namespace AutomationTool
{
	class WindowsHostPlatform : HostPlatform
	{
		[SupportedOSPlatform("windows")]
		public override string GetFrameworkMsbuildExe()
		{
			return WindowsExports.GetMSBuildToolPath();
		}

		public override string RelativeBinariesFolder
		{
			get { return @"Engine/Binaries/Win64/"; }
		}

		public override string GetUnrealExePath(string UnrealExe)
		{
			if(Path.IsPathRooted(UnrealExe))
			{
				return CommandUtils.CombinePaths(UnrealExe);
			}
			else
			{
				return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, RelativeBinariesFolder, UnrealExe);
			}
		}

		public override string LocalBuildsLogFolder
		{
			get { return CommandUtils.CombinePaths(PathSeparator.Slash, Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Unreal Engine", "AutomationTool", "Logs"); }
		}

		public override string P4Exe
		{
			get { return "p4.exe"; }
		}

		public override Process CreateProcess(string AppName)
		{
			var NewProcess = new Process();
			return NewProcess;
		}

		public override void SetupOptionsForRun(ref string AppName, ref CommandUtils.ERunOptions Options, ref string CommandLine)
		{
		}

		public override void SetConsoleCtrlHandler(ProcessManager.CtrlHandlerDelegate Handler)
		{
			ProcessManager.SetConsoleCtrlHandler(Handler, true);
		}

		public override UnrealTargetPlatform HostEditorPlatform
		{
			get { return UnrealTargetPlatform.Win64; }
		}

		public override string PdbExtension
		{
			get { return ".pdb"; }
		}

		static string[] SystemServices = new string[]
		{
			"winlogon",
			"system idle process",
			"taskmgr",
			"spoolsv",
			"csrss",
			"smss",
			"svchost",
			"services",
			"lsass",
            "conhost",
            "oobe",
            "mmc"
		};
		public override string[] DontKillProcessList
		{
			get 
			{
				return SystemServices;
			}
		}
	}
}
