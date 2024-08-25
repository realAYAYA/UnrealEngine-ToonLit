// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using OpenTracing.Util;
using UnrealBuildBase;
using UnrealBuildTool.Artifacts;

namespace UnrealBuildTool
{
	/// <summary>
	/// Options controlling how a target is built
	/// </summary>
	[Flags]
	enum BuildOptions
	{
		/// <summary>
		/// Default options
		/// </summary>
		None = 0,

		/// <summary>
		/// Don't build anything, just do target setup and terminate
		/// </summary>
		SkipBuild = 1,

		/// <summary>
		/// Just output a list of XGE actions; don't build anything
		/// </summary>
		XGEExport = 2,

		/// <summary>
		/// Fail if any engine files would be modified by the build
		/// </summary>
		NoEngineChanges = 4,
	}

	/// <summary>
	/// Builds a target
	/// </summary>
	[ToolMode("Build", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance | ToolModeOptions.StartPrefetchingEngine | ToolModeOptions.ShowExecutionTime | ToolModeOptions.UseStartupTraceListener)]
	class BuildMode : ToolMode
	{
		/// <summary>
		/// Specifies the file to use for logging.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string? BaseLogFileName;

		/// <summary>
		/// Whether to skip checking for files identified by the junk manifest.
		/// </summary>
		[XmlConfigFile]
		[CommandLine("-IgnoreJunk")]
		public bool bIgnoreJunk = false;

		/// <summary>
		/// Skip building; just do setup and terminate.
		/// </summary>
		[CommandLine("-SkipBuild")]
		public bool bSkipBuild = false;

		/// <summary>
		/// Skip pre build targets; just do the main target.
		/// </summary>
		[CommandLine("-SkipPreBuildTargets")]
		public bool bSkipPreBuildTargets = false;

		/// <summary>
		/// Whether we should just export the XGE XML and pretend it succeeded
		/// </summary>
		[CommandLine("-XGEExport")]
		public bool bXGEExport = false;

		/// <summary>
		/// Do not allow any engine files to be output (used by compile on startup functionality)
		/// </summary>
		[CommandLine("-NoEngineChanges")]
		public bool bNoEngineChanges = false;

		/// <summary>
		/// Whether we should just export the outdated actions list
		/// </summary>
		[CommandLine("-WriteOutdatedActions=")]
		public FileReference? WriteOutdatedActionsFile = null;

		/// <summary>
		/// An optional directory to copy crash dump files into
		/// </summary>
		[CommandLine("-SaveCrashDumps=")]
		public DirectoryReference? SaveCrashDumpDirectory = null;

		/// <summary>
		/// Main entry point
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <returns>One of the values of ECompilationResult</returns>
		/// <param name="Logger"></param>
		public override async Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			// Write the command line
			Logger.LogDebug("Command line: {EnvironmentCommandLine}", Environment.CommandLine);

			// Grab the environment.
			UnrealBuildTool.InitialEnvironment = Environment.GetEnvironmentVariables();
			if (UnrealBuildTool.InitialEnvironment.Count < 1)
			{
				throw new BuildException("Environment could not be read");
			}

			// Read the XML configuration files
			XmlConfig.ApplyTo(this);

			// Apply to architecture configs that need to read commandline arguments and didn't have the Arguments passed in during construction
			foreach (UnrealArchitectureConfig Config in UnrealArchitectureConfig.AllConfigs())
			{
				Arguments.ApplyTo(Config);
			}

			// Fixup the log path if it wasn't overridden by a config file
			if (BaseLogFileName == null)
			{
				BaseLogFileName = FileReference.Combine(Unreal.EngineProgramSavedDirectory, "UnrealBuildTool", "Log.txt").FullName;
			}

			// Create the log file, and flush the startup listener to it
			if (!Arguments.HasOption("-NoLog") && !Log.HasFileWriter())
			{
				Log.AddFileWriter("DefaultLogTraceListener", FileReference.FromString(BaseLogFileName));
			}
			else
			{
				IEnumerable<StartupTraceListener> StartupListeners = Trace.Listeners.OfType<StartupTraceListener>();
				if (StartupListeners.Any())
				{
					Trace.Listeners.Remove(StartupListeners.First());
				}
			}

			// Create the build configuration object, and read the settings
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			// Check the root path length isn't too long
			if (BuildHostPlatform.Current.Platform == UnrealTargetPlatform.Win64 && Unreal.RootDirectory.FullName.Length > BuildConfiguration.MaxRootPathLength)
			{
				Logger.LogWarning("Running from a path with a long directory name (\"{Path}\" = {NumChars} characters). Root paths shorter than {MaxChars} characters are recommended to avoid exceeding maximum path lengths on Windows.", Unreal.RootDirectory, Unreal.RootDirectory.FullName.Length, BuildConfiguration.MaxRootPathLength);
			}

			// now that we know the available platforms, we can delete other platforms' junk. if we're only building specific modules from the editor, don't touch anything else (it may be in use).
			if (!bIgnoreJunk && !Unreal.IsEngineInstalled())
			{
				using (GlobalTracer.Instance.BuildSpan("DeleteJunk()").StartActive())
				{
					JunkDeleter.DeleteJunk(Logger);
				}
			}

			// Parse and build the targets
			try
			{
				List<TargetDescriptor> TargetDescriptors;

				// Parse all the target descriptors
				using (GlobalTracer.Instance.BuildSpan("TargetDescriptor.ParseCommandLine()").StartActive())
				{
					TargetDescriptors = TargetDescriptor.ParseCommandLine(Arguments, BuildConfiguration, Logger);

					// Handle BuildConfiguration arguments nested inside -Target= args
					TargetDescriptors.ForEach(x => x.AdditionalArguments.ApplyTo(BuildConfiguration));
				}

				// Hack for specific files compile; don't build the ShaderCompileWorker target that's added to the command line for generated project files
				if (TargetDescriptors.Count > 1)
				{
					TargetDescriptors.RemoveAll(x => (x.Name == "ShaderCompileWorker" || x.Name == "LiveCodingConsole" || x.Name == "InterchangeWorker") && x.SpecificFilesToCompile.Count > 0);
				}

				// Clean any target that wanted to be cleaned before being rebuilt
				if (TargetDescriptors.Any(D => D.bRebuild))
				{
					CleanMode CleanMode = new CleanMode();
					CleanMode.bSkipPreBuildTargets = bSkipPreBuildTargets;
					CleanMode.Clean(TargetDescriptors.Where(D => D.bRebuild).ToList(), BuildConfiguration, Logger);
				}

				// Handle remote builds
				for (int Idx = 0; Idx < TargetDescriptors.Count; ++Idx)
				{
					TargetDescriptor TargetDesc = TargetDescriptors[Idx];
					if (RemoteMac.HandlesTargetPlatform(TargetDesc.Platform))
					{
						FileReference BaseLogFile = Log.OutputFile ?? new FileReference(BaseLogFileName);
						FileReference RemoteLogFile = FileReference.Combine(BaseLogFile.Directory, BaseLogFile.GetFileNameWithoutExtension() + "_Remote.txt");

						RemoteMac RemoteMac = new RemoteMac(TargetDesc.ProjectFile, Logger);
						if (!RemoteMac.Build(TargetDesc, RemoteLogFile, bSkipPreBuildTargets, Logger))
						{
							return (int)CompilationResult.Unknown;
						}

						TargetDescriptors.RemoveAt(Idx--);
					}
				}

				// Handle local builds
				if (TargetDescriptors.Count > 0)
				{
					// Get a set of all the project directories
					HashSet<DirectoryReference> ProjectDirs = new HashSet<DirectoryReference>();
					foreach (TargetDescriptor TargetDesc in TargetDescriptors)
					{
						if (TargetDesc.ProjectFile != null)
						{
							DirectoryReference ProjectDirectory = TargetDesc.ProjectFile.Directory;
							FileMetadataPrefetch.QueueProjectDirectory(ProjectDirectory);
							ProjectDirs.Add(ProjectDirectory);
						}

						// print out SDK info for only the platforms that are being compiled
						UEBuildPlatformSDK.GetSDKForPlatform(TargetDesc.Platform.ToString())?.PrintSDKInfoAndReturnValidity();
					}

					// Get all the build options
					BuildOptions Options = BuildOptions.None;
					if (bSkipBuild)
					{
						Options |= BuildOptions.SkipBuild;
					}
					if (bXGEExport)
					{
						Options |= BuildOptions.XGEExport;
					}
					if (bNoEngineChanges)
					{
						Options |= BuildOptions.NoEngineChanges;
					}

					// Create the working set provider per group.
					using (ISourceFileWorkingSet WorkingSet = SourceFileWorkingSet.Create(Unreal.RootDirectory, ProjectDirs, Logger))
					{
						await BuildAsync(TargetDescriptors, BuildConfiguration, WorkingSet, Options, WriteOutdatedActionsFile, Logger, bSkipPreBuildTargets);
					}
				}
			}
			finally
			{
				// Check if anything failed during our run, and act accordingly.
				ProcessCoreDumps(SaveCrashDumpDirectory, Logger);

				// Save all the caches
				SourceFileMetadataCache.SaveAll();
				CppDependencyCache.SaveAll();
			}
			return 0;
		}

		/// <summary>
		/// Creates scripts for executing the init scripts (only supported on project descriptors)
		/// </summary>
		/// <param name="TargetDescriptor">The current target</param>
		/// <returns>List of created script files</returns>
		public static FileReference[] CreateInitScripts(TargetDescriptor TargetDescriptor)
		{
			if (TargetDescriptor.ProjectFile == null)
			{
				return new FileReference[] { };
			}

			ProjectDescriptor ProjectDescriptor = ProjectDescriptor.FromFile(TargetDescriptor.ProjectFile);
			List<string[]> InitCommandBatches = new List<string[]>();

			if (ProjectDescriptor != null && ProjectDescriptor.InitSteps != null)
			{
				if (ProjectDescriptor.InitSteps.TryGetCommands(BuildHostPlatform.Current.Platform, out string[]? Commands))
				{
					InitCommandBatches.Add(Commands);
				}
			}

			if (InitCommandBatches.Count == 0)
			{
				return new FileReference[] { };
			}

			DirectoryReference ProjectDirectory = DirectoryReference.FromFile(TargetDescriptor.ProjectFile);
			string PlatformIntermediateFolder = UEBuildTarget.GetPlatformIntermediateFolder(TargetDescriptor.Platform, TargetDescriptor.Architectures, false);

			DirectoryReference ProjectIntermediateDirectory = DirectoryReference.Combine(ProjectDirectory, PlatformIntermediateFolder, TargetDescriptor.Name, TargetDescriptor.Configuration.ToString());

			return WriteInitScripts(TargetDescriptor, BuildHostPlatform.Current.Platform, ProjectIntermediateDirectory, "Init", InitCommandBatches);
		}

		/// <summary>
		/// Write scripts containing the custom build steps for the given host platform
		/// </summary>
		/// <param name="TargetDescriptor">The current target</param>
		/// <param name="HostPlatform">The current host platform</param>
		/// <param name="Directory">The output directory for the scripts</param>
		/// <param name="FilePrefix">Bare prefix for all the created script files</param>
		/// <param name="CommandBatches">List of custom build steps</param>
		/// <returns>List of created script files</returns>
		private static FileReference[] WriteInitScripts(TargetDescriptor TargetDescriptor, UnrealTargetPlatform HostPlatform, DirectoryReference Directory, string FilePrefix, List<string[]> CommandBatches)
		{
			List<FileReference> ScriptFiles = new List<FileReference>();
			foreach (string[] CommandBatch in CommandBatches)
			{
				// Find all the standard variables
				Dictionary<string, string> Variables = GetTargetVariables(TargetDescriptor);

				// Get the output path to the script
				string ScriptExtension = RuntimePlatform.IsWindows ? ".bat" : ".sh";
				FileReference ScriptFile = FileReference.Combine(Directory, String.Format("{0}-{1}{2}", FilePrefix, ScriptFiles.Count + 1, ScriptExtension));

				// Write it to disk
				List<string> Contents = new List<string>();
				if (RuntimePlatform.IsWindows)
				{
					Contents.Insert(0, "@echo off");
				}
				foreach (string Command in CommandBatch)
				{
					Contents.Add(Utils.ExpandVariables(Command, Variables));
				}
				if (!DirectoryReference.Exists(ScriptFile.Directory))
				{
					DirectoryReference.CreateDirectory(ScriptFile.Directory);
				}
				FileReference.WriteAllLines(ScriptFile, Contents);

				// Add the output file to the list of generated scripts
				ScriptFiles.Add(ScriptFile);
			}
			return ScriptFiles.ToArray();
		}

		/// <summary>
		/// Gets a list of variables that can be expanded in paths referenced by this target
		/// </summary>
		/// <param name="TargetDescriptor">The current target</param>
		/// <returns>Map of variable names to values</returns>
		private static Dictionary<string, string> GetTargetVariables(TargetDescriptor TargetDescriptor)
		{
			Dictionary<string, string> Variables = new Dictionary<string, string>();
			Variables.Add("RootDir", Unreal.RootDirectory.FullName);
			Variables.Add("EngineDir", Unreal.EngineDirectory.FullName);
			Variables.Add("TargetName", TargetDescriptor.Name);
			Variables.Add("TargetPlatform", TargetDescriptor.Platform.ToString());
			Variables.Add("TargetConfiguration", TargetDescriptor.Configuration.ToString());
			if (TargetDescriptor.ProjectFile != null)
			{
				Variables.Add("ProjectDir", TargetDescriptor.ProjectFile.Directory.FullName);
				Variables.Add("ProjectFile", TargetDescriptor.ProjectFile.FullName);
			}
			if (BuildVersion.TryRead(BuildVersion.GetDefaultFileName(), out BuildVersion? Version))
			{
				Variables.Add("EngineVersion", String.Format("{0}.{1}.{2}", Version.MajorVersion, Version.MinorVersion, Version.PatchVersion));
			}
			return Variables;
		}

		/// <summary>
		/// Build a list of targets
		/// </summary>
		/// <param name="TargetDescriptors">Target descriptors</param>
		/// <param name="BuildConfiguration">Current build configuration</param>
		/// <param name="WorkingSet">The source file working set</param>
		/// <param name="Options">Additional options for the build</param>
		/// <param name="WriteOutdatedActionsFile">Files to write the list of outdated actions to (rather than building them)</param>
		/// <param name="Logger">Logger for output</param>
		/// <param name="bSkipPreBuildTargets">If true then only the current target descriptors will be built.</param>
		/// <returns>Result from the compilation</returns>
		public static async Task BuildAsync(List<TargetDescriptor> TargetDescriptors, BuildConfiguration BuildConfiguration, ISourceFileWorkingSet WorkingSet, BuildOptions Options, FileReference? WriteOutdatedActionsFile, ILogger Logger, bool bSkipPreBuildTargets = false)
		{
			List<TargetMakefile> TargetMakefiles = new List<TargetMakefile>();

			for (int Idx = 0; Idx < TargetDescriptors.Count; ++Idx)
			{
				// Create and execute the init scripts
				FileReference[] InitScripts = CreateInitScripts(TargetDescriptors[Idx]);
				if (InitScripts.Length > 0)
				{
					Utils.ExecuteCustomBuildSteps(InitScripts, Logger);
				}

				TargetMakefile NewMakefile = await CreateMakefileAsync(BuildConfiguration, TargetDescriptors[Idx], WorkingSet, Logger);
				TargetMakefiles.Add(NewMakefile);
				if (!bSkipPreBuildTargets)
				{
					foreach (TargetInfo PreBuildTarget in NewMakefile.PreBuildTargets)
					{
						TargetDescriptor NewTarget = TargetDescriptor.FromTargetInfo(PreBuildTarget);
						if (!TargetDescriptors.Contains(NewTarget))
						{
							TargetDescriptors.Add(NewTarget);
						}
					}
				}
			}

			await BuildAsync(TargetMakefiles.ToArray(), TargetDescriptors, BuildConfiguration, Options, WriteOutdatedActionsFile, Logger);
		}

		/// <summary>
		/// Build a list of targets with a given set of makefiles.
		/// </summary>
		/// <param name="Makefiles">Makefiles created with CreateMakefiles</param>
		/// <param name="TargetDescriptors">Target descriptors</param>
		/// <param name="BuildConfiguration">Current build configuration</param>
		/// <param name="Options">Additional options for the build</param>
		/// <param name="WriteOutdatedActionsFile">Files to write the list of outdated actions to (rather than building them)</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Result from the compilation</returns>
		internal static async Task BuildAsync(TargetMakefile[] Makefiles, List<TargetDescriptor> TargetDescriptors, BuildConfiguration BuildConfiguration, BuildOptions Options, FileReference? WriteOutdatedActionsFile, ILogger Logger)
		{
			// Execute the build
			if ((Options & BuildOptions.SkipBuild) == 0)
			{
				// Make sure that none of the actions conflict with any other (producing output files differently, etc...)
				using (GlobalTracer.Instance.BuildSpan("ActionGraph.CheckForConflicts").StartActive())
				{
					// TODO: Skipping conflicts for WriteMetadata is a hack to maintain parity with the BuildGraph executor which allowed
					// exporting mulitple conflicing WriteMetadata actions.
					// This is technically buggy behavior and should be properly fixed in UEBuildTarget.cs
					IEnumerable<IExternalAction> CheckActions = Makefiles.SelectMany(x => x.Actions).Where(x => !x.IgnoreConflicts());
					ActionGraph.CheckForConflicts(CheckActions, Logger);
				}

				// Check we don't exceed the nominal max path length
				using (GlobalTracer.Instance.BuildSpan("ActionGraph.CheckPathLengths").StartActive())
				{
					ActionGraph.CheckPathLengths(BuildConfiguration, Makefiles.SelectMany(x => x.Actions), Logger);
				}

				HashSet<FileItem> MergedOutputItems = new HashSet<FileItem>();

				// Create a QueuedAction instance for each action in the makefiles
				List<LinkedAction>[] QueuedActions = new List<LinkedAction>[Makefiles.Length];
				for (int Idx = 0; Idx < Makefiles.Length; Idx++)
				{
					TargetDescriptor TargetDescriptor = TargetDescriptors[Idx];
					TargetMakefile Makefile = Makefiles[Idx];

					QueuedActions[Idx] = Makefile.Actions.ConvertAll(x => new LinkedAction(x, TargetDescriptors[Idx]));

					if (TargetDescriptor.SpecificFilesToCompile.Count != 0)
					{
						// Filter out SpecificFilesToCompile so only source files and headers included in the target are considered
						HashSet<FileReference> TargetFiles = Makefile.SourceAndHeaderFiles.Select(x => x.Location).ToHashSet();
						List<FileReference> FilesToBuild = TargetDescriptor.SpecificFilesToCompile
							.Where(x => TargetFiles.Contains(x))
							.ToList();

						// If we have specific files to build we need to put those as MergedOutputItems (and create special single file actions if needed)
						if (FilesToBuild.Count != 0)
						{
							// If there are headers in the list, expand the FilesToBuild to also include all files that include those headers
							if (TargetDescriptor.bSingleFileBuildDependents)
							{
								FilesToBuild = GetAllSourceFilesIncludingHeader(FilesToBuild, TargetDescriptor.ProjectFile, Makefile.Actions, Logger);
							}

							// We have specific files to compile so we will only queue up those files.
							List<FileItem> ProducedItems = CreateLinkedActionsFromFileList(TargetDescriptor, BuildConfiguration, FilesToBuild, QueuedActions[Idx], Logger);
							MergedOutputItems.UnionWith(ProducedItems);
						}
					}
				}

				// Clean up any previous hot reload runs, and reapply the current state if it's already active
				Dictionary<FileReference, FileReference>? InitialPatchedOldLocationToNewLocation = null;
				for (int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
				{
					InitialPatchedOldLocationToNewLocation = HotReload.Setup(TargetDescriptors[TargetIdx], Makefiles[TargetIdx], QueuedActions[TargetIdx], BuildConfiguration, Logger);
				}

				// Merge the action graphs together
				List<LinkedAction> MergedActions;
				if (TargetDescriptors.Count == 1)
				{
					MergedActions = QueuedActions[0];
				}
				else
				{
					MergedActions = MergeActionGraphs(TargetDescriptors, QueuedActions);
				}

				// Gather all the prerequisite actions that are part of the targets
				for (int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
				{
					GatherOutputItems(TargetDescriptors[TargetIdx], Makefiles[TargetIdx], MergedOutputItems);
				}

				// Link all the actions together (No need sorting, we'll do that later again)
				ActionGraph.Link(MergedActions, Logger, Sort: false);

				// Get all the actions that are prerequisites for these targets. This forms the list of actions that we want executed.
				List<LinkedAction> PrerequisiteActions = ActionGraph.GatherPrerequisiteActions(MergedActions, MergedOutputItems);

				// Create the action history
				ActionHistory History = new ActionHistory();
				for (int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
				{
					using (GlobalTracer.Instance.BuildSpan("Reading action history").StartActive())
					{
						TargetDescriptor TargetDescriptor = TargetDescriptors[TargetIdx];
						if (TargetDescriptor.ProjectFile != null)
						{
							History.Mount(TargetDescriptor.ProjectFile.Directory);
						}
					}
				}

				// Create the C++ dependency cache
				CppDependencyCache CppDependencies = new CppDependencyCache();
				for (int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
				{
					using (GlobalTracer.Instance.BuildSpan("Reading dependency cache").StartActive())
					{
						TargetDescriptor TargetDescriptor = TargetDescriptors[TargetIdx];
						CppDependencies.Mount(TargetDescriptor, Makefiles[TargetIdx].TargetType, Logger);
					}
				}

				// Now that we have the dependencies object, optionally create the artifact cache
				IActionArtifactCache? actionArtifactCache = null;
				if (!String.IsNullOrEmpty(BuildConfiguration.ArtifactDirectory) && (BuildConfiguration.bArtifactRead || BuildConfiguration.bArtifactWrites))
				{
					DirectoryReference artifactDirectory = new(BuildConfiguration.ArtifactDirectory);
					DirectoryReference.CreateDirectory(artifactDirectory);

					string mode = (BuildConfiguration.bArtifactRead, BuildConfiguration.bArtifactWrites) switch
					{
						(false, true) => "write only",
						(true, false) => "read only",
						_ => "read/write",
					};

					Logger.LogInformation("Experimental artifact system using '{directory}' in {mode} mode", artifactDirectory.FullName, mode);

					actionArtifactCache = ActionArtifactCache.CreateHordeFileCache(artifactDirectory, CppDependencies, Logger);
					actionArtifactCache.EnableReads = BuildConfiguration.bArtifactRead;
					actionArtifactCache.EnableWrites = BuildConfiguration.bArtifactWrites;
					actionArtifactCache.LogCacheMisses = BuildConfiguration.bLogArtifactCacheMisses;
					actionArtifactCache.EngineRoot = Unreal.EngineDirectory;
				}

				// Pre-process module interfaces to generate dependency files
				List<LinkedAction> ModuleDependencyActions = PrerequisiteActions.Where(x => x.ActionType == ActionType.GatherModuleDependencies).ToList();
				if (ModuleDependencyActions.Count > 0)
				{
					ConcurrentDictionary<LinkedAction, bool> PreprocessActionToOutdatedFlag = new ConcurrentDictionary<LinkedAction, bool>();
					ActionGraph.GatherAllOutdatedActions(ModuleDependencyActions, History, PreprocessActionToOutdatedFlag, CppDependencies, BuildConfiguration.bIgnoreOutdatedImportLibraries, Logger);

					List<LinkedAction> PreprocessActions = PreprocessActionToOutdatedFlag.Where(x => x.Value).Select(x => x.Key).ToList();
					if (PreprocessActions.Count > 0)
					{
						Logger.LogInformation("Updating module dependencies...");
						await ActionGraph.ExecuteActionsAsync(BuildConfiguration, PreprocessActions, TargetDescriptors, Logger);

						foreach (FileItem ProducedItem in PreprocessActions.SelectMany(x => x.ProducedItems))
						{
							ProducedItem.ResetCachedInfo();
						}
					}
				}

				// Figure out which actions need to be built
				ConcurrentDictionary<LinkedAction, bool> ActionToOutdatedFlag = new ConcurrentDictionary<LinkedAction, bool>();
				for (int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
				{
					TargetDescriptor TargetDescriptor = TargetDescriptors[TargetIdx];

					// Update the module dependencies
					Dictionary<string, FileItem> ModuleOutputs = new Dictionary<string, FileItem>(StringComparer.Ordinal);
					if (ModuleDependencyActions.Count > 0)
					{
						Dictionary<FileItem, List<string>> ModuleImports = new Dictionary<FileItem, List<string>>();

						List<FileItem> CompiledModuleInterfaces = new List<FileItem>();
						foreach (LinkedAction ModuleDependencyAction in ModuleDependencyActions)
						{
							ICppCompileAction? CppModulesAction = ModuleDependencyAction.Inner as ICppCompileAction;
							if (CppModulesAction != null && CppModulesAction.CompiledModuleInterfaceFile != null)
							{
								string? ProducedModule;
								if (CppDependencies.TryGetProducedModule(ModuleDependencyAction.DependencyListFile!, Logger, out ProducedModule))
								{
									ModuleOutputs[ProducedModule] = CppModulesAction.CompiledModuleInterfaceFile;
								}

								List<(string Name, string BMI)>? ImportedModules;
								if (CppDependencies.TryGetImportedModules(ModuleDependencyAction.DependencyListFile!, Logger, out ImportedModules))
								{
									ModuleImports[CppModulesAction.CompiledModuleInterfaceFile] = ImportedModules.Select(x => x.Name).ToList();
								}

								CompiledModuleInterfaces.Add(CppModulesAction.CompiledModuleInterfaceFile);
							}
						}

						Dictionary<FileItem, string> ModuleInputs = ModuleOutputs.ToDictionary(x => x.Value, x => x.Key);
						foreach (LinkedAction PrerequisiteAction in PrerequisiteActions)
						{
							if (PrerequisiteAction.ActionType == ActionType.CompileModuleInterface)
							{
								ICppCompileAction CppModulesAction = (ICppCompileAction)PrerequisiteAction.Inner;

								List<string>? ImportedModules;
								if (ModuleImports.TryGetValue(CppModulesAction.CompiledModuleInterfaceFile!, out ImportedModules))
								{
									foreach (string ImportedModule in ImportedModules)
									{
										FileItem? ModuleOutput;
										if (ModuleOutputs.TryGetValue(ImportedModule, out ModuleOutput))
										{
											Action NewAction = new Action(PrerequisiteAction.Inner);
											NewAction.PrerequisiteItems.Add(ModuleOutput);
											NewAction.CommandArguments += String.Format(" /reference \"{0}={1}\"", ImportedModule, ModuleOutput.FullName);
											PrerequisiteAction.Inner = NewAction;
										}
										else
										{
											throw new BuildException("Unable to find interface for module '{0}'", ImportedModule);
										}
									}
								}
							}
							else if (PrerequisiteAction.ActionType == ActionType.Compile)
							{
								foreach (FileItem PrerequisiteItem in PrerequisiteAction.PrerequisiteItems)
								{
									string? ModuleName;
									if (ModuleInputs.TryGetValue(PrerequisiteItem, out ModuleName))
									{
										Action NewAction = new Action(PrerequisiteAction.Inner);
										NewAction.CommandArguments += String.Format(" /reference \"{0}={1}\"", ModuleName, PrerequisiteItem.AbsolutePath);
										PrerequisiteAction.Inner = NewAction;
									}
								}
							}
							else
							{
								if (PrerequisiteAction.ActionType != ActionType.GatherModuleDependencies)
								{
									Action NewAction = new Action(PrerequisiteAction.Inner);
									NewAction.PrerequisiteItems.UnionWith(CompiledModuleInterfaces);
									PrerequisiteAction.Inner = NewAction;
								}
							}
						}
					}

					// Plan the actions to execute for the build. For single file compiles, always rebuild the source file regardless of whether it's out of date.
					ActionGraph.GatherAllOutdatedActions(PrerequisiteActions, History, ActionToOutdatedFlag, CppDependencies, BuildConfiguration.bIgnoreOutdatedImportLibraries, Logger);
				}

				// Link the action graph again to sort it
				List<LinkedAction> MergedActionsToExecute = ActionToOutdatedFlag.Where(x => x.Value).Select(x => x.Key).ToList();
				ActionGraph.Link(MergedActionsToExecute, Logger);

				// Allow hot reload to override the actions
				int HotReloadTargetIdx = -1;
				for (int Idx = 0; Idx < TargetDescriptors.Count; Idx++)
				{
					if (TargetDescriptors[Idx].HotReloadMode != HotReloadMode.Disabled)
					{
						if (HotReloadTargetIdx != -1)
						{
							throw new BuildException("Unable to perform hot reload with multiple targets.");
						}
						else
						{
							MergedActionsToExecute = HotReload.PatchActionsForTarget(BuildConfiguration, TargetDescriptors[Idx], Makefiles[Idx], PrerequisiteActions, MergedActionsToExecute, InitialPatchedOldLocationToNewLocation, Logger);
						}
						HotReloadTargetIdx = Idx;
					}
					else if (MergedActionsToExecute.Count > 0)
					{
						HotReload.CheckForLiveCodingSessionActive(TargetDescriptors[Idx], Makefiles[Idx], BuildConfiguration, Logger);
					}
				}

				if (HotReloadTargetIdx != -1)
				{
					Logger.LogDebug("Re-evaluating action graph");
					// Re-check the graph to remove any LiveCoding actions added by PatchActionsForTarget() that are already up to date.
					ConcurrentDictionary<LinkedAction, bool> LiveActionToOutdatedFlag = new ConcurrentDictionary<LinkedAction, bool>(Environment.ProcessorCount, MergedActionsToExecute.Count);
					ActionGraph.GatherAllOutdatedActions(MergedActionsToExecute, History, LiveActionToOutdatedFlag, CppDependencies, BuildConfiguration.bIgnoreOutdatedImportLibraries, Logger);
					List<LinkedAction> LiveCodingActionsToExecute = LiveActionToOutdatedFlag.Where(x => x.Value).Select(x => x.Key).ToList();
					ActionGraph.Link(LiveCodingActionsToExecute, Logger);
					MergedActionsToExecute = LiveCodingActionsToExecute;
				}

				// Make sure we're not modifying any engine files
				if ((Options & BuildOptions.NoEngineChanges) != 0)
				{
					List<FileItem> EngineChanges = MergedActionsToExecute.SelectMany(x => x.ProducedItems).Where(x => x.Location.IsUnderDirectory(Unreal.EngineDirectory)).Distinct().OrderBy(x => x.FullName).ToList();
					if (EngineChanges.Count > 0)
					{
						StringBuilder Result = new StringBuilder("Building would modify the following engine files:\n");
						foreach (FileItem EngineChange in EngineChanges)
						{
							Result.AppendFormat("\n{0}", EngineChange.FullName);
						}
						Result.Append("\n\nPlease rebuild from an IDE instead.");
						Logger.LogError("{Result}", Result.ToString());
						throw new CompilationResultException(CompilationResult.FailedDueToEngineChange);
					}
				}

				// Make sure the appropriate executor is selected
				foreach (TargetDescriptor TargetDescriptor in TargetDescriptors)
				{
					UEBuildPlatform BuildPlatform = UEBuildPlatform.GetBuildPlatform(TargetDescriptor.Platform);
					BuildConfiguration.bAllowXGE &= BuildPlatform.CanUseXGE();
					BuildConfiguration.bAllowFASTBuild &= BuildPlatform.CanUseFASTBuild();
					BuildConfiguration.bAllowSNDBS &= BuildPlatform.CanUseSNDBS();
				}

				// Delete produced items that are outdated.
				ActionGraph.DeleteOutdatedProducedItems(MergedActionsToExecute, Logger);

				// Save all the action histories now that files have been removed. We have to do this after deleting produced items to ensure that any
				// items created during the build don't have the wrong command line.
				History.Save();

				// Create directories for the outdated produced items.
				ActionGraph.CreateDirectoriesForProducedItems(MergedActionsToExecute);

				// Execute the actions
				if ((Options & BuildOptions.XGEExport) != 0)
				{
					OutputToolchainInfo(TargetDescriptors, Makefiles, Logger);

					// Just export to an XML file
					using (GlobalTracer.Instance.BuildSpan("XGE.ExportActions()").StartActive())
					{
						XGE.ExportActions(MergedActionsToExecute, Logger);
					}
				}
				else if (WriteOutdatedActionsFile != null)
				{
					OutputToolchainInfo(TargetDescriptors, Makefiles, Logger);

					// Write actions to an output file
					using (GlobalTracer.Instance.BuildSpan("ActionGraph.WriteActions").StartActive())
					{
						ActionGraph.ExportJson(MergedActionsToExecute, WriteOutdatedActionsFile);
						Logger.LogInformation("Exported build actions to {WriteOutdatedActionsFile}", WriteOutdatedActionsFile);
					}
				}
				else
				{
					// Execute the actions
					if (MergedActionsToExecute.Count == 0)
					{
						if (TargetDescriptors.Any(x => !x.bQuiet))
						{
							if (TargetDescriptors.Count == 1)
							{
								Logger.LogInformation("Target is up to date");
							}
							else
							{
								Logger.LogInformation("Targets are up to date");
							}
						}
					}
					else
					{
						if (TargetDescriptors.Any(x => !x.bQuiet))
						{
							Logger.LogInformation("Building {Targets}...", StringUtils.FormatList(TargetDescriptors.Select(x => x.Name).Distinct()));
						}

						OutputToolchainInfo(TargetDescriptors, Makefiles, Logger);

						ActionExecutor.SetMemoryPerActionOverride(Makefiles.Select(x => x.MemoryPerActionGB).Max());

						using (GlobalTracer.Instance.BuildSpan("ActionGraph.ExecuteActions()").StartActive())
						{

							// We need to wait for the cache to be ready.  If we fail to fetch from the cache for PCH files, they are
							// non-deterministic and will cause rippled cache misses.  Changing the system to assume a PCH file is the same
							// if the inputs are the same might be a good way to avoid this issue.
							if (actionArtifactCache != null && actionArtifactCache.ArtifactCache != null)
							{
								await actionArtifactCache.ArtifactCache.WaitForReadyAsync();
							}
							await ActionGraph.ExecuteActionsAsync(BuildConfiguration, MergedActionsToExecute, TargetDescriptors, Logger, actionArtifactCache);
							if (actionArtifactCache != null)
							{
								await actionArtifactCache.FlushChangesAsync(default); //ETSTODO
							}
						}

						// these are parsed by external tools wishing to open this file directly
						foreach (LinkedAction BuildAction in MergedActionsToExecute.Where(BuildAction => BuildAction.ActionType == ActionType.Compile && (BuildAction.Inner as VCCompileAction)?.bIsAnalyzing != true))
						{
							FileItem? PreprocessedFile = BuildAction.ProducedItems.FirstOrDefault(x => x.HasExtension(".i"));
							if (PreprocessedFile != null)
							{
								Logger.LogInformation("PreProcessPath: {File}", PreprocessedFile);
							}

							FileItem? AssemblyPath = BuildAction.ProducedItems.FirstOrDefault(x => x.HasExtension(".asm"));
							if (AssemblyPath != null)
							{
								Logger.LogInformation("AssemblyPath: {File}", AssemblyPath);
							}
						}
					}

					// Run the deployment steps
					foreach (TargetMakefile Makefile in Makefiles)
					{
						// Receipt file may not exist when compiling specific files
						if (Makefile.bDeployAfterCompile && FileReference.Exists(Makefile.ReceiptFile))
						{
							TargetReceipt Receipt = TargetReceipt.Read(Makefile.ReceiptFile);
							Logger.LogInformation("Deploying {ReceiptTargetName} {ReceiptPlatform} {ReceiptConfiguration}...", Receipt.TargetName, Receipt.Platform, Receipt.Configuration);

							UEBuildPlatform.GetBuildPlatform(Receipt.Platform).Deploy(Receipt);
						}
					}
				}
			}
			else
			{
				foreach (TargetMakefile Makefile in Makefiles)
				{
					FileReference TargetInfoFile = FileReference.Combine(Makefile.ProjectIntermediateDirectory, "TargetMetadata.dat");
					if (FileReference.Exists(TargetInfoFile))
					{
						List<string> ArgumentsList = new List<string>()
						{
							$"-Input={TargetInfoFile}",
							$"-Version={WriteMetadataMode.CurrentVersionNumber}"
						};
						CommandLineArguments Arguments = new CommandLineArguments(ArgumentsList.ToArray());
						WriteMetadataMode MetadataMode = new WriteMetadataMode();
						await MetadataMode.ExecuteAsync(Arguments, Logger);
					}
				}
			}
		}

		internal static List<FileItem> CreateLinkedActionsFromFileList(TargetDescriptor Target, BuildConfiguration BuildConfiguration, List<FileReference> FileList, List<LinkedAction> Actions, ILogger Logger)
		{
			// We have specific files to compile so we will only queue up those files.
			// We will also add them to the MergedOutputItems to make sure they are not skipped
			List<FileItem> ProducedItems = new();

			// First, find all the SpecificFileActions.
			// These are used to create a custom action for the source file in case it is inside a unity or normally not part of the build (headers for example)
			Dictionary<DirectoryReference, ISpecificFileAction> SpecificFileActions = new();
			foreach (ISpecificFileAction SpecificFileAction in Actions.Select(x => x.Inner).OfType<ISpecificFileAction>())
			{
				SpecificFileActions.TryAdd(SpecificFileAction.RootDirectory, SpecificFileAction);
			}

			// Now traverse all specific files and make sure we find or create actions for them.
			foreach (FileItem SourceFile in FileList.Select(x => FileItem.GetItemByFileReference(x)).Where(x => x.Exists))
			{
				ISpecificFileAction? SpecificFileAction = null;

				// Traverse upwards from file directory to try to find a SpecificFileAction.
				DirectoryItem? Dir = SourceFile.Directory;
				while (Dir != null)
				{
					if (SpecificFileActions.TryGetValue(Dir.Location, out SpecificFileAction))
					{
						break;
					}
					Dir = Dir.GetParentDirectoryItem();
				}

				// We found an action to take care of this file.
				if (SpecificFileAction != null)
				{
					IExternalAction? NewAction = SpecificFileAction.CreateAction(SourceFile, Logger);
					if (NewAction != null)
					{
						Actions.Add(new LinkedAction(NewAction, Target));
						ProducedItems.AddRange(NewAction.ProducedItems);
						continue;
					}
				}

				// There is no special action for this file.. let's look for an action that has this exact file as input and use that (for example ispc files)
				LinkedAction? Action = Actions.Find(x => x.PrerequisiteItems.Contains(SourceFile));

				if (Action != null)
				{
					ProducedItems.AddRange(Action.ProducedItems);
				}
				else if (!BuildConfiguration.bIgnoreInvalidFiles)
				{
					Logger.LogError("ERROR: Failed to find an Action that can be used to build \"{Path}\" (does target use this file?)", SourceFile);
				}
			}

			return ProducedItems;
		}

		/// <summary>
		/// If there are headers in the specific list, this function will exand the list to include all files that include those headers
		/// </summary>
		/// <param name="SpecificFilesToCompile">List of files to compile</param>
		/// <param name="ProjectFile">Project file if there is one</param>
		/// <param name="Actions">Actions to filter out expansion of files</param>
		/// <param name="Logger">Logger for output</param>
		internal static List<FileReference> GetAllSourceFilesIncludingHeader(List<FileReference> SpecificFilesToCompile, FileReference? ProjectFile, List<IExternalAction> Actions, ILogger Logger)
		{
			Func<FileReference, bool> IsCode = x => IsHeader(x) || x.HasExtension(".cpp") || x.HasExtension(".c");
			List<FileReference> SpecificHeaderFiles = SpecificFilesToCompile.Where(IsHeader).ToList();
			if (SpecificHeaderFiles.Count == 0)
			{
				return SpecificFilesToCompile;
			}

			// TODO: This code below works very similar to the code before this changelist but is not a great solution for figuring out which files to compile.
			// Suggested approach is to:
			// 1. Write out a file with additional include paths per module.
			// 2. Use the special single file actions to figure out if the specific headers are included by the source files using additional include paths
			// 3. Fix so CppIncludeLookup is including headers... Current code does not handle this: A.h <- B.h <- C.cpp... C.cpp is not added.
			// 4. Cache which files unity files include to reduce number of files needed to be read

			List<DirectoryReference> BaseDirs = new List<DirectoryReference>();
			BaseDirs.Add(Unreal.EngineDirectory);

			DirectoryReference CacheDir = Unreal.EngineDirectory;
			if (ProjectFile != null)
			{
				BaseDirs.Add(ProjectFile.Directory);
				CacheDir = ProjectFile.Directory;
			}

			FileReference IncludeCache = FileReference.Combine(CacheDir, "Intermediate", "HeaderLookup.dat");

			Logger.LogInformation("Building dependency cache for specified headers");
			foreach (FileReference HeaderFile in SpecificHeaderFiles)
			{
				Logger.LogDebug("  {HeaderFile}", HeaderFile);
			}

			CppIncludeLookup IncludeLookup = new CppIncludeLookup(IncludeCache);
			IncludeLookup.Load();
			IncludeLookup.Update(BaseDirs);
			IncludeLookup.Save();

			// Find all files that can be part of this build
			ConcurrentDictionary<FileReference, int> UsedFiles = new();
			Parallel.ForEach(Actions, (Action) =>
			{
				foreach (FileItem File in Action.PrerequisiteItems)
				{
					if (!IsCode(File.Location))
					{
						continue;
					}

					// If file is a unity file we want to traverse the internal files and add them as UsedFiles
					if (File.Name.StartsWith(Unity.ModulePrefix))
					{
						foreach (string Line in System.IO.File.ReadAllLines(File.FullName))
						{
							int IncludeStart = Line.IndexOf('"') + 1;
							if (IncludeStart != 0)
							{
								string Include = Line.Substring(IncludeStart, Line.Length - IncludeStart - 1);
								FileItem SourceFile = FileItem.GetItemByFileReference(FileReference.Combine(Unreal.EngineSourceDirectory, Include));
								UsedFiles.TryAdd(SourceFile.Location, 0);
							}
						}
					}
					else
					{
						UsedFiles.TryAdd(File.Location, 0);
					}
				}
			});

			SortedSet<FileReference> NewFiles = new();
			List<FileReference> FoundFiles = IncludeLookup.FindFiles(SpecificFilesToCompile.Select(x => x.GetFileName())).Select(x => x.Location).ToList();

			// Found files can be a lot (Commandline.h gives 130.000+ files). And we only want to build the ones that are in existing actions
			Parallel.ForEach(FoundFiles, (File) =>
			{
				if (UsedFiles.ContainsKey(File))
				{
					lock (NewFiles)
					{
						NewFiles.Add(File);
					}
				}
			});

			Logger.LogInformation("Found {NewFilesCount} source files", NewFiles.Count);
			foreach (FileReference NewFile in NewFiles)
			{
				Logger.LogDebug("  {NewFile}", NewFile);
			}

			NewFiles.UnionWith(SpecificFilesToCompile);
			return NewFiles.ToList();
		}

		/// <summary>
		/// Outputs the toolchain used to build each target
		/// </summary>
		/// <param name="TargetDescriptors">List of targets being built</param>
		/// <param name="Makefiles">Matching array of makefiles for each target</param>
		/// <param name="Logger">Logger for output</param>
		static void OutputToolchainInfo(List<TargetDescriptor> TargetDescriptors, TargetMakefile[] Makefiles, ILogger Logger)
		{
			List<int> OutputIndices = new List<int>();
			for (int Idx = 0; Idx < TargetDescriptors.Count; Idx++)
			{
				if (!TargetDescriptors[Idx].bQuiet)
				{
					OutputIndices.Add(Idx);
				}
			}

			if (OutputIndices.Count == 1)
			{
				foreach (string Diagnostic in Makefiles[OutputIndices[0]].Diagnostics)
				{
					Logger.LogInformation("{Diagnostic}", Diagnostic);
				}
			}
			else
			{
				foreach (int OutputIndex in OutputIndices)
				{
					foreach (string Diagnostic in Makefiles[OutputIndex].Diagnostics)
					{
						Logger.LogInformation("{Name}: {Diagnostic}", TargetDescriptors[OutputIndex].Name, Diagnostic);
					}
				}
			}
		}

		/// <summary>
		/// Returns true if File is header
		/// </summary>
		/// <param name="File">File to check</param>
		internal static bool IsHeader(FileReference File) { return File.HasExtension(".h") || File.HasExtension(".hpp") || File.HasExtension(".hxx"); }

		/// <summary>
		/// Creates the makefile for a target. If an existing, valid makefile already exists on disk, loads that instead.
		/// </summary>
		/// <param name="BuildConfiguration">The build configuration</param>
		/// <param name="TargetDescriptor">Target being built</param>
		/// <param name="WorkingSet">Set of source files which are part of the working set</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>Makefile for the given target</returns>
		internal static async Task<TargetMakefile> CreateMakefileAsync(BuildConfiguration BuildConfiguration, TargetDescriptor TargetDescriptor, ISourceFileWorkingSet WorkingSet, ILogger Logger)
		{
			// Get the path to the makefile for this target
			FileReference? MakefileLocation = null;
			if (BuildConfiguration.bUseUBTMakefiles)
			{
				MakefileLocation = TargetMakefile.GetLocation(TargetDescriptor.ProjectFile, TargetDescriptor.Name, TargetDescriptor.Platform, TargetDescriptor.Architectures, TargetDescriptor.Configuration, TargetDescriptor.IntermediateEnvironment);
			}

			// Try to load an existing makefile
			TargetMakefile? Makefile = null;
			if (MakefileLocation != null)
			{
				using (GlobalTracer.Instance.BuildSpan("TargetMakefile.Load()").StartActive())
				{
					string? ReasonNotLoaded;
					Makefile = TargetMakefile.Load(MakefileLocation, TargetDescriptor.ProjectFile, TargetDescriptor.Platform, TargetDescriptor.AdditionalArguments.GetRawArray(), Logger, out ReasonNotLoaded);
					if (Makefile == null)
					{
						Logger.LogInformation("Creating makefile for {TargetDescriptorName} ({ReasonNotLoaded})", TargetDescriptor.Name, ReasonNotLoaded);
					}
				}
			}

			// If we have a makefile, execute the pre-build steps and check it's still valid
			bool bHasRunPreBuildScripts = false;
			if (Makefile != null)
			{
				// Execute the scripts.
				Utils.ExecuteCustomBuildSteps(Makefile.PreBuildScripts, Logger);

				// Don't run the pre-build steps again, even if we invalidate the makefile.
				bHasRunPreBuildScripts = true;

				// Check that the makefile is still valid
				string? Reason;
				if (!TargetMakefile.IsValidForSourceFiles(Makefile, TargetDescriptor.ProjectFile, TargetDescriptor.Platform, WorkingSet, Logger, out Reason))
				{
					Logger.LogInformation("Invalidating makefile for {TargetDescriptorName} ({Reason})", TargetDescriptor.Name, Reason);
					Makefile = null;
				}
			}

			// If we couldn't load a makefile, create a new one
			if (Makefile == null)
			{
				// Create the target
				UEBuildTarget Target;
				using (GlobalTracer.Instance.BuildSpan("UEBuildTarget.Create()").StartActive())
				{
					Target = UEBuildTarget.Create(TargetDescriptor, BuildConfiguration, Logger);
				}

				// Create the pre-build scripts
				FileReference[] PreBuildScripts = Target.CreatePreBuildScripts();

				// Execute the pre-build scripts
				if (!bHasRunPreBuildScripts)
				{
					Utils.ExecuteCustomBuildSteps(PreBuildScripts, Logger);
					bHasRunPreBuildScripts = true;
				}

				// Build the target
				using (GlobalTracer.Instance.BuildSpan("UEBuildTarget.Build()").StartActive())
				{
					Makefile = await Target.BuildAsync(BuildConfiguration, WorkingSet, TargetDescriptor, Logger);
				}

				Makefile.MemoryPerActionGB = Target.Rules.MemoryPerActionGB;

				// Save the pre-build scripts onto the makefile
				Makefile.PreBuildScripts = PreBuildScripts;

				// Save the additional command line arguments
				Makefile.AdditionalArguments = TargetDescriptor.AdditionalArguments.GetRawArray();

				// Save the environment variables
				foreach (System.Collections.DictionaryEntry? EnvironmentVariable in Environment.GetEnvironmentVariables())
				{
					if (EnvironmentVariable != null)
					{
						Makefile.EnvironmentVariables.Add(Tuple.Create((string)EnvironmentVariable.Value.Key, (string)EnvironmentVariable.Value.Value!));
					}
				}

				// Save the makefile for next time
				if (MakefileLocation != null)
				{
					using (GlobalTracer.Instance.BuildSpan("TargetMakefile.Save()").StartActive())
					{
						Makefile.Save(MakefileLocation);
					}
				}
			}
			else
			{
				// Restore the environment variables
				foreach (Tuple<string, string> EnvironmentVariable in Makefile.EnvironmentVariables)
				{
					Environment.SetEnvironmentVariable(EnvironmentVariable.Item1, EnvironmentVariable.Item2);
				}

#if __VPROJECT_AVAILABLE__
				Task VNITask = Task.CompletedTask;
				// Same for VNI
				if (Makefile.VNIModules.Count > 0)
				{
					VNITask = Task.Run(() => VNIExecution.ExecuteVNITool(Makefile, TargetDescriptor, Logger));
				}
#endif
				// If the target needs UHT to be run, we'll go ahead and do that now
				if (Makefile.UObjectModules.Count > 0)
				{
					await ExternalExecution.ExecuteHeaderToolIfNecessaryAsync(BuildConfiguration, TargetDescriptor.ProjectFile, Makefile, TargetDescriptor.Name, WorkingSet, Logger);
				}

#if __VPROJECT_AVAILABLE__
				VNITask.Wait();
#endif
			}
			return Makefile;
		}

		/// <summary>
		/// Determines all the actions that should be executed for a target (filtering for single module/file, etc..)
		/// </summary>
		/// <param name="TargetDescriptor">The target being built</param>
		/// <param name="Makefile">Makefile for the target</param>
		/// <param name="OutputItems">Set of all output items</param>
		/// <returns>List of actions that need to be executed</returns>
		static void GatherOutputItems(TargetDescriptor TargetDescriptor, TargetMakefile Makefile, HashSet<FileItem> OutputItems)
		{
			if (TargetDescriptor.SpecificFilesToCompile.Count > 0)
			{
			}
			else if (TargetDescriptor.OnlyModuleNames.Count > 0)
			{
				// Find the output items for this module
				foreach (string OnlyModuleName in TargetDescriptor.OnlyModuleNames)
				{
					FileItem[]? OutputItemsForModule;
					if (!Makefile.ModuleNameToOutputItems.TryGetValue(OnlyModuleName, out OutputItemsForModule))
					{
						throw new BuildException("Unable to find output items for module '{0}'", OnlyModuleName);
					}
					OutputItems.UnionWith(OutputItemsForModule);
				}
			}
			else
			{
				// Use all the output items from the target
				OutputItems.UnionWith(Makefile.OutputItems);
			}
		}

		/// <summary>
		/// Merge action graphs for multiple targets into a single set of actions. Sets group names on merged actions to indicate which target they belong to.
		/// </summary>
		/// <param name="TargetDescriptors">List of target descriptors</param>
		/// <param name="TargetActions">Array of actions for each target</param>
		/// <returns>List of merged actions</returns>
		static List<LinkedAction> MergeActionGraphs(List<TargetDescriptor> TargetDescriptors, List<LinkedAction>[] TargetActions)
		{
			// Set of all output items. Knowing that there are no conflicts in produced items, we use this to eliminate duplicate actions.
			Dictionary<FileItem, LinkedAction> OutputItemToProducingAction = new Dictionary<FileItem, LinkedAction>();
			HashSet<LinkedAction> IgnoreConflictActions = new HashSet<LinkedAction>();
			for (int TargetIdx = 0; TargetIdx < TargetDescriptors.Count; TargetIdx++)
			{
				string GroupPrefix = String.Format("{0}-{1}-{2}", TargetDescriptors[TargetIdx].Name, TargetDescriptors[TargetIdx].Platform, TargetDescriptors[TargetIdx].Configuration);
				foreach (LinkedAction TargetAction in TargetActions[TargetIdx])
				{
					if (!TargetAction.ProducedItems.Any())
					{
						continue;
					}

					if (TargetAction.IgnoreConflicts())
					{
						IgnoreConflictActions.Add(TargetAction);
						TargetAction.GroupNames.Add(GroupPrefix);
						continue;
					}

					FileItem ProducedItem = TargetAction.ProducedItems.First();

					LinkedAction? ExistingAction;
					if (!OutputItemToProducingAction.TryGetValue(ProducedItem, out ExistingAction))
					{
						ExistingAction = new LinkedAction(TargetAction, TargetDescriptors[TargetIdx]);
						OutputItemToProducingAction[ProducedItem] = ExistingAction;
					}
					ExistingAction.GroupNames.Add(GroupPrefix);
				}
			}
			List<LinkedAction> Results = new List<LinkedAction>(OutputItemToProducingAction.Values);
			Results.AddRange(IgnoreConflictActions);
			return Results;
		}

		void ProcessCoreDumps(DirectoryReference? SaveCrashDumpDirectory, ILogger Logger)
		{
			if (SaveCrashDumpDirectory == null)
			{
				return;
			}

			if (!RuntimePlatform.IsWindows)
			{
				return;
			}

			// set to true to have this code create some files in expected locations to report 
			bool bTestDumpDetection = false;

			using OpenTracing.IScope Scope = GlobalTracer.Instance.BuildSpan("Processing crash dump files").StartActive();

			DateTime UBTStartTime = Process.GetCurrentProcess().StartTime;

			List<FileReference> FoundCrashDumps = new List<FileReference>();

			// examine the contents of %LOCALAPPDATA%\CrashDumps
			string? LocalAppData = Environment.GetEnvironmentVariable("LOCALAPPDATA");
			if (LocalAppData != null)
			{
				DirectoryReference CrashDumpsDirectory = DirectoryReference.Combine(DirectoryReference.FromString(LocalAppData)!, "CrashDumps");
				if (DirectoryReference.Exists(CrashDumpsDirectory))
				{
					if (bTestDumpDetection)
					{
						System.IO.File.Create(System.IO.Path.Combine(CrashDumpsDirectory.FullName, Guid.NewGuid().ToString() + ".dmp")).Close();
					}

					foreach (FileReference CrashDump in DirectoryReference.EnumerateFiles(CrashDumpsDirectory))
					{
						if (FileReference.GetLastWriteTime(CrashDump) > UBTStartTime)
						{
							Logger.LogWarning("Crash dump {CrashDump} was created duing UnrealBuildTool execution", CrashDump);
							FoundCrashDumps.Add(CrashDump);
						}
					}
				}
			}

			// examine the contents of %TMP% (on CI agents, this should not be unreasonably slow as the tmp dir gets cleaned between builds
			string? TMP = Environment.GetEnvironmentVariable("TMP");
			if (TMP != null)
			{
				DirectoryReference TmpDir = DirectoryReference.FromString(TMP)!;

				if (bTestDumpDetection)
				{
					System.IO.File.Create(System.IO.Path.Combine(TmpDir.FullName, Guid.NewGuid().ToString() + ".dmp")); //.Close(); Intentionally not closed, to trigger catch() block below
				}

				List<DirectoryReference> AccessibleTmpDirectories = new List<DirectoryReference>();

				AccessibleTmpDirectories.Add(TmpDir);

				for (int I = 0; I < AccessibleTmpDirectories.Count; ++I)
				{
					try
					{
						// Manual recursion to avoid the case where an inaccessible file prevents us from iterating reachable parts of the dir
						AccessibleTmpDirectories.AddRange(DirectoryReference.EnumerateDirectories(AccessibleTmpDirectories[I]));

						foreach (FileReference TmpFile in DirectoryReference.EnumerateFiles(AccessibleTmpDirectories[I]))
						{
							if (TmpFile.HasExtension(".dmp") && FileReference.GetLastWriteTime(TmpFile) > UBTStartTime)
							{
								Logger.LogWarning("Crash dump {TmpFile} was created duing UnrealBuildTool execution", TmpFile);
								FoundCrashDumps.Add(TmpFile);
							}
						}
					}
					catch
					{
						// silently ignore inaccessible directories
					}
				}
			}

			if (FoundCrashDumps.Count > 0)
			{
				DirectoryReference.CreateDirectory(SaveCrashDumpDirectory);
				foreach (FileReference CrashDump in FoundCrashDumps)
				{
					Logger.LogInformation("Copying {CrashDump} to {SaveCrashDumpDirectory}", CrashDump, SaveCrashDumpDirectory);
					try
					{
						FileReference.Copy(CrashDump, FileReference.Combine(SaveCrashDumpDirectory, CrashDump.GetFileName()));
					}
					catch (Exception Ex)
					{
						// don't stop if there was a problem copying one of the files
						Logger.LogWarning("Failed to copy crash dump {CrashDump}: {Ex}", CrashDump, ExceptionUtils.FormatException(Ex));
					}
				}
			}
		}
	}
}

