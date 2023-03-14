// Copyright Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using EpicGames.Core;
using UnrealBuildBase;
using Microsoft.Extensions.Logging;

namespace UnrealBuildTool
{
	/// <summary>
	/// Describes all of the information needed to initialize a UEBuildTarget object
	/// </summary>
	class TargetDescriptor
	{
		public static string TEST_TARGETS_SUFFIX = "Tests";

		public FileReference? ProjectFile;
		public string Name;
		public UnrealTargetPlatform Platform;
		public UnrealTargetConfiguration Configuration;
		public string Architecture;
		public CommandLineArguments AdditionalArguments;
		public bool IsTestsTarget = false;

		public static string GetTestedTargetName(string Name)
		{
			if (Name.EndsWith(TEST_TARGETS_SUFFIX))
			{
				return Name.Substring(0, Name.Length - TEST_TARGETS_SUFFIX.Length);
			}
			return Name;
		}

		/// <summary>
		/// Foreign plugin to compile against this target
		/// </summary>
		[CommandLine("-Plugin=")]
		public FileReference? ForeignPlugin = null;

		/// <summary>
		/// Set of module names to compile.
		/// </summary>
		[CommandLine("-Module=")]
		public HashSet<string> OnlyModuleNames = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Lists of files to compile
		/// </summary>
		[CommandLine("-FileList=")]
		public List<FileReference> FileLists = new List<FileReference>(); 

		/// <summary>
		/// Individual file(s) to compile
		/// </summary>
		[CommandLine("-File=")]
		[CommandLine("-SingleFile=")]
		public List<FileReference> SpecificFilesToCompile = new List<FileReference>();

		/// <summary>
		/// Individual files to compile which may or may not be part of the target. This file set is built from a header scan, and may include files
		/// not part of the target by design.
		/// </summary>
		public List<FileReference> OptionalFilesToCompile = new List<FileReference>();

		/// <summary>
		/// Whether to perform hot reload for this target
		/// </summary>
		[CommandLine("-NoHotReload", Value = nameof(HotReloadMode.Disabled))]
		[CommandLine("-ForceHotReload", Value = nameof(HotReloadMode.FromIDE))]
		[CommandLine("-LiveCoding", Value = nameof(HotReloadMode.LiveCoding))]
		public HotReloadMode HotReloadMode = HotReloadMode.Default;

		/// <summary>
		/// Map of module name to suffix for hot reloading from the editor
		/// </summary>
		public Dictionary<string, int> HotReloadModuleNameToSuffix = new Dictionary<string, int>(StringComparer.OrdinalIgnoreCase);

		/// <summary>
		/// Path to a file containing a list of modules that may be modified for live coding.
		/// </summary>
		[CommandLine("-LiveCodingModules=")]
		public FileReference? LiveCodingModules = null;

		/// <summary>
		/// Path to the manifest for passing info about the output to live coding
		/// </summary>
		[CommandLine("-LiveCodingManifest=")]
		public FileReference? LiveCodingManifest = null;

		/// <summary>
		/// If a non-zero value, a live coding request will be terminated if more than the given number of actions are required.
		/// </summary>
		[CommandLine("-LiveCodingLimit=")]
		public uint LiveCodingLimit = 0;

		/// <summary>
		/// Suppress messages about building this target
		/// </summary>
		[CommandLine("-Quiet")]
		public bool bQuiet;

		/// <summary>
		/// Clean the target before trying to build it
		/// </summary>
		[CommandLine("-Rebuild")]
		public bool bRebuild;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="ProjectFile">Path to the project file</param>
		/// <param name="TargetName">Name of the target to build</param>
		/// <param name="Platform">Platform to build for</param>
		/// <param name="Configuration">Configuration to build</param>
		/// <param name="Architecture">Architecture to build for</param>
		/// <param name="Arguments">Other command-line arguments for the target</param>
		public TargetDescriptor(FileReference? ProjectFile, string TargetName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string Architecture, CommandLineArguments? Arguments)
		{
			this.ProjectFile = ProjectFile;
			this.Name = TargetName;
			this.Platform = Platform;
			this.Configuration = Configuration;
			this.Architecture = Architecture;

			// If there are any additional command line arguments
			List<string> AdditionalArguments = new List<string>();
			if(Arguments != null)
			{
				// Apply the arguments to this object
				Arguments.ApplyTo(this);

				// Read the file lists
				foreach (FileReference FileList in FileLists)
				{
					string[] Files = FileReference.ReadAllLines(FileList);
					foreach (string File in Files)
					{
						if (!String.IsNullOrWhiteSpace(File))
						{
							SpecificFilesToCompile.Add(FileReference.Combine(Unreal.RootDirectory, File));
						}
					}
				}

				// Parse all the hot-reload module names
				foreach(string ModuleWithSuffix in Arguments.GetValues("-ModuleWithSuffix="))
				{
					int SuffixIdx = ModuleWithSuffix.LastIndexOf(',');
					if(SuffixIdx == -1)
					{
						throw new BuildException("Missing suffix argument from -ModuleWithSuffix=Name,Suffix");
					}

					string ModuleName = ModuleWithSuffix.Substring(0, SuffixIdx);

					int Suffix;
					if(!Int32.TryParse(ModuleWithSuffix.Substring(SuffixIdx + 1), out Suffix))
					{
						throw new BuildException("Suffix for modules must be an integer");
					}

					HotReloadModuleNameToSuffix[ModuleName] = Suffix;
				}

				// Pull out all the arguments that haven't been used so far
				for(int Idx = 0; Idx < Arguments.Count; Idx++)
				{
					if(!Arguments.HasBeenUsed(Idx))
					{
						AdditionalArguments.Add(Arguments[Idx]);
					}
				}
			}
			this.AdditionalArguments = new CommandLineArguments(AdditionalArguments.ToArray());
		}

		public TargetDescriptor Copy()
		{
			return (TargetDescriptor)this.MemberwiseClone();
		}

		public static TargetDescriptor FromTargetInfo(TargetInfo Info)
		{
			return new TargetDescriptor(Info.ProjectFile, Info.Name, Info.Platform, Info.Configuration, Info.Architecture, Info.Arguments);
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine distribution</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling rules assemblies</param>
		/// <param name="bForceRulesCompile">Whether to always compile all rules assemblies</param>
		/// <param name="Logger">Logger for output</param>
		/// <returns>List of target descriptors</returns>
		public static List<TargetDescriptor> ParseCommandLine(CommandLineArguments Arguments, bool bUsePrecompiled, bool bSkipRulesCompile, bool bForceRulesCompile, ILogger Logger)
		{
			List<TargetDescriptor> TargetDescriptors = new List<TargetDescriptor>();
			ParseCommandLine(Arguments, bUsePrecompiled, bSkipRulesCompile, bForceRulesCompile, TargetDescriptors, Logger);
			return TargetDescriptors;
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine distribution</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling rules assemblies</param>
		/// <param name="bForceRulesCompile">Whether to always compile rules assemblies</param>
		/// <param name="TargetDescriptors">Receives the list of parsed target descriptors</param>
		/// <param name="Logger">Logger for output</param>
		public static void ParseCommandLine(CommandLineArguments Arguments, bool bUsePrecompiled, bool bSkipRulesCompile, bool bForceRulesCompile, List<TargetDescriptor> TargetDescriptors, ILogger Logger)
		{
			List<string> TargetLists;
			Arguments = Arguments.Remove("-TargetList=", out TargetLists);

			List<string> Targets;
			Arguments = Arguments.Remove("-Target=", out Targets);

			if(TargetLists.Count > 0 || Targets.Count > 0)
			{
				// Try to parse multiple arguments from a single command line
				foreach(string TargetList in TargetLists)
				{
					string[] Lines = File.ReadAllLines(TargetList);
					foreach(string Line in Lines)
					{
						string TrimLine = Line.Trim();
						if(TrimLine.Length > 0 && TrimLine[0] != ';')
						{
							CommandLineArguments NewArguments = Arguments.Append(CommandLineArguments.Split(TrimLine));
							ParseCommandLine(NewArguments, bUsePrecompiled, bSkipRulesCompile, bForceRulesCompile, TargetDescriptors, Logger);
						}
					}
				}

				foreach(string Target in Targets)
				{
					CommandLineArguments NewArguments = Arguments.Append(CommandLineArguments.Split(Target));
					ParseCommandLine(NewArguments, bUsePrecompiled, bSkipRulesCompile, bForceRulesCompile, TargetDescriptors, Logger);
				}
			}
			else
			{
				// Otherwise just process the whole command line together
				ParseSingleCommandLine(Arguments, bUsePrecompiled, bSkipRulesCompile, bForceRulesCompile, TargetDescriptors, Logger);
			}
		}

		/// <summary>
		/// Parse a list of target descriptors from the command line
		/// </summary>
		/// <param name="Arguments">Command-line arguments</param>
		/// <param name="bUsePrecompiled">Whether to use a precompiled engine distribution</param>
		/// <param name="bSkipRulesCompile">Whether to skip compiling rules assemblies</param>
		/// <param name="bForceRulesCompile">Whether to always compile all rules assemblies</param>
		/// <param name="TargetDescriptors">List of target descriptors</param>
		/// <param name="Logger">Logger for output</param>
		public static void ParseSingleCommandLine(CommandLineArguments Arguments, bool bUsePrecompiled, bool bSkipRulesCompile, bool bForceRulesCompile, List<TargetDescriptor> TargetDescriptors, ILogger Logger)
		{
			List<UnrealTargetPlatform> Platforms = new List<UnrealTargetPlatform>();
			List<UnrealTargetConfiguration> Configurations = new List<UnrealTargetConfiguration>();
			List<string> TargetNames = new List<string>();
			FileReference? ProjectFile = Arguments.GetFileReferenceOrDefault("-Project=", null);

			// Settings for creating/using static libraries for the engine
			for (int ArgumentIndex = 0; ArgumentIndex < Arguments.Count; ArgumentIndex++)
			{
				string Argument = Arguments[ArgumentIndex];
				if(Argument.Length > 0 && Argument[0] != '-')
				{
					// Mark this argument as used. We'll interpret it as one thing or another.
					Arguments.MarkAsUsed(ArgumentIndex);

					// Check if it's a project file argument
					if(Argument.EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
					{
						FileReference NewProjectFile = new FileReference(Argument);
						if(ProjectFile != null && ProjectFile != NewProjectFile)
						{
							throw new BuildException("Multiple project files specified on command line (first {0}, then {1})", ProjectFile, NewProjectFile);
						}
						ProjectFile = new FileReference(Argument);
						continue;
					}

					// Split it into separate arguments
					string[] InlineArguments = Argument.Split('+');

					// Try to parse them as platforms
					UnrealTargetPlatform ParsedPlatform;
					if(UnrealTargetPlatform.TryParse(InlineArguments[0], out ParsedPlatform))
					{
						Platforms.Add(ParsedPlatform);
						for(int InlineArgumentIdx = 1; InlineArgumentIdx < InlineArguments.Length; InlineArgumentIdx++)
						{
							Platforms.Add(UnrealTargetPlatform.Parse(InlineArguments[InlineArgumentIdx]));
						}
						continue;
					}

					// Try to parse them as configurations
					UnrealTargetConfiguration ParsedConfiguration;
					if(Enum.TryParse(InlineArguments[0], true, out ParsedConfiguration))
					{
						Configurations.Add(ParsedConfiguration);
						for(int InlineArgumentIdx = 1; InlineArgumentIdx < InlineArguments.Length; InlineArgumentIdx++)
						{
							string InlineArgument = InlineArguments[InlineArgumentIdx];
							if(!Enum.TryParse(InlineArgument, true, out ParsedConfiguration))
							{
								throw new BuildException("Invalid configuration '{0}'", InlineArgument);
							}
							Configurations.Add(ParsedConfiguration);
						}
						continue;
					}

					// Otherwise assume they are target names
					TargetNames.AddRange(InlineArguments);
				}
			}

			if (Platforms.Count == 0)
			{
				throw new BuildException("No platforms specified for target");
			}
			if (Configurations.Count == 0)
			{
				throw new BuildException("No configurations specified for target");
			}

			// Make sure the project file exists, and make sure we're using the correct case.
			if(ProjectFile != null)
			{
				FileInfo ProjectFileInfo = FileUtils.FindCorrectCase(ProjectFile.ToFileInfo());
				if(!ProjectFileInfo.Exists)
				{
					throw new BuildException("Unable to find project '{0}'.", ProjectFile);
				}
				ProjectFile = new FileReference(ProjectFileInfo);
			}

			// Expand all the platforms, architectures and configurations
			foreach(UnrealTargetPlatform Platform in Platforms)
			{
				// Make sure the platform is valid
				if (!InstalledPlatformInfo.IsValid(null, Platform, null, EProjectType.Code, InstalledPlatformState.Downloaded))
				{
					if (!InstalledPlatformInfo.IsValid(null, Platform, null, EProjectType.Code, InstalledPlatformState.Supported))
					{
						throw new BuildException("The {0} platform is not supported from this engine distribution.", Platform);
					}
					else
					{
						throw new BuildException("Missing files required to build {0} targets. Enable {0} as an optional download component in the Epic Games Launcher.", Platform);
					}
				}

				// Parse the architecture parameter, or get the default for the platform
				List<string> Architectures = new List<string>(Arguments.GetValues("-Architecture=", '+'));

				if (Architectures.Count == 0)
				{
					Architectures.Add(UEBuildPlatform.GetBuildPlatform(Platform).GetDefaultArchitecture(ProjectFile));
				}
				else
				{
					// If the platform can do these in a single pass then turn them back into a single + separate string.
					// It is now responsible for splitting them as necessary.
					if (UEBuildPlatform.GetBuildPlatform(Platform).CanBuildArchitecturesInSinglePass(Architectures))
					{ 
						Architectures = new List<string> { string.Join("+", Architectures.OrderBy(S => S)) };
					}
				}				

				foreach(string Architecture in Architectures)
				{
					foreach(UnrealTargetConfiguration Configuration in Configurations)
					{
						// Create all the target descriptors for targets specified by type
						foreach(string TargetTypeString in Arguments.GetValues("-TargetType="))
						{
							TargetType TargetType;
							if(!Enum.TryParse(TargetTypeString, out TargetType))
							{
								throw new BuildException("Invalid target type '{0}'", TargetTypeString);
							}

							if (ProjectFile == null)
							{
								TargetNames.Add(RulesCompiler.CreateEngineRulesAssembly(bUsePrecompiled, bSkipRulesCompile, bForceRulesCompile, Logger).GetTargetNameByType(TargetType, Platform, Configuration, Architecture, null, Logger));
							}
							else
							{
								TargetNames.Add(RulesCompiler.CreateProjectRulesAssembly(ProjectFile, bUsePrecompiled, bSkipRulesCompile, bForceRulesCompile, Logger).GetTargetNameByType(TargetType, Platform, Configuration, Architecture, ProjectFile, Logger));
							}
						}

						// Make sure we could parse something
						if (TargetNames.Count == 0)
						{
							throw new BuildException("No target name was specified on the command-line.");
						}

						// Create all the target descriptors
						foreach(string TargetName in TargetNames)
						{
							// If a project file was not specified see if we can find one
							if (ProjectFile == null && NativeProjects.TryGetProjectForTarget(TargetName, Logger, out ProjectFile))
							{
								Logger.LogDebug("Found project file for {TargetName} - {ProjectFile}", TargetName, ProjectFile);
							}

							// Create the target descriptor
							TargetDescriptors.Add(new TargetDescriptor(ProjectFile, TargetName, Platform, Configuration, Architecture, Arguments));
						}
					}
				}
			}
		}

		/// <summary>
		/// Try to parse the project file from the command line
		/// </summary>
		/// <param name="Arguments">The command line arguments</param>
		/// <param name="ProjectFile">The project file that was parsed</param>
		/// <returns>True if the project file was parsed, false otherwise</returns>
		public static bool TryParseProjectFileArgument(CommandLineArguments Arguments, [NotNullWhen(true)] out FileReference? ProjectFile)
		{
			FileReference? ExplicitProjectFile;
			if(Arguments.TryGetValue("-Project=", out ExplicitProjectFile))
			{
				ProjectFile = ExplicitProjectFile;
				return true;
			}

			for(int Idx = 0; Idx < Arguments.Count; Idx++)
			{
				if(Arguments[Idx][0] != '-' && Arguments[Idx].EndsWith(".uproject", StringComparison.OrdinalIgnoreCase))
				{
					Arguments.MarkAsUsed(Idx);
					ProjectFile = new FileReference(Arguments[Idx]);
					return true;
				}
			}

			if(UnrealBuildTool.IsProjectInstalled())
			{
				ProjectFile = UnrealBuildTool.GetInstalledProjectFile()!;
				return true;
			}

			ProjectFile = null;
			return false;
		}

		/// <summary>
		/// Format this object for the debugger
		/// </summary>
		/// <returns>String representation of this target descriptor</returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			Result.AppendFormat("{0} {1} {2}", Name, Platform, Configuration);
			if(!String.IsNullOrEmpty(Architecture))
			{
				Result.AppendFormat(" -Architecture={0}", Architecture);
			}
			if(ProjectFile != null)
			{
				Result.AppendFormat(" -Project={0}", Utils.MakePathSafeToUseWithCommandLine(ProjectFile));
			}
			if(AdditionalArguments != null && AdditionalArguments.Count > 0)
			{
				Result.AppendFormat(" {0}", AdditionalArguments);
			}
			return Result.ToString();
		}

		public override int GetHashCode()
		{
#pragma warning disable RS1024
			return String.GetHashCode(ProjectFile?.FullName) + 
				Name.GetHashCode() + 
				Platform.GetHashCode() + 
				Configuration.GetHashCode() + 
				Architecture.GetHashCode();
#pragma warning restore RE1024
		}

		public override bool Equals(object? Obj) 
		{
			TargetDescriptor? Other = Obj as TargetDescriptor;
			if (Other != null)
			{
				return
					ProjectFile == Other.ProjectFile &&
					Name == Other.Name &&
					Platform == Other.Platform &&
					Configuration == Other.Configuration &&
					Architecture == Other.Architecture;
			}
			return false;
		}
	}
}
