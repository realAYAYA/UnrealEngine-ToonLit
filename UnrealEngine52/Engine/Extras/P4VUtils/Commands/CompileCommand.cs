// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace P4VUtils.Commands
{
	class CompileCommand : Command
	{
		public override string Description => "Compiles files in the selected changelist";

		public override CustomToolInfo CustomTool => new CustomToolInfo("Compile Files (Non-Unity)", "%c") { ShowConsole = true };

		public override async Task<int> Execute(string[] Args, IReadOnlyDictionary<string, string> ConfigValues, ILogger Logger)
		{
			int Change = -1;
			if (Args.Length < 2)
			{
				Logger.LogError("Missing changelist number");
				return 1;
			}
			else if (!Args[1].Equals("default", StringComparison.Ordinal) && !int.TryParse(Args[1], out Change))
			{
				Logger.LogError("'{Argument}' is not a numbered changelist", Args[1]);
				return 1;
			}

			using PerforceConnection Perforce = new PerforceConnection(null, null, null, Logger);
			bool Result = await BuildAsync(Perforce, Change, Args.Skip(2), Logger);
			return Result ? 0 : 1;
		}

		public static async Task<bool> BuildAsync(PerforceConnection Perforce, int Change, IEnumerable<string> AdditionalArguments, ILogger Logger)
		{
			ChangeRecord ChangeRecord = await Perforce.GetChangeAsync(GetChangeOptions.None, Change, CancellationToken.None);
			if (ChangeRecord.Files.Count == 0)
			{
				Logger.LogError("No files in selected changelist");
				return false;
			}

			List<FileReference> LocalFiles = new List<FileReference>();

			List<WhereRecord> WhereRecords = await Perforce.WhereAsync(ChangeRecord.Files.ToArray(), CancellationToken.None).ToListAsync();
			foreach (WhereRecord WhereRecord in WhereRecords)
			{
				if(WhereRecord.Path != null)
				{
					FileReference LocalFile = new FileReference(WhereRecord.Path);
					if (FileReference.Exists(LocalFile))
					{
						if (LocalFile.HasExtension(".cpp") || LocalFile.HasExtension(".h") || LocalFile.HasExtension(".c"))
						{
							LocalFiles.Add(LocalFile);
						}
					}
				}
			}

			if (LocalFiles.Count == 0)
			{
				Logger.LogError("No files to build in this changelist");
				return false;
			}

			HashSet<DirectoryReference> RootDirs = new HashSet<DirectoryReference>();
			HashSet<FileReference> ProjectFiles = new HashSet<FileReference>();

			HashSet<DirectoryReference> CheckedDirectories = new HashSet<DirectoryReference>();
			foreach (FileReference LocalFile in LocalFiles)
			{
				DirectoryReference CurrentDir = LocalFile.Directory;
				while (CheckedDirectories.Add(CurrentDir))
				{
					ProjectFiles.UnionWith(DirectoryReference.EnumerateFiles(CurrentDir, "*.uproject"));

					FileReference BatchFile = FileReference.Combine(CurrentDir, "Engine/Build/BatchFiles/Build.bat");
					if (FileReference.Exists(BatchFile))
					{
						RootDirs.Add(CurrentDir);
						break;
					}

					DirectoryReference? NextDir = CurrentDir.ParentDirectory;
					if (NextDir == null)
					{
						Logger.LogError("Unable to find engine directory for {FileName}", LocalFile);
						return false;
					}
					CurrentDir = NextDir;
				}
			}

			if (RootDirs.Count == 0)
			{
				Logger.LogError("Unable to find engine root directory");
				return false;
			}
			if (RootDirs.Count > 1)
			{
				Logger.LogError("Found multiple engine root directories for files in changelist");
				return false;
			}

			DirectoryReference RootDir = RootDirs.First();
			DirectoryReference EngineDir = DirectoryReference.Combine(RootDir, "Engine");

			List<string> Targets = new List<string>();

			DirectoryReference EngineIntermediateDir = DirectoryReference.Combine(EngineDir, "Intermediate");
			FileReference EngineFileList = FileReference.Combine(EngineIntermediateDir, "P4VUtils-Files.txt");
			if (WriteFilteredFileList(EngineFileList, LocalFiles, EngineDir, Logger))
			{
				Targets.Add($"Win64 Development -TargetType=Editor -FileList={EngineFileList.FullName.QuoteArgument()}");
			}
			
			foreach (FileReference ProjectFile in ProjectFiles)
			{
				DirectoryReference ProjectDir = ProjectFile.Directory;
				FileReference ProjectFileList = FileReference.Combine(ProjectDir, "Intermediate", "P4VUtils-Files.txt");
				if (WriteFilteredFileList(ProjectFileList, LocalFiles, ProjectDir, Logger))
				{
					Targets.Add($"Win64 Development -TargetType=Editor -FileList={ProjectFileList.FullName.QuoteArgument()} -Project={ProjectFile.FullName.QuoteArgument()}");
				}
			}

			FileReference TargetListFile = FileReference.Combine(EngineIntermediateDir, "P4VUtils-Targets.txt");
			WriteLines(TargetListFile, Targets);

			FileReference BuildBatchFile = FileReference.Combine(RootDir, @"Engine\Build\BatchFiles\Build.bat");
			StringBuilder Arguments = new StringBuilder($"{BuildBatchFile.FullName.QuoteArgument()} -TargetList={TargetListFile.FullName.QuoteArgument()}");
			foreach (string AdditionalArgument in AdditionalArguments)
			{
				Arguments.AppendFormat(" {0}", AdditionalArgument.QuoteArgument());
			}

			Logger.LogInformation("Running {Arguments}", Arguments);
			Logger.LogInformation("");

			string ShellFileName = Environment.GetEnvironmentVariable("COMSPEC") ?? "C:\\Windows\\System32\\cmd.exe";
			string ShellArguments = $"/C \"{Arguments}\"";

			using (ManagedProcessGroup Group = new ManagedProcessGroup())
			using (ManagedProcess Process = new ManagedProcess(Group, ShellFileName, ShellArguments, null, null, System.Diagnostics.ProcessPriorityClass.Normal))
			{
#pragma warning disable CA2000 // Dispose objects before losing scope
				await Process.CopyToAsync(Console.OpenStandardOutput(), CancellationToken.None);
#pragma warning restore CA2000 // Dispose objects before losing scope
			}

			return true;
		}

		static bool WriteFilteredFileList(FileReference FileList, List<FileReference> LocalFiles, DirectoryReference BaseDir, ILogger Logger)
		{
			List<FileReference> FilteredFiles = new List<FileReference>();
			foreach (FileReference LocalFile in LocalFiles)
			{
				if (LocalFile.IsUnderDirectory(BaseDir))
				{
					FilteredFiles.Add(LocalFile);
				}
			}
			if (FilteredFiles.Count > 0)
			{
				Logger.LogInformation("Files under {BaseDir}:", BaseDir);
				foreach (FileReference FilteredFile in FilteredFiles)
				{
					Logger.LogInformation("  {File}", FilteredFile);
				}
				Logger.LogInformation("");

				WriteLines(FileList, FilteredFiles.Select(x => x.FullName));
				return true;
			}
			return false;
		}

		static void WriteLines(FileReference File, IEnumerable<string> Lines)
		{
			DirectoryReference.CreateDirectory(File.Directory);
			FileReference.WriteAllLines(File, Lines);
		}
	}
}
