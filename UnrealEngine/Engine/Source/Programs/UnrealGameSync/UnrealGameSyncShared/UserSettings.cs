// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json.Serialization;
using EpicGames.Core;
using Microsoft.Extensions.Logging;

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
		public string Name { get; set; } = ""; // Name will the saved ID for this LatestChangeType
		public string Description { get; set; } = "";
		public int OrderIndex { get; set; } = Int32.MaxValue;

		// What Rules to Check for.
		public bool Good { get; set; } = false;
		public bool Starred { get; set; } = false;
		public bool FindNewestGoodContent { get; set; } = false;
		public List<string> RequiredBadges { get; init; } = new List<string>();

		// Depot path to read for the latest change number
		public string? ReadFrom { get; set; }

		public static bool TryParseConfigEntry(string text, [NotNullWhen(true)] out LatestChangeType? changeType)
		{
			ConfigObject definitionObject = new ConfigObject(text);

			string latestChangeTypeName = definitionObject.GetValue("Name", "");
			if (latestChangeTypeName.Length > 0)
			{
				changeType = new LatestChangeType();
				changeType.Name = latestChangeTypeName;
				changeType.Description = definitionObject.GetValue("Description", "No \"Description\" for LatestChangeType given.");
				changeType.OrderIndex = definitionObject.GetValue("OrderIndex", Int32.MaxValue);

				changeType.Good = definitionObject.GetValue("bGood", false);
				changeType.Starred = definitionObject.GetValue("bStarred", false);
				changeType.FindNewestGoodContent = definitionObject.GetValue("bFindNewestGoodContent", false);
				changeType.RequiredBadges.AddRange(definitionObject.GetValue("RequiredBadges", "").Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries));

				changeType.ReadFrom = definitionObject.GetValue("ReadFrom", null);
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

#pragma warning disable CA1027 // Mark enums with FlagsAttribute
	public enum UserSettingsVersion
	{
		Initial = 0,
		DefaultServerSettings = 1,
		XgeShaderCompilation = 2,
		DefaultNumberOfThreads = 3,
		Latest = DefaultNumberOfThreads
	}
#pragma warning restore CA1027 // Mark enums with FlagsAttribute

	public class ArchiveSettings
	{
		public bool Enabled { get; set; }
		public string Type { get; set; }
		public List<string> Order { get; init; }

		public ArchiveSettings(bool enabled, string type, IEnumerable<string> order)
		{
			Enabled = enabled;
			Type = type;
			Order = new List<string>(order);
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
			ServerAndPort = serverAndPort;
			UserName = userName;
			Type = type;
			ClientPath = clientPath;
			LocalPath = localPath;
		}

		public static bool TryParseConfigEntry(string text, [NotNullWhen(true)] out UserSelectedProjectSettings? project)
		{
			ConfigObject obj = new ConfigObject(text);

			UserSelectedProjectType type;
			if (Enum.TryParse(obj.GetValue("Type", ""), out type))
			{
				string? serverAndPort = obj.GetValue("ServerAndPort", null);
				if (String.IsNullOrWhiteSpace(serverAndPort))
				{
					serverAndPort = null;
				}

				// Fixup for code that was saving server host name rather than DNS entry
				if (serverAndPort != null && serverAndPort.Equals("p4-nodeb.epicgames.net:1666", StringComparison.OrdinalIgnoreCase))
				{
					serverAndPort = "perforce:1666";
				}

				string? userName = obj.GetValue("UserName", null);
				if (String.IsNullOrWhiteSpace(userName))
				{
					userName = null;
				}

				string? localPath = obj.GetValue("LocalPath", null);
				if (String.IsNullOrWhiteSpace(localPath))
				{
					localPath = null;
				}

				string? clientPath = obj.GetValue("ClientPath", null);
				if (String.IsNullOrWhiteSpace(clientPath))
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

			if (ServerAndPort != null)
			{
				obj.SetValue("ServerAndPort", ServerAndPort);
			}
			if (UserName != null)
			{
				obj.SetValue("UserName", UserName);
			}

			obj.SetValue("Type", Type.ToString());

			if (ClientPath != null)
			{
				obj.SetValue("ClientPath", ClientPath);
			}
			if (LocalPath != null)
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

			ServerAndPort = serverAndPort;
			UserName = userName;
			ClientName = clientName;
			BranchPath = branchPath;
			ProjectPath = projectPath;
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

		static readonly object _syncRoot = new object();

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

	public class UserProjectSettings
	{
		[JsonIgnore]
		public FileReference ConfigFile { get; private set; } = null!;

		public List<ConfigObject> BuildSteps { get; init; } = new List<ConfigObject>();
		public FilterType FilterType { get; set; }
		public HashSet<string> FilterBadges { get; init; } = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

		static readonly object _syncRoot = new object();

		private UserProjectSettings()
		{
		}

		public UserProjectSettings(FileReference configFile)
		{
			ConfigFile = configFile;
		}

		public static bool TryLoad(FileReference configFile, [NotNullWhen(true)] out UserProjectSettings? settings)
		{
			if (Utility.TryLoadJson(configFile, out settings))
			{
				settings.ConfigFile = configFile;
				return true;
			}
			else
			{
				settings = null;
				return false;
			}
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
			All,        // Show all changes from robomerge
			Badged,     // Show only robomerge changes that have an associated badge
			None        // Show no robomerge changes
		};

		readonly FileReference _fileName;
		readonly ConfigFile _configFile;

		// General settings
		public UserSettingsVersion Version { get; set; } = UserSettingsVersion.Latest;
		public bool BuildAfterSync { get; set; }
		public bool RunAfterSync { get; set; }
		public bool OpenSolutionAfterSync { get; set; }
		public bool ShowLogWindow { get; set; }
		public bool ShowUnreviewedChanges { get; set; }
		public bool ShowAutomatedChanges { get; set; }
		public RobomergeShowChangesOption ShowRobomerge { get; set; }
		public bool AnnotateRobmergeChanges { get; set; }
		public bool ShowLocalTimes { get; set; }
		public bool KeepInTray { get; set; }
		public HashSet<Guid> EnabledTools { get; init; } = new HashSet<Guid>();
		public int FilterIndex { get; set; }
		public UserSelectedProjectSettings? LastProject { get; set; }
		public List<UserSelectedProjectSettings> OpenProjects { get; init; }
		public List<UserSelectedProjectSettings> RecentProjects { get; init; }
		public string SyncTypeId { get; set; }
		public BuildConfig CompiledEditorBuildConfig { get; set; } // NB: This assumes not using precompiled editor. See CurrentBuildConfig.
		public TabLabels TabLabels { get; set; }
		public long NextLauncherVersionCheck { get; set; }

		// Precompiled binaries
		public List<ArchiveSettings> Archives { get; init; } = new List<ArchiveSettings>();

		// OIDC Settings
		public Dictionary<string, string> ProviderToRefreshTokens { get; init; } = new Dictionary<string, string>();

		// Window settings
		public bool WindowVisible { get; set; }
		public string WindowState { get; set; }
		public Rectangle? WindowBounds { get; set; }

		// Schedule settings
		public bool ScheduleEnabled { get; set; }
		public TimeSpan ScheduleTime { get; set; }
		public bool ScheduleAnyOpenProject { get; set; }
		public List<UserSelectedProjectSettings> ScheduleProjects { get; init; } = new List<UserSelectedProjectSettings>();

		// Run configuration
		public List<Tuple<string, bool>> EditorArguments { get; init; } = new List<Tuple<string, bool>>();
		public bool EditorArgumentsPrompt { get; set; }

		// Notification settings
		public List<string> NotifyProjects { get; init; } = new List<string>();
		public int NotifyUnassignedMinutes { get; set; }
		public int NotifyUnacknowledgedMinutes { get; set; }
		public int NotifyUnresolvedMinutes { get; set; }

		// Project settings
		readonly Dictionary<DirectoryReference, UserWorkspaceSettings> _workspaceDirToSettings = new Dictionary<DirectoryReference, UserWorkspaceSettings>();
		readonly Dictionary<FileReference, UserProjectSettings> _projectKeyToSettings = new Dictionary<FileReference, UserProjectSettings>();

		// Perforce settings
		public PerforceSyncOptions SyncOptions => base.Global.Perforce;

		private List<UserSelectedProjectSettings> ReadProjectList(string settingName, string legacySettingName)
		{
			List<UserSelectedProjectSettings> projects = new List<UserSelectedProjectSettings>();

			string[]? projectStrings = _configFile.GetValues(settingName, null);
			if (projectStrings != null)
			{
				foreach (string projectString in projectStrings)
				{
					UserSelectedProjectSettings? project;
					if (UserSelectedProjectSettings.TryParseConfigEntry(projectString, out project))
					{
						projects.Add(project);
					}
				}
			}
			else if (legacySettingName != null)
			{
				string[]? legacyProjectStrings = _configFile.GetValues(legacySettingName, null);
				if (legacyProjectStrings != null)
				{
					foreach (string legacyProjectString in legacyProjectStrings)
					{
						if (!String.IsNullOrWhiteSpace(legacyProjectString))
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
				coreSettingsData.Filter.View.AddRange(configFile.GetValues("General.SyncFilter", Array.Empty<string>()));
				coreSettingsData.Filter.SetCategories(GetCategorySettings(configFile.FindSection("General"), "SyncIncludedCategories", "SyncExcludedCategories"));
				coreSettingsData.Filter.AllProjects = configFile.GetValue("General.SyncAllProjects", false);
				coreSettingsData.Filter.AllProjectsInSln = configFile.GetValue("General.IncludeAllProjectsInSolution", false);
			}

			return new UserSettings(fileName, configFile, coreFileName, coreSettingsData);
		}

		public UserSettings(FileReference inFileName, ConfigFile inConfigFile, FileReference inCoreFileName, GlobalSettings inCoreSettingsData)
			: base(inCoreFileName, inCoreSettingsData)
		{
			_fileName = inFileName;
			_configFile = inConfigFile;

			// General settings
			Version = (UserSettingsVersion)_configFile.GetValue("General.Version", (int)UserSettingsVersion.Initial);
			BuildAfterSync = (_configFile.GetValue("General.BuildAfterSync", "1") != "0");
			RunAfterSync = (_configFile.GetValue("General.RunAfterSync", "1") != "0");
			bool syncPrecompiledEditor = (_configFile.GetValue("General.SyncPrecompiledEditor", "0") != "0");
			OpenSolutionAfterSync = (_configFile.GetValue("General.OpenSolutionAfterSync", "0") != "0");
			ShowLogWindow = (_configFile.GetValue("General.ShowLogWindow", false));

			string? autoResolveConflicts = _configFile.GetValue("General.AutoResolveConflicts", "");
			if (!String.IsNullOrEmpty(autoResolveConflicts))
			{
				Global.AutoResolveConflicts = (autoResolveConflicts != "0");
			}

			ShowUnreviewedChanges = _configFile.GetValue("General.ShowUnreviewed", true);
			ShowAutomatedChanges = _configFile.GetValue("General.ShowAutomated", false);
			NextLauncherVersionCheck = _configFile.GetValue("General.NextLauncherVersionCheck", 0);
			ShowRobomerge = _configFile.GetEnumValue("General.RobomergeFilter", RobomergeShowChangesOption.All);
			AnnotateRobmergeChanges = _configFile.GetValue("General.AnnotateRobomerge", true);
			ShowLocalTimes = _configFile.GetValue("General.ShowLocalTimes", false);
			KeepInTray = _configFile.GetValue("General.KeepInTray", true);

			EnabledTools.Clear();
			EnabledTools.UnionWith(_configFile.GetGuidValues("General.EnabledTools", Array.Empty<Guid>()));
			if (_configFile.GetValue("General.EnableP4VExtensions", false))
			{
				EnabledTools.Add(new Guid("963850A0-BF63-4E0E-B903-1C5954C7DCF8"));
			}
			if (_configFile.GetValue("General.EnableUshell", false))
			{
				EnabledTools.Add(new Guid("922EED87-E732-464C-92DC-5A8F7ED955E2"));
			}

			FilterIndex = _configFile.GetValue("General.FilterIndex", 0);

			string? lastProjectString = _configFile.GetValue("General.LastProject", null);
			if (lastProjectString != null)
			{
				UserSelectedProjectSettings? lastProject;
				if (!UserSelectedProjectSettings.TryParseConfigEntry(lastProjectString, out lastProject))
				{
					lastProject = null;
				}
				LastProject = lastProject;
			}
			else
			{
				string? lastProjectFileName = _configFile.GetValue("General.LastProjectFileName", null);
				if (lastProjectFileName != null)
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
			CompiledEditorBuildConfig = _configFile.GetEnumValue("General.BuildConfig", BuildConfig.Development);

			// Tab names
			TabLabels = _configFile.GetEnumValue("General.TabLabels", TabLabels.Stream);

			// Editor arguments
			string[] arguments = _configFile.GetValues("General.EditorArguments", new string[] { "0:-log", "0:-fastload" });
			if (Version < UserSettingsVersion.XgeShaderCompilation)
			{
				arguments = Enumerable.Concat(arguments, new string[] { "0:-noxgeshadercompile" }).ToArray();
			}
			foreach (string argument in arguments)
			{
				if (argument.StartsWith("0:", StringComparison.Ordinal))
				{
					EditorArguments.Add(new Tuple<string, bool>(argument.Substring(2), false));
				}
				else if (argument.StartsWith("1:", StringComparison.Ordinal))
				{
					EditorArguments.Add(new Tuple<string, bool>(argument.Substring(2), true));
				}
				else
				{
					EditorArguments.Add(new Tuple<string, bool>(argument, true));
				}
			}
			EditorArgumentsPrompt = _configFile.GetValue("General.EditorArgumentsPrompt", false);

			// Precompiled binaries
			string[] archiveValues = _configFile.GetValues("PrecompiledBinaries.Archives", Array.Empty<string>());
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
				Archives.Add(new ArchiveSettings(true, "Editor", Array.Empty<string>()));
			}

			// OIDC Settings
			string[] tokens = _configFile.GetValues("OIDCProviders.Tokens", Array.Empty<string>());
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

			TimeSpan scheduleTime;
			if (!TimeSpan.TryParse(_configFile.GetValue("Schedule.Time", ""), out scheduleTime))
			{
				scheduleTime = new TimeSpan(6, 0, 0);
			}
			ScheduleTime = scheduleTime;

			ScheduleAnyOpenProject = _configFile.GetValue("Schedule.AnyOpenProject", true);
			ScheduleProjects = ReadProjectList("Schedule.Projects", "Schedule.ProjectFileNames");

			// Notification settings
			NotifyProjects = _configFile.GetValues("Notifications.NotifyProjects", Array.Empty<string>()).ToList();
			NotifyUnassignedMinutes = _configFile.GetValue("Notifications.NotifyUnassignedMinutes", -1);
			NotifyUnacknowledgedMinutes = _configFile.GetValue("Notifications.NotifyUnacknowledgedMinutes", -1);
			NotifyUnresolvedMinutes = _configFile.GetValue("Notifications.NotifyUnresolvedMinutes", -1);

			// Perforce settings
			string? numThreadsStr = _configFile.GetValue("Perforce.NumThreads", null);
			if (numThreadsStr != null)
			{
				int numThreads;
				if (Int32.TryParse(numThreadsStr, out numThreads) && numThreads > 0)
				{
					if (Version >= UserSettingsVersion.DefaultNumberOfThreads || numThreads > 1)
					{
						SyncOptions.NumThreads = numThreads;
					}
				}
			}

			SyncOptions.MaxCommandsPerBatch = _configFile.GetOptionalIntValue("Perforce.MaxCommandsPerBatch", SyncOptions.MaxCommandsPerBatch);
			SyncOptions.MaxSizePerBatch = _configFile.GetOptionalIntValue("Perforce.MaxSizePerBatch", SyncOptions.MaxSizePerBatch);
			SyncOptions.NumSyncErrorRetries = _configFile.GetOptionalIntValue("Perforce.NumSyncErrorRetries", SyncOptions.NumSyncErrorRetries);
		}

		static Dictionary<Guid, bool> GetCategorySettings(ConfigSection? section, string includedKey, string excludedKey)
		{
			Dictionary<Guid, bool> result = new Dictionary<Guid, bool>();
			if (section != null)
			{
				foreach (Guid uniqueId in section.GetValues(includedKey, Array.Empty<Guid>()))
				{
					result[uniqueId] = true;
				}
				foreach (Guid uniqueId in section.GetValues(excludedKey, Array.Empty<Guid>()))
				{
					result[uniqueId] = false;
				}
			}
			return result;
		}

		static Rectangle? ParseRectangleValue(string text)
		{
			ConfigObject obj = new ConfigObject(text);

			int x = obj.GetValue("X", -1);
			int y = obj.GetValue("Y", -1);
			int w = obj.GetValue("W", -1);
			int h = obj.GetValue("H", -1);

			if (x == -1 || y == -1 || w == -1 || h == -1)
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

		protected override void ImportWorkspaceState(DirectoryReference rootDir, string clientName, string branchPath, WorkspaceState currentWorkspace)
		{
			// Read the workspace settings
			ConfigSection? workspaceSection = _configFile.FindSection(clientName + branchPath);
			if (workspaceSection == null)
			{
				string legacyBranchAndClientKey = clientName + branchPath;

				int slashIdx = legacyBranchAndClientKey.IndexOf('/', StringComparison.Ordinal);
				if (slashIdx != -1)
				{
					legacyBranchAndClientKey = legacyBranchAndClientKey.Substring(0, slashIdx) + "$" + legacyBranchAndClientKey.Substring(slashIdx + 1);
				}

				string? currentSync = _configFile.GetValue("Clients." + legacyBranchAndClientKey, null);
				if (currentSync != null)
				{
					int atIdx = currentSync.LastIndexOf('@');
					if (atIdx != -1)
					{
						int changeNumber;
						if (Int32.TryParse(currentSync.Substring(atIdx + 1), out changeNumber))
						{
							currentWorkspace.ProjectIdentifier = currentSync.Substring(0, atIdx);
							currentWorkspace.CurrentChangeNumber = changeNumber;
						}
					}
				}

				string? lastUpdateResultText = _configFile.GetValue("Clients." + legacyBranchAndClientKey + "$LastUpdate", null);
				if (lastUpdateResultText != null)
				{
					int colonIdx = lastUpdateResultText.LastIndexOf(':');
					if (colonIdx != -1)
					{
						int changeNumber;
						if (Int32.TryParse(lastUpdateResultText.Substring(0, colonIdx), out changeNumber))
						{
							WorkspaceUpdateResult result;
							if (Enum.TryParse(lastUpdateResultText.Substring(colonIdx + 1), out result))
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
				foreach (string additionalChangeNumberString in workspaceSection.GetValues("AdditionalChangeNumbers", Array.Empty<string>()))
				{
					int additionalChangeNumber;
					if (Int32.TryParse(additionalChangeNumberString, out additionalChangeNumber))
					{
						currentWorkspace.AdditionalChangeNumbers.Add(additionalChangeNumber);
					}
				}

				currentWorkspace.LastSyncResult = workspaceSection.GetEnumValue("LastSyncResult", WorkspaceUpdateResult.Canceled);
				currentWorkspace.LastSyncResultMessage = UnescapeText(workspaceSection.GetValue("LastSyncResultMessage"));
				currentWorkspace.LastSyncChangeNumber = workspaceSection.GetValue("LastSyncChangeNumber", -1);

				DateTime lastSyncTime;
				if (DateTime.TryParse(workspaceSection.GetValue("LastSyncTime", ""), out lastSyncTime))
				{
					currentWorkspace.LastSyncTime = lastSyncTime;
				}

				currentWorkspace.LastSyncDurationSeconds = workspaceSection.GetValue("LastSyncDuration", 0);
				currentWorkspace.LastBuiltChangeNumber = workspaceSection.GetValue("LastBuiltChangeNumber", 0);

				currentWorkspace.LastSyncEditorArchive = workspaceSection.GetValue("LastSyncEditorArchive", "0");

				currentWorkspace.ExpandedArchiveTypes.Clear();
				currentWorkspace.ExpandedArchiveTypes.UnionWith(workspaceSection.GetValues("ExpandedArchiveName", Array.Empty<string>()));

				string[] bisectEntries = workspaceSection.GetValues("Bisect", Array.Empty<string>());
				foreach (string bisectEntry in bisectEntries)
				{
					ConfigObject bisectEntryObject = new ConfigObject(bisectEntry);

					int changeNumber = bisectEntryObject.GetValue("Change", -1);
					if (changeNumber != -1)
					{
						BisectState state;
						if (Enum.TryParse(bisectEntryObject.GetValue("State", ""), out state))
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
			ConfigSection? workspaceSection = _configFile.FindSection(clientName + branchPath);
			if (workspaceSection != null)
			{
				currentWorkspace.Filter.View.Clear();
				currentWorkspace.Filter.View.AddRange(workspaceSection.GetValues("SyncFilter", Array.Empty<string>()));

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
			currentProject.BuildSteps.AddRange(projectSection.GetValues("BuildStep", Array.Empty<string>()).Select(x => new ConfigObject(x)));
			currentProject.FilterType = projectSection.GetEnumValue("FilterType", FilterType.None);
			currentProject.FilterBadges.UnionWith(projectSection.GetValues("FilterBadges", Array.Empty<string>()));
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
			generalSection.SetValue("ShowUnreviewed", ShowUnreviewedChanges);
			generalSection.SetValue("ShowAutomated", ShowAutomatedChanges);
			generalSection.SetValue("RobomergeFilter", ShowRobomerge.ToString());
			generalSection.SetValue("AnnotateRobomerge", AnnotateRobmergeChanges);
			generalSection.SetValue("ShowLocalTimes", ShowLocalTimes);
			if (LastProject != null)
			{
				generalSection.SetValue("LastProject", LastProject.ToConfigEntry());
			}
			generalSection.SetValues("OpenProjects", OpenProjects.Select(x => x.ToConfigEntry()).ToArray());
			generalSection.SetValue("KeepInTray", KeepInTray);
			generalSection.SetValues("EnabledTools", EnabledTools.ToArray());
			generalSection.SetValue("FilterIndex", FilterIndex);
			generalSection.SetValues("RecentProjects", RecentProjects.Select(x => x.ToConfigEntry()).ToArray());
			generalSection.SetValue("SyncTypeID", SyncTypeId);
			generalSection.SetValue("NextLauncherVersionCheck", NextLauncherVersionCheck);

			// Build configuration
			generalSection.SetValue("BuildConfig", CompiledEditorBuildConfig.ToString());

			// Tab labels
			generalSection.SetValue("TabLabels", TabLabels.ToString());

			// Editor arguments
			List<string> editorArgumentList = new List<string>();
			foreach (Tuple<string, bool> editorArgument in EditorArguments)
			{
				editorArgumentList.Add(String.Format("{0}:{1}", editorArgument.Item2 ? 1 : 0, editorArgument.Item1));
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
			if (WindowBounds != null)
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
			_configFile.RemoveSection("Perforce");

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
		static string? UnescapeText(string? text)
		{
			if (text == null)
			{
				return null;
			}

			StringBuilder result = new StringBuilder();
			for (int idx = 0; idx < text.Length; idx++)
			{
				if (text[idx] == '\\' && idx + 1 < text.Length)
				{
					switch (text[++idx])
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
			files.AddRange(_projectKeyToSettings.Keys);
			return files;
		}
	}
}
