// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	/// <summary>
	/// A module that is compiled from C++ code.
	/// </summary>
	class UEBuildModuleCPP : UEBuildModule
	{
		/// <summary>
		/// Stores a list of all source files, of different types
		/// </summary>
		public class InputFileCollection
		{
			public readonly List<FileItem> HeaderFiles = new List<FileItem>();
			public readonly List<FileItem> ISPCHeaderFiles = new List<FileItem>();

			public readonly List<FileItem> IXXFiles = new List<FileItem>();
			public readonly List<FileItem> CPPFiles = new List<FileItem>();
			public readonly List<FileItem> CFiles = new List<FileItem>();
			public readonly List<FileItem> CCFiles = new List<FileItem>();
			public readonly List<FileItem> MFiles = new List<FileItem>();
			public readonly List<FileItem> MMFiles = new List<FileItem>();
			public readonly List<FileItem> SwiftFiles = new List<FileItem>();
			public readonly List<FileItem> RCFiles = new List<FileItem>();
			public readonly List<FileItem> ISPCFiles = new List<FileItem>();
		}

		/// <summary>
		/// Instance where a CPP file doesn't include the expected header as the first include.
		/// </summary>
		public struct InvalidIncludeDirective
		{
			public readonly FileReference CppFile;
			public readonly FileReference HeaderFile;

			public InvalidIncludeDirective(FileReference cppFile, FileReference headerFile)
			{
				CppFile = cppFile;
				HeaderFile = headerFile;
			}
		}

		/// <summary>
		/// If UHT found any associated UObjects in this module's source files
		/// </summary>
		public bool bHasUObjects;

		/// <summary>
		/// The directory for this module's generated code
		/// </summary>
		public readonly DirectoryReference? GeneratedCodeDirectory;

		/// <summary>
		/// The directory for this module's generated UHT code
		/// </summary>
		public DirectoryReference? GeneratedCodeDirectoryUHT => GeneratedCodeDirectory != null ? DirectoryReference.Combine(GeneratedCodeDirectory!, "UHT") : null;

		/// <summary>
		/// The directory for this module's generated VNI code
		/// </summary>
		public DirectoryReference? GeneratedCodeDirectoryVNI => GeneratedCodeDirectory != null ? DirectoryReference.Combine(GeneratedCodeDirectory!, "VNI") : null;

		/// <summary>
		/// Global override to force all include paths to be always added
		/// </summary>
		public static bool bForceAddGeneratedCodeIncludePath;

		/// <summary>
		/// Paths containing *.gen.cpp files for this module.  If this is null then this module doesn't have any generated code.
		/// </summary>
		public List<string>? GeneratedCppDirectories;

		/// <summary>
		/// List of invalid include directives. These are buffered up and output before we start compiling.
		/// </summary>
		public List<InvalidIncludeDirective>? InvalidIncludeDirectives;

		/// <summary>
		/// Set of source directories referenced during a build
		/// </summary>
		HashSet<DirectoryReference>? SourceDirectories;

		/// <summary>
		/// Verse source code directory associated with this module
		/// </summary>
		DirectoryReference? AssociatedVerseDirectory;

		/// <summary>
		/// The Verse source code directory associated with this module if any
		/// </summary>
		public override DirectoryReference? VerseDirectory => AssociatedVerseDirectory;

		protected override void GetReferencedDirectories(HashSet<DirectoryReference> Directories)
		{
			base.GetReferencedDirectories(Directories);

			if (!Rules.bUsePrecompiled)
			{
				if (SourceDirectories == null)
				{
					throw new BuildException("GetReferencedDirectories() should not be called before building.");
				}
				Directories.UnionWith(SourceDirectories);
			}
		}

		/// <summary>
		/// List of allowed circular dependencies. Please do NOT add new modules here; refactor to allow the modules to be decoupled instead.
		/// </summary>
		static readonly KeyValuePair<string, string>[] CircularDependenciesAllowList =
		{
			new KeyValuePair<string, string>("AIModule", "AITestSuite"),
			new KeyValuePair<string, string>("AnimGraph", "GraphEditor"),
			new KeyValuePair<string, string>("AnimGraph", "UnrealEd"),
			new KeyValuePair<string, string>("AudioEditor", "DetailCustomizations"),
			new KeyValuePair<string, string>("AudioMixer", "NonRealtimeAudioRenderer"),
			new KeyValuePair<string, string>("AudioMixer", "SoundFieldRendering"),
			new KeyValuePair<string, string>("AssetTools", "UnrealEd"),
			new KeyValuePair<string, string>("BlueprintGraph", "CinematicCamera"),
			new KeyValuePair<string, string>("BlueprintGraph", "GraphEditor"),
			new KeyValuePair<string, string>("BlueprintGraph", "Kismet"),
			new KeyValuePair<string, string>("BlueprintGraph", "KismetCompiler"),
			new KeyValuePair<string, string>("BlueprintGraph", "UnrealEd"),
			new KeyValuePair<string, string>("ConfigEditor", "PropertyEditor"),
			new KeyValuePair<string, string>("Documentation", "SourceControl"),
			new KeyValuePair<string, string>("Engine", "AudioMixer"),
			new KeyValuePair<string, string>("Engine", "CinematicCamera"),
			new KeyValuePair<string, string>("Engine", "CollisionAnalyzer"),
			new KeyValuePair<string, string>("Engine", "GameplayTags"),
			new KeyValuePair<string, string>("Engine", "Kismet"),
			new KeyValuePair<string, string>("Engine", "Landscape"),
			new KeyValuePair<string, string>("Engine", "LogVisualizer"),
			new KeyValuePair<string, string>("Engine", "MaterialShaderQualitySettings"),
			new KeyValuePair<string, string>("Engine", "UMG"),
			new KeyValuePair<string, string>("Engine", "UnrealEd"),
			new KeyValuePair<string, string>("FoliageEdit", "ViewportInteraction"),
			new KeyValuePair<string, string>("FoliageEdit", "VREditor"),
			new KeyValuePair<string, string>("FunctionalTesting", "UnrealEd"),
			new KeyValuePair<string, string>("GameplayAbilitiesEditor", "BlueprintGraph"),
			new KeyValuePair<string, string>("GameplayDebugger", "AIModule"),
			new KeyValuePair<string, string>("GameplayDebugger", "GameplayTasks"),
			new KeyValuePair<string, string>("GameplayTasks", "UnrealEd"),
			new KeyValuePair<string, string>("GraphEditor", "Kismet"),
			new KeyValuePair<string, string>("HierarchicalLODOutliner", "UnrealEd"),
			new KeyValuePair<string, string>("Kismet", "BlueprintGraph"),
			new KeyValuePair<string, string>("Kismet", "Merge"),
			new KeyValuePair<string, string>("Kismet", "UMGEditor"),
			new KeyValuePair<string, string>("KismetWidgets", "BlueprintGraph"),
			new KeyValuePair<string, string>("Landscape", "MaterialUtilities"),
			new KeyValuePair<string, string>("Landscape", "UnrealEd"),
			new KeyValuePair<string, string>("LandscapeEditor", "ViewportInteraction"),
			new KeyValuePair<string, string>("LandscapeEditor", "VREditor"),
			new KeyValuePair<string, string>("LocalizationDashboard", "LocalizationService"),
			new KeyValuePair<string, string>("LocalizationDashboard", "MainFrame"),
			new KeyValuePair<string, string>("LocalizationDashboard", "TranslationEditor"),
			new KeyValuePair<string, string>("MaterialUtilities", "Landscape"),
			new KeyValuePair<string, string>("MovieSceneTools", "Sequencer"),
			new KeyValuePair<string, string>("NavigationSystem", "UnrealEd"),
			new KeyValuePair<string, string>("PacketHandler", "ReliabilityHandlerComponent"),
			new KeyValuePair<string, string>("PIEPreviewDeviceProfileSelector", "UnrealEd"),
			new KeyValuePair<string, string>("PIEPreviewDeviceProfileSelector", "MaterialShaderQualitySettings"),
			new KeyValuePair<string, string>("PixelInspectorModule", "UnrealEd"),
			new KeyValuePair<string, string>("Sequencer", "MovieSceneTools"),
			new KeyValuePair<string, string>("Sequencer", "ViewportInteraction"),
			new KeyValuePair<string, string>("SourceControl", "UnrealEd"),
			new KeyValuePair<string, string>("UnrealEd", "AudioEditor"),
			new KeyValuePair<string, string>("UnrealEd", "ClothingSystemEditor"),
			new KeyValuePair<string, string>("UnrealEd", "Documentation"),
			new KeyValuePair<string, string>("UnrealEd", "EditorInteractiveToolsFramework"),
			new KeyValuePair<string, string>("UnrealEd", "GraphEditor"),
			new KeyValuePair<string, string>("UnrealEd", "Kismet"),
			new KeyValuePair<string, string>("UnrealEd", "MeshPaint"),
			new KeyValuePair<string, string>("UnrealEd", "PropertyEditor"),
			new KeyValuePair<string, string>("UnrealEd", "ToolMenusEditor"),
			new KeyValuePair<string, string>("UnrealEd", "ViewportInteraction"),
			new KeyValuePair<string, string>("UnrealEd", "VREditor"),
			new KeyValuePair<string, string>("UnrealEd", "MaterialShaderQualitySettings"),
			new KeyValuePair<string, string>("WebBrowser", "WebBrowserTexture"),
		};

		public UEBuildModuleCPP(ModuleRules Rules, DirectoryReference IntermediateDirectory, DirectoryReference IntermediateDirectoryNoArch, DirectoryReference? GeneratedCodeDirectory, ILogger Logger)
			: base(Rules, IntermediateDirectory, IntermediateDirectoryNoArch, Logger)
		{
			this.GeneratedCodeDirectory = GeneratedCodeDirectory;

			// Check for a Verse directory next to the rules file
			DirectoryReference MaybeVerseDirectory = DirectoryReference.Combine(Rules.File.Directory, "Verse");
			if (IsValidVerseDirectory(MaybeVerseDirectory))
			{
				AssociatedVerseDirectory = MaybeVerseDirectory;
				bDependsOnVerse = true;
			}

			if (Rules.bValidateCircularDependencies || Rules.bTreatAsEngineModule)
			{
				foreach (string CircularlyReferencedModuleName in Rules.CircularlyReferencedDependentModules)
				{
					if (CircularlyReferencedModuleName != "BlueprintContext" &&
						!CircularDependenciesAllowList.Any(x =>
							x.Key == Name && x.Value == CircularlyReferencedModuleName))
					{
						Logger.LogWarning(
							"Found reference between '{Source}' and '{Target}'. Support for circular references is being phased out; please do not introduce new ones.",
							Name, CircularlyReferencedModuleName);
					}
				}
			}

			if (Rules.bAddDefaultIncludePaths)
			{
				AddDefaultIncludePaths();
			}
		}

		/// <summary>
		/// Determines if a file is part of the given module
		/// </summary>
		/// <param name="Location">Path to the file</param>
		/// <returns>True if the file is part of this module</returns>
		public override bool ContainsFile(FileReference Location)
		{
			if (base.ContainsFile(Location))
			{
				return true;
			}
			if (GeneratedCodeDirectory != null && Location.IsUnderDirectory(GeneratedCodeDirectory))
			{
				return true;
			}
			return false;
		}

		/// <summary>
		/// Add the default include paths for this module to its settings
		/// </summary>
		private void AddDefaultIncludePaths()
		{
			// Add the module's parent directory to the public include paths, so other modules may include headers from it explicitly.
			foreach (DirectoryReference ModuleDir in ModuleDirectories)
			{
				// Add the parent directory to the legacy parent include paths.
				LegacyParentIncludePaths.Add(ModuleDir.ParentDirectory!);

				// Add the base directory to the legacy include paths.
				LegacyPublicIncludePaths.Add(ModuleDir);

				// Add the 'classes' directory, if it exists
				DirectoryReference ClassesDirectory = DirectoryReference.Combine(ModuleDir, "Classes");
				if (DirectoryLookupCache.DirectoryExists(ClassesDirectory))
				{
					PublicIncludePaths.Add(ClassesDirectory);
				}

				// Add all the public directories
				DirectoryReference PublicDirectory = DirectoryReference.Combine(ModuleDir, "Public");
				if (DirectoryLookupCache.DirectoryExists(PublicDirectory))
				{
					PublicIncludePaths.Add(PublicDirectory);

					IReadOnlySet<string> ExcludeNames = UEBuildPlatform.GetBuildPlatform(Rules.Target.Platform).GetExcludedFolderNames();
					EnumerateLegacyIncludePaths(DirectoryItem.GetItemByDirectoryReference(PublicDirectory), ExcludeNames, LegacyPublicIncludePaths);
				}

				// Add the 'internal' directory, if it exists
				DirectoryReference InternalDirectory = DirectoryReference.Combine(ModuleDir, "Internal");
				if (DirectoryLookupCache.DirectoryExists(InternalDirectory))
				{
					InternalIncludePaths.Add(InternalDirectory);
				}

				// Add the base private directory for this module
				DirectoryReference PrivateDirectory = DirectoryReference.Combine(ModuleDir, "Private");
				if (DirectoryLookupCache.DirectoryExists(PrivateDirectory))
				{
					PrivateIncludePaths.Add(PrivateDirectory);
				}
			}
		}

		/// <summary>
		/// Enumerates legacy include paths under a given base directory
		/// </summary>
		/// <param name="BaseDirectory">The directory to start from. This directory is not added to the output list.</param>
		/// <param name="ExcludeNames">Set of folder names to exclude from the search.</param>
		/// <param name="LegacyPublicIncludePaths">List populated with the discovered directories</param>
		static void EnumerateLegacyIncludePaths(DirectoryItem BaseDirectory, IReadOnlySet<string> ExcludeNames, HashSet<DirectoryReference> LegacyPublicIncludePaths)
		{
			foreach (DirectoryItem SubDirectory in BaseDirectory.EnumerateDirectories())
			{
				if (!ExcludeNames.Contains(SubDirectory.Name))
				{
					LegacyPublicIncludePaths.Add(SubDirectory.Location);
					EnumerateLegacyIncludePaths(SubDirectory, ExcludeNames, LegacyPublicIncludePaths);
				}
			}
		}

		/// <summary>
		/// Path to the precompiled manifest location
		/// </summary>
		public virtual FileReference PrecompiledManifestLocation => FileReference.Combine(IntermediateDirectoryNoArch, String.Format("{0}.precompiled", Name));

		/// <summary>
		/// Sets up the environment for compiling any module that includes the public interface of this module.
		/// </summary>
		public override void AddModuleToCompileEnvironment(
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
			if (GeneratedCodeDirectory != null)
			{
				// This directory may not exist for this module (or ever exist, if it doesn't contain any generated headers), but we want the project files
				// to search it so we can pick up generated code definitions after UHT is run for the first time.
				bool bForceAddIncludePath = bForceAddGeneratedCodeIncludePath || ProjectFileGenerator.bGenerateProjectFiles;

				if (bHasUObjects || bForceAddIncludePath)
				{
					IncludePaths.Add(GeneratedCodeDirectoryUHT!);
				}

				if (bHasVerse || bForceAddIncludePath)
				{
					IncludePaths.Add(GeneratedCodeDirectoryVNI!);
				}

				foreach ((string Subdirectory, Action<ILogger, DirectoryReference> _) in Rules.GenerateHeaderFuncs)
				{
					IncludePaths.Add(DirectoryReference.Combine(GeneratedCodeDirectory, Subdirectory));
				}
			}

			ModuleInterfacePaths.Add(UEToolChain.GetModuleInterfaceDir(IntermediateDirectory));

			base.AddModuleToCompileEnvironment(SourceModule, SourceBinary, IncludePaths, SystemIncludePaths, ModuleInterfacePaths, Definitions, AdditionalFrameworks, AdditionalPrerequisites, bLegacyPublicIncludePaths, bLegacyParentIncludePaths);
		}

		// UEBuildModule interface.
		public override List<FileItem> Compile(ReadOnlyTargetRules Target, UEToolChain ToolChain, CppCompileEnvironment BinaryCompileEnvironment, ISourceFileWorkingSet WorkingSet, IActionGraphBuilder Graph, ILogger Logger)
		{
			//UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(BinaryCompileEnvironment.Platform);

			List<FileItem> LinkInputFiles = base.Compile(Target, ToolChain, BinaryCompileEnvironment, WorkingSet, Graph, Logger);

			CppCompileEnvironment ModuleCompileEnvironment = CreateModuleCompileEnvironment(Target, BinaryCompileEnvironment, Logger);

			// If the module is precompiled, read the object files from the manifest
			if (Rules.bUsePrecompiled && Target.LinkType == TargetLinkType.Monolithic)
			{
				if (!FileReference.Exists(PrecompiledManifestLocation))
				{
					throw new BuildException("Missing precompiled manifest for '{0}', '{1}'. This module was most likely not flagged for being included in a precompiled build - set 'PrecompileForTargets = PrecompileTargetsType.Any;' in {0}.build.cs to override." +
						" If part of a plugin, also check if its 'Type' is correct.", Name, PrecompiledManifestLocation);
				}

				PrecompiledManifest Manifest = PrecompiledManifest.Read(PrecompiledManifestLocation);
				foreach (FileReference OutputFile in Manifest.OutputFiles)
				{
					FileItem ObjectFile = FileItem.GetItemByFileReference(OutputFile);
					if (!ObjectFile.Exists)
					{
						throw new BuildException("Missing object file {0} listed in {1}", OutputFile, PrecompiledManifestLocation);
					}
					LinkInputFiles.Add(ObjectFile);
				}
				return LinkInputFiles;
			}

			// Add all the module source directories to the makefile
			foreach (DirectoryReference ModuleDirectory in ModuleDirectories)
			{
				DirectoryItem ModuleDirectoryItem = DirectoryItem.GetItemByDirectoryReference(ModuleDirectory);
				Graph.AddSourceDir(ModuleDirectoryItem);
			}

			// Find all the input files
			Dictionary<DirectoryItem, FileItem[]> DirectoryToSourceFiles = new Dictionary<DirectoryItem, FileItem[]>();
			InputFileCollection InputFiles = FindInputFiles(Target.Platform, DirectoryToSourceFiles, Logger);

			foreach (KeyValuePair<DirectoryItem, FileItem[]> Pair in DirectoryToSourceFiles)
			{
				Graph.AddSourceFiles(Pair.Key, Pair.Value);
			}

			Graph.AddHeaderFiles(InputFiles.HeaderFiles.ToArray());
			Graph.AddHeaderFiles(InputFiles.ISPCHeaderFiles.ToArray());

			// We are building with IWYU and thismodule does not support it, early out
			if (Target.bIWYU && Rules.IWYUSupport == IWYUSupport.None)
			{
				return new List<FileItem>();
			}

			// Process all of the header file dependencies for this module
			CheckFirstIncludeMatchesEachCppFile(Target, ModuleCompileEnvironment, InputFiles.HeaderFiles, InputFiles.CPPFiles);

			// Should we force a precompiled header to be generated for this module?  Usually, we only bother with a
			// precompiled header if there are at least several source files in the module (after combining them for unity
			// builds.)  But for game modules, it can be convenient to always have a precompiled header to single-file
			// changes to code is really quick to compile.
			int MinFilesUsingPrecompiledHeader = Target.MinFilesUsingPrecompiledHeader;
			if (Rules.MinFilesUsingPrecompiledHeaderOverride != 0)
			{
				MinFilesUsingPrecompiledHeader = Rules.MinFilesUsingPrecompiledHeaderOverride;
			}
			else if (!Rules.bTreatAsEngineModule && Target.bForcePrecompiledHeaderForGameModules)
			{
				// This is a game module with only a small number of source files, so go ahead and force a precompiled header
				// to be generated to make incremental changes to source files as fast as possible for small projects.
				MinFilesUsingPrecompiledHeader = 1;
			}

			// Set up the environment with which to compile the CPP files
			CppCompileEnvironment CompileEnvironment = ModuleCompileEnvironment;

			// Generate ISPC headers first so C++ can consume them
			if (InputFiles.ISPCFiles.Count > 0)
			{
				CreateHeadersForISPC(ToolChain, ModuleCompileEnvironment, InputFiles.ISPCFiles, IntermediateDirectory, Graph);
			}

			// Compile any module interfaces
			if (InputFiles.IXXFiles.Count > 0 && Target.bEnableCppModules)
			{
				CppCompileEnvironment IxxCompileEnvironment = CompileEnvironment;

				// Write all the definitions to a separate file for the ixx compile
				CreateHeaderForDefinitions(IxxCompileEnvironment, IntermediateDirectory, "ixx", Graph);

				CPPOutput ModuleOutput = ToolChain.CompileAllCPPFiles(IxxCompileEnvironment, InputFiles.IXXFiles, IntermediateDirectory, Name, Graph);

				LinkInputFiles.AddRange(ModuleOutput.ObjectFiles);
				CompileEnvironment.AdditionalPrerequisites.AddRange(ModuleOutput.CompiledModuleInterfaces);
			}

			// Configure the precompiled headers for this module
			CompileEnvironment = SetupPrecompiledHeaders(Target, ToolChain, CompileEnvironment, LinkInputFiles, Graph, Logger);
			if (CompileEnvironment.PerArchPrecompiledHeaderFiles != null)
			{
				foreach (UnrealArch Arch in CompileEnvironment.PerArchPrecompiledHeaderFiles.Keys)
				{
					Logger.LogDebug("Module '{ModuleName}' uses PCH '{PCHIncludeFilename}' for Architecture '{Arch}'", Name, Arch, CompileEnvironment.PerArchPrecompiledHeaderFiles[Arch]);
				}
			}
			else if (CompileEnvironment.PrecompiledHeaderFile != null)
			{
				Logger.LogDebug("Module '{ModuleName}' uses PCH '{PCHIncludeFilename}'", Name, CompileEnvironment.PrecompiledHeaderFile);
			}

			// Write all the definitions to a separate file
			CreateHeaderForDefinitions(CompileEnvironment, IntermediateDirectory, null, Graph);

			// Create shared rsp for the normal cpp files
			FileReference SharedResponseFile = FileReference.Combine(IntermediateDirectory, $"{Rules.ShortName ?? Name}.Shared{UEToolChain.ResponseExt}");
			CompileEnvironment = ToolChain.CreateSharedResponseFile(CompileEnvironment, SharedResponseFile, Graph);

			// Mapping of source file to unity file. We output this to intermediate directories for other tools (eg. live coding) to use.
			Dictionary<FileItem, FileItem> SourceFileToUnityFile = new Dictionary<FileItem, FileItem>();

			List<FileItem> CPPFiles = new List<FileItem>(InputFiles.CPPFiles);
			List<FileItem> GeneratedFileItems = new List<FileItem>();
			CppCompileEnvironment GeneratedCPPCompileEnvironment = CompileEnvironment;
			bool bMergeUnityFiles = !(Target.DisableMergingModuleAndGeneratedFilesInUnityFiles?.Contains(Name) ?? false) && Rules.bMergeUnityFiles;

			// Compile all the generated CPP files
			if (GeneratedCppDirectories != null && !CompileEnvironment.bHackHeaderGenerator)
			{
				Dictionary<string, FileItem> GeneratedFiles = new Dictionary<string, FileItem>();
				foreach (string GeneratedDir in GeneratedCppDirectories)
				{
					if (!Directory.Exists(GeneratedDir))
					{
						continue;
					}

					string Prefix = Path.GetFileName(GeneratedDir) + '/'; // "UHT/" or "VNI/"

					DirectoryItem DirItem = DirectoryItem.GetItemByPath(GeneratedDir);
					foreach (FileItem File in DirItem.EnumerateFiles())
					{
						string FileName = File.Name;
						if (FileName.EndsWith(".gen.cpp"))
						{
							string Key = Prefix + FileName.Substring(0, FileName.Length - ".gen.cpp".Length);
							GeneratedFiles.Add(Key, File);
						}
					}
				}

				if (GeneratedFiles.Count > 0)
				{
					// Remove any generated files from the compile list if they are inlined
					foreach (FileItem CPPFileItem in CPPFiles)
					{
						IList<string> ListOfInlinedGenCpps = ModuleCompileEnvironment.MetadataCache.GetListOfInlinedGeneratedCppFiles(CPPFileItem);
						foreach (string ListOfInlinedGenCppsItem in ListOfInlinedGenCpps)
						{
							string Prefix = "UHT/";
							string Key = Prefix + ListOfInlinedGenCppsItem;
							if (GeneratedFiles.Remove(Key, out FileItem? FoundGenCppFile))
							{
								if (!CompileEnvironment.FileInlineGenCPPMap.ContainsKey(CPPFileItem))
								{
									CompileEnvironment.FileInlineGenCPPMap[CPPFileItem] = new List<FileItem>();
								}
								CompileEnvironment.FileInlineGenCPPMap[CPPFileItem].Add(FoundGenCppFile);
							}
							else
							{
								Logger.LogError("'{CPPFileItem}' is looking for a generated cpp with named '{HeaderFile}.gen.cpp'", CPPFileItem.AbsolutePath, ListOfInlinedGenCppsItem);
							}
						}
					}
					ModuleCompileEnvironment.FileInlineGenCPPMap = CompileEnvironment.FileInlineGenCPPMap;

					if (Rules.bEnableNonInlinedGenCppWarnings)
					{
						Dictionary<string, FileItem> CPPFilesLookup = new Dictionary<string, FileItem>();
						foreach (FileItem CPPFile in CPPFiles)
						{
							CPPFilesLookup.Add(Utils.GetFilenameWithoutAnyExtensions(CPPFile.Name), CPPFile);
						}
						foreach (string Name in GeneratedFiles.Keys)
						{
							if (!Name.StartsWith("UHT/"))
							{
								continue;
							}
							string NameWithoutPrefix = Name.Substring(4);
							if (CPPFilesLookup.TryGetValue(NameWithoutPrefix, out FileItem? Item))
							{
								Logger.LogWarning("'{0}' .gen.cpp not inlined. Add '#include UE_INLINE_GENERATED_CPP_BY_NAME({1})'", Item.Name, NameWithoutPrefix);
							}
						}
					}

					// Create a compile environment for the generated files. We can disable creating debug info here to improve link times.
					if (GeneratedCPPCompileEnvironment.bCreateDebugInfo && Target.bDisableDebugInfoForGeneratedCode)
					{
						GeneratedCPPCompileEnvironment = new CppCompileEnvironment(GeneratedCPPCompileEnvironment);
						GeneratedCPPCompileEnvironment.bCreateDebugInfo = false;
						bMergeUnityFiles = false;
					}

					if (Target.StaticAnalyzer != StaticAnalyzer.None && !Target.bStaticAnalyzerIncludeGenerated)
					{
						GeneratedCPPCompileEnvironment = new CppCompileEnvironment(GeneratedCPPCompileEnvironment);
						GeneratedCPPCompileEnvironment.bDisableStaticAnalysis = true;
						bMergeUnityFiles = false;
					}

					// Always force include the PCH, even if PCHs are disabled, for generated code. Legacy code can rely on PCHs being included to compile correctly, and this used to be done by UHT manually including it.
					if (Target.bForceIncludePCHHeadersForGenCppFilesWhenPCHIsDisabled && GeneratedCPPCompileEnvironment.bHasPrecompiledHeader == false && Rules.PrivatePCHHeaderFile != null && Rules.PCHUsage != ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs)
					{
						FileItem PrivatePchFileItem = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile));
						if (!PrivatePchFileItem.Exists)
						{
							throw new BuildException("Unable to find private PCH file '{0}', referenced by '{1}'", PrivatePchFileItem.Location, RulesFile);
						}

						GeneratedCPPCompileEnvironment = new CppCompileEnvironment(GeneratedCPPCompileEnvironment);
						GeneratedCPPCompileEnvironment.ForceIncludeFiles.Add(PrivatePchFileItem);
						bMergeUnityFiles = false;
					}

					// Compile all the generated files
					foreach (FileItem GeneratedCppFileItem in GeneratedFiles.Values)
					{
						GeneratedFileItems.Add(GeneratedCppFileItem);
					}
				}
			}

			// Engine modules will always use unity build mode unless MinSourceFilesForUnityBuildOverride is specified in
			// the module rules file.  By default, game modules only use unity of they have enough source files for that
			// to be worthwhile.  If you have a lot of small game modules, consider specifying MinSourceFilesForUnityBuildOverride=0
			// in the modules that you don't typically iterate on source files in very frequently.
			int MinSourceFilesForUnityBuild = 0;
			if (Rules.MinSourceFilesForUnityBuildOverride != 0)
			{
				MinSourceFilesForUnityBuild = Rules.MinSourceFilesForUnityBuildOverride;
			}
			else if (Target.ProjectFile != null && RulesFile.IsUnderDirectory(DirectoryReference.Combine(Target.ProjectFile.Directory, "Source")))
			{
				// Game modules with only a small number of source files are usually better off having faster iteration times
				// on single source file changes, so we forcibly disable unity build for those modules
				MinSourceFilesForUnityBuild = Target.MinGameModuleSourceFilesForUnityBuild;
			}

			// Set up the NumIncludedBytesPerUnityCPP for this particular module
			int NumIncludedBytesPerUnityCPP = Rules.GetNumIncludedBytesPerUnityCPP();

			// Should we use unity build mode for this module?
			bool bModuleUsesUnityBuild = false;
			if (Target.bUseUnityBuild || Target.bForceUnityBuild)
			{
				int FileCount = CPPFiles.Count;

				// if we are merging the genearted cpp files then that needs to be part of the count
				if (bMergeUnityFiles)
				{
					FileCount += GeneratedFileItems.Count;
				}

				if (Target.bForceUnityBuild)
				{
					Logger.LogTrace("Module '{ModuleName}' using unity build mode (bForceUnityBuild enabled for this module)", Name);
					bModuleUsesUnityBuild = true;
				}
				else if (!Rules.bUseUnity)
				{
					Logger.LogTrace("Module '{ModuleName}' not using unity build mode (bUseUnity disabled for this module)", Name);
					bModuleUsesUnityBuild = false;
				}
				else if (FileCount < MinSourceFilesForUnityBuild)
				{
					Logger.LogTrace("Module '{ModuleName}' not using unity build mode (module with fewer than {NumFiles} source files)", Name, MinSourceFilesForUnityBuild);
					bModuleUsesUnityBuild = false;
				}
				else
				{
					Logger.LogTrace("Module '{ModuleName}' using unity build mode", Name);
					bModuleUsesUnityBuild = true;
				}
			}
			else
			{
				Logger.LogTrace("Module '{ModuleName}' not using unity build mode", Name);
			}

			CompileEnvironment.bUseUnity = bModuleUsesUnityBuild;
			GeneratedCPPCompileEnvironment.bUseUnity = bModuleUsesUnityBuild;

			// Create and register a special action that can be used to compile single files (even when unity is enabled) for generated files
			if (GeneratedFileItems.Any())
			{
				CppCompileEnvironment SingleGeneratedCPPCompileEnvironment = new CppCompileEnvironment(GeneratedCPPCompileEnvironment);
				SingleGeneratedCPPCompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.None;
				foreach (DirectoryReference Directory in GeneratedFileItems.Select(x => x.Location.Directory).Distinct())
				{
					ToolChain.CreateSpecificFileAction(SingleGeneratedCPPCompileEnvironment, Directory, IntermediateDirectory, Graph);
				}
			}

			// Create and register a special action that can be used to compile single files (even when unity is enabled) for normal files
			if (ModuleDirectories.Any())
			{
				CppCompileEnvironment SingleCompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
				SingleCompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.None;
				foreach (DirectoryReference Directory in ModuleDirectories)
				{
					ToolChain.CreateSpecificFileAction(SingleCompileEnvironment, Directory, IntermediateDirectory, Graph);
				}
			}

			// Compile Generated CPP Files
			if (bModuleUsesUnityBuild)
			{
				if (bMergeUnityFiles)
				{
					CPPFiles.AddRange(GeneratedFileItems);
				}
				else
				{
					Unity.GenerateUnityCPPs(Target, GeneratedFileItems, new List<FileItem>(), CompileEnvironment, WorkingSet, (Rules.ShortName ?? Name) + ".gen", IntermediateDirectory, Graph, SourceFileToUnityFile,
						out List<FileItem> NormalGeneratedFiles, out List<FileItem> AdaptiveGeneratedFiles, NumIncludedBytesPerUnityCPP);
					LinkInputFiles.AddRange(CompileFilesWithToolChain(Target, ToolChain, GeneratedCPPCompileEnvironment, ModuleCompileEnvironment, NormalGeneratedFiles, AdaptiveGeneratedFiles, Graph, Logger).ObjectFiles);
				}
			}
			else
			{
				LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(GeneratedCPPCompileEnvironment, GeneratedFileItems, IntermediateDirectory, Name, Graph).ObjectFiles);
			}

			// Compile CPP files
			if (bModuleUsesUnityBuild)
			{
				Unity.GenerateUnityCPPs(Target, CPPFiles, InputFiles.HeaderFiles, CompileEnvironment, WorkingSet, Rules.ShortName ?? Name, IntermediateDirectory, Graph, SourceFileToUnityFile,
					out List<FileItem> NormalFiles, out List<FileItem> AdaptiveFiles, NumIncludedBytesPerUnityCPP);
				LinkInputFiles.AddRange(CompileFilesWithToolChain(Target, ToolChain, CompileEnvironment, ModuleCompileEnvironment, NormalFiles, AdaptiveFiles, Graph, Logger).ObjectFiles);
			}
			else
			{
				Unity.GetAdaptiveFiles(Target, CPPFiles, InputFiles.HeaderFiles, CompileEnvironment, WorkingSet, Rules.ShortName ?? Name, IntermediateDirectory, Graph,
					out List<FileItem> NormalFiles, out List<FileItem> AdaptiveFiles);
				if (!NormalFiles.Where(file => !file.HasExtension(".gen.cpp")).Any())
				{
					NormalFiles = CPPFiles;
					AdaptiveFiles.RemoveAll(new HashSet<FileItem>(NormalFiles).Contains);
				}
				LinkInputFiles.AddRange(CompileFilesWithToolChain(Target, ToolChain, CompileEnvironment, ModuleCompileEnvironment, NormalFiles, AdaptiveFiles, Graph, Logger).ObjectFiles);
			}

			// Compile ISPC files directly
			if (InputFiles.ISPCFiles.Count > 0)
			{
				LinkInputFiles.AddRange(ToolChain.CompileISPCFiles(ModuleCompileEnvironment, InputFiles.ISPCFiles, IntermediateDirectory, Graph).ObjectFiles);
			}

			// Compile C files directly. Do not use a PCH here, because a C++ PCH is not compatible with C source files.
			if (InputFiles.CFiles.Count > 0)
			{
				CppCompileEnvironment CCompileEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
				CreateHeaderForDefinitions(CCompileEnvironment, IntermediateDirectory, "c", Graph);
				LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(CCompileEnvironment, InputFiles.CFiles, IntermediateDirectory, Name, Graph).ObjectFiles);
			}

			// Compile CC files directly.
			if (InputFiles.CCFiles.Count > 0)
			{
				CppCompileEnvironment CCCompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
				CreateHeaderForDefinitions(CCCompileEnvironment, IntermediateDirectory, "cc", Graph);
				LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(CCCompileEnvironment, InputFiles.CCFiles, IntermediateDirectory, Name, Graph).ObjectFiles);
			}

			// Compile M files directly. Do not use a PCH here, because a C++ PCH is not compatible with C source files.
			if (InputFiles.MFiles.Count > 0)
			{
				CppCompileEnvironment MCompileEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
				CreateHeaderForDefinitions(MCompileEnvironment, IntermediateDirectory, "m", Graph);
				LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(MCompileEnvironment, InputFiles.MFiles, IntermediateDirectory, Name, Graph).ObjectFiles);
			}

			// Compile MM files directly.
			if (InputFiles.MMFiles.Count > 0)
			{
				CppCompileEnvironment MMCompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
				CreateHeaderForDefinitions(MMCompileEnvironment, IntermediateDirectory, "mm", Graph);
				LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(MMCompileEnvironment, InputFiles.MMFiles, IntermediateDirectory, Name, Graph).ObjectFiles);
			}

			// Compile Swift files directly.
			if (InputFiles.SwiftFiles.Count > 0)
			{
				CppCompileEnvironment SwiftCompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
				CreateHeaderForDefinitions(SwiftCompileEnvironment, IntermediateDirectory, "swift", Graph);
				LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(SwiftCompileEnvironment, InputFiles.SwiftFiles, IntermediateDirectory, Name, Graph).ObjectFiles);
			}

			// Compile RC files. The resource compiler does not work with response files, and using the regular compile environment can easily result in the 
			// command line length exceeding the OS limit. Use the binary compile environment to keep the size down, and require that all include paths
			// must be specified relative to the resource file itself or Engine/Source.
			if (InputFiles.RCFiles.Count > 0)
			{
				CppCompileEnvironment ResourceCompileEnvironment = new CppCompileEnvironment(BinaryCompileEnvironment);
				if (Binary != null)
				{
					// @todo: This should be in some Windows code somewhere...
					ResourceCompileEnvironment.Definitions.Add("ORIGINAL_FILE_NAME=\"" + Binary.OutputFilePaths[0].GetFileName() + "\"");
				}
				LinkInputFiles.AddRange(ToolChain.CompileRCFiles(ResourceCompileEnvironment, InputFiles.RCFiles, IntermediateDirectory, Graph).ObjectFiles);
			}

			// Write the compiled manifest
			if (Rules.bPrecompile && Target.LinkType == TargetLinkType.Monolithic)
			{
				DirectoryReference.CreateDirectory(PrecompiledManifestLocation.Directory);

				PrecompiledManifest Manifest = new PrecompiledManifest();
				Manifest.OutputFiles.AddRange(LinkInputFiles.Select(x => x.Location));
				Manifest.WriteIfModified(PrecompiledManifestLocation);
			}

			// Write a mapping of unity object file to standalone object file for live coding
			if (Rules.Target.bWithLiveCoding)
			{
				StringWriter StringWriter = new();
				using (JsonWriter Writer = new JsonWriter(StringWriter))
				{
					Writer.WriteObjectStart();
					Writer.WriteObjectStart("RemapUnityFiles");
					foreach (IGrouping<FileItem, KeyValuePair<FileItem, FileItem>> UnityGroup in SourceFileToUnityFile.GroupBy(x => x.Value))
					{
						Writer.WriteArrayStart(UnityGroup.Key.Location.GetFileName() + ".obj");
						foreach (FileItem SourceFile in UnityGroup.Select(x => x.Key))
						{
							Writer.WriteValue(SourceFile.Location.GetFileName() + ".obj");
						}
						Writer.WriteArrayEnd();
					}
					Writer.WriteObjectEnd();
					Writer.WriteObjectEnd();
				}

				FileReference UnityManifestFile = FileReference.Combine(IntermediateDirectory, "LiveCodingInfo.json");
				Graph.CreateIntermediateTextFile(UnityManifestFile, StringWriter.ToString());
			}

			// IWYU needs to build all headers separate from cpp files to produce proper recommendations for includes
			if (Target.bIncludeHeaders)
			{
				// Collect the headers that should be built
				List<FileItem> HeaderFileItems = GetCompilableHeaders(InputFiles, CompileEnvironment);
				if (HeaderFileItems.Count > 0)
				{
					if (Target.bHeadersOnly)
					{
						LinkInputFiles.Clear();
					}

					// Add the compile actions
					LinkInputFiles.AddRange(ToolChain.CompileAllCPPFiles(CompileEnvironment, HeaderFileItems, IntermediateDirectory, Name, Graph).ObjectFiles);
				}
			}

			return LinkInputFiles;
		}

		List<FileItem> GetCompilableHeaders(InputFileCollection InputFiles, CppCompileEnvironment CompileEnvironment)
		{
			// Find FileItems for module's pch files
			FileItem? PrivatePchFileItem = null;
			if (Rules.PrivatePCHHeaderFile != null)
			{
				PrivatePchFileItem = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile));
			}

			FileItem? SharedPchFileItem = null;
			if (Rules.SharedPCHHeaderFile != null)
			{
				SharedPchFileItem = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile));
			}

			Dictionary<string, FileItem> NameToFileItem = new();

			HashSet<FileItem> CollidingFiles = new();

			// Collect the headers that should be built
			List<FileItem> HeaderFileItems = new();
			foreach (FileItem HeaderFileItem in InputFiles.HeaderFiles)
			{
				// We don't want to build pch files in iwyu, skip those.
				if (HeaderFileItem == PrivatePchFileItem || HeaderFileItem == SharedPchFileItem)
				{
					continue;
				}

				// If file is skipped by header units it means they can't be compiled by themselves and we must skip them too
				if (CompileEnvironment.MetadataCache.GetHeaderUnitType(HeaderFileItem) != HeaderUnitType.Valid)
				{
					continue;
				}

				if (!NameToFileItem.TryAdd(HeaderFileItem.Name, HeaderFileItem))
				{
					CollidingFiles.Add(NameToFileItem[HeaderFileItem.Name]);
					CollidingFiles.Add(HeaderFileItem);
				}

				HeaderFileItems.Add(HeaderFileItem);
			}

			if (CollidingFiles.Count != 0)
			{
				CompileEnvironment.CollidingNames = CollidingFiles;
			}

			return HeaderFileItems;
		}

		// Cache of files found while searching the includes
		static ConcurrentDictionary<DirectoryReference, HashSet<FileReference>?> KnownIncludeFilesDict = new();
		static HashSet<FileReference>? GetIncludeFiles(DirectoryReference Directory)
		{
			if (Directory == null)
			{
				return null;
			}
			return KnownIncludeFilesDict.GetOrAdd(Directory, (_) =>
			{
				if (DirectoryLookupCache.DirectoryExists(Directory))
				{
					return DirectoryLookupCache.EnumerateFiles(Directory).ToHashSet();
				}
				else
				{
					return null;
				}
			});
		}

		private void FindIncludedHeaders(HashSet<string> VisitedIncludes, HashSet<FileItem> FoundIncludeFileItems, HashSet<string> IncludesNotFound, CppCompileEnvironment CompileEnvironment, FileItem FileToSearch, ILogger Logger)
		{
			foreach (string HeaderInclude in CompileEnvironment.MetadataCache.GetHeaderIncludes(FileToSearch))
			{
				string TranformedHeaderInclude = HeaderInclude;
				if (TranformedHeaderInclude.Contains("COMPILED_PLATFORM_HEADER("))
				{
					string PlatformName = UEBuildPlatform.GetBuildPlatform(CompileEnvironment.Platform).GetPlatformName();
					TranformedHeaderInclude = TranformedHeaderInclude.Replace("COMPILED_PLATFORM_HEADER(", PlatformName + "/" + PlatformName).TrimEnd(')');
				}

				if (VisitedIncludes.Add(TranformedHeaderInclude))
				{
					var SearchForFileItem = (DirectoryReference dir) =>
					{
						FileReference FileRef = FileReference.Combine(dir, TranformedHeaderInclude);
						HashSet<FileReference>? Files = GetIncludeFiles(FileRef.Directory);
						if (Files != null && Files.Contains(FileRef))
						{
							return FileItem.GetItemByFileReference(FileRef);
						}
						return null;
					};

					// check to see if the file is a relative path first
					FileItem? IncludeFileItem = SearchForFileItem(FileToSearch.Directory.Location);

					// search through the include paths if the file isn't relative
					if (IncludeFileItem == null)
					{
						foreach (var IncludePath in CompileEnvironment.UserIncludePaths)
						{
							IncludeFileItem = SearchForFileItem(IncludePath);
							if (IncludeFileItem != null)
							{
								break;
							}
						}
					}

					if (IncludeFileItem != null)
					{
						FoundIncludeFileItems.Add(IncludeFileItem);
						// Logger.LogDebug("{0} SharedPCH - '{1}' included '{2}'.", Name, FileToSearch.Location, IncludeFileItem.Location);

						FindIncludedHeaders(VisitedIncludes, FoundIncludeFileItems, IncludesNotFound, CompileEnvironment, IncludeFileItem, Logger);
					}
					else
					{
						if (!TranformedHeaderInclude.Contains('.'))
						{
#if DEBUG
							Logger.LogDebug("{0} SharedPCH - Skipping '{1}' found in '{2}' because it doesn't appear to be a module header.", Name, TranformedHeaderInclude, FileToSearch.Location);
#endif
						}
						else if (TranformedHeaderInclude.EndsWith(".generated.h"))
						{
#if DEBUG
							Logger.LogDebug("{0} SharedPCH - Skipping '{1}' found in '{2}' because it appears to be a generated UHT header.", Name, TranformedHeaderInclude, FileToSearch.Location);
#endif
						}
						else
						{
#if DEBUG
							Logger.LogDebug("{0} SharedPCH - Could not find include directory for '{1}' found in '{2}'.", Name, TranformedHeaderInclude, FileToSearch.Location);
#endif
							IncludesNotFound.Add(TranformedHeaderInclude);
						}
					}
				}
			}
		}

		/// <summary>
		/// Gets the list of module dependencies that this module declares and then tries to optimize the list to what is actually used by the header.
		/// </summary>
		/// <param name="CompileEnvironment">Compile environment for this PCH</param>
		/// <param name="PCHHeaderFile">PCH header file</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Returns an optimized list of modules this module's shared PCH uses</returns>
		private HashSet<UEBuildModule> GetSharedPCHModuleDependencies(CppCompileEnvironment CompileEnvironment, FileItem PCHHeaderFile, ILogger Logger)
		{
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);

			HashSet<UEBuildModule> AllModuleDeps = ModuleToIncludePathsOnlyFlag.Keys.ToHashSet();
			HashSet<UEBuildModule> ModuleDeps = new HashSet<UEBuildModule>(AllModuleDeps) { this };

			HashSet<string> VisitedIncludes = new();
			HashSet<FileItem> FoundIncludeFileItems = new();
			HashSet<string> IncludesNotFound = new();

			// pre-populate all the known file locations for this compile environment
			foreach (DirectoryReference dir in CompileEnvironment.UserIncludePaths)
			{
				GetIncludeFiles(dir);
			}

			FindIncludedHeaders(VisitedIncludes, FoundIncludeFileItems, IncludesNotFound, CompileEnvironment, PCHHeaderFile, Logger);
			if (IncludesNotFound.Any())
			{
				// We are assuming that the includes not found aren't important. If this is found to be not true then we should return all the module dependencies
				//return AllModuleDeps;
			}

			bool FoundAllModules = true;
			HashSet<UEBuildModule> OptModules = new();
			foreach (var IncludeFile in FoundIncludeFileItems)
			{
				if (CompileEnvironment.MetadataCache.UsesAPIDefine(IncludeFile) || CompileEnvironment.MetadataCache.ContainsReflectionMarkup(IncludeFile))
				{
					UEBuildModule? FoundModule = ModuleDeps.FirstOrDefault(Module => Module.ContainsFile(IncludeFile.Location));
					if (FoundModule != null)
					{
#if DEBUG
						if (ModuleToIncludePathsOnlyFlag[FoundModule])
						{
							Logger.LogDebug("{0} SharedPCH - '{1}' is exporting types but '{2}' isn't declared as a public dependency.", Name, IncludeFile.Location, FoundModule);
						}
#endif

						OptModules.Add(FoundModule);
					}
					else
					{
#if DEBUG
						Logger.LogDebug("{0} SharedPCH - '{1}' is exporting types but the module this file belongs to isn't declared as a public dependency or include.", Name, IncludeFile.Location);
#endif
						FoundAllModules = false;
					}
				}
				else
				{
#if DEBUG
					Logger.LogDebug("{0} SharedPCH - '{1}' is not exporting types so we are ignoring the dependency", Name, IncludeFile.Location);
#endif
				}
			}

			if (!FoundAllModules)
			{
#if DEBUG
				Logger.LogDebug("{0} SharedPCH - Is missing public dependencies. To be safe, this shared PCH will fall back to use all the module dependencies. Note that this could affect compile times.", Name);
#endif
				return AllModuleDeps;
			}
			else
			{
				return OptModules;
			}
		}

		/// <summary>
		/// Create a shared PCH template for this module, which allows constructing shared PCH instances in the future
		/// </summary>
		/// <param name="Target">The target which owns this module</param>
		/// <param name="BaseCompileEnvironment">Base compile environment for this target</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Template for shared PCHs</returns>
		public PrecompiledHeaderTemplate CreateSharedPCHTemplate(UEBuildTarget Target, CppCompileEnvironment BaseCompileEnvironment, ILogger Logger)
		{
			CppCompileEnvironment CompileEnvironment = CreateSharedPCHCompileEnvironment(Target, BaseCompileEnvironment);
			FileItem HeaderFile = FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.SharedPCHHeaderFile!));

			DirectoryReference PrecompiledHeaderDir;
			if (Rules.bUsePrecompiled)
			{
				PrecompiledHeaderDir = DirectoryReference.Combine(Target.ProjectIntermediateDirectory, Name);
			}
			else
			{
				PrecompiledHeaderDir = IntermediateDirectory;
			}

			return new PrecompiledHeaderTemplate(this, CompileEnvironment, HeaderFile, PrecompiledHeaderDir, GetSharedPCHModuleDependencies(CompileEnvironment, HeaderFile, Logger));
		}

		static HashSet<string> GetImmutableDefinitions(List<string> Definitions)
		{
			HashSet<string> ImmutableDefinitions = new();
			foreach (string Definition in Definitions)
			{
				if (Definition.Contains("UE_IS_ENGINE_MODULE", StringComparison.Ordinal) ||
					Definition.Contains("UE_VALIDATE_FORMAT_STRINGS", StringComparison.Ordinal) ||
					Definition.Contains("UE_VALIDATE_INTERNAL_API", StringComparison.Ordinal) ||
					Definition.Contains("DEPRECATED_FORGAME", StringComparison.Ordinal) ||
					Definition.Contains("UE_DEPRECATED_FORGAME", StringComparison.Ordinal) ||
					Definition.Contains("UE_ENABLE_INCLUDE_ORDER_DEPRECATED_IN_", StringComparison.Ordinal))
				{
					continue;
				}
				ImmutableDefinitions.Add(Definition);
			}
			return ImmutableDefinitions;
		}

		/// <summary>
		/// Creates a precompiled header action to generate a new pch file 
		/// </summary>
		/// <param name="ToolChain">The toolchain to generate the PCH</param>
		/// <param name="HeaderFile"></param>
		/// <param name="ModuleCompileEnvironment"></param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		/// <returns>The created PCH instance.</returns>
		private PrecompiledHeaderInstance CreatePrivatePCH(UEToolChain? ToolChain, FileItem HeaderFile, CppCompileEnvironment ModuleCompileEnvironment, IActionGraphBuilder Graph)
		{
			// Create the wrapper file, which sets all the definitions needed to compile it
			FileItem DefinitionsFileItem = CreateHeaderForDefinitions(ModuleCompileEnvironment, IntermediateDirectory, null, Graph)!;
			FileReference WrapperLocation = FileReference.Combine(IntermediateDirectory, String.Format("PCH.{0}.h", Name));
			FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, DefinitionsFileItem!, HeaderFile, Graph);

			// Create a new C++ environment that is used to create the PCH.
			CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
			CompileEnvironment.Definitions.Clear();
			CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
			CompileEnvironment.PrecompiledHeaderIncludeFilename = WrapperFile.Location;
			CompileEnvironment.bOptimizeCode = ModuleCompileEnvironment.bOptimizeCode;
			CompileEnvironment.bCodeCoverage = ModuleCompileEnvironment.bCodeCoverage;

			// Create the action to compile the PCH file.
			CPPOutput Output;
			if (ToolChain == null)
			{
				Output = new CPPOutput();
			}
			else
			{
				Output = ToolChain.CompileAllCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, IntermediateDirectory, Name, Graph);
			}

			return new PrecompiledHeaderInstance(WrapperFile, DefinitionsFileItem, CompileEnvironment, Output, GetImmutableDefinitions(ModuleCompileEnvironment.Definitions));
		}

		/// <summary>
		/// Generates a precompiled header instance from the given template, or returns an existing one if it already exists
		/// </summary>
		/// <param name="ToolChain">The toolchain being used to build this module</param>
		/// <param name="Template">The PCH template</param>
		/// <param name="ModuleCompileEnvironment">Compile environment for the current module</param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		/// <returns>Instance of a PCH</returns>
		public PrecompiledHeaderInstance FindOrCreateSharedPCH(UEToolChain? ToolChain, PrecompiledHeaderTemplate Template, CppCompileEnvironment ModuleCompileEnvironment, IActionGraphBuilder Graph)
		{
			PrecompiledHeaderInstance? Instance = Template.Instances.Find(x => IsCompatibleForSharedPCH(x.CompileEnvironment, ModuleCompileEnvironment));
			if (Instance == null)
			{
				List<string> Definitions = Template.BaseCompileEnvironment.Definitions;

				// Modify definitions if we need to create a new shared pch for the include order
				if (ModuleCompileEnvironment.IncludeOrderVersion != Template.BaseCompileEnvironment.IncludeOrderVersion)
				{
					Definitions = new List<string>(Definitions);
					foreach (string OldDefine in EngineIncludeOrderHelper.GetDeprecationDefines(Template.BaseCompileEnvironment.IncludeOrderVersion))
					{
						Definitions.Remove(OldDefine);
					}
					Definitions.AddRange(EngineIncludeOrderHelper.GetDeprecationDefines(ModuleCompileEnvironment.IncludeOrderVersion));
				}

				// Modify definitions if we need to create a new shared pch for project modules
				if (ModuleCompileEnvironment.bTreatAsEngineModule != Template.BaseCompileEnvironment.bTreatAsEngineModule)
				{
					Definitions = new List<string>(Definitions);
					Definitions.RemoveAll(x => x.Contains("UE_IS_ENGINE_MODULE", StringComparison.Ordinal));
					Definitions.RemoveAll(x => x.Contains("DEPRECATED_FORGAME", StringComparison.Ordinal));
					Definitions.RemoveAll(x => x.Contains("UE_DEPRECATED_FORGAME", StringComparison.Ordinal));

					Definitions.Add($"UE_IS_ENGINE_MODULE={(Rules.bTreatAsEngineModule ? "1" : "0")}");
					Definitions.Add($"DEPRECATED_FORGAME={(Rules.bTreatAsEngineModule ? String.Empty : "DEPRECATED")}");
					Definitions.Add($"UE_DEPRECATED_FORGAME={(Rules.bTreatAsEngineModule ? String.Empty : "UE_DEPRECATED")}");
				}

				// Modify definitions if we need to create a new shared pch for validating format strings
				if (ModuleCompileEnvironment.bValidateFormatStrings != Template.BaseCompileEnvironment.bValidateFormatStrings)
				{
					Definitions = new List<string>(Definitions);
					Definitions.RemoveAll(x => x.Contains("UE_VALIDATE_FORMAT_STRINGS", StringComparison.Ordinal));
					Definitions.Add($"UE_VALIDATE_FORMAT_STRINGS={(ModuleCompileEnvironment.bValidateFormatStrings ? "1" : "0")}");
				}

				// Modify definitions if we need to create a new shared pch for validating internal api usage
				if (ModuleCompileEnvironment.bValidateInternalApi != Template.BaseCompileEnvironment.bValidateInternalApi)
				{
					Definitions = new List<string>(Definitions);
					Definitions.RemoveAll(x => x.Contains("UE_VALIDATE_INTERNAL_API", StringComparison.Ordinal));
					Definitions.Add($"UE_VALIDATE_INTERNAL_API={(ModuleCompileEnvironment.bValidateInternalApi ? "1" : "0")}");
				}

				// Create a suffix to distinguish this shared PCH variant from any others.
				string Variant = GetSuffixForSharedPCH(ModuleCompileEnvironment, Template.BaseCompileEnvironment);

				FileReference SharedDefinitionsLocation = FileReference.Combine(Template.OutputDir, String.Format("SharedDefinitions.{0}{1}.h", Template.Module.Name, Variant));
				List<string> NewDefinitions = new();
				StringBuilder Writer = new StringBuilder();
				WriteDefinitions($"Shared Definitions for {Template.Module.Name}{Variant}", Array.Empty<string>(), Definitions, Array.Empty<string>(), Writer);
				FileItem SharedDefinitionsFileItem = Graph.CreateIntermediateTextFile(SharedDefinitionsLocation, Writer.ToString(), AllowAsync: false);

				// Create the wrapper file, which sets all the definitions needed to compile it
				FileReference WrapperLocation = FileReference.Combine(Template.OutputDir, String.Format("SharedPCH.{0}{1}.h", Template.Module.Name, Variant));
				FileItem WrapperFile = CreatePCHWrapperFile(WrapperLocation, SharedDefinitionsFileItem, Template.HeaderFile, Graph);

				// Create the compile environment for this PCH
				CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(Template.BaseCompileEnvironment);
				CompileEnvironment.Definitions.Clear();
				CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				CompileEnvironment.PrecompiledHeaderIncludeFilename = WrapperFile.Location;
				CopySettingsForSharedPCH(ModuleCompileEnvironment, CompileEnvironment);

				// Setup PCH chaining
				PrecompiledHeaderInstance? ParentPCHInstance = null;
				Dictionary<string, string>? DefinitionsDictionary = null;
				if (Rules.Target.bChainPCHs)
				{
					// Create a lookup table for the definitions
					DefinitionsDictionary = Definitions.ToHashSet().ToDictionary(x => x.Split('=')[0], x => (x.Split("=").Length >= 2) ? x.Split("=")[1] : String.Empty);

					// Find all the dependencies of this module
					HashSet<UEBuildModule> ReferencedModules = new HashSet<UEBuildModule>();
					Template.Module.GetAllDependencyModules(new List<UEBuildModule>(), ReferencedModules, bIncludeDynamicallyLoaded: false, bForceCircular: false, bOnlyDirectDependencies: false);

					foreach (PrecompiledHeaderTemplate ParentTemplate in CompileEnvironment.SharedPCHs.Where(x => ReferencedModules.Contains(x.Module)))
					{
						if (ParentTemplate.IsValidFor(CompileEnvironment))
						{
							bool AreSharedPCHDefinitionsCompatible(PrecompiledHeaderInstance ParentInstance)
							{
								if (ParentInstance.DefinitionsDictionary == null)
								{
									return false;
								}

								foreach (KeyValuePair<string, string> Definition in DefinitionsDictionary)
								{
									if (ParentInstance.DefinitionsDictionary.TryGetValue(Definition.Key, out string? ParentValue))
									{
										if (ParentValue != Definition.Value)
										{
											return false;
										}
									}
								}
								return true;
							}

							ParentPCHInstance = ParentTemplate.Instances.Find(x => IsCompatibleForSharedPCH(x.CompileEnvironment, ModuleCompileEnvironment) && AreSharedPCHDefinitionsCompatible(x));
							if (ParentPCHInstance != null)
							{
								CompileEnvironment.ParentPCHInstance = ParentPCHInstance;
								break;
							}
						}
					}
				}

				// Create the PCH
				CPPOutput Output;
				if (ToolChain == null)
				{
					Output = new CPPOutput();
				}
				else
				{
					Output = ToolChain.CompileAllCPPFiles(CompileEnvironment, new List<FileItem>() { WrapperFile }, Template.OutputDir, "Shared", Graph);
				}
				Instance = new PrecompiledHeaderInstance(WrapperFile, SharedDefinitionsFileItem, CompileEnvironment, Output, GetImmutableDefinitions(Template.BaseCompileEnvironment.Definitions))
				{
					DefinitionsDictionary = DefinitionsDictionary,
					ParentPCHInstance = ParentPCHInstance
				};
				Template.Instances.Add(Instance);
			}

			Instance.Modules.Add(this);
			return Instance;
		}

		/// <summary>
		/// Determines if a module compile environment is compatible with the given shared PCH compile environment
		/// </summary>
		/// <param name="ModuleCompileEnvironment">The module compile environment</param>
		/// <param name="CompileEnvironment">The shared PCH compile environment</param>
		/// <returns>True if the two compile enviroments are compatible</returns>
		internal static bool IsCompatibleForSharedPCH(CppCompileEnvironment ModuleCompileEnvironment, CppCompileEnvironment CompileEnvironment)
		{
			if (ModuleCompileEnvironment.bTreatAsEngineModule != CompileEnvironment.bTreatAsEngineModule)
			{
				return false;
			}

			if (ModuleCompileEnvironment.bOptimizeCode != CompileEnvironment.bOptimizeCode)
			{
				return false;
			}
			if (ModuleCompileEnvironment.bUseRTTI != CompileEnvironment.bUseRTTI)
			{
				return false;
			}
			if (ModuleCompileEnvironment.bEnableExceptions != CompileEnvironment.bEnableExceptions)
			{
				return false;
			}
			if (ModuleCompileEnvironment.CppStandard != CompileEnvironment.CppStandard)
			{
				return false;
			}

			if (ModuleCompileEnvironment.IncludeOrderVersion != CompileEnvironment.IncludeOrderVersion)
			{
				return false;
			}
			
			if (ModuleCompileEnvironment.bValidateFormatStrings != CompileEnvironment.bValidateFormatStrings)
			{
				return false;
			}

			if (ModuleCompileEnvironment.bValidateInternalApi != CompileEnvironment.bValidateInternalApi)
			{
				return false;
			}
			return true;
		}

		/// <summary>
		/// Gets the unique suffix for a shared PCH
		/// </summary>
		/// <param name="CompileEnvironment">The shared PCH compile environment</param>
		/// <param name="BaseCompileEnvironment">The base compile environment</param>
		/// <returns>The unique suffix for the shared PCH</returns>
		private static string GetSuffixForSharedPCH(CppCompileEnvironment CompileEnvironment, CppCompileEnvironment BaseCompileEnvironment)
		{
			string Variant = "";
			if (CompileEnvironment.bTreatAsEngineModule != BaseCompileEnvironment.bTreatAsEngineModule)
			{
				if (CompileEnvironment.bTreatAsEngineModule)
				{
					Variant += ".Engine";
				}
				else
				{
					Variant += ".Project";
				}
			}
			if (CompileEnvironment.bOptimizeCode != BaseCompileEnvironment.bOptimizeCode)
			{
				if (CompileEnvironment.bOptimizeCode)
				{
					Variant += ".Optimized";
				}
				else
				{
					Variant += ".NonOptimized";
				}
			}
			if (CompileEnvironment.bUseRTTI != BaseCompileEnvironment.bUseRTTI)
			{
				if (CompileEnvironment.bUseRTTI)
				{
					Variant += ".RTTI";
				}
				else
				{
					Variant += ".NonRTTI";
				}
			}
			if (CompileEnvironment.bEnableExceptions != BaseCompileEnvironment.bEnableExceptions)
			{
				if (CompileEnvironment.bEnableExceptions)
				{
					Variant += ".Exceptions";
				}
				else
				{
					Variant += ".NoExceptions";
				}
			}
			if (CompileEnvironment.bValidateFormatStrings != BaseCompileEnvironment.bValidateFormatStrings)
			{
				if (CompileEnvironment.bValidateFormatStrings)
				{
					Variant += ".ValFmtStr";
				}
				else
				{
					Variant += ".NoValFmtStr";
				}
			}

			if (CompileEnvironment.bValidateInternalApi != BaseCompileEnvironment.bValidateInternalApi)
			{
				if (CompileEnvironment.bValidateInternalApi)
				{
					Variant += ".ValApi";
				}
				else
				{
					Variant += ".NoValApi";
				}
			}

			if (CompileEnvironment.bDeterministic != BaseCompileEnvironment.bDeterministic)
			{
				if (CompileEnvironment.bDeterministic)
				{
					Variant += ".Determ";
				}
				else
				{
					Variant += ".NonDeterm";
				}
			}

			// Always add the cpp standard into the filename to prevent variants when the engine standard is mismatched with a project
			switch (CompileEnvironment.CppStandard)
			{
				case CppStandardVersion.Cpp14: Variant += ".Cpp14"; break;
				case CppStandardVersion.Cpp17: Variant += ".Cpp17"; break;
				case CppStandardVersion.Cpp20: Variant += ".Cpp20"; break;
				case CppStandardVersion.Latest: Variant += ".CppLatest"; break;
			}

			if (CompileEnvironment.IncludeOrderVersion != BaseCompileEnvironment.IncludeOrderVersion)
			{
				if (CompileEnvironment.IncludeOrderVersion != EngineIncludeOrderVersion.Latest)
				{
					Variant += ".InclOrder" + CompileEnvironment.IncludeOrderVersion.ToString();
				}
			}

			return Variant;
		}

		/// <summary>
		/// Copy settings from the module's compile environment into the environment for the shared PCH
		/// </summary>
		/// <param name="ModuleCompileEnvironment">The module compile environment</param>
		/// <param name="CompileEnvironment">The shared PCH compile environment</param>
		private static void CopySettingsForSharedPCH(CppCompileEnvironment ModuleCompileEnvironment, CppCompileEnvironment CompileEnvironment)
		{
			CompileEnvironment.bOptimizeCode = ModuleCompileEnvironment.bOptimizeCode;
			CompileEnvironment.bCodeCoverage = ModuleCompileEnvironment.bCodeCoverage;
			CompileEnvironment.bUseRTTI = ModuleCompileEnvironment.bUseRTTI;
			CompileEnvironment.bEnableExceptions = ModuleCompileEnvironment.bEnableExceptions;
			CompileEnvironment.bTreatAsEngineModule = ModuleCompileEnvironment.bTreatAsEngineModule;
			CompileEnvironment.CppStandardEngine = ModuleCompileEnvironment.CppStandardEngine;
			CompileEnvironment.CppStandard = ModuleCompileEnvironment.CppStandard;
			CompileEnvironment.IncludeOrderVersion = ModuleCompileEnvironment.IncludeOrderVersion;
			CompileEnvironment.bUseAutoRTFMCompiler = ModuleCompileEnvironment.bUseAutoRTFMCompiler;
			CompileEnvironment.bAllowAutoRTFMInstrumentation = ModuleCompileEnvironment.bAllowAutoRTFMInstrumentation;
			CompileEnvironment.bValidateFormatStrings = ModuleCompileEnvironment.bValidateFormatStrings;
			CompileEnvironment.bValidateInternalApi = ModuleCompileEnvironment.bValidateInternalApi;
		}

		/// <summary>
		/// Compiles the provided CPP unity files. Will
		/// </summary>
		private CPPOutput CompileFilesWithToolChain(
			ReadOnlyTargetRules Target,
			UEToolChain ToolChain,
			CppCompileEnvironment CompileEnvironment,
			CppCompileEnvironment ModuleCompileEnvironment,
			List<FileItem> NormalFiles,
			List<FileItem> AdaptiveFiles,
			IActionGraphBuilder Graph,
			ILogger Logger)
		{
			bool bAdaptiveUnityDisablesPCH = false;
			if (Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs)
			{
				if (Rules.bTreatAsEngineModule || Rules.PrivatePCHHeaderFile == null)
				{
					bAdaptiveUnityDisablesPCH = Target.bAdaptiveUnityDisablesPCH;
				}
				else
				{
					bAdaptiveUnityDisablesPCH = Target.bAdaptiveUnityDisablesPCHForProject;
				}
			}

			if (AdaptiveFiles.Count > 0)
			{
				if (Target.bAdaptiveUnityCreatesDedicatedPCH)
				{
					Graph.AddDiagnostic("[Adaptive Build] Creating dedicated PCH for each excluded file. Set bAdaptiveUnityCreatesDedicatedPCH to false in BuildConfiguration.xml to change this behavior.");
				}
				else if (Target.bAdaptiveUnityDisablesPCH)
				{
					Graph.AddDiagnostic("[Adaptive Build] Disabling PCH for excluded files. Set bAdaptiveUnityDisablesPCH to false in BuildConfiguration.xml to change this behavior.");
				}

				if (Target.bAdaptiveUnityDisablesOptimizations)
				{
					Graph.AddDiagnostic("[Adaptive Build] Disabling optimizations for excluded files. Set bAdaptiveUnityDisablesOptimizations to false in BuildConfiguration.xml to change this behavior.");
				}
				if (Target.bAdaptiveUnityEnablesEditAndContinue)
				{
					Graph.AddDiagnostic("[Adaptive Build] Enabling Edit & Continue for excluded files. Set bAdaptiveUnityEnablesEditAndContinue to false in BuildConfiguration.xml to change this behavior.");
				}

				Graph.AddDiagnostic($"[Adaptive Build] Excluded from {Name} unity file: " + String.Join(", ", AdaptiveFiles.Select(File => Path.GetFileName(File.AbsolutePath))));
			}

			if ((!Target.bAdaptiveUnityDisablesOptimizations && !bAdaptiveUnityDisablesPCH && !Target.bAdaptiveUnityCreatesDedicatedPCH) || Target.bStressTestUnity)
			{
				NormalFiles = NormalFiles.Concat(AdaptiveFiles).ToList();
				AdaptiveFiles = new List<FileItem>();
			}

			CPPOutput OutputFiles = new CPPOutput();

			if (NormalFiles.Count > 0)
			{
				OutputFiles = ToolChain.CompileAllCPPFiles(CompileEnvironment, NormalFiles, IntermediateDirectory, Name, Graph);
			}

			if (AdaptiveFiles.Count > 0)
			{
				// Create the new compile environment. Always turn off PCH due to different compiler settings.
				CppCompileEnvironment AdaptiveUnityEnvironment = new CppCompileEnvironment(ModuleCompileEnvironment);
				if (Target.bAdaptiveUnityDisablesOptimizations)
				{
					AdaptiveUnityEnvironment.bOptimizeCode = false;
				}
				if (Target.bAdaptiveUnityEnablesEditAndContinue)
				{
					AdaptiveUnityEnvironment.bSupportEditAndContinue = true;
				}

				// Create a per-file PCH
				CPPOutput AdaptiveOutput;
				if (Target.bAdaptiveUnityCreatesDedicatedPCH)
				{
					AdaptiveOutput = CompileAdaptiveNonUnityFilesWithDedicatedPCH(ToolChain, AdaptiveUnityEnvironment, AdaptiveFiles, IntermediateDirectory, Name, Graph);
				}
				else if (bAdaptiveUnityDisablesPCH)
				{
					AdaptiveOutput = CompileAdaptiveNonUnityFilesWithoutPCH(ToolChain, AdaptiveUnityEnvironment, AdaptiveFiles, IntermediateDirectory, Name, Graph);
				}
				else if (AdaptiveUnityEnvironment.bOptimizeCode != CompileEnvironment.bOptimizeCode || AdaptiveUnityEnvironment.bSupportEditAndContinue != CompileEnvironment.bSupportEditAndContinue)
				{
					AdaptiveOutput = CompileAdaptiveNonUnityFiles(ToolChain, AdaptiveUnityEnvironment, AdaptiveFiles, IntermediateDirectory, Name, Graph);
				}
				else
				{
					AdaptiveOutput = CompileAdaptiveNonUnityFiles(ToolChain, CompileEnvironment, AdaptiveFiles, IntermediateDirectory, Name, Graph);
				}

				// Merge output
				OutputFiles.ObjectFiles.AddRange(AdaptiveOutput.ObjectFiles);
			}

			return OutputFiles;
		}

		static CPPOutput CompileAdaptiveNonUnityFiles(UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, List<FileItem> Files, DirectoryReference IntermediateDirectory, string ModuleName, IActionGraphBuilder Graph)
		{
			// Write all the definitions out to a separate file
			CreateHeaderForDefinitions(CompileEnvironment, IntermediateDirectory, "Adaptive", Graph);

			// Compile the files
			return ToolChain.CompileAllCPPFiles(CompileEnvironment, Files, IntermediateDirectory, ModuleName, Graph);
		}

		static CPPOutput CompileAdaptiveNonUnityFilesWithoutPCH(UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, List<FileItem> Files, DirectoryReference IntermediateDirectory, string ModuleName, IActionGraphBuilder Graph)
		{
			// Disable precompiled headers
			CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.None;

			// Write all the definitions out to a separate file
			CreateHeaderForDefinitions(CompileEnvironment, IntermediateDirectory, "Adaptive", Graph);

			// Compile the files
			return ToolChain.CompileAllCPPFiles(CompileEnvironment, Files, IntermediateDirectory, ModuleName, Graph);
		}

		static CPPOutput CompileAdaptiveNonUnityFilesWithDedicatedPCH(UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, List<FileItem> Files, DirectoryReference IntermediateDirectory, string ModuleName, IActionGraphBuilder Graph)
		{
			CPPOutput Output = new CPPOutput();
			foreach (FileItem File in Files)
			{
				// Build the contents of the wrapper file
				StringBuilder WrapperContents = new StringBuilder();
				{
					string FileString = File.AbsolutePath;
					if (File.Location.IsUnderDirectory(Unreal.RootDirectory))
					{
						FileString = File.Location.MakeRelativeTo(Unreal.EngineSourceDirectory);
					}
					FileString = FileString.Replace('\\', '/');
					WriteDefinitions($"Dedicated PCH for {FileString}", Array.Empty<string>(), CompileEnvironment.Definitions, Array.Empty<string>(), WrapperContents);
					WrapperContents.AppendLine();
					using (StreamReader Reader = new StreamReader(File.Location.FullName))
					{
						CppIncludeParser.CopyIncludeDirectives(Reader, WrapperContents);
					}
				}

				// Write the PCH header
				FileReference DedicatedPchLocation = FileReference.Combine(IntermediateDirectory, String.Format("PCH.Dedicated.{0}.h", File.Location.GetFileNameWithoutExtension()));
				FileItem DedicatedPchFile = Graph.CreateIntermediateTextFile(DedicatedPchLocation, WrapperContents.ToString());

				// Create a new C++ environment to compile the PCH
				CppCompileEnvironment PchEnvironment = new CppCompileEnvironment(CompileEnvironment);
				PchEnvironment.Definitions.Clear();
				PchEnvironment.UserIncludePaths.Add(File.Location.Directory); // Need to be able to include headers in the same directory as the source file
				PchEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Create;
				PchEnvironment.PrecompiledHeaderIncludeFilename = DedicatedPchFile.Location;

				// Create the action to compile the PCH file.
				CPPOutput PchOutput = ToolChain.CompileAllCPPFiles(PchEnvironment, new List<FileItem>() { DedicatedPchFile }, IntermediateDirectory, ModuleName, Graph);
				Output.ObjectFiles.AddRange(PchOutput.ObjectFiles);

				// Create a new C++ environment to compile the original file
				CppCompileEnvironment FileEnvironment = new CppCompileEnvironment(CompileEnvironment);
				FileEnvironment.Definitions.Clear();
				FileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
				FileEnvironment.PrecompiledHeaderIncludeFilename = DedicatedPchFile.Location;
				FileEnvironment.PCHInstance = new PrecompiledHeaderInstance(DedicatedPchFile, DedicatedPchFile, PchEnvironment, PchOutput, GetImmutableDefinitions(CompileEnvironment.Definitions));

				// Create the action to compile the PCH file.
				CPPOutput FileOutput = ToolChain.CompileAllCPPFiles(FileEnvironment, new List<FileItem>() { File }, IntermediateDirectory, ModuleName, Graph);
				Output.ObjectFiles.AddRange(FileOutput.ObjectFiles);
			}
			return Output;
		}

		/// <summary>
		/// Configure precompiled headers for this module
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="ToolChain">The toolchain to build with</param>
		/// <param name="CompileEnvironment">The current compile environment</param>
		/// <param name="LinkInputFiles">List of files that will be linked for the target</param>
		/// <param name="Graph">List of build actions</param>
		/// <param name="Logger">Logger for output</param>
		CppCompileEnvironment SetupPrecompiledHeaders(ReadOnlyTargetRules Target, UEToolChain? ToolChain, CppCompileEnvironment CompileEnvironment, List<FileItem> LinkInputFiles, IActionGraphBuilder Graph, ILogger Logger)
		{
			if (Target.bUsePCHFiles && Rules.PCHUsage != ModuleRules.PCHUsageMode.NoPCHs)
			{
				// If this module doesn't need a shared PCH, configure that
				if (Rules.PrivatePCHHeaderFile != null && (Rules.PCHUsage == ModuleRules.PCHUsageMode.NoSharedPCHs || Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs))
				{
					PrecompiledHeaderInstance Instance = CreatePrivatePCH(ToolChain, FileItem.GetItemByFileReference(FileReference.Combine(ModuleDirectory, Rules.PrivatePCHHeaderFile)), CompileEnvironment, Graph);

					CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
					CompileEnvironment.Definitions.Clear();
					CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
					CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Location;
					CompileEnvironment.PCHInstance = Instance;

					LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
				}

				// Try to find a suitable shared PCH for this module
				if (CompileEnvironment.bHasPrecompiledHeader == false && CompileEnvironment.SharedPCHs.Count > 0 && !CompileEnvironment.bIsBuildingLibrary && Rules.PCHUsage != ModuleRules.PCHUsageMode.NoSharedPCHs)
				{
					// Find the first shared PCH module we can use that doesn't reference this module
					HashSet<UEBuildModule> AllDependencies = GetAllDependencyModulesForPCH(true, false);
					PrecompiledHeaderTemplate? Template = CompileEnvironment.SharedPCHs.FirstOrDefault(x => !x.ModuleDependencies.Contains(this) && AllDependencies.Contains(x.Module) && x.IsValidFor(CompileEnvironment));

					if (Template != null)
					{
						PrecompiledHeaderInstance Instance = FindOrCreateSharedPCH(ToolChain, Template, CompileEnvironment, Graph);

						FileReference PrivateDefinitionsFile = FileReference.Combine(IntermediateDirectory, $"Definitions.{Rules.ShortName ?? Name}.h");

						FileItem PrivateDefinitionsFileItem;
						{
							// Only add new definitions that are not already existing in the shared pch
							List<string> NewDefinitions = new();
							List<string> Undefintions = new();
							bool ModuleApiUndef = false;
							foreach (string Definition in CompileEnvironment.Definitions)
							{
								if (Instance.ImmutableDefinitions.Contains(Definition))
								{
									continue;
								}

								NewDefinitions.Add(Definition);

								// Remove the module _API definition for cases where there are circular dependencies between the shared PCH module and modules using it
								if (!ModuleApiUndef && Definition.StartsWith(ModuleApiDefine, StringComparison.Ordinal))
								{
									ModuleApiUndef = true;
									Undefintions.Add(ModuleApiDefine);
								}
							}

							StringBuilder Writer = new();
							WriteDefinitions($"Shared PCH Definitions for {Name}", new string[] { GetIncludeString(Instance.DefinitionsFile) }, NewDefinitions, Undefintions, Writer);
							PrivateDefinitionsFileItem = Graph.CreateIntermediateTextFile(PrivateDefinitionsFile, Writer.ToString());
						}

						CompileEnvironment = new CppCompileEnvironment(CompileEnvironment);
						CompileEnvironment.Definitions.Clear();
						CompileEnvironment.ForceIncludeFiles.Add(PrivateDefinitionsFileItem);
						CompileEnvironment.PrecompiledHeaderAction = PrecompiledHeaderAction.Include;
						CompileEnvironment.PrecompiledHeaderIncludeFilename = Instance.HeaderFile.Location;
						CompileEnvironment.PCHInstance = Instance;

						LinkInputFiles.AddRange(Instance.Output.ObjectFiles);
					}
				}
			}
			return CompileEnvironment;
		}

		/// <summary>
		/// Creates a header file containing all the preprocessor definitions for a compile environment, and force-include it. We allow a more flexible syntax for preprocessor definitions than
		/// is typically allowed on the command line (allowing function macros or double-quote characters, for example). Ensuring all definitions are specified in a header files ensures consistent
		/// behavior.
		/// </summary>
		/// <param name="CompileEnvironment">The compile environment</param>
		/// <param name="IntermediateDirectory">Directory to create the intermediate file</param>
		/// <param name="HeaderSuffix">Suffix for the included file</param>
		/// <param name="Graph">The action graph being built</param>
		static FileItem? CreateHeaderForDefinitions(CppCompileEnvironment CompileEnvironment, DirectoryReference IntermediateDirectory, string? HeaderSuffix, IActionGraphBuilder Graph)
		{
			if (CompileEnvironment.Definitions.Count > 0)
			{
				string PrivateDefinitionsName = "Definitions.h";

				if (!String.IsNullOrEmpty(HeaderSuffix))
				{
					PrivateDefinitionsName = $"Definitions.{HeaderSuffix}.h";
				}

				FileReference PrivateDefinitionsFile = FileReference.Combine(IntermediateDirectory, PrivateDefinitionsName);
				{
					StringBuilder Writer = new();
					WriteDefinitions("Definitions", Array.Empty<string>(), CompileEnvironment.Definitions, Array.Empty<string>(), Writer);
					CompileEnvironment.Definitions.Clear();

					FileItem PrivateDefinitionsFileItem = Graph.CreateIntermediateTextFile(PrivateDefinitionsFile, Writer.ToString());
					CompileEnvironment.ForceIncludeFiles.Add(PrivateDefinitionsFileItem);
					return PrivateDefinitionsFileItem;
				}
			}
			return null;
		}

		/// <summary>
		/// Creates header files from ISPC for inclusion and adds them as dependencies.
		/// </summary>
		/// <param name="ToolChain">The toolchain to generate the PCH</param>
		/// <param name="CompileEnvironment">Compile environment</param>
		/// <param name="InputFiles">List of ISPC source files</param>
		/// <param name="IntermediateDirectory">Directory to create the intermediate file</param>
		/// <param name="Graph">List of actions to be executed. Additional actions will be added to this list.</param>
		static void CreateHeadersForISPC(UEToolChain ToolChain, CppCompileEnvironment CompileEnvironment, List<FileItem> InputFiles, DirectoryReference IntermediateDirectory, IActionGraphBuilder Graph)
		{
			CPPOutput Output = ToolChain.GenerateISPCHeaders(CompileEnvironment, InputFiles, IntermediateDirectory, Graph);

			CompileEnvironment.AdditionalPrerequisites.AddRange(Output.GeneratedHeaderFiles);
			CompileEnvironment.UserIncludePaths.Add(IntermediateDirectory);
		}

		static string GetIncludeString(FileItem FileItem)
		{
			string IncludeFileString = FileItem.AbsolutePath;
			if (FileItem.Location.IsUnderDirectory(Unreal.RootDirectory))
			{
				IncludeFileString = FileItem.Location.MakeRelativeTo(Unreal.EngineSourceDirectory);
			}
			return IncludeFileString.Replace('\\', '/');
		}

		/// <summary>
		/// Create a header file containing the module definitions, which also includes the PCH itself. Including through another file is necessary on 
		/// Clang, since we get warnings about #pragma once otherwise, but it also allows us to consistently define the preprocessor state on all 
		/// platforms.
		/// </summary>
		/// <param name="OutputFile">The output file to create</param>
		/// <param name="DefinitionsFile">File containing definitions required by the PCH</param>
		/// <param name="IncludedFile">The PCH file to include</param>
		/// <param name="Graph">The action graph builder</param>
		/// <returns>FileItem for the created file</returns>
		static FileItem CreatePCHWrapperFile(FileReference OutputFile, FileItem DefinitionsFile, FileItem IncludedFile, IActionGraphBuilder Graph)
		{
			// Build the contents of the wrapper file
			StringBuilder WrapperContents = new StringBuilder();
			{
				string IncludeFileString = GetIncludeString(IncludedFile);
				string DefinitionsFileString = GetIncludeString(DefinitionsFile);
				WrapperContents.AppendLine("// PCH for {0}", IncludeFileString);
				WrapperContents.AppendLine("#include \"{0}\"", DefinitionsFileString);
				WrapperContents.AppendLine("#include \"{0}\"", IncludeFileString);
			}

			// Create the item
			FileItem WrapperFile = Graph.CreateIntermediateTextFile(OutputFile, WrapperContents.ToString(), AllowAsync: false);

			// Touch it if the included file is newer, to make sure our timestamp dependency checking is accurate.
			if (IncludedFile.LastWriteTimeUtc > WrapperFile.LastWriteTimeUtc)
			{
				File.SetLastWriteTimeUtc(WrapperFile.AbsolutePath, DateTime.UtcNow);
				WrapperFile.ResetCachedInfo();
			}
			return WrapperFile;
		}

		/// <summary>
		/// Write a list of macro definitions to an output file
		/// </summary>
		/// <param name="Context">Additional context to add to generated comment</param>
		/// <param name="Includes">List of includes</param>
		/// <param name="Definitions">List of definitions</param>
		/// <param name="Undefinitions">List of definitions to undefine</param>
		/// <param name="Writer">Writer to receive output</param>
		static void WriteDefinitions(string Context, IEnumerable<string> Includes, IEnumerable<string> Definitions, IEnumerable<string> Undefinitions, StringBuilder Writer)
		{
			Writer.AppendLine($"// Generated by UnrealBuildTool (UEBuildModuleCPP.cs) : {Context}");
			Writer.AppendLine("#pragma once");
			foreach (string Include in Includes)
			{
				Writer.Append("#include \"").Append(Include).Append('\"').AppendLine();
			}
			foreach (string Undefinition in Undefinitions)
			{
				Writer.Append("#undef ").Append(Undefinition).AppendLine();
			}

			HashSet<string> Processed = new();
			foreach (string Definition in Definitions.Where(x => Processed.Add(x)))
			{
				int EqualsIdx = Definition.IndexOf('=', StringComparison.Ordinal);
				if (EqualsIdx == -1)
				{
					Writer.Append("#define ").Append(Definition).AppendLine(" 1");
				}
				else
				{
					Writer.Append("#define ").Append(Definition.AsSpan(0, EqualsIdx)).Append(' ').Append(Definition.AsSpan(EqualsIdx + 1)).AppendLine();
				}
			}
		}

		/// <summary>
		/// Checks that the first header included by the source files in this module all include the same header
		/// </summary>
		/// <param name="Target">The target being compiled</param>
		/// <param name="ModuleCompileEnvironment">Compile environment for the module</param>
		/// <param name="HeaderFiles">All header files for this module</param>
		/// <param name="CppFiles">List of C++ source files</param>
		private void CheckFirstIncludeMatchesEachCppFile(ReadOnlyTargetRules Target, CppCompileEnvironment ModuleCompileEnvironment, List<FileItem> HeaderFiles, List<FileItem> CppFiles)
		{
			if (Rules.PCHUsage == ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs)
			{
				if (InvalidIncludeDirectives == null)
				{
					// Find headers used by the source file.
					Dictionary<string, FileReference> NameToHeaderFile = new Dictionary<string, FileReference>();
					foreach (FileItem HeaderFile in HeaderFiles)
					{
						NameToHeaderFile[HeaderFile.Location.GetFileNameWithoutExtension()] = HeaderFile.Location;
					}

					// Find the directly included files for each source file, and make sure it includes the matching header if possible
					InvalidIncludeDirectives = new();
					if (Rules != null && Rules.IWYUSupport != IWYUSupport.None && Target.bEnforceIWYU)
					{
						foreach (FileItem CppFile in CppFiles)
						{
							string? FirstInclude = ModuleCompileEnvironment.MetadataCache.GetFirstInclude(CppFile);
							if (FirstInclude != null)
							{
								string IncludeName = Path.GetFileNameWithoutExtension(FirstInclude);
								string ExpectedName = CppFile.Location.GetFileNameWithoutExtension();
								if (String.Compare(IncludeName, ExpectedName, StringComparison.OrdinalIgnoreCase) != 0)
								{
									FileReference? HeaderFile;
									if (NameToHeaderFile.TryGetValue(ExpectedName, out HeaderFile) && !IgnoreMismatchedHeader(ExpectedName))
									{
										InvalidIncludeDirectives.Add(new InvalidIncludeDirective(CppFile.Location, HeaderFile));
									}
								}
							}
						}
					}
				}
			}
		}

		private bool IgnoreMismatchedHeader(string ExpectedName)
		{
			switch (Name)
			{
				case "D3D11RHI":
				case "D3D12RHI":
				case "VulkanRHI":
				case "OpenGLDrv":
				case "MetalRHI":
					return true;
			}
			return false;
		}

		private void CompileEnvironmentDebugInfoSettings(ReadOnlyTargetRules Target, CppCompileEnvironment Result)
		{
			// If bCreateDebugInfo is disabled for the whole target do not adjust any settings
			if (!Result.bCreateDebugInfo)
			{
				return;
			}

			if (Rules.Target.ProjectFile != null && Rules.File.IsUnderDirectory(Rules.Target.ProjectFile.Directory))
			{
				if (Target.DebugInfoLineTablesOnly.HasFlag(DebugInfoMode.ProjectPlugins) && Rules.Plugin != null)
				{
					Result.bDebugLineTablesOnly = true;
				}
				else if (Target.DebugInfoLineTablesOnly.HasFlag(DebugInfoMode.Project) && Rules.Plugin == null)
				{
					Result.bDebugLineTablesOnly = true;
				}
			}
			else
			{
				if (Target.DebugInfoLineTablesOnly.HasFlag(DebugInfoMode.EnginePlugins) && Rules.Plugin != null)
				{
					Result.bDebugLineTablesOnly = true;
				}
				else if (Target.DebugInfoLineTablesOnly.HasFlag(DebugInfoMode.Engine) && Rules.Plugin == null)
				{
					Result.bDebugLineTablesOnly = true;
				}
			}

			if (Rules.Plugin != null && Rules.Target.DebugInfoLineTablesOnlyPlugins.Contains(Rules.Plugin.Name))
			{
				Result.bDebugLineTablesOnly = true;
			}
			else if (Rules.Target.DebugInfoLineTablesOnlyModules.Contains(Name))
			{
				Result.bDebugLineTablesOnly = true;
			}

			if (Result.bDebugLineTablesOnly)
			{
				// Don't disable debug info if only line tables are requested
				return;
			}

			// Disable debug info for modules if requested
			if (!Target.bUsePDBFiles || !Target.Platform.IsInGroup("Microsoft"))
			{
				if (Rules.Target.ProjectFile != null && Rules.File.IsUnderDirectory(Rules.Target.ProjectFile.Directory))
				{
					if (!Target.DebugInfo.HasFlag(DebugInfoMode.ProjectPlugins) && Rules.Plugin != null)
					{
						Result.bCreateDebugInfo = false;
					}
					else if (!Target.DebugInfo.HasFlag(DebugInfoMode.Project) && Rules.Plugin == null)
					{
						Result.bCreateDebugInfo = false;
					}
				}
				else
				{
					if (!Target.DebugInfo.HasFlag(DebugInfoMode.EnginePlugins) && Rules.Plugin != null)
					{
						Result.bCreateDebugInfo = false;
					}
					else if (!Target.DebugInfo.HasFlag(DebugInfoMode.Engine) && Rules.Plugin == null)
					{
						Result.bCreateDebugInfo = false;
					}
				}

				if (Rules.Plugin != null && Rules.Target.DisableDebugInfoPlugins.Contains(Rules.Plugin.Name))
				{
					Result.bCreateDebugInfo = false;
				}
				else if (Rules.Target.DisableDebugInfoModules.Contains(Name))
				{
					Result.bCreateDebugInfo = false;
				}
			}
		}

		/// <summary>
		/// Determine whether optimization should be enabled for a given target
		/// </summary>
		/// <param name="Setting">The optimization setting from the rules file</param>
		/// <param name="Configuration">The active target configuration</param>
		/// <param name="bIsEngineModule">Whether the current module is an engine module</param>
		/// <param name="bCodeCoverage">Whether the current module should be compiled with code coverage support</param>
		/// <returns>True if optimization should be enabled</returns>
		public static bool ShouldEnableOptimization(ModuleRules.CodeOptimization Setting, UnrealTargetConfiguration Configuration, bool bIsEngineModule, bool bCodeCoverage)
		{
			if (bCodeCoverage) {
				return false;
			}
			switch (Setting)
			{
				case ModuleRules.CodeOptimization.Never:
					return false;
				case ModuleRules.CodeOptimization.Default:
				case ModuleRules.CodeOptimization.InNonDebugBuilds:
					return (Configuration == UnrealTargetConfiguration.Debug) ? false : (Configuration != UnrealTargetConfiguration.DebugGame || bIsEngineModule);
				case ModuleRules.CodeOptimization.InShippingBuildsOnly:
					return (Configuration == UnrealTargetConfiguration.Shipping);
				default:
					return true;
			}
		}

		public CppCompileEnvironment CreateCompileEnvironmentForIntellisense(ReadOnlyTargetRules Target, CppCompileEnvironment BaseCompileEnvironment, ILogger Logger)
		{
			CppCompileEnvironment CompileEnvironment = CreateModuleCompileEnvironment(Target, BaseCompileEnvironment, Logger);
			CompileEnvironment = SetupPrecompiledHeaders(Target, null, CompileEnvironment, new List<FileItem>(), new NullActionGraphBuilder(Logger), Logger);
			CreateHeaderForDefinitions(CompileEnvironment, IntermediateDirectory, null, new NullActionGraphBuilder(Logger));
			return CompileEnvironment;
		}

		/// <summary>
		/// Creates a compile environment from a base environment based on the module settings.
		/// </summary>
		/// <param name="Target">Rules for the target being built</param>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>The new module compile environment.</returns>
		public CppCompileEnvironment CreateModuleCompileEnvironment(ReadOnlyTargetRules Target, CppCompileEnvironment BaseCompileEnvironment, ILogger Logger)
		{
			CppCompileEnvironment Result = new CppCompileEnvironment(BaseCompileEnvironment);

			// Override compile environment
			Result.bUseUnity = Rules.bUseUnity;
			Result.bCodeCoverage = Target.bCodeCoverage;
			Result.bOptimizeCode = ShouldEnableOptimization(Rules.OptimizeCode, Target.Configuration, Rules.bTreatAsEngineModule, Result.bCodeCoverage);
			Result.bUseRTTI |= Rules.bUseRTTI;
			Result.bVcRemoveUnreferencedComdat &= Rules.bVcRemoveUnreferencedComdat;
			Result.bEnableBufferSecurityChecks = Rules.bEnableBufferSecurityChecks;
			Result.MinSourceFilesForUnityBuildOverride = Rules.MinSourceFilesForUnityBuildOverride;
			Result.MinFilesUsingPrecompiledHeaderOverride = Rules.MinFilesUsingPrecompiledHeaderOverride;
			Result.bBuildLocallyWithSNDBS = Rules.bBuildLocallyWithSNDBS;
			Result.bEnableExceptions |= Rules.bEnableExceptions;
			Result.bEnableObjCExceptions |= Rules.bEnableObjCExceptions;
			Result.bEnableObjCAutomaticReferenceCounting = Rules.bEnableObjCAutomaticReferenceCounting;
			Result.bWarningsAsErrors |= Rules.bWarningsAsErrors;
			Result.ShadowVariableWarningLevel = Rules.ShadowVariableWarningLevel;
			Result.UnsafeTypeCastWarningLevel = Rules.UnsafeTypeCastWarningLevel;
			Result.bDisableStaticAnalysis = Rules.bDisableStaticAnalysis || (Target.bStaticAnalyzerProjectOnly && Rules.bTreatAsEngineModule);
			Result.bStaticAnalyzerExtensions = Rules.bStaticAnalyzerExtensions;
			Result.StaticAnalyzerRulesets = Rules.StaticAnalyzerRulesets;
			Result.StaticAnalyzerCheckers = Rules.StaticAnalyzerCheckers;
			Result.StaticAnalyzerDisabledCheckers = Rules.StaticAnalyzerDisabledCheckers;
			Result.StaticAnalyzerAdditionalCheckers = Rules.StaticAnalyzerAdditionalCheckers;
			Result.bEnableUndefinedIdentifierWarnings = Rules.bEnableUndefinedIdentifierWarnings;
			Result.bTreatAsEngineModule = Rules.bTreatAsEngineModule;
			Result.IncludeOrderVersion = Rules.IncludeOrderVersion;
			Result.DeterministicWarningLevel = Rules.DeterministicWarningLevel;
			Result.bValidateFormatStrings = Rules.bValidateFormatStrings;
			Result.bValidateInternalApi = Rules.bValidateInternalApi;
			Result.bUseAutoRTFMCompiler = Target.bUseAutoRTFMCompiler;

			CompileEnvironmentDebugInfoSettings(Target, Result);

			// Only enable the AutoRTFM flag if we are using the AutoRTFM compiler
			if (Target.bUseAutoRTFMCompiler)
			{
				Result.bAllowAutoRTFMInstrumentation = Rules.bAllowAutoRTFMInstrumentation;
			}

			if (Result.OptimizationLevel != Rules.OptimizationLevel)
			{
				Logger.LogInformation("Module {0} - Optimization level changed for module due to override. Old: {1} New: {2}", Name, Result.OptimizationLevel, Rules.OptimizationLevel);
				if (Rules.PrivatePCHHeaderFile == null)
				{
					if (Rules.PCHUsage != ModuleRules.PCHUsageMode.NoPCHs)
					{
						Logger.LogInformation("  Overriding OptimizationLevel requires a private PCH. Disabling PCH usage for {0}", Name);
						Rules.PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
					}
				}
				else if (Rules.PCHUsage == ModuleRules.PCHUsageMode.UseSharedPCHs)
				{
					Logger.LogInformation("  Overriding OptimizationLevel requires a private PCH. A private PCH exists but UseSharedPCHs was specified. Overriding to NoSharedPCHs for {0}", Name);
					Rules.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
				}
				Result.OptimizationLevel = Rules.OptimizationLevel;
			}

			if (Result.FPSemantics != Rules.FPSemantics)
			{
				if (Rules.PrivatePCHHeaderFile == null)
				{
					if (Rules.PCHUsage != ModuleRules.PCHUsageMode.NoPCHs)
					{
						Logger.LogInformation($"  Overriding FPSemantics requires a private PCH. Disabling PCH usage for {Name}");
						Rules.PCHUsage = ModuleRules.PCHUsageMode.NoPCHs;
					}
				}
				else if (Rules.PCHUsage == ModuleRules.PCHUsageMode.UseSharedPCHs)
				{
					Logger.LogInformation($"  Overriding FPSemantics requires a private PCH. A private PCH exists but UseSharedPCHs was specified. Overriding to NoSharedPCHs for {Name}");
					Rules.PCHUsage = ModuleRules.PCHUsageMode.NoSharedPCHs;
				}
				Result.FPSemantics = Rules.FPSemantics;
			}

			// If the module overrides the C++ language version, override it on the compile environment
			if (Rules.CppStandard != null)
			{
				Result.CppStandard = (CppStandardVersion)Rules.CppStandard;
			}
			// Otherwise, for engine modules use the engine standard (typically CppStandardVersion.EngineDefault).
			else if (Rules.bTreatAsEngineModule)
			{
				Result.CppStandard = Result.CppStandardEngine;
			}
			// CppModules require at least Cpp20.
			if (Target.bEnableCppModules && Result.CppStandard < CppStandardVersion.Cpp20)
			{
				Result.CppStandard = CppStandardVersion.Cpp20;
			}

			// If the module overrides the C language version, override it on the compile environment
			if (Rules.CStandard != null)
			{
				Result.CStandard = (CStandardVersion)Rules.CStandard;
			}

			// If the module overrides the x64 minimum arch, override it on the compile environment
			if (Rules.MinCpuArchX64 != null)
			{
				Result.MinCpuArchX64 = (MinimumCpuArchitectureX64)Rules.MinCpuArchX64;
			}

			// Set the macro used to check whether monolithic headers can be used
			if ((Rules.bTreatAsEngineModule || Target.bWarnAboutMonolithicHeadersIncluded) && (Rules.IWYUSupport == IWYUSupport.None || !Target.bEnforceIWYU))
			{
				Result.Definitions.Add("SUPPRESS_MONOLITHIC_HEADER_WARNINGS=1");
			}

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			Result.Definitions.Add($"UE_IS_ENGINE_MODULE={(Rules.bTreatAsEngineModule || Target.bWarnAboutMonolithicHeadersIncluded ? "1" : "0")}");

			if (!Rules.bTreatAsEngineModule)
			{
				Result.Definitions.Add("DEPRECATED_FORGAME=DEPRECATED");
				Result.Definitions.Add("UE_DEPRECATED_FORGAME=UE_DEPRECATED");
			}

			Result.Definitions.Add($"UE_VALIDATE_FORMAT_STRINGS={(Rules.bValidateFormatStrings ? "1" : "0")}");
			Result.Definitions.Add($"UE_VALIDATE_INTERNAL_API={(Rules.bValidateInternalApi ? "1" : "0")}");

			Result.Definitions.AddRange(EngineIncludeOrderHelper.GetDeprecationDefines(Rules.IncludeOrderVersion));

			// If module intermediate files are under the project folder we can set UE_PROJECT_NAME and friends.
			// This makes it possible to create modular builds where you don't have to provide the project name on cmd line
			if (Target.ProjectFile != null && IntermediateDirectory.IsUnderDirectory(Target.ProjectFile.Directory))
			{
				string ProjectName = Target.ProjectFile.GetFileNameWithoutExtension();
				Result.Definitions.Add(String.Format("UE_PROJECT_NAME={0}", ProjectName));
				Result.Definitions.Add(String.Format("UE_TARGET_NAME={0}", Target.OriginalName));
			}

			// Add the module's public and private definitions.
			AddDefinitions(Result.Definitions, PublicDefinitions);

			Result.Definitions.AddRange(Rules.PrivateDefinitions);
			
			if (Rules.Name == "BuildSettings")
			{
				Result.Definitions.Add(String.Format("UE_WITH_DEBUG_INFO={0}", BaseCompileEnvironment.bCreateDebugInfo ? "1" : "0"));
			}

			// Add the project definitions
			if (!Rules.bTreatAsEngineModule)
			{
				Result.Definitions.AddRange(Rules.Target.ProjectDefinitions);
			}

			// Setup the compile environment for the module.
			SetupPrivateCompileEnvironment(Result.UserIncludePaths, Result.SystemIncludePaths, Result.ModuleInterfacePaths, Result.Definitions, Result.AdditionalFrameworks, Result.AdditionalPrerequisites, Rules.bLegacyPublicIncludePaths, Rules.bLegacyParentIncludePaths);

			return Result;
		}

		/// <summary>
		/// Creates a compile environment for a shared PCH from a base environment based on the module settings.
		/// </summary>
		/// <param name="Target">The target being built</param>
		/// <param name="BaseCompileEnvironment">An existing environment to base the module compile environment on.</param>
		/// <returns>The new shared PCH compile environment.</returns>
		public CppCompileEnvironment CreateSharedPCHCompileEnvironment(UEBuildTarget Target, CppCompileEnvironment BaseCompileEnvironment)
		{
			CppCompileEnvironment CompileEnvironment = new CppCompileEnvironment(BaseCompileEnvironment);

			// Use the default optimization setting for
			CompileEnvironment.bOptimizeCode = ShouldEnableOptimization(ModuleRules.CodeOptimization.Default, Target.Configuration, Rules.bTreatAsEngineModule, Rules.bCodeCoverage);
			CompileEnvironment.bCodeCoverage = Rules.bCodeCoverage;
			CompileEnvironment.bTreatAsEngineModule = Rules.bTreatAsEngineModule;
			CompileEnvironment.bValidateFormatStrings = Rules.bValidateFormatStrings;
			CompileEnvironment.bValidateInternalApi = Rules.bValidateInternalApi;

			// Override compile environment
			CompileEnvironment.bIsBuildingDLL = !Target.ShouldCompileMonolithic();
			CompileEnvironment.bIsBuildingLibrary = false;

			// Add a macro for when we're compiling an engine module, to enable additional compiler diagnostics through code.
			CompileEnvironment.Definitions.Add($"UE_IS_ENGINE_MODULE={(Rules.bTreatAsEngineModule ? "1" : "0")}");

			if (!Rules.bTreatAsEngineModule)
			{
				CompileEnvironment.Definitions.Add("DEPRECATED_FORGAME=DEPRECATED");
				CompileEnvironment.Definitions.Add("UE_DEPRECATED_FORGAME=UE_DEPRECATED");
			}

			CompileEnvironment.Definitions.Add($"UE_VALIDATE_FORMAT_STRINGS={(Rules.bValidateFormatStrings ? "1" : "0")}");
			CompileEnvironment.Definitions.Add($"UE_VALIDATE_INTERNAL_API={(Rules.bValidateInternalApi ? "1" : "0")}");

			CompileEnvironment.Definitions.AddRange(EngineIncludeOrderHelper.GetDeprecationDefines(Rules.IncludeOrderVersion));

			// Add the module's private definitions.
			CompileEnvironment.Definitions.AddRange(PublicDefinitions);

			// Find all the modules that are part of the public compile environment for this module.
			Dictionary<UEBuildModule, bool> ModuleToIncludePathsOnlyFlag = new Dictionary<UEBuildModule, bool>();
			FindModulesInPublicCompileEnvironment(ModuleToIncludePathsOnlyFlag);

			// Now set up the compile environment for the modules in the original order that we encountered them
			foreach (UEBuildModule Module in ModuleToIncludePathsOnlyFlag.Keys)
			{
				Module.AddModuleToCompileEnvironment(this, null, CompileEnvironment.UserIncludePaths, CompileEnvironment.SystemIncludePaths, CompileEnvironment.ModuleInterfacePaths, CompileEnvironment.Definitions, CompileEnvironment.AdditionalFrameworks, CompileEnvironment.AdditionalPrerequisites, Rules.bLegacyPublicIncludePaths, Rules.bLegacyParentIncludePaths);
			}
			return CompileEnvironment;
		}

		/// <summary>
		/// Finds all the source files that should be built for this module
		/// </summary>
		/// <param name="Platform">The platform the module is being built for</param>
		/// <param name="DirectoryToSourceFiles">Map of directory to source files inside it</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Set of source files that should be built</returns>
		public InputFileCollection FindInputFiles(UnrealTargetPlatform Platform, Dictionary<DirectoryItem, FileItem[]> DirectoryToSourceFiles, ILogger Logger)
		{
			IReadOnlySet<string> ExcludedNames = UEBuildPlatform.GetBuildPlatform(Platform).GetExcludedFolderNames();

			InputFileCollection InputFiles = new InputFileCollection();

			SourceDirectories = new HashSet<DirectoryReference>();
			foreach (DirectoryReference Dir in ModuleDirectories)
			{
				DirectoryItem ModuleDirectoryItem = DirectoryItem.GetItemByDirectoryReference(Dir);
				FindInputFilesFromDirectoryRecursive(ModuleDirectoryItem, ExcludedNames, SourceDirectories, DirectoryToSourceFiles, InputFiles, Logger);
			}

			return InputFiles;
		}

		/// <summary>
		/// Finds all the source files that should be built for this module
		/// </summary>
		/// <param name="BaseDirectory">Directory to search from</param>
		/// <param name="ExcludedNames">Set of excluded directory names (eg. other platforms)</param>
		/// <param name="SourceDirectories">Set of all non-empty source directories.</param>
		/// <param name="DirectoryToSourceFiles">Map from directory to source files inside it</param>
		/// <param name="InputFiles">Collection of source files, categorized by type</param>
		/// <param name="Logger">Logger for output</param>
		static void FindInputFilesFromDirectoryRecursive(DirectoryItem BaseDirectory, IReadOnlySet<string> ExcludedNames, HashSet<DirectoryReference> SourceDirectories, Dictionary<DirectoryItem, FileItem[]> DirectoryToSourceFiles, InputFileCollection InputFiles, ILogger Logger)
		{
			bool bIgnoreFileFound;
			FileItem[] SourceFiles = FindInputFilesFromDirectory(BaseDirectory, InputFiles, out bIgnoreFileFound, Logger);

			if (bIgnoreFileFound)
			{
				return;
			}

			foreach (DirectoryItem SubDirectory in BaseDirectory.EnumerateDirectories())
			{
				if (!ExcludedNames.Contains(SubDirectory.Name))
				{
					FindInputFilesFromDirectoryRecursive(SubDirectory, ExcludedNames, SourceDirectories, DirectoryToSourceFiles, InputFiles, Logger);
				}
			}

			if (SourceFiles.Length > 0)
			{
				SourceDirectories.Add(BaseDirectory.Location);
			}
			DirectoryToSourceFiles.Add(BaseDirectory, SourceFiles);
		}

		// List of known extensions that are checked in FindInputFilesFromDirectory, so we can log out only if a hidden file
		// is skipped, only if it has an extension that would normally be compiled
		static string[] KnownInputFileExtensions =
		{
			".h", ".ipsh",
			".cpp", ".ixx", ".c", ".cc",
			".m", ".mm", ".swift",
			".rc", ".ispc",
		};

		/// <summary>
		/// Finds the input files that should be built for this module, from a given directory
		/// </summary>
		/// <param name="BaseDirectory"></param>
		/// <param name="InputFiles"></param>
		/// <param name="bIgnoreFileFound"></param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Array of source files</returns>
		static FileItem[] FindInputFilesFromDirectory(DirectoryItem BaseDirectory, InputFileCollection InputFiles, out bool bIgnoreFileFound, ILogger Logger)
		{
			bIgnoreFileFound = false;
			List<FileItem> SourceFiles = new List<FileItem>();
			foreach (FileItem InputFile in BaseDirectory.EnumerateFiles())
			{
				if (InputFile.Name == ".ubtignore")
				{
					bIgnoreFileFound = true;
				}
				// some text editors will leave temp files that start with . next to a, say, .cpp file, and we do not want to compile them,
				// but skip files like .DS_Store that don't have an extension (so, no more .'s after the first)
				else if (InputFile.Name.StartsWith(".") && InputFile.Name.IndexOf('.', 1) != -1)
				{
					if (KnownInputFileExtensions.Contains(InputFile.Location.GetExtension()))
					{
						Logger.LogInformation("Hidden file '{HiddenFile}' found, skipping.", InputFile.FullName);
					}
				}
				else if (InputFile.HasExtension(".h"))
				{
					InputFiles.HeaderFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".isph"))
				{
					InputFiles.ISPCHeaderFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".cpp"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.CPPFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".ixx"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.IXXFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".c"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.CFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".cc"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.CCFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".m"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.MFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".mm"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.MMFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".swift"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.SwiftFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".rc"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.RCFiles.Add(InputFile);
				}
				else if (InputFile.HasExtension(".ispc"))
				{
					SourceFiles.Add(InputFile);
					InputFiles.ISPCFiles.Add(InputFile);
				}
				// if you add any extension checks here, make sure to update KnownInputFileExtensions
			}
			return SourceFiles.ToArray();
		}

		/// <summary>
		/// Gets a set of source files for the given directory. Used to detect when the makefile is out of date.
		/// </summary>
		/// <param name="Directory"></param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Array of source files</returns>
		public static FileItem[] GetSourceFiles(DirectoryItem Directory, ILogger Logger)
		{
			bool bIgnoreFileFound;
			FileItem[] Files = FindInputFilesFromDirectory(Directory, new InputFileCollection(), out bIgnoreFileFound, Logger);
			if (bIgnoreFileFound)
			{
				return Array.Empty<FileItem>();
			}
			return Files;
		}

		/// <summary>
		/// Gets a set of source files and headers for the given directory. Used to detect when the makefile is out of date due to a new directory
		/// </summary>
		/// <param name="Directory"></param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Array of source files and headers</returns>
		public static FileItem[] GetSourceFilesAndHeaders(DirectoryItem Directory, ILogger Logger)
		{
			bool bIgnoreFileFound;
			InputFileCollection InputFiles = new InputFileCollection();
			List<FileItem> Files = new List<FileItem>(FindInputFilesFromDirectory(Directory, InputFiles, out bIgnoreFileFound, Logger));
			if (bIgnoreFileFound)
			{
				return Array.Empty<FileItem>();
			}
			Files.AddRange(InputFiles.HeaderFiles);
			return Files.ToArray();
		}

		/// <summary>
		/// Checks a given directory path whether it exists and if it contains any Verse source files
		/// </summary>
		public static bool IsValidVerseDirectory(DirectoryReference MaybeVerseDirectory)
		{
			if (!DirectoryReference.Exists(MaybeVerseDirectory))
			{
				return false;
			}

			foreach (string FilePath in Directory.EnumerateFiles(MaybeVerseDirectory.FullName, "*.v*", SearchOption.AllDirectories))
			{
				if (FilePath.EndsWith(".verse") || FilePath.EndsWith(".vmodule"))
				{
					return true;
				}
			}

			return false;
		}
	}
}
