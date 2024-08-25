// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.Json;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using Microsoft.Extensions.Logging;
using UnrealBuildBase;

// IWYUMode is a mode that can be used to clean up includes in source code. It uses the clang based tool include-what-you-use (IWYU) to figure out what is needed in each .h/.cpp file
// and then cleans up accordingly. This mode can be used to clean up unreal as well as plugins and projects on top of unreal.
// Note, IWYU is not perfect. There are still c++ features not supported so even though it will do a good job cleaning up it might require a little bit of hands on but not much.
// When iwyu for some reason removes includes you want to keep, there are ways to anotate the code. Look at the pragma link below.
// 
// IWYU Github:  https://github.com/include-what-you-use/include-what-you-use 
// IWYU Pragmas: https://github.com/include-what-you-use/include-what-you-use/blob/master/docs/IWYUPragmas.md
// 
// Here are examples of how a commandline could look (using ushell)
// 
//  1. Will build lyra editor target using include-what-you-use.exe instead of clang.exe, and preview how iwyu would update module LyraGame and all the modules depending on it
//  
//   .build target LyraEditor linux development -- -Mode=IWYU -ModuleToUpdate=LyraGame -UpdateDependents
//   
//  2. Same as 1 but will check out files from p4 and modify them
//  
//   .build target LyraEditor linux development -- -Mode=IWYU -ModuleToUpdate=LyraGame -UpdateDependents -Write
//   
//  3. Will build and then update all modules that has Lyra/Plugins in its path. Note it will only include modules that are part of LyraEditor target.
// 
//   .build target LyraEditor linux development -- -Mode=IWYU -PathToUpdate=Lyra/Plugins -Write
//   
//  4. Update Niagara plugins private code (not public api) with preview
// 
//   .build target UnrealEditor linux development -- -Mode=IWYU -PathToUpdate=Engine/Plugins/FX/Niagara -UpdateOnlyPrivate

namespace UnrealBuildTool
{
	/// <summary>
	/// Representing an #include inside a code file.
	/// </summary>
	class IWYUIncludeEntry
	{
		/// <summary>
		/// Full path using forward paths, eg: d:/dev/folder/file.h
		/// </summary>
		public string Full { get; set; } = "";

		/// <summary>
		/// Printable path for actual include. Does not include quotes or angle brackets. eg: folder/file.h
		/// </summary>
		public string Printable { get; set; } = "";

		/// <summary>
		/// Reference to file info. Not populated with .iwyu file
		/// </summary>
		public IWYUInfo? Resolved;

		/// <summary>
		/// Decides if printable should be #include quote or angle bracket
		/// </summary>
		public bool System { get; set; }

		/// <summary>
		/// If true #include was found inside a declaration such as a namespace or class/struct/function
		/// </summary>
		public bool InsideDecl { get; set; }
	}

	/// <summary>
	/// Comparer for include entries using printable to compare
	/// </summary>
	class IWYUIncludeEntryPrintableComparer : IEqualityComparer<IWYUIncludeEntry>
	{
		public bool Equals(IWYUIncludeEntry? x, IWYUIncludeEntry? y)
		{
			return x!.Printable == y!.Printable;
		}

		public int GetHashCode([DisallowNull] IWYUIncludeEntry obj)
		{
			throw new NotImplementedException();
		}
	}

	/// <summary>
	/// Representing a forward declaration inside a code file. eg: class MyClass;
	/// </summary>
	struct IWYUForwardEntry
	{
		/// <summary>
		/// The string to add to the file
		/// </summary>
		public string Printable { get; set; }

		/// <summary>
		/// True if forward declaration has already been seen in the file
		/// </summary>
		public bool Present { get; set; }
	}

	/// <summary>
	/// Representing an include that the code file needs that is missing in the include list
	/// Note, in cpp files it is listing the includes that are in the matching h file
	/// </summary>
	struct IWYUMissingInclude
	{
		/// <summary>
		/// Full path using forward paths, eg: d:/dev/folder/file.h
		/// </summary>
		public string Full { get; set; }
	}

	/// <summary>
	/// Representing a file (header or source). Each .iwyu file has a list of these (most of the time only one)
	/// </summary>
	class IWYUInfo
	{
		/// <summary>
		/// Full path of file using forward paths, eg: d:/dev/folder/file.h
		/// </summary>
		public string File { get; set; } = "";

		/// <summary>
		/// Includes that this file needs. This is what iwyu have decided is used.
		/// </summary>
		public List<IWYUIncludeEntry> Includes { get; set; } = new List<IWYUIncludeEntry>();

		/// <summary>
		/// Forward declarations that this file needs. This is what iwyu have decided is used.
		/// </summary>
		public List<IWYUForwardEntry> ForwardDeclarations { get; set; } = new List<IWYUForwardEntry>();

		/// <summary>
		/// Includes seen in the file. This is how it looked like on disk.
		/// </summary>
		public List<IWYUIncludeEntry> IncludesSeenInFile { get; set; } = new List<IWYUIncludeEntry>();

		/// <summary>
		/// Includes that didnt end up in the Includes list but is needed by the code file
		/// </summary>
		public List<IWYUMissingInclude> MissingIncludes { get; set; } = new List<IWYUMissingInclude>();

		/// <summary>
		/// Transitive includes. This is all the includes that someone gets for free when including this file
		/// Note, this is based on what iwyu believes should be in all includes. So it will not look at "seen includes"
		/// and only use its generated list.
		/// </summary>
		public Dictionary<string, string> TransitiveIncludes = new();

		/// <summary>
		/// Transitive forward declarations. Same as transitive includes
		/// </summary>
		public Dictionary<string, string> TransitiveForwardDeclarations = new();

		/// <summary>
		/// Which .iwyu file that produced this info. Note, it might be null for some special cases (like .generated.h files)
		/// </summary>
		public IWYUFile? Source;

		/// <summary>
		/// Module that this file belongs to
		/// </summary>
		public UEBuildModule? Module;

		/// <summary>
		/// Is true if this file was included inside a declaration such as a namespace or class/struct/function
		/// </summary>
		public bool IncludedInsideDecl;

		/// <summary>
		/// Is true if this file ends with .cpp
		/// </summary>
		public bool IsCpp;
	}

	/// <summary>
	/// This is what include-what-you-use.exe produces for each build. a .iwyu file that is parsed into these instances
	/// </summary>
	class IWYUFile
	{
		/// <summary>
		/// The different headers and source files that was covered by this execution of iwyu.
		/// </summary>
		public List<IWYUInfo> Files { get; set; } = new();

		/// <summary>
		/// Name of .iwyu file
		/// </summary>
		public string? Name;
	}

	/// <summary>
	/// Profiles different unity sizes and prints out the different size and its timings
	/// </summary>
	[ToolMode("IWYU", ToolModeOptions.XmlConfig | ToolModeOptions.BuildPlatforms | ToolModeOptions.SingleInstance | ToolModeOptions.StartPrefetchingEngine | ToolModeOptions.ShowExecutionTime)]
	class IWYUMode : ToolMode
	{
		/// <summary>
		/// Specifies the file to use for logging.
		/// </summary>
		[XmlConfigFile(Category = "BuildConfiguration")]
		public string? IWYUBaseLogFileName;

		/// <summary>
		/// Will check out files from p4 and write to disk
		/// </summary>
		[CommandLine("-Write")]
		public bool bWrite = false;

		/// <summary>
		/// Update only includes in cpp files and don't touch headers
		/// </summary>
		[CommandLine("-UpdateOnlyPrivate")]
		public bool bUpdateOnlyPrivate = false;

		/// <summary>
		/// Which module to run IWYU on. Will also include all modules depending on this module
		/// </summary>
		[CommandLine("-ModuleToUpdate")]
		public string ModuleToUpdate = "";

		/// <summary>
		/// Which directory to run IWYU on. Will search for modules that has module directory matching this string
		/// If no module is found in -PathToUpdate it handles this for individual files instead.
		/// Note this can be combined with -ModuleToUpdate to double filter
		/// PathToUpdate supports multiple paths using semi colon separation
		/// </summary>
		[CommandLine("-PathToUpdate")]
		public string PathToUpdate = "";

		/// <summary>
		/// Same as PathToUpdate but provide the list of paths in a text file instead.
		/// Add all the desired paths on a separate line
		/// </summary>
		[CommandLine("-PathToUpdateFile")]
		public string PathToUpdateFile = "";

		/// <summary>
		/// Also update modules that are depending on the module we are updating
		/// </summary>
		[CommandLine("-UpdateDependents")]
		public bool bUpdateDependents = false;

		/// <summary>
		/// Will check out files from p4 and write to disk
		/// </summary>
		[CommandLine("-NoP4")]
		public bool bNoP4 = false;

		/// <summary>
		/// Allow files to not add includes if they are transitively included by other includes
		/// </summary>
		[CommandLine("-NoTransitive")]
		public bool bNoTransitiveIncludes = false;

		/// <summary>
		/// Will remove headers that are needed but redundant because they are included through other needed includes
		/// </summary>
		[CommandLine("-RemoveRedundant")]
		public bool bRemoveRedundantIncludes = false;

		/// <summary>
		/// Will skip compiling before updating. Handle with care, this is dangerous since files might not match .iwyu files
		/// </summary>
		[CommandLine("-NoCompile")]
		public bool bNoCompile = false;

		/// <summary>
		/// Will ignore update check that .iwyu is newer than source files.
		/// </summary>
		[CommandLine("-IgnoreUpToDateCheck")]
		public bool bIgnoreUpToDateCheck = false;

		/// <summary>
		/// If set, this will keep removed includes in #if/#endif scope at the end of updated file.
		/// Applied to non-private headers that are part of the Engine folder.
		/// </summary>
		[CommandLine("-DeprecateTag")]
		private string? HeaderDeprecationTagOverride;

		public string HeaderDeprecationTag
		{
			get
			{
				if (!String.IsNullOrEmpty(HeaderDeprecationTagOverride))
				{
					return HeaderDeprecationTagOverride;
				}
				return EngineIncludeOrderHelper.GetLatestDeprecationDefine();
			}
			set => HeaderDeprecationTagOverride = value;
		}

		/// <summary>
		/// Compare current include structure with how it would look like if iwyu was applied on all files
		/// </summary>
		[CommandLine("-Compare")]
		public bool bCompare = false;

		/// <summary>
		/// For Development only - Will write a toc referencing all .iwyu files. Toc can be used by -ReadToc
		/// </summary>
		[CommandLine("-WriteToc")]
		public bool bWriteToc = false;

		/// <summary>
		/// For Development only - Will read a toc to find all .iwyu files instead of building the code.
		/// </summary>
		[CommandLine("-ReadToc")]
		public bool bReadToc = false;

		private string? GetModuleToUpdateName(TargetDescriptor Descriptor)
		{
			if (!String.IsNullOrEmpty(ModuleToUpdate))
			{
				return ModuleToUpdate;
			}

			if (Descriptor.OnlyModuleNames.Count > 0)
			{
				return Descriptor.OnlyModuleNames.First();
			}

			return null;
		}

		private string AdjustModulePathForMatching(string ModulePath)
		{
			string NewModulePath = ModulePath.Replace('\\', '/');
			if (!NewModulePath.EndsWith('/'))
			{
				NewModulePath = NewModulePath + '/';
			}
			return NewModulePath;
		}

		/// <summary>
		/// Execute the command
		/// </summary>
		/// <param name="Arguments">Command line arguments</param>
		/// <returns>Exit code</returns>
		/// <param name="Logger"></param>
		public override async Task<int> ExecuteAsync(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			Logger.LogInformation($"====================================================");
			Logger.LogInformation($"Running IWYU. {(bWrite ? "" : "(Preview mode. Add -Write to write modifications to files)")}");
			Logger.LogInformation($"====================================================");

			// Fixup the log path if it wasn't overridden by a config file
			if (IWYUBaseLogFileName == null)
			{
				IWYUBaseLogFileName = FileReference.Combine(Unreal.EngineProgramSavedDirectory, "UnrealBuildTool", "IWYULog.txt").FullName;
			}

			// Create the log file, and flush the startup listener to it
			if (!Arguments.HasOption("-NoLog") && !Log.HasFileWriter())
			{
				Log.AddFileWriter("DefaultLogTraceListener", FileReference.FromString(IWYUBaseLogFileName));
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
			CommandLineArguments BuildArguments = Arguments.Append(new[] { "-IWYU" }); // Add in case it is not added (it is needed for iwyu toolchain)
			BuildConfiguration BuildConfiguration = new BuildConfiguration();
			XmlConfig.ApplyTo(BuildConfiguration);
			BuildArguments.ApplyTo(BuildConfiguration);
			BuildConfiguration.MaxNestedPathLength = 220; // For now since the path is slightly longer

			// Parse all the target descriptors
			List<TargetDescriptor> TargetDescriptors = TargetDescriptor.ParseCommandLine(BuildArguments, BuildConfiguration, Logger);

			if (TargetDescriptors.Count != 1)
			{
				Logger.LogError($"IWYUMode can only handle command lines that produce one target (Cmdline: {Arguments.ToString()})");
				return 0;
			}

			TargetDescriptors[0].IntermediateEnvironment = UnrealIntermediateEnvironment.IWYU;

			string? ModuleToUpdateName = GetModuleToUpdateName(TargetDescriptors[0]);

			if (!String.IsNullOrEmpty(ModuleToUpdateName))
			{
				foreach (string OnlyModuleName in TargetDescriptors[0].OnlyModuleNames)
				{
					if (String.Compare(OnlyModuleName, ModuleToUpdateName, StringComparison.OrdinalIgnoreCase) != 0)
					{
						Logger.LogError($"ModuleToUpdate '{ModuleToUpdateName}' was not in list of specified modules: {String.Join(", ", TargetDescriptors[0].OnlyModuleNames)}");
						return -1;
					}
				}
			}

			string[] PathToUpdateList = Array.Empty<string>();
			if (!String.IsNullOrEmpty(PathToUpdate))
			{
				PathToUpdateList = PathToUpdate.Split(";");
			}
			else if (!String.IsNullOrEmpty(PathToUpdateFile))
			{
				PathToUpdateList = File.ReadAllLines(PathToUpdateFile);
			}

			if (PathToUpdateList.Length > 0)
			{
				for (int I = 0; I != PathToUpdateList.Length; ++I)
				{
					PathToUpdateList[I] = PathToUpdateList[I].Replace('\\', '/');
				}
			}

			string TargetName = $"{TargetDescriptors[0].Name}_{TargetDescriptors[0].Configuration}_{TargetDescriptors[0].Platform}";

			// Calculate file paths filter to figure out which files that IWYU will run on
			HashSet<string> ValidPaths = new();
			Dictionary<string, UEBuildModule> PathToModule = new();
			Dictionary<string, UEBuildModule> NameToModule = new();
			{
				// Create target to be able to traverse all existing modules
				Logger.LogInformation($"Creating BuildTarget for {TargetName}...");
				UEBuildTarget Target = UEBuildTarget.Create(TargetDescriptors[0], BuildConfiguration, Logger);

				Logger.LogInformation($"Calculating file filter for IWYU...");

				UEBuildModule? UEModuleToUpdate = null;

				// Turn provided module string into UEBuildModule reference
				if (!String.IsNullOrEmpty(ModuleToUpdateName))
				{
					UEModuleToUpdate = Target.GetModuleByName(ModuleToUpdateName);
					if (UEModuleToUpdate == null)
					{
						Logger.LogError($"Can't find module with name {ModuleToUpdateName}");
						return -1;
					}
				}

				if (PathToUpdateList.Length == 0 && UEModuleToUpdate == null)
				{
					Logger.LogError($"Need to provide -ModuleToUpdate <modulename> or -PathToUpdate <partofpath> to run IWYU.");
					return -1;
				}

				int ModulesToUpdateCount = 0;
				int ModulesSkippedCount = 0;

				// Traverse all modules to figure out which ones should be updated. This is based on -ModuleToUpdate and -PathToUpdate
				List<UEBuildModule> ReferencedModules = new();
				HashSet<UEBuildModule> IgnoreReferencedModules = new();
				foreach (UEBuildBinary Binary in Target.Binaries)
				{
					foreach (UEBuildModule Module in Binary.Modules)
					{
						if (IgnoreReferencedModules.Add(Module))
						{
							ReferencedModules.Add(Module);
							Module.GetAllDependencyModules(ReferencedModules, IgnoreReferencedModules, true, false, false);
						}
					}
				}
				foreach (UEBuildModule Module in ReferencedModules)
				{
					NameToModule.TryAdd(Module.Name, Module);
					foreach (UEBuildModule Module2 in Module.GetDependencies(true, true))
					{
						NameToModule.TryAdd(Module2.Name, Module2);
					}
				}

				foreach (UEBuildModule Module in NameToModule.Values)
				{
					bool ShouldUpdate = false;
					string ModuleDir = Module.ModuleDirectory.FullName.Replace('\\', '/');

					if (!PathToModule.TryAdd(ModuleDir, Module))
					{
						continue;
					}
					foreach (DirectoryReference AdditionalDir in Module.ModuleDirectories)
					{
						if (AdditionalDir != Module.ModuleDirectory)
						{
							PathToModule.TryAdd(AdditionalDir.FullName.Replace('\\', '/'), Module);
						}
					}

					if (Module.Rules.Type != ModuleRules.ModuleType.CPlusPlus)
					{
						continue;
					}

					if (UEModuleToUpdate != null)
					{
						bool DependsOnModule = Module == UEModuleToUpdate;
						if (bUpdateDependents)
						{
							if (Module.PublicDependencyModules != null)
							{
								foreach (UEBuildModule Dependency in Module.PublicDependencyModules)
								{
									DependsOnModule = DependsOnModule || Dependency == UEModuleToUpdate;
								}
							}
							if (Module.PrivateDependencyModules != null)
							{
								foreach (UEBuildModule Dependency in Module.PrivateDependencyModules!)
								{
									DependsOnModule = DependsOnModule || Dependency == UEModuleToUpdate;
								}
							}
						}
						ShouldUpdate = DependsOnModule;
					}

					if (PathToUpdateList.Length > 0)
					{
						bool Match = false;
						for (int I = 0; I != PathToUpdateList.Length; ++I)
						{
							Match = Match || ModuleDir.Contains(PathToUpdateList[I], StringComparison.OrdinalIgnoreCase);
						}

						if (Match)
						{
							if (UEModuleToUpdate == null)
							{
								ShouldUpdate = true;
							}
						}
						else
						{
							ShouldUpdate = false;
						}
					}
					else if (UEModuleToUpdate == null)
					{
						ShouldUpdate = true;
					}

					if (!ShouldUpdate)
					{
						continue;
					}

					if (Module.Rules.IWYUSupport == IWYUSupport.None)
					{
						++ModulesSkippedCount;
						continue;
					}

					++ModulesToUpdateCount;

					// When adding to ValidPaths, make sure the Module directory ends in a / so we match the exact folder later
					ValidPaths.Add(AdjustModulePathForMatching(ModuleDir));

					foreach (DirectoryReference AdditionalDir in Module.ModuleDirectories)
					{
						if (AdditionalDir != Module.ModuleDirectory)
						{
							ValidPaths.Add(AdjustModulePathForMatching(AdditionalDir.FullName));
						}
					}
				}

				if (ValidPaths.Count == 0 && PathToUpdateList.Length > 0)
				{
					foreach (string Path in PathToUpdateList)
					{
						ValidPaths.Add(Path);
					}
					Logger.LogInformation($"Will update files matching {PathToUpdate}. Note, path is case sensitive");
				}
				else
				{
					Logger.LogInformation($"Will update {ModulesToUpdateCount} module(s) using IWYU. ({ModulesSkippedCount} skipped because of bEnforceIWYU=false)...");
				}
			}

			Dictionary<string, IWYUInfo> Infos = new();
			HashSet<string> GeneratedHeaderInfos = new();
			List<IWYUInfo> GeneratedCppInfos = new();

			using (ISourceFileWorkingSet WorkingSet = new EmptySourceFileWorkingSet())
			{
				List<string>? TocContent = null;
				int ReadSuccess = 1;

				if (bReadToc)
				{
					string TocName = TargetName + ".txt";
					FileReference TocReference = FileReference.Combine(Unreal.EngineDirectory, "Intermediate", "IWYU", TocName);
					Logger.LogInformation($"Reading TOC from {TocReference.FullName}...");
					if (FileReference.Exists(TocReference))
					{
						TocContent = FileReference.ReadAllLines(TocReference).ToList();
					}
				}

				if (TocContent == null)
				{
					TargetDescriptor Descriptor = TargetDescriptors.First();

					// Create the make file that contains all the actions we will use to find .iwyu files.
					Logger.LogInformation($"Creating MakeFile for target...");
					TargetMakefile Makefile;
					try
					{
						Makefile = await BuildMode.CreateMakefileAsync(BuildConfiguration, Descriptor, WorkingSet, Logger);
					}
					finally
					{
						SourceFileMetadataCache.SaveAll();
					}

					// Use the make file to build unless -NoCompile is set.
					if (!bNoCompile)
					{
						try
						{
							await BuildMode.BuildAsync(new TargetMakefile[] { Makefile }, new List<TargetDescriptor>() { Descriptor }, BuildConfiguration, BuildOptions.None, null, Logger);
						}
						finally
						{
							CppDependencyCache.SaveAll();
						}
					}

					HashSet<FileItem> OutputItems = new HashSet<FileItem>();
					if (Descriptor.OnlyModuleNames.Count > 0)
					{
						foreach (string OnlyModuleName in Descriptor.OnlyModuleNames)
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

					TocContent = new();

					foreach (FileItem OutputItem in OutputItems)
					{
						if (OutputItem.Name.EndsWith(".iwyu"))
						{
							TocContent.Add(OutputItem.AbsolutePath);
						}
					}
				}

				// Time to parse all the .iwyu files generated from iwyu.
				// We Do this in parallel since it involves reading a ton of .iwyu files from disk.
				Logger.LogInformation($"Parsing {TocContent.Count} .iwyu files...");
				Parallel.ForEach(TocContent, IWYUFilePath =>
				{
					string? JsonContent = File.ReadAllText(IWYUFilePath);
					if (JsonContent == null)
					{
						Logger.LogError($"Failed to read file {IWYUFilePath}");
						Interlocked.Exchange(ref ReadSuccess, 0);
						return;
					}

					try
					{
						IWYUFile? IWYUFile = JsonSerializer.Deserialize<IWYUFile>(JsonContent);
						IWYUFile!.Name = IWYUFilePath;

						// Traverse the cpp/inl/h file entries inside the .iwyu file
						foreach (IWYUInfo Info in IWYUFile!.Files)
						{
							Info.Source = IWYUFile;
							Info.IsCpp = Info.File.EndsWith(".cpp");

							// We track .gen.cpp in a special list, they need special treatment later
							if (Info.File.Contains(".gen.cpp", StringComparison.Ordinal))
							{
								lock (GeneratedCppInfos)
								{
									GeneratedCppInfos.Add(Info);
								}

								continue;
							}

							// Ok, time to add file entry to lookup
							lock (Infos)
							{
								if (!Infos.TryAdd(Info.File, Info))
								{
									// This is a valid scenario when Foo.cpp also registers Foo.h... and then Foo.h registers itself.
									IWYUInfo ExistingEntry = Infos[Info.File];
									if (IWYUFile.Files.Count == 1)
									{
										if (ExistingEntry.Source!.Files.Count == 1)
										{
											Logger.LogError($"{Info.File} - built twice somehow?");
											return;
										}
										else
										{
											Infos[Info.File] = Info;
										}
									}
									else
									{
										//bool Equals = Info.Includes.SequenceEqual(ExistingEntry.Includes, new IWYUIncludeEntryPrintableComparer());
										//if (!Equals)
										//{
										//	Logger.LogWarning($"{Info.File} - mismatch found in multiple .iwyu-files");
										//}
									}
								}
							}

							// TODO: Fix bad formatting coming from iwyu
							foreach (IWYUIncludeEntry Entry in Info.IncludesSeenInFile)
							{
								if (Entry.Printable[0] == '<')
								{
									Entry.Printable = Entry.Printable.Substring(1, Entry.Printable.Length - 2);
									Entry.System = true;
								}
							}

							Info.Module = GetModule(Info.File, PathToModule);

							// Some special logic for headers.
							if (Info.File.EndsWith(".h"))
							{
								// We need to add entries for .generated.h. They are not in the iwyu files
								foreach (IWYUIncludeEntry Include in Info.Includes)
								{
									if (Include.Full.Contains("/UHT/"))
									{
										lock (GeneratedHeaderInfos)
										{
											GeneratedHeaderInfos.Add(Include.Full);
										}

										break;
									}
									else if (Include.Full.Contains("/VNI/"))
									{
										lock (GeneratedHeaderInfos)
										{
											GeneratedHeaderInfos.Add(Include.Full);
										}
									}
								}
							}
						}
					}
					catch (Exception e)
					{
						Logger.LogError($"Failed to parse json {IWYUFilePath}: {e.Message} - File will be deleted");
						File.Delete(IWYUFilePath);
						Interlocked.Exchange(ref ReadSuccess, 0);
						return;
					}
				});

				// Something went wrong parsing iwyu files.
				if (ReadSuccess == 0)
				{
					return -1;
				}

				if (bWriteToc)
				{
					Logger.LogInformation($"Writing TOC that references all .iwyu files ");
					DirectoryReference TocDir = DirectoryReference.Combine(Unreal.EngineDirectory, "Intermediate", "IWYU");
					DirectoryReference.CreateDirectory(TocDir);
					string TocName = TargetName + ".txt";
					FileReference TocReference = FileReference.Combine(TocDir, TocName);
					FileReference.WriteAllLines(TocReference, TocContent);
				}
			}

			KeyValuePair<string, IWYUIncludeEntry?> GetInfo(string Include, string Path)
			{
				string FullPath = DirectoryReference.Combine(Unreal.EngineDirectory, Path).FullName.Replace('\\', '/');
				IWYUInfo? Out = null;
				if (!Infos.TryGetValue(FullPath, out Out))
				{
					return new(Include, null);
				}

				IWYUIncludeEntry Entry = new();
				Entry.Printable = Include;
				Entry.Full = FullPath;
				Entry.Resolved = Out;
				return new(Include, Entry);
			}

			Dictionary<string, IWYUIncludeEntry?> SpecialIncludes = new Dictionary<string, IWYUIncludeEntry?>(
				new[]
				{
					GetInfo("UObject/ObjectMacros.h", "Source/Runtime/CoreUObject/Public/UObject/ObjectMacros.h"),
					GetInfo("UObject/ScriptMacros.h", "Source/Runtime/CoreUObject/Public/UObject/ScriptMacros.h"),
					GetInfo("VerseInteropUtils.h", "Restricted/NotForLicensees/Plugins/Solaris/Source/VerseNative/Public/VerseInteropTypes.h"),
					GetInfo("Containers/ContainersFwd.h", "Source/Runtime/Core/Public/Containers/ContainersFwd.h"),
					GetInfo("Misc/OptionalFwd.h", "Source/Runtime/Core/Public/Misc/OptionalFwd.h"),
				}
			);

			Dictionary<string, IWYUIncludeEntry?> ForwardingHeaders = new Dictionary<string, IWYUIncludeEntry?>()
			{
				{ "TMap", SpecialIncludes["Containers/ContainersFwd.h"] },
				{ "TSet", SpecialIncludes["Containers/ContainersFwd.h"] },
				{ "TArray", SpecialIncludes["Containers/ContainersFwd.h"] },
				{ "TArrayView", SpecialIncludes["Containers/ContainersFwd.h"] },
				{ "TOptional", SpecialIncludes["Misc/OptionalFwd.h"] },
			};

			// Add all .generated.h files as entries in the lookup and explicitly add the includes they have which will never be removed
			Logger.LogInformation($"Generating infos for .generated.h files...");
			if (GeneratedHeaderInfos.Count > 0)
			{
				IWYUIncludeEntry? ObjectMacrosInclude = SpecialIncludes["UObject/ObjectMacros.h"];
				IWYUIncludeEntry? ScriptMacrosInclude = SpecialIncludes["UObject/ScriptMacros.h"];
				IWYUIncludeEntry? VerseInteropUtilsInclude = SpecialIncludes["VerseInteropUtils.h"];

				foreach (string Gen in GeneratedHeaderInfos)
				{
					IWYUInfo GenInfo = new();
					GenInfo.File = Gen;
					if (Gen.EndsWith(".generated.h"))
					{
						if (ObjectMacrosInclude != null)
						{
							GenInfo.IncludesSeenInFile.Add(ObjectMacrosInclude);
							GenInfo.Includes.Add(ObjectMacrosInclude);
						}
						if (ScriptMacrosInclude != null)
						{
							GenInfo.IncludesSeenInFile.Add(ScriptMacrosInclude);
							GenInfo.Includes.Add(ScriptMacrosInclude);
						}
					}
					else
					{
						if (VerseInteropUtilsInclude != null)
						{
							GenInfo.IncludesSeenInFile.Add(VerseInteropUtilsInclude);
							GenInfo.Includes.Add(VerseInteropUtilsInclude);
						}
					}
					Infos.Add(Gen, GenInfo);
				}
			}

			Logger.LogInformation($"Found {Infos.Count} IWYU entries...");

			Logger.LogInformation($"Resolving Includes...");

			LinuxPlatformSDK PlatformSDK = new LinuxPlatformSDK(Logger);
			DirectoryReference? BaseLinuxPath = PlatformSDK.GetBaseLinuxPathForArchitecture(LinuxPlatform.DefaultHostArchitecture);
			string SystemPath = BaseLinuxPath!.FullName.Replace('\\', '/');

			HashSet<IWYUInfo> IncludedInsideDecl = new();
			Dictionary<string, IWYUInfo> SkippedHeaders = new();

			Parallel.ForEach(Infos.Values, Info =>
			{
				// If KeepAsIs we transfer all "seen includes" into the include list.
				if (Info.Module != null && Info.Module.Rules.IWYUSupport == IWYUSupport.KeepAsIs)
				{
					foreach (IWYUIncludeEntry Entry in Info.IncludesSeenInFile)
					{
						// Special hack, we don't want CoreMinimal to be the reason we remove includes transitively
						if (!Entry.Printable.Contains("CoreMinimal.h", StringComparison.Ordinal))
						{
							Info.Includes.Add(Entry);
						}
					}
				}

				if (!Info.IsCpp)
				{
					// See if we need to replace forward declarations with includes
					Info.ForwardDeclarations.RemoveAll(Entry =>
					{
						int LastSpace = Entry.Printable.LastIndexOf(' ');
						string TypeName = Entry.Printable.Substring(LastSpace + 1, Entry.Printable.Length - LastSpace - 2);
						IWYUIncludeEntry? IncludeEntry;
						ForwardingHeaders.TryGetValue(TypeName, out IncludeEntry);
						if (IncludeEntry == null)
						{
							return false;
						}
						Info.Includes.Add(IncludeEntry);
						return true;
					});
				}

				// We don't want to mess around with third party includes.. since we can't see the hierarchy we just assumes that they are optimally included
				Info.Includes.RemoveAll(Entry => Entry.Full.Contains("/ThirdParty/", StringComparison.Ordinal) || Entry.Full.StartsWith(SystemPath));

				foreach (IWYUIncludeEntry Entry in Info.IncludesSeenInFile)
				{
					if (Entry.Full.Contains("/ThirdParty/", StringComparison.Ordinal) || Entry.Full.StartsWith(SystemPath, StringComparison.Ordinal))
					{
						Info.Includes.Add(Entry);
					}

					if (Entry.InsideDecl && Infos.TryGetValue(Entry.Full, out Entry.Resolved))
					{
						lock (IncludedInsideDecl)
						{
							IncludedInsideDecl.Add(Entry.Resolved);
							Info.Includes.Add(Entry);
						}
					}
				}
				/*
				if (Info.IncludesSeenInFile.Count == 0)
				{
					if (Info.File.EndsWith(".inl"))
					{
						Info.Includes.Clear();
						Info.ForwardDeclarations.Clear();
					}
				}
				*/
				// Definitions.h is automatically added in the reponse file and iwyu sees it as not included
				// If there are no includes but we depend on Definitions.h (which is missing) we add Platform.h because it is most likely a <module>_API entry in the file
				/*
				if (Info.Includes.Count == 0)
				{
					bool IsUsingDefinitionsH = false;
					foreach (var Missing in Info.MissingIncludes)
					{
						IsUsingDefinitionsH = IsUsingDefinitionsH || Missing.Full.Contains("Definitions.h");
					}
					if (IsUsingDefinitionsH)
					{
						Info.Includes.Add(new IWYUIncludeEntry() { Full = PlatformInfo!.File, Printable = "HAL/Platform.h", Resolved = PlatformInfo });
					}
				}
				*/
				foreach (IWYUIncludeEntry Include in Info.Includes)
				{
					if (Include.Resolved == null)
					{
						if (!Infos.TryGetValue(Include.Full, out Include.Resolved))
						{
							if (!Include.Full.Contains(".gen.cpp") && !Include.Full.Contains("/ThirdParty/") && !Include.Full.Contains("/AutoSDK/") && !Include.Full.Contains("/VNI/"))
							{
								lock (SkippedHeaders)
								{
									if (!SkippedHeaders.TryGetValue(Include.Full, out Include.Resolved))
									{
										IWYUInfo NewInfo = new();
										Include.Resolved = NewInfo;
										NewInfo.File = Include.Full;
										SkippedHeaders.Add(Include.Full, NewInfo);
									}
								}
							}
						}
					}
				}
			});

			int InfosCount = Infos.Count;
			Logger.LogInformation($"Parsing included headers not supporting being compiled...");
			ParseSkippedHeaders(SkippedHeaders, Infos, PathToModule, NameToModule);
			Logger.LogInformation($"Added {Infos.Count - InfosCount} more read-only IWYU entries...");

			foreach (IWYUInfo Info in IncludedInsideDecl)
			{
				if (Info.IncludesSeenInFile.Count != 0 && !Info.File.EndsWith("ScriptSerialization.h")) // Remove include in ScriptSerialization.h and remove this check
				{
					Logger.LogWarning($"{Info.File} - Included inside declaration in other file but has includes itself.");
				}
				Info.Includes.Clear();
				Info.ForwardDeclarations.Clear();
			}

			Logger.LogInformation($"Generating transitive include lists and forward declaration lists...");
			Parallel.ForEach(Infos.Values, Info =>
			{
				Stack<string> Stack = new();
				Info.TransitiveIncludes.EnsureCapacity(300);
				Info.TransitiveForwardDeclarations.EnsureCapacity(300);
				CalculateTransitive(Info, Info, Stack, Info.TransitiveIncludes, Info.TransitiveForwardDeclarations, false);
			});

			// If we have built .gen.cpp it means that it is not inlined in another cpp file
			// And we need to promote the includes needed to the header since we can never modify the .gen.cpp file
			Logger.LogInformation($"Transferring needed includes from .gen.cpp to owning file...");
			Parallel.ForEach(GeneratedCppInfos, GeneratedCpp =>
			{
				// First, check which files .gen.cpp will see
				HashSet<string> SeenTransitiveIncludes = new();
				foreach (IWYUIncludeEntry SeenInclude in GeneratedCpp.IncludesSeenInFile)
				{
					SeenTransitiveIncludes.Add(SeenInclude.Full);
					foreach (IWYUIncludeEntry Include in GeneratedCpp.Includes)
					{
						IWYUInfo? IncludeInfo;
						if (SeenInclude.Printable == Include.Printable && Infos.TryGetValue(Include.Full, out IncludeInfo))
						{
							foreach (string I in IncludeInfo.TransitiveIncludes.Keys)
							{
								SeenTransitiveIncludes.Add(I);
							}

							break;
						}
					}
				}

				IWYUInfo? IncluderInfo = null;

				// If there is only one file in .iwyu it means that .gen.cpp was compiled separately
				if (GeneratedCpp.Source!.Files.Count == 1)
				{
					int NameStart = GeneratedCpp.File.LastIndexOf('/');
					string HeaderName = GeneratedCpp.File.Substring(NameStart + 1, GeneratedCpp.File.Length - NameStart - ".gen.cpp".Length) + "h";
					foreach (IWYUIncludeEntry Include in GeneratedCpp.Includes)
					{
						NameStart = Include.Full.LastIndexOf('/');
						string IncludeName = Include.Full.Substring(NameStart + 1);
						if (HeaderName == IncludeName)
						{
							Infos.TryGetValue(Include.Full, out IncluderInfo);
							break;
						}
					}
				}
				else // this .gen.cpp is inlined in a cpp..
				{
					IncluderInfo = GeneratedCpp.Source.Files.FirstOrDefault(I => I.File.Contains(".cpp") && !I.File.Contains(".gen"));
				}

				if (IncluderInfo == null)
				{
					return;
				}

				foreach (IWYUIncludeEntry Include in GeneratedCpp.Includes)
				{
					if (SeenTransitiveIncludes.Contains(Include.Full))
					{
						continue;
					}

					// TODO: Remove UObject check once we've added "IWYU pragma: keep" around the includes in ScriptMacros and ObjectMacros

					if (Include.Full.Contains(".generated.h") || Include.Printable.StartsWith("UObject/"))
					{
						continue;
					}

					if (!IncluderInfo!.TransitiveIncludes.ContainsKey(Include.Full))
					{
						if (!Include.Full.Contains("/ThirdParty/") && !Include.Full.StartsWith(SystemPath))
						{
							IncluderInfo.Includes.Add(Include);
						}
					}
				}
			});

			if (bCompare)
			{
				return CompareFiles(Infos, ValidPaths, PathToModule, NameToModule, Logger);
			}
			else
			{
				return UpdateFiles(Infos, ValidPaths, Logger);
			}
		}

		static UEBuildModule? GetModule(string File, Dictionary<string, UEBuildModule> PathToModule)
		{
			string FilePath = File;
			while (true)
			{
				int LastIndexOfSlash = FilePath.LastIndexOf('/');
				if (LastIndexOfSlash == -1)
				{
					break;
				}

				FilePath = FilePath.Substring(0, LastIndexOfSlash);
				UEBuildModule? Module;
				if (PathToModule.TryGetValue(FilePath, out Module))
				{
					return Module;
				}
			}
			return null;
		}

		static string? GetFullName(string Include, string From, UEBuildModule? Module, Dictionary<string, UEBuildModule> NameToModule, HashSet<UEBuildModule>? Visited = null)
		{
			if (Module == null)
			{
				return null;
			}
			foreach (DirectoryReference? Dir in Module.PublicIncludePaths.Union(Module.PrivateIncludePaths).Union(Module.PublicSystemIncludePaths))
			{
				FileReference FileReference = FileReference.Combine(Dir, Include);
				FileItem FileItem = FileItem.GetItemByFileReference(FileReference);
				if (FileItem.Exists)
				{
					return FileItem.Location.FullName.Replace('\\', '/');
				}
			}

			bool Nested = Visited != null;

			if (!Nested)
			{
				FileReference FileReference = FileReference.Combine(FileReference.FromString(From).Directory, Include);
				FileItem FileItem = FileItem.GetItemByFileReference(FileReference);
				if (FileItem.Exists)
				{
					return FileItem.Location.FullName.Replace('\\', '/');
				}
			}

			foreach (string? PublicModule in Module.Rules.PublicDependencyModuleNames.Union(Module.Rules.PublicIncludePathModuleNames).Union(Module.Rules.PrivateIncludePathModuleNames).Union(Module.Rules.PrivateDependencyModuleNames))
			{
				UEBuildModule? DependencyModule;
				if (NameToModule.TryGetValue(PublicModule, out DependencyModule))
				{
					if (Visited == null)
					{
						Visited = new();
						Visited.Add(Module);
						Visited.Add(DependencyModule);
					}
					else if (!Visited.Add(DependencyModule))
					{
						continue;
					}

					string? Str = GetFullName(Include, From, DependencyModule, NameToModule, Visited);
					if (Str != null)
					{
						return Str;
					}
				}
			}

			if (!Nested)
			{
				// This should only show system includes..
				//Console.WriteLine($"Can't resolve {Include} from module {Module.Name}");
			}

			return null;
		}

		static void ParseSkippedHeaders(Dictionary<string, IWYUInfo> SkippedHeaders, Dictionary<string, IWYUInfo> Infos, Dictionary<string, UEBuildModule> PathToModule, Dictionary<string, UEBuildModule> NameToModule)
		{
			foreach (KeyValuePair<string, IWYUInfo> Kvp in SkippedHeaders)
			{
				Infos.Add(Kvp.Key, Kvp.Value);
			}
			while (true)
			{
				Dictionary<string, IWYUInfo> NewSkippedHeaders = new();
				Parallel.ForEach(SkippedHeaders.Values, Info =>
				{
					Info.Module = GetModule(Info.File, PathToModule);

					bool HasIncludeGuard = false;
					int IfCount = 0;
					string[] Lines = File.ReadAllLines(Info.File);
					foreach (string Line in Lines)
					{
						ReadOnlySpan<char> LineSpan = Line.AsSpan().TrimStart();
						if (LineSpan.Length == 0 || LineSpan[0] != '#')
						{
							continue;
						}
						LineSpan = LineSpan.Slice(1).TrimStart();
						if (LineSpan.StartsWith("if"))
						{
							// Include guards are special
							if (!HasIncludeGuard)
							{
								if (LineSpan.StartsWith("ifndef ") && LineSpan.EndsWith("_H"))
								{
									HasIncludeGuard = true;
									continue;
								}
							}
							++IfCount;
						}
						else if (LineSpan.StartsWith("endif"))
						{
							--IfCount;
						}
						else if (LineSpan.StartsWith("include"))
						{
							ReadOnlySpan<char> IncludeSpan = LineSpan.Slice("include".Length).TrimStart();
							char LeadingIncludeChar = IncludeSpan[0];
							if (LeadingIncludeChar == '"' || LeadingIncludeChar == '<')
							{
								ReadOnlySpan<char> Start = IncludeSpan.Slice(1);
								int EndIndex = Start.IndexOf(LeadingIncludeChar == '"' ? '"' : '>');
								ReadOnlySpan<char> FileSpan = Start.Slice(0, EndIndex);
								string Include = FileSpan.ToString();
								string? File = GetFullName(Include, Info.File, Info.Module, NameToModule);
								if (File != null)
								{
									IWYUInfo? IncludeInfo;
									lock (Infos)
									{
										if (!Infos.TryGetValue(File, out IncludeInfo))
										{
											if (!SkippedHeaders.TryGetValue(File, out IncludeInfo))
											{
												IncludeInfo = new();
												IncludeInfo.File = File;
												NewSkippedHeaders.Add(File, IncludeInfo);
												Infos.Add(File, IncludeInfo);
											}
										}
									}
									if (IncludeInfo != null)
									{
										IWYUIncludeEntry Entry = new();
										Entry.Full = File;
										Entry.Printable = Include;
										Entry.System = LeadingIncludeChar == '<';
										Entry.Resolved = IncludeInfo;
										Info.IncludesSeenInFile.Add(Entry);
										Info.Includes.Add(Entry);
									}
								}
							}
						}
					}
				});
				if (NewSkippedHeaders.Count == 0)
				{
					return;
				}
				SkippedHeaders = NewSkippedHeaders;
			}
		}

		static bool IsValidForUpdate(IWYUInfo Info, HashSet<string> ValidPaths, bool ObeyModuleRules)
		{
			if (Info.Source == null) // .generated.h is also in this list, ignore them
			{
				return false;
			}

			if (ObeyModuleRules && Info.Module?.Rules.IWYUSupport != IWYUSupport.Full)
			{
				return false;
			}

			// There are some codegen files with this name
			if (Info.File.Contains(".gen.h"))
			{
				return false;
			}

			// Filter out files
			foreach (string ValidPath in ValidPaths)
			{
				if (Info.File.Contains(ValidPath))
				{
					return true;
				}
			}
			return false;
		}

		int UpdateFiles(Dictionary<string, IWYUInfo> Infos, HashSet<string> ValidPaths, ILogger Logger)
		{
			object? ShouldLog = bWrite ? null : new();

			List<ValueTuple<IWYUInfo, List<string>>> UpdatedFiles = new(); // <FilePath>, <NewLines>
			HashSet<IWYUFile> SkippedFiles = new();
			int SkippedCount = 0;
			int OutOfDateCount = 0;

			Logger.LogInformation($"Updating code files (in memory)...");

			uint FilesParseCount = 0;
			int ProcessSuccess = 1;

			Parallel.ForEach(Infos.Values, Info =>
			{
				if (!IsValidForUpdate(Info, ValidPaths, true))
				{
					return;
				}

				bool IsCpp = Info.IsCpp;
				bool IsPrivate = IsCpp || Info.File.Contains("/Private/");
				bool IsInternal = Info.File.Contains("/Internal/");

				// If we only want to update private files we early out for non-private headers
				if (!IsPrivate && (bUpdateOnlyPrivate || (Info.Module?.Rules.IWYUSupport == IWYUSupport.KeepPublicAsIsForNow)))
				{
					return;
				}

				Interlocked.Increment(ref FilesParseCount);

				string MatchingH = "";
				if (IsCpp)
				{
					int LastSlash = Info.File.LastIndexOf('/') + 1;
					MatchingH = Info.File.Substring(LastSlash, Info.File.Length - LastSlash - 4) + ".h";
				}

				Dictionary<string, string> TransitivelyIncluded = new();
				SortedSet<string> CleanedupIncludes = new();
				SortedSet<string> ForwardDeclarationsToAdd = new();

				foreach (IWYUIncludeEntry Include in Info.Includes)
				{
					// We never remove header with name matching cpp
					string NameWithoutPath = Include.Printable;
					int LastSlash = NameWithoutPath.LastIndexOf("/");
					if (LastSlash != -1)
					{
						NameWithoutPath = NameWithoutPath.Substring(LastSlash + 1);
					}

					string QuotedPrintable = Include.System ? $"<{Include.Printable}>" : $"\"{Include.Printable}\"";

					bool Keep = true;
					if (Info.File == Include.Full) // Sometimes IWYU outputs include to the same file if .gen.cpp is inlined and includes file with slightly different path. just skip those
					{
						Keep = false;
					}
					else if (IsCpp && MatchingH == NameWithoutPath)
					{
						Keep = true;
					}
					else if (!bNoTransitiveIncludes)
					{
						foreach (IWYUIncludeEntry Include2 in Info.Includes)
						{
							if (Include2.Resolved != null && Include != Include2)
							{
								string Key = Include.Full;
								string? TransitivePath;
								if (Include2.Resolved.TransitiveIncludes!.TryGetValue(Key, out TransitivePath))
								{
									if (ShouldLog != null)
									{
										TransitivelyIncluded.TryAdd(QuotedPrintable, String.Join(" -> ", Include2.Printable, TransitivePath));
									}

									Keep = false;
									break;
								}
							}
						}
					}
					if (Keep)
					{
						CleanedupIncludes.Add(QuotedPrintable);
					}
				}

				// We don't remove seen includes that are redundant because they are included through other includes that are needed.
				if (!bRemoveRedundantIncludes)
				{
					List<string> ToReadd = new();
					foreach (IWYUIncludeEntry Seen in Info.IncludesSeenInFile)
					{
						string QuotedPrintable = Seen.System ? $"<{Seen.Printable}>" : $"\"{Seen.Printable}\"";
						if (!CleanedupIncludes.Contains(QuotedPrintable) && Info.TransitiveIncludes.ContainsKey(Seen.Full))
						{
							CleanedupIncludes.Add(QuotedPrintable);
						}
					}
				}

				// Ignore forward declarations for cpp files. They are very rarely needed and we let the user add them manually instead
				if (!IsCpp)
				{
					foreach (IWYUForwardEntry ForwardDeclaration in Info.ForwardDeclarations)
					{
						bool Add = ForwardDeclaration.Present == false;
						if (!bNoTransitiveIncludes)
						{
							foreach (IWYUIncludeEntry Include2 in Info.Includes)
							{
								if (Include2.Resolved != null && Include2.Resolved.TransitiveForwardDeclarations!.ContainsKey(ForwardDeclaration.Printable))
								{
									Add = false;
									break;
								}
							}
						}
						if (Add)
						{
							ForwardDeclarationsToAdd.Add(ForwardDeclaration.Printable);
						}
					}
				}

				// Read all lines of the header/source file
				string[] ExistingLines = File.ReadAllLines(Info.File);

				SortedDictionary<string, string> LinesToRemove = new();
				SortedSet<string> IncludesToAdd = new(CleanedupIncludes);

				bool HasIncludes = false;
				string? FirstForwardDeclareLine = null;

				HashSet<string> SeenIncludes = new();
				foreach (IWYUIncludeEntry SeenInclude in Info.IncludesSeenInFile)
				{
					if (SeenInclude.System)
					{
						SeenIncludes.Add($"<{SeenInclude.Printable}>");
					}
					else
					{
						SeenIncludes.Add($"\"{SeenInclude.Printable}\"");
					}
				}

				bool ForceKeepScope = false;
				bool ErrorOnMoreIncludes = false;
				int LineIndex = -1;

				// This makes sure that we have at least HAL/Platform.h included if the file contains XXX_API
				bool Contains_API = false;
				bool LookFor_API = false;
				if (Info.Includes.Count == 0)
				{
					foreach (IWYUMissingInclude Missing in Info.MissingIncludes)
					{
						LookFor_API = LookFor_API || Missing.Full.Contains("Definitions.h");
					}
				}

				// Traverse all lines in file and figure out which includes that should be added or removed
				foreach (string Line in ExistingLines)
				{
					++LineIndex;

					ReadOnlySpan<char> LineSpan = Line.AsSpan().Trim();
					bool StartsWithHash = LineSpan.Length > 0 && LineSpan[0] == '#';
					if (StartsWithHash)
					{
						LineSpan = LineSpan.Slice(1).TrimStart();
					}

					if (!StartsWithHash || !LineSpan.StartsWith("include"))
					{
						// Might be forward declaration.. 
						if (ForwardDeclarationsToAdd.Remove(Line)) // Skip adding the ones that already exists
						{
							if (FirstForwardDeclareLine == null)
							{
								FirstForwardDeclareLine = Line;
							}
						}

						if (Line.Contains("IWYU pragma: "))
						{
							if (Line.Contains(": begin_keep"))
							{
								ForceKeepScope = true;
							}
							else if (Line.Contains(": end_keep"))
							{
								ForceKeepScope = false;
							}
						}

						// File is autogenerated by some tool, don't mess with it
						if (Line.Contains("AUTO GENERATED CONTENT, DO NOT MODIFY"))
						{
							return;
						}

						if (LookFor_API && !Contains_API && Line.Contains("_API", StringComparison.Ordinal))
						{
							Contains_API = true;
						}

						continue;
					}

					if (ErrorOnMoreIncludes)
					{
						Interlocked.Exchange(ref ProcessSuccess, 0);
						Logger.LogError($"{Info.File} - Found special include using macro and did not expect more includes in this file");
						return;
					}

					HasIncludes = true;

					bool ForceKeep = false;

					ReadOnlySpan<char> IncludeSpan = LineSpan.Slice("include".Length).TrimStart();
					char LeadingIncludeChar = IncludeSpan[0];
					if (LeadingIncludeChar != '"' && LeadingIncludeChar != '<')
					{
						if (IncludeSpan.IndexOf("UE_INLINE_GENERATED_CPP_BY_NAME") != -1)
						{
							int Open = IncludeSpan.IndexOf('(') + 1;
							int Close = IncludeSpan.IndexOf(')');
							string ActualInclude = $"\"{IncludeSpan.Slice(Open, Close - Open).ToString()}.gen.cpp\"";
							IncludesToAdd.Remove(ActualInclude);
						}
						else if (IncludeSpan.IndexOf("COMPILED_PLATFORM_HEADER") != -1)
						{
							int Open = IncludeSpan.IndexOf('(') + 1;
							int Close = IncludeSpan.IndexOf(')');
							string ActualInclude = $"\"Linux/Linux{IncludeSpan.Slice(Open, Close - Open).ToString()}\"";
							IncludesToAdd.Remove(ActualInclude);
						}
						else
						{
							// TODO: These are includes made through defines. IWYU should probably report these in their original shape
							// .. so a #include MY_SPECIAL_INCLUDE is actually reported as MY_SPECIAL_INCLUDE from IWYU instead of what the define expands to
							// For now, let's assume that if there is one line in the file that is an include 
							if (IncludesToAdd.Count == 1)
							{
								//IncludesToAdd.Clear();
								//ErrorOnMoreIncludes = true;
							}
						}
						continue;
					}
					else
					{
						int Index = IncludeSpan.Slice(1).IndexOf(LeadingIncludeChar == '"' ? '"' : '>');
						if (Index != -1)
						{
							IncludeSpan = IncludeSpan.Slice(0, Index + 2);
						}

						if (Line.Contains("IWYU pragma: ", StringComparison.Ordinal))
						{
							ForceKeep = true;
						}
					}

					string Include = IncludeSpan.ToString();

					// If Include is not seen it means that it is probably inside a #if/#endif with condition false. These includes we can't touch
					if (!SeenIncludes.Contains(Include))
					{
						continue;
					}

					if (!ForceKeep && !ForceKeepScope && !CleanedupIncludes.Contains(Include))
					{
						LinesToRemove.TryAdd(Line, Include);
					}
					else
					{
						IncludesToAdd.Remove(Include);
					}
				}

				if (Contains_API && LookFor_API)
				{
					//IncludesToAdd.Add(new IWYUIncludeEntry() { Full = PlatformInfo!.File, Printable = "HAL/Platform.h", Resolved = PlatformInfo });
					if (!LinesToRemove.Remove("#include \"HAL/Platform.h\""))
					{
						IncludesToAdd.Add("\"HAL/Platform.h\"");
					}
				}

				// Nothing has changed! early out of this file
				if (IncludesToAdd.Count == 0 && LinesToRemove.Count == 0 && ForwardDeclarationsToAdd.Count == 0)
				{
					return;
				}

				// If code file last write time is newer than IWYU file this means that iwyu is not up to date and needs to compile before we can apply anything
				if (!bIgnoreUpToDateCheck)
				{
					FileInfo IwyuFileInfo = new FileInfo(Info.Source!.Name!);
					FileInfo CodeFileInfo = new FileInfo(Info.File!);
					if (CodeFileInfo.LastWriteTime > IwyuFileInfo.LastWriteTime)
					{
						Interlocked.Increment(ref OutOfDateCount);
						return;
					}
				}

				SortedSet<string> LinesToAdd = new();

				foreach (string IncludeToAdd in IncludesToAdd)
				{
					LinesToAdd.Add("#include " + IncludeToAdd);
				}

				if (ShouldLog != null)
				{
					lock (ShouldLog)
					{
						System.Console.WriteLine(Info.File);
						foreach (string I in LinesToAdd)
						{
							System.Console.WriteLine("  +" + I);
						}
						foreach (KeyValuePair<string, string> Pair in LinesToRemove)
						{
							System.Console.Write("  -" + Pair.Key);
							string? Reason;
							if (TransitivelyIncluded.TryGetValue(Pair.Value, out Reason))
							{
								System.Console.Write("  (Transitively included from " + Reason + ")");
							}
							System.Console.WriteLine();
						}
						foreach (string I in ForwardDeclarationsToAdd)
						{
							System.Console.WriteLine("  +" + I);
						}
						System.Console.WriteLine();
					}
				}

				List<string> NewLines = new(ExistingLines.Length);
				SortedSet<String> LinesRemoved = new();

				if (!HasIncludes)
				{
					LineIndex = 0;
					foreach (string OldLine in ExistingLines)
					{
						NewLines.Add(OldLine);
						++LineIndex;

						if (!OldLine.TrimStart().StartsWith("#pragma"))
						{
							continue;
						}

						NewLines.Add("");
						foreach (string Line in LinesToAdd)
						{
							NewLines.Add(Line);
						}

						NewLines.AddRange(ExistingLines.Skip(LineIndex));
						break;
					}
				}
				else
				{
					// This is a bit of a tricky problem to solve in a generic ways since there are lots of exceptions and hard to make assumptions
					// Right now we make the assumption that if there are 
					// That will be the place where we add/remove our includes.

					int ContiguousNonIncludeLineCount = 0;
					bool IsInFirstIncludeBlock = true;
					int LastSeenIncludeBeforeCode = -1;
					int FirstForwardDeclareLineIndex = -1;

					foreach (string OldLine in ExistingLines)
					{
						ReadOnlySpan<char> OldLineSpan = OldLine.AsSpan().TrimStart();
						bool StartsWithHash = OldLineSpan.Length > 0 && OldLineSpan[0] == '#';
						if (StartsWithHash)
						{
							OldLineSpan = OldLineSpan.Slice(1).TrimStart();
						}

						bool IsInclude = StartsWithHash && OldLineSpan.StartsWith("include");

						string OldLineTrimmedStart = OldLine.TrimStart();

						if (!IsInclude)
						{
							if (IsInFirstIncludeBlock)
							{
								if (!String.IsNullOrEmpty(OldLineTrimmedStart))
								{
									++ContiguousNonIncludeLineCount;
								}
								if (LastSeenIncludeBeforeCode != -1 && ContiguousNonIncludeLineCount > 10)
								{
									IsInFirstIncludeBlock = false;
								}
								if (OldLineTrimmedStart.StartsWith("#if"))
								{
									// This logic is a bit shaky but handle the situations where file starts with #if and ends with #endif
									if (LastSeenIncludeBeforeCode != -1)
									{
										IsInFirstIncludeBlock = false;
									}
								}

								// This need to be inside "IsInFirstIncludeBlock" check because some files have forward declares far down in the file
								if (FirstForwardDeclareLine == OldLine)
								{
									FirstForwardDeclareLineIndex = NewLines.Count;
								}
							}

							NewLines.Add(OldLine);
							continue;
						}

						ContiguousNonIncludeLineCount = 0;

						if (!LinesToRemove.ContainsKey(OldLine))
						{
							// If we find #include SOME_DEFINE we assume that should be last and "end" the include block with that
							if (!OldLineTrimmedStart.Contains('\"') && !OldLineTrimmedStart.Contains('<'))
							{
								IsInFirstIncludeBlock = false;
							}
							else if (LinesToAdd.Count != 0 && (!IsCpp || LastSeenIncludeBeforeCode != -1))
							{
								string LineToAdd = LinesToAdd.First();
								if (LineToAdd.CompareTo(OldLine) < 0)
								{
									NewLines.Add(LineToAdd);
									LinesToAdd.Remove(LineToAdd);
								}
							}

							NewLines.Add(OldLine);
						}
						else
						{
							FirstForwardDeclareLineIndex = -1; // This should never happen, but just in case, reset since lines have changed
							LinesRemoved.Add(OldLine);
						}

						if (IsInFirstIncludeBlock)
						{
							LastSeenIncludeBeforeCode = NewLines.Count - 1;
						}
					}

					if (LinesToAdd.Count > 0)
					{
						int InsertPos = LastSeenIncludeBeforeCode + 1;
						if (NewLines[LastSeenIncludeBeforeCode].Contains(".generated.h", StringComparison.Ordinal))
						{
							--InsertPos;
						}

						NewLines.InsertRange(InsertPos, LinesToAdd);
						LastSeenIncludeBeforeCode += LinesToAdd.Count;

						if (FirstForwardDeclareLineIndex != -1 && FirstForwardDeclareLineIndex > LastSeenIncludeBeforeCode)
						{
							FirstForwardDeclareLineIndex += LinesToAdd.Count;
						}
					}

					if (ForwardDeclarationsToAdd.Count > 0)
					{
						int InsertPos;
						if (FirstForwardDeclareLineIndex == -1)
						{
							InsertPos = LastSeenIncludeBeforeCode + 1;
							if (!String.IsNullOrEmpty(NewLines[InsertPos]))
							{
								NewLines.Insert(InsertPos++, "");
							}
							NewLines.Insert(InsertPos + 1, "");
						}
						else
						{
							InsertPos = FirstForwardDeclareLineIndex;
						}
						NewLines.InsertRange(InsertPos + 1, ForwardDeclarationsToAdd);
					}
				}

				// If file is public, in engine and we have a deprecation tag set we will 
				// add a deprecated include scope at the end of the file (unless scope already exists, then we'll add it inside that)
				string EngineDir = Unreal.EngineDirectory.FullName.Replace('\\', '/');
				if (!(IsPrivate || IsInternal) && Info.File.StartsWith(EngineDir) && !String.IsNullOrEmpty(HeaderDeprecationTag))
				{
					Dictionary<string, string> PrintableToFull = new();
					foreach (IWYUIncludeEntry Seen in Info.IncludesSeenInFile)
					{
						PrintableToFull.TryAdd(Seen.Printable, Seen.Full);
					}

					// Remove the includes in LinesRemoved
					LinesRemoved.RemoveWhere(Line =>
					{
						ReadOnlySpan<char> IncludeSpan = Line.AsSpan(8).TrimStart();
						char LeadingIncludeChar = IncludeSpan[0];
						if (LeadingIncludeChar != '"' && LeadingIncludeChar != '<')
						{
							return false;
						}
						int Index = IncludeSpan.Slice(1).IndexOf(LeadingIncludeChar == '"' ? '"' : '>');
						if (Index == -1)
						{
							return false;
						}
						IncludeSpan = IncludeSpan.Slice(1, Index);
						string? Full;
						if (!PrintableToFull.TryGetValue(IncludeSpan.ToString(), out Full))
						{
							return false;
						}

						return Info.TransitiveIncludes.ContainsKey(Full);
					});

					if (LinesRemoved.Count > 0)
					{
						int IndexOfDeprecateScope = -1;
						string Match = "#if " + HeaderDeprecationTag;
						for (int I = NewLines.Count - 1; I != 0; --I)
						{
							if (NewLines[I] == Match)
							{
								IndexOfDeprecateScope = I + 1;
								break;
							}
						}

						if (IndexOfDeprecateScope == -1)
						{
							NewLines.Add("");
							NewLines.Add(Match);
							IndexOfDeprecateScope = NewLines.Count;
							NewLines.Add("#endif");
						}
						else
						{
							// Scan the already added includes to prevent additional adds.
						}

						NewLines.InsertRange(IndexOfDeprecateScope, LinesRemoved);
					}
				}
				lock (UpdatedFiles)
				{
					UpdatedFiles.Add(new(Info, NewLines));
				}
			});

			// Something went wrong processing code files.
			if (ProcessSuccess == 0)
			{
				return -1;
			}

			Logger.LogInformation($"Parsed {FilesParseCount} and updated {UpdatedFiles.Count} files (Found {OutOfDateCount} .iwyu files out of date)");

			// Wooohoo, all files are up-to-date
			if (UpdatedFiles.Count == 0)
			{
				Logger.LogInformation($"All files are up to date!");
				return 0;
			}

			// If we have been logging we can exit now since we don't want to write any files to disk
			if (ShouldLog != null)
			{
				return 0;
			}

			List<System.Diagnostics.Process> P4Processes = new();

			Action<string> AddP4Process = (Arguments) =>
			{
				System.Diagnostics.Process Process = new System.Diagnostics.Process();
				System.Diagnostics.ProcessStartInfo StartInfo = new System.Diagnostics.ProcessStartInfo();
				Process.StartInfo.WindowStyle = System.Diagnostics.ProcessWindowStyle.Hidden;
				Process.StartInfo.CreateNoWindow = true;
				Process.StartInfo.FileName = "p4.exe";
				Process.StartInfo.Arguments = Arguments;
				Process.Start();
				P4Processes.Add(Process);
			};

			Func<bool> WaitForP4 = () =>
			{
				bool P4Success = true;
				foreach (System.Diagnostics.Process P4Process in P4Processes)
				{
					P4Process.WaitForExit();
					if (P4Process.ExitCode != 0)
					{
						P4Success = false;
						Logger.LogError($"p4 edit failed - {P4Process.StartInfo.Arguments}");
					}
					P4Process.Close();
				}
				P4Processes.Clear();
				return P4Success;
			};

			if (!bNoP4)
			{
				List<IWYUInfo> ReadOnlyFileInfos = new();
				foreach ((IWYUInfo Info, List<string> NewLines) in UpdatedFiles)
				{
					if (new FileInfo(Info.File).IsReadOnly)
					{
						ReadOnlyFileInfos.Add(Info);
					}
				}

				if (ReadOnlyFileInfos.Count > 0)
				{
					// Check out files in batches. This can go quite crazy if there are lots of files.
					// Should probably revisit this code to prevent 100s of p4 processes to start at once
					Logger.LogInformation($"Opening {ReadOnlyFileInfos.Count} files for edit in P4... ({SkippedCount} files skipped)");
					int ShowCount = 8;
					foreach (IWYUInfo Info in ReadOnlyFileInfos)
					{
						Logger.LogInformation($"   edit {Info.File}");
						if (--ShowCount == 0)
						{
							break;
						}
					}
					if (ReadOnlyFileInfos.Count > 5)
					{
						Logger.LogInformation($"   ... and {ReadOnlyFileInfos.Count - 5} more.");
					}

					StringBuilder P4Arguments = new();
					int BatchSize = 10;
					int BatchCount = 0;
					int Index = 0;
					foreach (IWYUInfo Info in ReadOnlyFileInfos)
					{
						if (!SkippedFiles.Contains(Info.Source!))
						{
							P4Arguments.Append(" \"").Append(Info.File).Append('\"');
							++BatchCount;
						}
						++Index;
						if (BatchCount == BatchSize || Index == ReadOnlyFileInfos.Count)
						{
							AddP4Process($"edit{P4Arguments}");
							P4Arguments.Clear();
							BatchCount = 0;
						}
					}

					// Waiting for edit
					if (!WaitForP4())
					{
						return -1;
					}
				}
			}

			bool WriteSuccess = true;
			Logger.LogInformation($"Writing {UpdatedFiles.Count - SkippedCount} files to disk...");

			foreach ((IWYUInfo Info, List<string> NewLines) in UpdatedFiles)
			{
				if (SkippedFiles.Contains(Info.Source!))
				{
					continue;
				}

				try
				{
					File.WriteAllLines(Info.File, NewLines);
				}
				catch (Exception e)
				{
					Logger.LogError($"Failed to write {Info.File}: {e.Message} - File will be reverted");
					SkippedFiles.Add(Info.Source!); // In case other entries from same file is queued

					AddP4Process($"revert {String.Join(' ', Info.Source!.Files.Select(f => f.File))}");
					WriteSuccess = false;
				}
			}

			// Waiting for reverts
			if (!WaitForP4())
			{
				return -1;
			}

			if (!WriteSuccess)
			{
				return -1;
			}

			Logger.LogInformation($"Done!");
			return 0;
		}

		/// <summary>
		/// Calculate all indirect transitive includes for a file. This list contains does not contain itself and will handle circular dependencies
		/// </summary>
		static void CalculateTransitive(IWYUInfo Root, IWYUInfo Info, Stack<string> Stack, Dictionary<string, string> TransitiveIncludes, Dictionary<string, string> TransitiveForwardDeclarations, bool UseSeenIncludes)
		{
			List<IWYUIncludeEntry> Includes = UseSeenIncludes ? Info.IncludesSeenInFile : Info.Includes;
			foreach (IWYUIncludeEntry Include in Includes)
			{
				string Key = Include.Full;

				if (TransitiveIncludes.ContainsKey(Key) || Include.Resolved == Root)
				{
					continue;
				}

				Stack.Push(Include.Printable);
				string TransitivePath = String.Join(" -> ", Stack.Reverse());
				TransitiveIncludes.Add(Key, TransitivePath);
				if (Include.Resolved != null)
				{
					CalculateTransitive(Root, Include.Resolved, Stack, TransitiveIncludes, TransitiveForwardDeclarations, UseSeenIncludes);
				}
				Stack.Pop();
			}

			foreach (IWYUForwardEntry ForwardDeclaration in Info.ForwardDeclarations)
			{
				string Key = ForwardDeclaration.Printable;
				TransitiveForwardDeclarations.TryAdd(Key, Info.File);
			}
		}

		int CompareFiles(Dictionary<string, IWYUInfo> Infos, HashSet<string> ValidPaths, Dictionary<string, UEBuildModule> PathToModule, Dictionary<string, UEBuildModule> NameToModule, ILogger Logger)
		{
			Logger.LogInformation($"Comparing code files...");

			Dictionary<string, IWYUInfo> SkippedHeaders = new();

			Logger.LogInformation($"Parsing seen headers not supporting being compiled...");
			Parallel.ForEach(Infos.Values, Info =>
			{
				foreach (IWYUIncludeEntry Include in Info.IncludesSeenInFile)
				{
					if (Include.Resolved == null)
					{
						if (!Infos.TryGetValue(Include.Full, out Include.Resolved))
						{
							lock (SkippedHeaders)
							{
								if (!SkippedHeaders.TryGetValue(Include.Full, out Include.Resolved))
								{
									IWYUInfo NewInfo = new();
									Include.Resolved = NewInfo;
									NewInfo.File = Include.Full;
									SkippedHeaders.Add(Include.Full, NewInfo);
								}
							}
						}
					}
				}
			});
			ParseSkippedHeaders(SkippedHeaders, Infos, PathToModule, NameToModule);

			//List<ValueTuple<float, IWYUInfo>> RatioList = new();

			ConcurrentDictionary<string, long> AllSeenIncludes = new();

			List<ValueTuple<IWYUInfo, Dictionary<string, string>>> InfosToCompare = new();

			Logger.LogInformation($"Generating transitive seen include lists...");
			Parallel.ForEach(Infos.Values, Info =>
			{
				if (!IsValidForUpdate(Info, ValidPaths, false) || Info.File.EndsWith(".h"))
				{
					return;
				}

				Dictionary<string, string> SeenTransitiveIncludes = new();
				Stack<string> Stack = new();
				CalculateTransitive(Info, Info, Stack, SeenTransitiveIncludes, new Dictionary<string, string>(), true);

				lock (InfosToCompare)
				{
					InfosToCompare.Add(new(Info, SeenTransitiveIncludes));
				}

				foreach (KeyValuePair<string, string> Include in SeenTransitiveIncludes.Union(Info.TransitiveIncludes))
				{
					AllSeenIncludes.TryAdd(Include.Key, 0);
				}

				/*
				if (Info.TransitiveIncludes.Count < SeenTransitiveIncludes.Count)
				{
					float Ratio = (float)Info.TransitiveIncludes.Count / SeenTransitiveIncludes.Count;
					lock (RatioList)
						RatioList.Add(new(Ratio, Info));
				}
				if (SeenTransitiveIncludes.Count < Info.TransitiveIncludes.Count)
					Console.WriteLine($"{Info.File} - Now: {SeenTransitiveIncludes.Count}  Optimized: {Info.TransitiveIncludes.Count}");
				*/
			});

			Dictionary<string, long> FileToSize = new Dictionary<string, long>(AllSeenIncludes);

			Logger.LogInformation($"Reading file sizes...");
			Parallel.ForEach(AllSeenIncludes, Info =>
			{
				FileToSize[Info.Key] = (new FileInfo(Info.Key)).Length;
			});

			Logger.LogInformation($"Calculate total amount of bytes included per file...");
			List<ValueTuple<long, IWYUInfo>> ByteSizeDiffList = new();
			Parallel.ForEach(InfosToCompare, Kvp =>
			{
				long OptimizedSize = 0;
				long SeenSize = 0;
				foreach (string Include in Kvp.Item1.TransitiveIncludes.Keys)
				{
					OptimizedSize += FileToSize[Include];
				}

				foreach (string? Include in Kvp.Item2.Keys)
				{
					SeenSize += FileToSize[Include];
				}

				long Diff = SeenSize - OptimizedSize;
				lock (ByteSizeDiffList)
				{
					ByteSizeDiffList.Add(new(Diff, Kvp.Item1));
				}
			});

			ByteSizeDiffList.Sort((a, b) =>
			{
				if (a.Item1 == b.Item1)
				{
					return 0;
				}

				if (a.Item1 < b.Item1)
				{
					return 1;
				}

				return -1;
			});

			Func<long, string> PrettySize = (Size) =>
			{
				if (Size > 1024 * 1024)
				{
					return (((float)Size) / (1024 * 1024)).ToString("0.00") + "mb";
				}

				if (Size > 1024)
				{
					return (((float)Size) / (1024)).ToString("0.00") + "kb";
				}

				return Size + "b";
			};

			Logger.LogInformation("");
			Logger.LogInformation("Top 20 most reduction in bytes parsed by compiler frontend");
			Logger.LogInformation("----------------------------------------------------------");
			for (int I = 0; I != Math.Min(20, ByteSizeDiffList.Count); ++I)
			{
				long Saved = ByteSizeDiffList[I].Item1;
				IWYUInfo File = ByteSizeDiffList[I].Item2;
				long OptimizedSize = 0;
				foreach (string Include in File.TransitiveIncludes.Keys)
				{
					OptimizedSize += FileToSize[Include];
				}

				float Percent = 100.0f - (((float)OptimizedSize) / (OptimizedSize + Saved) * 100.0f);
				Logger.LogInformation($"{Path.GetFileName(File.File)}   {PrettySize(OptimizedSize + Saved)} -> {PrettySize(OptimizedSize)}  (Saved {PrettySize(Saved)} or {Percent.ToString("0.0")}%)");
			}
			Logger.LogInformation("");

			/*
			RatioList.Sort((a, b) =>
			{
				if (a.Item1 == b.Item1)
					return 0;
				if (a.Item1 < b.Item1)
					return -1;
				return 1;
			});
			*/

			return 0;
		}
	}
}
