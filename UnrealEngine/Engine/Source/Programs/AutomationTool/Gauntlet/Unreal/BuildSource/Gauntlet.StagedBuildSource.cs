// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using AutomationTool;
using UnrealBuildTool;
using System.Threading;
using System.Text.RegularExpressions;
using System.Linq;

namespace Gauntlet
{

	public class EditorBuild : IBuild 
	{
		public UnrealTargetPlatform Platform { get { return BuildHostPlatform.Current.Platform; } }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public BuildFlags Flags { get { return BuildFlags.CanReplaceCommandLine | BuildFlags.Loose; } }

		public bool CanSupportRole(UnrealTargetRole InRoleType) { return InRoleType.UsesEditor(); }

		public string ExecutablePath { get; protected set; }

		public EditorBuild(string InExecutablePath, UnrealTargetConfiguration InConfig)
		{
			ExecutablePath = InExecutablePath;
			Configuration = InConfig;
		}
	}

	public class PackagedBuild : IBuild
	{
		public UnrealTargetPlatform Platform { get; protected set; }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public UnrealTargetRole Role { get; protected set; }

		public BuildFlags Flags { get; protected set; }

		public string BuildPath { get; protected set; }

		public virtual bool CanSupportRole(UnrealTargetRole InRoleType)
		{
			if (InRoleType.IsEditor())
			{
				return Role.IsCookedEditor();
			}
			return InRoleType == Role;
		}

		public PackagedBuild(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfig, UnrealTargetRole InRole, string InBuildPath, BuildFlags InFlags)
		{
			Platform = InPlatform;
			Configuration = InConfig;
			Role = InRole;
			BuildPath = InBuildPath;
			Flags = InFlags | BuildFlags.Packaged;	// always add this.
		}
	}


	public class StagedBuild : IBuild
	{
		public UnrealTargetPlatform Platform { get; protected set; }

		public UnrealTargetConfiguration Configuration { get; protected set; }

		public UnrealTargetRole Role { get; protected set; }

		public BuildFlags Flags { get; protected set; }

		public string BuildPath { get; protected set; }

		public string ExecutablePath { get; protected set; }

		public virtual bool CanSupportRole(UnrealTargetRole InRoleType)
		{
			if (InRoleType.IsEditor())
			{
				return Role.IsCookedEditor();
			}
			return InRoleType == Role;
		}

		public StagedBuild(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfig, UnrealTargetRole InRole, string InBuildPath, string InExecutablePath)
		{
			Platform = InPlatform;
			Configuration = InConfig;
			Role = InRole;
			BuildPath = InBuildPath;
			ExecutablePath = InExecutablePath;
			Flags = BuildFlags.CanReplaceCommandLine | BuildFlags.CanReplaceExecutable | BuildFlags.Loose;
		}

		enum InstallStatus
		{
			Error,
			Installing,
			Installed
		}

		class Install
		{
			public InstallStatus Status;
			public string Path;

			public Install(InstallStatus InStatus, string InPath)
			{
				Status = InStatus;
				Path = InPath;
			}
		}

		static Dictionary<StagedBuild, Install> LocalInstalls = new Dictionary<StagedBuild, Install>();

		/// <summary>
		/// When running parallel tests using staged builds, local desktop devices only need one copy of client/server 
		/// this method is used to coordinate the copy across multiple local devices
		/// </summary>
		public static string InstallBuildParallel(UnrealAppConfig AppConfig, StagedBuild InBuild, string BuildPath, string DestPath, string Desc)
		{
			// In parallel tests, we only want to copy the client/server once
			bool Install = false;
			InstallStatus Status = InstallStatus.Error;

			lock (Globals.MainLock)
			{
				if (!LocalInstalls.ContainsKey(InBuild))
				{
					Install = true;
					Status = InstallStatus.Installing;
					LocalInstalls[InBuild] = new Install(Status, DestPath);
				}
				else
				{
					Status = LocalInstalls[InBuild].Status;
					DestPath = LocalInstalls[InBuild].Path;
				}
			}

			// check if we've already errored in another thread
			if (Status == InstallStatus.Error)
			{
				throw new AutomationException("Parallel build error installing {0} to {1}", BuildPath, DestPath);
			}

			if (Install)
			{
				try
				{
					Log.Info("Installing {0} to {1}", AppConfig.Name, Desc);
					Log.Verbose("\tCopying {0} to {1}", BuildPath, DestPath);
					Utils.SystemHelpers.CopyDirectory(BuildPath, DestPath, Utils.SystemHelpers.CopyOptions.Mirror);
				}
				catch
				{
					lock (Globals.MainLock)
					{
						LocalInstalls[InBuild].Status = InstallStatus.Error;
					}

					throw;
				}

				lock (Globals.MainLock)
				{
					LocalInstalls[InBuild].Status = InstallStatus.Installed;
				}

			}
			else
			{
				DateTime StartTime = DateTime.Now;
				while (true)
				{
					if ((DateTime.Now - StartTime).TotalMinutes > 60)
					{
						throw new AutomationException("Parallel build error installing {0} to {1}, timed out after an hour", BuildPath, DestPath);
					}

					Thread.Sleep(1000);

					lock (Globals.MainLock)
					{
						Status = LocalInstalls[InBuild].Status;
					}

					// install process failed in other thread, disk full, etc
					if (Status == InstallStatus.Error)
					{
						throw new AutomationException("Error installing parallel build from {0} to {1}", BuildPath, DestPath);
					}

					if (Status == InstallStatus.Installed)
					{
						Log.Verbose("Parallel build successfully installed from {0} to {1}", BuildPath, DestPath);
						break;
					}
				}
			}

			return DestPath;
		}

		public static IEnumerable<T> CreateFromPath<T>(UnrealTargetPlatform InPlatform, string InProjectName, string InPath, string InExecutableExtension)
			where T : StagedBuild
		{
			string BuildPath = InPath;

			List<T> DiscoveredBuilds = new List<T>();

			// Turn FooGame into just Foo as we need to check for client/server builds too
			string ShortName = Regex.Replace(InProjectName, "Game", "", RegexOptions.IgnoreCase);

			string ContentPath = Path.Combine(InPath, InProjectName, "Content");

			if (Directory.Exists(ContentPath))
			{
				string EngineBinaryPath = Path.Combine(InPath, "Engine", "Binaries", InPlatform.ToString());
				string GameBinaryPath = Path.Combine(InPath, InProjectName, "Binaries", InPlatform.ToString());

				// Executable will either be Project*.exe or for content-only UnrealGame.exe
				List<string> ExecutableMatches = new List<string>
				{
					ShortName + "*" + InExecutableExtension,
					"UnrealGame*" + InExecutableExtension,
				};
				foreach (KeyValuePair<string, UnrealTargetRole> ModuleAndRole in UnrealHelpers.CustomModuleToRoles)
				{
					ExecutableMatches.Add(string.Format("{0}*{1}", ModuleAndRole.Key, InExecutableExtension));
				}
				
				// check 
				// 1) Path/Project/Binaries/Platform
				// 2) Path (content only builds on some platforms write out a stub exe here)
				// 3) path/Engine/Binaries/Platform 

				string[] ExecutablePaths = new string[]
				{
					Path.Combine(InPath, InProjectName, "Binaries", InPlatform.ToString()),
					Path.Combine(InPath, "Engine", "Binaries", InPlatform.ToString()),
					InPath,
				};

				List<FileSystemInfo> Binaries = new List<FileSystemInfo>();

				foreach (var BinaryPath in ExecutablePaths)
				{
					if (Directory.Exists(BinaryPath))
					{
						DirectoryInfo Di = new DirectoryInfo(BinaryPath);

						foreach (var FileMatch in ExecutableMatches)
						{
							// Look at files & directories since apps on Mac are bundles
							FileSystemInfo[] AppFiles = Di.GetFileSystemInfos(FileMatch);
							Binaries.AddRange(AppFiles);
						}
					}
				}

				foreach (FileSystemInfo App in Binaries)
				{
					UnrealTargetConfiguration Config = UnrealHelpers.GetConfigurationFromExecutableName(InProjectName, App.Name);
					UnrealTargetRole Role = UnrealHelpers.GetRoleFromExecutableName(InProjectName, App.Name);

					if (Config != UnrealTargetConfiguration.Unknown && Role != UnrealTargetRole.Unknown && !DiscoveredBuilds.Any(B => B.Configuration == Config && B.Role == Role))
					{
						// store the exe path as relative to the staged dir path
						T NewBuild = Activator.CreateInstance(typeof(T), new object[] { InPlatform, Config, Role, InPath, Utils.SystemHelpers.MakePathRelative(App.FullName, InPath) }) as T;

						if (App.Name.StartsWith("UnrealGame", StringComparison.OrdinalIgnoreCase))
						{
							NewBuild.Flags |= BuildFlags.ContentOnlyProject;
						}

						DiscoveredBuilds.Add(NewBuild);
					}
				}
			}

			return DiscoveredBuilds;
		}

		public static void CleanupInstalls()
		{
			lock (Globals.MainLock)
			{
				LocalInstalls.Clear();
			}
		}
	}


	public abstract class StagedBuildSource<T> : IFolderBuildSource 
		where T : StagedBuild
	{
		public abstract string BuildName { get; }

		public abstract UnrealTargetPlatform Platform { get; }

		public abstract string PlatformFolderPrefix { get; }

		virtual public bool CanSupportPlatform(UnrealTargetPlatform InPlatform)
		{
			return InPlatform == Platform; 
		}

		virtual public string ExecutableExtension
		{
			get { return AutomationTool.Platform.GetExeExtension(Platform); }
		}

		public virtual List<IBuild> GetBuildsAtPath(string InProjectName, string InPath, int MaxRecursion = 3)
		{
			List<DirectoryInfo> AllDirs = new List<DirectoryInfo>();

			List<IBuild> Builds = new List<IBuild>();

			// c:\path\to\build
			DirectoryInfo PathDI = new DirectoryInfo(InPath);

			if (PathDI.Exists)
			{
				// If the path is directly to a platform folder, add us
				if (PathDI.Name.IndexOf(PlatformFolderPrefix, StringComparison.OrdinalIgnoreCase) >= 0)
				{
					AllDirs.Add(PathDI);
				}

				// Assume it's a folder of all build types so find all sub directories that begin with the foldername for this platform
				IEnumerable<DirectoryInfo> MatchingDirs = PathDI.GetDirectories("*", SearchOption.TopDirectoryOnly);

				MatchingDirs = MatchingDirs.Where(D => D.Name.StartsWith(PlatformFolderPrefix, StringComparison.OrdinalIgnoreCase)).ToArray();

				AllDirs.AddRange(MatchingDirs);

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

				// every directory that contains a valid build should have at least a ProjectName folder
				AllDirs = AllDirs.Where(D =>
				{
					var SubDirs = D.GetDirectories();
					return SubDirs.Any(SD => SD.Name.Equals(InProjectName, StringComparison.OrdinalIgnoreCase));

				}).ToList();

				foreach (DirectoryInfo Di in AllDirs)
				{
					IEnumerable<IBuild> FoundBuilds = StagedBuild.CreateFromPath<T>(Platform, InProjectName, Di.FullName, ExecutableExtension);

					if (FoundBuilds != null)
					{
						Builds.AddRange(FoundBuilds);
					}
				}
			}

			return Builds;
		}
	}

}
