// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;

namespace UnrealBuildBase
{
	public static class Unreal
	{
		private static DirectoryReference FindRootDirectory()
		{
			if (LocationOverride.RootDirectory != null)
			{
				return DirectoryReference.FindCorrectCase(LocationOverride.RootDirectory);
			}

			string? OverrideArg = Environment.GetCommandLineArgs().FirstOrDefault(x => x?.StartsWith("-rootdirectory=") ?? false, null);
			if (OverrideArg != null)
			{
				string[] Parts = OverrideArg.Split('=', 2);
				return new DirectoryReference(Path.GetFullPath(Parts[1]));
			}

			// This base library may be used - and so be launched - from more than one location (at time of writing, UnrealBuildTool and AutomationTool)
			// Programs that use this assembly must be located under "Engine/Binaries/DotNET" and so we look for that sequence of directories in that path of the executing assembly

			// Use the EntryAssembly (the application path), rather than the ExecutingAssembly (the library path)
			string AssemblyLocation = Assembly.GetEntryAssembly()!.GetOriginalLocation();

			DirectoryReference? FoundRootDirectory = DirectoryReference.FindCorrectCase(DirectoryReference.FromString(AssemblyLocation)!);

			// Search up through the directory tree for the deepest instance of the sub-path "Engine/Binaries/DotNET"
			while (FoundRootDirectory != null)
			{
				if (String.Equals("DotNET", FoundRootDirectory.GetDirectoryName()))
				{
					FoundRootDirectory = FoundRootDirectory.ParentDirectory;
					if (FoundRootDirectory != null && String.Equals("Binaries", FoundRootDirectory.GetDirectoryName()))
					{
						FoundRootDirectory = FoundRootDirectory.ParentDirectory;
						if (FoundRootDirectory != null && String.Equals("Engine", FoundRootDirectory.GetDirectoryName()))
						{
							FoundRootDirectory = FoundRootDirectory.ParentDirectory;
							break;
						}
						continue;
					}
					continue;
				}
				FoundRootDirectory = FoundRootDirectory.ParentDirectory;
			}

			if (FoundRootDirectory == null)
			{
				throw new Exception($"This code requires that applications using it are launched from a path containing \"Engine/Binaries/DotNET\". This application was launched from {Path.GetDirectoryName(AssemblyLocation)}");
			}

			// Confirm that we've found a valid root directory, by testing for the existence of a well-known file
			FileReference ExpectedExistingFile = FileReference.Combine(FoundRootDirectory, "Engine", "Build", "Build.version");
			if (!FileReference.Exists(ExpectedExistingFile))
			{
				throw new Exception($"Expected file \"Engine/Build/Build.version\" was not found at {ExpectedExistingFile.FullName}");
			}

			return FoundRootDirectory;
		}

		private static FileReference FindUnrealBuildToolDll()
		{
			// UnrealBuildTool.dll is assumed to be located under {RootDirectory}/Engine/Binaries/DotNET/UnrealBuildTool/
			FileReference UnrealBuildToolDllPath = FileReference.Combine(EngineDirectory, "Binaries", "DotNET", "UnrealBuildTool", "UnrealBuildTool.dll");

			UnrealBuildToolDllPath = FileReference.FindCorrectCase(UnrealBuildToolDllPath);

			if (!FileReference.Exists(UnrealBuildToolDllPath))
			{
				throw new Exception($"Unable to find UnrealBuildTool.dll in the expected location at {UnrealBuildToolDllPath.FullName}");
			}

			return UnrealBuildToolDllPath;
		}

		static private string DotnetVersionDirectory = "6.0.302";

		static private string FindRelativeDotnetDirectory(RuntimePlatform.Type HostPlatform)
		{
			string HostDotNetDirectoryName;
			switch (HostPlatform)
			{
				case RuntimePlatform.Type.Windows:
					{
						HostDotNetDirectoryName = "windows";
						if (RuntimeInformation.ProcessArchitecture == Architecture.Arm64)
						{
							HostDotNetDirectoryName = "win-arm64";
						}
						break;
					}
				case RuntimePlatform.Type.Mac:
					{
						HostDotNetDirectoryName = "mac-x64";
						if (RuntimeInformation.ProcessArchitecture == Architecture.Arm64)
						{
							HostDotNetDirectoryName = "mac-arm64";
						}
						break;
					}
				case RuntimePlatform.Type.Linux: HostDotNetDirectoryName = "linux"; break;
				default: throw new Exception("Unknown host platform");
			}

			return Path.Combine("Binaries", "ThirdParty", "DotNet", DotnetVersionDirectory, HostDotNetDirectoryName);
		}

		static private string FindRelativeDotnetDirectory() => FindRelativeDotnetDirectory(RuntimePlatform.Current);

		/// <summary>
		/// Relative path to the dotnet executable from EngineDir
		/// </summary>
		/// <returns></returns>
		static public string RelativeDotnetDirectory = FindRelativeDotnetDirectory();

		static private DirectoryReference FindDotnetDirectory() => DirectoryReference.Combine(EngineDirectory, RelativeDotnetDirectory);

		/// <summary>
		/// The full name of the root UE directory
		/// </summary>
		public static readonly DirectoryReference RootDirectory = FindRootDirectory();

		/// <summary>
		/// The full name of the Engine directory
		/// </summary>
		public static readonly DirectoryReference EngineDirectory = DirectoryReference.Combine(RootDirectory, "Engine");

		/// <summary>
		/// The full name of the Engine/Source directory
		/// </summary>
		public static readonly DirectoryReference EngineSourceDirectory = DirectoryReference.Combine(EngineDirectory, "Source");

		/// <summary>
		/// Returns the User Settings Directory path. This matches FPlatformProcess::UserSettingsDir().
		/// </summary>
		static public readonly DirectoryReference UserSettingDirectory = GetUserSettingDirectory();

		/// <summary>
		/// Returns the User Directory path. This matches FPlatformProcess::UserDir().
		/// </summary>
		static public readonly DirectoryReference? UserDirectory = GetUserDirectory();

		/// <summary>
		/// Writable engine directory. Uses the user's settings folder for installed builds.
		/// </summary>
		public static readonly DirectoryReference WritableEngineDirectory = IsEngineInstalled() ? DirectoryReference.Combine(UserSettingDirectory, "UnrealEngine") : EngineDirectory;

		/// <summary>
		/// The engine saved programs directory
		/// </summary>
		public static readonly DirectoryReference EngineProgramSavedDirectory = IsEngineInstalled() ? UserSettingDirectory : DirectoryReference.Combine(EngineDirectory, "Programs");

		/// <summary>
		/// The path to UBT
		/// </summary>
		[Obsolete("Deprecated in UE5.1; to launch UnrealBuildTool, use this dll as the first argument with DonetPath")]
		public static readonly FileReference UnrealBuildToolPath = FindUnrealBuildToolDll().ChangeExtension(RuntimePlatform.ExeExtension);

		/// <summary>
		/// The path to UBT
		/// </summary>
		public static readonly FileReference UnrealBuildToolDllPath = FindUnrealBuildToolDll();

		/// <summary>
		/// The directory containing the bundled .NET installation
		/// </summary>
		static public readonly DirectoryReference DotnetDirectory = FindDotnetDirectory();

		/// <summary>
		/// The path of the bundled dotnet executable
		/// </summary>
		static public readonly FileReference DotnetPath = FileReference.Combine(DotnetDirectory, "dotnet" + RuntimePlatform.ExeExtension);

		/// <summary>
		/// Whether we're running on a build machine
		/// </summary>
		static private bool? bIsBuildMachine;

		/// <summary>
		/// Whether we're running with engine installed
		/// </summary>
		static private bool? bIsEngineInstalled;

		/// <summary>
		/// Returns where another platform's Dotnet is located
		/// </summary>
		/// <param name="HostPlatform"></param>
		/// <returns></returns>
		static public DirectoryReference FindDotnetDirectoryForPlatform(RuntimePlatform.Type HostPlatform)
		{
			return DirectoryReference.Combine(EngineDirectory, FindRelativeDotnetDirectory(HostPlatform));
		}

		/// <summary>
		/// Returns true of UnrealBuildTool is running on a build machine
		/// </summary>
		/// <returns>True if running on a build machine</returns>
		static public bool IsBuildMachine()
		{
			if (!bIsBuildMachine.HasValue)
			{
				bIsBuildMachine = Environment.GetEnvironmentVariable("IsBuildMachine")?.Trim() == "1";
			}
			return bIsBuildMachine.Value;
		}

		/// <summary>
		/// Returns true if UnrealBuildTool is running using installed Engine components
		/// </summary>
		/// <returns>True if running using installed Engine components</returns>
		static public bool IsEngineInstalled()
		{
			if (!bIsEngineInstalled.HasValue)
			{
				bIsEngineInstalled = FileReference.Exists(FileReference.Combine(EngineDirectory, "Build", "InstalledBuild.txt"));
			}
			return bIsEngineInstalled.Value;
		}

		public static class LocationOverride
		{
			/// <summary>
			/// If set, this value will be used to populate Unreal.RootDirectory
			/// </summary>
			public static DirectoryReference? RootDirectory = null;
		}


		// A subset of the functionality in DataDrivenPlatformInfo.GetAllPlatformInfos() - finds the DataDrivenPlatformInfo.ini files and records their existence, but does not parse them
		// (perhaps DataDrivenPlatformInfo.GetAllPlatformInfos() could be modified to use this data to avoid an additional search through the filesystem)
		public static HashSet<string>? IniPresentForPlatform = null;
		private static bool DataDrivenPlatformInfoIniIsPresent(string PlatformName)
		{
			if (IniPresentForPlatform == null)
			{
				IniPresentForPlatform = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

				// find all platform directories (skipping NFL/NoRedist)
				foreach (DirectoryReference EngineConfigDir in GetExtensionDirs(Unreal.EngineDirectory, "Config", bIncludeRestrictedDirectories: false))
				{
					// look through all config dirs looking for the data driven ini file
					foreach (string FilePath in Directory.EnumerateFiles(EngineConfigDir.FullName, "DataDrivenPlatformInfo.ini", SearchOption.AllDirectories))
					{
						FileReference FileRef = new FileReference(FilePath);

						// get the platform name from the path
						string IniPlatformName;
						if (FileRef.IsUnderDirectory(DirectoryReference.Combine(Unreal.EngineDirectory, "Config")))
						{
							// Foo/Engine/Config/<Platform>/DataDrivenPlatformInfo.ini
							IniPlatformName = Path.GetFileName(Path.GetDirectoryName(FilePath))!;
						}
						else
						{
							// Foo/Engine/Platforms/<Platform>/Config/DataDrivenPlatformInfo.ini
							IniPlatformName = Path.GetFileName(Path.GetDirectoryName(Path.GetDirectoryName(FilePath)))!;
						}

						// DataDrivenPlatformInfo.GetAllPlatformInfos() checks that [DataDrivenPlatformInfo] section exists as part of validating that the file exists
						// This code should probably behave the same way.

						IniPresentForPlatform.Add(IniPlatformName);
					}
				}
			}
			return IniPresentForPlatform.Contains(PlatformName);
		}

		// cached dictionary of BaseDir to extension directories
		private static ConcurrentDictionary<DirectoryReference, Tuple<List<DirectoryReference>, List<DirectoryReference>>> CachedExtensionDirectories = new ConcurrentDictionary<DirectoryReference, Tuple<List<DirectoryReference>, List<DirectoryReference>>>();

		/// <summary>
		/// Finds all the extension directories for the given base directory. This includes platform extensions and restricted folders.
		/// </summary>
		/// <param name="BaseDir">Location of the base directory</param>
		/// <param name="bIncludePlatformDirectories">If true, platform subdirectories are included (will return platform directories under Restricted dirs, even if bIncludeRestrictedDirectories is false)</param>
		/// <param name="bIncludeRestrictedDirectories">If true, restricted (NotForLicensees, NoRedist) subdirectories are included</param>
		/// <param name="bIncludeBaseDirectory">If true, BaseDir is included</param>
		/// <returns>List of extension directories, including the given base directory</returns>
		public static List<DirectoryReference> GetExtensionDirs(DirectoryReference BaseDir, bool bIncludePlatformDirectories = true, bool bIncludeRestrictedDirectories = true, bool bIncludeBaseDirectory = true)
		{
			Tuple<List<DirectoryReference>, List<DirectoryReference>> CachedDirs = CachedExtensionDirectories.GetOrAdd(BaseDir, _ =>
			{
				Tuple<List<DirectoryReference>, List<DirectoryReference>> NewCachedDirs = Tuple.Create(new List<DirectoryReference>(), new List<DirectoryReference>());

				DirectoryReference PlatformExtensionBaseDir = DirectoryReference.Combine(BaseDir, "Platforms");
				if (DirectoryReference.Exists(PlatformExtensionBaseDir))
				{
					NewCachedDirs.Item1.AddRange(DirectoryReference.EnumerateDirectories(PlatformExtensionBaseDir));
				}

				DirectoryReference RestrictedBaseDir = DirectoryReference.Combine(BaseDir, "Restricted");
				if (DirectoryReference.Exists(RestrictedBaseDir))
				{
					IEnumerable<DirectoryReference> RestrictedDirs = DirectoryReference.EnumerateDirectories(RestrictedBaseDir);
					NewCachedDirs.Item2.AddRange(RestrictedDirs);

					// also look for nested platforms in the restricted
					foreach (DirectoryReference RestrictedDir in RestrictedDirs)
					{
						DirectoryReference RestrictedPlatformExtensionBaseDir = DirectoryReference.Combine(RestrictedDir, "Platforms");
						if (DirectoryReference.Exists(RestrictedPlatformExtensionBaseDir))
						{
							NewCachedDirs.Item1.AddRange(DirectoryReference.EnumerateDirectories(RestrictedPlatformExtensionBaseDir));
						}
					}
				}

				// remove any platform directories in non-engine locations if the engine doesn't have the platform 
				if (BaseDir != Unreal.EngineDirectory && NewCachedDirs.Item1.Count > 0)
				{
					// if the DDPI.ini file doesn't exist, we haven't synced the platform, so just skip this directory
					NewCachedDirs.Item1.RemoveAll(x => !DataDrivenPlatformInfoIniIsPresent(x.GetDirectoryName()));
				}
				return NewCachedDirs;
			});

			// now return what the caller wanted (always include BaseDir)
			List<DirectoryReference> ExtensionDirs = new List<DirectoryReference>();
			if (bIncludeBaseDirectory)
			{
				ExtensionDirs.Add(BaseDir);
			}
			if (bIncludePlatformDirectories)
			{
				ExtensionDirs.AddRange(CachedDirs.Item1);
			}
			if (bIncludeRestrictedDirectories)
			{
				ExtensionDirs.AddRange(CachedDirs.Item2);
			}
			return ExtensionDirs;
		}

		/// <summary>
		/// Finds all the extension directories for the given base directory. This includes platform extensions and restricted folders.
		/// </summary>
		/// <param name="BaseDir">Location of the base directory</param>
		/// <param name="SubDir">The subdirectory to find</param>
		/// <param name="bIncludePlatformDirectories">If true, platform subdirectories are included (will return platform directories under Restricted dirs, even if bIncludeRestrictedDirectories is false)</param>
		/// <param name="bIncludeRestrictedDirectories">If true, restricted (NotForLicensees, NoRedist) subdirectories are included</param>
		/// <param name="bIncludeBaseDirectory">If true, BaseDir is included</param>
		/// <returns>List of extension directories, including the given base directory</returns>
		public static List<DirectoryReference> GetExtensionDirs(DirectoryReference BaseDir, string SubDir, bool bIncludePlatformDirectories = true, bool bIncludeRestrictedDirectories = true, bool bIncludeBaseDirectory = true)
		{
			return GetExtensionDirs(BaseDir, bIncludePlatformDirectories, bIncludeRestrictedDirectories, bIncludeBaseDirectory).Select(x => DirectoryReference.Combine(x, SubDir)).Where(x => DirectoryReference.Exists(x)).ToList();
		}

		/// <summary>
		/// Returns the User Settings Directory path. This matches FPlatformProcess::UserSettingsDir().
		/// </summary>
		private static DirectoryReference GetUserSettingDirectory()
		{
			if (RuntimePlatform.IsMac)
			{
				return new DirectoryReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Personal), "Library", "Application Support", "Epic"));
			}
			else if (RuntimePlatform.IsLinux)
			{
				return new DirectoryReference(Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData), "Epic"));
			}
			else
			{
				// Not all user accounts have a local application data directory (eg. SYSTEM, used by Jenkins for builds).
				List<Environment.SpecialFolder> DataFolders = new List<Environment.SpecialFolder>() {
					Environment.SpecialFolder.LocalApplicationData,
					Environment.SpecialFolder.CommonApplicationData
				};
				foreach (Environment.SpecialFolder DataFolder in DataFolders)
				{
					string DirectoryName = Environment.GetFolderPath(DataFolder);
					if (!String.IsNullOrEmpty(DirectoryName))
					{
						return new DirectoryReference(DirectoryName);
					}
				}
			}

			return DirectoryReference.Combine(EngineDirectory, "Saved");
		}

		/// <summary>
		/// Returns the User Directory path. This matches FPlatformProcess::UserDir().
		/// </summary>
		private static DirectoryReference? GetUserDirectory()
		{
			// Some user accounts (eg. SYSTEM on Windows) don't have a home directory. Ignore them if Environment.GetFolderPath() returns an empty string.
			string PersonalFolder = Environment.GetFolderPath(Environment.SpecialFolder.Personal);
			if (!String.IsNullOrEmpty(PersonalFolder))
			{
				if (RuntimePlatform.IsMac || RuntimePlatform.IsLinux)
				{
					return new DirectoryReference(System.IO.Path.Combine(PersonalFolder, "Documents"));
				}
				else
				{
					return new DirectoryReference(PersonalFolder);
				}
			}
			return null;
		}

		private static string? _MachineName;
		public static string MachineName
		{
			get
			{
				if (_MachineName == null)
				{
					try
					{
						// this likely can't fail, but just in case, fallback to preview implementation
						_MachineName = System.Net.Dns.GetHostName();

						if (RuntimePlatform.IsMac && _MachineName.EndsWith(".local")) 
						{
							_MachineName = _MachineName.Replace(".local", "");
						}
					}
					catch (Exception)
					{
						_MachineName = System.Environment.MachineName;
					}
				}
				return _MachineName!;
			}
		}
	}
}
