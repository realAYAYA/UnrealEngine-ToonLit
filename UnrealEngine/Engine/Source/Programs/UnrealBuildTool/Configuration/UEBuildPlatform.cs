// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;
using System.Linq;
using EpicGames.Core;
using System.Reflection;
using System.Diagnostics.CodeAnalysis;
using OpenTracing.Util;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	abstract class UEBuildPlatform
	{
		private static Dictionary<UnrealTargetPlatform, UEBuildPlatform> BuildPlatformDictionary = new Dictionary<UnrealTargetPlatform, UEBuildPlatform>();

		// a mapping of a group to the platforms in the group (ie, Microsoft contains Win32 and Win64)
		static Dictionary<UnrealPlatformGroup, List<UnrealTargetPlatform>> PlatformGroupDictionary = new Dictionary<UnrealPlatformGroup, List<UnrealTargetPlatform>>();

		/// <summary>
		/// The corresponding target platform enum
		/// </summary>
		public readonly UnrealTargetPlatform Platform;

		/// <summary>
		/// Logger for this platform
		/// </summary>
		protected readonly ILogger Logger;

		/// <summary>
		/// All the platform folder names
		/// </summary>
		private static string[]? CachedPlatformFolderNames;

		/// <summary>
		/// Cached copy of the list of folders to include for this platform
		/// </summary>
		private ReadOnlyHashSet<string>? CachedIncludedFolderNames;

		/// <summary>
		/// Cached copy of the list of folders to exclude for this platform
		/// </summary>
		private ReadOnlyHashSet<string>? CachedExcludedFolderNames;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="InPlatform">The enum value for this platform</param>
		/// <param name="SDK">The SDK management object for this platform</param>
		/// <param name="InLogger">Logger for output</param>
		public UEBuildPlatform(UnrealTargetPlatform InPlatform, UEBuildPlatformSDK SDK, ILogger InLogger)
		{
			Platform = InPlatform;
			Logger = InLogger;

			// check DDPI to see if the platform is enabled on this host platform
			string IniPlatformName = ConfigHierarchy.GetIniPlatformName(Platform);
			bool bIsEnabled = DataDrivenPlatformInfo.GetDataDrivenInfoForPlatform(IniPlatformName)?.bIsEnabled ?? true;

			// set up the SDK if the platform is enabled
			UEBuildPlatformSDK.RegisterSDKForPlatform(SDK, Platform.ToString(), bIsEnabled);
			if (bIsEnabled)
			{
				SDK.ManageAndValidateSDK();
			}
		}

		/// <summary>
		/// Finds all the UEBuildPlatformFactory types in this assembly and uses them to register all the available platforms
		/// </summary>
		/// <param name="bIncludeNonInstalledPlatforms">Whether to register platforms that are not installed</param>
		/// <param name="bHostPlatformOnly">Only register the host platform</param>
		/// <param name="Logger">Logger for output</param>
		public static void RegisterPlatforms(bool bIncludeNonInstalledPlatforms, bool bHostPlatformOnly, ILogger Logger)
		{
			// Initialize the installed platform info
			using (GlobalTracer.Instance.BuildSpan("Initializing InstalledPlatformInfo").StartActive())
			{
				InstalledPlatformInfo.Initialize();
			}

			// Find and register all tool chains and build platforms that are present
			Type[] AllTypes;
			using (GlobalTracer.Instance.BuildSpan("Querying types").StartActive())
			{
				AllTypes = Assembly.GetExecutingAssembly().GetTypes();
			}

			// register all build platforms first, since they implement SDK-switching logic that can set environment variables
			foreach (Type CheckType in AllTypes)
			{
				if (CheckType.IsClass && !CheckType.IsAbstract)
				{
					if (CheckType.IsSubclassOf(typeof(UEBuildPlatformFactory)))
					{
						Logger.LogDebug("    Registering build platform: {Platform}", CheckType.ToString());
						using (GlobalTracer.Instance.BuildSpan(CheckType.Name).StartActive())
						{
							UEBuildPlatformFactory TempInst = (UEBuildPlatformFactory)Activator.CreateInstance(CheckType)!;
							
							if(bHostPlatformOnly && TempInst.TargetPlatform != BuildHostPlatform.Current.Platform)
							{
								continue;
							}


							// We need all platforms to be registered when we run -validateplatform command to check SDK status of each
							if (bIncludeNonInstalledPlatforms || InstalledPlatformInfo.IsValidPlatform(TempInst.TargetPlatform))
							{
								TempInst.RegisterBuildPlatforms(Logger);
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Gets an array of all platform folder names
		/// </summary>
		/// <returns>Array of platform folders</returns>
		public static string[] GetPlatformFolderNames()
		{
			if(CachedPlatformFolderNames == null)
			{
				List<string> PlatformFolderNames = new List<string>();

				// Find all the platform folders to exclude from the list of precompiled modules
				PlatformFolderNames.AddRange(UnrealTargetPlatform.GetValidPlatformNames());

				// Also exclude all the platform groups that this platform is not a part of
				PlatformFolderNames.AddRange(UnrealPlatformGroup.GetValidGroupNames());

				// Save off the list as an array
				CachedPlatformFolderNames = PlatformFolderNames.ToArray();
			}
			return CachedPlatformFolderNames;
		}

		/// <summary>
		/// Finds a list of folder names to include when building for this platform
		/// </summary>
		public ReadOnlyHashSet<string> GetIncludedFolderNames()
		{
			if(CachedIncludedFolderNames == null)
			{
				HashSet<string> Names = new HashSet<string>(DirectoryReference.Comparer);

				Names.Add(Platform.ToString());
				foreach(UnrealPlatformGroup Group in UEBuildPlatform.GetPlatformGroups(Platform))
				{
					Names.Add(Group.ToString());
				}

				CachedIncludedFolderNames = new ReadOnlyHashSet<string>(Names, DirectoryReference.Comparer);
			}
			return CachedIncludedFolderNames;
		}

		/// <summary>
		/// Finds a list of folder names to exclude when building for this platform
		/// </summary>
		public ReadOnlyHashSet<string> GetExcludedFolderNames()
		{
			if(CachedExcludedFolderNames == null)
			{
				CachedExcludedFolderNames = new ReadOnlyHashSet<string>(GetPlatformFolderNames().Except(GetIncludedFolderNames()), DirectoryReference.Comparer);
			}
			return CachedExcludedFolderNames;
		}

		/// <summary>
		/// Whether the required external SDKs are installed for this platform. Could be either a manual install or an AutoSDK.
		/// </summary>
		public SDKStatus HasRequiredSDKsInstalled()
		{
			return UEBuildPlatform.GetSDK(Platform)!.HasRequiredSDKsInstalled();
		}

		/// <summary>
		/// Whether this platform requires specific Visual Studio version.
		/// </summary>
		public virtual VCProjectFileFormat GetRequiredVisualStudioVersion()
		{
			return VCProjectFileFormat.Default;
		}

		/// <summary>
		/// The version required to support Visual Studio
		/// </summary>
		public virtual Version GetVersionRequiredForVisualStudio(VCProjectFileFormat Format)
		{
			return new Version();
		}

		/// <summary>
		/// Gets all the registered platforms
		/// </summary>
		/// <returns>Sequence of registered platforms</returns>
		public static IEnumerable<UnrealTargetPlatform> GetRegisteredPlatforms()
		{
			return BuildPlatformDictionary.Keys;
		}

		/// <summary>
		/// Returns true if this platform is capable of building the specified architectures in a single pass
		/// (e.g. creating a fat binary).
		/// </summary>
		/// <param name="InArchitectures">Architectures that are being built</param>
		public virtual bool CanBuildArchitecturesInSinglePass(IEnumerable<string> InArchitectures)
		{
			return false;
		}

		/// <summary>
		/// Get the default architecture for a project. This may be overriden on the command line to UBT.
		/// </summary>
		/// <param name="ProjectFile">Optional project to read settings from </param>
		public virtual string GetDefaultArchitecture(FileReference? ProjectFile)
		{
			// by default, use an empty architecture (which is really just a modifer to the platform for some paths/names)
			return "";
		}

		/// <summary>
		/// Get name for architecture-specific directories (can be shorter than architecture name itself)
		/// </summary>
		public virtual string GetFolderNameForArchitecture(string Architecture)
		{
			// by default, use the architecture name
			return Architecture;
		}

		/// <summary>
		/// Searches a directory tree for build products to be cleaned.
		/// </summary>
		/// <param name="BaseDir">The directory to search</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <param name="FilesToClean">List to receive a list of files to be cleaned</param>
		/// <param name="DirectoriesToClean">List to receive a list of directories to be cleaned</param>
		public void FindBuildProductsToClean(DirectoryReference BaseDir, string[] NamePrefixes, string[] NameSuffixes, List<FileReference> FilesToClean, List<DirectoryReference> DirectoriesToClean)
		{
			foreach (FileReference File in DirectoryReference.EnumerateFiles(BaseDir))
			{
				string FileName = File.GetFileName();
				if (IsDefaultBuildProduct(FileName, NamePrefixes, NameSuffixes) || IsBuildProduct(FileName, NamePrefixes, NameSuffixes))
				{
					FilesToClean.Add(File);
				}
			}
			foreach (DirectoryReference SubDir in DirectoryReference.EnumerateDirectories(BaseDir))
			{
				string SubDirName = SubDir.GetDirectoryName();
				if (IsBuildProduct(SubDirName, NamePrefixes, NameSuffixes))
				{
					DirectoriesToClean.Add(SubDir);
				}
				else
				{
					FindBuildProductsToClean(SubDir, NamePrefixes, NameSuffixes, FilesToClean, DirectoriesToClean);
				}
			}
		}

		/// <summary>
		/// Enumerates any additional directories needed to clean this target
		/// </summary>
		/// <param name="Target">The target to clean</param>
		/// <param name="FilesToDelete">Receives a list of files to be removed</param>
		/// <param name="DirectoriesToDelete">Receives a list of directories to be removed</param>
		public virtual void FindAdditionalBuildProductsToClean(ReadOnlyTargetRules Target, List<FileReference> FilesToDelete, List<DirectoryReference> DirectoriesToDelete)
		{
		}

		/// <summary>
		/// Determines if a filename is a default UBT build product
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the substring matches the name of a build product, false otherwise</returns>
		public static bool IsDefaultBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes)
		{
			return UEBuildPlatform.IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".target")
				|| UEBuildPlatform.IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".modules")
				|| UEBuildPlatform.IsBuildProductName(FileName, NamePrefixes, NameSuffixes, ".version");
		}

		/// <summary>
		/// Determines if the given name is a build product for a target.
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public abstract bool IsBuildProduct(string FileName, string[] NamePrefixes, string[] NameSuffixes);

		/// <summary>
		/// Determines if a string is in the canonical name of a UE build product, with a specific extension (eg. "UnrealEditor-Win64-Debug.exe" or "UnrealEditor-ModuleName-Win64-Debug.dll"). 
		/// </summary>
		/// <param name="FileName">The file name to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <param name="Extension">The extension to check for</param>
		/// <returns>True if the string matches the name of a build product, false otherwise</returns>
		public static bool IsBuildProductName(string FileName, string[] NamePrefixes, string[] NameSuffixes, string Extension)
		{
			return IsBuildProductName(FileName, 0, FileName.Length, NamePrefixes, NameSuffixes, Extension);
		}

		/// <summary>
		/// Determines if a substring is in the canonical name of a UE build product, with a specific extension (eg. "UnrealEditor-Win64-Debug.exe" or "UnrealEditor-ModuleName-Win64-Debug.dll"). 
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="Index">Index of the first character to be checked</param>
		/// <param name="Count">Number of characters of the substring to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <param name="Extension">The extension to check for</param>
		/// <returns>True if the substring matches the name of a build product, false otherwise</returns>
		public static bool IsBuildProductName(string FileName, int Index, int Count, string[] NamePrefixes, string[] NameSuffixes, string Extension)
		{
			// Check if the extension matches, and forward on to the next IsBuildProductName() overload without it if it does.
			if (Count > Extension.Length && String.Compare(FileName, Index + Count - Extension.Length, Extension, 0, Extension.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
			{
				return IsBuildProductName(FileName, Index, Count - Extension.Length, NamePrefixes, NameSuffixes);
			}
			return false;
		}

		/// <summary>
		/// Determines if a substring is in the canonical name of a UE build product, excluding extension or other decoration (eg. "UnrealEditor-Win64-Debug" or "UnrealEditor-ModuleName-Win64-Debug"). 
		/// </summary>
		/// <param name="FileName">The name to check</param>
		/// <param name="Index">Index of the first character to be checked</param>
		/// <param name="Count">Number of characters of the substring to check</param>
		/// <param name="NamePrefixes">Target or application names that may appear at the start of the build product name (eg. "UnrealEditor", "ShooterGameEditor")</param>
		/// <param name="NameSuffixes">Suffixes which may appear at the end of the build product name</param>
		/// <returns>True if the substring matches the name of a build product, false otherwise</returns>
		public static bool IsBuildProductName(string FileName, int Index, int Count, string[] NamePrefixes, string[] NameSuffixes)
		{
			foreach (string NamePrefix in NamePrefixes)
			{
				if (Count >= NamePrefix.Length && String.Compare(FileName, Index, NamePrefix, 0, NamePrefix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
				{
					int MinIdx = Index + NamePrefix.Length;
					foreach (string NameSuffix in NameSuffixes)
					{
						int MaxIdx = Index + Count - NameSuffix.Length;
						if (MaxIdx >= MinIdx && String.Compare(FileName, MaxIdx, NameSuffix, 0, NameSuffix.Length, StringComparison.InvariantCultureIgnoreCase) == 0)
						{
							if (MinIdx < MaxIdx && FileName[MinIdx] == '-')
							{
								MinIdx++;
								while (MinIdx < MaxIdx && FileName[MinIdx] != '-' && FileName[MinIdx] != '.')
								{
									MinIdx++;
								}
							}
							if (MinIdx == MaxIdx)
							{
								return true;
							}
						}
					}
				}
			}
			return false;
		}

		public virtual void PostBuildSync(UEBuildTarget Target)
		{
		}

		/// <summary>
		/// Get the bundle directory for the shared link environment
		/// </summary>
		/// <param name="Rules">The target rules</param>
		/// <param name="OutputFiles">List of executable output files</param>
		/// <returns>Path to the bundle directory</returns>
		public virtual DirectoryReference? GetBundleDirectory(ReadOnlyTargetRules Rules, List<FileReference> OutputFiles)
		{
			return null;
		}

		/// <summary>
		/// Determines whether a given platform is available
		/// </summary>
		/// <param name="Platform">The platform to check for</param>
		/// <returns>True if it's available, false otherwise</returns>
		public static bool IsPlatformAvailable(UnrealTargetPlatform Platform)
		{
			return BuildPlatformDictionary.ContainsKey(Platform) && BuildPlatformDictionary[Platform].HasRequiredSDKsInstalled() == SDKStatus.Valid;
		}

		/// <summary>
		/// Determines whether a given platform is available in the context of a particular Taget
		/// </summary>
		/// <param name="Platform">The platform to check for</param>
		/// <param name="Target">A Target object that may further restrict available platforms</param>
		/// <returns>True if it's available, false otherwise</returns>
		public static bool IsPlatformAvailableForTarget(UnrealTargetPlatform Platform, ReadOnlyTargetRules Target)
		{
			return IsPlatformAvailable(Platform) && Target.IsPlatformOptedIn(Platform);
		}

		/// <summary>
		/// Register the given platforms UEBuildPlatform instance
		/// </summary>
		/// <param name="InBuildPlatform"> The UEBuildPlatform instance to use for the InPlatform</param>
		/// <param name="Logger">Logger for output</param>
		public static void RegisterBuildPlatform(UEBuildPlatform InBuildPlatform, ILogger Logger)
		{
			Logger.LogDebug("        Registering build platform: {Platform} - buildable: {Buildable}", InBuildPlatform.Platform, InBuildPlatform.HasRequiredSDKsInstalled() == SDKStatus.Valid);

			if (BuildPlatformDictionary.ContainsKey(InBuildPlatform.Platform) == true)
			{
				Logger.LogWarning("RegisterBuildPlatform Warning: Registering build platform {Platform} for {ForPlatform} when it is already set to {CurPlatform}",
					InBuildPlatform.ToString(), InBuildPlatform.Platform.ToString(), BuildPlatformDictionary[InBuildPlatform.Platform].ToString());
				BuildPlatformDictionary[InBuildPlatform.Platform] = InBuildPlatform;
			}
			else
			{
				BuildPlatformDictionary.Add(InBuildPlatform.Platform, InBuildPlatform);
			}
		}

		/// <summary>
		/// Assign a platform as a member of the given group
		/// </summary>
		public static void RegisterPlatformWithGroup(UnrealTargetPlatform InPlatform, UnrealPlatformGroup InGroup)
		{
			// find or add the list of groups for this platform
			List<UnrealTargetPlatform>? Platforms;
			if(!PlatformGroupDictionary.TryGetValue(InGroup, out Platforms))
			{
				Platforms = new List<UnrealTargetPlatform>();
				PlatformGroupDictionary.Add(InGroup, Platforms);
			}
			Platforms.Add(InPlatform);
		}

		/// <summary>
		/// Retrieve the list of platforms in this group (if any)
		/// </summary>
		public static List<UnrealTargetPlatform> GetPlatformsInGroup(UnrealPlatformGroup InGroup)
		{
			List<UnrealTargetPlatform>? PlatformList;
			if (!PlatformGroupDictionary.TryGetValue(InGroup, out PlatformList))
			{
				PlatformList = new List<UnrealTargetPlatform>();
			}
			return PlatformList;
		}

		/// <summary>
		/// Enumerates all the platform groups for a given platform
		/// </summary>
		/// <param name="Platform">The platform to look for</param>
		/// <returns>List of platform groups that this platform is a member of</returns>
		public static IEnumerable<UnrealPlatformGroup> GetPlatformGroups(UnrealTargetPlatform Platform)
		{
			return PlatformGroupDictionary.Where(x => x.Value.Contains(Platform)).Select(x => x.Key);
		}

		/// <summary>
		/// Retrieve the IUEBuildPlatform instance for the given TargetPlatform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <returns>UEBuildPlatform  The instance of the build platform</returns>
		public static UEBuildPlatform GetBuildPlatform(UnrealTargetPlatform InPlatform)
		{
			UEBuildPlatform? Platform;
			if(!TryGetBuildPlatform(InPlatform, out Platform))
			{
				throw new BuildException("GetBuildPlatform: No BuildPlatform found for {0}", InPlatform.ToString());
			}
			return Platform;
			}

		/// <summary>
		/// Retrieve the IUEBuildPlatform instance for the given TargetPlatform
		/// </summary>
		/// <param name="InPlatform">  The UnrealTargetPlatform being built</param>
		/// <param name="Platform"></param>
		/// <returns>UEBuildPlatform  The instance of the build platform</returns>
		public static bool TryGetBuildPlatform(UnrealTargetPlatform InPlatform, [NotNullWhen(true)] out UEBuildPlatform? Platform)
		{
			return BuildPlatformDictionary.TryGetValue(InPlatform, out Platform);
		}

		/// <summary>
		/// Allow all registered build platforms to modify the newly created module
		/// passed in for the given platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public static void PlatformModifyHostModuleRules(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
			foreach (KeyValuePair<UnrealTargetPlatform, UEBuildPlatform> PlatformEntry in BuildPlatformDictionary)
			{
				PlatformEntry.Value.ModifyModuleRulesForOtherPlatform(ModuleName, Rules, Target);
			}
		}

		/// <summary>
		/// Returns the delimiter used to separate paths in the PATH environment variable for the platform we are executing on.
		/// </summary>
		public static String GetPathVarDelimiter()
		{
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Linux || BuildHostPlatform.Current.Platform == UnrealTargetPlatform.LinuxArm64 ||
				BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Mac)
			{
				return ":";
			}
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64)
			{
				return ";";
			}

			throw new InvalidOperationException($"PATH variable delimiter unknown for platform {BuildHostPlatform.Current.Platform}");
		}

		/// <summary>
		/// Gets the platform name that should be used.
		/// </summary>
		public virtual string GetPlatformName()
		{
			return Platform.ToString();
		}

		/// <summary>
		/// If this platform can be compiled with XGE
		/// </summary>
		public virtual bool CanUseXGE()
		{
			return true;
		}

		/// <summary>
		/// If this platform can be compiled with the parallel executor
		/// </summary>
		public virtual bool CanUseParallelExecutor()
		{
			return CanUseXGE();
		}

		/// <summary>
		/// If this platform can be compiled with FASTBuild
		/// </summary>
		public virtual bool CanUseFASTBuild()
		{
			return false;
		}

		/// <summary>
		/// If this platform can be compiled with SN-DBS
		/// </summary>
		public virtual bool CanUseSNDBS()
		{
			return false;
		}

		/// <summary>
		/// Set all the platform-specific defaults for a new target
		/// </summary>
		public virtual void ResetTarget(TargetRules Target)
		{
		}

		/// <summary>
		/// Validate a target's settings
		/// </summary>
		public virtual void ValidateTarget(TargetRules Target)
		{
		}

		/// <summary>
		/// Validate a UEBuildModule before it's processed
		/// <param name="Module">The UEBuildModule that needs to be validated</param>
		/// <param name="Target">Options for the target being built</param>
		/// </summary>
		public virtual void ValidateModule(UEBuildModule Module, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Return whether the given platform requires a monolithic build
		/// </summary>
		/// <param name="InPlatform">The platform of interest</param>
		/// <param name="InConfiguration">The configuration of interest</param>
		/// <returns></returns>
		public static bool PlatformRequiresMonolithicBuilds(UnrealTargetPlatform InPlatform, UnrealTargetConfiguration InConfiguration)
		{
			// Some platforms require monolithic builds...
			UEBuildPlatform? BuildPlatform;
			if (TryGetBuildPlatform(InPlatform, out BuildPlatform))
			{
				return BuildPlatform.ShouldCompileMonolithicBinary(InPlatform);
			}

			// We assume it does not
			return false;
		}

		/// <summary>
		/// Get the extension to use for the given binary type
		/// </summary>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string    The binary extension (i.e. 'exe' or 'dll')</returns>
		public virtual string GetBinaryExtension(UEBuildBinaryType InBinaryType)
		{
			throw new BuildException("GetBinaryExtensiton for {0} not handled in {1}", InBinaryType.ToString(), this.ToString());
		}

		/// <summary>
		/// Get the extensions to use for debug info for the given binary type
		/// </summary>
		/// <param name="InTarget">Options for the target being built</param>
		/// <param name="InBinaryType"> The binary type being built</param>
		/// <returns>string[]    The debug info extensions (i.e. 'pdb')</returns>
		public virtual string[] GetDebugInfoExtensions(ReadOnlyTargetRules InTarget, UEBuildBinaryType InBinaryType)
		{
			throw new BuildException("GetDebugInfoExtensions for {0} not handled in {1}", InBinaryType.ToString(), this.ToString());
		}

		/// <summary>
		/// Whether this platform should build a monolithic binary
		/// </summary>
		public virtual bool ShouldCompileMonolithicBinary(UnrealTargetPlatform InPlatform)
		{
			return false;
		}

		/// <summary>
		/// Modify the rules for a newly created module, where the target is a different host platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public virtual void ModifyModuleRulesForOtherPlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Allows the platform to override whether the architecture name should be appended to the name of binaries.
		/// </summary>
		/// <returns>True if the architecture name should be appended to the binary</returns>
		public virtual bool RequiresArchitectureSuffix()
		{
			return true;
		}

		/// <summary>
		/// For platforms that need to output multiple files per binary (ie Android "fat" binaries)
		/// this will emit multiple paths. By default, it simply makes an array from the input
		/// </summary>
		public virtual List<FileReference> FinalizeBinaryPaths(FileReference BinaryName, FileReference? ProjectFile, ReadOnlyTargetRules Target)
		{
			List<FileReference> TempList = new List<FileReference>() { BinaryName };
			return TempList;
		}

		/// <summary>
		/// Return all valid configurations for this platform
		/// Typically, this is always Debug, Development, and Shipping - but Test is a likely future addition for some platforms
		/// </summary>
		public virtual List<UnrealTargetConfiguration> GetConfigurations(UnrealTargetPlatform InUnrealTargetPlatform, bool bIncludeDebug)
		{
			List<UnrealTargetConfiguration> Configurations = new List<UnrealTargetConfiguration>()
			{
				UnrealTargetConfiguration.Development,
			};

			if (bIncludeDebug)
			{
				Configurations.Insert(0, UnrealTargetConfiguration.Debug);
			}

			return Configurations;
		}

		protected static bool DoProjectSettingsMatchDefault(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName, string Section, string[]? BoolKeys, string[]? IntKeys, string[]? StringKeys, ILogger Logger)
		{
			ConfigHierarchy ProjIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, ProjectDirectoryName, Platform);
			ConfigHierarchy DefaultIni = ConfigCache.ReadHierarchy(ConfigHierarchyType.Engine, (DirectoryReference?)null, Platform);

			// look at all bool values
			if (BoolKeys != null) foreach (string Key in BoolKeys)
				{
					bool Default = false, Project = false;
					DefaultIni.GetBool(Section, Key, out Default);
					ProjIni.GetBool(Section, Key, out Project);
					if (Default != Project)
					{
						Log.TraceInformationOnce("{0} is not set to default. (Base: {1} vs. {2}: {3})", Key, Default, Path.GetFileName(ProjectDirectoryName.FullName), Project);
						return false;
					}
				}

			// look at all int values
			if (IntKeys != null) foreach (string Key in IntKeys)
				{
					int Default = 0, Project = 0;
					DefaultIni.GetInt32(Section, Key, out Default);
					ProjIni.GetInt32(Section, Key, out Project);
					if (Default != Project)
					{
						Log.TraceInformationOnce("{0} is not set to default. (Base: {1} vs. {2}: {3})", Key, Default, Path.GetFileName(ProjectDirectoryName.FullName), Project);
						return false;
					}
				}

			// look for all string values
			if (StringKeys != null) foreach (string Key in StringKeys)
				{
					string? Default = "", Project = "";
					DefaultIni.GetString(Section, Key, out Default);
					ProjIni.GetString(Section, Key, out Project);
					if (Default != Project)
					{
						Log.TraceInformationOnce("{0} is not set to default. (Base: {1} vs. {2}: {3})", Key, Default, Path.GetFileName(ProjectDirectoryName.FullName), Project);
						return false;
					}
				}

			// if we get here, we match all important settings
			return true;
		}

		/// <summary>
		/// Check for the default configuration
		/// return true if the project uses the default build config
		/// </summary>
		public virtual bool HasDefaultBuildConfig(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			string[] BoolKeys = new string[] {
				"bCompileApex", "bCompileICU",
				"bCompileRecast", "bCompileSpeedTree",
				"bCompileWithPluginSupport", "bCompilePhysXVehicle", "bCompileFreeType",
				"bCompileForSize", "bCompileCEF3", "bCompileCustomSQLitePlatform"
			};

			return DoProjectSettingsMatchDefault(Platform, ProjectDirectoryName, "/Script/BuildSettings.BuildSettings",
				BoolKeys, null, null, Logger);
		}

		/// <summary>
		/// Check for whether we require a build for platform reasons
		/// return true if the project requires a build
		/// </summary>
		public virtual bool RequiresBuild(UnrealTargetPlatform Platform, DirectoryReference ProjectDirectoryName)
		{
			return false;
		}

		/// <summary>
		/// Get a list of extra modules the platform requires.
		/// This is to allow undisclosed platforms to add modules they need without exposing information about the platform.
		/// </summary>
		/// <param name="Target">The target being build</param>
		/// <param name="ExtraModuleNames">List of extra modules the platform needs to add to the target</param>
		public virtual void AddExtraModules(ReadOnlyTargetRules Target, List<string> ExtraModuleNames)
		{
		}

		/// <summary>
		/// Modify the rules for a newly created module, in a target that's being built for this platform.
		/// This is not required - but allows for hiding details of a particular platform.
		/// </summary>
		/// <param name="ModuleName">The name of the module</param>
		/// <param name="Rules">The module rules</param>
		/// <param name="Target">The target being build</param>
		public virtual void ModifyModuleRulesForActivePlatform(string ModuleName, ModuleRules Rules, ReadOnlyTargetRules Target)
		{
		}

		/// <summary>
		/// Setup the target environment for building
		/// </summary>
		/// <param name="Target">Settings for the target being compiled</param>
		/// <param name="CompileEnvironment">The compile environment for this target</param>
		/// <param name="LinkEnvironment">The link environment for this target</param>
		public abstract void SetUpEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment CompileEnvironment, LinkEnvironment LinkEnvironment);

		/// <summary>
		/// Setup the configuration environment for building
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="GlobalCompileEnvironment">The global compile environment</param>
		/// <param name="GlobalLinkEnvironment">The global link environment</param>
		public virtual void SetUpConfigurationEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment GlobalCompileEnvironment, LinkEnvironment GlobalLinkEnvironment)
		{
			if (GlobalCompileEnvironment.bUseDebugCRT)
			{
				GlobalCompileEnvironment.Definitions.Add("_DEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}
			else
			{
				GlobalCompileEnvironment.Definitions.Add("NDEBUG=1"); // the engine doesn't use this, but lots of 3rd party stuff does
			}

			switch (Target.Configuration)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEBUG=1");
					break;
				case UnrealTargetConfiguration.DebugGame:
				// Individual game modules can be switched to be compiled in debug as necessary. By default, everything is compiled in development.
				case UnrealTargetConfiguration.Development:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_DEVELOPMENT=1");
					break;
				case UnrealTargetConfiguration.Shipping:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_SHIPPING=1");
					break;
				case UnrealTargetConfiguration.Test:
					GlobalCompileEnvironment.Definitions.Add("UE_BUILD_TEST=1");
					break;
			}

			// Create debug info based on the heuristics specified by the user.
			GlobalCompileEnvironment.bCreateDebugInfo =
				!Target.bDisableDebugInfo && ShouldCreateDebugInfo(Target);
			GlobalLinkEnvironment.bCreateDebugInfo = GlobalCompileEnvironment.bCreateDebugInfo;
		}

		/// <summary>
		/// Allows the platform to return various build metadata that is not tracked by other means. If the returned string changes, the makefile will be invalidated.
		/// </summary>
		/// <param name="ProjectFile">The project file being built</param>
		/// <returns>String describing the current build metadata</returns>
		public string GetExternalBuildMetadata(FileReference? ProjectFile)
		{
			StringBuilder Result = new StringBuilder();
			GetExternalBuildMetadata(ProjectFile, Result);
			return Result.ToString();
		}

		/// <summary>
		/// Allows the platform to return various build metadata that is not tracked by other means. If the returned string changes, the makefile will be invalidated.
		/// </summary>
		/// <param name="ProjectFile">The project file being built</param>
		/// <param name="Metadata">String builder to contain build metadata</param>
		public virtual void GetExternalBuildMetadata(FileReference? ProjectFile, StringBuilder Metadata)
		{
		}


		/// <summary>
		/// Allows the platform to modify the binary link environment before the binary is built
		/// </summary>
		/// <param name="BinaryLinkEnvironment">The binary link environment being created</param>
		/// <param name="BinaryCompileEnvironment">The binary compile environment being used</param>
		/// <param name="Target">The target rules in use</param>
		/// <param name="ToolChain">The toolchain being used</param>
		/// <param name="Graph">Action graph that is used to build the binary</param>
		public virtual void ModifyBinaryLinkEnvironment( LinkEnvironment BinaryLinkEnvironment, CppCompileEnvironment BinaryCompileEnvironment, ReadOnlyTargetRules Target, UEToolChain ToolChain, IActionGraphBuilder Graph)
		{
		}

		/// <summary>
		/// Indicates whether this platform requires a .loadorder file to be generated for the build.
		/// .loadorder files contain a list of dynamic modules and the exact order in which they should be loaded
		/// to ensure that all dependencies are satisfied i.e. we don't attempt to load a module without loading
		/// all its dependencies first.
		/// As such, this file is only needed in modular builds on some platforms (depending on the way they implement dynamic modules such as DLLs).
		/// </summary>
		/// <param name="Target">The target rules in use</param>
		/// <returns>True if .loadorder file should be generated</returns>
		public virtual bool RequiresLoadOrderManifest(ReadOnlyTargetRules Target)
		{
			return false;
		}


		/// <summary>
		/// Checks if platform is part of a given platform group
		/// </summary>
		/// <param name="Platform">The platform to check</param>
		/// <param name="PlatformGroup">The platform group to check</param>
		/// <returns>True if platform is part of a platform group</returns>
		internal static bool IsPlatformInGroup(UnrealTargetPlatform Platform, UnrealPlatformGroup PlatformGroup)
		{
			List<UnrealTargetPlatform>? Platforms = UEBuildPlatform.GetPlatformsInGroup(PlatformGroup);
			if (Platforms != null)
			{
				return Platforms.Contains(Platform);
			}
			else
			{
				return false;
			}
		}

		/// <summary>
		/// Gets the SDK object that was passed in to the constructor
		/// </summary>
		/// <returns>The SDK object</returns>
		public UEBuildPlatformSDK? GetSDK()
		{
			return UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString());
		}
		/// <summary>
		/// Gets the SDK object that was passed in to the constructor to the UEBuildPlatform constructor for this platform
		/// </summary>
		/// <returns>The SDK object</returns>
		public static UEBuildPlatformSDK? GetSDK(UnrealTargetPlatform Platform)
		{
			return UEBuildPlatformSDK.GetSDKForPlatform(Platform.ToString());
		}

		/// <summary>
		/// Whether this platform should create debug information or not
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>bool    true if debug info should be generated, false if not</returns>
		public abstract bool ShouldCreateDebugInfo(ReadOnlyTargetRules Target);

		/// <summary>
		/// Creates a toolchain instance for this platform. There should be a single toolchain instance per-target, as their may be
		/// state data and configuration cached between calls.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <returns>New toolchain instance.</returns>
		public abstract UEToolChain CreateToolChain(ReadOnlyTargetRules Target);

		/// <summary>
		/// Deploys the given target
		/// </summary>
		/// <param name="Receipt">Receipt for the target being deployed</param>
		public abstract void Deploy(TargetReceipt Receipt);
	}
}
