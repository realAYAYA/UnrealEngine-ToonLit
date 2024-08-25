// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// A unit of code compilation and linking.
	/// </summary>
	abstract class UEBuildModule
	{
		/// <summary>
		/// The rules for this module
		/// </summary>
		public readonly ModuleRules Rules;

		/// <summary>
		/// The directory for this module's object files
		/// </summary>
		public readonly DirectoryReference IntermediateDirectory;

		/// <summary>
		/// The directory for this module's generated files that are architecture independent
		/// </summary>
		public readonly DirectoryReference IntermediateDirectoryNoArch;

		/// <summary>
		/// The name that uniquely identifies the module.
		/// </summary>
		public string Name => Rules.Name;

		/// <summary>
		/// Path to the module directory
		/// </summary>
		public DirectoryReference ModuleDirectory => Rules.Directory;

		/// <summary>
		/// Paths to all potential module source directories (with platform extension directories added in)
		/// </summary>
		public DirectoryReference[] ModuleDirectories;

		/// <summary>
		/// The name of the .Build.cs file this module was created from, if any
		/// </summary>
		public FileReference RulesFile => Rules.File;

		/// <summary>
		/// The binary the module will be linked into for the current target.  Only set after UEBuildBinary.BindModules is called.
		/// </summary>
		public UEBuildBinary Binary = null!;

		/// <summary>
		/// The name of the _API define for this module
		/// </summary>
		protected readonly string ModuleApiDefine;

		/// <summary>
		/// Set of all the public definitions
		/// </summary>
		public readonly HashSet<string> PublicDefinitions;

		/// <summary>
		/// Set of all public include paths
		/// </summary>
		public readonly HashSet<DirectoryReference> PublicIncludePaths;

		/// <summary>
		/// Set of all internal include paths
		/// </summary>
		public readonly HashSet<DirectoryReference> InternalIncludePaths;

		/// <summary>
		/// Nested public include paths which used to be added automatically, but are now only added for modules with bNestedPublicIncludePaths set.
		/// </summary>
		public readonly HashSet<DirectoryReference> LegacyPublicIncludePaths = new HashSet<DirectoryReference>();

		/// <summary>
		/// Parent include paths which used to be added automatically, but are now only added for modules with bLegacyParentIncludePaths set.
		/// </summary>
		public readonly HashSet<DirectoryReference> LegacyParentIncludePaths = new HashSet<DirectoryReference>();

		/// <summary>
		/// Set of all private include paths
		/// </summary>
		public readonly HashSet<DirectoryReference> PrivateIncludePaths;

		/// <summary>
		/// Set of all system include paths
		/// </summary>
		public readonly HashSet<DirectoryReference> PublicSystemIncludePaths;

		/// <summary>
		/// Set of all additional libraries
		/// </summary>
		public readonly HashSet<FileReference> PublicLibraries = new HashSet<FileReference>();

		/// <summary>
		/// Set of all system libraries
		/// </summary>
		public readonly HashSet<string> PublicSystemLibraries = new HashSet<string>();

		/// <summary>
		/// Set of all public system library paths
		/// </summary>
		public readonly HashSet<DirectoryReference> PublicSystemLibraryPaths;

		/// <summary>
		/// Set of additional frameworks
		/// </summary>
		public readonly HashSet<string> PublicFrameworks;

		/// <summary>
		/// 
		/// </summary>
		public readonly HashSet<string> PublicWeakFrameworks;

		/// <summary>
		/// 
		/// </summary>
		protected readonly HashSet<UEBuildFramework> PublicAdditionalFrameworks;

		/// <summary>
		/// 
		/// </summary>
		protected readonly HashSet<UEBuildBundleResource> PublicAdditionalBundleResources;

		/// <summary>
		/// Names of modules with header files that this module's public interface needs access to.
		/// </summary>
		public List<UEBuildModule>? PublicIncludePathModules;

		/// <summary>
		/// Names of modules that this module's public interface depends on.
		/// </summary>
		public List<UEBuildModule>? PublicDependencyModules;

		/// <summary>
		/// Names of DLLs that this module should delay load
		/// </summary>
		public HashSet<string> PublicDelayLoadDLLs;

		/// <summary>
		/// Names of modules with header files that this module's private implementation needs access to.
		/// </summary>
		public List<UEBuildModule>? PrivateIncludePathModules;

		/// <summary>
		/// Names of modules that this module's private implementation depends on.
		/// </summary>
		public List<UEBuildModule>? PrivateDependencyModules;

		/// <summary>
		/// Extra modules this module may require at run time
		/// </summary>
		public List<UEBuildModule>? DynamicallyLoadedModules;

		/// <summary>
		/// Set of modules that have referenced this module
		/// </summary>
		protected HashSet<UEBuildModule>? ReferenceStackParentModules;

		/// <summary>
		/// Set of all allowed restricted folder references
		/// </summary>
		private readonly HashSet<DirectoryReference> RestrictedFoldersAllowList;

		/// <summary>
		/// Set of aliased restricted folder references
		/// </summary>
		public readonly Dictionary<string, string> AliasRestrictedFolders;

		/// <summary>
		/// Per-architecture lists of dependencies for linking to ignore (useful when building for multiple architectures, and a lib only is needed for one architecture), it's up to the Toolchain to use this
		/// </summary>
		public Dictionary<string, List<UnrealArch>> DependenciesToSkipPerArchitecture;

		/// <summary>
		/// The Verse source code directory associated with this module if any
		/// </summary>
		public virtual DirectoryReference? VerseDirectory => null;

		/// <summary>
		/// If this module has Verse code associated with it (convenience function)
		/// </summary>
		public bool bHasVerse => VerseDirectory != null;

		/// <summary>
		/// If this module or any of its dependencies has Verse code associated with it
		/// </summary>
		public bool bDependsOnVerse = false;

		public List<FileItem> NatvisFiles = new();

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Rules">Rules for this module</param>
		/// <param name="IntermediateDirectory">Intermediate directory for this module</param>
		/// <param name="IntermediateDirectoryNoArch">Intermediate directory for this module for files that are architecture independent</param>
		/// <param name="Logger">Logger for output</param>
		public UEBuildModule(ModuleRules Rules, DirectoryReference IntermediateDirectory, DirectoryReference IntermediateDirectoryNoArch, ILogger Logger)
		{
			this.Rules = Rules;
			this.IntermediateDirectory = IntermediateDirectory;
			this.IntermediateDirectoryNoArch = IntermediateDirectoryNoArch;

			ModuleApiDefine = Name.ToUpperInvariant() + "_API";

			HashSet<string> PublicPreBuildLibraries = HashSetFromOptionalEnumerableStringParameter(Rules.PublicPreBuildLibraries);
			PublicDefinitions = HashSetFromOptionalEnumerableStringParameter(Rules.PublicDefinitions);
			PublicIncludePaths = CreateDirectoryHashSet(Rules.PublicIncludePaths);
			InternalIncludePaths = CreateDirectoryHashSet(Rules.InternalIncludePaths);
			PublicSystemIncludePaths = CreateDirectoryHashSet(Rules.PublicSystemIncludePaths);
			PublicSystemLibraryPaths = CreateDirectoryHashSet(Rules.PublicSystemLibraryPaths);
			HashSet<string> PublicAdditionalLibraries = HashSetFromOptionalEnumerableStringParameter(Rules.PublicAdditionalLibraries.Union(PublicPreBuildLibraries));
			PublicSystemLibraries = HashSetFromOptionalEnumerableStringParameter(Rules.PublicSystemLibraries);
			PublicFrameworks = HashSetFromOptionalEnumerableStringParameter(Rules.PublicFrameworks);
			PublicWeakFrameworks = HashSetFromOptionalEnumerableStringParameter(Rules.PublicWeakFrameworks);

			foreach (string LibraryName in PublicAdditionalLibraries)
			{
				FileItem Library = FileItem.GetItemByPath(LibraryName);
				if (Library.Exists)
				{
					// if the library path is fully qualified we just add it, this is the preferred method of adding a library
					FileReference Location = Library.Location;
					PublicLibraries.Add(Location);
				}
				else if (PublicPreBuildLibraries.Contains(LibraryName))
				{
					Logger.LogDebug("Library '{LibraryName}' was not resolvable to a file when used in Module '{Name}'.  Be sure to add either a TargetRules.PreBuildSteps entry or a TargetRules.PreBuildTargets entry to assure it is built for your target.", LibraryName, Name);
					PublicLibraries.Add(Library.Location);
				}
				else
				{
					// the library path does not seem to be resolvable as is, lets warn about it as dependency checking will not work for it
					LogWarningOrThrowError(Logger, Rules.Target.DefaultWarningLevel, "Library '{0}' was not resolvable to a file when used in Module '{1}', assuming it is a filename and will search library paths for it. This is slow and dependency checking will not work for it. Please update reference to be fully qualified alternatively use PublicSystemLibraryPaths if you do intended to use this slow path to suppress this warning. ", LibraryName, Name);
					PublicSystemLibraries.Add(LibraryName);
				}
			}

			PublicAdditionalFrameworks = new HashSet<UEBuildFramework>();
			if (Rules.PublicAdditionalFrameworks != null)
			{
				foreach (ModuleRules.Framework FrameworkRules in Rules.PublicAdditionalFrameworks)
				{
					bool bLinkFramework = FrameworkRules.Mode == ModuleRules.Framework.FrameworkMode.Link || FrameworkRules.Mode == ModuleRules.Framework.FrameworkMode.LinkAndCopy;
					bool bCopyFramework = FrameworkRules.Mode == ModuleRules.Framework.FrameworkMode.Copy || FrameworkRules.Mode == ModuleRules.Framework.FrameworkMode.LinkAndCopy;

					UEBuildFramework Framework;
					if (FrameworkRules.IsZipFile())
					{
						// If FrameworkPath ends in .zip, it needs to be extracted
						Framework = new UEBuildFramework(
							Name: FrameworkRules.Name,
							ZipFile: FileReference.Combine(ModuleDirectory, FrameworkRules.Path),
							OutputDirectory: DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate", "UnzippedFrameworks", FrameworkRules.Name, Path.GetFileNameWithoutExtension(FrameworkRules.Path)),
							CopyBundledAssets: FrameworkRules.CopyBundledAssets,
							bLinkFramework, bCopyFramework);
					}
					else
					{
						// Framework on disk
						Framework = new UEBuildFramework(
							Name: FrameworkRules.Name,
							FrameworkDirectory: DirectoryReference.Combine(ModuleDirectory, FrameworkRules.Path),
							CopyBundledAssets: FrameworkRules.CopyBundledAssets,
							bLinkFramework, bCopyFramework, Logger);
					}
					PublicAdditionalFrameworks.Add(Framework);
				}
			}

			PublicAdditionalBundleResources = Rules.AdditionalBundleResources == null ? new HashSet<UEBuildBundleResource>() : new HashSet<UEBuildBundleResource>(Rules.AdditionalBundleResources.Select(x => new UEBuildBundleResource(x)));
			PublicDelayLoadDLLs = HashSetFromOptionalEnumerableStringParameter(Rules.PublicDelayLoadDLLs);
			if (Rules.bUsePrecompiled)
			{
				PrivateIncludePaths = new HashSet<DirectoryReference>();
			}
			else
			{
				PrivateIncludePaths = CreateDirectoryHashSet(Rules.PrivateIncludePaths);
			}

			RestrictedFoldersAllowList = new HashSet<DirectoryReference>(Rules.AllowedRestrictedFolders.Select(x => DirectoryReference.Combine(ModuleDirectory, x)));

			AliasRestrictedFolders = new Dictionary<string, string>(Rules.AliasRestrictedFolders);

			DependenciesToSkipPerArchitecture = Rules.DependenciesToSkipPerArchitecture;

			// get the module directories from the module
			ModuleDirectories = Rules.GetAllModuleDirectories();

			foreach (DirectoryItem Directory in ModuleDirectories.Select(x => DirectoryItem.GetItemByDirectoryReference(x)))
			{
				NatvisFiles.AddRange(Directory.EnumerateFiles().Where(x => x.HasExtension(".natvis") || x.HasExtension(".natstepfilter")));
			}
		}

		/// <summary>
		/// Log a warning or throw an error message
		/// </summary>
		/// <param name="Logger"></param>
		/// <param name="Level"></param>
		/// <param name="Format"></param>
		/// <param name="Args"></param>
		static void LogWarningOrThrowError(ILogger Logger, WarningLevel Level, string Format, params object[] Args)
		{
			if (Level == WarningLevel.Error)
			{
				throw new BuildException(Format, Args);
			}
			else if (Level == WarningLevel.Warning)
			{
				Logger.LogWarning("{Message}", String.Format(Format, Args));
			}
		}

		/// <summary>
		/// Determines if a file is part of the given module
		/// </summary>
		/// <param name="Location">Path to the file</param>
		/// <returns>True if the file is part of this module</returns>
		public virtual bool ContainsFile(FileReference Location)
		{
			return ModuleDirectories.Any(x => Location.IsUnderDirectory(x));
		}

		/// <summary>
		/// Returns a list of this module's dependencies.
		/// </summary>
		/// <returns>An enumerable containing the dependencies of the module.</returns>
		public HashSet<UEBuildModule> GetDependencies(bool bWithIncludePathModules, bool bWithDynamicallyLoadedModules)
		{
			HashSet<UEBuildModule> Modules = new HashSet<UEBuildModule>();
			Modules.UnionWith(PublicDependencyModules ?? new());
			Modules.UnionWith(PrivateDependencyModules ?? new());
			if (bWithIncludePathModules)
			{
				Modules.UnionWith(PublicIncludePathModules ?? new());
				Modules.UnionWith(PrivateIncludePathModules ?? new());
			}
			if (bWithDynamicallyLoadedModules)
			{
				Modules.UnionWith(DynamicallyLoadedModules ?? new());
			}
			return Modules;
		}

		/// <summary>
		/// Returns a list of this module's frameworks.
		/// </summary>
		/// <returns>A List containing the frameworks this module requires.</returns>
		public List<string> GetPublicFrameworks()
		{
			return new List<string>(PublicFrameworks);
		}

		/// <summary>
		/// Returns a list of this module's immediate dependencies.
		/// </summary>
		/// <returns>An enumerable containing the dependencies of the module.</returns>
		public IEnumerable<UEBuildModule> GetDirectDependencyModules()
		{
			return PublicDependencyModules!.Concat(PrivateDependencyModules!).Concat(DynamicallyLoadedModules!);
		}

		/// <summary>
		/// Converts an optional string list parameter to a well-defined hash set.
		/// </summary>
		protected HashSet<DirectoryReference> CreateDirectoryHashSet(IEnumerable<string> InEnumerableStrings)
		{
			HashSet<DirectoryReference> Directories = new HashSet<DirectoryReference>();
			if (InEnumerableStrings != null)
			{
				foreach (string InputString in InEnumerableStrings)
				{
					DirectoryReference Dir = new DirectoryReference(ExpandPathVariables(InputString, null, null));
					if (DirectoryLookupCache.DirectoryExists(Dir))
					{
						Directories.Add(Dir);
					}
					else
					{
						Log.TraceWarningTask(RulesFile, $"Referenced directory '{Dir}' does not exist.");
					}
				}
			}
			return Directories;
		}

		/// <summary>
		/// Converts an optional string list parameter to a well-defined hash set.
		/// </summary>
		protected HashSet<string> HashSetFromOptionalEnumerableStringParameter(IEnumerable<string> InEnumerableStrings)
		{
			return InEnumerableStrings == null ? new HashSet<string>() : new HashSet<string>(InEnumerableStrings.Select(x => ExpandPathVariables(x, null, null)));
		}

		/// <summary>
		/// Determines whether this module has a circular dependency on the given module
		/// </summary>
		public bool HasCircularDependencyOn(string ModuleName)
		{
			return Rules.CircularlyReferencedDependentModules.Contains(ModuleName);
		}

		/// <summary>
		/// Enumerates additional build products which may be produced by this module. Some platforms (eg. Mac, Linux) can link directly against .so/.dylibs, but they 
		/// are also copied to the output folder by the toolchain.
		/// </summary>
		/// <param name="Libraries">List to which libraries required by this module are added</param>
		/// <param name="BundleResources">List of bundle resources required by this module</param>
		public void GatherAdditionalResources(List<string> Libraries, List<UEBuildBundleResource> BundleResources)
		{
			Libraries.AddRange(PublicLibraries.Select(x => x.FullName));
			Libraries.AddRange(PublicSystemLibraries);
			BundleResources.AddRange(PublicAdditionalBundleResources);
		}

		/// <summary>
		/// Determines the distribution level of a module based on its directory and includes.
		/// </summary>
		/// <param name="RootDirectories">The set of additional paths to check, if available</param>
		/// <returns>Map of the restricted folder types to the first found instance</returns>
		public Dictionary<RestrictedFolder, DirectoryReference> FindRestrictedFolderReferences(List<DirectoryReference> RootDirectories)
		{
			Dictionary<RestrictedFolder, DirectoryReference> References = new Dictionary<RestrictedFolder, DirectoryReference>();
			if (!Rules.bLegalToDistributeObjectCode)
			{
				// Find all the directories that this module references
				HashSet<DirectoryReference> ReferencedDirs = new HashSet<DirectoryReference>();
				GetReferencedDirectories(ReferencedDirs);

				// Remove all the allow listed folders
				ReferencedDirs.ExceptWith(RestrictedFoldersAllowList);
				ReferencedDirs.ExceptWith(PublicDependencyModules!.SelectMany(x => x.RestrictedFoldersAllowList));
				ReferencedDirs.ExceptWith(PrivateDependencyModules!.SelectMany(x => x.RestrictedFoldersAllowList));

				// Add flags for each of them
				foreach (DirectoryReference ReferencedDir in ReferencedDirs)
				{
					// Find the base directory containing this reference
					DirectoryReference? BaseDir = RootDirectories.FirstOrDefault(x => ReferencedDir.IsUnderDirectory(x));
					// @todo platplug does this need to check platform extension engine directories? what are ReferencedDir's here?
					if (BaseDir == null)
					{
						continue;
					}

					// Add references to each of the restricted folders
					List<RestrictedFolder> Folders = RestrictedFolders.FindRestrictedFolders(BaseDir, ReferencedDir);
					foreach (RestrictedFolder Folder in Folders)
					{
						if (!References.ContainsKey(Folder))
						{
							References.Add(Folder, ReferencedDir);
						}
					}
				}
			}
			return References;
		}

		/// <summary>
		/// Finds all the directories that this folder references when building
		/// </summary>
		/// <param name="Directories">Set of directories to add to</param>
		protected virtual void GetReferencedDirectories(HashSet<DirectoryReference> Directories)
		{
			Directories.Add(ModuleDirectory);

			foreach (DirectoryReference PublicIncludePath in PublicIncludePaths)
			{
				Directories.Add(PublicIncludePath);
			}
			foreach (DirectoryReference InternalIncludePath in InternalIncludePaths)
			{
				Directories.Add(InternalIncludePath);
			}
			foreach (DirectoryReference PrivateIncludePath in PrivateIncludePaths)
			{
				Directories.Add(PrivateIncludePath);
			}
			foreach (DirectoryReference PublicSystemIncludePath in PublicSystemIncludePaths)
			{
				Directories.Add(PublicSystemIncludePath);
			}
			foreach (DirectoryReference PublicSystemLibraryPath in PublicSystemLibraryPaths)
			{
				Directories.Add(PublicSystemLibraryPath);
			}
		}

		/// <summary>
		/// Find all the modules which affect the private compile environment.
		/// </summary>
		/// <param name="ModuleToIncludePathsOnlyFlag"></param>
		protected void FindModulesInPrivateCompileEnvironment(Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			// Add in all the modules that are only in the private compile environment
			foreach (UEBuildModule PrivateDependencyModule in PrivateDependencyModules!)
			{
				PrivateDependencyModule.FindModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);
			}
			foreach (UEBuildModule PrivateIncludePathModule in PrivateIncludePathModules!)
			{
				PrivateIncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);
			}

			// Add the modules in the public compile environment
			FindModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);
		}

		/// <summary>
		/// Find all the modules which affect the public compile environment. 
		/// </summary>
		/// <param name="ModuleToIncludePathsOnlyFlag"></param>
		protected void FindModulesInPublicCompileEnvironment(Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			//
			bool bModuleIncludePathsOnly;
			if (ModuleToIncludePathsOnlyFlag.TryGetValue(this, out bModuleIncludePathsOnly) && !bModuleIncludePathsOnly)
			{
				return;
			}

			ModuleToIncludePathsOnlyFlag[this] = false;

			foreach (UEBuildModule DependencyModule in PublicDependencyModules!)
			{
				DependencyModule.FindModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);
			}

			// Now add an include paths from modules with header files that we need access to, but won't necessarily be importing
			foreach (UEBuildModule IncludePathModule in PublicIncludePathModules!)
			{
				IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);
			}
		}

		/// <summary>
		/// Find all the modules which affect the public compile environment. Searches through 
		/// </summary>
		/// <param name="ModuleToIncludePathsOnlyFlag"></param>
		protected void FindIncludePathModulesInPublicCompileEnvironment(Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag)
		{
			if (!ModuleToIncludePathsOnlyFlag.ContainsKey(this))
			{
				// Add this module to the list
				ModuleToIncludePathsOnlyFlag.Add(this, true);

				// Include any of its public include path modules in the compile environment too
				foreach (UEBuildModule IncludePathModule in PublicIncludePathModules!)
				{
					IncludePathModule.FindIncludePathModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);
				}
			}
		}

		private void AddIncludePaths(HashSet<DirectoryReference> IncludePaths, HashSet<DirectoryReference> IncludePathsToAdd)
		{
			// Need to check whether directories exist to avoid bloating compiler command line with generated code directories
			IncludePaths.UnionWith(IncludePathsToAdd);
		}

		/// <summary>
		/// Add definitions from source to target. This code also reformats defines to match strict rules which does not allow space before and after '='
		/// </summary>
		protected void AddDefinitions(List<string> Target, HashSet<string> Source)
		{
			StringBuilder Builder = new();
			foreach (string Def in Source)
			{
				string FixedDef = Def;
				int IndexOfAssign = Def.IndexOf("=");
				if (IndexOfAssign != -1)
				{
					ReadOnlySpan<char> DefSpan = Def.AsSpan();
					ReadOnlySpan<char> Name = DefSpan.Slice(0, IndexOfAssign);
					ReadOnlySpan<char> NameTrim = Name.Trim();
					ReadOnlySpan<char> Value = DefSpan.Slice(IndexOfAssign + 1);
					ReadOnlySpan<char> ValueTrim = Value.Trim();
					if (Name.Length != NameTrim.Length || Value.Length != ValueTrim.Length)
					{
						Builder.Clear();
						Builder.Append(NameTrim);
						Builder.Append('=');
						Builder.Append(ValueTrim);
						FixedDef = Builder.ToString();
					}
				}
				Target.Add(FixedDef);
			}
		}

		/// <summary>
		/// Sets up the environment for compiling any module that includes the public interface of this module.
		/// </summary>
		public virtual void AddModuleToCompileEnvironment(
			UEBuildModule? SourceModule,
			UEBuildBinary? SourceBinary,
			HashSet<DirectoryReference> IncludePaths,
			HashSet<DirectoryReference> SystemIncludePaths,
			HashSet<DirectoryReference> ModuleInterfacePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks,
			List<FileItem> AdditionalPrerequisites,
			bool bLegacyPublicIncludePaths,
			bool bLegacyParentIncludePaths
			)
		{
			// Add this module's public include paths and definitions
			AddIncludePaths(IncludePaths, PublicIncludePaths);

			// Add the module's parent directory to the include path, so we can root #includes from generated source files to it. Not recommended (Use BuildSetting.V3 or later)
			if (bLegacyParentIncludePaths)
			{
				AddIncludePaths(IncludePaths, LegacyParentIncludePaths);
				IncludePaths.Add(ModuleDirectory.ParentDirectory!);
			}

			// Add this module's legacy public include paths. Not recommended (Use BuildSetting.V2 or later)
			if (bLegacyPublicIncludePaths)
			{
				AddIncludePaths(IncludePaths, LegacyPublicIncludePaths);
			}

			// Add this module's internal include paths, only if the scope contains the same as the SourceModule's scope or if this module is an engine module
			if (SourceModule != null && (Rules.Context.Scope.Contains(SourceModule.Rules.Context.Scope) || Rules.bTreatAsEngineModule))
			{
				AddIncludePaths(IncludePaths, InternalIncludePaths);
			}

			// Add this module's public system include paths
			SystemIncludePaths.UnionWith(PublicSystemIncludePaths);

			// Add this module's public definitions
			AddDefinitions(Definitions, PublicDefinitions);

			// Add the import or export declaration for the module
			if (Rules.Type == ModuleRules.ModuleType.CPlusPlus)
			{
				if (Rules.Target.LinkType == TargetLinkType.Monolithic)
				{
					if (Rules.Target.bShouldCompileAsDLL && (Rules.Target.bHasExports || Rules.ModuleSymbolVisibility == ModuleRules.SymbolVisibility.VisibileForDll))
					{
						Definitions.Add(ModuleApiDefine + "=DLLEXPORT");
					}
					else
					{
						Definitions.Add(ModuleApiDefine + "=");
					}
				}
				else if (Binary == null || SourceBinary != Binary)
				{
					Definitions.Add(ModuleApiDefine + "=DLLIMPORT");
				}
				else if (!Binary.bAllowExports)
				{
					Definitions.Add(ModuleApiDefine + "=");
				}
				else
				{
					Definitions.Add(ModuleApiDefine + "=DLLEXPORT");
				}
			}

			// Add the additional frameworks so that the compiler can know about their #include paths
			AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);

			// Add any generated type library headers
			if (Rules.TypeLibraries.Count > 0)
			{
				IncludePaths.Add(IntermediateDirectory);
				foreach (ModuleRules.TypeLibrary TypeLibrary in Rules.TypeLibraries)
				{
					AdditionalPrerequisites.Add(FileItem.GetItemByFileReference(FileReference.Combine(IntermediateDirectory, TypeLibrary.Header)));
				}
			}
		}

		/// <summary>
		/// Sets up the environment for compiling this module.
		/// </summary>
		protected virtual void SetupPrivateCompileEnvironment(
			HashSet<DirectoryReference> IncludePaths,
			HashSet<DirectoryReference> SystemIncludePaths,
			HashSet<DirectoryReference> ModuleInterfacePaths,
			List<string> Definitions,
			List<UEBuildFramework> AdditionalFrameworks,
			List<FileItem> AdditionalPrerequisites,
			bool bWithLegacyPublicIncludePaths,
			bool bWithLegacyParentIncludePaths
			)
		{
			// Add this module's private include paths and definitions.
			IncludePaths.UnionWith(PrivateIncludePaths);

			// Find all the modules that are part of the public compile environment for this module.
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPrivateCompileEnvironment(ModuleToIncludePathsOnlyFlag);

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in ModuleToIncludePathsOnlyFlag.Keys)
			{
				Module.AddModuleToCompileEnvironment(this, Binary, IncludePaths, SystemIncludePaths, ModuleInterfacePaths, Definitions, AdditionalFrameworks, AdditionalPrerequisites, bWithLegacyPublicIncludePaths, bWithLegacyParentIncludePaths);
			}
		}

		/// <summary>
		/// Expand path variables within the context of this module
		/// </summary>
		/// <param name="Path">Path to expand variables within</param>
		/// <param name="BinaryOutputDir">Directory containing the binary that links this module. May be mull.</param>
		/// <param name="TargetOutputDir">Directory containing the output executable. May be null.</param>
		/// <returns>The path with variables expanded</returns>
		public string ExpandPathVariables(string Path, DirectoryReference? BinaryOutputDir, DirectoryReference? TargetOutputDir)
		{
			if (Path.StartsWith("$(", StringComparison.Ordinal))
			{
				int StartIdx = 2;
				for (int EndIdx = StartIdx; EndIdx < Path.Length; EndIdx++)
				{
					if (Path[EndIdx] == ')')
					{
						if (MatchVariableName(Path, StartIdx, EndIdx, "EngineDir"))
						{
							Path = Unreal.EngineDirectory + Path.Substring(EndIdx + 1);
						}
						else if (MatchVariableName(Path, StartIdx, EndIdx, "ProjectDir"))
						{
							if (Rules.Target.ProjectFile == null)
							{
								Path = Unreal.EngineDirectory + Path.Substring(EndIdx + 1);
							}
							else
							{
								Path = Rules.Target.ProjectFile.Directory + Path.Substring(EndIdx + 1);
							}
						}
						else if (MatchVariableName(Path, StartIdx, EndIdx, "ModuleDir"))
						{
							Path = Rules.ModuleDirectory + Path.Substring(EndIdx + 1);
						}
						else if (MatchVariableName(Path, StartIdx, EndIdx, "PluginDir"))
						{
							Path = Rules.PluginDirectory + Path.Substring(EndIdx + 1);
						}
						else if (BinaryOutputDir != null && MatchVariableName(Path, StartIdx, EndIdx, "BinaryOutputDir"))
						{
							Path = BinaryOutputDir.FullName + Path.Substring(EndIdx + 1);
						}
						else if (TargetOutputDir != null && MatchVariableName(Path, StartIdx, EndIdx, "TargetOutputDir"))
						{
							Path = TargetOutputDir.FullName + Path.Substring(EndIdx + 1);
						}
						else
						{
							string Name = Path.Substring(StartIdx, EndIdx - StartIdx);
							string? Value = Environment.GetEnvironmentVariable(Name);
							if (String.IsNullOrEmpty(Value))
							{
								throw new BuildException("Environment variable '{0}' is not defined (referenced by {1})", Name, Rules.File);
							}
							Path = Value + Path.Substring(EndIdx + 1);
						}
						break;
					}
				}
			}
			return Path;
		}

		/// <summary>
		/// Match a variable name within a path
		/// </summary>
		/// <param name="Path">The path variable</param>
		/// <param name="StartIdx">Start index of the substring to match</param>
		/// <param name="EndIdx">End index of the substring to match</param>
		/// <param name="Name">Variable name to compare against</param>
		/// <returns>True if the variable name matches</returns>
		private bool MatchVariableName(string Path, int StartIdx, int EndIdx, string Name)
		{
			return Name.Length == EndIdx - StartIdx && String.Compare(Path, StartIdx, Name, 0, EndIdx - StartIdx) == 0;
		}

		/// <summary>
		/// Expand path variables within the context of this module
		/// </summary>
		/// <param name="Paths">Path to expand variables within</param>
		/// <param name="BinaryDir">Directory containing the binary that links this module. May be mull.</param>
		/// <param name="ExeDir">Directory containing the output executable. May be null.</param>
		/// <returns>The path with variables expanded</returns>
		private IEnumerable<string> ExpandPathVariables(IEnumerable<string> Paths, DirectoryReference? BinaryDir, DirectoryReference? ExeDir)
		{
			foreach (string Path in Paths)
			{
				yield return ExpandPathVariables(Path, BinaryDir, ExeDir);
			}
		}

		/// <summary>
		/// Sets up the environment for linking any module that includes the public interface of this module.
		/// </summary>
		protected virtual void SetupPublicLinkEnvironment(
			UEBuildBinary? SourceBinary,
			List<FileReference> Libraries,
			List<DirectoryReference> SystemLibraryPaths,
			List<string> SystemLibraries,
			List<string> RuntimeLibraryPaths,
			List<string> Frameworks,
			List<string> WeakFrameworks,
			List<UEBuildFramework> AdditionalFrameworks,
			List<UEBuildBundleResource> AdditionalBundleResources,
			List<string> DelayLoadDLLs,
			List<UEBuildBinary> BinaryDependencies,
			Dictionary<string, HashSet<UnrealArch>> DependenciesToSkip,
			HashSet<UEBuildModule> VisitedModules,
			DirectoryReference ExeDir
			)
		{
			// There may be circular dependencies in compile dependencies, so we need to avoid reentrance.
			if (VisitedModules.Add(this))
			{
				// Add this module's binary to the binary dependencies.
				if (Binary != null
					&& Binary != SourceBinary
					&& !BinaryDependencies.Contains(Binary))
				{
					BinaryDependencies.Add(Binary);
				}

				// If this module belongs to a static library that we are not currently building, recursively add the link environment settings for all of its dependencies too.
				// Keep doing this until we reach a module that is not part of a static library (or external module, since they have no associated binary).
				// Static libraries do not contain the symbols for their dependencies, so we need to recursively gather them to be linked into other binary types.
				bool bIsBuildingAStaticLibrary = (SourceBinary != null && SourceBinary.Type == UEBuildBinaryType.StaticLibrary);
				bool bIsModuleBinaryAStaticLibrary = (Binary != null && Binary.Type == UEBuildBinaryType.StaticLibrary);
				if (!bIsBuildingAStaticLibrary && bIsModuleBinaryAStaticLibrary)
				{
					// Gather all dependencies and recursively call SetupPublicLinkEnvironmnet
					List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
					AllDependencyModules.AddRange(PrivateDependencyModules!);
					AllDependencyModules.AddRange(PublicDependencyModules!);

					foreach (UEBuildModule DependencyModule in AllDependencyModules)
					{
						bool bIsExternalModule = (DependencyModule as UEBuildModuleExternal != null);
						bool bIsInStaticLibrary = (DependencyModule.Binary != null && DependencyModule.Binary.Type == UEBuildBinaryType.StaticLibrary);
						if (bIsExternalModule || bIsInStaticLibrary)
						{
							DependencyModule.SetupPublicLinkEnvironment(SourceBinary, Libraries, SystemLibraryPaths, SystemLibraries, RuntimeLibraryPaths, Frameworks, WeakFrameworks,
								AdditionalFrameworks, AdditionalBundleResources, DelayLoadDLLs, BinaryDependencies, DependenciesToSkip, VisitedModules, ExeDir);
						}
					}
				}

				// Add this module's public include library paths and additional libraries.
				Libraries.AddRange(PublicLibraries);
				SystemLibraryPaths.AddRange(PublicSystemLibraryPaths);
				SystemLibraries.AddRange(PublicSystemLibraries);
				RuntimeLibraryPaths.AddRange(ExpandPathVariables(Rules.PublicRuntimeLibraryPaths, SourceBinary?.OutputDir, ExeDir));
				Frameworks.AddRange(PublicFrameworks);
				WeakFrameworks.AddRange(PublicWeakFrameworks);
				AdditionalBundleResources.AddRange(PublicAdditionalBundleResources);
				AdditionalFrameworks.AddRange(PublicAdditionalFrameworks);
				DelayLoadDLLs.AddRange(PublicDelayLoadDLLs);

				// merge in to the outgoing dictionary
				foreach (KeyValuePair<string, List<UnrealArch>> Pair in DependenciesToSkipPerArchitecture)
				{
					if (!DependenciesToSkip.ContainsKey(Pair.Key))
					{
						DependenciesToSkip[Pair.Key] = new HashSet<UnrealArch>();
					}
					DependenciesToSkip[Pair.Key] = DependenciesToSkip[Pair.Key].Union(Pair.Value).ToHashSet();
				}
			}
		}

		/// <summary>
		/// Sets up the environment for linking this module.
		/// </summary>
		public virtual void SetupPrivateLinkEnvironment(
			UEBuildBinary SourceBinary,
			LinkEnvironment LinkEnvironment,
			List<UEBuildBinary> BinaryDependencies,
			HashSet<UEBuildModule> VisitedModules,
			DirectoryReference ExeDir
			)
		{
			// Add the private rpaths
			LinkEnvironment.RuntimeLibraryPaths.AddRange(ExpandPathVariables(Rules.PrivateRuntimeLibraryPaths, SourceBinary.OutputDir, ExeDir));

			// Allow the module's public dependencies to add library paths and additional libraries to the link environment.
			SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.Libraries, LinkEnvironment.SystemLibraryPaths, LinkEnvironment.SystemLibraries, LinkEnvironment.RuntimeLibraryPaths, LinkEnvironment.Frameworks, LinkEnvironment.WeakFrameworks,
				LinkEnvironment.AdditionalFrameworks, LinkEnvironment.AdditionalBundleResources, LinkEnvironment.DelayLoadDLLs, BinaryDependencies, LinkEnvironment.DependenciesToSkipPerArchitecture, VisitedModules, ExeDir);

			// Also allow the module's public and private dependencies to modify the link environment.
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>();
			AllDependencyModules.AddRange(PrivateDependencyModules!);
			AllDependencyModules.AddRange(PublicDependencyModules!);

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				DependencyModule.SetupPublicLinkEnvironment(SourceBinary, LinkEnvironment.Libraries, LinkEnvironment.SystemLibraryPaths, LinkEnvironment.SystemLibraries, LinkEnvironment.RuntimeLibraryPaths, LinkEnvironment.Frameworks, LinkEnvironment.WeakFrameworks,
					LinkEnvironment.AdditionalFrameworks, LinkEnvironment.AdditionalBundleResources, LinkEnvironment.DelayLoadDLLs, BinaryDependencies, LinkEnvironment.DependenciesToSkipPerArchitecture, VisitedModules, ExeDir);
			}

			// Add all the additional properties
			LinkEnvironment.AdditionalProperties.AddRange(Rules.AdditionalPropertiesForReceipt.Inner);

			// this is a link-time property that needs to be accumulated (if any modules contributing to this module is ignoring, all are ignoring)
			LinkEnvironment.bIgnoreUnresolvedSymbols |= Rules.bIgnoreUnresolvedSymbols;
		}

		/// <summary>
		/// Compiles the module, and returns a list of files output by the compiler.
		/// </summary>
		public virtual List<FileItem> Compile(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, ISourceFileWorkingSet WorkingSet, IActionGraphBuilder Graph, ILogger Logger)
		{
			// Generate type libraries for Windows
			foreach (ModuleRules.TypeLibrary TypeLibrary in Rules.TypeLibraries)
			{
				FileReference OutputFile = FileReference.Combine(IntermediateDirectory, TypeLibrary.Header);
				ToolChain.GenerateTypeLibraryHeader(CompileEnvironment, TypeLibrary, OutputFile, Graph);
			}

			return new List<FileItem>();
		}

		public IEnumerable<FileItem> CopyDebuggerVisualizers(UEToolChain ToolChain, IActionGraphBuilder Graph, ILogger Logger)
		{
			List<FileItem> Results = new();
			foreach (FileItem NatvisSourceFile in NatvisFiles)
			{
				FileItem? Item = ToolChain.CopyDebuggerVisualizer(NatvisSourceFile, IntermediateDirectory, Graph);
				if (Item != null)
				{
					Results.Add(Item);
				}
			}
			return Results;
		}
		
		public void LinkDebuggerVisualizers(List<FileItem> OutFiles, UEToolChain ToolChain, ILogger Logger)
		{
			foreach (FileItem NatvisSourceFile in NatvisFiles)
			{
				FileItem? Item = ToolChain.LinkDebuggerVisualizer(NatvisSourceFile, IntermediateDirectory);
				if (Item != null)
				{
					OutFiles.Add(Item);
				}
			}
		}

		// Object interface.
		public override string ToString()
		{
			return Name;
		}

		/// <summary>
		/// Finds the modules referenced by this module which have not yet been bound to a binary
		/// </summary>
		/// <returns>List of unbound modules</returns>
		public List<UEBuildModule> GetUnboundReferences()
		{
			List<UEBuildModule> Modules = new List<UEBuildModule>();
			Modules.AddRange(PrivateDependencyModules!.Where(x => x.Binary == null));
			Modules.AddRange(PublicDependencyModules!.Where(x => x.Binary == null));
			return Modules;
		}

		/// <summary>
		/// Gets all of the modules referenced by this module
		/// </summary>
		/// <param name="ReferencedModules">Hash of all referenced modules with their addition index.</param>
		/// <param name="IgnoreReferencedModules">Hashset used to ignore modules which are already added to the list</param>
		/// <param name="bIncludeDynamicallyLoaded">True if dynamically loaded modules (and all of their dependent modules) should be included.</param>
		/// <param name="bForceCircular">True if circular dependencies should be processed</param>
		/// <param name="bOnlyDirectDependencies">True to return only this module's direct dependencies</param>
		public void GetAllDependencyModules(List<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludeDynamicallyLoaded, bool bForceCircular, bool bOnlyDirectDependencies)
		{
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>(PrivateDependencyModules!.Count + PublicDependencyModules!.Count + (bIncludeDynamicallyLoaded ? DynamicallyLoadedModules!.Count : 0));
			AllDependencyModules.AddRange(PrivateDependencyModules!);
			AllDependencyModules.AddRange(PublicDependencyModules!);
			if (bIncludeDynamicallyLoaded)
			{
				AllDependencyModules.AddRange(DynamicallyLoadedModules!);
			}

			foreach (UEBuildModule DependencyModule in AllDependencyModules)
			{
				// Don't follow circular back-references!
				if (bForceCircular || !HasCircularDependencyOn(DependencyModule.Name))
				{
					if (IgnoreReferencedModules.Add(DependencyModule))
					{
						if (!bOnlyDirectDependencies)
						{
							// Recurse into dependent modules first
							DependencyModule.GetAllDependencyModules(ReferencedModules, IgnoreReferencedModules, bIncludeDynamicallyLoaded, bForceCircular, bOnlyDirectDependencies);
						}

						ReferencedModules.Add(DependencyModule);
					}
				}
			}
		}

		/// <summary>
		/// Gathers all the module dependencies a PCH would have
		/// </summary>
		/// <param name="bIncludePrivateModules">Whether to include private modules.</param>
		/// <param name="bForceCircular">True if circular dependencies should be processed</param>
		public HashSet<UEBuildModule> GetAllDependencyModulesForPCH(bool bIncludePrivateModules, bool bForceCircular)
		{
			HashSet<UEBuildModule> ReferencedModules = new HashSet<UEBuildModule>();
			HashSet<UEBuildModule> IgnoreReferencedModules = new HashSet<UEBuildModule> { this };
			InternalGetAllDependencyModulesForPCH(ReferencedModules, IgnoreReferencedModules, bIncludePrivateModules, bForceCircular);
			return ReferencedModules;
		}

		/// <summary>
		/// Internal function that gathers all the module dependencies a PCH would have
		/// </summary>
		/// <param name="ReferencedModules">Hash of all referenced modules with their addition index.</param>
		/// <param name="IgnoreReferencedModules">Hashset used to ignore modules which are already added to the list</param>
		/// <param name="bIncludePrivateModules">Whether to include private modules.</param>
		/// <param name="bForceCircular">True if circular dependencies should be processed</param>
		private void InternalGetAllDependencyModulesForPCH(HashSet<UEBuildModule> ReferencedModules, HashSet<UEBuildModule> IgnoreReferencedModules, bool bIncludePrivateModules, bool bForceCircular)
		{
			List<UEBuildModule> AllDependencyModules = new List<UEBuildModule>(
				((bIncludePrivateModules && PrivateDependencyModules != null) ? PrivateDependencyModules.Count : 0) +
				(PublicDependencyModules != null ? PublicDependencyModules.Count : 0) +
				(PublicIncludePathModules != null ? PublicIncludePathModules!.Count : 0)
				);
			if (bIncludePrivateModules && PrivateDependencyModules != null)
			{
				AllDependencyModules.AddRange(PrivateDependencyModules);
			}
			if (PublicDependencyModules != null)
			{
				AllDependencyModules.AddRange(PublicDependencyModules!);
			}
			if (PublicIncludePathModules != null)
			{
				AllDependencyModules.AddRange(PublicIncludePathModules);
			}

			foreach (UEBuildModule DependencyModule in AllDependencyModules.Distinct())
			{
				// Don't follow circular back-references!
				if (bForceCircular || !HasCircularDependencyOn(DependencyModule.Name))
				{
					if (IgnoreReferencedModules.Add(DependencyModule))
					{
						// Recurse into dependent modules first
						DependencyModule.InternalGetAllDependencyModulesForPCH(ReferencedModules, IgnoreReferencedModules, false, bForceCircular);

						ReferencedModules.Add(DependencyModule);
					}
				}
			}
		}

		public delegate UEBuildModule CreateModuleDelegate(string Name, string ReferenceChain);

		/// <summary>
		/// Public entry point to recursively create a module and all its dependencies
		/// </summary>
		/// <param name="CreateModule"></param>
		/// <param name="ReferenceChain"></param>
		/// <param name="Logger"></param>
		public void RecursivelyCreateModules(CreateModuleDelegate CreateModule, string ReferenceChain, ILogger Logger)
		{
			List<UEBuildModule> ReferenceStack = new List<UEBuildModule>();
			RecursivelyCreateModules(CreateModule, ReferenceChain, ReferenceStack, Logger);
		}

		/// <summary>
		/// Creates all the modules required for this target
		/// </summary>
		/// <param name="CreateModule">Delegate to create a module with a given name</param>
		/// <param name="ReferenceChain">Chain of references before reaching this module</param>
		/// <param name="ReferenceStack">Stack of module dependencies that led to this module</param>
		/// <param name="Logger">Logger for output</param>
		protected void RecursivelyCreateModules(CreateModuleDelegate CreateModule, string ReferenceChain, List<UEBuildModule> ReferenceStack, ILogger Logger)
		{
			// Name of this reference
			string ThisRefName = (RulesFile == null) ? Name : RulesFile.GetFileName();

			// Set the reference chain for anything referenced by this module
			string NextReferenceChain = String.Format("{0} -> {1}", ReferenceChain, ThisRefName);

			// Add us to the reference stack
			ReferenceStack.Add(this);

			// Check if this module is invalid for the current target, based on Module attributes
			if (!ModuleRules.IsValidForTarget(Rules.GetType(), Rules.Target, out string? InvalidReason))
			{
				Logger.LogWarning("Warning: Referenced module '{Module}' does not support {InvalidReason} via {ReferenceChain}", ThisRefName, InvalidReason, NextReferenceChain);
			}

			// Create all the referenced modules. This path can be recursive, so we check against PublicDependencyModules to ensure we don't recurse through the 
			// same module twice (it produces better errors if something fails).
			if (PublicDependencyModules == null)
			{
				// Log dependencies if required
				if (Logger.IsEnabled((LogLevel)LogEventType.VeryVerbose))
				{
					Logger.LogTrace("Module {Name} dependencies:", Name);
					LogDependencyNameList("Public:", Rules.PublicDependencyModuleNames, Logger);
					LogDependencyNameList("Private:", Rules.PrivateDependencyModuleNames, Logger);
					LogDependencyNameList("Dynamic:", Rules.DynamicallyLoadedModuleNames, Logger);
					LogDependencyNameList("Public Include Paths:", Rules.PublicIncludePathModuleNames, Logger);
					LogDependencyNameList("Private Include Paths:", Rules.PrivateIncludePathModuleNames, Logger);
				}

				// Create all the dependency modules - pass through the reference stack so we can check for cycles
				RecursivelyCreateModulesByName(Rules.PublicDependencyModuleNames, ref PublicDependencyModules, ref bDependsOnVerse, CreateModule, NextReferenceChain, ReferenceStack, Logger);
				if (Rules.Target.IsTestTarget)
				{
					// Move the test runner dependency to last position to give it the opportunity to build its special dependencies such as CoreUObject, ApplicationCore etc.
					MoveTestsRunnerDependencyToLastPosition();
				}
				RecursivelyCreateModulesByName(Rules.PrivateDependencyModuleNames, ref PrivateDependencyModules, ref bDependsOnVerse, CreateModule, NextReferenceChain, ReferenceStack, Logger);

				// Recursively create all the public include path modules
				HashSet<string> PublicIncludePathModuleNames = new();
				PublicIncludePathModuleNames.UnionWith(Rules.PublicIncludePathModuleNames!);
				PublicIncludePathModuleNames.UnionWith(Rules.PublicDependencyModuleNames!);
				RecursivelyCreateIncludePathModulesByName(PublicIncludePathModuleNames, ref PublicIncludePathModules, ref bDependsOnVerse, CreateModule, NextReferenceChain);

				// Create the private include path modules
				RecursivelyCreateIncludePathModulesByName(Rules.PrivateIncludePathModuleNames, ref PrivateIncludePathModules, ref bDependsOnVerse, CreateModule, NextReferenceChain);

				// Dynamic loads aren't considered a reference chain so start with an empty stack
				RecursivelyCreateModulesByName(Rules.DynamicallyLoadedModuleNames, ref DynamicallyLoadedModules, ref bDependsOnVerse, CreateModule, NextReferenceChain, new List<UEBuildModule>(), Logger);
			}

			// pop us off the current stack
			ReferenceStack.RemoveAt(ReferenceStack.Count - 1);
		}

		/// <summary>
		/// Public entry point to validate a module
		/// </summary>
		/// <param name="ReferenceChain"></param>
		/// <param name="Logger"></param>
		public bool ValidateModule(string ReferenceChain, ILogger Logger)
		{
			List<UEBuildModule> ReferenceStack = new List<UEBuildModule>();
			RecursivelyCheckForCircularReferences(ReferenceChain, ReferenceStack, Logger);

			return false; // No fatal errors
		}

		/// <summary>
		/// Recursively check a module for circular dependencies
		/// </summary>
		/// <param name="ReferenceChain">Chain of references before reaching this module</param>
		/// <param name="ReferenceStack">Stack of module dependencies that led to this module</param>
		/// <param name="Logger">Logger for output</param>
		protected void RecursivelyCheckForCircularReferences(string ReferenceChain, List<UEBuildModule> ReferenceStack, ILogger Logger)
		{
			// Name of this reference
			string ThisRefName = (RulesFile == null) ? Name : RulesFile.GetFileName();

			// Set the reference chain for anything referenced by this module
			string NextReferenceChain = String.Format("{0} -> {1}", ReferenceChain, ThisRefName);

			// We need to check for cycles if this module has already been created and its name is in the stack.
			bool CheckForCycles = PrivateIncludePathModules != null && ReferenceStack.Contains(this);

			UEBuildModule? PreviousModule = ReferenceStack.LastOrDefault();
			ReferenceStackParentModules ??= new();
			if (PreviousModule != null && !ReferenceStackParentModules.Add(PreviousModule))
			{
				return;
			}

			// Add us to the reference stack - note do this before checking for cycles as we allow them if an element in the stack
			// declares the next element at leading to a dependency cycle.
			ReferenceStack.Add(this);

			if (CheckForCycles)
			{
				// We are FeatureModuleA and we know we're already in the list so there must be a circular dependency like this:
				//   Target -> FeatureModuleA -> FeatureModuleB -> BaseModule -> FeatureModuleA
				// In this case it's up to BaseModule to confess that it's going to introduce a 
				// cycle by referencing FeatureModuleA so let's check it did that.
				// (Note: it's *bad* that it is doing this, but it's even worse not to flag these
				// cycles when they're introduced and have a way of tracking them!)

				string? GuiltyModule = null;
				string? VictimModule = null;

				for (int i = 0; i < ReferenceStack.Count - 1; i++)
				{
					UEBuildModule ReferringModule = ReferenceStack.ElementAt(i);
					UEBuildModule TargetModule = ReferenceStack.ElementAt(i + 1);

					if (ReferringModule.HasCircularDependencyOn(TargetModule.Name))
					{
						GuiltyModule = ReferringModule.Name;
						VictimModule = TargetModule.Name;
						break;
					}
				}

				// No module has confessed its guilt, so this is an error.
				if (String.IsNullOrEmpty(GuiltyModule))
				{
					string CycleChain = String.Join(" -> ", ReferenceStack.SkipWhile(x => x != this));
					Logger.LogError("Circular dependency on {Name} detected:\n" +
						"\tFull Route: {FullRoute}\n" +
						"\tCycled Route: is {CycleRoute}\n" +
						"Break this loop by moving dependencies into a separate module or using Private/PublicIncludePathModuleNames to reference declarations\n",
						ThisRefName, NextReferenceChain, CycleChain);
				}
				else
				{
					Logger.LogDebug("Found circular reference to {ThisRefName}, but {GuiltyModule} declares a cycle on {VictimModule} which breaks the chain", ThisRefName, GuiltyModule, VictimModule);
				}
			}
			else
			{
				// Continue checking for circular dependencies until a cycle is found or the graph is exhausted
				PublicDependencyModules?.ForEach(x => x.RecursivelyCheckForCircularReferences(NextReferenceChain, ReferenceStack, Logger));
				PrivateDependencyModules?.ForEach(x => x.RecursivelyCheckForCircularReferences(NextReferenceChain, ReferenceStack, Logger));
			}
			// pop us off the current stack
			ReferenceStack.RemoveAt(ReferenceStack.Count - 1);
		}

		private void MoveTestsRunnerDependencyToLastPosition()
		{
			if (Rules.PrivateDependencyModuleNames.Contains("LowLevelTestsRunner"))
			{
				Rules.PrivateDependencyModuleNames.Remove("LowLevelTestsRunner");
				Rules.PrivateDependencyModuleNames.Add("LowLevelTestsRunner");
			}
		}

		private static void LogDependencyNameList(string Title, List<string> DependencyNameList, ILogger logger)
		{
			logger.LogTrace("  {Title}", Title);
			foreach (string name in DependencyNameList)
			{
				logger.LogTrace("    {Name}", name);
			}
		}

		private static void RecursivelyCreateModulesByName(List<string> ModuleNames, ref List<UEBuildModule>? Modules, ref bool bDependsOnVerse, CreateModuleDelegate CreateModule, string ReferenceChain, List<UEBuildModule> ReferenceStack, ILogger Logger)
		{
			// Check whether the module list is already set. We set this immediately (via the ref) to avoid infinite recursion.
			if (Modules == null)
			{
				Modules = new List<UEBuildModule>();
				for (int i = 0; i < ModuleNames.Count; i++)
				{
					string ModuleName = ModuleNames[i];
					UEBuildModule Module = CreateModule(ModuleName, ReferenceChain);
					if (!Modules.Contains(Module))
					{
						Module.RecursivelyCreateModules(CreateModule, ReferenceChain, ReferenceStack, Logger);
						Modules.Add(Module);
						bDependsOnVerse |= Module.bDependsOnVerse;
					}
				}
			}
		}

		private static void RecursivelyCreateIncludePathModulesByName(IEnumerable<string> ModuleNames, ref List<UEBuildModule>? Modules, ref bool bDependsOnVerse, CreateModuleDelegate CreateModule, string ReferenceChain)
		{
			// Check whether the module list is already set. We set this immediately (via the ref) to avoid infinite recursion.
			if (Modules == null)
			{
				Modules = new List<UEBuildModule>();
				foreach (string ModuleName in ModuleNames)
				{
					UEBuildModule Module = CreateModule(ModuleName, ReferenceChain);
					if (!Modules.Contains(Module))
					{
						// Name of this reference
						string ModuleRefName = (Module.RulesFile == null) ? Module.Name : Module.RulesFile.GetFileName();
						// Set the reference chain for anything referenced by this module
						string NextReferenceChain = String.Format("{0} -> {1} (public include)", ReferenceChain, ModuleRefName);

						HashSet<string> PublicIncludePathModuleNames = new();
						PublicIncludePathModuleNames.UnionWith(Module.Rules.PublicIncludePathModuleNames);
						PublicIncludePathModuleNames.UnionWith(Module.Rules.PublicDependencyModuleNames);
						RecursivelyCreateIncludePathModulesByName(PublicIncludePathModuleNames, ref Module.PublicIncludePathModules, ref Module.bDependsOnVerse, CreateModule, NextReferenceChain);
						Modules.Add(Module);
						bDependsOnVerse |= Module.bDependsOnVerse;
					}
				}
			}
		}

		/// <summary>
		/// Returns valueless API defines (like MODULE_API)
		/// </summary>
		public IEnumerable<string> GetEmptyApiMacros()
		{
			if (Rules.Type == ModuleRules.ModuleType.CPlusPlus)
			{
				return new[] { ModuleApiDefine + "=" };
			}

			return new string[0];
		}

		/// <summary>
		/// Write information about this binary to a JSON file
		/// </summary>
		/// <param name="BinaryOutputDir">The output directory for the binary containing this module</param>
		/// <param name="TargetOutputDir">The output directory for the target executable</param>
		/// <param name="Writer">Writer for this binary's data</param>
		public virtual void ExportJson(DirectoryReference? BinaryOutputDir, DirectoryReference? TargetOutputDir, JsonWriter Writer)
		{
			Writer.WriteValue("Name", Name);
			Writer.WriteValue("Type", Rules.Type.ToString());
			Writer.WriteValue("Directory", ModuleDirectory.FullName);
			Writer.WriteValue("Rules", RulesFile.FullName);
			Writer.WriteValue("PCHUsage", Rules.PCHUsage.ToString());

			if (Rules.PrivatePCHHeaderFile != null)
			{
				Writer.WriteValue("PrivatePCH", FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile).FullName);
			}

			if (Rules.SharedPCHHeaderFile != null)
			{
				Writer.WriteValue("SharedPCH", FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile).FullName);
			}
			Writer.WriteValue("ChainSharedPCH", Rules.Target.bChainPCHs);

			ExportJsonModuleArray(Writer, "PublicDependencyModules", PublicDependencyModules);
			ExportJsonModuleArray(Writer, "PublicIncludePathModules", PublicIncludePathModules);
			ExportJsonModuleArray(Writer, "PrivateDependencyModules", PrivateDependencyModules);
			ExportJsonModuleArray(Writer, "PrivateIncludePathModules", PrivateIncludePathModules);
			ExportJsonModuleArray(Writer, "DynamicallyLoadedModules", DynamicallyLoadedModules);

			ExportJsonStringArray(Writer, "PublicSystemIncludePaths", PublicSystemIncludePaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PublicIncludePaths", PublicIncludePaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "InternalIncludePaths", PublicIncludePaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PrivateIncludePaths", PrivateIncludePaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PublicLibraries", PublicLibraries.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PublicSystemLibraries", PublicSystemLibraries);
			ExportJsonStringArray(Writer, "PublicSystemLibraryPaths", PublicSystemLibraryPaths.Select(x => x.FullName));
			ExportJsonStringArray(Writer, "PublicFrameworks", PublicFrameworks);
			ExportJsonStringArray(Writer, "PublicWeakFrameworks", PublicWeakFrameworks);
			ExportJsonStringArray(Writer, "PublicDelayLoadDLLs", PublicDelayLoadDLLs);
			ExportJsonStringArray(Writer, "PublicDefinitions", PublicDefinitions);

			Writer.WriteArrayStart("CircularlyReferencedModules");
			foreach (string ModuleName in Rules.CircularlyReferencedDependentModules)
			{
				Writer.WriteValue(ModuleName);
			}
			Writer.WriteArrayEnd();

			// Don't add runtime dependencies for modules that aren't being linked in. They may reference BinaryOutputDir, which is invalid.
			if (Binary != null)
			{
				Writer.WriteArrayStart("RuntimeDependencies");
				foreach (ModuleRules.RuntimeDependency RuntimeDependency in Rules.RuntimeDependencies.Inner)
				{
					Writer.WriteObjectStart();
					Writer.WriteValue("Path", ExpandPathVariables(RuntimeDependency.Path, BinaryOutputDir, TargetOutputDir));
					if (RuntimeDependency.SourcePath != null)
					{
						Writer.WriteValue("SourcePath", ExpandPathVariables(RuntimeDependency.SourcePath, BinaryOutputDir, TargetOutputDir));
					}
					Writer.WriteValue("Type", RuntimeDependency.Type.ToString());
					Writer.WriteObjectEnd();
				}
				Writer.WriteArrayEnd();
			}
		}

		/// <summary>
		/// Write an array of module names to a JSON writer
		/// </summary>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="ArrayName">Name of the array property</param>
		/// <param name="Modules">Sequence of modules to write. May be null.</param>
		void ExportJsonModuleArray(JsonWriter Writer, string ArrayName, IEnumerable<UEBuildModule>? Modules)
		{
			Writer.WriteArrayStart(ArrayName);
			if (Modules != null)
			{
				foreach (UEBuildModule Module in Modules)
				{
					Writer.WriteValue(Module.Name);
				}
			}
			Writer.WriteArrayEnd();
		}

		/// <summary>
		/// Write an array of strings to a JSON writer
		/// </summary>
		/// <param name="Writer">Writer for the array data</param>
		/// <param name="ArrayName">Name of the array property</param>
		/// <param name="Strings">Sequence of strings to write. May be null.</param>
		void ExportJsonStringArray(JsonWriter Writer, string ArrayName, IEnumerable<string>? Strings)
		{
			Writer.WriteArrayStart(ArrayName);
			if (Strings != null)
			{
				foreach (string String in Strings)
				{
					Writer.WriteValue(String);
				}
			}
			Writer.WriteArrayEnd();
		}

		/// <summary>
		/// Returns a copy of Nodes sorted by dependency.  Independent or circularly-dependent nodes should
		/// remain in their same relative order within the original Nodes sequence.
		/// </summary>
		/// <param name="NodeList">The list of nodes to sort.</param>
		public static List<UEBuildModule> StableTopologicalSort(List<UEBuildModule> NodeList)
		{
			int NodeCount = NodeList.Count;

			// For each Node in NodeList, populated with the full circular dependency list from
			// Node.GetAllDependencyModules()
			List<Task<HashSet<UEBuildModule>>> NodeDependencies = new List<Task<HashSet<UEBuildModule>>>(NodeCount);
			// Used to populate an element of NodeDependencies
			HashSet<UEBuildModule> FetchDependencies(int NodeIndex)
			{
				HashSet<UEBuildModule> Dependencies = new HashSet<UEBuildModule>();
				NodeList[NodeIndex].GetAllDependencyModules(new List<UEBuildModule>(), Dependencies, true, true, false);
				return Dependencies;
			}

			// For each Node in NodeList, populated with the nodes with a lower index in NodeList that Node depends on
			List<Task<HashSet<UEBuildModule>>> PrecedingDependents = new List<Task<HashSet<UEBuildModule>>>(NodeCount);
			HashSet<UEBuildModule> ComputePrecedingDependents(int NodeIndex)
			{
				HashSet<UEBuildModule> Results = new HashSet<UEBuildModule>();

				UEBuildModule Node = NodeList[NodeIndex];
				HashSet<UEBuildModule> Dependencies = NodeDependencies[NodeIndex].Result;

				for (int I = 0; I < NodeIndex; ++I)
				{
					if (NodeDependencies[I].Result.Contains(Node) && !Dependencies.Contains(NodeList[I]))
					{
						Results.Add(NodeList[I]);
					}
				}

				return Results;
			}

			for (int I = 0; I < NodeCount; ++I)
			{
				int LocalI = I;
				NodeDependencies.Add(Task.Run(() => FetchDependencies(LocalI)));
				PrecedingDependents.Add(Task.Run(() => ComputePrecedingDependents(LocalI)));
			}

			List<UEBuildModule> Out = new List<UEBuildModule>(NodeCount);
			// Write the ordered output			
			for (int Index1 = 0; Index1 != NodeCount; ++Index1)
			{
				UEBuildModule Node1 = NodeList[Index1];
				HashSet<UEBuildModule> NodesThatDependOnNode1 = PrecedingDependents[Index1].Result;
				Out.Add(Node1);

				if (NodesThatDependOnNode1.Count == 0)
				{
					continue;
				}

				for (int Index2 = 0; Index2 != Index1; ++Index2)
				{
					UEBuildModule Node2 = Out[Index2];

					if (NodesThatDependOnNode1.Contains(Node2))
					{
						// Rotate element at Index1 into position at Index2
						for (int Index3 = Index1; Index3 != Index2;)
						{
							--Index3;
							Out[Index3 + 1] = Out[Index3];
						}

						Out[Index2] = Node1;

						// Break out of this loop, because this iteration must have covered all existing cases
						// involving the node formerly at position Index1
						break;
					}
				}
			}

			return Out;
		}
	};
}
