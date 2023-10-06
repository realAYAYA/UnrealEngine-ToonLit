// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Linq;
using AutomationTool;
using UnrealBuildTool;
using EpicGames.Core;
using System.Text.RegularExpressions;
using System.Threading;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

[Help("Attempts to sync UGS binaries for the specified project at the currently synced CL of the project/engine folders")]
[Help("project=<FortniteGame>", "Project to sync. Will search current path and paths in ueprojectdirs.")]
[Help("preview", "If specified show actions but do not perform them.")]
[RequireP4]
[DoesNotNeedP4CL]
class SyncBinariesFromUGS : SyncProjectBase
{

	/// <summary>
	/// Project name or file that we will sync binaries for
	/// </summary>
	public string ProjectArg = "";

	/// <summary>
	/// If true no actions will be performed
	/// </summary>
	public bool Preview = false;

	public override ExitCode Execute()
	{
		// Parse the project filename (as a local path)
		ProjectArg = ParseParamValue("Project", ProjectArg);
		Preview = Preview || ParseParam("Preview");

		// Will be the full local path to the project file once resolved
		FileReference ProjectFile = ProjectUtils.FindProjectFileFromName(ProjectArg);

		if (ProjectFile == null)
		{
			throw new AutomationException("Could not find project file for {0}. Provide a full path or check the subdirectory is listed in UE4Games.uprojectdirs", ProjectArg);
		}

		// Will be the full workspace path to the project in P4
		P4WhereRecord ProjectRecord = GetP4RecordForPath(ProjectFile.FullName);

		if (ProjectRecord == null)
		{
			throw new AutomationException("Could not find a P4 record for {0} with the current workspace settings. use RunUAT P4WriteConfig to write a config file with the relevant info", ProjectFile.FullName);
		}

		// get the path to binaries in the depot
		string DepotBinaryPath = GetZippedBinaryPathFromUGSConfig(ProjectFile.Directory, ProjectRecord.DepotFile);

		// get the CL
		int CurrentChangeList = P4Env.Changelist;
		int CompatibleChangeList = P4Env.CodeChangelist;

		Logger.LogInformation("Current CL: {CurrentChangeList}, Required Compatible CL: {CompatibleChangeList}", CurrentChangeList, CompatibleChangeList);

		List<P4Connection.ChangeRecord> BinaryChanges;
		P4.Changes(out BinaryChanges, DepotBinaryPath, AllowSpew: false);

		// changes will be in ascending order, so pull out the CL's they were submitted for and return the first CL that is greater than
		// the change we need. This lets people regress issues if needed.
		int CheckedInBinaryCL = BinaryChanges.Where(R => {

			// use a regex to pull the CL the binaries were built from. The description will be something like "[CL 21611] Updated..", 
			// where 21611 is the change they were generated from
			Match M = Regex.Match(R.Summary, @"\[CL\s+(\d+)\]");

			if (M == null)
			{
				Logger.LogWarning("Change description for {Arg0} did not include expected format of [CL xxxx]", R.CL);
				return false;
			}

			int SourceCL = Convert.ToInt32(M.Groups[1].ToString());
			
			// Only interested in records greater than the code CL
			return SourceCL >= CompatibleChangeList;
		})
		.Select(R => R.CL)		// We want the CL of that record
		.FirstOrDefault();		// And we only care about the first match

		if (CheckedInBinaryCL == 0)
		{
			throw new AutomationException("Could not find a submitted CL for {0} that was for source code >= CL {1}", DepotBinaryPath, CompatibleChangeList);
		}

		string VersionedFile = string.Format("{0}@{1}", DepotBinaryPath, CheckedInBinaryCL);

		Logger.LogInformation("Will sync and extract binaries from {VersionedFile}", VersionedFile);

		if (!Preview)
		{
			string TmpFile = Path.Combine(Path.GetTempPath(), "UGS.zip");

			try
			{
				P4.PrintToFile(VersionedFile, TmpFile);

				Logger.LogInformation("Unzipping to {Arg0}", Unreal.RootDirectory);

				// we can't use helpers as unlike UGS we don't want to extract anything for UAT, since that us running us....
				// That should be fine since we are either being run from the source CL that has already been syned to, or the 
				// next time UAT is run we'll be rebuilt with that CL... 

				int FileCount = 0;
				string UATDirectory = "Engine/Binaries/DotNET";

				using (Ionic.Zip.ZipFile Zip = new Ionic.Zip.ZipFile(TmpFile))
				{
					foreach (Ionic.Zip.ZipEntry Entry in Zip.Entries.Where(x => !x.IsDirectory))
					{
						string OutputFileName = Path.Combine(Unreal.RootDirectory.FullName, Entry.FileName);

						if (Entry.FileName.Replace("\\", "/").StartsWith(UATDirectory, StringComparison.OrdinalIgnoreCase))
						{
							Logger.LogInformation("Skipping {OutputFileName} as UAT is running", OutputFileName);
						}
						else
						{
							Directory.CreateDirectory(Path.GetDirectoryName(OutputFileName));
							using (FileStream OutputStream = new FileStream(OutputFileName, FileMode.Create, FileAccess.Write))
							{
								Entry.Extract(OutputStream);
							}
							Logger.LogInformation("Extracted {OutputFileName}", OutputFileName);
							FileCount++;
						}
					}
				}

				Logger.LogInformation("Unzipped {FileCount} files", FileCount);
			}
			catch (Exception Ex)
			{
				throw new AutomationException("Failed to uinzip files: {0}", Ex.Message);
			}
			finally
			{
				try
				{
					if (File.Exists(TmpFile))
					{
						File.Delete(TmpFile);
					}
				}
				catch
				{
					Logger.LogInformation("Failed to remove tmp file {TmpFile}", TmpFile);
				}
			}

			// Update version files with our current and compatible CLs
			Logger.LogInformation("Updating Version files to CL: {CurrentChangeList} CompatibleCL: {CompatibleChangeList}", CurrentChangeList, CompatibleChangeList);
			UnrealBuild Build = new UnrealBuild(this);
			Build.UpdateVersionFiles(ActuallyUpdateVersionFiles: true, ChangelistNumberOverride: CurrentChangeList, CompatibleChangelistNumberOverride: CompatibleChangeList, IsPromotedOverride: false);
		}

		return ExitCode.Success;
	}

	/// <summary>
	/// Returns the depot path used to store zipped biaries for this project by looking in the UnrealGameSync.ini file for an entry
	/// matching the stream or depot that contains the project
	/// </summary>
	/// <param name="LocalProjectDir"></param>
	/// <param name="ProjectDepotPath"></param>
	/// <returns></returns>
	string GetZippedBinaryPathFromUGSConfig(DirectoryReference LocalProjectDir, string ProjectDepotPath)
	{
		FileReference UGSFile = FileReference.Combine(LocalProjectDir, "Build", "UnrealGameSync.ini");

		if (!FileReference.Exists(UGSFile))
		{
			throw new AutomationException("Could not find UGS configuration at {0}", UGSFile);
		}

		ConfigFile UGSConfig = new ConfigFile(UGSFile);

		if (UGSConfig.SectionNames.Contains(ProjectDepotPath) == false)
		{
			throw new AutomationException("{0} has no entry for {1}", UGSFile, ProjectDepotPath);
		}

		string BinaryPath = null;

		ConfigFileSection UGSSection;

		if (UGSConfig.TryGetSection(ProjectDepotPath, out UGSSection))
		{
			BinaryPath = UGSSection.Lines.Where(L => L.Key == "ZippedBinariesPath").Select(L => L.Value).FirstOrDefault();
		}

		if (BinaryPath == null)
		{
			throw new AutomationException("{0} does not include a ZippedBinariesPath entry for {1}", UGSFile, ProjectDepotPath);
		}

		return BinaryPath;
	}
}
