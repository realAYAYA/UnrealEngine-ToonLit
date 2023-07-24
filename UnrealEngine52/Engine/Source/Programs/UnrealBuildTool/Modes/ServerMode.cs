// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.Linq;
using System.Text.Json;
using System.Text.Json.Serialization;
using System.Collections.Generic;
using OpenTracing.Util;
using UnrealBuildTool;
using UnrealBuildBase;

namespace UnrealBuildTool
{
	// TODO: Interface for commands/results?
	enum CommandType
	{
		Quit,
		DiscoverTargets,
		SetTarget,
		QueryFile,
		GetBrowseConfiguration,
		GetCompileSettings,
	}

	interface ICommand
	{
		public CommandType Type { get; }
		public long Sequence { get; set; }
	}
	interface ICommandResponse
	{
		public CommandType Type { get; }
		public long Sequence { get; set; }
	}

	struct Command : ICommand
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get; set; }
		public long Sequence { get; set; }
	};

	struct SetTargetCommand : ICommand
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get; set; }
		public long Sequence { get; set; }
		public string Target { get; set; }
		public string Platform { get; set; }
		public string Configuration { get; set; }
	}

	struct QueryFileCommand : ICommand
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get; set; }
		public long Sequence { get; set; }
		public string AbsolutePath { get; set; }
	}

	struct GetBrowseConfigurationCommand : ICommand
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get; set; }
		public long Sequence { get; set; }
		public string? AbsolutePath { get; set; }
	}

	struct GetCompileSettingsCommand : ICommand
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get; set; }
		public long Sequence { get; set; }
		public List<string> AbsolutePaths { get; set; }
	}

	struct DiscoverTargetsResponse : ICommandResponse
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get { return CommandType.DiscoverTargets; } }
		public long Sequence { get; set; }
		public List<string> Configurations { get; set; }
		public List<string> Platforms { get; set; }
		public List<string> Targets { get; set; }

		public string DefaultConfiguration { get; set; }
		public string DefaultPlatform { get; set; }
		public string DefaultTarget { get; set; }
	}

	struct SetTargetResponse : ICommandResponse
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get { return CommandType.SetTarget; } }
		public long Sequence { get; set; }
	}
	struct QueryFileResponse : ICommandResponse 
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get { return CommandType.QueryFile; } }
		public long Sequence { get; set; }
		public bool Found { get; set; }
	}

	struct GetBrowseConfigurationResponse : ICommandResponse
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get { return CommandType.GetBrowseConfiguration; } }
		public long Sequence { get; set; }
		public bool Success { get; set; }
		
		public List<string> Paths { get; set; }
		public string? Standard { get; set; }
		public string? WindowsSdkVersion { get; set; } 
	}

	class CompileSettings
	{
		public List<string> IncludePaths { get; set; } = new();
		public List<string> Defines { get; set; } = new();
		public string? Standard { get; set; }
		public List<string> ForcedIncludes { get; set; } = new();
		public string? CompilerPath { get; set; }
		public List<string> CompilerArgs { get; set; } = new();
		public string? WindowsSdkVersion { get; set; }

	}

	class GetCompileSettingsResponse : ICommandResponse
	{
		[JsonConverter(typeof(JsonStringEnumConverter))]
		public CommandType Type { get { return CommandType.GetCompileSettings; } }
		public long Sequence { get; set; }
		public List<CompileSettings?> Settings { get; set; } = new();
	}

	internal class TargetIntellisenseInfo
	{
		public Dictionary<UEBuildModule, CompileSettings> ModuleToCompileSettings = new();
		public Dictionary<DirectoryReference, UEBuildModule> DirToModule = new();

		public UEBuildModule? FindModuleForFile(FileReference File)
		{
			DirectoryReference? Dir = File.Directory;
			while (Dir != null)
			{
				if (DirToModule.TryGetValue(Dir, out UEBuildModule? Module))
				{
					return Module;
				}
				Dir = Dir.ParentDirectory;
			}
			return null;
		}
		public UEBuildModule? FindModuleForDirectory(DirectoryReference Directory)
		{
			DirectoryReference? Dir = Directory;
			while (Dir != null)
			{
				if (DirToModule.TryGetValue(Dir, out UEBuildModule? Module))
				{
					return Module;
				}
				Dir = Dir.ParentDirectory;
			}
			return null;
		}
	}

	[ToolMode("Server", ToolModeOptions.BuildPlatforms | ToolModeOptions.XmlConfig | ToolModeOptions.UseStartupTraceListener)]
	class ServerMode : ToolMode
	{
		[CommandLine("-LogDirectory=")]
		public DirectoryReference? LogDirectory = null;

		private bool KeepRunning { get; set; } = true;

		private BuildConfiguration BuildConfiguration = new();
		private UEBuildTarget? CurrentTarget = null;
		private TargetIntellisenseInfo? CurrentTargetIntellisenseInfo = null;
		private GetBrowseConfigurationResponse CurrentBrowseConfiguration;

		public override int Execute(CommandLineArguments Arguments, ILogger Logger)
		{
			Arguments.ApplyTo(this);

			if (LogDirectory == null)
			{
				LogDirectory = DirectoryReference.Combine(Unreal.EngineProgramSavedDirectory, "UnrealBuildTool");
			}

			DirectoryReference.CreateDirectory(LogDirectory);

			FileReference LogFile = FileReference.Combine(LogDirectory, "Log_Server.txt");
			Log.AddFileWriter("DefaultLogTraceListener", LogFile);
			Logger.LogInformation("Server mode started");

			XmlConfig.ApplyTo(BuildConfiguration);
			Arguments.ApplyTo(BuildConfiguration);

			ProjectFileGenerator.bGenerateProjectFiles = true;

			while (Arguments.TryGetPositionalArgument(out string? PositionalArg))
			{
				HandleCommand(PositionalArg!, Logger);
			}

			while (KeepRunning)
			{
				var Line = Console.ReadLine();
				if (Line == null) { continue; }
				HandleCommand(Line, Logger);
			}
			return 0;
		}

		private void HandleCommand(string CommandString, ILogger Logger)
		{
			var jsonOptions = new JsonSerializerOptions
			{
				PropertyNameCaseInsensitive = true
			};
			var responseOptions = new JsonSerializerOptions
			{
				PropertyNamingPolicy = JsonNamingPolicy.CamelCase
			};
			try
			{
				Command? MaybeCommand = JsonSerializer.Deserialize<Command>(CommandString, jsonOptions);
				if (MaybeCommand is null)
				{
					Console.Error.WriteLine($"Failed to parse command {CommandString}");
					return;
				}
				Logger.LogInformation($"Got command {CommandString}");
				var command = MaybeCommand.Value;
				switch (command.Type)
				{
					case CommandType.Quit:
						KeepRunning = false;
						break;
					case CommandType.DiscoverTargets:
						{
							var response = Handle_DiscoverTargets(Logger);
							response.Sequence = command.Sequence;
							Console.WriteLine(JsonSerializer.Serialize(response, responseOptions));
						}
						break;
					case CommandType.SetTarget:
						{
							var response = Handle_SetTarget(JsonSerializer.Deserialize<SetTargetCommand>(CommandString, jsonOptions), Logger);
							response.Sequence = command.Sequence;
							Console.WriteLine(JsonSerializer.Serialize(response, responseOptions));
						}
						break;
					case CommandType.QueryFile:
						{
							var response = Handle_QueryFile(JsonSerializer.Deserialize<QueryFileCommand>(CommandString, jsonOptions), Logger);
							response.Sequence = command.Sequence;
							Console.WriteLine(JsonSerializer.Serialize(response, responseOptions));
						}
						break;
					case CommandType.GetBrowseConfiguration:
						{
							var response = Handle_GetBrowseConfiguration(JsonSerializer.Deserialize<GetBrowseConfigurationCommand>(CommandString, jsonOptions), Logger);
							response.Sequence = command.Sequence;
							Console.WriteLine(JsonSerializer.Serialize(response, responseOptions));
						}
						break;
					case CommandType.GetCompileSettings:
						{
							var response = Handle_GetCompileSettings(JsonSerializer.Deserialize<GetCompileSettingsCommand>(CommandString, jsonOptions), Logger);
							response.Sequence = command.Sequence;
							Console.WriteLine(JsonSerializer.Serialize(response, responseOptions));
						}
						break;
				}
			}
			catch
			{
				Console.Error.WriteLine($"Failed to parse command {CommandString}");
			}
		}

		// TODO: make async 
		private DiscoverTargetsResponse Handle_DiscoverTargets(ILogger Logger)
		{
			List<UnrealTargetPlatform> Platforms = new();
			foreach (UnrealTargetPlatform Platform in UnrealTargetPlatform.GetValidPlatforms())
			{
				// Only include desktop platforms for now 
				if (ProjectFileGenerator.IsValidDesktopPlatform(Platform))
				{
					// If there is a build platform present, add it to the SupportedPlatforms list
					UEBuildPlatform? BuildPlatform;
					if (UEBuildPlatform.TryGetBuildPlatform(Platform, out BuildPlatform))
					{
						if (InstalledPlatformInfo.IsValidPlatform(Platform, EProjectType.Code))
						{
							Platforms.Add(Platform);
						}
					}
				}
			}

			List<UnrealTargetConfiguration> AllowedTargetConfigurations = new List<UnrealTargetConfiguration>();
			AllowedTargetConfigurations = Enum.GetValues(typeof(UnrealTargetConfiguration)).Cast<UnrealTargetConfiguration>().ToList();

			List<string> Configurations = new();
			foreach (UnrealTargetConfiguration CurConfiguration in AllowedTargetConfigurations)
			{
				if (CurConfiguration != UnrealTargetConfiguration.Unknown)
				{
					if (InstalledPlatformInfo.IsValidConfiguration(CurConfiguration, EProjectType.Code))
					{
						Configurations.Add(CurConfiguration.ToString());
					}
				}
			}

			List<FileReference> Projects = NativeProjects.EnumerateProjectFiles(Logger).ToList();
			List<FileReference> AllTargetFiles = ProjectFileGenerator.DiscoverTargets(Projects, Logger, null, Platforms, bIncludeEngineSource: true, bIncludeTempTargets: false);
			Projects.RemoveAll(x => !AllTargetFiles.Any(y => y.IsUnderDirectory(x.Directory)));

			List<string> TargetNames = new();
			foreach (FileReference TargetFilePath in AllTargetFiles)
			{
				string TargetName = TargetFilePath.GetFileNameWithoutAnyExtensions();
				TargetNames.Add(TargetName);
			}

			// TODO: Handle 0 target names

			var Reply = new DiscoverTargetsResponse
			{
				Configurations = Configurations,
				Platforms = Platforms.Select(x => x.ToString()).ToList(),
				Targets = TargetNames,

				// TODO: Allow selecting default from project file 
				DefaultTarget = TargetNames.FirstOrDefault(x => x == "UnrealEditor", TargetNames[0]),
				DefaultPlatform = Platforms[0].ToString(),
				DefaultConfiguration = UnrealTargetConfiguration.Development.ToString(),
			};
			return Reply;
		}

		private SetTargetResponse Handle_SetTarget(SetTargetCommand command, ILogger Logger)
		{
			CommandLineArguments Args = new CommandLineArguments(new string[] { command.Target, command.Configuration, command.Platform });
			List<TargetDescriptor> TargetDescriptors = new();

			TargetDescriptor.ParseSingleCommandLine(Args, false, false, false, TargetDescriptors, Logger);
			if (TargetDescriptors.Count == 0)
			{
				// TOOD: Error 
				return new SetTargetResponse { };
			}

			HashSet<string> BrowseConfigurationFolders = new HashSet<string>();
			try
			{
				using (GlobalTracer.Instance.BuildSpan("UEBuildTarget.Create()").StartActive())
				{
					bool bUsePrecompiled = false;
					CurrentTarget = UEBuildTarget.Create(TargetDescriptors[0], false, false, bUsePrecompiled, Logger);
				}

				CurrentTargetIntellisenseInfo = new TargetIntellisenseInfo();
				CurrentBrowseConfiguration = new GetBrowseConfigurationResponse { Success = true };

				// Partially duplicated from UEBuildTarget.Build because we just want to get C++ compile actions without running UHT
				// or generating link actions / full dependency graph 
				CppConfiguration CppConfiguration = UEBuildTarget.GetCppConfiguration(CurrentTarget.Configuration);
				SourceFileMetadataCache MetadataCache = SourceFileMetadataCache.CreateHierarchy(null, Logger);
				CppCompileEnvironment GlobalCompileEnvironment = new CppCompileEnvironment(CurrentTarget.Platform, CppConfiguration, CurrentTarget.Architectures, MetadataCache);
				LinkEnvironment GlobalLinkEnvironment = new LinkEnvironment(GlobalCompileEnvironment.Platform, GlobalCompileEnvironment.Configuration, GlobalCompileEnvironment.Architectures);

				UEToolChain TargetToolChain = CurrentTarget.CreateToolchain(CurrentTarget.Platform);
				TargetToolChain.SetEnvironmentVariables();
				CurrentTarget.SetupGlobalEnvironment(TargetToolChain, GlobalCompileEnvironment, GlobalLinkEnvironment);

				// TODO: For installed builds, filter out all the binaries that aren't in mods
				foreach (UEBuildBinary Binary in CurrentTarget.Binaries)
				{
					HashSet<UEBuildModule> LinkEnvironmentVisitedModules = new HashSet<UEBuildModule>();
					CppCompileEnvironment BinaryCompileEnvironment = Binary.CreateBinaryCompileEnvironment(GlobalCompileEnvironment);
					CurrentBrowseConfiguration.Standard = BinaryCompileEnvironment.CppStandard.ToString();
					CurrentBrowseConfiguration.WindowsSdkVersion = CurrentTarget.Rules.WindowsPlatform.WindowsSdkVersion;

					foreach (UEBuildModuleCPP Module in Binary.Modules.OfType<UEBuildModuleCPP>())
					{
						if (Module.Binary != null && Module.Binary != Binary) 
						{
							continue;
						}

						CppCompileEnvironment ModuleCompileEnvironment = Module.CreateModuleCompileEnvironment(CurrentTarget.Rules, BinaryCompileEnvironment);
						foreach (DirectoryReference Dir in Module.ModuleDirectories)
						{
							BrowseConfigurationFolders.Add(Dir.ToString());
							CurrentTargetIntellisenseInfo.DirToModule.TryAdd(Dir, Module);
						}
						if (Module.GeneratedCodeDirectory != null)
						{
							CurrentTargetIntellisenseInfo.DirToModule.TryAdd(Module.GeneratedCodeDirectory, Module);
						}

						foreach (DirectoryReference Dir in ModuleCompileEnvironment.SystemIncludePaths)
						{
							BrowseConfigurationFolders.Add(Dir.ToString());
						}
						foreach (DirectoryReference Dir in ModuleCompileEnvironment.UserIncludePaths)
						{
							BrowseConfigurationFolders.Add(Dir.ToString());
						}

						CompileSettings Settings = new CompileSettings();
						Settings.IncludePaths.AddRange(ModuleCompileEnvironment.SystemIncludePaths.Select(x => x.ToString()));
						Settings.IncludePaths.AddRange(ModuleCompileEnvironment.UserIncludePaths.Select(x => x.ToString()));
						Settings.Defines = ModuleCompileEnvironment.Definitions;
						Settings.Standard = ModuleCompileEnvironment.CppStandard.ToString();
						Settings.ForcedIncludes = ModuleCompileEnvironment.ForceIncludeFiles.Select(x => x.ToString()).ToList();
						Settings.CompilerPath = TargetToolChain.GetCppCompilerPath()?.ToString();
						Settings.WindowsSdkVersion = CurrentTarget.Rules.WindowsPlatform.WindowsSdkVersion;
						CurrentTargetIntellisenseInfo.ModuleToCompileSettings.Add(Module, Settings);
					}
				}
			}
			catch (Exception e)
			{
				Logger.LogError("Caught exception setting up target: {0}", e);
			}

			CurrentBrowseConfiguration.Paths = BrowseConfigurationFolders.ToList();

			var Reply = new SetTargetResponse
			{
			};
			return Reply;
		}

		private QueryFileResponse Handle_QueryFile(QueryFileCommand Command, ILogger Logger)
		{
			var FailResponse = new QueryFileResponse { Found = false };
			if (CurrentTargetIntellisenseInfo == null )
			{
				return FailResponse;
			}
			try 
			{
				var Path = System.IO.Path.GetFullPath(Command.AbsolutePath); 
				if (CurrentTargetIntellisenseInfo.FindModuleForFile(new FileReference(Path)) != null)
				{
					return new QueryFileResponse { Found = true };
				}
				return FailResponse;
			}
			catch (Exception )
			{
				return FailResponse;
			}
		}
		private GetBrowseConfigurationResponse Handle_GetBrowseConfiguration(GetBrowseConfigurationCommand Command, ILogger Logger)
		{
			GetBrowseConfigurationResponse Response = new GetBrowseConfigurationResponse { Success = false, Paths = new List<string>() };
			if (CurrentTargetIntellisenseInfo == null)
			{
				return Response;
			}
			if (Command.AbsolutePath != null)
			{
				var Path = System.IO.Path.GetFullPath(Command.AbsolutePath); 
				UEBuildModule? Module = CurrentTargetIntellisenseInfo.FindModuleForDirectory(new DirectoryReference(Path));
				if (Module != null)
				{
					Response.Success = true;
					Response.Paths.AddRange(Module.ModuleDirectories.Select(x => x.ToString()));
					if (CurrentTargetIntellisenseInfo.ModuleToCompileSettings.TryGetValue(Module, out var Settings))
					{
						Response.Paths.AddRange(Settings.IncludePaths);
						Response.WindowsSdkVersion = Settings.WindowsSdkVersion;
						Response.Standard = Settings.Standard;
					}
				}
				return Response;
			}

			Response = CurrentBrowseConfiguration;
			return Response;
		}

		private GetCompileSettingsResponse Handle_GetCompileSettings(GetCompileSettingsCommand Command, ILogger Logger)
		{
			GetCompileSettingsResponse FailResponse = new GetCompileSettingsResponse { }; 
			try 
			{
				GetCompileSettingsResponse Response = new GetCompileSettingsResponse { };
				foreach (string AbsolutePath in Command.AbsolutePaths)
				{
					CompileSettings? Settings = null;
					var Path = System.IO.Path.GetFullPath(AbsolutePath); // convert absolute path to dos? 
					UEBuildModule? Module = CurrentTargetIntellisenseInfo!.FindModuleForFile(new FileReference(Path));
					if (Module is not null)
					{
						CurrentTargetIntellisenseInfo!.ModuleToCompileSettings.TryGetValue(Module, out Settings);
					}
					Response.Settings.Add(Settings);
				}
				return Response;
			}
			catch (Exception)
			{
				return FailResponse;
			}
		}
	}
}
