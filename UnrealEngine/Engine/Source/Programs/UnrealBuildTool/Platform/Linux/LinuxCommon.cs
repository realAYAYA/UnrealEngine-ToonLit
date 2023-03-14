// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Linq;
using Microsoft.Win32;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	class LinuxCommon
	{
		public static string? Which(string name, ILogger Logger)
		{
			Process proc = new Process();
			proc.StartInfo.FileName = "/bin/sh";
			proc.StartInfo.Arguments = string.Format("-c 'which {0}'", name);
			proc.StartInfo.UseShellExecute = false;
			proc.StartInfo.CreateNoWindow = true;
			proc.StartInfo.RedirectStandardOutput = true;
			proc.StartInfo.RedirectStandardError = true;

			proc.Start();
			proc.WaitForExit();

			string? path = proc.StandardOutput.ReadLine();
			Logger.LogDebug("which {Name} result: ({ExitCode}) {Path}", name, proc.ExitCode, path);

			if (proc.ExitCode == 0 && string.IsNullOrEmpty(proc.StandardError.ReadToEnd()))
			{
				return path;
			}
			return null;
		}

		public static string? WhichClang(ILogger Logger)
		{
			string? InternalSDKPath = UEBuildPlatform.GetSDK(UnrealTargetPlatform.Linux)?.GetInternalSDKPath();
			if (!string.IsNullOrEmpty(InternalSDKPath))
			{
				return Path.Combine(InternalSDKPath, "bin", "clang++");
			}

			return Which("clang++", Logger);
		}
	}
}

