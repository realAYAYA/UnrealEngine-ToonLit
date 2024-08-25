// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using System.Reflection;
using System.Linq;
using System.Text;
using AutomationTool;
using UnrealBuildTool;
using System.Collections.Concurrent;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace AutomationScripts
{
	/// <summary>
	/// Helper command used for cooking.
	/// </summary>
	/// <remarks>
	/// Command line parameters used by this command:
	/// -clean
	/// </remarks>
	public partial class Project : CommandUtils
	{
		private static string GetGenericCookCommandletParams(ProjectParams Params)
		{
			string CommandletParams = "";

			if (IsBuildMachine)
			{
				CommandletParams += " -buildmachine";
			}
			if (Params.ZenStore)
			{
				CommandletParams += " -zenstore";
			}
			if (Params.HasDDCGraph)
			{
				CommandletParams += " -ddc=" + Params.DDCGraph;
			}
			if (Params.UnversionedCookedContent)
			{
				CommandletParams += " -unversioned";
			}
			if (Params.OptionalContent)
			{
				CommandletParams += " -editoroptional";
			}
			if (Params.FastCook)
			{
				CommandletParams += " -FastCook";
			}
			if (Params.IterativeCooking)
			{
				CommandletParams += " -iterate";
			}
			if (Params.SkipCookingEditorContent)
			{
				CommandletParams += " -skipeditorcontent";
			}
			if (!String.IsNullOrEmpty(Params.CookOutputDir))
			{
				CommandletParams += " -outputdir=" + CommandUtils.MakePathSafeToUseWithCommandLine(Params.CookOutputDir);
			}
			if (!String.IsNullOrEmpty(Params.Trace))
			{
				CommandletParams += " " + Params.Trace;
			}
			if (!String.IsNullOrEmpty(Params.TraceHost))
			{
				CommandletParams += " " + Params.TraceHost;
			}
			if (!String.IsNullOrEmpty(Params.TraceFile))
			{
				CommandletParams += " " + Params.TraceFile;
			}

			// process additional cooker options
			if (Params.HasAdditionalCookerOptions)
			{
				string FormatedAdditionalCookerParams = Params.AdditionalCookerOptions.TrimStart(new char[] { '\"', ' ' }).TrimEnd(new char[] { '\"', ' ' });
				CommandletParams += " ";
				CommandletParams += FormatedAdditionalCookerParams;
			}

			// process config overrides (-ini)
			foreach (string ConfigOverrideParam in Params.ConfigOverrideParams)
			{
				CommandletParams += " -";
				CommandletParams += ConfigOverrideParam;
			}
			return CommandletParams;
		}

		public static void GetCookByTheBookCommandletParams(ProjectParams Params, out string CommandletParams, out string[] Maps, out string[] DirectoriesToCook, out string InternationalizationPreset, out string[] CulturesToCook, out string PlatformsToCook)
		{
			var PlatformsToCookSet = new HashSet<string>();
			if (!Params.NoClient)
			{
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					// Use the data platform, sometimes we will copy another platform's data
					var DataPlatformDesc = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
					string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(false, Params.Client);
					PlatformsToCookSet.Add(PlatformToCook);
				}
			}
			if (Params.DedicatedServer)
			{
				foreach (var ServerPlatform in Params.ServerTargetPlatforms)
				{
					// Use the data platform, sometimes we will copy another platform's data
					var DataPlatformDesc = Params.GetCookedDataPlatformForServerTarget(ServerPlatform);
					string PlatformToCook = Platform.Platforms[DataPlatformDesc].GetCookPlatform(true, false);
					PlatformsToCookSet.Add(PlatformToCook);
				}
			}

			PlatformsToCook = CombineCommandletParams(PlatformsToCookSet);

			if (Params.Clean.HasValue && Params.Clean.Value && !Params.IterativeCooking)
			{
				Logger.LogInformation("Cleaning cooked data.");
				CleanupCookedData(PlatformsToCookSet.ToList(), Params);
			}

			// cook the set of maps, or the run map, or nothing
			Maps = null;
			if (Params.HasMapsToCook)
			{
				Maps = Params.MapsToCook.ToArray();
				foreach (var M in Maps)
				{
					Logger.LogInformation("{Text}", "HasMapsToCook " + M.ToString());
				}
				foreach (var M in Params.MapsToCook)
				{
					Logger.LogInformation("{Text}", "Params.HasMapsToCook " + M.ToString());
				}
			}

			DirectoriesToCook = null;
			if (Params.HasDirectoriesToCook)
			{
				DirectoriesToCook = Params.DirectoriesToCook.ToArray();
			}

			InternationalizationPreset = null;
			if (Params.HasInternationalizationPreset)
			{
				InternationalizationPreset = Params.InternationalizationPreset;
			}

			CulturesToCook = null;
			if (Params.HasCulturesToCook)
			{
				CulturesToCook = Params.CulturesToCook.ToArray();
			}

			CommandletParams = GetGenericCookCommandletParams(Params);

			if (Params.KeepFileOpenLog)
			{
				CommandletParams += " -fileopenlog";
			}

			if (Params.Manifests)
			{
				CommandletParams += " -manifests";
			}
			if (Params.HasIterateSharedCookedBuild)
			{
				new SharedCookedBuild(Params).CopySharedCookedBuilds();
				CommandletParams += " -iteratesharedcookedbuild";
			}

			if (Params.CookMapsOnly)
			{
				CommandletParams += " -mapsonly";
			}
			if (Params.CookAll)
			{
				CommandletParams += " -cookall";
			}
			if (Params.HasCreateReleaseVersion)
			{
				CommandletParams += " -createreleaseversion=" + Params.CreateReleaseVersion;
			}
			if (Params.CookPartialGC)
			{
				CommandletParams += " -partialgc";
			}
			if (Params.HasMapIniSectionsToCook)
			{
				string MapIniSections = CombineCommandletParams(Params.MapIniSectionsToCook.ToArray());

				CommandletParams += " -MapIniSection=" + MapIniSections;
			}
			if (Params.HasDLCName)
			{
				CommandletParams += " -dlcname=" + Params.DLCFile.GetFileNameWithoutExtension();
				if (!Params.DLCIncludeEngineContent)
				{
					CommandletParams += " -errorOnEngineContentUse";
				}
			}
			// don't include the based on release version unless we are cooking dlc or creating a new release version
			// in this case the based on release version is used in packaging
			if (Params.HasBasedOnReleaseVersion && (Params.HasDLCName || Params.HasCreateReleaseVersion))
			{
				CommandletParams += " -basedonreleaseversion=" + Params.BasedOnReleaseVersion;
			}
			if (!String.IsNullOrEmpty(Params.CreateReleaseVersionBasePath))
			{
				CommandletParams += " -createreleaseversionroot=" + Params.CreateReleaseVersionBasePath;
			}
			if (!String.IsNullOrEmpty(Params.BasedOnReleaseVersionBasePath))
			{
				CommandletParams += " -basedonreleaseversionroot=" + Params.BasedOnReleaseVersionBasePath;
			}

			if (!Params.NoClient)
			{
				var MapsList = Maps == null ? new List<string>() : Maps.ToList();
				foreach (var ClientPlatform in Params.ClientTargetPlatforms)
				{
					var DataPlatformDesc = Params.GetCookedDataPlatformForClientTarget(ClientPlatform);
					CommandletParams += (Platform.Platforms[DataPlatformDesc].GetCookExtraCommandLine(Params));
					MapsList.AddRange((Platform.Platforms[ClientPlatform].GetCookExtraMaps()));
				}
				Maps = MapsList.ToArray();
			}

		}

		public static void Cook(ProjectParams Params)
		{
			if ((!Params.Cook && !(Params.CookOnTheFly && !Params.SkipServer)) || Params.SkipCook)
			{
				return;
			}
			Params.ValidateAndLog();

			Logger.LogInformation("********** COOK COMMAND STARTED **********");
			var StartTime = DateTime.UtcNow;

			string UEEditorExe = HostPlatform.Current.GetUnrealExePath(Params.UnrealExe);
			if (!FileExists(UEEditorExe))
			{
				throw new AutomationException("Missing " + UEEditorExe + " executable. Needs to be built first.");
			}

			if (Params.CookOnTheFly)
			{
				if (Params.HasDLCName)
				{
					throw new AutomationException("Cook on the fly doesn't support cooking dlc");
				}

				var LogFolderOutsideOfSandbox = GetLogFolderOutsideOfSandbox();
				if (!Unreal.IsEngineInstalled())
				{
					// In the installed runs, this is the same folder as CmdEnv.LogFolder so delete only in not-installed
					DeleteDirectory(LogFolderOutsideOfSandbox);
					CreateDirectory(LogFolderOutsideOfSandbox);
				}

				string COTFCommandLine = GetGenericCookCommandletParams(Params);
				if (Params.ZenStore)
				{
					COTFCommandLine += " -messaging";
				}

				var CookServerLogFile = CombinePaths(LogFolderOutsideOfSandbox, "CookServer.log");
				CookServerProcess = RunCookOnTheFlyServer(Params, Params.NoClient ? "" : CookServerLogFile, COTFCommandLine);
			}
			else
			{
				try
				{
					string CommandletParams;
					string[] DirectoriesToCook;
					string[] MapsToCook;
					string InternationalizationPreset;
					string[] CulturesToCook;
					string PlatformsToCook;

					// get the parameters to CBTB
					GetCookByTheBookCommandletParams(Params, out CommandletParams, out MapsToCook, out DirectoriesToCook, out InternationalizationPreset, out CulturesToCook, out PlatformsToCook);

					// run a blocking cook
					CookCommandlet(Params.RawProjectPath, Params.UnrealExe, MapsToCook, DirectoriesToCook, InternationalizationPreset, CulturesToCook, PlatformsToCook, CommandletParams);
				}
				catch (Exception Ex)
				{
					if (Params.IgnoreCookErrors)
					{
						Logger.LogWarning("Ignoring cook failure.");
					}
					else
					{
						throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "Cook failed.");
					}
				}

				if (Params.HasDiffCookedContentPath)
				{
					try
					{
						DiffCookedContent(Params);
					}
					catch (Exception Ex)
					{
						throw new AutomationException(ExitCode.Error_UnknownCookFailure, Ex, "Cook failed.");
					}
				}

			}


			Logger.LogInformation("Cook command time: {0:0.00} s", (DateTime.UtcNow - StartTime).TotalMilliseconds / 1000);
			Logger.LogInformation("********** COOK COMMAND COMPLETED **********");
		}

		public struct FileInfo
		{
			public FileInfo(string InFilename)
			{
				Filename = InFilename;
				FirstByteFailed = -1;
				BytesMismatch = 0;
				File1Size = 0;
				File2Size = 0;
			}
			public FileInfo(string InFilename, long InFirstByteFailed, long InBytesMismatch, long InFile1Size, long InFile2Size)
			{
				Filename = InFilename;
				FirstByteFailed = InFirstByteFailed;
				BytesMismatch = InBytesMismatch;
				File1Size = InFile1Size;
				File2Size = InFile2Size;
			}
			public string Filename;
			public long FirstByteFailed;
			public long BytesMismatch;
			public long File1Size;
			public long File2Size;
		};

		private static void DiffCookedContent(ProjectParams Params)
		{
			List<TargetPlatformDescriptor> PlatformsToCook = Params.ClientTargetPlatforms;
			string ProjectPath = Params.RawProjectPath.FullName;

			var CookedSandboxesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Cooked");

			for (int CookPlatformIndex = 0; CookPlatformIndex < PlatformsToCook.Count; ++CookPlatformIndex)
			{
				// temporary directory to save the pak file to (pak file is usually not local and on network drive)
				var TemporaryPakPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Temp", "LocalPKG");
				// extracted files from pak file
				var TemporaryFilesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Temp", "LocalFiles");



				try
				{
					Directory.Delete(TemporaryPakPath, true);
				}
				catch (Exception Ex)
				{
					if (!(Ex is System.IO.DirectoryNotFoundException))
					{
						Logger.LogInformation("{Text}", "Failed deleting temporary directories " + TemporaryPakPath + " continuing. " + Ex.GetType().ToString());
					}
				}
				try
				{
					Directory.Delete(TemporaryFilesPath, true);
				}
				catch (Exception Ex)
				{
					if (!(Ex is System.IO.DirectoryNotFoundException))
					{
						Logger.LogInformation("{Text}", "Failed deleting temporary directories " + TemporaryFilesPath + " continuing. " + Ex.GetType().ToString());
					}
				}

				try
				{

					Directory.CreateDirectory(TemporaryPakPath);
					Directory.CreateDirectory(TemporaryFilesPath);

					Platform CurrentPlatform = Platform.Platforms[PlatformsToCook[CookPlatformIndex]];

					string SourceCookedContentPath = Params.DiffCookedContentPath;

					List<string> PakFiles = new List<string>();

					string CookPlatformString = CurrentPlatform.GetCookPlatform(false, Params.Client);

					if (Path.HasExtension(SourceCookedContentPath) && (!SourceCookedContentPath.EndsWith(".pak")))
					{
						// must be a per platform pkg file try this
						CurrentPlatform.ExtractPackage(Params, Params.DiffCookedContentPath, TemporaryPakPath);

						// find the pak file
						PakFiles.AddRange(Directory.EnumerateFiles(TemporaryPakPath, Params.ShortProjectName + "*.pak", SearchOption.AllDirectories));
						PakFiles.AddRange(Directory.EnumerateFiles(TemporaryPakPath, "pakchunk*.pak", SearchOption.AllDirectories));
					}
					else if (!Path.HasExtension(SourceCookedContentPath))
					{
						// try find the pak or pkg file
						string SourceCookedContentPlatformPath = CombinePaths(SourceCookedContentPath, CookPlatformString);

						foreach (var PakName in Directory.EnumerateFiles(SourceCookedContentPlatformPath, Params.ShortProjectName + "*.pak", SearchOption.AllDirectories))
						{
							string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(PakName));
							File.Copy(PakName, TemporaryPakFilename);
							PakFiles.Add(TemporaryPakFilename);
						}

						foreach (var PakName in Directory.EnumerateFiles(SourceCookedContentPlatformPath, "pakchunk*.pak", SearchOption.AllDirectories))
						{
							string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(PakName));
							File.Copy(PakName, TemporaryPakFilename);
							PakFiles.Add(TemporaryPakFilename);
						}

						if (PakFiles.Count <= 0)
						{
							Logger.LogInformation("No Pak files found in " + SourceCookedContentPlatformPath + " :(");
						}
					}
					else if (SourceCookedContentPath.EndsWith(".pak"))
					{
						string TemporaryPakFilename = CombinePaths(TemporaryPakPath, Path.GetFileName(SourceCookedContentPath));
						File.Copy(SourceCookedContentPath, TemporaryPakFilename);
						PakFiles.Add(TemporaryPakFilename);
					}


					string FullCookPath = CombinePaths(CookedSandboxesPath, CookPlatformString);

					var UnrealPakExe = CombinePaths(CmdEnv.LocalRoot, "Engine/Binaries/Win64/UnrealPak.exe");


					foreach (var Name in PakFiles)
					{
						Logger.LogInformation("{Text}", "Extracting pak " + Name + " for comparision to location " + TemporaryFilesPath);

						string UnrealPakParams = Name + " -Extract " + " " + TemporaryFilesPath + " -ExtractToMountPoint";
						try
						{
							RunAndLog(CmdEnv, UnrealPakExe, UnrealPakParams, Options: ERunOptions.Default | ERunOptions.UTF8Output | ERunOptions.LoggingOfRunDuration);
						}
						catch (Exception Ex)
						{
							Logger.LogInformation("{Text}", "Pak failed to extract because of " + Ex.GetType().ToString());
						}
					}

					string RootFailedContentDirectory = "\\\\epicgames.net\\root\\Developers\\Daniel.Lamb";
					if (Params.ShortProjectName == "FortniteGame")
					{
						RootFailedContentDirectory = "\\\\epicgames.net\\root\\Developers\\Hongyi.Yu";
					}

					string FailedContentDirectory = CombinePaths(RootFailedContentDirectory, CommandUtils.P4Env.Branch + CommandUtils.P4Env.Changelist.ToString(), Params.ShortProjectName, CookPlatformString);

					Directory.CreateDirectory(FailedContentDirectory);

					// diff the content
					ConcurrentBag<FileInfo> FileReport = new ConcurrentBag<FileInfo>();

					List<string> AllFiles = Directory.EnumerateFiles(FullCookPath, "*.uasset", System.IO.SearchOption.AllDirectories).ToList();
					AllFiles.AddRange(Directory.EnumerateFiles(FullCookPath, "*.umap", System.IO.SearchOption.AllDirectories).ToList());
					Parallel.ForEach(AllFiles, SourceFilename =>
					{
						StringBuilder LogStringBuilder = new StringBuilder();

						string RelativeFilename = SourceFilename.Remove(0, FullCookPath.Length);

						string DestFilename = TemporaryFilesPath + RelativeFilename;

						LogStringBuilder.AppendLine("Comparing file " + RelativeFilename);

						byte[] SourceFile = null;
						try
						{
							SourceFile = File.ReadAllBytes(SourceFilename);
						}
						catch (Exception Ex)
						{
							LogStringBuilder.AppendLine("Diff cooked content failed to load source file " + SourceFilename + " Exception " + Ex.ToString());
						}

						byte[] DestFile = null;
						try
						{
							DestFile = File.ReadAllBytes(DestFilename);
						}
						catch (Exception Ex)
						{
							LogStringBuilder.AppendLine("Diff cooked content failed to load target file " + DestFilename + " Exception " + Ex.ToString());
						}

						if (SourceFile == null || DestFile == null)
						{
							Logger.LogInformation("{Text}", LogStringBuilder.ToString());
							Logger.LogError("Diff cooked content failed on file " + SourceFilename + " when comparing against " + DestFilename + " " + (SourceFile == null ? SourceFilename : DestFilename) + " file is missing");
							return;
						}

						if (SourceFile.LongLength == DestFile.LongLength)
						{
							FileInfo DiffFileInfo = new FileInfo(SourceFilename);
							DiffFileInfo.File1Size = DiffFileInfo.File2Size = SourceFile.LongLength;

							for (long Index = 0; Index < SourceFile.LongLength; ++Index)
							{
								if (SourceFile[Index] != DestFile[Index])
								{
									if (DiffFileInfo.FirstByteFailed == -1)
									{
										DiffFileInfo.FirstByteFailed = Index;
									}
									DiffFileInfo.BytesMismatch += 1;
								}
							}

							if (DiffFileInfo.BytesMismatch != 0)
							{
								FileReport.Add(DiffFileInfo);

								LogStringBuilder.AppendLine("Diff cooked content failed on file " + SourceFilename + " when comparing against " + DestFilename + " at offset " + DiffFileInfo.FirstByteFailed.ToString());
								string SavedSourceFilename = CombinePaths(FailedContentDirectory, Path.GetFileName(SourceFilename) + "Source");
								string SavedDestFilename = CombinePaths(FailedContentDirectory, Path.GetFileName(DestFilename) + "Dest");

								LogStringBuilder.AppendLine("Creating directory " + Path.GetDirectoryName(SavedSourceFilename));
								try
								{
									Directory.CreateDirectory(Path.GetDirectoryName(SavedSourceFilename));
								}
								catch (Exception E)
								{
									LogStringBuilder.AppendLine("Failed to create directory " + Path.GetDirectoryName(SavedSourceFilename) + " Exception " + E.ToString());
								}

								LogStringBuilder.AppendLine("Creating directory " + Path.GetDirectoryName(SavedDestFilename));
								try
								{
									Directory.CreateDirectory(Path.GetDirectoryName(SavedDestFilename));
								}
								catch (Exception E)
								{
									LogStringBuilder.AppendLine("Failed to create directory " + Path.GetDirectoryName(SavedDestFilename) + " Exception " + E.ToString());
								}

								bool bFailedToSaveSourceFile = !Directory.Exists(Path.GetDirectoryName(SavedSourceFilename));
								bool bFailedToSaveDestFile = !Directory.Exists(Path.GetDirectoryName(SavedDestFilename));
								if (bFailedToSaveSourceFile || bFailedToSaveDestFile)
								{
									Logger.LogInformation("{Text}", LogStringBuilder.ToString());

									if (bFailedToSaveSourceFile)
									{
										Logger.LogError("{Text}", "Failed to save source file" + SavedSourceFilename);
									}

									if (bFailedToSaveDestFile)
									{
										Logger.LogError("{Text}", "Failed to save dest file" + SavedDestFilename);
									}

									return;
								}

								LogStringBuilder.AppendLine("Content temporarily saved to " + SavedSourceFilename + " and " + SavedDestFilename + " at offset " + DiffFileInfo.FirstByteFailed.ToString());
								File.Copy(SourceFilename, SavedSourceFilename, true);
								File.Copy(DestFilename, SavedDestFilename, true);
							}
							else
							{
								LogStringBuilder.AppendLine("Content matches for " + SourceFilename + " and " + DestFilename);
							}
						}
						else
						{
							LogStringBuilder.AppendLine("Diff cooked content failed on file " + SourceFilename + " when comparing against " + DestFilename + " files are different sizes " + SourceFile.LongLength.ToString() + " " + DestFile.LongLength.ToString());

							FileInfo DiffFileInfo = new FileInfo(SourceFilename);

							DiffFileInfo.File1Size = SourceFile.LongLength;
							DiffFileInfo.File2Size = DestFile.LongLength;

							FileReport.Add(DiffFileInfo);
						}

						Logger.LogInformation("{Text}", LogStringBuilder.ToString());
					});

					Logger.LogInformation("Mismatching files:");
					foreach (var Report in FileReport)
					{
						if (Report.FirstByteFailed == -1)
						{
							Logger.LogInformation("{Text}", "File " + Report.Filename + " size mismatch: " + Report.File1Size + " VS " + Report.File2Size);
						}
						else
						{
							Logger.LogInformation("{Text}", "File " + Report.Filename + " bytes mismatch: " + Report.BytesMismatch + " first byte failed at: " + Report.FirstByteFailed + " file size: " + Report.File1Size);
						}
					}

				}
				catch (Exception Ex)
				{
					Logger.LogInformation("{Text}", "Exception " + Ex.ToString());
					continue;
				}
			}
		}

		private static void CleanupCookedData(List<string> PlatformsToCook, ProjectParams Params)
		{
			var ProjectPath = Params.RawProjectPath.FullName;
			var CookedSandboxesPath = CombinePaths(GetDirectoryName(ProjectPath), "Saved", "Cooked");
			var CleanDirs = new string[PlatformsToCook.Count];
			for (int DirIndex = 0; DirIndex < CleanDirs.Length; ++DirIndex)
			{
				CleanDirs[DirIndex] = CombinePaths(CookedSandboxesPath, PlatformsToCook[DirIndex]);
			}

			const bool bQuiet = true;
			foreach (string CleanDir in CleanDirs)
			{
				DeleteDirectory(bQuiet, CleanDir);
			}
		}
	}
}
