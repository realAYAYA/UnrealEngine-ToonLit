// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json.Serialization;
using System.Text.RegularExpressions;

namespace UnrealGameSync
{
	public enum BuildConfig
	{
		Debug,
		DebugGame,
		Development,
	}

	public enum TabLabels
	{
		Stream,
		WorkspaceName,
		WorkspaceRoot,
		ProjectFile,
	}

	public enum BisectState
	{
		Include,
		Exclude,
		Pass,
		Fail,
	}

	public enum UserSelectedProjectType
	{
		Client,
		Local
	}

	public enum FilterType
	{
		None,
		Code,
		Content
	}

	/// <summary>
	/// Config specified class to determine what is the Latest Change to Sync
	/// Can be configured using badges, good and starred CLs
	/// </summary>
	public class LatestChangeType
	{
		// Display Specifiers
		public string Name = ""; // Name will the saved ID for this LatestChangeType
		public string Description = "";
		public int OrderIndex = Int32.MaxValue;

		// What Rules to Check for.
		public bool Good = false;
		public bool Starred = false;
		public bool FindNewestGoodContent = false;
		public List<string> RequiredBadges = new List<string>();

		public static bool TryParseConfigEntry(string text, [NotNullWhen(true)] out LatestChangeType? changeType)
		{
			ConfigObject definitionObject = new ConfigObject(text);

			string latestChangeTypeName = definitionObject.GetValue("Name", "");
			if (latestChangeTypeName != "")
			{
				changeType = new LatestChangeType();
				changeType.Name = latestChangeTypeName;
				changeType.Description = definitionObject.GetValue("Description", "No \"Description\" for LatestChangeType given.");
				changeType.OrderIndex = definitionObject.GetValue("OrderIndex", Int32.MaxValue);

				changeType.Good = definitionObject.GetValue("bGood", false);
				changeType.Starred = definitionObject.GetValue("bStarred", false);
				changeType.FindNewestGoodContent = definitionObject.GetValue("bFindNewestGoodContent", false);
				changeType.RequiredBadges = new List<string>(definitionObject.GetValue("RequiredBadges", "").Split(new char[] { ',',' '}, StringSplitOptions.RemoveEmptyEntries));
				return true;
			}
			else
			{
				changeType = null;
			}

			return false;
		}

		// Default Template for Latest Change
		public static LatestChangeType LatestChange()
		{
			LatestChangeType latest = new LatestChangeType();
			latest.Name = "LatestChange";
			latest.Description = "Latest Change";
			latest.OrderIndex = -3;
			latest.FindNewestGoodContent = true;

			return latest;
		}

		// Default Template for Latest Good Change
		public static LatestChangeType LatestGoodChange()
		{
			LatestChangeType latestGood = new LatestChangeType();
			latestGood.Name = "LatestGoodChange";
			latestGood.Description = "Latest Good Change";
			latestGood.OrderIndex = -2;
			latestGood.Good = true;
			latestGood.FindNewestGoodContent = true;

			return latestGood;
		}

		// Default Template for Latest Starred Change
		public static LatestChangeType LatestStarredChange()
		{
			LatestChangeType latestStarred = new LatestChangeType();
			latestStarred.Name = "LatestStarredChange";
			latestStarred.Description = "Latest Starred Change";
			latestStarred.OrderIndex = -1;
			latestStarred.Starred = true;
			latestStarred.FindNewestGoodContent = true;

			return latestStarred;
		}
	}

	public enum UserSettingsVersion
	{
		Initial = 0,
		DefaultServerSettings = 1,
		XgeShaderCompilation = 2,
		DefaultNumberOfThreads = 3,
		Latest = DefaultNumberOfThreads
	}

	public class ArchiveSettings
	{
		public bool Enabled;
		public string Type;
		public List<string> Order;

		public ArchiveSettings(bool enabled, string type, IEnumerable<string> order)
		{
			this.Enabled = enabled;
			this.Type = type;
			this.Order = new List<string>(order);
		}

		public static bool TryParseConfigEntry(string text, [NotNullWhen(true)] out ArchiveSettings? settings)
		{
			ConfigObject obj = new ConfigObject(text);

			string? type = obj.GetValue("Type", null);
			if (type == null)
			{
				settings = null;
				return false;
			}
			else
			{
				string[] order = obj.GetValue("Order", "").Split(new char[] { ';' }, StringSplitOptions.RemoveEmptyEntries);
				bool enabled = obj.GetValue("Enabled", 0) != 0;

				settings = new ArchiveSettings(enabled, type, order);
				return true;
			}
		}

		public string ToConfigEntry()
		{
			ConfigObject obj = new ConfigObject();

			obj.SetValue("Enabled", Enabled ? 1 : 0);
			obj.SetValue("Type", Type);
			obj.SetValue("Order", String.Join(";", Order));

			return obj.ToString();
		}

		public override string ToString()
		{
			return ToConfigEntry();
		}
	}

	public class UserSelectedProjectSettings
	{
		public string? ServerAndPort { get; set; }
		public string? UserName { get; set; }
		public UserSelectedProjectType Type { get; set; }
		public string? ClientPath { get; set; }
		public string? LocalPath { get; set; }
		public string? ScheduledSyncTypeId { get; set; }

		public UserSelectedProjectSettings(string? serverAndPort, string? userName, UserSelectedProjectType type, string? clientPath, string? localPath)
		{
			this.ServerAndPort = serverAndPort;
			this.UserName = userName;
			this.Type = type;
			this.ClientPath = clientPath;
			this.LocalPath = localPath;
		}

		public static bool TryParseConfigEntry(string text, [NotNullWhen(true)] out UserSelectedProjectSettings? project)
		{
			ConfigObject obj = new ConfigObject(text);

			UserSelectedProjectType type;
			if(Enum.TryParse(obj.GetValue("Type", ""), out type))
			{
				string? serverAndPort = obj.GetValue("ServerAndPort", null);
				if(String.IsNullOrWhiteSpace(serverAndPort))
				{
					serverAndPort = null;
				}

				// Fixup for code that was saving server host name rather than DNS entry
				if(serverAndPort != null && serverAndPort.Equals("p4-nodeb.epicgames.net:1666", StringComparison.OrdinalIgnoreCase))
				{
					serverAndPort = "perforce:1666";
				}

				string? userName = obj.GetValue("UserName", null);
				if(String.IsNullOrWhiteSpace(userName))
				{
					userName = null;
				}

				string? localPath = obj.GetValue("LocalPath", null);
				if(String.IsNullOrWhiteSpace(localPath))
				{
					localPath = null;
				}

				string? clientPath = obj.GetValue("ClientPath", null);
				if(String.IsNullOrWhiteSpace(clientPath))
				{
					clientPath = null;
				}

				string? scheduledSyncTypeId = obj.GetValue("ScheduledSyncTypeID", null);
				if (String.IsNullOrWhiteSpace(scheduledSyncTypeId))
				{
					scheduledSyncTypeId = null;
				}

				if ((type == UserSelectedProjectType.Client && clientPath != null) || (type == UserSelectedProjectType.Local && localPath != null))
				{
					project = new UserSelectedProjectSettings(serverAndPort, userName, type, clientPath, localPath);
					project.ScheduledSyncTypeId = scheduledSyncTypeId;
					return true;
				}
			}

			project = null;
			return false;
		}

		public string ToConfigEntry()
		{
			ConfigObject obj = new ConfigObject();

			if(ServerAndPort != null)
			{
				obj.SetValue("ServerAndPort", ServerAndPort);
			}
			if(UserName != null)
			{
				obj.SetValue("UserName", UserName);
			}

			obj.SetValue("Type", Type.ToString());

			if(ClientPath != null)
			{
				obj.SetValue("ClientPath", ClientPath);
			}
			if(LocalPath != null)
			{
				obj.SetValue("LocalPath", LocalPath);
			}
			if (ScheduledSyncTypeId != null)
			{
				obj.SetValue("ScheduledSyncTypeID", ScheduledSyncTypeId);
			}

			return obj.ToString();
		}

		public override string? ToString()
		{
			return LocalPath ?? ClientPath;
		}
	}

	public class SyncCategory
	{
		public Guid Id { get; set; }
		public bool Enable { get; set; }
	}

	public class BisectEntry
	{
		public int Change { get; set; }
		public BisectState State { get; set; }
	}

	public class UserWorkspaceSettings
	{
		[JsonIgnore]
		public DirectoryReference RootDir { get; set; } = null!;

		[JsonIgnore]
		public long LastModifiedTimeUtc { get; set; }

		// Connection settings
		public string? ServerAndPort { get; set; }
		public string? UserName { get; set; }
		public string ClientName { get; set; } = String.Empty;

		// Path to the root of the branch within this client, with a trailing slash if non-empty
		public string BranchPath { get; set; } = String.Empty;

		// The currently selected project, relative to the root directory
		public string ProjectPath { get; set; } = String.Empty;

		// Workspace specific SyncFilters
		public FilterSettings Filter { get; set; } = new FilterSettings();

		[JsonIgnore]
		public FileReference ConfigFile => GetConfigFile(RootDir);

		[JsonIgnore]
		public string ClientProjectPath => $"//{ClientName}{BranchPath}{ProjectPath}";

		[JsonIgnore]
		public FileReference LocalProjectPath => new FileReference(RootDir.FullName + ProjectPath);

		public void Init(string? serverAndPort, string? userName, string clientName, string branchPath, string projectPath)
		{
			ProjectInfo.ValidateBranchPath(branchPath);
			ProjectInfo.ValidateProjectPath(projectPath);

			this.ServerAndPort = serverAndPort;
			this.UserName = userName;
			this.ClientName = clientName;
			this.BranchPath = branchPath;
			this.ProjectPath = projectPath;
		}

		public static bool TryLoad(DirectoryReference rootDir, [NotNullWhen(true)] out UserWorkspaceSettings? settings)
		{
			FileReference configFile = GetConfigFile(rootDir);
			if (Utility.TryLoadJson(configFile, out settings))
			{
				settings.RootDir = rootDir;
				settings.LastModifiedTimeUtc = FileReference.GetLastWriteTimeUtc(configFile).Ticks;
				return true;
			}
			else
			{
				settings = null;
				return false;
			}
		}

		static object _syncRoot = new object();

		public bool Save(ILogger logger)
		{
			try
			{
				SaveInternal();
				return true;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Unable to save {ConfigFile}: {Message}", ConfigFile, ex.Message);
				return false;
			}
		}

		private void SaveInternal()
		{
			lock (_syncRoot)
			{
				UserSettings.CreateConfigDir(ConfigFile.Directory);
				Utility.SaveJson(ConfigFile, this);
				LastModifiedTimeUtc = FileReference.GetLastWriteTimeUtc(ConfigFile).Ticks;
			}
		}

		public static FileReference GetConfigFile(DirectoryReference rootDir)
		{
			return FileReference.Combine(UserSettings.GetConfigDir(rootDir), "settings.json");
		}
	}

	public class UserWorkspaceState
	{
		[JsonIgnore]
		public DirectoryReference RootDir { get; set; } = null!;

		// Cached state about the project, configured using UserWorkspaceSettings and taken from computed values in ProjectInfo. Assumed valid unless manually updated.
		public long SettingsTimeUtc { get; set; }
		public string ClientName { get; set; } = String.Empty;
		public string BranchPath { get; set; } = String.Empty;
		public string ProjectPath { get; set; } = String.Empty;
		public string? StreamName { get; set; }
		public string ProjectIdentifier { get; set; } = String.Empty; // Should be fully reset if this changes
		public bool IsEnterpriseProject { get; set; }

		// Settings for the currently synced project in this workspace. CurrentChangeNumber is only valid for this workspace if CurrentProjectPath is the current project.
		public int CurrentChangeNumber { get; set; } = -1;
		public int CurrentCodeChangeNumber { get; set; } = -1;
		public string? CurrentSyncFilterHash { get; set; }
		public List<int> AdditionalChangeNumbers { get; set; } = new List<int>();

		// Settings for the last attempted sync. These values are set to persist error messages between runs.
		public int LastSyncChangeNumber { get; set; }
		public WorkspaceUpdateResult LastSyncResult { get; set; }
		public string? LastSyncResultMessage { get; set; }
		public DateTime? LastSyncTime { get; set; }
		public int LastSyncDurationSeconds { get; set; }

		// The last successful build, regardless of whether a failed sync has happened in the meantime. Used to determine whether to force a clean due to entries in the project config file.
		public int LastBuiltChangeNumber { get; set; }

		// Expanded archives in the workspace
		public string[]? ExpandedArchiveTypes { get; set; }

		// The changes that we're regressing at the moment
		public List<BisectEntry> BisectChanges { get; set; } = new List<BisectEntry>();

		// Path to the config file
		[JsonIgnore]
		public FileReference ConfigFile => GetConfigFile(RootDir);

		public static UserWorkspaceState CreateNew(DirectoryReference rootDir)
		{
			UserWorkspaceState state = new UserWorkspaceState();
			state.RootDir = rootDir;
			return state;
		}

		public ProjectInfo CreateProjectInfo()
		{
			return new ProjectInfo(RootDir, ClientName, BranchPath, ProjectPath, StreamName, ProjectIdentifier, IsEnterpriseProject);
		}

		public void UpdateCachedProjectInfo(ProjectInfo projectInfo, long settingsTimeUtc)
		{
			this.SettingsTimeUtc = settingsTimeUtc;

			RootDir = projectInfo.LocalRootPath;
			ClientName = projectInfo.ClientName;
			BranchPath = projectInfo.BranchPath;
			ProjectPath = projectInfo.ProjectPath;
			StreamName = projectInfo.StreamName;
			ProjectIdentifier = projectInfo.ProjectIdentifier;
			IsEnterpriseProject = projectInfo.IsEnterpriseProject;
		}

		public bool IsValid(ProjectInfo projectInfo)
		{
			return ProjectIdentifier.Equals(projectInfo.ProjectIdentifier, StringComparison.OrdinalIgnoreCase);
		}

		public void SetBisectState(int change, BisectState state)
		{
			BisectEntry entry = BisectChanges.FirstOrDefault(x => x.Change == change);
			if (entry == null)
			{
				entry = new BisectEntry();
				entry.Change = change;
				BisectChanges.Add(entry);
			}
			entry.State = state;
		}

		public static bool TryLoad(DirectoryReference rootDir, [NotNullWhen(true)] out UserWorkspaceState? state)
		{
			FileReference configFile = GetConfigFile(rootDir);
			if (Utility.TryLoadJson(configFile, out state))
			{
				state.RootDir = rootDir;
				return true;
			}
			else
			{
				state = null;
				return false;
			}
		}

		public void SetLastSyncState(WorkspaceUpdateResult result, WorkspaceUpdateContext context, string statusMessage)
		{
			LastSyncChangeNumber = context.ChangeNumber;
			LastSyncResult = result;
			LastSyncResultMessage = statusMessage;
			LastSyncTime = DateTime.UtcNow;
			LastSyncDurationSeconds = (int)(LastSyncTime.Value - context.StartTime).TotalSeconds;
		}

		static object _syncRoot = new object();

		public bool Save(ILogger logger)
		{
			try
			{
				SaveInternal();
				return true;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Unable to save {ConfigFile}: {Message}", ConfigFile, ex.Message);
				return false;
			}
		}

		private void SaveInternal()
		{
			lock (_syncRoot)
			{
				UserSettings.CreateConfigDir(ConfigFile.Directory);
				Utility.SaveJson(ConfigFile, this);
			}
		}

		public static FileReference GetConfigFile(DirectoryReference rootDir) => FileReference.Combine(UserSettings.GetConfigDir(rootDir), "state.json");
	}

	public class UserProjectSettings
	{
		[JsonIgnore]
		public FileReference ConfigFile { get; private set; } = null!;

		public List<ConfigObject> BuildSteps { get; set; } = new List<ConfigObject>();
		public FilterType FilterType { get; set; }
		public HashSet<string> FilterBadges { get; set; } = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

		static object _syncRoot = new object();

		private UserProjectSettings()
		{
		}

		public UserProjectSettings(FileReference configFile)
		{
			this.ConfigFile = configFile;
		}

		public static bool TryLoad(FileReference configFile, [NotNullWhen(true)] out UserProjectSettings? settings)
		{
			if (FileReference.Exists(configFile))
			{
				settings = Utility.LoadJson<UserProjectSettings>(configFile);
				settings.ConfigFile = configFile;
				return true;
			}

			settings = null;
			return false;
		}

		public bool Save(ILogger logger)
		{
			try
			{
				SaveInternal();
				return true;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Unable to save {ConfigFile}: {Message}", ConfigFile, ex.Message);
				return false;
			}
		}
		
		void SaveInternal()
		{
			lock (_syncRoot)
			{
				Utility.SaveJson(ConfigFile, this);
			}
		}
	}

	public class UserSettings : GlobalSettingsFile
	{
		/// <summary>
		/// Enum that decribes which robomerge changes to show
		/// </summary>
		public enum RobomergeShowChangesOption
		{
			All,		// Show all changes from robomerge
			Badged,		// Show only robomerge changes that have an associated badge
			None		// Show no robomerge changes
		};

		FileReference _fileName;
		ConfigFile _configFile;

		// General settings
		public UserSettingsVersion Version = UserSettingsVersion.Latest;
		public bool BuildAfterSync;
		public bool RunAfterSync;
		public bool OpenSolutionAfterSync;
		public bool ShowLogWindow;
		public bool AutoResolveConflicts;
		public bool ShowUnreviewedChanges;
		public bool ShowAutomatedChanges;
		public RobomergeShowChangesOption ShowRobomerge;
		public bool AnnotateRobmergeChanges;
		public bool ShowLocalTimes;
		public bool KeepInTray;
		public Guid[] EnabledTools;
		public int FilterIndex;
		public UserSelectedProjectSettings? LastProject;
		public List<UserSelectedProjectSettings> OpenProjects;
		public List<UserSelectedProjectSettings> RecentProjects;
		public string SyncTypeId;
		public BuildConfig CompiledEditorBuildConfig; // NB: This assumes not using precompiled editor. See CurrentBuildConfig.
		public TabLabels TabLabels;

		// Precompiled binaries
		public List<ArchiveSettings> Archives = new List<ArchiveSettings>();

		// OIDC Settings
		public Dictionary<string, string> ProviderToRefreshTokens = new Dictionary<string, string>();

		// Window settings
		public bool WindowVisible;
		public string WindowState;
		public Rectangle? WindowBounds;
		
		// Schedule settings
		public bool ScheduleEnabled;
		public TimeSpan ScheduleTime;
		public bool ScheduleAnyOpenProject;
		public List<UserSelectedProjectSettings> ScheduleProjects;

		// Run configuration
		public List<Tuple<string, bool>> EditorArguments = new List<Tuple<string,bool>>();
		public bool EditorArgumentsPrompt;

		// Notification settings
		public List<string> NotifyProjects;
		public int NotifyUnassignedMinutes;
		public int NotifyUnacknowledgedMinutes;
		public int NotifyUnresolvedMinutes;

		// Project settings
		Dictionary<DirectoryReference, UserWorkspaceState> _workspaceDirToState = new Dictionary<DirectoryReference, UserWorkspaceState>();
		Dictionary<DirectoryReference, UserWorkspaceSettings> _workspaceDirToSettings = new Dictionary<DirectoryReference, UserWorkspaceSettings>();
		Dictionary<FileReference, UserProjectSettings> _projectKeyToSettings = new Dictionary<FileReference, UserProjectSettings>();

		// Perforce settings
		public PerforceSyncOptions SyncOptions = new PerforceSyncOptions();

		private List<UserSelectedProjectSettings> ReadProjectList(string settingName, string legacySettingName)
		{
			List<UserSelectedProjectSettings> projects = new List<UserSelectedProjectSettings>();

			string[]? projectStrings = _configFile.GetValues(settingName, null);
			if(projectStrings != null)
			{
				foreach(string projectString in projectStrings)
				{
					UserSelectedProjectSettings? project;
					if(UserSelectedProjectSettings.TryParseConfigEntry(projectString, out project))
					{
						projects.Add(project);
					}
				}
			}
			else if(legacySettingName != null)
			{
				string[]? legacyProjectStrings = _configFile.GetValues(legacySettingName, null);
				if(legacyProjectStrings != null)
				{
					foreach(string legacyProjectString in legacyProjectStrings)
					{
						if(!String.IsNullOrWhiteSpace(legacyProjectString))
						{
							projects.Add(new UserSelectedProjectSettings(null, null, UserSelectedProjectType.Local, null, legacyProjectString));
						}
					}
				}
			}

			return projects;
		}

		public static UserSettings Create(DirectoryReference settingsDir, ILogger logger)
		{
			return Create(FileReference.Combine(settingsDir, "UnrealGameSyncV2.ini"), FileReference.Combine(settingsDir, "UnrealGameSync.ini"), FileReference.Combine(settingsDir, "Global.json"), logger);
		}

		public static UserSettings Create(FileReference fileName, FileReference legacyFileName, FileReference coreFileName, ILogger logger)
		{
			ConfigFile configFile = new ConfigFile();
			if (FileReference.Exists(fileName))
			{
				configFile.TryLoad(fileName, logger);
			}
			else
			{
				configFile.TryLoad(legacyFileName, logger);
			}

			GlobalSettings? coreSettingsData = null;
			if (FileReference.Exists(coreFileName))
			{
				try
				{
					coreSettingsData = Utility.LoadJson<GlobalSettings>(coreFileName);
				}
				catch (Exception ex)
				{
					logger.LogError(ex, "Error while reading {File}.", coreFileName);
				}
			}

			if (coreSettingsData == null)
			{
				coreSettingsData = new GlobalSettings();
				coreSettingsData.Filter.View = configFile.GetValues("General.SyncFilter", new string[0]).ToList();
				coreSettingsData.Filter.SetCategories(GetCategorySettings(configFile.FindSection("General"), "SyncIncludedCategories", "SyncExcludedCategories"));
				coreSettingsData.Filter.AllProjects = configFile.GetValue("General.SyncAllProjects", false);
				coreSettingsData.Filter.AllProjectsInSln = configFile.GetValue("General.IncludeAllProjectsInSolution", false);
			}

			return new UserSettings(fileName, configFile, coreFileName, coreSettingsData);
		}

		public UserSettings(FileReference inFileName, ConfigFile inConfigFile, FileReference inCoreFileName, GlobalSettings inCoreSettingsData)
			: base(inCoreFileName, inCoreSettingsData)
		{
			this._fileName = inFileName;
			this._configFile = inConfigFile;

			// General settings
			Version = (UserSettingsVersion)_configFile.GetValue("General.Version", (int)UserSettingsVersion.Initial);
			BuildAfterSync = (_configFile.GetValue("General.BuildAfterSync", "1") != "0");
			RunAfterSync = (_configFile.GetValue("General.RunAfterSync", "1") != "0");
			bool syncPrecompiledEditor = (_configFile.GetValue("General.SyncPrecompiledEditor", "0") != "0");
			OpenSolutionAfterSync = (_configFile.GetValue("General.OpenSolutionAfterSync", "0") != "0");
			ShowLogWindow = (_configFile.GetValue("General.ShowLogWindow", false));
			AutoResolveConflicts = (_configFile.GetValue("General.AutoResolveConflicts", "1") != "0");
			ShowUnreviewedChanges = _configFile.GetValue("General.ShowUnreviewed", true);
			ShowAutomatedChanges = _configFile.GetValue("General.ShowAutomated", false);

			// safely parse the filter enum
			ShowRobomerge = RobomergeShowChangesOption.All;
			Enum.TryParse(_configFile.GetValue("General.RobomergeFilter", ""), out ShowRobomerge);

			AnnotateRobmergeChanges = _configFile.GetValue("General.AnnotateRobomerge", true);
			ShowLocalTimes = _configFile.GetValue("General.ShowLocalTimes", false);
			KeepInTray = _configFile.GetValue("General.KeepInTray", true);

			List<Guid> enabledTools = _configFile.GetGuidValues("General.EnabledTools", new Guid[0]).ToList();
			if (_configFile.GetValue("General.EnableP4VExtensions", false))
			{
				enabledTools.Add(new Guid("963850A0-BF63-4E0E-B903-1C5954C7DCF8"));
			}
			if (_configFile.GetValue("General.EnableUshell", false))
			{
				enabledTools.Add(new Guid("922EED87-E732-464C-92DC-5A8F7ED955E2"));
			}
			this.EnabledTools = enabledTools.ToArray();

			int.TryParse(_configFile.GetValue("General.FilterIndex", "0"), out FilterIndex);

			string? lastProjectString = _configFile.GetValue("General.LastProject", null);
			if(lastProjectString != null)
			{
				UserSelectedProjectSettings.TryParseConfigEntry(lastProjectString, out LastProject);
			}
			else
			{
				string? lastProjectFileName = _configFile.GetValue("General.LastProjectFileName", null);
				if(lastProjectFileName != null)
				{
					LastProject = new UserSelectedProjectSettings(null, null, UserSelectedProjectType.Local, null, lastProjectFileName);
				}
			}

			OpenProjects = ReadProjectList("General.OpenProjects", "General.OpenProjectFileNames");
			RecentProjects = ReadProjectList("General.RecentProjects", "General.OtherProjectFileNames");
			SyncTypeId = _configFile.GetValue("General.SyncTypeID", "");
			string? oldSyncTye = _configFile.GetValue("General.SyncType", null);
			if (oldSyncTye != null)
			{
				if (oldSyncTye == "Any")
				{
					SyncTypeId = LatestChangeType.LatestChange().Name;
				}
				else if (oldSyncTye == "Good")
				{
					SyncTypeId = LatestChangeType.LatestGoodChange().Name;
				}
				else if (oldSyncTye == "Starred")
				{
					SyncTypeId = LatestChangeType.LatestStarredChange().Name;
				}
			}

			// Build configuration
			string compiledEditorBuildConfigName = _configFile.GetValue("General.BuildConfig", "");
			if(!Enum.TryParse(compiledEditorBuildConfigName, true, out CompiledEditorBuildConfig))
			{
				CompiledEditorBuildConfig = BuildConfig.DebugGame;
			}

			// Tab names
			string tabLabelsValue = _configFile.GetValue("General.TabLabels", "");
			if(!Enum.TryParse(tabLabelsValue, true, out TabLabels))
			{
				TabLabels = TabLabels.Stream;
			}

			// Editor arguments
			string[] arguments = _configFile.GetValues("General.EditorArguments", new string[]{ "0:-log", "0:-fastload" });
			if (Version < UserSettingsVersion.XgeShaderCompilation)
			{
				arguments = Enumerable.Concat(arguments, new string[] { "0:-noxgeshadercompile" }).ToArray();
			}
			foreach(string argument in arguments)
			{
				if(argument.StartsWith("0:"))
				{
					EditorArguments.Add(new Tuple<string,bool>(argument.Substring(2), false));
				}
				else if(argument.StartsWith("1:"))
				{
					EditorArguments.Add(new Tuple<string,bool>(argument.Substring(2), true));
				}
				else
				{
					EditorArguments.Add(new Tuple<string,bool>(argument, true));
				}
			}
			EditorArgumentsPrompt = _configFile.GetValue("General.EditorArgumentsPrompt", false);

			// Precompiled binaries
			string[] archiveValues = _configFile.GetValues("PrecompiledBinaries.Archives", new string[0]);
			foreach (string archiveValue in archiveValues)
			{
				ArchiveSettings? settings;
				if (ArchiveSettings.TryParseConfigEntry(archiveValue, out settings))
				{
					Archives.Add(settings);
				}
			}

			if (syncPrecompiledEditor)
			{
				Archives.Add(new ArchiveSettings(true, "Editor", new string[0]));
			}

			// OIDC Settings
			string[] tokens = _configFile.GetValues("OIDCProviders.Tokens", new string[0]);
			foreach (string tokenValue in tokens)
			{
				ConfigObject o = new ConfigObject(tokenValue);
				string? provider = o.GetValue("Provider");
				string? token = o.GetValue("Token");
				if (provider != null && token != null)
				{
					ProviderToRefreshTokens.TryAdd(provider, token);
				}
			}

			// Window settings
			WindowVisible = _configFile.GetValue("Window.Visible", true);
			WindowState = _configFile.GetValue("Window.State", "");
			WindowBounds = ParseRectangleValue(_configFile.GetValue("Window.Bounds", ""));

			// Schedule settings
			ScheduleEnabled = _configFile.GetValue("Schedule.Enabled", false);
			if(!TimeSpan.TryParse(_configFile.GetValue("Schedule.Time", ""), out ScheduleTime))
			{
				ScheduleTime = new TimeSpan(6, 0, 0);
			}

			ScheduleAnyOpenProject = _configFile.GetValue("Schedule.AnyOpenProject", true);
			ScheduleProjects = ReadProjectList("Schedule.Projects", "Schedule.ProjectFileNames");

			// Notification settings
			NotifyProjects = _configFile.GetValues("Notifications.NotifyProjects", new string[0]).ToList();
			NotifyUnassignedMinutes = _configFile.GetValue("Notifications.NotifyUnassignedMinutes", -1);
			NotifyUnacknowledgedMinutes = _configFile.GetValue("Notifications.NotifyUnacknowledgedMinutes", -1);
			NotifyUnresolvedMinutes = _configFile.GetValue("Notifications.NotifyUnresolvedMinutes", -1);

			// Perforce settings
			if(!int.TryParse(_configFile.GetValue("Perforce.NumRetries", "0"), out SyncOptions.NumRetries))
			{
				SyncOptions.NumRetries = 0;
			}

			int numThreads;
			if(int.TryParse(_configFile.GetValue("Perforce.NumThreads", "0"), out numThreads) && numThreads > 0)
			{
				if(Version >= UserSettingsVersion.DefaultNumberOfThreads || numThreads > 1)
				{
					SyncOptions.NumThreads = numThreads;
				}
			}

			SyncOptions.TcpBufferSize = _configFile.GetValue("Perforce.TcpBufferSize", PerforceSyncOptions.DefaultTcpBufferSize);
			SyncOptions.FileBufferSize = _configFile.GetValue("Perforce.FileBufferSize", PerforceSyncOptions.DefaultFileBufferSize);
			SyncOptions.MaxCommandsPerBatch = _configFile.GetValue("Perforce.MaxCommandsPerBatch", PerforceSyncOptions.DefaultMaxCommandsPerBatch);
			SyncOptions.MaxSizePerBatch = _configFile.GetValue("Perforce.MaxSizePerBatch", PerforceSyncOptions.DefaultMaxSizePerBatch);
			SyncOptions.NumSyncErrorRetries = _configFile.GetValue("Perforce.NumSyncErrorRetries", PerforceSyncOptions.DefaultNumSyncErrorRetries);
		}

		static Dictionary<Guid, bool> GetCategorySettings(ConfigSection section, string includedKey, string excludedKey)
		{
			Dictionary<Guid, bool> result = new Dictionary<Guid, bool>();
			if (section != null)
			{
				foreach (Guid uniqueId in section.GetValues(includedKey, new Guid[0]))
				{
					result[uniqueId] = true;
				}
				foreach (Guid uniqueId in section.GetValues(excludedKey, new Guid[0]))
				{
					result[uniqueId] = false;
				}
			}
			return result;
		}

		static void SetCategorySettings(ConfigSection section, string includedKey, string excludedKey, Dictionary<Guid, bool> categories)
		{
			Guid[] includedCategories = categories.Where(x => x.Value).Select(x => x.Key).ToArray();
			if (includedCategories.Length > 0)
			{
				section.SetValues(includedKey, includedCategories);
			}

			Guid[] excludedCategories = categories.Where(x => !x.Value).Select(x => x.Key).ToArray();
			if (excludedCategories.Length > 0)
			{
				section.SetValues(excludedKey, excludedCategories);
			}
		}

		static Rectangle? ParseRectangleValue(string text)
		{
			ConfigObject obj = new ConfigObject(text);

			int x = obj.GetValue("X", -1);
			int y = obj.GetValue("Y", -1);
			int w = obj.GetValue("W", -1);
			int h = obj.GetValue("H", -1);

			if(x == -1 || y == -1 || w == -1 || h == -1)
			{
				return null;
			}
			else
			{
				return new Rectangle(x, y, w, h);
			}
		}

		static string FormatRectangleValue(Rectangle value)
		{
			ConfigObject obj = new ConfigObject();

			obj.SetValue("X", value.X);
			obj.SetValue("Y", value.Y);
			obj.SetValue("W", value.Width);
			obj.SetValue("H", value.Height);

			return obj.ToString();
		}

		public static DirectoryReference GetConfigDir(DirectoryReference workspaceDir)
		{
			DirectoryReference configDir = DirectoryReference.Combine(workspaceDir, ".ugs");
			return configDir;
		}

		public static void CreateConfigDir(DirectoryReference configDir)
		{
			DirectoryInfo configDirInfo = configDir.ToDirectoryInfo();
			if (!configDirInfo.Exists)
			{
				configDirInfo.Create();
				if (RuntimeInformation.IsOSPlatform(OSPlatform.Windows))
				{
					configDirInfo.Attributes = FileAttributes.Directory | FileAttributes.Hidden;
				}
			}
		}

		protected override void ImportWorkspaceState(DirectoryReference rootDir, string clientName, string branchPath, UserWorkspaceState currentWorkspace)
		{
			// Read the workspace settings
			ConfigSection workspaceSection = _configFile.FindSection(clientName + branchPath);
			if(workspaceSection == null)
			{
				string legacyBranchAndClientKey = clientName + branchPath;

				int slashIdx = legacyBranchAndClientKey.IndexOf('/');
				if(slashIdx != -1)
				{
					legacyBranchAndClientKey = legacyBranchAndClientKey.Substring(0, slashIdx) + "$" + legacyBranchAndClientKey.Substring(slashIdx + 1);
				}

				string? currentSync = _configFile.GetValue("Clients." + legacyBranchAndClientKey, null);
				if(currentSync != null)
				{
					int atIdx = currentSync.LastIndexOf('@');
					if(atIdx != -1)
					{
						int changeNumber;
						if(int.TryParse(currentSync.Substring(atIdx + 1), out changeNumber))
						{
							currentWorkspace.ProjectIdentifier = currentSync.Substring(0, atIdx);
							currentWorkspace.CurrentChangeNumber = changeNumber;
						}
					}
				}

				string? lastUpdateResultText = _configFile.GetValue("Clients." + legacyBranchAndClientKey + "$LastUpdate", null);
				if(lastUpdateResultText != null)
				{
					int colonIdx = lastUpdateResultText.LastIndexOf(':');
					if(colonIdx != -1)
					{
						int changeNumber;
						if(int.TryParse(lastUpdateResultText.Substring(0, colonIdx), out changeNumber))
						{
							WorkspaceUpdateResult result;
							if(Enum.TryParse(lastUpdateResultText.Substring(colonIdx + 1), out result))
							{
								currentWorkspace.LastSyncChangeNumber = changeNumber;
								currentWorkspace.LastSyncResult = result;
							}
						}
					}
				}
			}
			else
			{
				currentWorkspace.ProjectIdentifier = workspaceSection.GetValue("CurrentProjectPath", "");
				currentWorkspace.CurrentChangeNumber = workspaceSection.GetValue("CurrentChangeNumber", -1);
				currentWorkspace.CurrentSyncFilterHash = workspaceSection.GetValue("CurrentSyncFilterHash", null);
				foreach(string additionalChangeNumberString in workspaceSection.GetValues("AdditionalChangeNumbers", new string[0]))
				{
					int additionalChangeNumber;
					if(int.TryParse(additionalChangeNumberString, out additionalChangeNumber))
					{
						currentWorkspace.AdditionalChangeNumbers.Add(additionalChangeNumber);
					}
				}

				WorkspaceUpdateResult lastSyncResult;
				Enum.TryParse(workspaceSection.GetValue("LastSyncResult", ""), out lastSyncResult);
				currentWorkspace.LastSyncResult = lastSyncResult;

				currentWorkspace.LastSyncResultMessage = UnescapeText(workspaceSection.GetValue("LastSyncResultMessage"));
				currentWorkspace.LastSyncChangeNumber = workspaceSection.GetValue("LastSyncChangeNumber", -1);

				DateTime lastSyncTime;
				if(DateTime.TryParse(workspaceSection.GetValue("LastSyncTime", ""), out lastSyncTime))
				{
					currentWorkspace.LastSyncTime = lastSyncTime;
				}

				currentWorkspace.LastSyncDurationSeconds = workspaceSection.GetValue("LastSyncDuration", 0);
				currentWorkspace.LastBuiltChangeNumber = workspaceSection.GetValue("LastBuiltChangeNumber", 0);
				currentWorkspace.ExpandedArchiveTypes = workspaceSection.GetValues("ExpandedArchiveName", new string[0]);

				string[] bisectEntries = workspaceSection.GetValues("Bisect", new string[0]);
				foreach(string bisectEntry in bisectEntries)
				{
					ConfigObject bisectEntryObject = new ConfigObject(bisectEntry);

					int changeNumber = bisectEntryObject.GetValue("Change", -1);
					if(changeNumber != -1)
					{
						BisectState state;
						if(Enum.TryParse(bisectEntryObject.GetValue("State", ""), out state))
						{
							BisectEntry entry = new BisectEntry();
							entry.Change = changeNumber;
							entry.State = state;
							currentWorkspace.BisectChanges.Add(entry);
						}
					}
				}
			}
		}

		protected override void ImportWorkspaceSettings(DirectoryReference rootDir, string clientName, string branchPath, UserWorkspaceSettings currentWorkspace)
		{
			ConfigSection workspaceSection = _configFile.FindSection(clientName + branchPath);
			if (workspaceSection != null)
			{
				currentWorkspace.Filter.View = workspaceSection.GetValues("SyncFilter", new string[0]).ToList();
				currentWorkspace.Filter.SetCategories(GetCategorySettings(workspaceSection, "SyncIncludedCategories", "SyncExcludedCategories"));

				int syncAllProjects = workspaceSection.GetValue("SyncAllProjects", -1);
				currentWorkspace.Filter.AllProjects = (syncAllProjects == 0) ? (bool?)false : (syncAllProjects == 1) ? (bool?)true : (bool?)null;

				int includeAllProjectsInSolution = workspaceSection.GetValue("IncludeAllProjectsInSolution", -1);
				currentWorkspace.Filter.AllProjectsInSln = (includeAllProjectsInSolution == 0) ? (bool?)false : (includeAllProjectsInSolution == 1) ? (bool?)true : (bool?)null;
			}
		}

		protected override void ImportProjectSettings(ProjectInfo projectInfo, UserProjectSettings currentProject)
		{
			string clientProjectFileName = projectInfo.ClientFileName;

			// Read the project settings
			ConfigSection projectSection = _configFile.FindOrAddSection(clientProjectFileName);
			currentProject.BuildSteps.AddRange(projectSection.GetValues("BuildStep", new string[0]).Select(x => new ConfigObject(x)));

			FilterType filterType;
			if (!Enum.TryParse(projectSection.GetValue("FilterType", ""), true, out filterType))
			{
				filterType = FilterType.None;
			}

			currentProject.FilterType = filterType;
			currentProject.FilterBadges.UnionWith(projectSection.GetValues("FilterBadges", new string[0]));
		}

		public override bool Save(ILogger logger)
		{
			if (!base.Save(logger))
			{
				return false;
			}

			// General settings
			ConfigSection generalSection = _configFile.FindOrAddSection("General");
			generalSection.Clear();
			generalSection.SetValue("Version", (int)Version);
			generalSection.SetValue("BuildAfterSync", BuildAfterSync);
			generalSection.SetValue("RunAfterSync", RunAfterSync);
			generalSection.SetValue("OpenSolutionAfterSync", OpenSolutionAfterSync);
			generalSection.SetValue("ShowLogWindow", ShowLogWindow);
			generalSection.SetValue("AutoResolveConflicts", AutoResolveConflicts);
			generalSection.SetValue("ShowUnreviewed", ShowUnreviewedChanges);
			generalSection.SetValue("ShowAutomated", ShowAutomatedChanges);
			generalSection.SetValue("RobomergeFilter", ShowRobomerge.ToString());
			generalSection.SetValue("AnnotateRobomerge", AnnotateRobmergeChanges);
			generalSection.SetValue("ShowLocalTimes", ShowLocalTimes);
			if(LastProject != null)
			{
				generalSection.SetValue("LastProject", LastProject.ToConfigEntry());
			}
			generalSection.SetValues("OpenProjects", OpenProjects.Select(x => x.ToConfigEntry()).ToArray());
			generalSection.SetValue("KeepInTray", KeepInTray);
			generalSection.SetValues("EnabledTools", EnabledTools);
			generalSection.SetValue("FilterIndex", FilterIndex);
			generalSection.SetValues("RecentProjects", RecentProjects.Select(x => x.ToConfigEntry()).ToArray());
			generalSection.SetValue("SyncTypeID", SyncTypeId);

			// Build configuration
			generalSection.SetValue("BuildConfig", CompiledEditorBuildConfig.ToString());

			// Tab labels
			generalSection.SetValue("TabLabels", TabLabels.ToString());

			// Editor arguments
			List<string> editorArgumentList = new List<string>();
			foreach(Tuple<string, bool> editorArgument in EditorArguments)
			{
				editorArgumentList.Add(String.Format("{0}:{1}", editorArgument.Item2? 1 : 0, editorArgument.Item1));
			}
			generalSection.SetValues("EditorArguments", editorArgumentList.ToArray());
			generalSection.SetValue("EditorArgumentsPrompt", EditorArgumentsPrompt);

			// Schedule settings
			ConfigSection scheduleSection = _configFile.FindOrAddSection("Schedule");
			scheduleSection.Clear();
			scheduleSection.SetValue("Enabled", ScheduleEnabled);
			scheduleSection.SetValue("Time", ScheduleTime.ToString());
			scheduleSection.SetValue("AnyOpenProject", ScheduleAnyOpenProject);
			scheduleSection.SetValues("Projects", ScheduleProjects.Select(x => x.ToConfigEntry()).ToArray());

			// Precompiled binaries
			ConfigSection archivesSection = _configFile.FindOrAddSection("PrecompiledBinaries");
			archivesSection.SetValues("Archives", Archives.Select(x => x.ToConfigEntry()).ToArray());

			// OIDC Settings
			ConfigSection oidcSection = _configFile.FindOrAddSection("OIDCProviders");
			List<ConfigObject> tokenObjects = new List<ConfigObject>();
			foreach (KeyValuePair<string, string> pair in ProviderToRefreshTokens)
			{
				ConfigObject tokenEntryObject = new ConfigObject();
				tokenEntryObject.SetValue("Provider", pair.Key);
				tokenEntryObject.SetValue("Token", pair.Value);
				tokenObjects.Add(tokenEntryObject);
			}
			oidcSection.SetValues("Tokens", tokenObjects.Select(x => x.ToString()).ToArray());


			// Window settings
			ConfigSection windowSection = _configFile.FindOrAddSection("Window");
			windowSection.Clear();
			windowSection.SetValue("Visible", WindowVisible);
			windowSection.SetValue("State", WindowState);
			if(WindowBounds != null)
			{
				windowSection.SetValue("Bounds", FormatRectangleValue(WindowBounds.Value));
			}

			// Notification settings
			ConfigSection notificationSection = _configFile.FindOrAddSection("Notifications");
			notificationSection.Clear();
			if (NotifyProjects.Count > 0)
			{
				notificationSection.SetValues("NotifyProjects", NotifyProjects.ToArray());
			}
			if (NotifyUnassignedMinutes != -1)
			{
				notificationSection.SetValue("NotifyUnassignedMinutes", NotifyUnassignedMinutes);
			}
			if (NotifyUnacknowledgedMinutes != -1)
			{
				notificationSection.SetValue("NotifyUnacknowledgedMinutes", NotifyUnacknowledgedMinutes);
			}
			if (NotifyUnresolvedMinutes != -1)
			{
				notificationSection.SetValue("NotifyUnresolvedMinutes", NotifyUnresolvedMinutes);
			}

			// Perforce settings
			ConfigSection perforceSection = _configFile.FindOrAddSection("Perforce");
			perforceSection.Clear();
			if(SyncOptions.NumRetries > 0 && SyncOptions.NumRetries != PerforceSyncOptions.DefaultNumRetries)
			{
				perforceSection.SetValue("NumRetries", SyncOptions.NumRetries);
			}
			if(SyncOptions.NumThreads > 0 && SyncOptions.NumThreads != PerforceSyncOptions.DefaultNumThreads)
			{
				perforceSection.SetValue("NumThreads", SyncOptions.NumThreads);
			}
			if(SyncOptions.TcpBufferSize > 0 && SyncOptions.TcpBufferSize != PerforceSyncOptions.DefaultTcpBufferSize)
			{
				perforceSection.SetValue("TcpBufferSize", SyncOptions.TcpBufferSize);
			}
			if (SyncOptions.FileBufferSize > 0 && SyncOptions.FileBufferSize != PerforceSyncOptions.DefaultFileBufferSize)
			{
				perforceSection.SetValue("FileBufferSize", SyncOptions.FileBufferSize);
			}
			if (SyncOptions.MaxCommandsPerBatch > 0 && SyncOptions.MaxCommandsPerBatch != PerforceSyncOptions.DefaultMaxCommandsPerBatch)
			{
				perforceSection.SetValue("MaxCommandsPerBatch", SyncOptions.MaxCommandsPerBatch);
			}
			if (SyncOptions.MaxSizePerBatch > 0 && SyncOptions.MaxSizePerBatch != PerforceSyncOptions.DefaultMaxSizePerBatch)
			{
				perforceSection.SetValue("MaxSizePerBatch", SyncOptions.MaxSizePerBatch);
			}
			if (SyncOptions.NumSyncErrorRetries > 0 && SyncOptions.NumSyncErrorRetries != PerforceSyncOptions.DefaultNumSyncErrorRetries)
			{
				perforceSection.SetValue("NumSyncErrorRetries", SyncOptions.NumSyncErrorRetries);
			}

			// Save the file
			try
			{
				_configFile.Save(_fileName);
				return true;
			}
			catch (Exception ex)
			{
				logger.LogError(ex, "Unable to save config file {FileName}: {Message}", _fileName, ex.Message);
				return false;
			}
		}

		[return: NotNullIfNotNull("text")]
		static string? EscapeText(string? text)
		{
			if(text == null)
			{
				return null;
			}

			StringBuilder result = new StringBuilder();
			for(int idx = 0; idx < text.Length; idx++)
			{
				switch(text[idx])
				{
					case '\\':
						result.Append("\\\\");
						break;
					case '\t':
						result.Append("\\t");
						break;
					case '\r':
						result.Append("\\r");
						break;
					case '\n':
						result.Append("\\n");
						break;
					case '\'':
						result.Append("\\\'");
						break;
					case '\"':
						result.Append("\\\"");
						break;
					default:
						result.Append(text[idx]);
						break;
				}
			}
			return result.ToString();
		}

		[return: NotNullIfNotNull("text")]
		static string? UnescapeText(string? text)
		{
			if(text == null)
			{
				return null;
			}

			StringBuilder result = new StringBuilder();
			for(int idx = 0; idx < text.Length; idx++)
			{
				if(text[idx] == '\\' && idx + 1 < text.Length)
				{
					switch(text[++idx])
					{
						case 't':
							result.Append('\t');
							break;
						case 'r':
							result.Append('\r');
							break;
						case 'n':
							result.Append('\n');
							break;
						case '\'':
							result.Append('\'');
							break;
						case '\"':
							result.Append('\"');
							break;
						default:
							result.Append(text[idx]);
							break;
					}
				}
				else
				{
					result.Append(text[idx]);
				}
			}
			return result.ToString();
		}

		public IEnumerable<FileReference> GetCachedFilePaths()
		{
			List<FileReference> files = new List<FileReference>();
			files.AddRange(_workspaceDirToSettings.Values.Select(x => x.ConfigFile));
			files.AddRange(_workspaceDirToState.Values.Select(x => x.ConfigFile));
			files.AddRange(_projectKeyToSettings.Keys);
			return files;
		}
	}
}
