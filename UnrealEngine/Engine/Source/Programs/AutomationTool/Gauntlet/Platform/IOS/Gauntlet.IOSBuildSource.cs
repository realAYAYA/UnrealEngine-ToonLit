// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;
using EpicGames.Core;

namespace Gauntlet
{
	public class IOSBuild : IBuild
	{
		public int PreferenceOrder { get { return 0; } }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public string SourceIPAPath;

		public Dictionary<string, string> FilesToInstall;

		public string PackageName;

		public BuildFlags Flags { get; protected set; }

		public string Flavor { get { return ""; } }

		public UnrealTargetPlatform Platform { get { return UnrealTargetPlatform.IOS; } }

		public IOSBuild(UnrealTargetConfiguration InConfig, string InPackageName, string InIPAPath, Dictionary<string, string> InFilesToInstall, BuildFlags InFlags)
		{
			Configuration = InConfig;
			PackageName = InPackageName;
			SourceIPAPath = InIPAPath;
			FilesToInstall = InFilesToInstall;
			Flags = InFlags;
		}

		public bool CanSupportRole(UnrealTargetRole RoleType)
		{
			if (RoleType.IsClient())
			{
				return true;
			}

			return false;
		}

		internal static IProcessResult ExecuteCommand(String Command, String Arguments)
		{
			CommandUtils.ERunOptions RunOptions = CommandUtils.ERunOptions.AppMustExist;

			if (Log.IsVeryVerbose)
			{
				RunOptions |= CommandUtils.ERunOptions.AllowSpew;
			}
			else
			{
				RunOptions |= CommandUtils.ERunOptions.NoLoggingOfRunCommand;
			}

			Log.Verbose("Executing '{0} {1}'", Command, Arguments);

			IProcessResult Result = CommandUtils.Run(Command, Arguments, Options: RunOptions);

			return Result;
		}

		// There are issues with IPA Zip64 files being created with Ionic.Zip possibly limited to when running on mono (see IOSPlatform.PackageIPA)
		// This manifests as header overflow errors, etc in 7zip, Ionic.Zip, System.IO.Compression, and OSX system unzip		
		internal static bool ExecuteIPAZipCommand(String Arguments, out String Output, String ShouldExist = "")
		{
			using (new ScopedSuspendECErrorParsing())
			{
				IProcessResult Result = ExecuteCommand("unzip", Arguments);
				Output = Result.Output;

				if (Result.ExitCode != 0)
				{
					if (!String.IsNullOrEmpty(ShouldExist))
					{
						if (!File.Exists(ShouldExist) && !Directory.Exists(ShouldExist))
						{
							Log.Error(KnownLogEvents.Gauntlet_BuildDropEvent, "unzip encountered an error or warning procesing IPA, possibly due to Zip64 issue, {File} missing", ShouldExist);
							return false;
						}
					}

					Log.Info(String.Format("unzip encountered an issue procesing IPA, possibly due to Zip64. Future steps may fail."));
				}
			}
			
			return true;
		}

		// IPA handling using ditto command, which is capable of handling IPA's > 4GB/Zip64
		internal static bool ExecuteIPADittoCommand(String Arguments, out String Output, String ShouldExist = "")
		{
			using (new ScopedSuspendECErrorParsing())
			{
				IProcessResult Result = ExecuteCommand("ditto", Arguments);
				Output = Result.Output;

				if (Result.ExitCode != 0)
				{
					if (!String.IsNullOrEmpty(ShouldExist))
					{
						if (!File.Exists(ShouldExist) && !Directory.Exists(ShouldExist))
						{
							Log.Error(String.Format("ditto encountered an error or warning procesing IPA, {0} missing", ShouldExist));
							return false;
						}
					}

					Log.Error(String.Format("ditto encountered an issue procesing IPA"));
					return false;

				}
			}
			
			return true;
		}


		private static string GetBundleIdentifier(string SourceIPA)
		{
			string Output;

			// Get a list of files in the IPA
			if (!ExecuteIPAZipCommand(String.Format("-Z1 {0}", SourceIPA), out Output))
			{
				Log.Warning(String.Format("Unable to list files for IPA {0}", SourceIPA));
				return null;
			}


			string[] Filenames = Regex.Split(Output, "\r\n|\r|\n");			
			string PList = Filenames.Where(F => Regex.IsMatch(F.ToLower().Trim(), @"(payload\/)([^\/]+)(\/info\.plist)")).FirstOrDefault();

			if (String.IsNullOrEmpty(PList))
			{
				Log.Warning(String.Format("Unable to find plist for IPA {0}", SourceIPA));
				return null;
			}

			// Get the plist info
			if (!ExecuteIPAZipCommand(String.Format("-p '{0}' '{1}'", SourceIPA, PList), out Output))
			{
				Log.Warning(String.Format("Unable to extract plist data for IPA {0}", SourceIPA));
				return null;
			}

			string PlistInfo = Output;

			// todo: plist parsing, could be better
			string PackageName = null;
			string KeyString = "<key>CFBundleIdentifier</key>";
			int KeyIndex = PlistInfo.IndexOf(KeyString);
			if (KeyIndex > 0)
			{
				int StartIdx = PlistInfo.IndexOf("<string>", KeyIndex + KeyString.Length) + "<string>".Length;
				int EndIdx = PlistInfo.IndexOf("</string>", StartIdx);
				if (StartIdx > 0 && EndIdx > StartIdx)
				{
					PackageName = PlistInfo.Substring(StartIdx, EndIdx - StartIdx);
				}
			}

			if (String.IsNullOrEmpty(PackageName))
			{
				Log.Warning(String.Format("Unable to find CFBundleIdentifier in plist info for IPA {0}", SourceIPA));
				return null;
			}

			Log.Verbose("Found bundle id: {0}", PackageName);

			return PackageName;

		}

		public static IEnumerable<IOSBuild> CreateFromPath(string InProjectName, string InPath)
		{
			string BuildPath = InPath;

			List<IOSBuild> DiscoveredBuilds = new List<IOSBuild>();

			DirectoryInfo Di = new DirectoryInfo(BuildPath);

			// find all install batchfiles
			FileInfo[] InstallFiles = Di.GetFiles("*.ipa");

			foreach (FileInfo Fi in InstallFiles)
			{	

				var UnrealConfig = UnrealHelpers.GetConfigurationFromExecutableName(InProjectName, Fi.Name);

				Log.Verbose("Pulling package data from {0}", Fi.FullName);

				string AbsPath = Fi.Directory.FullName;

				// IOS builds are always packaged, and can always replace the command line and executable as we cache the unzip'd IPA
				BuildFlags Flags = BuildFlags.Packaged | BuildFlags.CanReplaceCommandLine | BuildFlags.CanReplaceExecutable;

				if (AbsPath.Contains("Bulk"))
				{
					Flags |= BuildFlags.Bulk;
				}
				else
				{
					Flags |= BuildFlags.NotBulk;
				}

				string SourceIPAPath = Fi.FullName;
				string PackageName = GetBundleIdentifier(SourceIPAPath);

				if (String.IsNullOrEmpty(PackageName))
				{
					continue;
				}

				Dictionary<string, string> FilesToInstall = new Dictionary<string, string>();

				IOSBuild NewBuild = new IOSBuild(UnrealConfig, PackageName, SourceIPAPath, FilesToInstall, Flags);

				DiscoveredBuilds.Add(NewBuild);

				Log.Verbose("Found {0} {1} build at {2}", UnrealConfig, ((Flags & BuildFlags.Bulk) == BuildFlags.Bulk) ? "(bulk)" : "(not bulk)", AbsPath);

			}

			return DiscoveredBuilds;
		}
	}

	public class IOSBuildSource : IFolderBuildSource
	{
		public string BuildName { get { return "IOSBuildSource"; } }

		public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == UnrealTargetPlatform.IOS;
		}

		public string ProjectName { get; protected set; }

		public IOSBuildSource()
		{
		}

		public List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
		{
			// We only want iOS builds on Mac host
			if (BuildHostPlatform.Current.Platform != UnrealTargetPlatform.Mac)
			{
				return new List<IBuild>();
			}

			List<DirectoryInfo> AllDirs = new List<DirectoryInfo>();

			List<IBuild> Builds = new List<IBuild>();

			// c:\path\to\build
			DirectoryInfo PathDI = new DirectoryInfo(InPath);

			if (PathDI.Exists)
			{
				if (PathDI.Name.IndexOf("IOS", StringComparison.OrdinalIgnoreCase) >= 0)
				{
					AllDirs.Add(PathDI);
				}

				// find all directories that begin with IOS
				DirectoryInfo[] IOSDirs = PathDI.GetDirectories("IOS*", SearchOption.TopDirectoryOnly);

				AllDirs.AddRange(IOSDirs);

				List<DirectoryInfo> DirsToRecurse = AllDirs;

				// now get subdirs
				while (MaxRecursion-- > 0)
				{
					List<DirectoryInfo> DiscoveredDirs = new List<DirectoryInfo>();

					DirsToRecurse.ToList().ForEach((D) =>
					{
						DiscoveredDirs.AddRange(D.GetDirectories("*", SearchOption.TopDirectoryOnly));
					});

					AllDirs.AddRange(DiscoveredDirs);
					DirsToRecurse = DiscoveredDirs;
				}

				//IOSBuildSource BuildSource = null;

				string IOSBuildFilter = Globals.Params.ParseValue("IOSBuildFilter", "");
				foreach (DirectoryInfo Di in AllDirs)
				{
					IEnumerable<IOSBuild> FoundBuilds = IOSBuild.CreateFromPath(InProjectName, Di.FullName);

					if (FoundBuilds != null)
					{
						if (!string.IsNullOrEmpty(IOSBuildFilter))
						{
							//IndexOf used because Contains must be case-sensitive
							FoundBuilds = FoundBuilds.Where(B => B.SourceIPAPath.IndexOf(IOSBuildFilter, StringComparison.OrdinalIgnoreCase) >= 0);
						}
						Builds.AddRange(FoundBuilds);
					}
				}
			}

			return Builds;
		}

	}
}