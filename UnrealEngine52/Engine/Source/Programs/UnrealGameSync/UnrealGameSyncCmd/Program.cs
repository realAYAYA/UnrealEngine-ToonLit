// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using Serilog;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.OIDC;
using Microsoft.Extensions.Configuration;
using UnrealGameSync;

namespace UnrealGameSyncCmd
{
	using ILogger = Microsoft.Extensions.Logging.ILogger;

	sealed class UserErrorException : Exception
	{
		public LogEvent Event { get; }
		public int Code { get; }

		public UserErrorException(LogEvent evt)
			: base(evt.ToString())
		{
			this.Event = evt;
			this.Code = 1;
		}

		public UserErrorException(string message, params object[] args)
			: this(LogEvent.Create(LogLevel.Error, message, args))
		{
		}
	}

	public class Program
	{
		static BuildConfig EditorConfig => BuildConfig.Development;

		class CommandInfo
		{
			public string Name { get; }
			public Type Type { get; }
			public string Usage { get; }
			public string Brief { get; }

			public CommandInfo(string name, Type type, string usage, string brief)
			{
				this.Name = name;
				this.Type = type;
				this.Usage = usage;
				this.Brief = brief;
			}
		}

		static CommandInfo[] _commands =
		{
			new CommandInfo("init", typeof(InitCommand),
				"ugs init [stream-path] [-client=..] [-server=..] [-user=..] [-branch=..] [-project=..]",
				"Create a client for the given stream, or initializes an existing client for use by UGS."
			),
			new CommandInfo("switch", typeof(SwitchCommand),
				"ugs switch [project name|project path|stream]",
				"Changes the active project to the one in the workspace with the given name, or switches to a new stream."
			),
			new CommandInfo("changes", typeof(ChangesCommand),
				"ugs changes",
				"List recently submitted changes to the current branch."
			),
			new CommandInfo("config", typeof(ConfigCommand),
				"ugs config",
				"Updates the configuration for the current workspace."
			),
			new CommandInfo("filter", typeof(FilterCommand),
				"ugs filter [-reset] [-include=..] [-exclude=..] [-view=..] [-addview=..] [-removeview=..] [-global]",
				"Displays or updates the workspace or global sync filter"
			),
			new CommandInfo("sync", typeof(SyncCommand),
				"ugs sync [change|'latest'] [-build] [-binaries] [-remove] [-only]",
				"Syncs the current workspace to the given changelist, optionally removing all local state."
			),
			new CommandInfo("clients", typeof(ClientsCommand),
				"ugs clients",
				"Lists all clients suitable for use on the current machine."
			),
			new CommandInfo("run", typeof(RunCommand),
				"ugs run",
				"Runs the editor for the current branch."
			),
			new CommandInfo("build", typeof(BuildCommand),
				"ugs build [id] [-list]",
				"Runs the default build steps for the current project, or a particular step referenced by id."
			),
			new CommandInfo("status", typeof(StatusCommand),
				"ugs status [-update]",
				"Shows the status of the currently synced branch."
			),
			new CommandInfo("login", typeof(LoginCommand),
				"ugs login",
				"Starts a interactive login flow against the configured Identity Provider"
			),
			new CommandInfo("version", typeof(VersionCommand),
				"ugs version",
				"Prints the current application version"
			),
		};

		class CommandContext
		{
			public CommandLineArguments Arguments { get; }
			public ILogger Logger { get; }
			public ILoggerFactory LoggerFactory { get; }
			public GlobalSettingsFile UserSettings { get; }

			public CommandContext(CommandLineArguments arguments, ILogger logger, ILoggerFactory loggerFactory, GlobalSettingsFile userSettings)
			{
				this.Arguments = arguments;
				this.Logger = logger;
				this.LoggerFactory = loggerFactory;
				this.UserSettings = userSettings;
			}
		}

		class ServerOptions
		{
			[CommandLine("-Server=")]
			public string? ServerAndPort { get; set; }

			[CommandLine("-User=")]
			public string? UserName { get; set; }
		}

		class ProjectConfigOptions : ServerOptions
		{
			public void ApplyTo(UserWorkspaceSettings settings)
			{
				if (ServerAndPort != null)
				{
					settings.ServerAndPort = (ServerAndPort.Length == 0) ? null : ServerAndPort;
				}
				if (UserName != null)
				{
					settings.UserName = (UserName.Length == 0) ? null : UserName;
				}
			}
		}

		class ProjectInitOptions : ProjectConfigOptions
		{
			[CommandLine("-Client=")]
			public string? ClientName { get; set; }

			[CommandLine("-Branch=")]
			public string? BranchPath { get; set; }

			[CommandLine("-Project=")]
			public string? ProjectName { get; set; }
		}

		public static async Task<int> Main(string[] rawArgs)
		{
			DirectoryReference globalConfigFolder;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
			{
				globalConfigFolder = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.LocalApplicationData)!, "UnrealGameSync");
			}
			else
			{
				globalConfigFolder = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile)!, ".config", "UnrealGameSync");
			}
			DirectoryReference.CreateDirectory(globalConfigFolder);

			string logName;
			DirectoryReference logFolder;
			if (RuntimeInformation.IsOSPlatform(OSPlatform.OSX))
			{
				logFolder = DirectoryReference.Combine(DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile)!, "Library", "Logs", "Unreal Engine", "UnrealGameSync");
				logName = "UnrealGameSync-.log";
			}
			else if (RuntimeInformation.IsOSPlatform(OSPlatform.Linux))
			{
				logFolder = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.UserProfile)!;
				logName = ".ugs-.log";
			}
			else
			{
				logFolder = globalConfigFolder;
				logName = "UnrealGameSyncCmd-.log";
			}

			Serilog.ILogger serilogLogger = new LoggerConfiguration()
				.Enrich.FromLogContext()
				.WriteTo.Console(Serilog.Events.LogEventLevel.Information, outputTemplate: "{Message:lj}{NewLine}")
				.WriteTo.File(FileReference.Combine(logFolder, logName).FullName, Serilog.Events.LogEventLevel.Debug, rollingInterval: RollingInterval.Day, rollOnFileSizeLimit: true, fileSizeLimitBytes: 20 * 1024 * 1024, retainedFileCountLimit: 10)
				.CreateLogger();

			using ILoggerFactory loggerFactory = new Serilog.Extensions.Logging.SerilogLoggerFactory(serilogLogger, true);
			ILogger logger = loggerFactory.CreateLogger("Main");
			try
			{
				GlobalSettingsFile settings;
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					settings = UserSettings.Create(globalConfigFolder, logger);
				}
				else
				{
					settings = GlobalSettingsFile.Create(FileReference.Combine(globalConfigFolder, "Global.json"));
				}

				CommandLineArguments args = new CommandLineArguments(rawArgs);

				string? commandName;
				if (!args.TryGetPositionalArgument(out commandName))
				{
					PrintHelp();
					return 0;
				}

				CommandInfo? command = _commands.FirstOrDefault(x => x.Name.Equals(commandName, StringComparison.OrdinalIgnoreCase));
				if (command == null)
				{
					logger.LogError($"unknown command '{commandName}'");
					Console.WriteLine();
					PrintHelp();
					return 1;
				}

				Command instance = (Command)Activator.CreateInstance(command.Type)!;
				await instance.ExecuteAsync(new CommandContext(args, logger, loggerFactory, settings));
				return 0;
			}
			catch (UserErrorException ex)
			{
				logger.Log(ex.Event.Level, "{Message}", ex.Event.ToString());
				return ex.Code;
			}
			catch (PerforceException ex)
			{
				logger.LogError(ex, "{Message}", ex.Message);
				return 1;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Unhandled exception.\n{Str}", ex);
				return 1;
			}
		}

		static void PrintHelp()
		{
			Console.WriteLine("Usage:");
			foreach (CommandInfo command in _commands)
			{
				Console.WriteLine();
				ConsoleUtils.WriteLineWithWordWrap(command.Usage, 2, 8);
				ConsoleUtils.WriteLineWithWordWrap(command.Brief, 4, 4);
			}
		}

		public static UserWorkspaceSettings? ReadOptionalUserWorkspaceSettings()
		{
			DirectoryReference? dir = DirectoryReference.GetCurrentDirectory();
			for (; dir != null; dir = dir.ParentDirectory)
			{
				try
				{
					UserWorkspaceSettings? settings;
					if (UserWorkspaceSettings.TryLoad(dir, out settings))
					{
						return settings;
					}
				}
				catch
				{
					// Guard against directories we can't access, eg. /Users/.ugs
				}
			}
			return null;
		}

		public static UserWorkspaceSettings ReadRequiredUserWorkspaceSettings()
		{
			UserWorkspaceSettings? settings = ReadOptionalUserWorkspaceSettings();
			if (settings == null)
			{
				throw new UserErrorException("Unable to find UGS workspace in current directory.");
			}
			return settings;
		}

		public static async Task<UserWorkspaceState> ReadWorkspaceState(IPerforceConnection perforceClient, UserWorkspaceSettings settings, GlobalSettingsFile userSettings, ILogger logger)
		{
			UserWorkspaceState state = userSettings.FindOrAddWorkspaceState(settings, logger);
			if (state.SettingsTimeUtc != settings.LastModifiedTimeUtc)
			{
				logger.LogDebug("Updating state due to modified settings timestamp");
				ProjectInfo info = await ProjectInfo.CreateAsync(perforceClient, settings, CancellationToken.None);
				state.UpdateCachedProjectInfo(info, settings.LastModifiedTimeUtc);
				state.Save(logger);
			}
			return state;
		}

		public static Task<IPerforceConnection> ConnectAsync(string? serverAndPort, string? userName, string? clientName, ILoggerFactory loggerFactory)
		{
			PerforceSettings settings = new PerforceSettings(PerforceSettings.Default);
			settings.ClientName = clientName;
			settings.PreferNativeClient = true;
			if (!String.IsNullOrEmpty(serverAndPort))
			{
				settings.ServerAndPort = serverAndPort;
			}
			if (!String.IsNullOrEmpty(userName))
			{
				settings.UserName = userName;
			}

			return PerforceConnection.CreateAsync(settings, loggerFactory.CreateLogger("Perforce"));
		}

		public static Task<IPerforceConnection> ConnectAsync(UserWorkspaceSettings settings, ILoggerFactory loggerFactory)
		{
			return ConnectAsync(settings.ServerAndPort, settings.UserName, settings.ClientName, loggerFactory);
		}

		static string[] ReadSyncFilter(UserWorkspaceSettings workspaceSettings, GlobalSettingsFile userSettings, ConfigFile projectConfig)
		{
			Dictionary<Guid, WorkspaceSyncCategory> syncCategories = ConfigUtils.GetSyncCategories(projectConfig);
			string[] combinedSyncFilter = GlobalSettingsFile.GetCombinedSyncFilter(syncCategories, userSettings.Global.Filter, workspaceSettings.Filter);

			ConfigSection perforceSection = projectConfig.FindSection("Perforce");
			if (perforceSection != null)
			{
				IEnumerable<string> additionalPaths = perforceSection.GetValues("AdditionalPathsToSync", new string[0]);
				combinedSyncFilter = additionalPaths.Union(combinedSyncFilter).ToArray();
			}

			return combinedSyncFilter;
		}

		static async Task<string> FindProjectPathAsync(IPerforceConnection perforce, string clientName, string branchPath, string? projectName)
		{
			using IPerforceConnection perforceClient = await PerforceConnection.CreateAsync(new PerforceSettings(perforce.Settings) { ClientName = clientName }, perforce.Logger);

			// Find or validate the selected project
			string searchPath;
			if (projectName == null)
			{
				searchPath = $"//{clientName}{branchPath}/*.uprojectdirs";
			}
			else if (projectName.Contains('.'))
			{
				searchPath = $"//{clientName}{branchPath}/{projectName.TrimStart('/')}";
			}
			else
			{
				searchPath = $"//{clientName}{branchPath}/.../{projectName}.uproject";
			}

			List<FStatRecord> projectFileRecords = await perforceClient.FStatAsync(FStatOptions.ClientFileInPerforceSyntax, searchPath).ToListAsync();
			projectFileRecords.RemoveAll(x => x.HeadAction == FileAction.Delete || x.HeadAction == FileAction.MoveDelete);
			projectFileRecords.RemoveAll(x => !x.IsMapped);

			List<string> paths = projectFileRecords.Select(x => PerforceUtils.GetClientRelativePath(x.ClientFile!)).Distinct(StringComparer.Ordinal).ToList();
			if (paths.Count == 0)
			{
				throw new UserErrorException("No project file found matching {SearchPath}", searchPath);
			}
			if (paths.Count > 1)
			{
				throw new UserErrorException("Multiple projects found matching {SearchPath}: {Paths}", searchPath, String.Join(", ", paths));
			}

			return "/" + paths[0];
		}

		abstract class Command
		{
			public abstract Task ExecuteAsync(CommandContext context);
		}

		class InitCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				// Get the positional argument indicating the file to look for
				string? initName;
				context.Arguments.TryGetPositionalArgument(out initName);

				// Get the config settings from the command line
				ProjectInitOptions options = new ProjectInitOptions();
				context.Arguments.ApplyTo(options);
				context.Arguments.CheckAllArgumentsUsed();

				// Get the host name
				using IPerforceConnection perforce = await ConnectAsync(options.ServerAndPort, options.UserName, null, context.LoggerFactory);
				InfoRecord perforceInfo = await perforce.GetInfoAsync(InfoOptions.ShortOutput);
				string hostName = perforceInfo.ClientHost ?? Dns.GetHostName();

				// Create the perforce connection
				if (initName != null)
				{
					await InitNewClientAsync(perforce, context, initName, hostName, options, logger);
				}
				else
				{
					await InitExistingClientAsync(perforce, context, hostName, options, logger);
				}
			}

			async Task InitNewClientAsync(IPerforceConnection perforce, CommandContext context, string streamName, string hostName, ProjectInitOptions options, ILogger logger)
			{
				logger.LogInformation("Checking stream...");

				// Get the given stream
				PerforceResponse<StreamRecord> streamResponse = await perforce.TryGetStreamAsync(streamName, true);
				if (!streamResponse.Succeeded)
				{
					throw new UserErrorException($"Unable to find stream '{streamName}'");
				}
				StreamRecord stream = streamResponse.Data;

				// Get the new directory for the client
				DirectoryReference clientDir = DirectoryReference.Combine(DirectoryReference.GetCurrentDirectory(), stream.Stream.Replace('/', '+'));
				DirectoryReference.CreateDirectory(clientDir);

				// Make up a new client name 
				string clientName = options.ClientName ?? Regex.Replace($"{perforce.Settings.UserName}_{hostName}_{stream.Stream.Trim('/')}", "[^0-9a-zA-Z_.-]", "+");

				// Check there are no existing clients under the current path
				List<ClientsRecord> clients = await FindExistingClients(perforce, hostName, clientDir);
				if (clients.Count > 0)
				{
					if (clients.Count == 1 && clientName.Equals(clients[0].Name, StringComparison.OrdinalIgnoreCase) && clientDir == TryParseRoot(clients[0].Root))
					{
						logger.LogInformation("Reusing existing client for {ClientDir} ({ClientName})", clientDir, options.ClientName);
					}
					else
					{
						throw new UserErrorException("Current directory is already within a Perforce workspace ({ClientName})", clients[0].Name);
					}
				}

				// Create the new client
				ClientRecord client = new ClientRecord(clientName, perforce.Settings.UserName, clientDir.FullName);
				client.Host = hostName;
				client.Stream = stream.Stream;
				client.Options = ClientOptions.Rmdir;
				await perforce.CreateClientAsync(client);

				// Branch root is currently hard-coded at the root
				string branchPath = options.BranchPath ?? String.Empty;
				string projectPath = await FindProjectPathAsync(perforce, clientName, branchPath, options.ProjectName);

				// Create the settings object
				UserWorkspaceSettings settings = new UserWorkspaceSettings();
				settings.RootDir = clientDir;
				settings.Init(perforce.Settings.ServerAndPort, perforce.Settings.UserName, clientName, branchPath, projectPath);
				options.ApplyTo(settings);
				settings.Save(logger);

				logger.LogInformation("Initialized {ClientName} with root at {RootDir}", clientName, clientDir);
			}

			static DirectoryReference? TryParseRoot(string root)
			{
				try
				{
					return new DirectoryReference(root);
				}
				catch
				{
					return null;
				}
			}

			async Task InitExistingClientAsync(IPerforceConnection perforce, CommandContext context, string hostName, ProjectInitOptions options, ILogger logger)
			{
				DirectoryReference currentDir = DirectoryReference.GetCurrentDirectory();

				// Make sure the client name is set
				string? clientName = options.ClientName;
				if (clientName == null)
				{
					List<ClientsRecord> clients = await FindExistingClients(perforce, hostName, currentDir);
					if (clients.Count == 0)
					{
						throw new UserErrorException("Unable to find client for {HostName} under {ClientDir}", hostName, currentDir);
					}
					if (clients.Count > 1)
					{
						throw new UserErrorException("Multiple clients found for {HostName} under {ClientDir}: {ClientList}", hostName, currentDir, String.Join(", ", clients.Select(x => x.Name)));
					}

					clientName = clients[0].Name;
					logger.LogInformation("Found client {ClientName}", clientName);
				}

				// Get the client info
				ClientRecord client = await perforce.GetClientAsync(clientName);
				DirectoryReference clientDir = new DirectoryReference(client.Root);

				// If a project path was specified in local syntax, try to convert it to client-relative syntax
				string? projectName = options.ProjectName;
				if (options.ProjectName != null && options.ProjectName.Contains('.'))
				{
					options.ProjectName = FileReference.Combine(currentDir, options.ProjectName).MakeRelativeTo(clientDir).Replace('\\', '/');
				}

				// Branch root is currently hard-coded at the root
				string branchPath = options.BranchPath ?? String.Empty;
				string projectPath = await FindProjectPathAsync(perforce, clientName, branchPath, projectName);

				// Create the settings object
				UserWorkspaceSettings settings = new UserWorkspaceSettings();
				settings.RootDir = clientDir;
				settings.Init(perforce.Settings.ServerAndPort, perforce.Settings.UserName, clientName, branchPath, projectPath);
				options.ApplyTo(settings);
				settings.Save(logger);

				logger.LogInformation("Initialized workspace at {RootDir} for {ClientProject}", clientDir, settings.ClientProjectPath);
			}

			static async Task<List<ClientsRecord>> FindExistingClients(IPerforceConnection perforce, string hostName, DirectoryReference clientDir)
			{
				List<ClientsRecord> matchingClients = new List<ClientsRecord>();

				List<ClientsRecord> clients = await perforce.GetClientsAsync(ClientsOptions.None, perforce.Settings.UserName);
				foreach (ClientsRecord client in clients)
				{
					if (!String.IsNullOrEmpty(client.Root) && !String.IsNullOrEmpty(client.Host) && String.Compare(hostName, client.Host, StringComparison.OrdinalIgnoreCase) == 0)
					{
						DirectoryReference? rootDir;
						try
						{
							rootDir = new DirectoryReference(client.Root);
						}
						catch
						{
							rootDir = null;
						}

						if (rootDir != null && clientDir.IsUnderDirectory(rootDir))
						{
							matchingClients.Add(client);
						}
					}
				}

				return matchingClients;
			}
		}

		class SyncCommand : Command
		{
			class SyncOptions
			{
				[CommandLine("-Only")]
				public bool SingleChange { get; set; }

				[CommandLine("-Build")]
				public bool Build { get; set; }

				[CommandLine("-Binaries")]
				public bool Binaries { get; set; }

				[CommandLine("-NoGPF", Value = "false")]
				[CommandLine("-NoProjectFiles", Value = "false")]
				public bool ProjectFiles { get; set; } = true;

				[CommandLine("-Clobber")]
				public bool Clobber { get; set; }

				[CommandLine("-Refilter")]
				public bool Refilter { get; set; }
			}

			async Task<bool> IsCodeChangeAsync(IPerforceConnection perforce, int change)
			{
				DescribeRecord describeRecord = await perforce.DescribeAsync(change);
				return IsCodeChange(describeRecord);
			}

			bool IsCodeChange(DescribeRecord describeRecord)
			{
				foreach (DescribeFileRecord file in describeRecord.Files)
				{
					if (PerforceUtils.CodeExtensions.Any(extension => file.DepotFile.EndsWith(extension, StringComparison.OrdinalIgnoreCase)))
					{
						return true;
					}
				}
				return false;
			}

			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;
				context.Arguments.TryGetPositionalArgument(out string? changeString);

				SyncOptions syncOptions = new SyncOptions();
				context.Arguments.ApplyTo(syncOptions);

				context.Arguments.CheckAllArgumentsUsed();

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				using IPerforceConnection perforceClient = await ConnectAsync(settings, context.LoggerFactory);
				UserWorkspaceState state = await ReadWorkspaceState(perforceClient, settings, context.UserSettings, logger);

				changeString ??= "latest";

				ProjectInfo projectInfo = state.CreateProjectInfo();
				UserProjectSettings projectSettings = context.UserSettings.FindOrAddProjectSettings(projectInfo, settings, logger);

				ConfigFile projectConfig = await ConfigUtils.ReadProjectConfigFileAsync(perforceClient, projectInfo, logger, CancellationToken.None);

				bool syncLatest = String.Equals(changeString, "latest", StringComparison.OrdinalIgnoreCase);

				int change;
				if (!int.TryParse(changeString, out change))
				{
					if (syncLatest)
					{
						List<ChangesRecord> changes = await perforceClient.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, $"//{settings.ClientName}/...");
						change = changes[0].Number;
					}
					else
					{
						throw new UserErrorException("Unknown change type for sync '{Change}'", changeString);
					}
				}

				WorkspaceUpdateOptions options = syncOptions.SingleChange? WorkspaceUpdateOptions.SyncSingleChange : WorkspaceUpdateOptions.Sync;
				if (syncOptions.Build)
				{
					options |= WorkspaceUpdateOptions.Build;
				}
				if (syncOptions.ProjectFiles)
				{
					options |= WorkspaceUpdateOptions.GenerateProjectFiles;
				}
				if (syncOptions.Clobber)
				{
					options |= WorkspaceUpdateOptions.Clobber;
				}
				if (syncOptions.Refilter)
				{
					options |= WorkspaceUpdateOptions.Refilter;
				}
				options |= WorkspaceUpdateOptions.RemoveFilteredFiles;

				string[] syncFilter = ReadSyncFilter(settings, context.UserSettings, projectConfig);

				WorkspaceUpdateContext updateContext = new WorkspaceUpdateContext(change, options, BuildConfig.Development, syncFilter, projectSettings.BuildSteps, null);
				if (syncOptions.Binaries)
				{
					List<PerforceArchiveInfo> archives = await PerforceArchive.EnumerateAsync(perforceClient, projectConfig, state.ProjectIdentifier, CancellationToken.None);

					PerforceArchiveInfo? editorArchiveInfo = archives.FirstOrDefault(x => x.Name == IArchiveInfo.EditorArchiveType);
					if (editorArchiveInfo == null)
					{
						throw new UserErrorException("No editor archives found for project");
					}

					KeyValuePair<int, string> revision = editorArchiveInfo.ChangeNumberToFileRevision.LastOrDefault(x => x.Key <= change);
					if (revision.Key == 0)
					{
						throw new UserErrorException($"No editor archives found for CL {change}");
					}

					if (revision.Key < change)
					{
						int lastChange = revision.Key;

						List<ChangesRecord> changeRecords = await perforceClient.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, $"//{settings.ClientName}/...@{revision.Key + 1},{change}");
						foreach (ChangesRecord changeRecord in changeRecords.OrderBy(x => x.Number))
						{
							if (await IsCodeChangeAsync(perforceClient, changeRecord.Number))
							{
								if (syncLatest)
								{
									updateContext.ChangeNumber = lastChange;
								}
								else
								{
									throw new UserErrorException($"No editor binaries found for CL {change} (last archive at CL {revision.Key}, but CL {changeRecord.Number} is a code change)");
								}
								break;
							}
							change = changeRecord.Number;
						}
					}

					updateContext.Options |= WorkspaceUpdateOptions.SyncArchives;
					updateContext.ArchiveTypeToArchive[IArchiveInfo.EditorArchiveType] = Tuple.Create<IArchiveInfo, string>(editorArchiveInfo, revision.Value);
				}

				WorkspaceUpdate update = new WorkspaceUpdate(updateContext);
				(WorkspaceUpdateResult result, string message) = await update.ExecuteAsync(perforceClient.Settings, projectInfo, state, context.Logger, CancellationToken.None);
				if (result == WorkspaceUpdateResult.FilesToClobber)
				{
					logger.LogWarning("The following files are modified in your workspace:");
					foreach (string file in updateContext.ClobberFiles.Keys.OrderBy(x => x))
					{
						logger.LogWarning("  {File}", file);
					}
					logger.LogWarning("Use -Clobber to overwrite");
				}
				else if (result != WorkspaceUpdateResult.Success)
				{
					logger.LogError("{Message} (Result: {Result})", message, result);
				}

				state.SetLastSyncState(result, updateContext, message);
				state.Save(logger);
			}
		}

		class ClientsCommand : Command
		{
			public class ClientsOptions : ServerOptions
			{
			}

			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				ClientsOptions options = context.Arguments.ApplyTo<ClientsOptions>(logger);
				context.Arguments.CheckAllArgumentsUsed();

				using IPerforceConnection perforceClient = await ConnectAsync(options.ServerAndPort, options.UserName, null, context.LoggerFactory);
				InfoRecord info = await perforceClient.GetInfoAsync(InfoOptions.ShortOutput);

				List<ClientsRecord> clients = await perforceClient.GetClientsAsync(EpicGames.Perforce.ClientsOptions.None, perforceClient.Settings.UserName);
				foreach (ClientsRecord client in clients)
				{
					if (String.Equals(info.ClientHost, client.Host, StringComparison.OrdinalIgnoreCase))
					{
						logger.LogInformation("{Client,-50} {Root}", client.Name, client.Root);
					}
				}
			}
		}

		class RunCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				using IPerforceConnection perforceClient = await ConnectAsync(settings, context.LoggerFactory);
				UserWorkspaceState state = await ReadWorkspaceState(perforceClient, settings, context.UserSettings, logger);

				ProjectInfo projectInfo = state.CreateProjectInfo();
				ConfigFile projectConfig = await ConfigUtils.ReadProjectConfigFileAsync(perforceClient, projectInfo, logger, CancellationToken.None);

				FileReference receiptFile = ConfigUtils.GetEditorReceiptFile(projectInfo, projectConfig, EditorConfig);
				logger.LogDebug("Receipt file: {Receipt}", receiptFile);

				if (!ConfigUtils.TryReadEditorReceipt(projectInfo, receiptFile, out TargetReceipt? receipt) || String.IsNullOrEmpty(receipt.Launch))
				{
					throw new UserErrorException("The editor needs to be built before you can run it. (Missing {ReceiptFile}).", receiptFile);
				}
				if (!File.Exists(receipt.Launch))
				{
					throw new UserErrorException("The editor needs to be built before you can run it. (Missing {LaunchFile}).", receipt.Launch);
				}

				List<string> launchArguments = new List<string>();
				if (settings.LocalProjectPath.HasExtension(".uproject"))
				{
					launchArguments.Add($"\"{settings.LocalProjectPath}\"");
				}
				if (EditorConfig == BuildConfig.Debug || EditorConfig == BuildConfig.DebugGame)
				{
					launchArguments.Append(" -debug");
				}
				for (int idx = 0; idx < context.Arguments.Count; idx++)
				{
					if (!context.Arguments.HasBeenUsed(idx))
					{
						launchArguments.Add(context.Arguments[idx]);
					}
				}

				string commandLine = CommandLineArguments.Join(launchArguments);
				logger.LogInformation("Spawning: {LaunchFile} {CommandLine}", CommandLineArguments.Quote(receipt.Launch), commandLine);

				if (!Utility.SpawnProcess(receipt.Launch, commandLine))
				{
					logger.LogError("Unable to spawn {0} {1}", receipt.Launch, launchArguments.ToString());
				}
			}
		}

		class ChangesCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				int count = context.Arguments.GetIntegerOrDefault("-Count=", 10);
				int lineCount = context.Arguments.GetIntegerOrDefault("-Lines=", 3);
				context.Arguments.CheckAllArgumentsUsed(context.Logger);

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				using IPerforceConnection perforceClient = await ConnectAsync(settings, context.LoggerFactory);

				List<ChangesRecord> changes = await perforceClient.GetChangesAsync(ChangesOptions.None, count, ChangeStatus.Submitted, $"//{settings.ClientName}/...");
				foreach(IEnumerable<ChangesRecord> changesBatch in changes.Batch(10))
				{
					List<DescribeRecord> describeRecords = await perforceClient.DescribeAsync(changesBatch.Select(x => x.Number).ToArray());

					logger.LogInformation("  Change    Type     Author          Description");
					foreach (DescribeRecord describeRecord in describeRecords)
					{
						PerforceChangeDetails details = new PerforceChangeDetails(describeRecord);

						string type;
						if (details.ContainsCode)
						{
							if (details.ContainsContent)
							{
								type = "Both";
							}
							else
							{
								type = "Code";
							}
						}
						else
						{
							if (details.ContainsContent)
							{
								type = "Content";
							}
							else
							{
								type = "None";
							}
						}

						string author = StringUtils.Truncate(describeRecord.User, 15);

						List<string> lines = StringUtils.WordWrap(details.Description, Math.Max(ConsoleUtils.WindowWidth - 40, 10)).ToList();
						if (lines.Count == 0)
						{
							lines.Add(String.Empty);
						}

						lineCount = Math.Min(lineCount, lines.Count);

						logger.LogInformation("  {Change,-9} {Type,-8} {Author,-15} {Description}", describeRecord.Number, type, author, lines[0]);
						for (int lineIndex = 1; lineIndex < lineCount; lineIndex++)
						{
							logger.LogInformation("                                     {Description}", lines[lineIndex]);
						}
					}
				}
			}
		}

		class ConfigCommand : Command
		{
			public override Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				if (!context.Arguments.GetUnusedArguments().Any())
				{
					ProcessStartInfo startInfo = new ProcessStartInfo();
					startInfo.FileName = settings.ConfigFile.FullName;
					startInfo.UseShellExecute = true;
					using (Process? editor = Process.Start(startInfo))
					{
						if (editor != null)
						{
							editor.WaitForExit();
						}
					}
				}
				else
				{
					ProjectConfigOptions options = new ProjectConfigOptions();
					context.Arguments.ApplyTo(options);
					context.Arguments.CheckAllArgumentsUsed(context.Logger);

					options.ApplyTo(settings);
					settings.Save(logger);

					logger.LogInformation("Updated {ConfigFile}", settings.ConfigFile);
				}
				
				return Task.CompletedTask;
			}
		}

		class FilterCommand : Command
		{
			class FilterCommandOptions
			{
				[CommandLine("-Reset")]
				public bool Reset = false;

				[CommandLine("-Include=")]
				public List<string> Include { get; set; } = new List<string>();

				[CommandLine("-Exclude=")]
				public List<string> Exclude { get; set; } = new List<string>();

				[CommandLine("-View=", ListSeparator = ';')]
				public List<string>? View { get; set; } 

				[CommandLine("-AddView=", ListSeparator = ';')]
				public List<string> AddView { get; set; } = new List<string>();

				[CommandLine("-RemoveView=", ListSeparator = ';')]
				public List<string> RemoveView { get; set; } = new List<string>();

				[CommandLine("-AllProjects", Value = "true")]
				[CommandLine("-OnlyCurrent", Value = "false")]
				public bool? AllProjects = null;

				[CommandLine("-GpfAllProjects", Value ="true")]
				[CommandLine("-GpfOnlyCurrent", Value = "false")]
				public bool? AllProjectsInSln = null;

				[CommandLine("-Global")]
				public bool Global { get; set; }
			}

			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				UserWorkspaceSettings workspaceSettings = ReadRequiredUserWorkspaceSettings();
				using IPerforceConnection perforceClient = await ConnectAsync(workspaceSettings, context.LoggerFactory);
				UserWorkspaceState workspaceState = await ReadWorkspaceState(perforceClient, workspaceSettings, context.UserSettings, logger);
				ProjectInfo projectInfo = workspaceState.CreateProjectInfo();

				ConfigFile projectConfig = await ConfigUtils.ReadProjectConfigFileAsync(perforceClient, projectInfo, logger, CancellationToken.None);
				Dictionary<Guid, WorkspaceSyncCategory> syncCategories = ConfigUtils.GetSyncCategories(projectConfig);

				FilterSettings globalFilter = context.UserSettings.Global.Filter;
				FilterSettings workspaceFilter = workspaceSettings.Filter;

				FilterCommandOptions options = context.Arguments.ApplyTo<FilterCommandOptions>(logger);
				context.Arguments.CheckAllArgumentsUsed(context.Logger);

				if (options.Global)
				{
					ApplyCommandOptions(context.UserSettings.Global.Filter, options, syncCategories.Values, logger);
					context.UserSettings.Save(logger);
				}
				else
				{
					ApplyCommandOptions(workspaceSettings.Filter, options, syncCategories.Values, logger);
					workspaceSettings.Save(logger);
				}

				Dictionary<Guid, bool> globalCategories = globalFilter.GetCategories();
				Dictionary<Guid, bool> workspaceCategories = workspaceFilter.GetCategories();

				logger.LogInformation("Categories:");
				foreach (WorkspaceSyncCategory syncCategory in syncCategories.Values)
				{
					bool enabled;

					string scope = "(Default)";
					if (globalCategories.TryGetValue(syncCategory.UniqueId, out enabled))
					{
						scope = "(Global)";
					}
					else if (workspaceCategories.TryGetValue(syncCategory.UniqueId, out enabled))
					{
						scope = "(Workspace)";
					}
					else
					{
						enabled = syncCategory.Enable;
					}

					logger.LogInformation("  {Id,30} {Enabled,3} {Scope,-9} {Name}", syncCategory.UniqueId, enabled? "Yes" : "No", scope, syncCategory.Name);
				}

				if (globalFilter.View.Count > 0)
				{
					logger.LogInformation("");
					logger.LogInformation("Global View:");
					foreach (string line in globalFilter.View)
					{
						logger.LogInformation("  {Line}", line);
					}
				}
				if (workspaceFilter.View.Count > 0)
				{
					logger.LogInformation("");
					logger.LogInformation("Workspace View:");
					foreach (string line in workspaceFilter.View)
					{
						logger.LogInformation("  {Line}", line);
					}
				}

				string[] filter = ReadSyncFilter(workspaceSettings, context.UserSettings, projectConfig);

				logger.LogInformation("");
				logger.LogInformation("Combined view:");
				foreach (string filterLine in filter)
				{
					logger.LogInformation("  {FilterLine}", filterLine);
				}
			}

			static void ApplyCommandOptions(FilterSettings settings, FilterCommandOptions commandOptions, IEnumerable<WorkspaceSyncCategory> syncCategories, ILogger logger)
			{
				if (commandOptions.Reset)
				{
					logger.LogInformation("Resetting settings...");
					settings.Reset();
				}

				HashSet<Guid> includeCategories = new HashSet<Guid>(commandOptions.Include.Select(x => GetCategoryId(x, syncCategories)));
				HashSet<Guid> excludeCategories = new HashSet<Guid>(commandOptions.Exclude.Select(x => GetCategoryId(x, syncCategories)));

				Guid id = includeCategories.FirstOrDefault(x => excludeCategories.Contains(x));
				if (id != Guid.Empty)
				{
					throw new UserErrorException("Category {Id} cannot be both included and excluded", id);
				}

				includeCategories.ExceptWith(settings.IncludeCategories);
				settings.IncludeCategories.AddRange(includeCategories);

				excludeCategories.ExceptWith(settings.ExcludeCategories);
				settings.ExcludeCategories.AddRange(excludeCategories);

				if (commandOptions.View != null)
				{
					settings.View = commandOptions.View;
				}
				if (commandOptions.RemoveView.Count > 0)
				{
					HashSet<string> viewRemove = new HashSet<string>(commandOptions.RemoveView, StringComparer.OrdinalIgnoreCase);
					settings.View.RemoveAll(x => viewRemove.Contains(x));
				}
				if (commandOptions.AddView.Count > 0)
				{
					HashSet<string> viewLines = new HashSet<string>(settings.View, StringComparer.OrdinalIgnoreCase);
					settings.View.AddRange(commandOptions.AddView.Where(x => !viewLines.Contains(x)));
				}

				settings.AllProjects = commandOptions.AllProjects ?? settings.AllProjects;
				settings.AllProjectsInSln = commandOptions.AllProjectsInSln ?? settings.AllProjectsInSln;
			}

			static Guid GetCategoryId(string text, IEnumerable<WorkspaceSyncCategory> syncCategories)
			{
				Guid id;
				if (Guid.TryParse(text, out id))
				{
					return id;
				}

				WorkspaceSyncCategory? category = syncCategories.FirstOrDefault(x => x.Name.Equals(text, StringComparison.OrdinalIgnoreCase));
				if (category != null)
				{
					return category.UniqueId;
				}

				throw new UserErrorException("Unable to find category '{Category}'", text);
			}
		}

		class BuildCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;
				context.Arguments.TryGetPositionalArgument(out string? target);
				bool listOnly = context.Arguments.HasOption("-List");
				context.Arguments.CheckAllArgumentsUsed();

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				using IPerforceConnection perforceClient = await ConnectAsync(settings, context.LoggerFactory);
				UserWorkspaceState state = await ReadWorkspaceState(perforceClient, settings, context.UserSettings, logger);

				ProjectInfo projectInfo = state.CreateProjectInfo();

				if (listOnly)
				{
					ConfigFile projectConfig = await ConfigUtils.ReadProjectConfigFileAsync(perforceClient, projectInfo, logger, CancellationToken.None);

					FileReference editorTarget = ConfigUtils.GetEditorTargetFile(projectInfo, projectConfig);

					Dictionary<Guid, ConfigObject> buildStepObjects = ConfigUtils.GetDefaultBuildStepObjects(projectInfo, editorTarget.GetFileNameWithoutAnyExtensions(), EditorConfig, projectConfig, false);

					logger.LogInformation("Available build steps:");
					logger.LogInformation("");
					logger.LogInformation("  Id                                   | Description                              | Type       | Enabled");
					logger.LogInformation("  -------------------------------------|------------------------------------------|------------|-----------------");
					foreach (BuildStep buildStep in buildStepObjects.Values.Select(x => new BuildStep(x)).OrderBy(x => x.OrderIndex))
					{
						logger.LogInformation("  {Id,-36} | {Name,-40} | {Type,-10} | {Enabled,-8}", buildStep.UniqueId, buildStep.Description, buildStep.Type, buildStep.NormalSync);
					}
					return;
				}

				HashSet<Guid>? steps = null;
				if (target != null)
				{
					Guid id;
					if (!Guid.TryParse(target, out id))
					{
						logger.LogError("Unable to parse '{Target}' as a GUID. Pass -List to show all available build steps and their identifiers.", target);
					}
					steps = new HashSet<Guid> { id };
				}

				WorkspaceUpdateContext updateContext = new WorkspaceUpdateContext(state.CurrentChangeNumber, WorkspaceUpdateOptions.Build, BuildConfig.Development, null, new List<ConfigObject>(), steps);

				WorkspaceUpdate update = new WorkspaceUpdate(updateContext);
				await update.ExecuteAsync(perforceClient.Settings, projectInfo, state, context.Logger, CancellationToken.None);
			}
		}

		class StatusCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;
				bool update = context.Arguments.HasOption("-Update");
				context.Arguments.CheckAllArgumentsUsed();

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				logger.LogInformation("User: {UserName}", settings.UserName);
				logger.LogInformation("Server: {ServerAndPort}", settings.ServerAndPort);
				logger.LogInformation("Project: {ClientProjectPath}", settings.ClientProjectPath);

				using IPerforceConnection perforceClient = await ConnectAsync(settings, context.LoggerFactory);

				UserWorkspaceState state = await ReadWorkspaceState(perforceClient, settings, context.UserSettings, logger);
				if (update)
				{
					ProjectInfo newProjectInfo = await ProjectInfo.CreateAsync(perforceClient, settings, CancellationToken.None);
					state.UpdateCachedProjectInfo(newProjectInfo, settings.LastModifiedTimeUtc);
				}

				string streamOrBranchName = state.StreamName ?? settings.BranchPath.TrimStart('/');
				if (state.LastSyncResultMessage == null)
				{
					logger.LogInformation("Not currently synced to {Stream}", streamOrBranchName);
				}
				else if (state.LastSyncResult == WorkspaceUpdateResult.Success)
				{
					logger.LogInformation("Synced to {Stream} CL {Change}", streamOrBranchName, state.LastSyncChangeNumber);
				}
				else
				{
					logger.LogWarning("Last sync to {Stream} CL {Change} failed: {Result}", streamOrBranchName, state.LastSyncChangeNumber, state.LastSyncResultMessage);
				}
			}
		}

		class LoginCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;

				// Get the positional argument indicating the file to look for
				if (!context.Arguments.TryGetPositionalArgument(out string? providerIdentifier))
				{
					throw new UserErrorException("Missing provider identifier to login to.");
				}
				context.Arguments.CheckAllArgumentsUsed();

				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				
				// Find the valid config file paths
				DirectoryInfo engineDir = DirectoryReference.Combine(settings.RootDir, "Engine").ToDirectoryInfo();
				DirectoryInfo gameDir = new DirectoryInfo(settings.ProjectPath);
				using ITokenStore tokenStore = TokenStoreFactory.CreateTokenStore();
				IConfiguration providerConfiguration = ProviderConfigurationFactory.ReadConfiguration(engineDir, gameDir);
				OidcTokenManager oidcTokenManager = OidcTokenManager.CreateTokenManager(providerConfiguration, tokenStore, new List<string>() {providerIdentifier});
				OidcTokenInfo result = await oidcTokenManager.Login(providerIdentifier);

				logger.LogInformation("Logged in to provider {ProviderIdentifier}", providerIdentifier);
			}
		}

		class SwitchCommand : Command
		{
			public override async Task ExecuteAsync(CommandContext context)
			{
				// Get the positional argument indicating the file to look for
				string? targetName;
				if (!context.Arguments.TryGetPositionalArgument(out targetName))
				{
					throw new UserErrorException("Missing stream or project name to switch to.");
				}

				bool force = targetName.StartsWith("//", StringComparison.Ordinal) && context.Arguments.HasOption("-Force");

				// Finish argument parsing
				context.Arguments.CheckAllArgumentsUsed();

				// Get a connection to the client for this workspace
				UserWorkspaceSettings settings = ReadRequiredUserWorkspaceSettings();
				using IPerforceConnection perforceClient = await ConnectAsync(settings, context.LoggerFactory);

				// Check whether we're switching stream or project
				if (targetName.StartsWith("//", StringComparison.Ordinal))
				{
					await SwitchStreamAsync(perforceClient, targetName, force, context.Logger);
				}
				else
				{
					await SwitchProjectAsync(perforceClient, settings, targetName, context.Logger);
				}
			}

			public async Task SwitchStreamAsync(IPerforceConnection perforceClient, string streamName, bool force, ILogger logger)
			{
				if (!force && await perforceClient.OpenedAsync(OpenedOptions.None, FileSpecList.Any).AnyAsync())
				{
					throw new UserErrorException("Client {ClientName} has files opened. Use -Force to switch anyway.", perforceClient.Settings.ClientName!);
				}

				await perforceClient.SwitchClientToStreamAsync(streamName, SwitchClientOptions.IgnoreOpenFiles);

				logger.LogInformation("Switched to stream {StreamName}", streamName);
			}

			public async Task SwitchProjectAsync(IPerforceConnection perforceClient, UserWorkspaceSettings settings, string projectName, ILogger logger)
			{
				settings.ProjectPath = await FindProjectPathAsync(perforceClient, settings.ClientName, settings.BranchPath, projectName);
				settings.Save(logger);
				logger.LogInformation("Switched to project {ProjectPath}", settings.ClientProjectPath);
			}
		}

		class VersionCommand : Command
		{
			public override Task ExecuteAsync(CommandContext context)
			{
				ILogger logger = context.Logger;
 
				AssemblyInformationalVersionAttribute? version = Assembly.GetExecutingAssembly().GetCustomAttribute<AssemblyInformationalVersionAttribute>();
				logger.LogInformation("UnrealGameSync {Version}", version?.InformationalVersion ?? "Unknown");

				return Task.CompletedTask;
			}
		}
	}
}
