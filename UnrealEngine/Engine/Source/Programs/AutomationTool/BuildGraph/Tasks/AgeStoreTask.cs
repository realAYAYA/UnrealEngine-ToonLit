// Copyright Epic Games, Inc. All Rights Reserved.

using AutomationTool;
using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml;
using EpicGames.Core;
using UnrealBuildTool;
using System.Threading.Tasks;
using Microsoft.Extensions.Logging;

using static AutomationTool.CommandUtils;

namespace AutomationTool.Tasks
{
	/// <summary>
	/// Parameters for a task that purges data from a symbol store after a given age
	/// </summary>
	public class AgeStoreTaskParameters
	{
		/// <summary>
		/// The target platform to age symbols for.
		/// </summary>
		[TaskParameter]
		public UnrealTargetPlatform Platform;

		/// <summary>
		/// The symbol server directory.
		/// </summary>
		[TaskParameter]
		public string StoreDir;

		/// <summary>
		/// Number of days worth of symbols to keep.
		/// </summary>
		[TaskParameter]
		public int Days;

		/// <summary>
		/// The root of the build directory to check for existing buildversion named directories.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string BuildDir;

		/// <summary>
		/// A substring to match in directory file names before deleting symbols. This allows the "age store" task
		/// to avoid deleting symbols from other builds in the case where multiple builds share the same symbol server.
		/// Specific use of the filter value is determined by the symbol server structure defined by the platform toolchain.
		/// </summary>
		[TaskParameter(Optional = true)]
		public string Filter;
	}

	/// <summary>
	/// Task that strips symbols from a set of files. This task is named after the AGESTORE utility that comes with the Microsoft debugger tools SDK, but is actually a separate implementation. The main
	/// difference is that it uses the last modified time rather than last access time to determine which files to delete.
	/// </summary>
	[TaskElement("AgeStore", typeof(AgeStoreTaskParameters))]
	public class AgeStoreTask : BgTaskImpl
	{
		/// <summary>
		/// Parameters for this task
		/// </summary>
		AgeStoreTaskParameters Parameters;

		/// <summary>
		/// Construct a spawn task
		/// </summary>
		/// <param name="InParameters">Parameters for the task</param>
		public AgeStoreTask(AgeStoreTaskParameters InParameters)
		{
			Parameters = InParameters;
		}

		private static void TryDelete(DirectoryInfo Directory)
		{
			try
			{
				Directory.Delete(true);
                Logger.LogInformation("Removed '{Arg0}'", Directory.FullName);
			}
			catch
			{
				Logger.LogWarning("Couldn't delete '{Arg0}' - skipping", Directory.FullName);
			}
		}

		private static void TryDelete(FileInfo File)
		{
			try
			{
				File.Delete();
				Logger.LogInformation("Removed '{Arg0}'", File.FullName);
			}
			catch
			{
				Logger.LogWarning("Couldn't delete '{Arg0}' - skipping", File.FullName);
			}
		}

		// Checks if an existing build has a version file, returns false to NOT delete if it exists
		private static bool CheckCanDeleteFromVersionFile(HashSet<string> ExistingBuilds, DirectoryInfo Directory, FileInfo IndividualFile = null)
		{
			// check for any existing version files
			foreach (FileInfo BuildVersionFile in Directory.EnumerateFiles("*.version"))
			{
				// If the buildversion matches one of the directories in build share provided, don't delete no matter the age.
				string BuildVersion = Path.GetFileNameWithoutExtension(BuildVersionFile.Name);
				if (ExistingBuilds.Contains(BuildVersion))
				{
					// if checking for an individual file, see if the filename matches what's in the .version file.
					// these file names won't have extensions.
					if (IndividualFile != null)
					{
						string IndividualFilePath = IndividualFile.FullName;
						string FilePointerName = File.ReadAllText(BuildVersionFile.FullName).Trim();
						if(FilePointerName == Path.GetFileNameWithoutExtension(IndividualFilePath))
						{
							Logger.LogInformation("Found existing build {BuildVersion} in the BuildDir with matching individual file {IndividualFilePath} - skipping.", BuildVersion, IndividualFilePath);
							return false;
						}
					}
					// otherwise it's okay to just mark the entire folder for delete
					else
					{
						Logger.LogInformation("Found existing build {BuildVersion} in the BuildDir - skipping.", BuildVersion);
						return false;
					}
				}
			}
			return true;
		}

		private void RecurseDirectory(DateTime ExpireTimeUtc, DirectoryInfo CurrentDirectory, string[] DirectoryStructure, int Level, string Filter, HashSet<string> ExistingBuilds, bool bDeleteIndividualFiles)
		{
			// Do a file search at the last level.
			if (Level == DirectoryStructure.Length)
			{
				if (bDeleteIndividualFiles)
				{
					// Delete any file in the directory that is out of date.
					foreach (FileInfo OutdatedFile in CurrentDirectory.EnumerateFiles().Where(x => x.LastWriteTimeUtc < ExpireTimeUtc && x.Extension != ".version"))
					{
						// check to make sure this file is valid to delete
						if (CheckCanDeleteFromVersionFile(ExistingBuilds, CurrentDirectory, OutdatedFile))
						{
							TryDelete(OutdatedFile);
						}
					}
				}
				// If all files are out of date, delete the directory...
				else if (CurrentDirectory.EnumerateFiles().Where(x => x.Extension != ".version").All(x => x.LastWriteTimeUtc < ExpireTimeUtc) && CheckCanDeleteFromVersionFile(ExistingBuilds, CurrentDirectory))
				{
					TryDelete(CurrentDirectory);
				}
			}
			else
			{
				string[] Patterns = DirectoryStructure[Level].Split(';');
				foreach (var Pattern in Patterns)
				{
					string ReplacedPattern = string.Format(Pattern, Filter);

					foreach (var ChildDirectory in CurrentDirectory.GetDirectories(ReplacedPattern, SearchOption.TopDirectoryOnly))
					{
						RecurseDirectory(ExpireTimeUtc, ChildDirectory, DirectoryStructure, Level + 1, Filter, ExistingBuilds, bDeleteIndividualFiles);
					}
				}

				// Delete this directory if it is empty, and it is not the root directory.
				if (Level > 0 && !CurrentDirectory.EnumerateFileSystemInfos().Any())
					TryDelete(CurrentDirectory);
			}
		}

		/// <summary>
		/// Execute the task.
		/// </summary>
		/// <param name="Job">Information about the current job</param>
		/// <param name="BuildProducts">Set of build products produced by this node.</param>
		/// <param name="TagNameToFileSet">Mapping from tag names to the set of files they include</param>
		public override Task ExecuteAsync(JobContext Job, HashSet<FileReference> BuildProducts, Dictionary<string, HashSet<FileReference>> TagNameToFileSet)
		{
			// Get the list of symbol file name patterns from the platform.
			Platform TargetPlatform = Platform.GetPlatform(Parameters.Platform);
			string[] DirectoryStructure = TargetPlatform.SymbolServerDirectoryStructure;
			if (DirectoryStructure == null)
			{
				throw new AutomationException("Platform does not specify the symbol server structure. Cannot age the symbol server.");
			}

			string Filter = string.IsNullOrWhiteSpace(Parameters.Filter)
				? string.Empty
				: Parameters.Filter.Trim();

			// Eumerate the root directory of builds for buildversions to check against
			// Folder names in the root directory should match the name of the .version files
			HashSet<string> ExistingBuilds = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
			if(!string.IsNullOrWhiteSpace(Parameters.BuildDir))
			{
				DirectoryReference BuildDir = new DirectoryReference(Parameters.BuildDir);
				if(DirectoryReference.Exists(BuildDir))
				{
					foreach (string BuildName in DirectoryReference.EnumerateDirectories(BuildDir).Select(Build => Build.GetDirectoryName()))
					{
						ExistingBuilds.Add(BuildName);
					}
				}
				else
				{
					Logger.LogWarning("BuildDir of {Arg0} was provided but it doesn't exist! Will not check buildversions against it.", Parameters.BuildDir);
				}
			}

			// Get the time at which to expire files
			DateTime ExpireTimeUtc = DateTime.UtcNow - TimeSpan.FromDays(Parameters.Days);
            Logger.LogInformation("Expiring all files before {ExpireTimeUtc}...", ExpireTimeUtc);

			// Scan the store directory and delete old symbol files
			DirectoryReference SymbolServerDirectory = ResolveDirectory(Parameters.StoreDir);
			CommandUtils.OptionallyTakeLock(TargetPlatform.SymbolServerRequiresLock, SymbolServerDirectory, TimeSpan.FromMinutes(15), () =>
			{
				RecurseDirectory(ExpireTimeUtc, new DirectoryInfo(SymbolServerDirectory.FullName), DirectoryStructure, 0, Filter, ExistingBuilds, TargetPlatform.SymbolServerDeleteIndividualFiles);
			});
			return Task.CompletedTask;
		}

		/// <summary>
		/// Output this task out to an XML writer.
		/// </summary>
		public override void Write(XmlWriter Writer)
		{
			Write(Writer, Parameters);
		}

		/// <summary>
		/// Find all the tags which are used as inputs to this task
		/// </summary>
		/// <returns>The tag names which are read by this task</returns>
		public override IEnumerable<string> FindConsumedTagNames()
		{
			yield break;
		}

		/// <summary>
		/// Find all the tags which are modified by this task
		/// </summary>
		/// <returns>The tag names which are modified by this task</returns>
		public override IEnumerable<string> FindProducedTagNames()
		{
			yield break;
		}
	}
}
