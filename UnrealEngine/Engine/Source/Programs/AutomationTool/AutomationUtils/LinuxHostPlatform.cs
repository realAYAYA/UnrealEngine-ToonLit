// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Diagnostics;
using System.IO;
using UnrealBuildTool;
using EpicGames.Core;
using UnrealBuildBase;

namespace AutomationTool
{
	class LinuxHostPlatform : HostPlatform
	{
		static string CachedFrameworkMsbuildTool = "";

		public override string GetFrameworkMsbuildExe()
		{
			// As of 5.0 mono comes with msbuild which performs better. If that's installed then use it
			if (string.IsNullOrEmpty(CachedFrameworkMsbuildTool))
			{
				int Value;
				bool CanUseMsBuild = (int.TryParse(Environment.GetEnvironmentVariable("UE_USE_SYSTEM_MONO"), out Value) &&
						Value != 0 &&
						!string.IsNullOrEmpty(CommandUtils.WhichApp("msbuild")));

				if (CanUseMsBuild)
				{
					CachedFrameworkMsbuildTool = "msbuild";
				}
				else
				{
					CachedFrameworkMsbuildTool = "xbuild";
				}
			}

			return CachedFrameworkMsbuildTool;
		}

		public override string RelativeBinariesFolder
		{
			get { return @"Engine/Binaries/Linux/"; }
		}

		public override string GetUnrealExePath(string UnrealExe)
		{
			if(Path.IsPathRooted(UnrealExe))
			{
				return CommandUtils.CombinePaths(UnrealExe);
			}

			int CmdExeIndex = UnrealExe.IndexOf("-Cmd.exe");
			if (CmdExeIndex != -1)
			{
				UnrealExe = UnrealExe.Substring (0, CmdExeIndex);
			}
			else
			{
				CmdExeIndex = UnrealExe.IndexOf (".exe");
				if (CmdExeIndex != -1)
				{
					UnrealExe = UnrealExe.Substring (0, CmdExeIndex);
				}
			}
			return CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, RelativeBinariesFolder, UnrealExe);
		}

		public override string LocalBuildsLogFolder
		{
			// @FIXME: should use xdg-user-dir DOCUMENTS
			get { return Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Documents/Unreal Engine/LocalBuildLogs"); }
		}

		public override string P4Exe
		{
			get { return "p4"; }
		}

		public override Process CreateProcess(string AppName)
		{
			var NewProcess = new Process();
			if (AppName == "mono")
			{
				// Enable case-insensitive mode for Mono
				if (!NewProcess.StartInfo.EnvironmentVariables.ContainsKey("MONO_IOMAP"))
				{
					NewProcess.StartInfo.EnvironmentVariables.Add("MONO_IOMAP", "case");
				}
			}
			return NewProcess;
		}

		public override void SetupOptionsForRun(ref string AppName, ref CommandUtils.ERunOptions Options, ref string CommandLine)
		{
			if (AppName == "sh" || AppName == "xbuild" || AppName == "codesign")
			{
				Options &= ~CommandUtils.ERunOptions.AppMustExist;
			}
			if (AppName == "xbuild")
			{
				AppName = "xbuild";
				CommandLine = (String.IsNullOrEmpty(CommandLine) ? "" : CommandLine) + " /verbosity:quiet /nologo";
				// Pass #define MONO to all the automation scripts
				CommandLine += " /p:DefineConstants=MONO";
				CommandLine += " /p:DefineConstants=__MonoCS__";
				// Some projects have TargetFrameworkProfile=Client which causes warnings on Linux
				// so force it to empty.
				CommandLine += " /p:TargetFrameworkProfile=";
			}
			if (AppName.EndsWith(".exe") || ((AppName.Contains("/Binaries/Win64/") || AppName.Contains("/Binaries/Linux/")) && string.IsNullOrEmpty(Path.GetExtension(AppName))))
			{
				if (AppName.Contains("/Binaries/Win64/") || AppName.Contains("/Binaries/Linux/"))
				{
					AppName = AppName.Replace("/Binaries/Win64/", "/Binaries/Linux/");
					AppName = AppName.Replace("-cmd.exe", "");
					AppName = AppName.Replace("-Cmd.exe", "");
					AppName = AppName.Replace(".exe", "");
				}
				// some of our C# applications are converted to dotnet core, do not run those via mono
				else if (AppName.Contains("UnrealBuildTool") || AppName.Contains("AutomationTool"))
				{
					Options &= ~CommandUtils.ERunOptions.AppMustExist;
				}
				else
				{
					// It's a C# app, so run it with Mono
					CommandLine = "\"" + AppName + "\" " + (String.IsNullOrEmpty(CommandLine) ? "" : CommandLine);
					AppName = "mono";
					Options &= ~CommandUtils.ERunOptions.AppMustExist;
				}
			}
		}

		public override void SetConsoleCtrlHandler(ProcessManager.CtrlHandlerDelegate Handler)
		{
			// @todo: add mono support
		}

		public override UnrealTargetPlatform HostEditorPlatform
		{
			get { return UnrealTargetPlatform.Linux; }
		}

		public override string PdbExtension
		{
			get { return ".exe.mdb"; }
		}
		static string[] SystemServices = new string[]
		{
			// TODO: Add any system process names here
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
