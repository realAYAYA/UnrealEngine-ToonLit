// Copyright Epic Games, Inc.All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Net;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using AutomationTool;
using FileLockInfo;

#pragma warning disable SYSLIB0014

namespace LyraDeployment
{
	// Documentation
	// https://dev.epicgames.com/docs/services/en-US/EpicGamesStore/TechFeaturesConfig/BPTInstructionsSPT/index.html

	public class DeployToEpicGameStore : BuildCommand
	{
		static string DownloadBuildPackageTool()
		{
			// Download build package tool 1.5
			string BuildPackageToolUrl = "https://launcher-public-service-prod.ol.epicgames.com/launcher/api/installer/download/BuildPatchTool.zip";

			var BuildPathToolDownloadPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "BuildPatchTool.zip");
			var BuildPathToolInstallPath = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "BPT");
			var BuildPathToolPath = CommandUtils.CombinePaths(BuildPathToolInstallPath, "Engine/Binaries/Win64/BuildPatchTool.exe");

			if (Directory.Exists(BuildPathToolInstallPath))
			{
				Directory.Delete(BuildPathToolInstallPath, true);
			}

			using (var client = new WebClient())
			{
				client.DownloadFile(BuildPackageToolUrl, BuildPathToolDownloadPath);
			}

			// Decompress the zip file to the target directory
			CommandUtils.LogInformation("Decompressing to {0}...", BuildPathToolInstallPath);
			CommandUtils.LegacyUnzipFiles(BuildPathToolDownloadPath, BuildPathToolInstallPath);
			CommandUtils.DeleteFile(BuildPathToolDownloadPath);

			// Make sure the file we need exists.
			CommandUtils.FileExists(false, BuildPathToolPath);

			return BuildPathToolPath;
		}

		public DeployToEpicGameStore()
		{
		}

		public override void ExecuteBuild()
		{
			LogInformation("*************************");

			var LogInstanceId = Guid.NewGuid().ToString("N");
			var LogFileDirectory = CommandUtils.CombinePaths(CommandUtils.CmdEnv.LocalRoot, "Saved", "Logs", "BuildPatchToolLog");

			var BuildPathToolPath = DownloadBuildPackageTool();

			string ArtifactId = ParseParamValue("ArtifactId", null);
			string BuildRoot = ParseParamValue("BuildRoot", null);
			string CloudDir = ParseParamValue("CloudDir", null);
			string BuildVersion = ParseParamValue("BuildVersion", null);
			string AppLaunch = ParseParamValue("AppLaunch", null);
			string AppArgs = ParseParamValue("AppArgs", null);
			string FileAttributeList = ParseParamValue("FileAttributeList", null);
			string FileIgnoreList = ParseParamValue("FileIgnoreList", null);
			string Platform = ParseParamValue("Platform", null);
			string Label = ParseParamValue("Label", null);
			string CommandLineFile = ParseParamValue("CommandLineFile", null);

			if (Directory.Exists(CloudDir))
			{
				Directory.Delete(CloudDir, true);
			}

			// Verify nothing is locked before moving on.
			{
				CommandUtils.LogInformation("Are Build Files Are Unlocked: Starting");

				string[] FilesInBuildRoot = Directory.GetFiles(BuildRoot);

				// We need to make sure nothing is locking the build files.
				Stopwatch stopwatch = new Stopwatch();
				stopwatch.Start();
				while (stopwatch.Elapsed.TotalSeconds < 30)
				{
					List<Process> LockingProcesses = Win32Processes.GetProcessesLockingFiles(FilesInBuildRoot);
					if (LockingProcesses.Count > 0)
					{
						string LockingProcessesString = string.Join(", ", LockingProcesses.Select(c => c.ProcessName));
						CommandUtils.LogInformation("The following processes are locking files, {0}", LockingProcessesString);
						Thread.Sleep(2000);
					}
				}
				stopwatch.Stop();

				CommandUtils.LogInformation("Are Build Files Are Unlocked: Done");
			}

			CommandUtils.LogInformation("Running BuildPatchTool Patch Generation on {0}", ArtifactId);
			{
				string Args = "";
				Args += Arg("mode", "PatchGeneration");
				Args += Arg("ArtifactId", ArtifactId);
				Args += Arg("BuildRoot", Path.GetFullPath(BuildRoot));
				Args += Arg("CloudDir", Path.GetFullPath(CloudDir));
				Args += Arg("BuildVersion", BuildVersion);
				Args += Arg("AppLaunch", AppLaunch);
				Args += Arg("AppArgs", AppArgs);
				Args += Arg("FileAttributeList", FileAttributeList);
				Args += Arg("FileIgnoreList", FileIgnoreList);

				if (!String.IsNullOrEmpty(CommandLineFile))
				{
					Args += " " + File.ReadAllText(Path.GetFullPath(CommandLineFile));
				}

				// Add -stdout so that Horde gets logging.
				Args += Arg("stdout");

				//string LogFileName = String.Concat("BuildPatchTool-PatchGeneration-", LogInstanceId);
				//string LogFilePath = Path.Combine(LogFileDirectory, String.Concat(LogFileName, ".stdout.log"));

				try
				{
					CommandUtils.RunAndLog(BuildPathToolPath, Args);
				}
				catch (AutomationException Ex)
				{
					CommandUtils.LogError(Ex.ToString());

					//if (File.Exists(LogFilePath))
					//{
					//	CommandUtils.LogInformation(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
					//	CommandUtils.LogInformation(File.ReadAllText(LogFilePath));
					//	CommandUtils.LogInformation(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
					//}

					throw Ex;
				}
			}
			CommandUtils.LogInformation("BuildPatchTool Patch Generation Done!");

			CommandUtils.LogInformation("Running BuildPatchTool LabelBuild on {0}", ArtifactId);
			{
				string Args = "";
				Args += Arg("mode", "LabelBuild");
				Args += Arg("ArtifactId", ArtifactId);
				Args += Arg("BuildRoot", Path.GetFullPath(BuildRoot));
				Args += Arg("CloudDir", Path.GetFullPath(CloudDir));
				Args += Arg("BuildVersion", BuildVersion);
				Args += Arg("Platform", Platform);
				Args += Arg("Label", Label);

				if (!String.IsNullOrEmpty(CommandLineFile))
				{
					Args += " " + File.ReadAllText(Path.GetFullPath(CommandLineFile));
				}

				// Add -stdout so that Horde gets logging.
				Args += Arg("stdout");

				//string LogFileName = String.Concat("BuildPatchTool-LabelBuild-", LogInstanceId);
				//string LogFilePath = Path.Combine(LogFileDirectory, String.Concat(LogFileName, ".stdout.log"));

				try
				{
					CommandUtils.RunAndLog(BuildPathToolPath, Args/*, LogFilePath*/);
				}
				catch (AutomationException Ex)
				{
					CommandUtils.LogError(Ex.ToString());

					//if (File.Exists(LogFilePath))
					//{
					//	CommandUtils.LogInformation(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
					//	CommandUtils.LogInformation(File.ReadAllText(LogFilePath));
					//	CommandUtils.LogInformation(">>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>");
					//}

					throw Ex;
				}
			}
			CommandUtils.LogInformation("BuildPatchTool Patch Generation Done!");


			LogInformation("*************************");
		}

		private static string Arg(string Key, string Value)
		{
			return Value == null ? "" : " -" + Key + "=\"" + Value + "\"";
		}
		private static string Arg(string Key, int Value)
		{
			return " -" + Key + "=" + Value;
		}
		private static string Arg(string Key, ulong Value)
		{
			return " -" + Key + "=" + Value;
		}
		private static string Arg(string Key)
		{
			return " -" + Key;
		}

		private static string Arg(string Key, IEnumerable<string> Value)
		{
			return " -" + Key + "=\"" + string.Join(",", Value) + "\"";
		}
		private static string CustomArgs<T>(string ArgName, List<KeyValuePair<string, T>> CustomArgs)
		{
			var Result = new StringBuilder();
			if (CustomArgs != null)
			{
				foreach (var CustomArg in CustomArgs)
				{
					Result.Append(string.Format(" -{0}=\"{1}={2}\"", ArgName, CustomArg.Key, CustomArg.Value.ToString()));
				}
			}
			return Result.ToString();
		}
	}
}
