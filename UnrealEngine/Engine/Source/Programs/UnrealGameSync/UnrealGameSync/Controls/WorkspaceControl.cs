// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.IO;
using System.Linq;
using System.Runtime.InteropServices;
using System.Text;
using System.Text.Json;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Windows.Forms.VisualStyles;
using EpicGames.Core;
using EpicGames.Horde;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using Microsoft.Win32;

namespace UnrealGameSync
{
	partial class WorkspaceControl : UserControl, IMainWindowTabPanel
	{
		enum HorizontalAlignment
		{
			Left,
			Center,
			Right
		}

		enum VerticalAlignment
		{
			Top,
			Middle,
			Bottom
		}

		class BadgeInfo
		{
			public string _label;
			public string? _group;
			public string? _uniqueId;
			public int _offset;
			public int _width;
			public int _height;
			public Color _backgroundColor;
			public Color _hoverBackgroundColor;
			public Action? _leftClickHandler;
			public Action<Control, Point>? _rightClickHandler;
			public string? _toolTip;

			public BadgeInfo(string label, string group, Color badgeColor)
				: this(label, group, null, badgeColor, badgeColor, null, null)
			{
			}

			public BadgeInfo(string label, string? group, string? uniqueId, Color backgroundColor,
				Color hoverBackgroundColor, Action? leftClickHandler, Action<Control, Point>? rightClickHandler)
			{
				_label = label;
				_group = group;
				_uniqueId = uniqueId;
				_backgroundColor = backgroundColor;
				_hoverBackgroundColor = hoverBackgroundColor;
				_leftClickHandler = leftClickHandler;
				_rightClickHandler = rightClickHandler;
			}

			public BadgeInfo(BadgeInfo other)
				: this(other._label, other._group, other._uniqueId, other._backgroundColor, other._hoverBackgroundColor, other._leftClickHandler, other._rightClickHandler)
			{
				_offset = other._offset;
				_width = other._width;
				_height = other._height;
			}

			public Rectangle GetBounds(Point listLocation)
			{
				return new Rectangle(listLocation.X + _offset, listLocation.Y, _width, _height);
			}
		}

		class ServiceBadgeInfo : IEquatable<ServiceBadgeInfo>
		{
			public string Name { get; }
			public string? Url { get; }
			public BadgeResult Result { get; }

			public ServiceBadgeInfo(string name, string? url, BadgeResult result)
			{
				Name = name;
				Url = url;
				Result = result;
			}

			public bool Equals(ServiceBadgeInfo? otherBadge)
			{
				return otherBadge != null && Name == otherBadge.Name && Url == otherBadge.Url && Result == otherBadge.Result;
			}

			public override bool Equals(object? other)
			{
				return Equals(other as ServiceBadgeInfo);
			}

			public override int GetHashCode()
			{
				return HashCode.Combine(Name, Url, Result);
			}
		}

		class ChangeLayoutInfo
		{
			public List<BadgeInfo> _descriptionBadges = new List<BadgeInfo>();
			public List<BadgeInfo> _typeBadges = new List<BadgeInfo>();
			public List<BadgeInfo> _buildBadges = new List<BadgeInfo>();
			public Dictionary<string, List<BadgeInfo>> _customBadges = new Dictionary<string, List<BadgeInfo>>();
		}

		class BuildListItemComparer : System.Collections.IComparer
		{
			public int Compare(object? a, object? b)
			{
				ChangesRecord? changeA = ((ListViewItem?)a)?.Tag as ChangesRecord;
				ChangesRecord? changeB = ((ListViewItem?)b)?.Tag as ChangesRecord;
				if (changeA == null)
				{
					return +1;
				}
				else if (changeB == null)
				{
					return -1;
				}
				else
				{
					return changeB.Number - changeA.Number;
				}
			}
		}

		public static Rectangle GoodBuildIcon { get; } = new Rectangle(0, 0, 16, 16);
		public static Rectangle MixedBuildIcon { get; } = new Rectangle(16, 0, 16, 16);
		public static Rectangle BadBuildIcon { get; } = new Rectangle(32, 0, 16, 16);
		public static Rectangle DefaultBuildIcon { get; } = new Rectangle(48, 0, 16, 16);
		static Rectangle PromotedBuildIcon { get; } = new Rectangle(64, 0, 16, 16);
		static Rectangle InfoIcon { get; } = new Rectangle(96, 0, 16, 16);
		static Rectangle CancelIcon { get; } = new Rectangle(112, 0, 16, 16);
		static Rectangle HappyIcon { get; } = new Rectangle(160, 0, 16, 16);
		static Rectangle DisabledHappyIcon { get; } = new Rectangle(176, 0, 16, 16);
		static Rectangle FrownIcon { get; } = new Rectangle(192, 0, 16, 16);
		static Rectangle DisabledFrownIcon { get; } = new Rectangle(208, 0, 16, 16);
		static Rectangle PreviousSyncIcon { get; } = new Rectangle(224, 0, 16, 16);
		static Rectangle AdditionalSyncIcon { get; } = new Rectangle(240, 0, 16, 16);
		static Rectangle BisectPassIcon { get; } = new Rectangle(256, 0, 16, 16);
		static Rectangle BisectFailIcon { get; } = new Rectangle(273, 0, 16, 16);
		static Rectangle BisectImplicitPassIcon { get; } = new Rectangle(290, 0, 16, 16);
		static Rectangle BisectImplicitFailIcon { get; } = new Rectangle(307, 0, 16, 16);

		[DllImport("uxtheme.dll", CharSet = CharSet.Unicode)]
		static extern int SetWindowTheme(IntPtr hWnd, string pszSubAppName, string pszSubIdList);

		[DllImport("user32.dll")]
		static extern bool FlashWindow(IntPtr hwnd, bool bInvert);

		const int BuildListExpandCount = 250;

		readonly IWorkspaceControlOwner _owner;
		readonly DirectoryReference _appDataFolder;
		readonly string? _apiUrl;
		readonly DirectoryReference _workspaceDataFolder;
		readonly IServiceProvider _serviceProvider;
		readonly ILogger _logger;
		readonly IPerforceSettings _perforceSettings;
		readonly IHordeClient? _hordeClient;
		ProjectInfo ProjectInfo { get; }

		readonly UserSettings _settings;
		readonly UserWorkspaceSettings _workspaceSettings;
		readonly UserProjectSettings _projectSettings;

		bool _userHasOpenIssues = false;

		public UserSelectedProjectSettings SelectedProject { get; }

		public FileReference SelectedFileName => ProjectInfo.LocalFileName;
		string SelectedProjectIdentifier => ProjectInfo.ProjectIdentifier;
		public DirectoryReference BranchDirectoryName => ProjectInfo.LocalRootPath;
		public string? StreamName => ProjectInfo.StreamName;
		public string ClientName => ProjectInfo.ClientName;

		readonly SynchronizationContext _mainThreadSynchronizationContext;
		bool _isDisposing;

		PerforceMonitor _perforceMonitor;
		Workspace _workspace;
#pragma warning disable CA2213 //warning CA2213: 'WorkspaceControl' contains field '_issueMonitor' that is of IDisposable type 'IssueMonitor', but it is never disposed. Change the Dispose method on 'WorkspaceControl' to call Close or Dispose on this field.
		IssueMonitor _issueMonitor;
#pragma warning restore CA2213
		EventMonitor _eventMonitor;
#pragma warning disable CA2213 // warning CA2213: 'WorkspaceControl' contains field '_updateTimer' that is of IDisposable type 'Timer', but it is never disposed. Change the Dispose method on 'WorkspaceControl' to call Close or Dispose on this field.
		readonly System.Windows.Forms.Timer _updateTimer;
#pragma warning restore CA2213
		HashSet<int> _promotedChangeNumbers = new HashSet<int>();
		List<int> _listIndexToChangeIndex = new List<int>();
		List<int> _sortedChangeNumbers = new List<int>();
		readonly Dictionary<string, Dictionary<int, IArchive?>> _archiveToChangeNumberToArchiveKey = new Dictionary<string, Dictionary<int, IArchive?>>();
		readonly Dictionary<int, ChangeLayoutInfo> _changeNumberToLayoutInfo = new Dictionary<int, ChangeLayoutInfo>();
		readonly List<ContextMenuStrip> _customStatusPanelMenus = new List<ContextMenuStrip>();
		readonly List<(string, Action<Point, Rectangle>)> _customStatusPanelLinks = new List<(string, Action<Point, Rectangle>)>();
		int _numChanges;
		int _pendingSelectedChangeNumber = -1;
		bool _hasBuildSteps = false;

		readonly Dictionary<string, int> _notifiedBuildTypeToChangeNumber = new Dictionary<string, int>();

		bool _mouseOverExpandLink;

		string? _hoverBadgeUniqueId = null;
		bool _hoverSync;
		ChangesRecord? _contextMenuChange;
		Font? _buildFont;
		Font? _selectedBuildFont;
		Font? _badgeFont;
		List<KeyValuePair<string, string>> _badgeNameAndGroupPairs = new List<KeyValuePair<string, string>>();
		readonly Dictionary<string, Size> _badgeLabelToSize = new Dictionary<string, Size>();
		List<ServiceBadgeInfo> _serviceBadges = new List<ServiceBadgeInfo>();

		NotificationWindow _notificationWindow;

		public Tuple<TaskbarState, float> DesiredTaskbarState
		{
			get;
			private set;
		}

		int _buildListWidth;
		float[] _columnWidths = Array.Empty<float>();
		float[] _columnWeights = Array.Empty<float>();
		int[] _minColumnWidths = Array.Empty<int>();
		int[] _desiredColumnWidths = Array.Empty<int>();
		string? _lastColumnSettings;
		List<ColumnHeader> _customColumns = new List<ColumnHeader>();
		int _maxBuildBadgeChars;
		ListViewItem? _expandItem;

		bool _updateBuildListPosted;
		bool _updateBuildMetadataPosted;
		bool _updateReviewsPosted;

		WorkspaceUpdateCallback? _updateCallback;

		System.Threading.Timer? _startupTimer;
		List<WorkspaceStartupCallback>? _startupCallbacks;

		// When an author filter is applied this will be non-empty and != AuthorFilterPlaceholderText
		string _authorFilterText = "";

		// Placeholder text that is in the control and cleared when the user starts editing.
		const string AuthorFilterPlaceholderText = "<username>";

		static readonly Bitmap s_hackToolStripMenuGutter = new Bitmap(1, 1);

		public WorkspaceControl(IWorkspaceControlOwner owner, DirectoryReference appDataFolder, string? apiUrl, OpenProjectInfo openProjectInfo, IServiceProvider serviceProvider, UserSettings settings)
		{
			InitializeComponent();

			// Winforms doesn't seem to DPI scale the left gutter for toolstrip menus correctly, but setting a custom image forces it to work correctly. (?)
			OptionsContextMenu_ApplicationSettings.Image = s_hackToolStripMenuGutter;

			_mainThreadSynchronizationContext = SynchronizationContext.Current!;

			_owner = owner;
			_appDataFolder = appDataFolder;
			_apiUrl = apiUrl;
			_workspaceDataFolder = openProjectInfo.ProjectInfo.DataFolder;
			_serviceProvider = serviceProvider;
			_logger = serviceProvider.GetRequiredService<ILogger<WorkspaceControl>>();
			_perforceSettings = openProjectInfo.PerforceSettings;
			ProjectInfo = openProjectInfo.ProjectInfo;
			_settings = settings;
			_workspaceSettings = openProjectInfo.WorkspaceSettings;
			_projectSettings = settings.FindOrAddProjectSettings(openProjectInfo.ProjectInfo, openProjectInfo.WorkspaceSettings, _logger);
			_hordeClient = serviceProvider.GetService<IHordeClient>();

			DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);

			System.Reflection.PropertyInfo doubleBufferedProperty = typeof(Control).GetProperty("DoubleBuffered", System.Reflection.BindingFlags.NonPublic | System.Reflection.BindingFlags.Instance)!;
			doubleBufferedProperty.SetValue(BuildList, true, null);

			// force the height of the rows
			BuildList.SmallImageList = new ImageList() { ImageSize = new Size(1, 20) };
			BuildList_FontChanged(null, null!);
			BuildList.OnScroll += BuildList_OnScroll;
			BuildList.ListViewItemSorter = new BuildListItemComparer();

			Splitter.OnVisibilityChanged += Splitter_OnVisibilityChanged;

			_updateTimer = new System.Windows.Forms.Timer();
			_updateTimer.Interval = 30;
			_updateTimer.Tick += TimerCallback;
			components!.Add(_updateTimer);

			UpdateCheckedBuildConfig();

			UpdateSyncActionCheckboxes();

			// Set the project logo on the status panel and notification window
			_notificationWindow = new NotificationWindow(Properties.Resources.DefaultNotificationLogo);
			if (ProjectInfo.LocalFileName.HasExtension(".uproject"))
			{
				FileReference logoFileName = FileReference.Combine(ProjectInfo.LocalFileName.Directory, "Build", "UnrealGameSync.png");
				if (FileReference.Exists(logoFileName))
				{
					try
					{
						// Duplicate the image, otherwise we'll leave the file locked
						using (Image image = Image.FromFile(logoFileName.FullName))
						{
							StatusPanel.SetProjectLogo(new Bitmap(image), true);
						}
					}
					catch
					{
					}
				}
			}

			// Commit all the new project info
			IPerforceSettings perforceClientSettings = openProjectInfo.PerforceSettings;
			SelectedProject = openProjectInfo.SelectedProject;

			// Figure out which API server to use
			string? newApiUrl;
			if (TryGetProjectSetting(openProjectInfo.LatestProjectConfigFile, "ApiUrl", out newApiUrl))
			{
				_apiUrl = newApiUrl;
			}

			ProjectInfo project = openProjectInfo.ProjectInfo;

			_workspace = new Workspace(perforceClientSettings, project, openProjectInfo.WorkspaceStateWrapper, openProjectInfo.WorkspaceProjectConfigFile, openProjectInfo.WorkspaceProjectStreamFilter,
				new LogControlTextWriter(SyncLog), _serviceProvider);

			_workspace.OnStateChanged += StateChangedCallback;
			_workspace.OnUpdateComplete += UpdateCompleteCallback;

			FileReference projectLogBaseName = FileReference.Combine(_workspaceDataFolder, "sync.log");

			ILogger perforceLogger = _serviceProvider.GetRequiredService<ILogger<PerforceMonitor>>();
			_perforceMonitor = new PerforceMonitor(perforceClientSettings, openProjectInfo.ProjectInfo, openProjectInfo.LatestProjectConfigFile, openProjectInfo.ProjectInfo.CacheFolder, openProjectInfo.LocalConfigFiles, _serviceProvider);

			_perforceMonitor.OnUpdate += UpdateBuildListCallback;
			_perforceMonitor.OnUpdateMetadata += UpdateBuildMetadataCallback;
			_perforceMonitor.OnStreamChange += StreamChanged;
			_perforceMonitor.OnLoginExpired += LoginExpired;

			ILogger eventLogger = _serviceProvider.GetRequiredService<ILogger<EventMonitor>>();
			_eventMonitor = new EventMonitor(_apiUrl, PerforceUtils.GetClientOrDepotDirectoryName(SelectedProjectIdentifier), openProjectInfo.PerforceSettings.UserName, _serviceProvider);
			_eventMonitor.OnUpdatesReady += UpdateReviewsCallback;

			UpdateColumnSettings(true);
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point);

			SyncLog.OpenFile(projectLogBaseName);

			Splitter.SetLogVisibility(_settings.ShowLogWindow);

			BuildList.Items.Clear();
			UpdateBuildList();
			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();
			UpdateStatusPanel();
			UpdateServiceBadges();

			_perforceMonitor.Start();
			_eventMonitor.Start();

			_startupTimer = new System.Threading.Timer(x => _mainThreadSynchronizationContext.Post((o) =>
			{
				if (!IsDisposed)
				{
					StartupTimerElapsed(false);
				}
			}, null), null, TimeSpan.FromSeconds(20.0), TimeSpan.FromMilliseconds(-1.0));
			_startupCallbacks = new List<WorkspaceStartupCallback>();

			string? issuesApiUrl = GetIssuesApiUrl();
			_issueMonitor = owner.CreateIssueMonitor(issuesApiUrl, openProjectInfo.PerforceSettings.UserName);
			_issueMonitor.OnIssuesChanged += IssueMonitor_OnIssuesChangedAsync;

			if (SelectedFileName.HasExtension(".uproject"))
			{
				DirectoryReference configDir = DirectoryReference.Combine(SelectedFileName.Directory, "Saved", "Config");

				DirectoryReference.CreateDirectory(configDir);

				EditorConfigWatcher.Path = configDir.FullName;
				EditorConfigWatcher.NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.FileName;
				EditorConfigWatcher.Filter = "*.ini";
				EditorConfigWatcher.IncludeSubdirectories = true;
				EditorConfigWatcher.EnableRaisingEvents = true;

				TintColor = GetTintColor();
			}

			_owner.ToolUpdateMonitor.OnChange += UpdateStatusPanel_CrossThread;
		}

		public void IssueMonitor_OnIssuesChangedAsync()
		{
			_mainThreadSynchronizationContext.Post((o) =>
			{
				if (_issueMonitor != null)
				{
					IssueMonitor_OnIssuesChanged();
				}
			}, null);
		}

		public List<IssueData> GetOpenIssuesForUser()
		{
			return _issueMonitor.GetIssues().Where(x => x.FixChange <= 0 && String.Equals(x.Owner, _perforceSettings.UserName, StringComparison.OrdinalIgnoreCase)).ToList();
		}

		public void IssueMonitor_OnIssuesChanged()
		{
			bool prevUserHasOpenIssues = _userHasOpenIssues;
			_userHasOpenIssues = GetOpenIssuesForUser().Count > 0;
			if (_userHasOpenIssues != prevUserHasOpenIssues)
			{
				UpdateStatusPanel();
				StatusPanel.Invalidate();
			}
		}

		public IssueMonitor GetIssueMonitor()
		{
			return _issueMonitor;
		}

		private void CheckForStartupComplete()
		{
			if (_startupTimer != null)
			{
				if (FindChangeToSync(_settings.SyncTypeId, out _))
				{
					StartupTimerElapsed(false);
				}
			}
		}

		private void StartupTimerElapsed(bool cancel)
		{
			if (_startupTimer != null)
			{
				_startupTimer.Dispose();
				_startupTimer = null;
			}

			if (_startupCallbacks != null)
			{
				foreach (WorkspaceStartupCallback startupCallback in _startupCallbacks)
				{
					startupCallback(this, cancel);
				}
				_startupCallbacks = null;
			}
		}

		public void AddStartupCallback(WorkspaceStartupCallback startupCallback)
		{
			if (_startupTimer == null)
			{
				startupCallback(this, false);
			}
			else
			{
				_startupCallbacks!.Add(startupCallback);
			}
		}

		private void UpdateColumnSettings(bool forceUpdate)
		{
			string? nextColumnSettings;
			TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "Columns", out nextColumnSettings);

			if (forceUpdate || nextColumnSettings != _lastColumnSettings)
			{
				_lastColumnSettings = nextColumnSettings;

				foreach (ColumnHeader customColumn in _customColumns)
				{
					BuildList.Columns.Remove(customColumn);
				}

				Dictionary<string, ColumnHeader> nameToColumn = new Dictionary<string, ColumnHeader>();
				foreach (ColumnHeader? column in BuildList.Columns)
				{
					if (column != null)
					{
						nameToColumn[column.Text] = column;
						column.Tag = null;
					}
				}

				_customColumns = new List<ColumnHeader>();
				if (nextColumnSettings != null)
				{
					foreach (string customColumn in nextColumnSettings.Split('\n'))
					{
						ConfigObject columnConfig = new ConfigObject(customColumn);

						string? name = columnConfig.GetValue("Name", null);
						if (name != null)
						{
							ColumnHeader? column;
							if (nameToColumn.TryGetValue(name, out column))
							{
								column.Tag = columnConfig;
							}
							else
							{
								int index = columnConfig.GetValue("Index", -1);
								if (index < 0 || index > BuildList.Columns.Count)
								{
									index = ((_customColumns.Count > 0) ? _customColumns[^1].Index : CISColumn.Index) + 1;
								}

								column = new ColumnHeader();
								column.Text = name;
								column.Tag = columnConfig;
								BuildList.Columns.Insert(index, column);

								_customColumns.Add(column);
							}
						}
					}
				}

				_columnWidths = new float[BuildList.Columns.Count];
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					_columnWidths[idx] = BuildList.Columns[idx].Width;
				}

				float dpiScaleX = DeviceDpi / 96.0f;

				_minColumnWidths = Enumerable.Repeat((int)(32 * dpiScaleX), BuildList.Columns.Count).ToArray();
				_minColumnWidths[IconColumn.Index] = (int)(50 * dpiScaleX);
				_minColumnWidths[TypeColumn.Index] = (int)(100 * dpiScaleX);
				_minColumnWidths[TimeColumn.Index] = (int)(75 * dpiScaleX);
				_minColumnWidths[ChangeColumn.Index] = (int)(75 * dpiScaleX);
				_minColumnWidths[CISColumn.Index] = (int)(200 * dpiScaleX);

				_desiredColumnWidths = Enumerable.Repeat(65536, BuildList.Columns.Count).ToArray();
				_desiredColumnWidths[IconColumn.Index] = _minColumnWidths[IconColumn.Index];
				_desiredColumnWidths[TypeColumn.Index] = _minColumnWidths[TypeColumn.Index];
				_desiredColumnWidths[TimeColumn.Index] = _minColumnWidths[TimeColumn.Index];
				_desiredColumnWidths[ChangeColumn.Index] = _minColumnWidths[ChangeColumn.Index];
				_desiredColumnWidths[AuthorColumn.Index] = (int)(120 * dpiScaleX);
				_desiredColumnWidths[CISColumn.Index] = (int)(200 * dpiScaleX);
				_desiredColumnWidths[StatusColumn.Index] = (int)(300 * dpiScaleX);

				_columnWeights = Enumerable.Repeat(1.0f, BuildList.Columns.Count).ToArray();
				_columnWeights[IconColumn.Index] = 3.0f;
				_columnWeights[TypeColumn.Index] = 3.0f;
				_columnWeights[TimeColumn.Index] = 3.0f;
				_columnWeights[ChangeColumn.Index] = 3.0f;
				_columnWeights[DescriptionColumn.Index] = 1.25f;
				_columnWeights[CISColumn.Index] = 1.5f;

				foreach (ColumnHeader? column in BuildList.Columns)
				{
					if (column != null)
					{
						ConfigObject? columnConfig = (ConfigObject?)column.Tag;
						if (columnConfig != null)
						{
							_minColumnWidths[column.Index] = (int)(columnConfig.GetValue("MinWidth", _minColumnWidths[column.Index]) * dpiScaleX);
							_desiredColumnWidths[column.Index] = (int)(columnConfig.GetValue("DesiredWidth", _desiredColumnWidths[column.Index]) * dpiScaleX);
							_columnWeights[column.Index] = columnConfig.GetValue("Weight", _minColumnWidths[column.Index]);
						}
					}
				}

				ConfigFile projectConfigFile = _perforceMonitor.LatestProjectConfigFile;
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					if (!String.IsNullOrEmpty(BuildList.Columns[idx].Text))
					{
						string? stringValue;
						if (TryGetProjectSetting(projectConfigFile, String.Format("ColumnWidth_{0}", BuildList.Columns[idx].Text), out stringValue))
						{
							int intValue;
							if (Int32.TryParse(stringValue, out intValue))
							{
								_desiredColumnWidths[idx] = (int)(intValue * dpiScaleX);
							}
						}
					}
				}

				if (_columnWidths != null)
				{
					ResizeColumns(_columnWidths.Sum());
				}
			}
		}

		private bool UpdateServiceBadges()
		{
			string[]? serviceBadgeNames;
			if (!TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "ServiceBadges", out serviceBadgeNames))
			{
				serviceBadgeNames = Array.Empty<string>();
			}

			List<ServiceBadgeInfo> prevServiceBadges = _serviceBadges;

			_serviceBadges = new List<ServiceBadgeInfo>();
			foreach (string serviceBadgeName in serviceBadgeNames)
			{
				BadgeData? latestBuild;
				if (_eventMonitor.TryGetLatestBadge(serviceBadgeName, out latestBuild))
				{
					_serviceBadges.Add(new ServiceBadgeInfo(serviceBadgeName, latestBuild.Url, latestBuild.Result));
				}
				else
				{
					_serviceBadges.Add(new ServiceBadgeInfo(serviceBadgeName, null, BadgeResult.Skipped));
				}
			}

			return !Enumerable.SequenceEqual(_serviceBadges, prevServiceBadges);
		}

		protected override void OnLoad(EventArgs e)
		{
			base.OnLoad(e);

			_buildListWidth = BuildList.Width;

			// Find the default widths 
			_columnWidths = new float[BuildList.Columns.Count];
			for (int idx = 0; idx < BuildList.Columns.Count; idx++)
			{
				if (_desiredColumnWidths[idx] > 0)
				{
					_columnWidths[idx] = _desiredColumnWidths[idx];
				}
				else
				{
					_columnWidths[idx] = BuildList.Columns[idx].Width;
				}
			}

			// Resize them to fit the size of the window
			ResizeColumns(BuildList.Width - 32);
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		/// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
		protected override void Dispose(bool disposing)
		{
			if (_owner?.ToolUpdateMonitor?.OnChange != null)
			{
				_owner.ToolUpdateMonitor.OnChange -= UpdateStatusPanel_CrossThread;
			}

			_isDisposing = true;

			if (disposing && (components != null))
			{
				components.Dispose();
			}

			if (_startupCallbacks != null)
			{
				foreach (WorkspaceStartupCallback startupCallback in _startupCallbacks)
				{
					startupCallback(this, true);
				}
				_startupCallbacks = null!;
			}

			if (_issueMonitor != null)
			{
				_issueMonitor.OnIssuesChanged -= IssueMonitor_OnIssuesChangedAsync;
				_owner?.ReleaseIssueMonitor(_issueMonitor);
				_issueMonitor = null!;
			}
			if (_startupTimer != null)
			{
				_startupTimer.Dispose();
				_startupTimer = null!;
			}
			if (_notificationWindow != null)
			{
				_notificationWindow.Dispose();
				_notificationWindow = null!;
			}
			if (_perforceMonitor != null)
			{
				_perforceMonitor.Dispose();
				_perforceMonitor = null!;
			}
			if (_workspace != null)
			{
				_workspace.Dispose();
				_workspace = null!;
			}
			if (_eventMonitor != null)
			{
				_eventMonitor.Dispose();
				_eventMonitor = null!;
			}
			if (_buildFont != null)
			{
				_buildFont.Dispose();
				_buildFont = null!;
			}
			if (_selectedBuildFont != null)
			{
				_selectedBuildFont.Dispose();
				_selectedBuildFont = null!;
			}
			if (_badgeFont != null)
			{
				_badgeFont.Dispose();
				_badgeFont = null!;
			}

			base.Dispose(disposing);
		}

		public bool IsBusy()
		{
			return _workspace.IsBusy();
		}

		public bool CanSyncNow()
		{
			return _workspace != null && !_workspace.IsBusy();
		}

		public bool CanLaunchEditor()
		{
			return _workspace != null && !_workspace.IsBusy() && _workspace.CurrentChangeNumber != -1;
		}

		private void MainWindow_Load(object sender, EventArgs e)
		{
			UpdateStatusPanel();
		}

		private void ShowErrorDialog(string format, params object[] args)
		{
			string message = String.Format(format, args);
			_logger.LogError("{Message}", message);
			MessageBox.Show(message);
		}

		public bool CanClose()
		{
			CancelWorkspaceUpdate();
			return !_workspace.IsBusy();
		}

		private void StreamChanged()
		{
			_owner.StreamChanged(this);
			/*
			StatusPanel.SuspendDisplay();

			string PrevSelectedFileName = SelectedFileName;
			if(TryCloseProject())
			{
				OpenProject(PrevSelectedFileName);
			}

			StatusPanel.ResumeDisplay();*/
		}

		private void LoginExpired()
		{
			if (!_isDisposing)
			{
				_logger.LogInformation("Login has expired. Requesting project to be closed.");
				_owner.RequestProjectChange(this, SelectedProject, false);
			}
		}

		private void BuildList_OnScroll()
		{
			_pendingSelectedChangeNumber = -1;
		}

		void ShrinkNumRequestedBuilds()
		{
			if (_perforceMonitor != null && BuildList.Items.Count > 0 && _pendingSelectedChangeNumber == -1)
			{
				// Find the number of visible items using a (slightly wasteful) binary search
				int visibleItemCount = 1;
				for (int stepSize = BuildList.Items.Count / 2; stepSize >= 1;)
				{
					int testIndex = visibleItemCount + stepSize;
					if (testIndex < BuildList.Items.Count && BuildList.GetItemRect(testIndex).Top < BuildList.Height)
					{
						visibleItemCount += stepSize;
					}
					else
					{
						stepSize /= 2;
					}
				}

				// Figure out the last index to ensure is visible
				int lastVisibleIndex = visibleItemCount;
				if (lastVisibleIndex >= _listIndexToChangeIndex.Count)
				{
					lastVisibleIndex = _listIndexToChangeIndex.Count - 1;
				}

				// Get the max number of changes to ensure this
				int newPendingMaxChanges = 0;
				if (lastVisibleIndex >= 0)
				{
					newPendingMaxChanges = _listIndexToChangeIndex[lastVisibleIndex];
				}
				newPendingMaxChanges = PerforceMonitor.InitialMaxChangesValue + ((Math.Max(newPendingMaxChanges - PerforceMonitor.InitialMaxChangesValue, 0) + BuildListExpandCount - 1) / BuildListExpandCount) * BuildListExpandCount;

				// Shrink the number of changes retained by the PerforceMonitor class
				if (_perforceMonitor.PendingMaxChanges > newPendingMaxChanges)
				{
					_perforceMonitor.PendingMaxChanges = newPendingMaxChanges;
				}
			}
		}

		void StartSync(int changeNumber)
		{
			StartSync(changeNumber, false, null);
		}

		void StartSync(int changeNumber, bool syncOnly, WorkspaceUpdateCallback? callback)
		{
			WorkspaceUpdateOptions options = WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.GenerateProjectFiles;
			if (!syncOnly)
			{
				if (_settings.BuildAfterSync)
				{
					options |= WorkspaceUpdateOptions.Build;
				}
				if ((_settings.BuildAfterSync || ShouldSyncPrecompiledEditor) && _settings.RunAfterSync)
				{
					options |= WorkspaceUpdateOptions.RunAfterSync;
				}
				if (_settings.OpenSolutionAfterSync)
				{
					options |= WorkspaceUpdateOptions.OpenSolutionAfterSync;
				}
			}
			StartWorkspaceUpdate(changeNumber, options, callback);
		}

		void StartWorkspaceUpdate(int changeNumber, WorkspaceUpdateOptions options)
		{
			StartWorkspaceUpdate(changeNumber, options, null);
		}

		void StartWorkspaceUpdate(int changeNumber, WorkspaceUpdateOptions options, WorkspaceUpdateCallback? callback)
		{
			if ((options & (WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.Build)) != 0 && GetProcessesRunningInWorkspace().Length > 0)
			{
				if ((options & WorkspaceUpdateOptions.ScheduledBuild) != 0)
				{
					SyncLog.Clear();
					SyncLog.AppendLine("Editor is open; scheduled sync has been aborted.");
					return;
				}
				else
				{
					if (!WaitForProgramsToFinish())
					{
						return;
					}
				}
			}

			string[] combinedSyncFilter = UserSettings.GetCombinedSyncFilter(GetSyncCategories(), _settings.Global.Filter, _workspaceSettings.Filter, _perforceMonitor.LatestPerforceConfigSection());

			WorkspaceUpdateContext context = new WorkspaceUpdateContext(changeNumber, options, GetEditorBuildConfig(), combinedSyncFilter, _projectSettings.BuildSteps, null);
			if (options.HasFlag(WorkspaceUpdateOptions.SyncArchives))
			{
				IReadOnlyList<IArchiveChannel> archives = GetArchiveChannels();
				foreach (IArchiveChannel archive in archives)
				{
					context.ArchiveTypeToArchive[archive.Type] = null;
				}

				List<IArchiveChannel> selectedArchiveChannels = GetSelectedArchiveChannels(archives);
				foreach (IArchiveChannel archiveChannel in selectedArchiveChannels)
				{
					IArchive? archivePath = GetArchiveForChangeNumber(archiveChannel, changeNumber);
					if (archivePath == null)
					{
						MessageBox.Show(String.Format("There are no compiled {0} binaries for this change. To sync it, you must disable syncing of precompiled editor binaries.", archiveChannel.Name));
						return;
					}

					if (archiveChannel.Type == IArchiveChannel.EditorArchiveType)
					{
						context.Options &= ~(WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.GenerateProjectFiles | WorkspaceUpdateOptions.OpenSolutionAfterSync);
					}

					string[]? zippedBinariesSyncFilter;
					if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "ZippedBinariesSyncFilter", out zippedBinariesSyncFilter) && zippedBinariesSyncFilter.Length > 0)
					{
						context.SyncFilter.AddRange(zippedBinariesSyncFilter);
					}

					context.ArchiveTypeToArchive[archiveChannel.Type] = archivePath;
				}
			}
			StartWorkspaceUpdate(context, callback);
		}

		void StartWorkspaceUpdate(WorkspaceUpdateContext context, WorkspaceUpdateCallback? callback)
		{
			context.Options |= WorkspaceUpdateContext.GetOptionsFromConfig(_settings.Global, _workspaceSettings);

			_updateCallback = callback;

			context.StartTime = DateTime.UtcNow;
			context.PerforceSyncOptions = (PerforceSyncOptions)_settings.SyncOptions.Clone();

			_logger.LogInformation("Updating workspace at {Time}...", context.StartTime.ToLocalTime().ToString());
			_logger.LogInformation("  ChangeNumber={Change}", context.ChangeNumber);
			_logger.LogInformation("  Options={Options}", context.Options.ToString());
			_logger.LogInformation("  Clobbering {NumFiles} files", context.ClobberFiles.Count);

			if (context.Options.HasFlag(WorkspaceUpdateOptions.Sync))
			{
				_eventMonitor.PostEvent(context.ChangeNumber, EventType.Syncing);
			}

			if (context.Options.HasFlag(WorkspaceUpdateOptions.Sync) || context.Options.HasFlag(WorkspaceUpdateOptions.Build))
			{
				if (!context.Options.HasFlag(WorkspaceUpdateOptions.ContentOnly) && (context.CustomBuildSteps == null || context.CustomBuildSteps.Count == 0))
				{
					bool usingPrecompiledEditor = context.ArchiveTypeToArchive.TryGetValue(IArchiveChannel.EditorArchiveType, out IArchive? archive) && archive != null;
					bool usingLastSyncedEditorArchive = archive != null && archive.Key == _workspace.State.LastSyncEditorArchive;
					if (!usingPrecompiledEditor || !usingLastSyncedEditorArchive)
					{
						FileReference targetFile = ConfigUtils.GetEditorTargetFile(_workspace.Project, _workspace.ProjectConfigFile);
						foreach (BuildConfig config in Enum.GetValues(typeof(BuildConfig)).OfType<BuildConfig>())
						{
							FileReference receiptFile = ConfigUtils.GetReceiptFile(_workspace.Project, _workspace.ProjectConfigFile, targetFile, config.ToString());
							if (FileReference.Exists(receiptFile))
							{
								try
								{
									FileReference.Delete(receiptFile);
								}
								catch (Exception) { }
							}
						}
					}
				}
			}

			SyncLog.Clear();
			_workspace.Update(context);
			UpdateSyncActionCheckboxes();
			Refresh();
			_updateTimer.Start();
		}

		void CancelWorkspaceUpdate()
		{
			if (_workspace != null && _workspace.IsBusy() && MessageBox.Show("Are you sure you want to cancel the current operation?", "Cancel operation", MessageBoxButtons.YesNo) == DialogResult.Yes)
			{
				_workspace.CancelUpdate();

				if (_updateCallback != null)
				{
					_updateCallback(WorkspaceUpdateResult.Canceled);
					_updateCallback = null;
				}

				_updateTimer.Stop();

				UpdateSyncActionCheckboxes();
				Refresh();
				UpdateStatusPanel();
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
				_owner.UpdateProgress();
			}
		}

		void StateChangedCallback(ReadOnlyWorkspaceState state)
		{
			if (!_isDisposing)
			{
				_changeNumberToLayoutInfo.Clear();
				UpdateBuildList();
				UpdateStatusPanel();
			}
		}

		void UpdateCompleteCallback(WorkspaceUpdateContext context, WorkspaceUpdateResult result, string resultMessage)
		{
			_mainThreadSynchronizationContext.Post((o) =>
			{
				if (!_isDisposing)
				{
					UpdateComplete(context, result);
				}
			}, null);
		}

		void UpdateComplete(WorkspaceUpdateContext context, WorkspaceUpdateResult result)
		{
			if (_isDisposing)
			{
				return;
			}

			_updateTimer.Stop();

			if (result == WorkspaceUpdateResult.FilesToResolve)
			{
				FlashWindow(ParentForm.Handle, true);
				MessageBox.Show(ParentForm, "You have files to resolve after syncing your workspace. Please check P4.");
			}
			else if (result == WorkspaceUpdateResult.FilesToDelete)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Paused, 0.0f);
				_owner.UpdateProgress();

				FlashWindow(ParentForm.Handle, true);

				if (context.DeleteFiles.Count > 2000)
				{
					if (MessageBox.Show($"{context.DeleteFiles.Count:n0} files will be removed from your workspace by the current sync filter. Would you like to continue?", "Delete Files", MessageBoxButtons.OKCancel) == DialogResult.OK)
					{
						StartWorkspaceUpdate(context, _updateCallback);
						return;
					}
				}
				else
				{
					using DeleteWindow window = new DeleteWindow(context.DeleteFiles);
					if (window.ShowDialog(this) == DialogResult.OK)
					{
						StartWorkspaceUpdate(context, _updateCallback);
						return;
					}
				}
			}
			else if (result == WorkspaceUpdateResult.FilesToClobber)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Paused, 0.0f);
				_owner.UpdateProgress();

				HashSet<string> uncontrolledFiles = new HashSet<string>();

				if (SelectedProject.LocalPath != null && SelectedProject.LocalPath.EndsWith(".uprojectdirs", StringComparison.InvariantCultureIgnoreCase))
				{
					List<string> projectRoots = GetProjectRoots(SelectedProject.LocalPath);

					foreach (string projectRoot in projectRoots)
					{
						ParseUncontrolledChangelistsPersistencyFile(projectRoot, uncontrolledFiles);
					}
				}
				else
				{
					ParseUncontrolledChangelistsPersistencyFile(Path.GetDirectoryName(SelectedProject.LocalPath)!, uncontrolledFiles);
				}

				if (_settings.Global.AlwaysClobberFiles)
				{
					StartWorkspaceUpdate(context, _updateCallback);
					return;
				}
				else
				{
					using ClobberWindow window = new ClobberWindow(context.ClobberFiles, uncontrolledFiles);
					if (window.ShowDialog(this) == DialogResult.OK)
					{
						StartWorkspaceUpdate(context, _updateCallback);
						return;
					}
				}
			}
			else if (result == WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace)
			{
				_eventMonitor.PostEvent(context.ChangeNumber, EventType.DoesNotCompile);
			}
			else if (result == WorkspaceUpdateResult.Success)
			{
				if (context.Options.HasFlag(WorkspaceUpdateOptions.Build))
				{
					_eventMonitor.PostEvent(context.ChangeNumber, EventType.Compiles);
				}
				if (context.Options.HasFlag(WorkspaceUpdateOptions.RunAfterSync))
				{
					LaunchEditor();
				}
				if (context.Options.HasFlag(WorkspaceUpdateOptions.OpenSolutionAfterSync))
				{
					OpenSolution();
				}
			}

			if (_updateCallback != null)
			{
				_updateCallback(result);
				_updateCallback = null;
			}

			DesiredTaskbarState = Tuple.Create((result == WorkspaceUpdateResult.Success) ? TaskbarState.NoProgress : TaskbarState.Error, 0.0f);
			_owner.UpdateProgress();

			BuildList.Invalidate();
			Refresh();
			UpdateStatusPanel();
			UpdateSyncActionCheckboxes();

			FlashWindow(ParentForm.Handle, true);

			// Do this last because it may result in the control being disposed
			if (result == WorkspaceUpdateResult.FailedToSyncLoginExpired)
			{
				LoginExpired();
			}
		}

		void UpdateBuildListCallback()
		{
			if (!_updateBuildListPosted)
			{
				_updateBuildListPosted = true;
				_mainThreadSynchronizationContext.Post((o) =>
				{
					_updateBuildListPosted = false;
					if (!_isDisposing)
					{
						UpdateBuildList();
					}
				}, null);
			}
		}

		void UpdateBuildList()
		{
			if (SelectedFileName != null)
			{
				_archiveToChangeNumberToArchiveKey.Clear();
				_changeNumberToLayoutInfo.Clear();

				List<ChangesRecord> changes = _perforceMonitor.GetChanges();
				_eventMonitor.FilterChanges(changes.Select(x => x.Number));

				_promotedChangeNumbers = _perforceMonitor.GetPromotedChangeNumbers();

				string[] excludeChanges = Array.Empty<string>();
				if (_workspace != null)
				{
					ConfigFile projectConfigFile = _perforceMonitor.LatestProjectConfigFile;
					if (projectConfigFile != null)
					{
						excludeChanges = projectConfigFile.GetValues("Options.ExcludeChanges", excludeChanges);
					}
				}

				bool firstChange = true;
				bool hideUnreviewed = !_settings.ShowUnreviewedChanges;

				_numChanges = changes.Count;
				_listIndexToChangeIndex = new List<int>();
				_sortedChangeNumbers = new List<int>();

				List<ChangesRecord> filteredChanges = new List<ChangesRecord>();
				for (int changeIdx = 0; changeIdx < changes.Count; changeIdx++)
				{
					ChangesRecord change = changes[changeIdx];
					if (ShouldShowChange(change, excludeChanges) || _promotedChangeNumbers.Contains(change.Number))
					{
						_sortedChangeNumbers.Add(change.Number);

						if (!hideUnreviewed || (!_eventMonitor.IsUnderInvestigation(change.Number) && (ShouldIncludeInReviewedList(change.Number) || firstChange)))
						{
							firstChange = false;

							// Add the new change
							filteredChanges.Add(change);

							// Store off the list index for this change
							_listIndexToChangeIndex.Add(changeIdx);
						}
					}
				}

				UpdateBuildListInternal(filteredChanges, changes.Count > 0);

				_sortedChangeNumbers.Sort();

				if (_pendingSelectedChangeNumber != -1)
				{
					SelectChange(_pendingSelectedChangeNumber);
				}
			}

			if (BuildList.HoverItem > BuildList.Items.Count)
			{
				BuildList.HoverItem = -1;
			}

			UpdateBuildFailureNotification();

			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();
		}

		void UpdateBuildListInternal(List<ChangesRecord> changes, bool showExpandItem)
		{
			BuildList.BeginUpdate();

			// Remove any changes that no longer exist, and update the rest
			Dictionary<int, ChangesRecord> changeNumberToSummary = changes.ToDictionary(x => x.Number, x => x);
			for (int idx = BuildList.Items.Count - 1; idx >= 0; idx--)
			{
				ChangesRecord? change = BuildList.Items[idx].Tag as ChangesRecord;
				if (change != null)
				{
					ChangesRecord? summary;
					if (changeNumberToSummary.TryGetValue(change.Number, out summary))
					{
						// Update
						BuildList.Items[idx].SubItems[DescriptionColumn.Index].Text = change.Description!.Replace('\n', ' ');
						changeNumberToSummary.Remove(change.Number);
					}
					else
					{
						// Delete
						BuildList.Items.RemoveAt(idx);
					}
				}
			}

			// Add everything left over
			foreach (ChangesRecord change in changeNumberToSummary.Values)
			{
				BuildList_AddItem(change);
			}

			// Figure out which group the expand item should be in
			ListViewGroup? newExpandItemGroup = null;
			for (int idx = BuildList.Items.Count - 1; idx >= 0; idx--)
			{
				ListViewItem item = BuildList.Items[idx];
				if (item != _expandItem)
				{
					newExpandItemGroup = item.Group;
					break;
				}
			}

			// Remove the expand row if it's in the wrong place
			if (_expandItem != null)
			{
				if (!showExpandItem || _expandItem.Group != newExpandItemGroup)
				{
					BuildList.Items.Remove(_expandItem);
					_expandItem = null;
				}
			}

			// Remove any empty groups
			for (int idx = BuildList.Groups.Count - 1; idx >= 0; idx--)
			{
				if (BuildList.Groups[idx].Items.Count == 0)
				{
					BuildList.Groups.RemoveAt(idx);
				}
			}

			// Add the expand row back in
			if (showExpandItem && _expandItem == null)
			{
				_expandItem = new ListViewItem(newExpandItemGroup);
				_expandItem.Tag = null;
				_expandItem.Selected = false;
				_expandItem.Text = "";
				for (int columnIdx = 1; columnIdx < BuildList.Columns.Count; columnIdx++)
				{
					_expandItem.SubItems.Add(new ListViewItem.ListViewSubItem(_expandItem, ""));
				}
				BuildList.Items.Add(_expandItem);
			}

			BuildList.EndUpdate();
		}

		ListViewGroup BuildList_FindOrAddGroup(ChangesRecord change, string groupName)
		{
			// Find or add the new group
			int groupIndex = 0;
			for (; groupIndex < BuildList.Groups.Count; groupIndex++)
			{
				ListViewGroup nextGroup = BuildList.Groups[groupIndex];
				if (nextGroup.Header == groupName)
				{
					return nextGroup;
				}

				ChangesRecord? lastChange = null;
				for (int idx = nextGroup.Items.Count - 1; idx >= 0 && lastChange == null; idx--)
				{
					lastChange = nextGroup.Items[idx].Tag as ChangesRecord;
				}
				if (lastChange == null || change.Number > lastChange.Number)
				{
					break;
				}
			}

			// Create the new group
			ListViewGroup group = new ListViewGroup(groupName);
			BuildList.Groups.Insert(groupIndex, group);
			return group;
		}

		void BuildList_AddItem(ChangesRecord change)
		{
			// Get the display time for this item
			DateTime displayTime;
			if (_settings.ShowLocalTimes)
			{
				displayTime = change.Time.ToLocalTime();
			}
			else
			{
				displayTime = new DateTime(change.Time.Ticks + _perforceMonitor.ServerTimeZone.Ticks, DateTimeKind.Local);
			}

			// Find or add the new group
			ListViewGroup group = BuildList_FindOrAddGroup(change, displayTime.ToString("D"));

			// Create the new item
			ListViewItem item = new ListViewItem();
			item.Tag = change;

			// Get the new text for each column
			string[] columns = new string[BuildList.Columns.Count];
			columns[ChangeColumn.Index] = String.Format("{0}", change.Number);

			columns[TimeColumn.Index] = displayTime.ToShortTimeString();
			string userName = FormatUserName(change.User!);

			// If annotate robomerge is on, add a prefix to the user name
			if (_settings.AnnotateRobmergeChanges && IsRobomergeChange(change))
			{
				userName += " (robo)";
			}

			columns[AuthorColumn.Index] = userName;
			columns[DescriptionColumn.Index] = change.Description!.Replace('\n', ' ');

			for (int columnIdx = 1; columnIdx < BuildList.Columns.Count; columnIdx++)
			{
				item.SubItems.Add(new ListViewItem.ListViewSubItem(item, columns[columnIdx] ?? ""));
			}

			// Insert it at the right position within the group
			int groupInsertIdx = 0;
			for (; groupInsertIdx < group.Items.Count; groupInsertIdx++)
			{
				ChangesRecord? otherChange = group.Items[groupInsertIdx].Tag as ChangesRecord;
				if (otherChange == null || change.Number >= otherChange.Number)
				{
					break;
				}
			}
			group.Items.Insert(groupInsertIdx, item);

			// Insert it into the build list
			BuildList.Items.Add(item);
		}

		/// <summary>
		/// Returns true if this change was submitted on behalf of someone by robomerge
		/// </summary>
		/// <param name="change"></param>
		/// <returns></returns>
		static bool IsRobomergeChange(ChangesRecord change)
		{
			// If hiding robomerge, filter out based on workspace name. Note - don't look at the description because we
			// *do* want to see conflicts that were manually merged by someone
			return change.Client!.Contains("ROBOMERGE", StringComparison.OrdinalIgnoreCase);
		}

		bool ShouldShowChange(ChangesRecord change, string[] excludeChanges)
		{
			if (_projectSettings.FilterType != FilterType.None)
			{
				PerforceChangeDetails? details;
				if (!_perforceMonitor.TryGetChangeDetails(change.Number, out details))
				{
					return false;
				}
				if (_projectSettings.FilterType == FilterType.Code && !details.ContainsCode)
				{
					return false;
				}
				if (_projectSettings.FilterType == FilterType.Content && !details.ContainsContent)
				{
					return false;
				}
			}

			// if filtering by user, only show changes where the author contains the filter text
			if (!String.IsNullOrEmpty(_authorFilterText)
				&& _authorFilterText != AuthorFilterPlaceholderText
				&& change.User!.IndexOf(_authorFilterText, StringComparison.OrdinalIgnoreCase) < 0)
			{
				return false;
			}

			// If this is a robomerge change, check if any filters will cause it to be hidden
			if (IsRobomergeChange(change))
			{
				if (_settings.ShowRobomerge == UserSettings.RobomergeShowChangesOption.None)
				{
					return false;
				}
				else if (_settings.ShowRobomerge == UserSettings.RobomergeShowChangesOption.Badged)
				{
					// if this change has any badges, we'll show it unless it's later filtered out
					EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);
					if (summary == null || !summary.Badges.Any())
					{
						return false;
					}
				}
			}

			if (_projectSettings.FilterBadges.Count > 0)
			{
				EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);
				if (summary == null || !summary.Badges.Any(x => _projectSettings.FilterBadges.Contains(x.BadgeName)))
				{
					return false;
				}
			}
			if (!_settings.ShowAutomatedChanges)
			{
				foreach (string excludeChange in excludeChanges)
				{
					if (Regex.IsMatch(change.Description, excludeChange, RegexOptions.IgnoreCase))
					{
						return false;
					}
				}

				if (String.Equals(change.User, "buildmachine", StringComparison.OrdinalIgnoreCase) && !change.Description!.Contains("lightmaps", StringComparison.OrdinalIgnoreCase))
				{
					return false;
				}
			}
			if (IsBisectModeEnabled() && !_workspace.State.BisectChanges.Any(x => x.Change == change.Number))
			{
				return false;
			}
			return true;
		}

		void UpdateBuildMetadataCallback()
		{
			if (!_updateBuildMetadataPosted)
			{
				_updateBuildMetadataPosted = true;
				_mainThreadSynchronizationContext.Post((o) =>
				{
					_updateBuildMetadataPosted = false;
					if (!_isDisposing)
					{
						UpdateBuildMetadata();
					}
				}, null);
			}
		}

		string? GetIssuesApiUrl()
		{
			string? issuesApiUrl;
			if (!TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "IssuesApiUrl", out issuesApiUrl))
			{
				issuesApiUrl = _apiUrl;
			}
			return issuesApiUrl;
		}

		void UpdateBuildMetadata()
		{
			// Refresh the issue monitor if it's changed
			string? issuesApiUrl = GetIssuesApiUrl();
			if (issuesApiUrl != _issueMonitor.ApiUrl)
			{
				_logger.LogInformation("Changing issues API url from {OldApiUrl} to {ApiUrl}", _issueMonitor.ApiUrl, issuesApiUrl);
				IssueMonitor newIssueMonitor = _owner.CreateIssueMonitor(issuesApiUrl, _perforceSettings.UserName);
				_owner.ReleaseIssueMonitor(_issueMonitor);
				_issueMonitor = newIssueMonitor;
			}

			// Update the column settings first, since this may affect the badges we hide
			UpdateColumnSettings(false);

			// Clear the badge size cache
			_badgeLabelToSize.Clear();

			// Reset the badge groups
			Dictionary<string, string> badgeNameToGroup = new Dictionary<string, string>();

			// Read the group mappings from the config file
			string? groupDefinitions;
			if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "BadgeGroups", out groupDefinitions))
			{
				string[] groupDefinitionsArray = groupDefinitions.Split('\n');
				for (int idx = 0; idx < groupDefinitionsArray.Length; idx++)
				{
					string groupName = String.Format("{0:0000}", idx);
					foreach (string badgeName in groupDefinitionsArray[idx].Split(',').Select(x => x.Trim()))
					{
						badgeNameToGroup[badgeName] = groupName;
					}
				}
			}

			// Add a dummy group for any other badges we have
			foreach (ListViewItem? item in BuildList.Items)
			{
				if (item != null && item.Tag != null)
				{
					EventSummary? summary = _eventMonitor.GetSummaryForChange(((ChangesRecord)item.Tag).Number);
					if (summary != null)
					{
						foreach (BadgeData badge in summary.Badges)
						{
							string badgeSlot = badge.BadgeName;
							if (!badgeNameToGroup.ContainsKey(badgeSlot))
							{
								badgeNameToGroup.Add(badgeSlot, "XXXX");
							}
						}
					}
				}
			}

			// Remove anything that's a service badge
			foreach (ServiceBadgeInfo serviceBadge in _serviceBadges)
			{
				badgeNameToGroup.Remove(serviceBadge.Name);
			}

			// Remove everything that's in a custom column
			foreach (ColumnHeader customColumn in _customColumns)
			{
				ConfigObject columnConfig = (ConfigObject)customColumn.Tag;
				foreach (string badgeName in columnConfig.GetValue("Badges", "").Split(','))
				{
					badgeNameToGroup.Remove(badgeName);
				}
			}

			// Sort the list of groups for display
			_badgeNameAndGroupPairs = badgeNameToGroup.OrderBy(x => x.Value).ThenBy(x => x.Key).ToList();

			// Figure out whether to show smaller badges due to limited space
			UpdateMaxBuildBadgeChars();

			// Update everything else
			_archiveToChangeNumberToArchiveKey.Clear();
			_changeNumberToLayoutInfo.Clear();
			BuildList.Invalidate();
			UpdateServiceBadges();
			UpdateStatusPanel();
			UpdateBuildFailureNotification();
			CheckForStartupComplete();

			// If we are filtering by badges, we may also need to update the build list
			if (_projectSettings.FilterType != FilterType.None || _projectSettings.FilterBadges.Count > 0)
			{
				UpdateBuildList();
			}
		}

		void UpdateMaxBuildBadgeChars()
		{
			int newMaxBuildBadgeChars;
			if (GetBuildBadgeStripWidth(-1) < CISColumn.Width)
			{
				newMaxBuildBadgeChars = -1;
			}
			else if (GetBuildBadgeStripWidth(3) < CISColumn.Width)
			{
				newMaxBuildBadgeChars = 3;
			}
			else if (GetBuildBadgeStripWidth(2) < CISColumn.Width)
			{
				newMaxBuildBadgeChars = 2;
			}
			else
			{
				newMaxBuildBadgeChars = 1;
			}

			if (newMaxBuildBadgeChars != _maxBuildBadgeChars)
			{
				_changeNumberToLayoutInfo.Clear();
				BuildList.Invalidate();
				_maxBuildBadgeChars = newMaxBuildBadgeChars;
			}
		}

		int GetBuildBadgeStripWidth(int maxNumChars)
		{
			// Create dummy badges for each badge name
			List<BadgeInfo> dummyBadges = new List<BadgeInfo>();
			foreach (KeyValuePair<string, string> badgeNameAndGroupPair in _badgeNameAndGroupPairs)
			{
				string badgeName = badgeNameAndGroupPair.Key;
				if (maxNumChars != -1 && badgeName.Length > maxNumChars)
				{
					badgeName = badgeName.Substring(0, maxNumChars);
				}

				BadgeInfo dummyBadge = CreateBadge(-1, badgeName, badgeNameAndGroupPair.Value, null);
				dummyBadges.Add(dummyBadge);
			}

			// Check if they fit within the column
			int width = 0;
			if (dummyBadges.Count > 0)
			{
				LayoutBadges(dummyBadges);
				width = dummyBadges[^1]._offset + dummyBadges[^1]._width;
			}
			return width;
		}

		bool ShouldIncludeInReviewedList(int changeNumber)
		{
			if (_promotedChangeNumbers.Contains(changeNumber))
			{
				return true;
			}

			EventSummary? review = _eventMonitor.GetSummaryForChange(changeNumber);
			if (review != null)
			{
				if (review.LastStarReview != null && review.LastStarReview.Type == EventType.Starred)
				{
					return true;
				}
				if (review.Verdict == ReviewVerdict.Good || review.Verdict == ReviewVerdict.Mixed)
				{
					return true;
				}
			}
			return false;
		}

		Dictionary<Guid, WorkspaceSyncCategory> GetSyncCategories()
		{
			return ConfigUtils.GetSyncCategories(_perforceMonitor.LatestProjectConfigFile);
		}

		static List<string> GetProjectRoots(string inUProjectDirsPath)
		{
			List<string> projectRoots = new List<string>();

			if (!File.Exists(inUProjectDirsPath))
			{
				return projectRoots;
			}

			string uProjectDirsRoot = Path.GetDirectoryName(inUProjectDirsPath)!;

			foreach (string line in File.ReadLines(inUProjectDirsPath))
			{
				// Remove unnecessary whitespaces
				string trimmedLine = line.Trim();

				// Remove every comments
				int commentIndex = trimmedLine.IndexOf(';', StringComparison.Ordinal);

				if (commentIndex >= 0)
				{
					trimmedLine = trimmedLine.Substring(0, commentIndex);
				}

				if (trimmedLine.Length <= 0)
				{
					continue;
				}

				string uProjectDirsEntry = Path.Combine(uProjectDirsRoot, trimmedLine);

				if (!Directory.Exists(uProjectDirsEntry))
				{
					continue;
				}

				List<string> entryDirectories = new List<string>(Directory.EnumerateDirectories(uProjectDirsEntry));

				projectRoots = projectRoots.Concat(entryDirectories).ToList();
			}

			return projectRoots;
		}

		void ParseUncontrolledChangelistsPersistencyFile(string inProjectRoot, HashSet<string> outUncontrolledFiles)
		{
			string uncontrolledChangelistPersistencyFilePath = Path.Combine(inProjectRoot, "Saved", "SourceControl", "UncontrolledChangelists.json");

			if (File.Exists(uncontrolledChangelistPersistencyFilePath))
			{
				try
				{
					string jsonString = File.ReadAllText(uncontrolledChangelistPersistencyFilePath);
					UncontrolledChangelistPersistency? uclPersistency = JsonSerializer.Deserialize<UncontrolledChangelistPersistency>(jsonString, Utility.DefaultJsonSerializerOptions);
					if (uclPersistency != null)
					{
						foreach (UncontrolledChangelist ucl in uclPersistency.Changelists)
						{
							foreach (string uncontrolledFile in ucl.Files)
							{
								outUncontrolledFiles.Add(uncontrolledFile);
							}
						}
					}
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Exception while parsing Uncontrolled Changelist persistency file");
				}
			}
		}

		void UpdateReviewsCallback()
		{
			if (!_updateReviewsPosted)
			{
				_updateReviewsPosted = true;
				_mainThreadSynchronizationContext.Post((o) =>
				{
					_updateReviewsPosted = false;
					if (!_isDisposing)
					{
						UpdateReviews();
					}
				}, null);
			}
		}

		void UpdateReviews()
		{
			_archiveToChangeNumberToArchiveKey.Clear();
			_changeNumberToLayoutInfo.Clear();
			_eventMonitor.ApplyUpdates();

			if (UpdateServiceBadges())
			{
				UpdateStatusPanel();
			}

			Refresh();
			UpdateBuildFailureNotification();
			CheckForStartupComplete();
		}

		void UpdateBuildFailureNotification()
		{
			if (!DeploymentSettings.Instance.EnableAlerts)
			{
				return;
			}

			// Ignore this if we're using the new build health system
			if (GetDefaultIssueFilter() != null)
			{
				return;
			}

			int lastChangeByCurrentUser = _perforceMonitor.LastChangeByCurrentUser;
			int lastCodeChangeByCurrentUser = _perforceMonitor.LastCodeChangeByCurrentUser;

			// Find all the badges which should notify users due to content changes
			HashSet<string> contentBadges = new HashSet<string>();
			contentBadges.UnionWith(_perforceMonitor.LatestProjectConfigFile.GetValues("Notifications.ContentBadges", Array.Empty<string>()));

			// Find the most recent build of each type, and the last time it succeeded
			Dictionary<string, BadgeData> typeToLastBuild = new Dictionary<string, BadgeData>();
			Dictionary<string, BadgeData> typeToLastSucceededBuild = new Dictionary<string, BadgeData>();
			for (int idx = _sortedChangeNumbers.Count - 1; idx >= 0; idx--)
			{
				EventSummary? summary = _eventMonitor.GetSummaryForChange(_sortedChangeNumbers[idx]);
				if (summary != null)
				{
					foreach (BadgeData badge in summary.Badges)
					{
						if (!typeToLastBuild.ContainsKey(badge.BuildType) && (badge.Result == BadgeResult.Success || badge.Result == BadgeResult.Warning || badge.Result == BadgeResult.Failure))
						{
							typeToLastBuild.Add(badge.BuildType, badge);
						}
						if (!typeToLastSucceededBuild.ContainsKey(badge.BuildType) && badge.Result == BadgeResult.Success)
						{
							typeToLastSucceededBuild.Add(badge.BuildType, badge);
						}
					}
				}
			}

			// Find all the build types that the user needs to be notified about.
			int requireNotificationForChange = -1;
			List<BadgeData> notifyBuilds = new List<BadgeData>();
			foreach (BadgeData lastBuild in typeToLastBuild.Values.OrderBy(x => x.BuildType))
			{
				if (lastBuild.Result == BadgeResult.Failure || lastBuild.Result == BadgeResult.Warning)
				{
					// Get the last submitted changelist by this user of the correct type
					int lastChangeByCurrentUserOfType;
					if (contentBadges.Contains(lastBuild.BuildType))
					{
						lastChangeByCurrentUserOfType = lastChangeByCurrentUser;
					}
					else
					{
						lastChangeByCurrentUserOfType = lastCodeChangeByCurrentUser;
					}

					// Check if the failed build was after we submitted
					if (lastChangeByCurrentUserOfType > 0 && lastBuild.ChangeNumber >= lastChangeByCurrentUserOfType)
					{
						// And check that there wasn't a successful build after we submitted (if there was, we're in the clear)
						BadgeData? lastSuccessfulBuild;
						if (!typeToLastSucceededBuild.TryGetValue(lastBuild.BuildType, out lastSuccessfulBuild) || lastSuccessfulBuild.ChangeNumber < lastChangeByCurrentUserOfType)
						{
							// Add it to the list of notifications
							notifyBuilds.Add(lastBuild);

							// Check if this is a new notification, rather than one we've already dismissed
							int notifiedChangeNumber;
							if (!_notifiedBuildTypeToChangeNumber.TryGetValue(lastBuild.BuildType, out notifiedChangeNumber) || notifiedChangeNumber < lastChangeByCurrentUserOfType)
							{
								requireNotificationForChange = Math.Max(requireNotificationForChange, lastChangeByCurrentUserOfType);
							}
						}
					}
				}
			}

			// If there's anything we haven't already notified the user about, do so now
			if (requireNotificationForChange != -1)
			{
				// Format the platform list
				StringBuilder platformList = new StringBuilder();
				platformList.AppendFormat("{0}: {1}", StreamName, notifyBuilds[0].BuildType);
				for (int idx = 1; idx < notifyBuilds.Count - 1; idx++)
				{
					platformList.AppendFormat(", {0}", notifyBuilds[idx].BuildType);
				}
				if (notifyBuilds.Count > 1)
				{
					platformList.AppendFormat(" and {0}", notifyBuilds[^1].BuildType);
				}

				// Show the balloon tooltip
				if (notifyBuilds.Any(x => x.Result == BadgeResult.Failure))
				{
					string title = String.Format("{0} Errors", platformList.ToString());
					string message = String.Format("CIS failed after your last submitted changelist ({0}).", requireNotificationForChange);
					_notificationWindow.Show(NotificationType.Error, title, message);
				}
				else
				{
					string title = String.Format("{0} Warnings", platformList.ToString());
					string message = String.Format("CIS completed with warnings after your last submitted changelist ({0}).", requireNotificationForChange);
					_notificationWindow.Show(NotificationType.Warning, title, message);
				}

				// Set the link to open the right build pages
				int highlightChange = notifyBuilds.Max(x => x.ChangeNumber);
				_notificationWindow.OnMoreInformation = () =>
				{
					_owner.ShowAndActivate();
					SelectChange(highlightChange);
				};

				// Don't show messages for this change again
				foreach (BadgeData notifyBuild in notifyBuilds)
				{
					_notifiedBuildTypeToChangeNumber[notifyBuild.BuildType] = requireNotificationForChange;
				}
			}
		}

		private void BuildList_DrawColumnHeader(object sender, DrawListViewColumnHeaderEventArgs e)
		{
			e.DrawDefault = true;
		}

		class ExpandRowLayout
		{
			public string? _mainText;
			public Rectangle _mainRect;
			public string? _linkText;
			public Rectangle _linkRect;
		}

		ExpandRowLayout LayoutExpandRow(Graphics graphics, Rectangle bounds)
		{
			ExpandRowLayout layout = new ExpandRowLayout();

			string showingChanges = String.Format("Showing {0}/{1} changes.", _listIndexToChangeIndex.Count, _numChanges);

			int currentMaxChanges = _perforceMonitor.CurrentMaxChanges;
			if (_perforceMonitor.PendingMaxChanges > currentMaxChanges)
			{
				layout._mainText = String.Format("{0}. Fetching {1} more from server...  ", showingChanges, _perforceMonitor.PendingMaxChanges - currentMaxChanges);
				layout._linkText = "Cancel";
			}
			else if (_perforceMonitor.CurrentMaxChanges > _numChanges)
			{
				layout._mainText = showingChanges;
				layout._linkText = "";
			}
			else
			{
				layout._mainText = String.Format("{0}  ", showingChanges);
				layout._linkText = "Show more...";
			}

			Size mainTextSize = TextRenderer.MeasureText(graphics, layout._mainText, _buildFont, new Size(1000, 1000), TextFormatFlags.NoPadding);
			Size linkTextSize = TextRenderer.MeasureText(graphics, layout._linkText, _buildFont, new Size(1000, 1000), TextFormatFlags.NoPadding);

			int minX = bounds.Left + (bounds.Width - mainTextSize.Width - linkTextSize.Width) / 2;
			int minY = bounds.Bottom - mainTextSize.Height - 1;

			layout._mainRect = new Rectangle(new Point(minX, minY), mainTextSize);
			layout._linkRect = new Rectangle(new Point(minX + mainTextSize.Width, minY), linkTextSize);

			return layout;
		}

		private void DrawExpandRow(Graphics graphics, ExpandRowLayout layout)
		{
			TextRenderer.DrawText(graphics, layout._mainText, _buildFont, layout._mainRect, SystemColors.WindowText, TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix | TextFormatFlags.NoPadding);

			Color linkColor = SystemColors.HotTrack;
			if (_mouseOverExpandLink)
			{
				linkColor = Color.FromArgb(linkColor.B, linkColor.G, linkColor.R);
			}

			TextRenderer.DrawText(graphics, layout._linkText, _buildFont, layout._linkRect, linkColor, TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix | TextFormatFlags.NoPadding);
		}

		private bool HitTestExpandLink(Point location)
		{
			if (_expandItem == null)
			{
				return false;
			}
			using (Graphics graphics = Graphics.FromHwnd(IntPtr.Zero))
			{
				ExpandRowLayout layout = LayoutExpandRow(graphics, _expandItem.Bounds);
				return layout._linkRect.Contains(location);
			}
		}

		private void BuildList_DrawItem(object sender, DrawListViewItemEventArgs e)
		{
			if (e.Item == _expandItem)
			{
				BuildList.DrawDefaultBackground(e.Graphics, e.Bounds);

				ExpandRowLayout expandLayout = LayoutExpandRow(e.Graphics, e.Bounds);
				DrawExpandRow(e.Graphics, expandLayout);
			}
			else if (_workspace != null)
			{
				if (e.Item.Selected)
				{
					BuildList.DrawSelectedBackground(e.Graphics, e.Bounds);
				}
				else if (e.ItemIndex == BuildList.HoverItem)
				{
					BuildList.DrawTrackedBackground(e.Graphics, e.Bounds);
				}
				else if (((ChangesRecord)e.Item.Tag).Number == _workspace.PendingChangeNumber)
				{
					BuildList.DrawTrackedBackground(e.Graphics, e.Bounds);
				}
				else
				{
					BuildList.DrawDefaultBackground(e.Graphics, e.Bounds);
				}
			}
		}

		private IArchive? GetArchiveForChangeNumber(IArchiveChannel archiveChannel, int changeNumber)
		{
			Dictionary<int, IArchive?>? changeNumberToArchive;
			if (!_archiveToChangeNumberToArchiveKey.TryGetValue(archiveChannel.Name, out changeNumberToArchive))
			{
				changeNumberToArchive = new Dictionary<int, IArchive?>();
				_archiveToChangeNumberToArchiveKey[archiveChannel.Name] = changeNumberToArchive;
			}
			return GetArchiveForChangeNumber(archiveChannel, changeNumber, changeNumber, changeNumberToArchive);
		}

		private IArchive? GetArchiveForChangeNumber(IArchiveChannel archiveChannel, int changeNumber, int maxChangeNumber, Dictionary<int, IArchive?> changeNumberToArchive)
		{
			IArchive? archive;
			if (!changeNumberToArchive.TryGetValue(changeNumber, out archive))
			{
				archive = GetArchiveForChangeNumberUncached(archiveChannel, changeNumber, maxChangeNumber);
				changeNumberToArchive.Add(changeNumber, archive);
			}
			return archive;
		}

		private IArchive? GetArchiveForChangeNumberUncached(IArchiveChannel archiveChannel, int changeNumber, int maxChangeNumber)
		{
			int idx = _sortedChangeNumbers.BinarySearch(changeNumber);
			if (idx < 0)
			{
				return null;
			}

			PerforceChangeDetails? details;
			while (idx > 0 && _perforceMonitor.TryGetChangeDetails(_sortedChangeNumbers[idx], out details) && !details.ContainsCode)
			{
				idx--;
			}

			for (; ; )
			{
				IArchive? archive = archiveChannel.TryGetArchiveForChangeNumber(_sortedChangeNumbers[idx], maxChangeNumber);
				if (archive != null)
				{
					return archive;
				}

				idx++;
				if (idx >= _sortedChangeNumbers.Count 
					|| !_perforceMonitor.TryGetChangeDetails(_sortedChangeNumbers[idx], out details) 
					|| details.ContainsCode)
				{
					return null;
				}
			}
		}

		private static Color Blend(Color first, Color second, float t)
		{
			return Color.FromArgb((int)(first.R + (second.R - first.R) * t), (int)(first.G + (second.G - first.G) * t), (int)(first.B + (second.B - first.B) * t));
		}

		private bool CanSyncChange(int changeNumber)
		{
			if (_perforceMonitor == null)
			{
				return false;
			}

			List<IArchiveChannel> selectedArchives = GetSelectedArchiveChannels(GetArchiveChannels());
			return selectedArchives.Count == 0 || selectedArchives.All(x => GetArchiveForChangeNumber(x, changeNumber) != null);
		}

		/// <summary>
		/// Should read as Do Required Badges Exist and Are They Good.
		/// Determines if the ini specified INI filter is a subset of the InBadges passed
		/// BadgeGroupSyncFilter - is used as the ini 
		/// </summary>
		private static bool DoRequiredBadgesExist(List<string> requiredBadgeList, List<BadgeData> inBadges)
		{
			Dictionary<string, BadgeData> inBadgeDictionary = new Dictionary<string, BadgeData>();
			foreach (BadgeData badge in inBadges)
			{
				inBadgeDictionary.Add(badge.BadgeName, badge);
			}

			// make sure all required badges exist in the InBadgeSet
			foreach (string badgeName in requiredBadgeList)
			{
				BadgeData? badge;
				if (inBadgeDictionary.TryGetValue(badgeName, out badge))
				{
					// If any required badge is not successful then the filter isn't matched.
					if (!badge.IsSuccess)
					{
						return false;
					}
				}
				else
				{
					// required badge not found.
					return false;
				}
			}

			// All required badges existed and were successful 
			return true;
		}

		private bool CanSyncChangeType(LatestChangeType changeType, ChangesRecord change, EventSummary? summary)
		{
			if (changeType.Good)
			{
				if (summary == null || summary.Verdict != ReviewVerdict.Good)
				{
					return false;
				}
			}

			if (changeType.Starred)
			{
				if ((summary == null || summary.LastStarReview == null || summary.LastStarReview.Type != EventType.Starred) && !_promotedChangeNumbers.Contains(change.Number))
				{
					return false;
				}
			}

			if (changeType.RequiredBadges.Count > 0)
			{
				if (summary == null || !DoRequiredBadgesExist(changeType.RequiredBadges, summary.Badges))
				{
					return false;
				}
			}

			return true;
		}

		private ChangeLayoutInfo GetChangeLayoutInfo(ChangesRecord change)
		{
			ChangeLayoutInfo? layoutInfo;
			if (!_changeNumberToLayoutInfo.TryGetValue(change.Number, out layoutInfo))
			{
				layoutInfo = new ChangeLayoutInfo();

				layoutInfo._descriptionBadges = CreateDescriptionBadges(change);
				layoutInfo._typeBadges = CreateTypeBadges(change.Number);

				EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);
				layoutInfo._buildBadges = CreateBuildBadges(change.Number, summary);
				layoutInfo._customBadges = CreateCustomBadges(change.Number, summary);

				_changeNumberToLayoutInfo[change.Number] = layoutInfo;
			}
			return layoutInfo;
		}

		private void GetRemainingBisectRange(out int outPassChangeNumber, out int outFailChangeNumber)
		{
			int passChangeNumber = -1;
			foreach (BisectEntry entry in _workspace.State.BisectChanges)
			{
				if (entry.State == BisectState.Pass && (entry.Change > passChangeNumber || passChangeNumber == -1))
				{
					passChangeNumber = entry.Change;
				}
			}

			int failChangeNumber = -1;
			foreach (BisectEntry entry in _workspace.State.BisectChanges)
			{
				if (entry.State == BisectState.Fail && entry.Change > passChangeNumber && (entry.Change < failChangeNumber || failChangeNumber == -1))
				{
					failChangeNumber = entry.Change;
				}
			}

			outPassChangeNumber = passChangeNumber;
			outFailChangeNumber = failChangeNumber;
		}

		private void BuildList_DrawSubItem(object sender, DrawListViewSubItemEventArgs e)
		{
			if (e.Item == null || e.SubItem == null)
			{
				return;
			}

			ChangesRecord? change = (ChangesRecord?)e.Item.Tag;
			if (change == null)
			{
				return;
			}

			float dpiScaleX = e.Graphics.DpiX / 96.0f;
			float dpiScaleY = e.Graphics.DpiY / 96.0f;

			float iconY = e.Bounds.Top + (e.Bounds.Height - 16 * dpiScaleY) / 2;

			using StringFormat format = new StringFormat();
			format.LineAlignment = StringAlignment.Center;
			format.FormatFlags = StringFormatFlags.NoWrap;
			format.Trimming = StringTrimming.EllipsisCharacter;

			Font currentFont = (change.Number == _workspace.PendingChangeNumber || change.Number == _workspace.CurrentChangeNumber) ? _selectedBuildFont! : _buildFont!;

			bool allowSync = CanSyncChange(change.Number);
			int badgeAlpha = allowSync ? 255 : 128;
			Color textColor = (allowSync || change.Number == _workspace.PendingChangeNumber || change.Number == _workspace.CurrentChangeNumber || _workspace.State.AdditionalChangeNumbers.Contains(change.Number)) ? SystemColors.WindowText : Blend(SystemColors.Window, SystemColors.WindowText, 0.25f);

			const int FadeRange = 6;
			if (e.ItemIndex >= BuildList.Items.Count - FadeRange && _numChanges >= _perforceMonitor.CurrentMaxChanges && !IsBisectModeEnabled())
			{
				float opacity = (float)(BuildList.Items.Count - e.ItemIndex - 0.9f) / FadeRange;
				badgeAlpha = (int)(badgeAlpha * opacity);
				textColor = Blend(SystemColors.Window, textColor, opacity);
			}

			if (e.ColumnIndex == IconColumn.Index)
			{
				EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);

				float minX = 4 * dpiScaleX;
				if ((summary != null && _eventMonitor.WasSyncedByCurrentUser(summary.ChangeNumber)) || (_workspace != null && _workspace.CurrentChangeNumber == change.Number))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, minX, iconY, PreviousSyncIcon, GraphicsUnit.Pixel);
				}
				else if (_workspaceSettings != null && _workspace != null && _workspace.State.AdditionalChangeNumbers.Contains(change.Number))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, minX, iconY, AdditionalSyncIcon, GraphicsUnit.Pixel);
				}
				else if (allowSync && ((summary != null && summary.LastStarReview != null && summary.LastStarReview.Type == EventType.Starred) || _promotedChangeNumbers.Contains(change.Number)))
				{
					e.Graphics.DrawImage(Properties.Resources.Icons, minX, iconY, PromotedBuildIcon, GraphicsUnit.Pixel);
				}
				minX += PromotedBuildIcon.Width * dpiScaleX;

				if (allowSync)
				{
					Rectangle qualityIcon = DefaultBuildIcon;

					if (IsBisectModeEnabled())
					{
						int firstPass, firstFail;
						GetRemainingBisectRange(out firstPass, out firstFail);

						BisectEntry? entry = _workspace?.State.BisectChanges.FirstOrDefault(x => x.Change == change.Number);
						if (entry == null || entry.State == BisectState.Exclude)
						{
							qualityIcon = new Rectangle(0, 0, 0, 0);
						}
						else if (entry.State == BisectState.Pass)
						{
							qualityIcon = BisectPassIcon;
						}
						else if (entry.State == BisectState.Fail)
						{
							qualityIcon = BisectFailIcon;
						}
						else if (firstFail != -1 && change.Number > firstFail)
						{
							qualityIcon = BisectImplicitFailIcon;
						}
						else if (firstPass != -1 && change.Number < firstPass)
						{
							qualityIcon = BisectImplicitPassIcon;
						}
					}
					else
					{
						if (_eventMonitor.IsUnderInvestigation(change.Number))
						{
							qualityIcon = BadBuildIcon;
						}
						else if (summary != null)
						{
							if (summary.Verdict == ReviewVerdict.Good)
							{
								qualityIcon = GoodBuildIcon;
							}
							else if (summary.Verdict == ReviewVerdict.Bad)
							{
								qualityIcon = BadBuildIcon;
							}
							else if (summary.Verdict == ReviewVerdict.Mixed)
							{
								qualityIcon = MixedBuildIcon;
							}
						}
					}
					e.Graphics.DrawImage(Properties.Resources.Icons, minX, iconY, qualityIcon, GraphicsUnit.Pixel);

					minX += qualityIcon.Width * dpiScaleX;
				}
			}
			else if (e.ColumnIndex == ChangeColumn.Index || e.ColumnIndex == TimeColumn.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, currentFont, e.Bounds, textColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if (e.ColumnIndex == AuthorColumn.Index)
			{
				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, currentFont, e.Bounds, textColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if (e.ColumnIndex == DescriptionColumn.Index)
			{
				ChangeLayoutInfo layout = GetChangeLayoutInfo(change);

				Rectangle remainingBounds = e.Bounds;
				if (layout._descriptionBadges.Count > 0)
				{
					e.Graphics.IntersectClip(e.Bounds);
					e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					remainingBounds = new Rectangle(remainingBounds.Left, remainingBounds.Top, remainingBounds.Width - (int)(2 * dpiScaleX), remainingBounds.Height);

					Point listLocation = GetBadgeListLocation(layout._descriptionBadges, remainingBounds, HorizontalAlign.Right, VerticalAlignment.Middle);
					DrawBadges(e.Graphics, listLocation, layout._descriptionBadges, badgeAlpha);

					remainingBounds = new Rectangle(remainingBounds.Left, remainingBounds.Top, listLocation.X - remainingBounds.Left - (int)(2 * dpiScaleX), remainingBounds.Height);
				}

				TextRenderer.DrawText(e.Graphics, e.SubItem.Text, currentFont, remainingBounds, textColor, TextFormatFlags.EndEllipsis | TextFormatFlags.SingleLine | TextFormatFlags.VerticalCenter | TextFormatFlags.NoPrefix);
			}
			else if (e.ColumnIndex == TypeColumn.Index)
			{
				ChangeLayoutInfo layout = GetChangeLayoutInfo(change);
				if (layout._typeBadges.Count > 0)
				{
					e.Graphics.IntersectClip(e.Bounds);
					e.Graphics.SmoothingMode = SmoothingMode.HighQuality;
					Point typeLocation = GetBadgeListLocation(layout._typeBadges, e.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
					DrawBadges(e.Graphics, typeLocation, layout._typeBadges, badgeAlpha);
				}
			}
			else if (e.ColumnIndex == CISColumn.Index)
			{
				ChangeLayoutInfo layout = GetChangeLayoutInfo(change);
				if (layout._buildBadges.Count > 0)
				{
					e.Graphics.IntersectClip(e.Bounds);
					e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					Point buildsLocation = GetBadgeListLocation(layout._buildBadges, e.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
					buildsLocation.X = Math.Max(buildsLocation.X, e.Bounds.Left);

					DrawBadges(e.Graphics, buildsLocation, layout._buildBadges, badgeAlpha);
				}
			}
			else if (e.ColumnIndex == StatusColumn.Index)
			{
				float maxX = e.SubItem.Bounds.Right;

				if (change.Number == _workspace.PendingChangeNumber && _workspace.IsBusy())
				{
					Tuple<string, float> progress = _workspace.CurrentProgress;

					maxX -= CancelIcon.Width * dpiScaleX;
					e.Graphics.DrawImage(Properties.Resources.Icons, maxX, iconY, CancelIcon, GraphicsUnit.Pixel);

					if (!Splitter.IsLogVisible())
					{
						maxX -= InfoIcon.Width * dpiScaleX;
						e.Graphics.DrawImage(Properties.Resources.Icons, maxX, iconY, InfoIcon, GraphicsUnit.Pixel);
					}

					float minX = e.Bounds.Left;

					TextRenderer.DrawText(e.Graphics, progress.Item1, currentFont, new Rectangle((int)minX, e.Bounds.Top, (int)(maxX - minX), e.Bounds.Height), textColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
				}
				else
				{
					e.Graphics.IntersectClip(e.Bounds);
					e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

					if (change.Number == _workspace.CurrentChangeNumber)
					{
						EventData? review = _eventMonitor.GetReviewByCurrentUser(change.Number);

						maxX -= FrownIcon.Width * dpiScaleX;
						e.Graphics.DrawImage(Properties.Resources.Icons, maxX, iconY, (review == null || !EventMonitor.IsPositiveReview(review.Type)) ? FrownIcon : DisabledFrownIcon, GraphicsUnit.Pixel);

						maxX -= HappyIcon.Width * dpiScaleX;
						e.Graphics.DrawImage(Properties.Resources.Icons, maxX, iconY, (review == null || !EventMonitor.IsNegativeReview(review.Type)) ? HappyIcon : DisabledHappyIcon, GraphicsUnit.Pixel);
					}
					else if (e.ItemIndex == BuildList.HoverItem && allowSync)
					{
						Rectangle syncBadgeRectangle = GetSyncBadgeRectangle(e.SubItem.Bounds);
						DrawBadge(e.Graphics, syncBadgeRectangle, "Sync", _hoverSync ? Color.FromArgb(140, 180, 230) : Color.FromArgb(112, 146, 190), true, true);
						maxX = syncBadgeRectangle.Left - (int)(2 * dpiScaleX);
					}

					string? summaryText;
					if (_workspace.State.LastSyncChangeNumber == -1 || _workspace.State.LastSyncChangeNumber != change.Number || !GetLastUpdateMessage(_workspace.State.LastSyncResult, _workspace.State.LastSyncResultMessage, out summaryText))
					{
						StringBuilder summaryTextBuilder = new StringBuilder();

						EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);

						AppendItemList(summaryTextBuilder, " ", "Under investigation by {0}.", _eventMonitor.GetInvestigatingUsers(change.Number).Select(x => FormatUserName(x)));

						if (summary != null)
						{
							string comments = String.Join(", ", summary.Comments.Where(x => !String.IsNullOrWhiteSpace(x.Text)).Select(x => String.Format("{0}: \"{1}\"", FormatUserName(x.UserName), x.Text)));
							if (comments.Length > 0)
							{
								summaryTextBuilder.Append(((summaryTextBuilder.Length == 0) ? "" : " ") + comments);
							}
							else
							{
								AppendItemList(summaryTextBuilder, " ", "Used by {0}.", summary.CurrentUsers.Select(x => FormatUserName(x)));
							}
						}

						summaryText = (summaryTextBuilder.Length == 0) ? "No current users." : summaryTextBuilder.ToString();
					}

					if (summaryText != null && summaryText.Contains('\n', StringComparison.Ordinal))
					{
						summaryText = summaryText.Substring(0, summaryText.IndexOf('\n', StringComparison.Ordinal)).TrimEnd() + "...";
					}

					TextRenderer.DrawText(e.Graphics, summaryText, currentFont, new Rectangle(e.Bounds.Left, e.Bounds.Top, (int)maxX - e.Bounds.Left, e.Bounds.Height), textColor, TextFormatFlags.Left | TextFormatFlags.VerticalCenter | TextFormatFlags.EndEllipsis | TextFormatFlags.NoPrefix);
				}
			}
			else
			{
				ColumnHeader column = BuildList.Columns[e.ColumnIndex];
				if (_customColumns.Contains(column))
				{
					ChangeLayoutInfo layout = GetChangeLayoutInfo(change);

					List<BadgeInfo>? badges;
					if (layout._customBadges.TryGetValue(column.Text, out badges) && badges.Count > 0)
					{
						e.Graphics.IntersectClip(e.Bounds);
						e.Graphics.SmoothingMode = SmoothingMode.HighQuality;

						Point buildsLocation = GetBadgeListLocation(badges, e.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
						DrawBadges(e.Graphics, buildsLocation, badges, badgeAlpha);
					}
				}
			}
		}

		private void LayoutBadges(List<BadgeInfo> badges)
		{
			int offset = 0;
			for (int idx = 0; idx < badges.Count; idx++)
			{
				BadgeInfo badge = badges[idx];

				if (idx > 0 && badge._group != badges[idx - 1]._group)
				{
					offset += 6;
				}
				badge._offset = offset;

				Size badgeSize = GetBadgeSize(badge._label);
				badge._width = badgeSize.Width;
				badge._height = badgeSize.Height;

				offset += badgeSize.Width + 1;
			}
		}

		private static Point GetBadgeListLocation(List<BadgeInfo> badges, Rectangle bounds, HorizontalAlign horzAlign, VerticalAlignment vertAlign)
		{
			if (badges.Count == 0)
			{
				return bounds.Location;
			}

			BadgeInfo lastBadge = badges[^1];

			int x = bounds.X;
			switch (horzAlign)
			{
				case HorizontalAlign.Center:
					x += (bounds.Width - lastBadge._width - lastBadge._offset) / 2;
					break;
				case HorizontalAlign.Right:
					x += (bounds.Width - lastBadge._width - lastBadge._offset);
					break;
			}

			int y = bounds.Y;
			switch (vertAlign)
			{
				case VerticalAlignment.Middle:
					y += (bounds.Height - lastBadge._height) / 2;
					break;
				case VerticalAlignment.Bottom:
					y += (bounds.Height - lastBadge._height);
					break;
			}

			return new Point(x, y);
		}

		private void DrawBadges(Graphics graphics, Point listLocation, List<BadgeInfo> badges, int alpha)
		{
			for (int idx = 0; idx < badges.Count; idx++)
			{
				BadgeInfo badge = badges[idx];
				bool mergeLeft = (idx > 0 && badges[idx - 1]._group == badge._group);
				bool mergeRight = (idx < badges.Count - 1 && badges[idx + 1]._group == badge._group);

				Rectangle bounds = new Rectangle(listLocation.X + badge._offset, listLocation.Y, badge._width, badge._height);
				if (badge._uniqueId != null && badge._uniqueId == _hoverBadgeUniqueId)
				{
					DrawBadge(graphics, bounds, badge._label, Color.FromArgb((alpha * badge._hoverBackgroundColor.A) / 255, badge._hoverBackgroundColor), mergeLeft, mergeRight);
				}
				else
				{
					DrawBadge(graphics, bounds, badge._label, Color.FromArgb((alpha * badge._backgroundColor.A) / 255, badge._backgroundColor), mergeLeft, mergeRight);
				}
			}
		}

		private List<BadgeInfo> CreateDescriptionBadges(ChangesRecord change)
		{
			string description = change.Description ?? String.Empty;

			PerforceChangeDetails? details;
			if (_perforceMonitor.TryGetChangeDetails(change.Number, out details))
			{
				description = details.Description;
			}

			List<BadgeInfo> badges = new List<BadgeInfo>();

			try
			{
				ConfigFile projectConfigFile = _perforceMonitor.LatestProjectConfigFile;
				if (projectConfigFile != null)
				{
					string[] badgeDefinitions = projectConfigFile.GetValues("Badges.DescriptionBadges", Array.Empty<string>());
					foreach (string badgeDefinition in badgeDefinitions.Distinct())
					{
						ConfigObject badgeDefinitionObject = new ConfigObject(badgeDefinition);
						string? pattern = badgeDefinitionObject.GetValue("Pattern", null);
						string? name = badgeDefinitionObject.GetValue("Name", null);
						string? group = badgeDefinitionObject.GetValue("Group", null);
						string color = badgeDefinitionObject.GetValue("Color", "#909090");
						string hoverColor = badgeDefinitionObject.GetValue("HoverColor", "#b0b0b0");
						string? url = badgeDefinitionObject.GetValue("Url", null);
						string? arguments = badgeDefinitionObject.GetValue("Arguments", null);
						bool singleMatch = badgeDefinitionObject.GetValue("SingleMatch", false);
						if (!String.IsNullOrEmpty(name) && !String.IsNullOrEmpty(pattern))
						{
							foreach (Match? matchResult in Regex.Matches(description, pattern, RegexOptions.Multiline))
							{
								if (matchResult != null)
								{
									Color badgeColor = System.Drawing.ColorTranslator.FromHtml(color);
									Color hoverBadgeColor = System.Drawing.ColorTranslator.FromHtml(hoverColor);

									string? uniqueId = String.IsNullOrEmpty(url) ? null : String.Format("Description:{0}:{1}", change.Number, badges.Count);

									string? expandedUrl = ReplaceRegexMatches(url, matchResult);
									string? expandedArguments = ReplaceRegexMatches(arguments, matchResult);

									Action? clickHandler;
									if (String.IsNullOrEmpty(expandedUrl))
									{
										clickHandler = null;
									}
									else if (String.IsNullOrEmpty(expandedArguments))
									{
										clickHandler = () => SafeProcessStart(expandedUrl);
									}
									else
									{
										clickHandler = () => SafeProcessStart(expandedUrl, expandedArguments);
									}

									badges.Add(new BadgeInfo(ReplaceRegexMatches(name, matchResult), group, uniqueId, badgeColor, hoverBadgeColor, clickHandler, null));

									if (singleMatch)
									{
										break;
									}
								}
							}
						}
					}
				}
			}
			catch (Exception)
			{
			}

			LayoutBadges(badges);

			return badges;
		}

		private void PrintLatestChangeTypeUsage(string erroringDefinition)
		{
			string errorMessage = "Error: Name not set for config value in \"Sync.LatestChangeType\"";
			string usageMessage = "Expected Format under the [Sync] ini category: +LatestChangeType=(Name=[string], Description=[string], OrderIndex=[int], bGood=[bool], bStarred=[bool], RequiredBadges=\"List of Badges For Changelist to have\")";

			ShowErrorDialog("{0} Erroring String({1}) \n {2}", errorMessage, erroringDefinition, usageMessage);
		}

		/// <summary>
		/// Array of config specified options for what the Latest Change to Sync should be.
		/// See PrintLatestChangeTypeUsage for usage information
		/// </summary>
		public List<LatestChangeType> GetCustomLatestChangeTypes()
		{
			List<LatestChangeType> foundLatestChangeTypes = new List<LatestChangeType>();

			// Add three default change types that can be synced to
			foundLatestChangeTypes.Add(LatestChangeType.LatestChange());
			foundLatestChangeTypes.Add(LatestChangeType.LatestGoodChange());
			foundLatestChangeTypes.Add(LatestChangeType.LatestStarredChange());

			ConfigFile configFile = _perforceMonitor.LatestProjectConfigFile;

			List<string> latestChangeTypeDefinitions = new List<string>();
			latestChangeTypeDefinitions.AddRange(configFile.GetValues("Sync.LatestChangeType", Array.Empty<string>()));
			ConfigUtils.GetProjectSettings(configFile, SelectedProjectIdentifier, "LatestChangeType", latestChangeTypeDefinitions);

			foreach (string latestChangeDefinition in latestChangeTypeDefinitions.Distinct())
			{
				LatestChangeType? newType;
				if (LatestChangeType.TryParseConfigEntry(latestChangeDefinition, out newType))
				{
					foundLatestChangeTypes.Add(newType);
				}
				else
				{
					PrintLatestChangeTypeUsage(latestChangeDefinition);
				}
			}

			foundLatestChangeTypes = foundLatestChangeTypes.OrderBy(x => x.OrderIndex).ToList();

			return foundLatestChangeTypes;
		}

		private static void SafeProcessStart(ProcessStartInfo startInfo)
		{
			try
			{
				Process.Start(startInfo);
			}
			catch
			{
				MessageBox.Show(String.Format("Unable to open {0}", startInfo.FileName));
			}
		}

		private static void SafeProcessStart(string url)
		{
			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo();
				startInfo.FileName = url;
				startInfo.UseShellExecute = true;
				using Process? _ = Process.Start(startInfo);
			}
			catch
			{
				MessageBox.Show(String.Format("Unable to open {0}", url));
			}
		}

		private static void SafeProcessStart(string url, string arguments)
		{
			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo();
				startInfo.FileName = url;
				startInfo.Arguments = arguments;
				startInfo.UseShellExecute = true;
				using Process? _ = Process.Start(startInfo);
			}
			catch
			{
				MessageBox.Show(String.Format("Unable to open {0} {1}", url, arguments));
			}
		}

		private static void SafeProcessStart(string url, string arguments, string workingDir)
		{
			try
			{
				ProcessStartInfo startInfo = new ProcessStartInfo();
				startInfo.FileName = url;
				startInfo.Arguments = arguments;
				startInfo.WorkingDirectory = workingDir;
				startInfo.UseShellExecute = true;
				using Process? _ = Process.Start(startInfo);
			}
			catch
			{
				MessageBox.Show(String.Format("Unable to open {0} {1}", url, arguments));
			}
		}

		public void UpdateSettings()
		{
			UpdateBuildSteps();
			UpdateStatusPanel();
		}

		[return: NotNullIfNotNull("text")]
		private static string? ReplaceRegexMatches(string? text, Match matchResult)
		{
			if (text != null)
			{
				for (int idx = 1; idx < matchResult.Groups.Count; idx++)
				{
					string sourceText = String.Format("${0}", idx);
					text = text.Replace(sourceText, matchResult.Groups[idx].Value, StringComparison.Ordinal);
				}
			}
			return text;
		}

		private List<BadgeInfo> CreateTypeBadges(int changeNumber)
		{
			List<BadgeInfo> badges = new List<BadgeInfo>();

			PerforceChangeDetails? details;
			if (_perforceMonitor.TryGetChangeDetails(changeNumber, out details))
			{
				if (details.ContainsCode)
				{
					badges.Add(new BadgeInfo("Code", "ChangeType", Color.FromArgb(116, 185, 255)));
				}
				if (details.ContainsContent)
				{
					badges.Add(new BadgeInfo("Content", "ChangeType", Color.FromArgb(162, 155, 255)));
				}
			}
			if (badges.Count == 0)
			{
				badges.Add(new BadgeInfo("Unknown", "ChangeType", Color.FromArgb(192, 192, 192)));
			}
			LayoutBadges(badges);

			return badges;
		}

		private bool TryGetProjectSetting(ConfigFile projectConfigFile, string name, [NotNullWhen(true)] out string? value)
		{
			return ConfigUtils.TryGetProjectSetting(projectConfigFile, SelectedProjectIdentifier, name, out value);
		}

		private bool TryGetProjectSetting(ConfigFile projectConfigFile, string name, [NotNullWhen(true)] out string[]? values)
		{
			string? valueList;
			if (TryGetProjectSetting(projectConfigFile, name, out valueList))
			{
				values = valueList.Split('\n').Select(x => x.Trim()).Where(x => x.Length > 0).ToArray();
				return true;
			}
			else
			{
				values = null;
				return false;
			}
		}

		private List<BadgeInfo> CreateBuildBadges(int changeNumber, EventSummary? summary)
		{
			List<BadgeInfo> badges = new List<BadgeInfo>();

			if (summary != null && summary.Badges.Count > 0)
			{
				// Create a lookup for build data for each badge name
				Dictionary<string, BadgeData> badgeNameToBuildData = new Dictionary<string, BadgeData>();
				foreach (BadgeData badge in summary.Badges)
				{
					badgeNameToBuildData[badge.BadgeName] = badge;
				}

				// Add all the badges, sorted by group
				foreach (KeyValuePair<string, string> badgeNameAndGroup in _badgeNameAndGroupPairs)
				{
					BadgeData? badgeData;
					badgeNameToBuildData.TryGetValue(badgeNameAndGroup.Key, out badgeData);

					BadgeInfo badgeInfo = CreateBadge(changeNumber, badgeNameAndGroup.Key, badgeNameAndGroup.Value, badgeData, _maxBuildBadgeChars);
					badges.Add(badgeInfo);
				}
			}

			// Layout the badges
			LayoutBadges(badges);
			return badges;
		}

		private BadgeInfo CreateBadge(int changeNumber, string badgeName, string badgeGroup, BadgeData? badgeData, int maxBadgeChars = -1)
		{
			string badgeLabel = badgeName;
			Color badgeColor = Color.FromArgb(0, Color.White);

			if (badgeData != null)
			{
				badgeLabel = badgeData.BadgeLabel;
				badgeColor = GetBuildBadgeColor(badgeData.Result);
			}

			Color hoverBadgeColor = Color.FromArgb(badgeColor.A, Math.Min(badgeColor.R + 32, 255), Math.Min(badgeColor.G + 32, 255), Math.Min(badgeColor.B + 32, 255));

			Action? leftClickHandler;
			if (badgeData == null || String.IsNullOrEmpty(badgeData.Url))
			{
				leftClickHandler = null;
			}
			else
			{
				leftClickHandler = () => SafeProcessStart(badgeData.Url);
			}

			Action<Control, Point>? rightClickHandler;
			if (badgeData?.Metadata?.Links == null || !badgeData.Metadata.Links.Any())
			{
				rightClickHandler = null;
			}
			else
			{
				rightClickHandler = (control, point) =>
				{
					BadgeContextMenu.Items.Clear();

					foreach (Link additionalUrl in badgeData.Metadata.Links)
					{
						ToolStripMenuItem item = new ToolStripMenuItem();
						item.Text = additionalUrl.Text;
						item.Click += (sender, e) => SafeProcessStart(additionalUrl.Url);
						BadgeContextMenu.Items.Add(item);
					}

					BadgeContextMenu.Show(control, point);
				};
			}

			string uniqueId = String.Format("{0}:{1}", changeNumber, badgeName);
			BadgeInfo badgeInfo = new BadgeInfo(badgeLabel, badgeGroup, uniqueId, badgeColor, hoverBadgeColor, leftClickHandler, rightClickHandler);

			if (maxBadgeChars != -1 && badgeInfo._label.Length > maxBadgeChars)
			{
				badgeInfo._toolTip = badgeInfo._label;
				badgeInfo._label = badgeInfo._label.Substring(0, maxBadgeChars);
			}

			if (badgeInfo._rightClickHandler != null)
			{
				string rightClickIndictor = "\U0001F4DC";
				if (badgeInfo._toolTip == null)
				{
					badgeInfo._toolTip = rightClickIndictor;
				}
				else
				{
					badgeInfo._toolTip += (" " + rightClickIndictor);
				}
			}

			return badgeInfo;
		}

		private Dictionary<string, List<BadgeInfo>> CreateCustomBadges(int changeNumber, EventSummary? summary)
		{
			Dictionary<string, List<BadgeInfo>> columnNameToBadges = new Dictionary<string, List<BadgeInfo>>();
			if (summary != null && summary.Badges.Count > 0)
			{
				foreach (ColumnHeader customColumn in _customColumns)
				{
					ConfigObject config = (ConfigObject)customColumn.Tag;
					if (config != null)
					{
						List<BadgeInfo> badges = new List<BadgeInfo>();

						string[] badgeNames = config.GetValue("Badges", "").Split(new char[] { ',' }, StringSplitOptions.RemoveEmptyEntries);
						foreach (string badgeName in badgeNames)
						{
							BadgeInfo badge = CreateBadge(changeNumber, badgeName, "XXXX", summary.Badges.FirstOrDefault(x => x.BadgeName == badgeName));
							badges.Add(badge);
						}

						LayoutBadges(badges);

						columnNameToBadges[customColumn.Text] = badges;
					}
				}
			}
			return columnNameToBadges;
		}

		private static Color GetBuildBadgeColor(BadgeResult result)
		{
			if (result == BadgeResult.Starting)
			{
				return Color.FromArgb(128, 192, 255);
			}
			else if (result == BadgeResult.Warning)
			{
				return Color.FromArgb(255, 192, 0);
			}
			else if (result == BadgeResult.Failure)
			{
				return Color.FromArgb(192, 64, 0);
			}
			else if (result == BadgeResult.Skipped)
			{
				return Color.FromArgb(192, 192, 192);
			}
			else
			{
				return Color.FromArgb(128, 192, 64);
			}
		}

		private Rectangle GetSyncBadgeRectangle(Rectangle bounds)
		{
			Size badgeSize = GetBadgeSize("Sync");
			return new Rectangle(bounds.Right - badgeSize.Width - 2, bounds.Top + (bounds.Height - badgeSize.Height) / 2, badgeSize.Width, badgeSize.Height);
		}

		private void DrawBadge(Graphics graphics, Rectangle badgeRect, string badgeText, Color badgeColor, bool mergeLeft, bool mergeRight)
		{
			if (badgeColor.A != 0)
			{
				using (GraphicsPath path = new GraphicsPath())
				{
					path.StartFigure();
					path.AddLine(badgeRect.Left + (mergeLeft ? 1 : 0), badgeRect.Top, badgeRect.Left - (mergeLeft ? 1 : 0), badgeRect.Bottom);
					path.AddLine(badgeRect.Left - (mergeLeft ? 1 : 0), badgeRect.Bottom, badgeRect.Right - 1 - (mergeRight ? 1 : 0), badgeRect.Bottom);
					path.AddLine(badgeRect.Right - 1 - (mergeRight ? 1 : 0), badgeRect.Bottom, badgeRect.Right - 1 + (mergeRight ? 1 : 0), badgeRect.Top);
					path.AddLine(badgeRect.Right - 1 + (mergeRight ? 1 : 0), badgeRect.Top, badgeRect.Left + (mergeLeft ? 1 : 0), badgeRect.Top);
					path.CloseFigure();

					using (SolidBrush brush = new SolidBrush(badgeColor))
					{
						graphics.FillPath(brush, path);
					}
				}

				TextRenderer.DrawText(graphics, badgeText, _badgeFont, badgeRect, Color.White, TextFormatFlags.HorizontalCenter | TextFormatFlags.VerticalCenter | TextFormatFlags.SingleLine | TextFormatFlags.NoPrefix | TextFormatFlags.PreserveGraphicsClipping);
			}
		}

		private Size GetBadgeSize(string badgeText)
		{
			Size badgeSize;
			if (!_badgeLabelToSize.TryGetValue(badgeText, out badgeSize))
			{
				Size labelSize = TextRenderer.MeasureText(badgeText, _badgeFont);
				int badgeHeight = _badgeFont!.Height + 1;

				badgeSize = new Size(labelSize.Width + badgeHeight - 4, badgeHeight);
				_badgeLabelToSize[badgeText] = badgeSize;
			}
			return badgeSize;
		}

		private static bool GetLastUpdateMessage(WorkspaceUpdateResult result, string? resultMessage, [NotNullWhen(true)] out string? message)
		{
			if (result != WorkspaceUpdateResult.Success && resultMessage != null)
			{
				message = resultMessage;
				return true;
			}
			return GetGenericLastUpdateMessage(result, out message);
		}

		private static bool GetGenericLastUpdateMessage(WorkspaceUpdateResult result, [NotNullWhen(true)] out string? message)
		{
			switch (result)
			{
				case WorkspaceUpdateResult.Canceled:
					message = "Sync canceled.";
					return true;
				case WorkspaceUpdateResult.FailedToSync:
					message = "Failed to sync files.";
					return true;
				case WorkspaceUpdateResult.FailedToSyncLoginExpired:
					message = "Failed to sync files (login expired).";
					return true;
				case WorkspaceUpdateResult.FilesToResolve:
					message = "Sync finished with unresolved files.";
					return true;
				case WorkspaceUpdateResult.FilesToClobber:
					message = "Sync failed due to modified files in workspace.";
					return true;
				case WorkspaceUpdateResult.FilesToDelete:
					message = "Sync aborted pending confirmation of files to delete.";
					return true;
				case WorkspaceUpdateResult.FailedToCompile:
				case WorkspaceUpdateResult.FailedToCompileWithCleanWorkspace:
					message = "Compile failed.";
					return true;
				default:
					message = null;
					return false;
			}
		}

		public static string FormatUserName(string userName)
		{
			if (userName == null)
			{
				return "(Invalid Name)";
			}
			else
			{
				return Utility.FormatUserName(userName);
			}
		}

		private static void AppendUserList(StringBuilder builder, string separator, string format, IEnumerable<EventData> reviews)
		{
			AppendItemList(builder, separator, format, reviews.Select(x => FormatUserName(x.UserName)));
		}

		private static void AppendItemList(StringBuilder builder, string separator, string format, IEnumerable<string> items)
		{
			string? itemList = FormatItemList(format, items);
			if (itemList != null)
			{
				if (builder.Length > 0)
				{
					builder.Append(separator);
				}
				builder.Append(itemList);
			}
		}

		private static string? FormatItemList(string format, IEnumerable<string> items)
		{
			string[] itemsArray = items.Distinct().ToArray();
			if (itemsArray.Length == 0)
			{
				return null;
			}

			StringBuilder builder = new StringBuilder(itemsArray[0]);
			if (itemsArray.Length > 1)
			{
				for (int idx = 1; idx < itemsArray.Length - 1; idx++)
				{
					builder.Append(", ");
					builder.Append(itemsArray[idx]);
				}
				builder.Append(" and ");
				builder.Append(itemsArray.Last());
			}
			return String.Format(format, builder.ToString());
		}

		private void BuildList_MouseDoubleClick(object sender, MouseEventArgs args)
		{
			if (args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo hitTest = BuildList.HitTest(args.Location);
				if (hitTest.Item != null)
				{
					ChangesRecord change = (ChangesRecord)hitTest.Item.Tag;
					if (change != null)
					{
						if (change.Number == _workspace.CurrentChangeNumber)
						{
							LaunchEditor();
						}
						else
						{
							StartSync(change.Number, false, null);
						}
					}
				}
			}
		}

		public void LaunchEditor()
		{
			if (!_workspace.IsBusy() && _workspace.CurrentChangeNumber != -1)
			{
				BuildConfig editorBuildConfig = GetEditorBuildConfig();

				FileReference receiptFile = ConfigUtils.GetEditorReceiptFile(_workspace.Project, _workspace.ProjectConfigFile, editorBuildConfig);

				if (!FileReference.Exists(receiptFile))
				{
					ShowEditorLaunchError($"{receiptFile} not found");
				}
				else if (!ConfigUtils.TryReadEditorReceipt(_workspace.Project, receiptFile, out TargetReceipt? receipt))
				{
					ShowEditorLaunchError($"Unable to read {receiptFile}");
				}
				else if (receipt.Launch == null)
				{
					ShowEditorLaunchError($"No launch target in {receiptFile}");
				}
				else if (!File.Exists(receipt.Launch))
				{
					ShowEditorLaunchError($"{receipt.Launch} not found");
				}
				else
				{
					if (_settings.EditorArgumentsPrompt && !ModifyEditorArguments())
					{
						return;
					}

					StringBuilder launchArguments = new StringBuilder();
					if (SelectedFileName.HasExtension(".uproject"))
					{
						launchArguments.AppendFormat("\"{0}\"", SelectedFileName);
					}
					foreach (Tuple<string, bool> editorArgument in _settings.EditorArguments)
					{
						if (editorArgument.Item2)
						{
							launchArguments.AppendFormat(" {0}", editorArgument.Item1);
						}
					}
					if (editorBuildConfig == BuildConfig.Debug || editorBuildConfig == BuildConfig.DebugGame)
					{
						launchArguments.Append(" -debug");
					}

					if (!Utility.SpawnProcess(receipt.Launch, launchArguments.ToString()))
					{
						ShowErrorDialog("Unable to spawn {0} {1}", receipt.Launch, launchArguments.ToString());
					}
				}
			}
		}

		private void ShowEditorLaunchError(string message)
		{
			if (MessageBox.Show($"The editor needs to be built before you can run it. Build it now?\r\n\r\n({message})", "Editor out of date", MessageBoxButtons.YesNo) == System.Windows.Forms.DialogResult.Yes)
			{
				_owner.ShowAndActivate();

				WorkspaceUpdateOptions options = WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.RunAfterSync;
				WorkspaceUpdateContext context = new WorkspaceUpdateContext(_workspace.CurrentChangeNumber, options, _settings.CompiledEditorBuildConfig, null, _projectSettings.BuildSteps, null);
				StartWorkspaceUpdate(context, null);
			}
		}

		private bool WaitForProgramsToFinish()
		{
			FileReference[] processFileNames = GetProcessesRunningInWorkspace();
			if (processFileNames.Length > 0)
			{
				using ProgramsRunningWindow programsRunning = new ProgramsRunningWindow(GetProcessesRunningInWorkspace, processFileNames);
				if (programsRunning.ShowDialog() != DialogResult.OK)
				{
					return false;
				}
			}
			return true;
		}

		private FileReference[] GetProcessesRunningInWorkspace()
		{
			object? skipRunnningProcessCheck = Registry.GetValue("HKEY_CURRENT_USER\\SOFTWARE\\Epic Games\\UnrealGameSync", "SkipRunningProcessCheck", null);
			if (skipRunnningProcessCheck != null && skipRunnningProcessCheck is int value && value != 0)
			{
				return Array.Empty<FileReference>();
			}

			HashSet<string> processNames = new HashSet<string>(StringComparer.InvariantCultureIgnoreCase);
			processNames.Add("Win64\\UE4Editor.exe");
			processNames.Add("Win64\\UE4Editor-Cmd.exe");
			processNames.Add("Win64\\UE4Editor-Win64-Debug.exe");
			processNames.Add("Win64\\UE4Editor-Win64-Debug-Cmd.exe");
			processNames.Add("Win64\\UE4Editor-Win64-DebugGame.exe");
			processNames.Add("Win64\\UE4Editor-Win64-DebugGame-Cmd.exe");
			processNames.Add("Win64\\UnrealEditor.exe");
			processNames.Add("Win64\\UnrealEditor-Cmd.exe");
			processNames.Add("Win64\\UnrealEditor-Win64-Debug.exe");
			processNames.Add("Win64\\UnrealEditor-Win64-Debug-Cmd.exe");
			processNames.Add("Win64\\UnrealEditor-Win64-DebugGame.exe");
			processNames.Add("Win64\\UnrealEditor-Win64-DebugGame-Cmd.exe");
			processNames.Add("Win64\\CrashReportClient.exe");
			processNames.Add("Win64\\CrashReportClient-Win64-Development.exe");
			processNames.Add("Win64\\CrashReportClient-Win64-Debug.exe");
			processNames.Add("Win64\\CrashReportClientEditor.exe");
			processNames.Add("Win64\\CrashReportClientEditor-Win64-Development.exe");
			processNames.Add("Win64\\CrashReportClientEditor-Win64-Debug.exe");
			processNames.Add("DotNET\\UnrealBuildTool\\UnrealBuildTool.exe");
			processNames.Add("DotNET\\AutomationTool\\AutomationTool.exe");

			List<FileReference> processFileNames = new List<FileReference>();
			try
			{
				DirectoryReference binariesRootPath = DirectoryReference.Combine(_workspace.Project.LocalRootPath, "Engine\\Binaries");

				foreach (string processName in processNames)
				{
					try
					{
						FileReference processFilename = FileReference.Combine(binariesRootPath, processName);

						if (FileReference.Exists(processFilename))
						{
							try
							{
								// Try to open the file to determine whether the executable is running.
								// We do this so that we can also find processes running under other user sessions, without needing PROCESS_QUERY_INFORMATION access rights.
								using FileStream testStream = File.OpenWrite(processFilename.FullName);
							}
							catch (IOException)
							{
								// This file is in use, so add it to the list of running processes
								processFileNames.Add(processFilename);
							}
						}
					}
					catch
					{
					}
				}
			}
			catch
			{
			}
			return processFileNames.ToArray();
		}

		private void TimerCallback(object? sender, EventArgs args)
		{
			Tuple<string, float> progress = _workspace.CurrentProgress;
			if (progress != null && progress.Item2 > 0.0f)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.Normal, progress.Item2);
			}
			else
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
			}

			_owner.UpdateProgress();

			UpdateStatusPanel();
			BuildList.Refresh();
		}

		private string? GetDefaultIssueFilter()
		{
			string? buildHealthProject;
			if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "BuildHealthProject", out buildHealthProject))
			{
				return buildHealthProject;
			}

			string? defaultIssueFilter;
			if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "DefaultIssueFilter", out defaultIssueFilter))
			{
				return defaultIssueFilter;
			}

			return null;
		}

		private Dictionary<string, Func<IssueData, bool>> GetCustomIssueFilters()
		{
			Dictionary<string, Func<IssueData, bool>> customFilters = new Dictionary<string, Func<IssueData, bool>>();

			string? buildHealthProject;
			if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "BuildHealthProject", out buildHealthProject))
			{
				customFilters[buildHealthProject] = x => !String.IsNullOrEmpty(x.Project) && x.Project.Equals(buildHealthProject, StringComparison.OrdinalIgnoreCase);
			}

			string[]? filters;
			if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "IssueFilters", out filters))
			{
				foreach (string filter in filters)
				{
					try
					{
						ConfigObject config = new ConfigObject(filter);

						string? name = config.GetValue("Name", null);
						string? pattern = config.GetValue("Pattern", null);

						if (name != null && pattern != null && StreamName != null)
						{
							string? depotName;
							if (PerforceUtils.TryGetDepotName(StreamName, out depotName))
							{
								pattern = Regex.Replace(pattern, @"\$\(Depot\)", depotName, RegexOptions.IgnoreCase);
							}
							pattern = Regex.Replace(pattern, @"\$\(Stream\)", StreamName, RegexOptions.IgnoreCase);

							Regex patternRegex = new Regex(String.Format("^(?:{0})$", pattern), RegexOptions.IgnoreCase);
							customFilters[name] = x => x.Streams.Any(y => patternRegex.IsMatch(y));
						}
					}
					catch (Exception ex)
					{
						_logger.LogError(ex, "Unable to parse config filter '{Filter}'", filter);
					}
				}
			}

			return customFilters;
		}

		private void OpenPerforce()
		{
			StringBuilder commandLine = new StringBuilder();
			if (_workspace != null)
			{
				commandLine.Append(_perforceSettings.GetArgumentsForExternalProgram(true));
			}
			SafeProcessStart("p4v.exe", commandLine.ToString());
		}

		void RunTool(ToolInfo tool, ToolLink link)
		{
			DirectoryReference? toolDir = _owner.ToolUpdateMonitor.GetToolPath(tool.Name);
			if (toolDir != null)
			{
				Dictionary<string, string> variables = _workspace.GetVariables(GetEditorBuildConfig());
				variables["ToolDir"] = toolDir.FullName;

				string fileName = Utility.ExpandVariables(link.FileName, variables);
				string arguments = Utility.ExpandVariables(link.Arguments ?? String.Empty, variables);
				string workingDir = Utility.ExpandVariables(link.WorkingDir ?? toolDir.FullName, variables);

				SafeProcessStart(fileName, arguments, workingDir);
			}
		}

		private void UpdateStatusPanel_CrossThread()
		{
			_mainThreadSynchronizationContext.Post((o) =>
			{
				if (!IsDisposed && !_isDisposing)
				{
					UpdateBuildSteps();
					UpdateStatusPanel();
				}
			}, null);
		}

		private void AddHordeStatus(StatusLine statusLine)
		{
			if (_hordeClient != null)
			{
				statusLine.AddText("  |  ");

				if (_hordeClient.IsConnected())
				{
					statusLine.AddLink("Connected to Horde", FontStyle.Regular, (p, r) => Utility.OpenUrl(_hordeClient.ServerUrl.ToString()));
				}
				else
				{
					statusLine.AddLink("Connect to Horde...", FontStyle.Bold | FontStyle.Underline, (p, r) => ConnectToHorde(_hordeClient));
				}
			}
		}

		private void UpdateStatusPanel()
		{
			if (_workspace == null)
			{
				return;
			}

			int newContentWidth = Math.Max(TextRenderer.MeasureText(String.Format("Opened {0}  |  Browse...  |  Connect...", SelectedFileName), StatusPanel.Font).Width, 400);
			StatusPanel.SetContentWidth(newContentWidth);

			List<StatusLine> lines = new List<StatusLine>();
			if (_workspace.IsBusy() || _workspace.IsExternalSyncActive())
			{
				// Project
				StatusLine projectLine = new StatusLine();
				projectLine.AddText("Opened " + SelectedFileName.FullName);
				AddHordeStatus(projectLine);
				lines.Add(projectLine);

				// Spacer
				lines.Add(new StatusLine() { LineHeight = 0.5f });

				// Sync in progress
				StatusLine summaryLine = new StatusLine();
				if (_workspace.IsBusy())
				{
					Tuple<string, float> progress = _workspace.CurrentProgress;

					if (_workspace.PendingChangeNumber == -1)
					{
						summaryLine.AddText("Working... | ");
					}
					else
					{
						summaryLine.AddText("Syncing to changelist ");
						summaryLine.AddLink(_workspace.PendingChangeNumber.ToString(), FontStyle.Regular, () => { SelectChange(_workspace.PendingChangeNumber); });
						summaryLine.AddText("... | ");
					}
					summaryLine.AddLink(Splitter.IsLogVisible() ? "Hide Log" : "Show Log", FontStyle.Bold | FontStyle.Underline, () => { ToggleLogVisibility(); });
					summaryLine.AddText(" | ");
					summaryLine.AddLink("Cancel", FontStyle.Bold | FontStyle.Underline, () => { CancelWorkspaceUpdate(); });
					lines.Add(summaryLine);

					StatusLine progressLine = new StatusLine();
					progressLine.AddText(String.Format("{0}  ", progress.Item1));
					if (progress.Item2 > 0.0f)
					{
						progressLine.AddProgressBar(progress.Item2);
					}
					lines.Add(progressLine);
				}
				else
				{
					summaryLine.AddText("Command-line sync in progress...");
					lines.Add(summaryLine);
				}

				// Programs
				StatusLine programsLine = new StatusLine();

				string[]? sdkInfoEntries;
				if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "SdkInfo", out sdkInfoEntries))
				{
					programsLine.AddLink("SDK Info", FontStyle.Regular, () => { ShowRequiredSdkInfo(); });
					programsLine.AddText("  |  ");
				}

				programsLine.AddLink("Perforce", FontStyle.Regular, () => { OpenPerforce(); });
				programsLine.AddText("  |  ");
				if (!ShouldSyncPrecompiledEditor)
				{
					programsLine.AddLink("Visual Studio", FontStyle.Regular, () => { OpenSolution(); });
					programsLine.AddText("  |  ");
				}

				IReadOnlyList<ToolInfo> tools = _owner.ToolUpdateMonitor.GetEnabledTools();
				foreach (ToolInfo tool in tools)
				{
					if (tool.Settings.SafeWhenBusy)
					{
						foreach (ToolLink link in tool.Settings.StatusPanelLinks)
						{
							programsLine.AddLink(link.Label, FontStyle.Regular, () => RunTool(tool, link));
							programsLine.AddText("  |  ");
						}
					}
				}

				programsLine.AddLink("Windows Explorer", FontStyle.Regular, () => { SafeProcessStart("explorer.exe", String.Format("\"{0}\"", SelectedFileName.Directory.FullName)); });

				if (GetDefaultIssueFilter() != null)
				{
					programsLine.AddText("  |  ");
					if (_userHasOpenIssues)
					{
						programsLine.AddBadge("Build Health", GetBuildBadgeColor(BadgeResult.Failure), (p, r) => ShowBuildHealthMenu(r));
					}
					else
					{
						programsLine.AddLink("Build Health", FontStyle.Regular, (p, r) => { ShowBuildHealthMenu(r); });
					}
				}

				lines.Add(programsLine);
			}
			else
			{
				// Project
				StatusLine projectLine = new StatusLine();
				projectLine.AddText(String.Format("Opened "));
				projectLine.AddLink(SelectedFileName.FullName + " \u25BE", FontStyle.Regular, (p, r) => { SelectRecentProject(r); });
				AddHordeStatus(projectLine);
				projectLine.AddText("  |  ");
				projectLine.AddLink("Settings...", FontStyle.Regular, (p, r) => { _owner.EditSelectedProject(this); });
				lines.Add(projectLine);

				// Spacer
				lines.Add(new StatusLine() { LineHeight = 0.5f });

				// Sync status
				StatusLine summaryLine = new StatusLine();
				if (IsBisectModeEnabled())
				{
					int passChangeNumber;
					int failChangeNumber;
					GetRemainingBisectRange(out passChangeNumber, out failChangeNumber);

					summaryLine.AddText("Bisecting changes between ");
					summaryLine.AddLink(String.Format("{0}", passChangeNumber), FontStyle.Regular, () => { SelectChange(passChangeNumber); });
					summaryLine.AddText(" and ");
					summaryLine.AddLink(String.Format("{0}", failChangeNumber), FontStyle.Regular, () => { SelectChange(failChangeNumber); });
					summaryLine.AddText(".  ");

					int bisectChangeNumber = GetBisectChangeNumber();
					if (bisectChangeNumber != -1)
					{
						summaryLine.AddLink("Sync Next", FontStyle.Bold | FontStyle.Underline, () => { SyncBisectChange(); });
						summaryLine.AddText(" | ");
					}

					summaryLine.AddLink("Cancel", FontStyle.Bold | FontStyle.Underline, () => { CancelBisectMode(); });
				}
				else
				{
					if (_workspace.CurrentChangeNumber != -1)
					{
						if (StreamName == null)
						{
							summaryLine.AddText("Last synced to changelist ");
						}
						else
						{
							summaryLine.AddText("Last synced to ");
							if (_workspace.ProjectStreamFilter == null || _workspace.ProjectStreamFilter.Count == 0)
							{
								summaryLine.AddLink(StreamName, FontStyle.Regular, (p, r) => { SelectOtherStreamDialog(); });
							}
							else
							{
								summaryLine.AddLink(StreamName + "\u25BE", FontStyle.Regular, (p, r) => { SelectOtherStream(r); });
							}
							summaryLine.AddText(" at changelist ");
						}
						summaryLine.AddLink(String.Format("{0}.", _workspace.CurrentChangeNumber), FontStyle.Regular, () => { SelectChange(_workspace.CurrentChangeNumber); });
						summaryLine.AddLink(" \u25BE", 0, (point, bounds) =>
						{
							// Try to find the CL record in our stored list in order to avoid requesting the info from p4:
							foreach (ListViewItem? item in BuildList.Items)
							{
								if (item != null)
								{
									ChangesRecord summary = (ChangesRecord)item.Tag;
									if (summary != null)
									{
										// No need to keep on iterating if we're past the required CL :
										if (summary.Number < _workspace.CurrentChangeNumber)
										{
											break;
										}
										else if (summary.Number == _workspace.CurrentChangeNumber)
										{
											_contextMenuChange = summary;
											ShowChangelistContextMenu(StatusPanel, showTimeRelatedItems: false, new Point(bounds.Left, bounds.Bottom), ToolStripDropDownDirection.BelowRight);
											return;
										}
									}
								}
							}

							// Couldn't find the CL in our BuildList, get the info from p4 :
							ChangesRecord? foundRecord = null;
							async Task<bool> GetChangesRecordFunc(IPerforceConnection perforce, CancellationToken cancellationToken)
							{
								List<ChangesRecord> changes = await perforce.GetChangesAsync(ChangesOptions.None, 1, ChangeStatus.Submitted, $"//...@{_workspace.CurrentChangeNumber},{_workspace.CurrentChangeNumber}", cancellationToken);

								if (changes.Count == 1)
								{
									foundRecord = changes[0];
								}
								return true;
							}

							ModalTask<bool>? getChangesTask = PerforceModalTask.Execute<bool>(this, "Getting CL info", "Please wait...", _perforceSettings, GetChangesRecordFunc, _logger);
							if ((getChangesTask != null) && getChangesTask.Succeeded && (foundRecord != null))
							{
								_contextMenuChange = foundRecord;
								ShowChangelistContextMenu(StatusPanel, showTimeRelatedItems: false, new Point(bounds.Left, bounds.Bottom), ToolStripDropDownDirection.BelowRight);
							}
						});
					}
					else
					{
						summaryLine.AddText("You are not currently synced to ");
						if (StreamName == null)
						{
							summaryLine.AddText("this branch.");
						}
						else
						{
							summaryLine.AddLink(StreamName + " \u25BE", FontStyle.Regular, (p, r) => { SelectOtherStream(r); });
						}
					}
					summaryLine.AddText("  |  ");
					summaryLine.AddLink("Sync Now", FontStyle.Bold | FontStyle.Underline, () => { SyncLatestChange(); });
					summaryLine.AddText(" - ");
					summaryLine.AddLink(" To... \u25BE", 0, (p, r) => { ShowSyncMenu(r); });
				}
				lines.Add(summaryLine);

				// Programs
				StatusLine programsLine = new StatusLine();
				if (_workspace.CurrentChangeNumber != -1)
				{
					programsLine.AddLink("Unreal Editor", FontStyle.Regular, () => { LaunchEditor(); });
					programsLine.AddText("  |  ");
				}

				string[]? sdkInfoEntries;
				if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "SdkInfo", out sdkInfoEntries))
				{
					programsLine.AddLink("SDK Info", FontStyle.Regular, () => { ShowRequiredSdkInfo(); });
					programsLine.AddText("  |  ");
				}

				programsLine.AddLink("Perforce", FontStyle.Regular, () => { OpenPerforce(); });
				programsLine.AddText("  |  ");
				if (!ShouldSyncPrecompiledEditor)
				{
					programsLine.AddLink("Visual Studio", FontStyle.Regular, () => { OpenSolution(); });
					programsLine.AddText("  |  ");
				}

				IReadOnlyList<ToolInfo> tools = _owner.ToolUpdateMonitor.GetEnabledTools();
				foreach (ToolInfo tool in tools)
				{
					foreach (ToolLink link in tool.Settings.StatusPanelLinks)
					{
						programsLine.AddLink(link.Label, FontStyle.Regular, () => RunTool(tool, link));
						programsLine.AddText("  |  ");
					}
				}

				foreach ((string name, Action<Point, Rectangle> action) in _customStatusPanelLinks)
				{
					programsLine.AddLink(name, FontStyle.Regular, action);
					programsLine.AddText("  |  ");
				}

				programsLine.AddLink("Windows Explorer", FontStyle.Regular, () => { SafeProcessStart("explorer.exe", String.Format("\"{0}\"", SelectedFileName.Directory.FullName)); });

				if (GetDefaultIssueFilter() != null)
				{
					programsLine.AddText("  |  ");
					if (_userHasOpenIssues)
					{
						programsLine.AddBadge("Build Health", GetBuildBadgeColor(BadgeResult.Failure), (p, r) => ShowBuildHealthMenu(r));
					}
					else
					{
						programsLine.AddLink("Build Health", FontStyle.Regular, (p, r) => { ShowBuildHealthMenu(r); });
					}
				}

				foreach (ServiceBadgeInfo serviceBadge in _serviceBadges)
				{
					programsLine.AddText("  |  ");
					if (serviceBadge.Url == null)
					{
						programsLine.AddBadge(serviceBadge.Name, GetBuildBadgeColor(serviceBadge.Result), null);
					}
					else
					{
						programsLine.AddBadge(serviceBadge.Name, GetBuildBadgeColor(serviceBadge.Result), (p, r) => { SafeProcessStart(serviceBadge.Url); });
					}
				}
				programsLine.AddText("  |  ");
				programsLine.AddLink("More... \u25BE", FontStyle.Regular, (p, r) => { ShowActionsMenu(r); });
				lines.Add(programsLine);

				// Get the summary of the last sync
				if (_workspace.State.LastSyncChangeNumber > 0)
				{
					string? summaryText;
					if (_workspace.State.LastSyncChangeNumber == _workspace.CurrentChangeNumber && _workspace.State.LastSyncResult == WorkspaceUpdateResult.Success && _workspace.State.LastSyncTime.HasValue)
					{
						lines.Add(new StatusLine() { LineHeight = 0.5f });

						StatusLine successLine = new StatusLine();
						successLine.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 0);
						successLine.AddText(String.Format("  Sync took {0}{1}s, completed at {2}.", (_workspace.State.LastSyncDurationSeconds >= 60) ? String.Format("{0}m ", _workspace.State.LastSyncDurationSeconds / 60) : "", _workspace.State.LastSyncDurationSeconds % 60, _workspace.State.LastSyncTime.Value.ToLocalTime().ToString("h\\:mmtt")));
						lines.Add(successLine);
					}
					else if (GetLastUpdateMessage(_workspace.State.LastSyncResult, _workspace.State.LastSyncResultMessage, out summaryText))
					{
						lines.Add(new StatusLine() { LineHeight = 0.5f });

						int summaryTextLength = summaryText.IndexOf('\n', StringComparison.Ordinal);
						if (summaryTextLength == -1)
						{
							summaryTextLength = summaryText.Length;
						}
						summaryTextLength = Math.Min(summaryTextLength, 80);

						StatusLine failLine = new StatusLine();
						failLine.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 1);

						if (summaryTextLength == summaryText.Length)
						{
							failLine.AddText(String.Format("  {0}  ", summaryText));
						}
						else
						{
							failLine.AddText(String.Format("  {0}...  ", summaryText.Substring(0, summaryTextLength).TrimEnd()));
							failLine.AddLink("More...", FontStyle.Bold | FontStyle.Underline, () => { ViewLastSyncStatus(); });
							failLine.AddText("  |  ");
						}
						failLine.AddLink("Show Log", FontStyle.Bold | FontStyle.Underline, () => { ShowErrorInLog(); });
						lines.Add(failLine);
					}
				}
			}

			StatusLine? caption = null;
			if (StreamName != null)
			{
				caption = new StatusLine();
				if (_workspace.IsBusy() || _workspace.IsExternalSyncActive())
				{
					caption.AddText(StreamName);
				}
				else
				{
					caption.AddLink(StreamName + "\u25BE", FontStyle.Bold, (p, r) => { SelectOtherStream(r); });
				}
			}

			StatusLine? alert = null;
			Color? tintColor = null;

			ConfigFile projectConfigFile = _perforceMonitor.LatestProjectConfigFile;
			if (projectConfigFile != null)
			{
				string? message;
				if (TryGetProjectSetting(projectConfigFile, "Message", out message))
				{
					alert = CreateStatusLineFromMarkdown(message);
				}

				string? statusPanelColor;
				if (TryGetProjectSetting(projectConfigFile, "StatusPanelColor", out statusPanelColor))
				{
					tintColor = System.Drawing.ColorTranslator.FromHtml(statusPanelColor);
				}
			}

			using (Graphics graphics = CreateGraphics())
			{
				int statusPanelHeight = (int)(148.0f * graphics.DpiY / 96.0f);
				if (alert != null)
				{
					statusPanelHeight += (int)(40.0f * graphics.DpiY / 96.0f);
				}
				if (StatusLayoutPanel.RowStyles[0].Height != statusPanelHeight)
				{
					StatusLayoutPanel.RowStyles[0].Height = statusPanelHeight;
				}
			}

			StatusPanel.Set(lines, caption, alert, tintColor);
		}

		private void ConnectToHorde(IHordeClient hordeClient)
		{
			ModalTask.Execute(this, "Horde Login", "Connecting to Horde...", async cancellationToken =>
			{
				try
				{
					await hordeClient.ConnectAsync(true, cancellationToken);
				}
				catch when (cancellationToken.IsCancellationRequested)
				{
					// nop
				}
			});
			UpdateStatusPanel();
		}

		private void ShowBuildHealthMenu(Rectangle bounds)
		{
			int minSeparatorIdx = BuildHealthContextMenu.Items.IndexOf(BuildHealthContextMenu_MinSeparator);

			while (BuildHealthContextMenu.Items[minSeparatorIdx + 1] != BuildHealthContextMenu_MaxSeparator)
			{
				BuildHealthContextMenu.Items.RemoveAt(minSeparatorIdx + 1);
			}

			Func<IssueData, bool>? predicate = null;
			string? defaultIssueFilter = GetDefaultIssueFilter();
			if (defaultIssueFilter == null || !GetCustomIssueFilters().TryGetValue(defaultIssueFilter, out predicate))
			{
				predicate = x => true;
			}

			List<IssueData> issues = new List<IssueData>();
			bool hasOtherAssignedIssue = false;
			foreach (IssueData issue in _issueMonitor.GetIssues().OrderByDescending(x => x.Id))
			{
				if (predicate(issue))
				{
					issues.Add(issue);
				}
				else if (issue.FixChange == 0 && issue.Owner != null && String.Equals(issue.Owner, _perforceSettings.UserName, StringComparison.OrdinalIgnoreCase))
				{
					hasOtherAssignedIssue = true;
				}
			}

			BuildHealthContextMenu_Browse.Font = new Font(BuildHealthContextMenu_Browse.Font, hasOtherAssignedIssue ? FontStyle.Bold : FontStyle.Regular);

			if (issues.Count == 0)
			{
				BuildHealthContextMenu_MaxSeparator.Visible = false;
			}
			else
			{
				BuildHealthContextMenu_MaxSeparator.Visible = true;
				for (int idx = 0; idx < issues.Count; idx++)
				{
					IssueData issue = issues[idx];

					string summary = issue.Summary;
					if (summary.Length > 100)
					{
						summary = summary.Substring(0, 100).TrimEnd() + "...";
					}

					StringBuilder description = new StringBuilder();
					description.AppendFormat("{0}: {1}", issue.Id, summary);
					if (issue.Owner == null)
					{
						description.AppendFormat(" - Unassigned");
					}
					else
					{
						description.AppendFormat(" - {0}", FormatUserName(issue.Owner));
						if (!issue.AcknowledgedAt.HasValue)
						{
							description.Append(" (?)");
						}
					}
					if (issue.FixChange > 0)
					{
						description.AppendFormat(" (Unverified fix in CL {0})", issue.FixChange);
					}

					description.AppendFormat(" ({0})", Utility.FormatDurationMinutes((int)((issue.RetrievedAt - issue.CreatedAt).TotalMinutes + 1)));

#pragma warning disable CA2000 // warning CA2000: Call System.IDisposable.Dispose on object created by 'new ToolStripMenuItem(description.ToString())' before all references to it are out of scope
					ToolStripMenuItem item = new ToolStripMenuItem(description.ToString());
					if (issue.FixChange > 0)
					{
						item.ForeColor = SystemColors.GrayText;
					}
					else if (issue.Owner != null && String.Equals(issue.Owner, _perforceSettings.UserName, StringComparison.OrdinalIgnoreCase))
					{
						item.Font = new Font(item.Font, FontStyle.Bold);
					}
					item.Click += (s, e) => { BuildHealthContextMenu_Issue_Click(issue); };
					BuildHealthContextMenu.Items.Insert(minSeparatorIdx + idx + 1, item);
#pragma warning restore CA2000
				}
			}

			BuildHealthContextMenu.Show(StatusPanel, new Point(bounds.Right, bounds.Bottom), ToolStripDropDownDirection.BelowLeft);
		}

		private void BuildHealthContextMenu_Issue_Click(IssueData issue)
		{
			ShowIssueDetails(issue);
		}

		private TimeSpan? GetServerTimeOffset()
		{
			TimeSpan? offset = null;
			if (_settings.ShowLocalTimes)
			{
				offset = _perforceMonitor.ServerTimeZone;
			}
			return offset;
		}

		public void ShowIssueDetails(IssueData issue)
		{
			IssueDetailsWindow.Show(ParentForm, _issueMonitor, _perforceSettings, GetServerTimeOffset(), issue, _serviceProvider, StreamName);
		}

		private void BuildHealthContextMenu_Browse_Click(object sender, EventArgs e)
		{
			string? defaultFilter = GetDefaultIssueFilter();
			Dictionary<string, Func<IssueData, bool>> customFilters = GetCustomIssueFilters();
			IssueBrowserWindow.Show(ParentForm, _issueMonitor, _perforceSettings, GetServerTimeOffset(), _serviceProvider, StreamName, customFilters, defaultFilter);
		}

		private void BuildHealthContextMenu_Settings_Click(object sender, EventArgs e)
		{
			string? buildHealthProject;
			TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "BuildHealthProject", out buildHealthProject);

			using IssueSettingsWindow issueSettings = new IssueSettingsWindow(_settings, buildHealthProject ?? "", _logger);
			if (issueSettings.ShowDialog(this) == DialogResult.OK)
			{
				_owner.UpdateAlertWindows();
			}
		}

		private void ShowRequiredSdkInfo()
		{
			string[]? sdkInfoEntries;
			if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "SdkInfo", out sdkInfoEntries))
			{
				Dictionary<string, string> variables = _workspace.GetVariables(GetEditorBuildConfig());
				using SdkInfoWindow window = new SdkInfoWindow(sdkInfoEntries, variables, _badgeFont!);
				window.ShowDialog();
			}
		}

		private static StatusLine CreateStatusLineFromMarkdown(string text)
		{
			StatusLine line = new StatusLine();

			FontStyle style = FontStyle.Regular;

			StringBuilder elementText = new StringBuilder();
			for (int idx = 0; idx < text.Length;)
			{
				// Bold and italic
				if (text[idx] == '_' || text[idx] == '*')
				{
					if (idx + 1 < text.Length && text[idx + 1] == text[idx])
					{
						FlushMarkdownText(line, elementText, style);
						idx += 2;
						style ^= FontStyle.Bold;
						continue;
					}
					else
					{
						FlushMarkdownText(line, elementText, style);
						idx++;
						style ^= FontStyle.Italic;
						continue;
					}
				}

				// Strikethrough
				if (idx + 2 < text.Length && text[idx] == '~' && text[idx + 1] == '~')
				{
					FlushMarkdownText(line, elementText, style);
					idx += 2;
					style ^= FontStyle.Strikeout;
					continue;
				}

				// Link
				if (text[idx] == '[')
				{
					if (idx + 1 < text.Length && text[idx + 1] == '[')
					{
						elementText.Append(text[idx]);
						idx += 2;
						continue;
					}

					int endIdx = text.IndexOf("](", idx, StringComparison.Ordinal);
					if (endIdx != -1)
					{
						int urlEndIdx = text.IndexOf(')', endIdx + 2);
						if (urlEndIdx != -1)
						{
							FlushMarkdownText(line, elementText, style);
							string linkText = text.Substring(idx + 1, endIdx - (idx + 1));
							string linkUrl = text.Substring(endIdx + 2, urlEndIdx - (endIdx + 2));
							line.AddLink(linkText, style, () => SafeProcessStart(linkUrl));
							idx = urlEndIdx + 1;
							continue;
						}
					}
				}

				// Icon
				if (text[idx] == ':')
				{
					int endIdx = text.IndexOf(':', idx + 1);
					if (endIdx != -1)
					{
						if (String.Compare(":alert:", 0, text, idx, endIdx - idx, StringComparison.Ordinal) == 0)
						{
							FlushMarkdownText(line, elementText, style);
							line.AddIcon(Properties.Resources.StatusIcons, new Size(16, 16), 4);
							idx = endIdx + 1;
							continue;
						}
					}
				}

				// Otherwise, just append the current character
				elementText.Append(text[idx++]);
			}
			FlushMarkdownText(line, elementText, style);

			return line;
		}

		private static void FlushMarkdownText(StatusLine line, StringBuilder elementText, FontStyle style)
		{
			if (elementText.Length > 0)
			{
				line.AddText(elementText.ToString(), style);
				elementText.Clear();
			}
		}

		private void SelectOtherStream(Rectangle bounds)
		{
			bool shownContextMenu = false;
			if (StreamName != null)
			{
				IReadOnlyList<string>? otherStreamNames = _workspace.ProjectStreamFilter;
				if (otherStreamNames != null)
				{
					StreamContextMenu.Items.Clear();

					ToolStripMenuItem currentStreamItem = new ToolStripMenuItem(StreamName, null, new EventHandler((s, e) => SelectStream(StreamName)));
					currentStreamItem.Checked = true;
					StreamContextMenu.Items.Add(currentStreamItem);

					StreamContextMenu.Items.Add(new ToolStripSeparator());

					foreach (string otherStreamName in otherStreamNames.OrderBy(x => x).Where(x => !x.EndsWith("/Dev-Binaries", StringComparison.OrdinalIgnoreCase)))
					{
						string thisStreamName = otherStreamName; // Local for lambda capture
						if (String.Equals(StreamName, otherStreamName, StringComparison.OrdinalIgnoreCase))
						{
							ToolStripMenuItem item = new ToolStripMenuItem(thisStreamName, null, new EventHandler((s, e) => SelectStream(thisStreamName)));
							StreamContextMenu.Items.Add(item);
						}
					}

					if (StreamContextMenu.Items.Count > 2)
					{
						StreamContextMenu.Items.Add(new ToolStripSeparator());
					}

					StreamContextMenu.Items.Add(new ToolStripMenuItem("Select Other...", null, new EventHandler((s, e) => SelectOtherStreamDialog())));

					int x = (bounds.Left + bounds.Right) / 2 + StreamContextMenu.Bounds.Width / 2;
					int y = bounds.Bottom + 2;
					StreamContextMenu.Show(StatusPanel, new Point(x, y), ToolStripDropDownDirection.Left);

					shownContextMenu = true;
				}
			}
			if (!shownContextMenu)
			{
				SelectOtherStreamDialog();
			}
		}

		private void SelectOtherStreamDialog()
		{
			string? newStreamName;
			if (SelectStreamWindow.ShowModal(this, _perforceSettings, StreamName, _serviceProvider, out newStreamName))
			{
				SelectStream(newStreamName);
			}
		}

		private void SelectStream(string newStreamName)
		{
			if (StreamName != newStreamName)
			{
				if (_workspace.IsBusy())
				{
					MessageBox.Show("Please retry after the current sync has finished.", "Sync in Progress");
				}
				else
				{
					for (int idx = 0; idx < 2; idx++)
					{
						async Task<bool> SwitchFunc(IPerforceConnection perforce, CancellationToken cancellationToken)
						{
							if (idx == 1 || !await perforce.OpenedAsync(OpenedOptions.None, FileSpecList.Any, cancellationToken).AnyAsync(cancellationToken))
							{
								await perforce.SwitchClientToStreamAsync(newStreamName, SwitchClientOptions.IgnoreOpenFiles, cancellationToken);
								return true;
							}
							return false;
						}

						ModalTask<bool>? switchTask = PerforceModalTask.Execute<bool>(this, "Switching streams", "Please wait...", _perforceSettings, SwitchFunc, _logger);
						if (switchTask == null || !switchTask.Succeeded)
						{
							break;
						}
						if (switchTask.Result)
						{
							StatusPanel.SuspendLayout();
							StreamChanged();

							// Reset the last code change we found when switching streams
							_workspace.ModifyState(x =>
							{
								x.CurrentCodeChangeNumber = -1;
							});

							StatusPanel.ResumeLayout();
							break;
						}
						if (MessageBox.Show("You have files open for edit in this workspace. If you continue, you will not be able to submit them until you switch back.\n\nContinue switching streams?", "Files checked out", MessageBoxButtons.YesNo) != DialogResult.Yes)
						{
							break;
						}
					}
				}
			}
		}

		private void ViewLastSyncStatus()
		{
			string? summaryText;
			if (GetLastUpdateMessage(_workspace.State.LastSyncResult, _workspace.State.LastSyncResultMessage, out summaryText))
			{
				string? captionText;
				if (!GetGenericLastUpdateMessage(_workspace.State.LastSyncResult, out captionText))
				{
					captionText = "Sync error";
				}
				MessageBox.Show(summaryText, captionText);
			}
		}

		private void ShowErrorInLog()
		{
			if (!Splitter.IsLogVisible())
			{
				ToggleLogVisibility();
			}
			SyncLog.ScrollToEnd();
		}

		private void ShowToolContextMenu(Rectangle bounds, ContextMenuStrip menuStrip)
		{
			menuStrip.Show(StatusPanel, new Point(bounds.Left, bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void ShowActionsMenu(Rectangle bounds)
		{
			MoreToolsContextMenu_CleanWorkspace.Image = s_hackToolStripMenuGutter;
			MoreToolsContextMenu.Show(StatusPanel, new Point(bounds.Left, bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void SelectChange(int changeNumber)
		{
			BuildList.SelectedItems.Clear();

			_pendingSelectedChangeNumber = -1;

			foreach (ListViewItem? item in BuildList.Items)
			{
				if (item != null)
				{
					ChangesRecord summary = (ChangesRecord)item.Tag;
					if (summary != null && summary.Number <= changeNumber)
					{
						item.Selected = true;
						item.EnsureVisible();
						return;
					}
				}
			}

			_pendingSelectedChangeNumber = changeNumber;

			int currentMaxChanges = _perforceMonitor.CurrentMaxChanges;
			if (_perforceMonitor.PendingMaxChanges <= currentMaxChanges && BuildList.SelectedItems.Count == 0)
			{
				_perforceMonitor.PendingMaxChanges = currentMaxChanges + BuildListExpandCount;
				_expandItem?.EnsureVisible();
			}
		}

		private void BuildList_MouseClick(object sender, MouseEventArgs args)
		{
			if (args.Button == MouseButtons.Left)
			{
				ListViewHitTestInfo hitTest = BuildList.HitTest(args.Location);
				if (hitTest.Item != null)
				{
					if (hitTest.Item == _expandItem)
					{
						if (HitTestExpandLink(args.Location))
						{
							int currentMaxChanges = _perforceMonitor.CurrentMaxChanges;
							if (_perforceMonitor.PendingMaxChanges > currentMaxChanges)
							{
								_perforceMonitor.PendingMaxChanges = currentMaxChanges;
							}
							else
							{
								_perforceMonitor.PendingMaxChanges = currentMaxChanges + BuildListExpandCount;
							}
							BuildList.Invalidate();
						}
					}
					else
					{
						ChangesRecord change = (ChangesRecord)hitTest.Item.Tag;
						if (_workspace.PendingChangeNumber == change.Number)
						{
							Rectangle subItemRect = hitTest.Item.SubItems[StatusColumn.Index].Bounds;

							if (_workspace.IsBusy())
							{
								Rectangle cancelRect = new Rectangle(subItemRect.Right - 16, subItemRect.Top, 16, subItemRect.Height);
								Rectangle infoRect = new Rectangle(subItemRect.Right - 32, subItemRect.Top, 16, subItemRect.Height);
								if (cancelRect.Contains(args.Location))
								{
									CancelWorkspaceUpdate();
								}
								else if (infoRect.Contains(args.Location) && !Splitter.IsLogVisible())
								{
									ToggleLogVisibility();
								}
							}
							else
							{
								Rectangle happyRect = new Rectangle(subItemRect.Right - 32, subItemRect.Top, 16, subItemRect.Height);
								Rectangle frownRect = new Rectangle(subItemRect.Right - 16, subItemRect.Top, 16, subItemRect.Height);
								if (happyRect.Contains(args.Location))
								{
									_eventMonitor.PostEvent(change.Number, EventType.Good);
									BuildList.Invalidate();
								}
								else if (frownRect.Contains(args.Location))
								{
									_eventMonitor.PostEvent(change.Number, EventType.Bad);
									BuildList.Invalidate();
								}
							}
						}
						else
						{
							Rectangle syncBadgeRectangle = GetSyncBadgeRectangle(hitTest.Item.SubItems[StatusColumn.Index].Bounds);
							if (syncBadgeRectangle.Contains(args.Location) && CanSyncChange(change.Number))
							{
								StartSync(change.Number, false, null);
							}
						}

						if (DescriptionColumn.Index < hitTest.Item.SubItems.Count && hitTest.Item.SubItems[DescriptionColumn.Index] == hitTest.SubItem)
						{
							ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo(change);
							if (layoutInfo._descriptionBadges.Count > 0)
							{
								Point buildListLocation = GetBadgeListLocation(layoutInfo._descriptionBadges, hitTest.SubItem.Bounds, HorizontalAlign.Right, VerticalAlignment.Middle);
								buildListLocation.Offset(-2, 0);

								foreach (BadgeInfo badge in layoutInfo._descriptionBadges)
								{
									Rectangle badgeBounds = badge.GetBounds(buildListLocation);
									if (badgeBounds.Contains(args.Location) && badge._leftClickHandler != null)
									{
										badge._leftClickHandler();
										break;
									}
								}
							}
						}

						if (CISColumn.Index < hitTest.Item.SubItems.Count && hitTest.Item.SubItems[CISColumn.Index] == hitTest.SubItem)
						{
							ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo(change);
							if (layoutInfo._buildBadges.Count > 0)
							{
								Point buildListLocation = GetBadgeListLocation(layoutInfo._buildBadges, hitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
								buildListLocation.X = Math.Max(buildListLocation.X, hitTest.SubItem.Bounds.Left);

								BadgeInfo? badgeInfo = HitTestBadge(args.Location, layoutInfo._buildBadges, buildListLocation);
								if (badgeInfo != null && badgeInfo._leftClickHandler != null)
								{
									badgeInfo._leftClickHandler();
								}
							}
						}

						foreach (ColumnHeader customColumn in _customColumns)
						{
							if (customColumn.Index < hitTest.Item.SubItems.Count && hitTest.Item.SubItems[customColumn.Index] == hitTest.SubItem)
							{
								ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo((ChangesRecord)hitTest.Item.Tag);

								List<BadgeInfo>? badges;
								if (layoutInfo._customBadges.TryGetValue(customColumn.Text, out badges) && badges.Count > 0)
								{
									Point listLocation = GetBadgeListLocation(badges, hitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);

									BadgeInfo? badgeInfo = HitTestBadge(args.Location, badges, listLocation);
									if (badgeInfo != null && badgeInfo._leftClickHandler != null)
									{
										badgeInfo._leftClickHandler();
									}
								}
							}
						}
					}
				}
			}
			else if (args.Button == MouseButtons.Right)
			{
				ListViewHitTestInfo hitTest = BuildList.HitTest(args.Location);
				if (hitTest.Item != null && hitTest.Item.Tag != null)
				{
					if (BuildList.SelectedItems.Count > 1 && BuildList.SelectedItems.Contains(hitTest.Item))
					{
						bool isTimeColumn = (hitTest.Item.SubItems.IndexOf(hitTest.SubItem) == TimeColumn.Index);
						BuildListMultiContextMenu_TimeZoneSeparator.Visible = isTimeColumn;
						BuildListMultiContextMenu_ShowLocalTimes.Visible = isTimeColumn;
						BuildListMultiContextMenu_ShowLocalTimes.Checked = _settings.ShowLocalTimes;
						BuildListMultiContextMenu_ShowServerTimes.Visible = isTimeColumn;
						BuildListMultiContextMenu_ShowServerTimes.Checked = !_settings.ShowLocalTimes;

						BuildListMultiContextMenu.Show(BuildList, args.Location);
					}
					else
					{
						bool badgeWasClicked = false;
						if (CISColumn.Index < hitTest.Item.SubItems.Count && hitTest.Item.SubItems[CISColumn.Index] == hitTest.SubItem)
						{
							ChangesRecord change = (ChangesRecord)hitTest.Item.Tag;
							ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo(change);
							if (layoutInfo._buildBadges.Count > 0)
							{
								Point buildListLocation = GetBadgeListLocation(layoutInfo._buildBadges, hitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
								buildListLocation.X = Math.Max(buildListLocation.X, hitTest.SubItem.Bounds.Left);

								BadgeInfo? badgeInfo = HitTestBadge(args.Location, layoutInfo._buildBadges, buildListLocation);
								if (badgeInfo != null)
								{
									badgeWasClicked = true;
									if (badgeInfo._rightClickHandler != null)
									{
										badgeInfo._rightClickHandler(BuildList, args.Location);
									}
								}
							}
						}

						if (!badgeWasClicked)
						{
							_contextMenuChange = (ChangesRecord)hitTest.Item.Tag;
							// Show time related context menu items if the click was made on the time column :
							bool isTimeColumn = (hitTest.Item.SubItems.IndexOf(hitTest.SubItem) == TimeColumn.Index);
							ShowChangelistContextMenu(BuildList, showTimeRelatedItems: isTimeColumn, args.Location);
						}
					}
				}
			}
		}

		private void ShowChangelistContextMenu(Control parentControl, bool showTimeRelatedItems, Point location, ToolStripDropDownDirection direction = ToolStripDropDownDirection.BelowRight)
		{
			if (_contextMenuChange != null)
			{
				BuildListContextMenu_Sync.Image = s_hackToolStripMenuGutter;
				BuildListContextMenu_Cancel.Image = s_hackToolStripMenuGutter;

				BuildListContextMenu_WithdrawReview.Visible = (_eventMonitor.GetReviewByCurrentUser(_contextMenuChange.Number) != null);
				BuildListContextMenu_StartInvestigating.Visible = !_eventMonitor.IsUnderInvestigationByCurrentUser(_contextMenuChange.Number);
				BuildListContextMenu_FinishInvestigating.Visible = _eventMonitor.IsUnderInvestigation(_contextMenuChange.Number);

				string? commentText;
				bool hasExistingComment = _eventMonitor.GetCommentByCurrentUser(_contextMenuChange.Number, out commentText);
				BuildListContextMenu_LeaveComment.Visible = !hasExistingComment;
				BuildListContextMenu_EditComment.Visible = hasExistingComment;

				bool isBusy = _workspace.IsBusy();
				bool isCurrentChange = (_contextMenuChange.Number == _workspace.CurrentChangeNumber);
				BuildListContextMenu_Sync.Visible = !isBusy;
				BuildListContextMenu_Sync.Font = new Font(SystemFonts.MenuFont!, isCurrentChange ? FontStyle.Regular : FontStyle.Bold);
				BuildListContextMenu_SyncContentOnly.Visible = !isBusy && ShouldSyncPrecompiledEditor;
				BuildListContextMenu_SyncOnlyThisChange.Visible = !isBusy && !isCurrentChange && _contextMenuChange.Number > _workspace.CurrentChangeNumber && _workspace.CurrentChangeNumber != -1;
				BuildListContextMenu_Build.Visible = !isBusy && isCurrentChange && !ShouldSyncPrecompiledEditor;
				BuildListContextMenu_Rebuild.Visible = !isBusy && isCurrentChange && !ShouldSyncPrecompiledEditor;
				BuildListContextMenu_GenerateProjectFiles.Visible = !isBusy && isCurrentChange;
				BuildListContextMenu_LaunchEditor.Visible = !isBusy && _contextMenuChange.Number == _workspace.CurrentChangeNumber;
				BuildListContextMenu_LaunchEditor.Font = new Font(SystemFonts.MenuFont!, FontStyle.Bold);
				BuildListContextMenu_OpenVisualStudio.Visible = !isBusy && isCurrentChange;
				BuildListContextMenu_Cancel.Visible = isBusy;

				BisectState state = _workspace.State.BisectChanges.FirstOrDefault(x => x.Change == _contextMenuChange.Number)?.State ?? BisectState.Include;
				bool isBisectMode = IsBisectModeEnabled();
				BuildListContextMenu_Bisect_Pass.Visible = isBisectMode && state != BisectState.Pass;
				BuildListContextMenu_Bisect_Fail.Visible = isBisectMode && state != BisectState.Fail;
				BuildListContextMenu_Bisect_Exclude.Visible = isBisectMode && state != BisectState.Exclude;
				BuildListContextMenu_Bisect_Include.Visible = isBisectMode && state != BisectState.Include;
				BuildListContextMenu_Bisect_Separator.Visible = isBisectMode;

				BuildListContextMenu_MarkGood.Visible = !isBisectMode;
				BuildListContextMenu_MarkBad.Visible = !isBisectMode;
				BuildListContextMenu_WithdrawReview.Visible = !isBisectMode;

				EventSummary? summary = _eventMonitor.GetSummaryForChange(_contextMenuChange.Number);
				bool starred = (summary != null && summary.LastStarReview != null && summary.LastStarReview.Type == EventType.Starred);
				BuildListContextMenu_AddStar.Visible = !starred;
				BuildListContextMenu_RemoveStar.Visible = starred;

				BuildListContextMenu_TimeZoneSeparator.Visible = showTimeRelatedItems;
				BuildListContextMenu_ShowLocalTimes.Visible = showTimeRelatedItems;
				BuildListContextMenu_ShowLocalTimes.Checked = _settings.ShowLocalTimes;
				BuildListContextMenu_ShowServerTimes.Visible = showTimeRelatedItems;
				BuildListContextMenu_ShowServerTimes.Checked = !_settings.ShowLocalTimes;

				int customToolStart = BuildListContextMenu.Items.IndexOf(BuildListContextMenu_CustomTool_Start) + 1;
				int customToolEnd = BuildListContextMenu.Items.IndexOf(BuildListContextMenu_CustomTool_End);
				while (customToolEnd > customToolStart)
				{
					BuildListContextMenu.Items.RemoveAt(customToolEnd - 1);
					customToolEnd--;
				}

				ConfigFile? projectConfigFile = _perforceMonitor.LatestProjectConfigFile;
				if (projectConfigFile != null)
				{
					Dictionary<string, string> variables = _workspace.GetVariables(GetEditorBuildConfig(), _contextMenuChange.Number, -1);

					string[] changeContextMenuEntries = projectConfigFile.GetValues("Options.ContextMenu", Array.Empty<string>());
					foreach (string changeContextMenuEntry in changeContextMenuEntries)
					{
						ConfigObject obj = new ConfigObject(changeContextMenuEntry);

						string? label = obj.GetValue("Label");
						string? execute = obj.GetValue("Execute");
						string? arguments = obj.GetValue("Arguments");

						if (label != null && execute != null)
						{
							label = Utility.ExpandVariables(label, variables);
							execute = Utility.ExpandVariables(execute, variables);
							arguments = Utility.ExpandVariables(arguments ?? "", variables);

#pragma warning disable CA2000 // warning CA2000: Call System.IDisposable.Dispose on object created by 'new ToolStripMenuItem(label, null, new EventHandler((o, a) => SafeProcessStart(execute, arguments)))' before all references to it are out of scope
							ToolStripMenuItem item = new ToolStripMenuItem(label, null, new EventHandler((o, a) => SafeProcessStart(execute, arguments)));
							BuildListContextMenu.Items.Insert(customToolEnd, item);
#pragma warning restore CA2000

							customToolEnd++;
						}
					}
				}

				BuildListContextMenu_CustomTool_End.Visible = (customToolEnd > customToolStart);

				string? swarmUrl;
				BuildListContextMenu_ViewInSwarm.Visible = projectConfigFile != null && TryGetProjectSetting(projectConfigFile, "SwarmURL", out swarmUrl);

				BuildListContextMenu.Show(parentControl, location, direction);
			}
		}

		private void BuildList_MouseMove(object sender, MouseEventArgs e)
		{
			ListViewHitTestInfo hitTest = BuildList.HitTest(e.Location);

			bool newMouseOverExpandLink = hitTest.Item != null && HitTestExpandLink(e.Location);
			if (_mouseOverExpandLink != newMouseOverExpandLink)
			{
				_mouseOverExpandLink = newMouseOverExpandLink;
				Cursor = _mouseOverExpandLink ? NativeCursors.Hand : Cursors.Arrow;
				BuildList.Invalidate();
			}

			string? newHoverBadgeUniqueId = null;
			if (hitTest.Item != null && hitTest.Item.Tag is ChangesRecord changesRecord)
			{
				int columnIndex = hitTest.Item.SubItems.IndexOf(hitTest.SubItem);
				if (columnIndex == DescriptionColumn.Index)
				{
					ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo(changesRecord);
					if (layoutInfo._descriptionBadges.Count > 0)
					{
						Point listLocation = GetBadgeListLocation(layoutInfo._descriptionBadges, hitTest.SubItem.Bounds, HorizontalAlign.Right, VerticalAlignment.Middle);
						newHoverBadgeUniqueId = HitTestBadge(e.Location, layoutInfo._descriptionBadges, listLocation)?._uniqueId;
					}
				}
				else if (columnIndex == CISColumn.Index)
				{
					ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo(changesRecord);
					if (layoutInfo._buildBadges.Count > 0)
					{
						Point buildListLocation = GetBadgeListLocation(layoutInfo._buildBadges, hitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
						buildListLocation.X = Math.Max(buildListLocation.X, hitTest.SubItem.Bounds.Left);

						BadgeInfo? badge = HitTestBadge(e.Location, layoutInfo._buildBadges, buildListLocation);
						newHoverBadgeUniqueId = badge?._uniqueId;

						if (_hoverBadgeUniqueId != newHoverBadgeUniqueId)
						{
							if (badge?._toolTip != null && badge._backgroundColor.A != 0)
							{
								BuildListToolTip.Show(badge._toolTip, BuildList, new Point(buildListLocation.X + badge._offset, hitTest.Item.Bounds.Bottom + 2));
							}
							else
							{
								BuildListToolTip.Hide(BuildList);
							}
						}
					}
				}
				else if (_customColumns.Contains(BuildList.Columns[columnIndex]))
				{
					ColumnHeader column = BuildList.Columns[columnIndex];

					ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo((ChangesRecord)hitTest.Item.Tag);

					List<BadgeInfo>? badges;
					if (layoutInfo._customBadges.TryGetValue(column.Text, out badges) && badges.Count > 0)
					{
						Point listLocation = GetBadgeListLocation(badges, hitTest.SubItem.Bounds, HorizontalAlign.Center, VerticalAlignment.Middle);
						newHoverBadgeUniqueId = HitTestBadge(e.Location, badges, listLocation)?._uniqueId;
					}
				}
			}
			if (_hoverBadgeUniqueId != newHoverBadgeUniqueId)
			{
				_hoverBadgeUniqueId = newHoverBadgeUniqueId;
				BuildList.Invalidate();
			}

			bool newHoverSync = false;
			if (hitTest.Item != null && StatusColumn.Index < hitTest.Item.SubItems.Count)
			{
				newHoverSync = GetSyncBadgeRectangle(hitTest.Item.SubItems[StatusColumn.Index].Bounds).Contains(e.Location);
			}
			if (newHoverSync != _hoverSync)
			{
				_hoverSync = newHoverSync;
				BuildList.Invalidate();
			}
		}

		private static BadgeInfo? HitTestBadge(Point location, List<BadgeInfo> badgeList, Point listLocation)
		{
			foreach (BadgeInfo badge in badgeList)
			{
				Rectangle badgeBounds = badge.GetBounds(listLocation);
				if (badgeBounds.Contains(location))
				{
					return badge;
				}
			}
			return null;
		}

		private List<IArchiveChannel> GetSelectedArchiveChannels(IReadOnlyList<IArchiveChannel> archives)
		{
			Dictionary<string, KeyValuePair<IArchiveChannel, int>> archiveTypeToSelection = new Dictionary<string, KeyValuePair<IArchiveChannel, int>>();
			foreach (IArchiveChannel archive in archives)
			{
				ArchiveSettings? archiveSettings = _settings.Archives.FirstOrDefault(x => x.Type == archive.Type);
				if (archiveSettings != null && archiveSettings.Enabled)
				{
					int preference = archiveSettings.Order.IndexOf(archive.Name);
					if (preference == -1)
					{
						preference = archiveSettings.Order.Count;
					}

					KeyValuePair<IArchiveChannel, int> existingItem;
					if (!archiveTypeToSelection.TryGetValue(archive.Type, out existingItem) || existingItem.Value > preference)
					{
						archiveTypeToSelection[archive.Type] = new KeyValuePair<IArchiveChannel, int>(archive, preference);
					}
				}
			}
			return archiveTypeToSelection.Select(x => x.Value.Key).ToList();
		}

		private void ClearSelectedArchives()
		{
			foreach (ArchiveSettings archiveSettings in _settings.Archives)
			{
				archiveSettings.Enabled = false;
			}
			_settings.Save(_logger);

			UpdateSelectedArchives();
		}

		private void SetSelectedArchive(IArchiveChannel archive, bool selected)
		{
			ArchiveSettings? archiveSettings = _settings.Archives.FirstOrDefault(x => x.Type == archive.Type);
			if (archiveSettings == null)
			{
				archiveSettings = new ArchiveSettings(selected, archive.Type, new string[] { archive.Name });
				_settings.Archives.Add(archiveSettings);
			}
			else if (selected)
			{
				archiveSettings.Enabled = true;
				archiveSettings.Order.Remove(archive.Name);
				archiveSettings.Order.Insert(0, archive.Name);
			}
			else
			{
				archiveSettings.Enabled = false;
			}
			_settings.Save(_logger);

			UpdateSelectedArchives();
		}

		private void UpdateSelectedArchives()
		{
			UpdateBuildSteps();
			UpdateSyncActionCheckboxes();

			BuildList.Invalidate();
		}

		private void OptionsButton_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AutoResolveConflicts.Checked = _settings.Global.AutoResolveConflicts;
			OptionsContextMenu_AlwaysClobberFiles.Checked = _settings.Global.AlwaysClobberFiles;

			OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.Clear();

			IReadOnlyList<IArchiveChannel> channels = GetArchiveChannels();
			if (channels == null || channels.Count == 0)
			{
				OptionsContextMenu_SyncPrecompiledBinaries.Enabled = false;
				OptionsContextMenu_SyncPrecompiledBinaries.ToolTipText = String.Format("Precompiled binaries are not available for {0}", SelectedProjectIdentifier);
				OptionsContextMenu_SyncPrecompiledBinaries.Checked = false;
			}
			else
			{
				List<IArchiveChannel> selectedArchives = GetSelectedArchiveChannels(channels);

				OptionsContextMenu_SyncPrecompiledBinaries.Enabled = true;
				OptionsContextMenu_SyncPrecompiledBinaries.ToolTipText = null;
				OptionsContextMenu_SyncPrecompiledBinaries.Checked = selectedArchives.Count > 0;

				if (channels.Count > 1 || channels[0].Type != IArchiveChannel.EditorArchiveType)
				{
					ToolStripMenuItem disableItem = new ToolStripMenuItem("Disable (compile locally)");
					disableItem.Checked = (selectedArchives.Count == 0);
					disableItem.Click += (sender, args) => ClearSelectedArchives();
					OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.Add(disableItem);

					ToolStripSeparator separator = new ToolStripSeparator();
					OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.Add(separator);

					foreach (IArchiveChannel channel in channels)
					{
						ToolStripMenuItem item = new ToolStripMenuItem(channel.Name);
						item.Enabled = channel.HasAny();
						item.ToolTipText = channel.ToolTip;
						item.Checked = selectedArchives.Contains(channel);
						item.Click += (sender, args) => SetSelectedArchive(channel, !item.Checked);
						OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.Add(item);
					}
				}
			}

			OptionsContextMenu_EditorBuildConfiguration.Enabled = !ShouldSyncPrecompiledEditor;
			UpdateCheckedBuildConfig();
			OptionsContextMenu_CustomizeBuildSteps.Enabled = (_workspace != null);
			OptionsContextMenu_EditorArguments.Checked = _settings.EditorArguments.Any(x => x.Item2);
			OptionsContextMenu_ScheduledSync.Checked = _settings.ScheduleEnabled;
			OptionsContextMenu_TimeZone_Local.Checked = _settings.ShowLocalTimes;
			OptionsContextMenu_TimeZone_PerforceServer.Checked = !_settings.ShowLocalTimes;
			OptionsContextMenu_ShowChanges_ShowUnreviewed.Checked = _settings.ShowUnreviewedChanges;
			OptionsContextMenu_ShowChanges_ShowAutomated.Checked = _settings.ShowAutomatedChanges;
			OptionsContextMenu_TabNames_Stream.Checked = _settings.TabLabels == TabLabels.Stream;
			OptionsContextMenu_TabNames_WorkspaceName.Checked = _settings.TabLabels == TabLabels.WorkspaceName;
			OptionsContextMenu_TabNames_WorkspaceRoot.Checked = _settings.TabLabels == TabLabels.WorkspaceRoot;
			OptionsContextMenu_TabNames_ProjectFile.Checked = _settings.TabLabels == TabLabels.ProjectFile;
			OptionsContextMenu.Show(OptionsButton, new Point(OptionsButton.Width - OptionsContextMenu.Size.Width, OptionsButton.Height));
		}

		private IReadOnlyList<IArchiveChannel> GetArchiveChannels()
		{
			if (_perforceMonitor != null)
			{
				return _perforceMonitor.AvailableArchiveChannels;
			}

			return new List<IArchiveChannel>();
		}

		private void BuildAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			_settings.BuildAfterSync = BuildAfterSyncCheckBox.Checked;
			_settings.Save(_logger);

			UpdateSyncActionCheckboxes();
		}

		private void RunAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			_settings.RunAfterSync = RunAfterSyncCheckBox.Checked;
			_settings.Save(_logger);

			UpdateSyncActionCheckboxes();
		}

		private void OpenSolutionAfterSyncCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			_settings.OpenSolutionAfterSync = OpenSolutionAfterSyncCheckBox.Checked;
			_settings.Save(_logger);

			UpdateSyncActionCheckboxes();
		}

		private void UpdateSyncActionCheckboxes()
		{
			BuildAfterSyncCheckBox.Checked = _settings.BuildAfterSync;
			RunAfterSyncCheckBox.Checked = _settings.RunAfterSync;
			OpenSolutionAfterSyncCheckBox.Checked = _settings.OpenSolutionAfterSync;

			if (_workspace == null || _workspace.IsBusy())
			{
				AfterSyncingLabel.Enabled = false;
				BuildAfterSyncCheckBox.Enabled = false;
				RunAfterSyncCheckBox.Enabled = false;
				OpenSolutionAfterSyncCheckBox.Enabled = false;
			}
			else
			{
				AfterSyncingLabel.Enabled = true;
				BuildAfterSyncCheckBox.Enabled = _hasBuildSteps;
				RunAfterSyncCheckBox.Enabled = BuildAfterSyncCheckBox.Checked || ShouldSyncPrecompiledEditor;
				OpenSolutionAfterSyncCheckBox.Enabled = !ShouldSyncPrecompiledEditor;
			}
		}

		private void BuildList_FontChanged(object? sender, EventArgs e)
		{
			_buildFont?.Dispose();
			_buildFont = BuildList.Font;

			_selectedBuildFont?.Dispose();
			_selectedBuildFont = new Font(_buildFont, FontStyle.Bold);

			_badgeFont?.Dispose();
			_badgeFont = new Font(_buildFont.FontFamily, _buildFont.SizeInPoints - 2, FontStyle.Bold);
		}

		public void ExecCommand(string description, string statusText, string fileName, string arguments, string workingDir, bool useLogWindow)
		{
			if (_workspace != null && !_workspace.IsBusy())
			{
				Guid id = Guid.NewGuid();

				BuildStep customBuildStep = new BuildStep(Guid.NewGuid(), 0, description, statusText, 1, fileName, arguments, workingDir, useLogWindow);

				List<ConfigObject> userBuildSteps = new List<ConfigObject>();
				userBuildSteps.Add(customBuildStep.ToConfigObject());

				WorkspaceUpdateContext context = new WorkspaceUpdateContext(_workspace.CurrentChangeNumber, WorkspaceUpdateOptions.Build, GetEditorBuildConfig(), null, userBuildSteps, new HashSet<Guid> { customBuildStep.UniqueId });
				StartWorkspaceUpdate(context, null);
			}
		}

		public void SyncChange(int changeNumber, bool syncOnly, WorkspaceUpdateCallback? callback)
		{
			if (_workspace != null)
			{
				_owner.ShowAndActivate();
				SelectChange(changeNumber);
				StartSync(changeNumber, syncOnly, callback);
			}
		}

		public void SyncLatestChange()
		{
			SyncLatestChange(null);
		}

		public void SyncLatestChange(WorkspaceUpdateCallback? callback)
		{
			if (_workspace != null)
			{
				int changeNumber;
				if (!FindChangeToSync(_settings.SyncTypeId, out changeNumber))
				{
					string syncTypeName = "";
					foreach (LatestChangeType changeType in GetCustomLatestChangeTypes())
					{
						if (changeType.Name == _settings.SyncTypeId)
						{
							syncTypeName = changeType.Name;
							break;
						}
					}
					ShowErrorDialog(String.Format("Couldn't find any {0}changelist. Double-click on the change you want to sync manually.", syncTypeName));
				}
				else if (changeNumber < _workspace.CurrentChangeNumber)
				{
					ShowErrorDialog("Workspace is already synced to a later change ({0} vs {1}).", _workspace.CurrentChangeNumber, changeNumber);
				}
				else if (changeNumber >= _perforceMonitor.LastChangeByCurrentUser || MessageBox.Show(String.Format("The changelist that would be synced is before the last change you submitted.\n\nIf you continue, your changes submitted after CL {0} will be locally removed from your workspace until you can sync past them again.\n\nAre you sure you want to continue?", changeNumber), "Local changes will be removed", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					SyncChange(changeNumber, false, callback);
				}
			}
		}

		private void BuildListContextMenu_MarkGood_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				_eventMonitor.PostEvent(_contextMenuChange.Number, EventType.Good);
			}
		}

		private void BuildListContextMenu_MarkBad_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				_eventMonitor.PostEvent(_contextMenuChange.Number, EventType.Bad);
			}
		}

		private void BuildListContextMenu_WithdrawReview_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				_eventMonitor.PostEvent(_contextMenuChange.Number, EventType.Unknown);
			}
		}

		private void BuildListContextMenu_Sync_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				StartSync(_contextMenuChange.Number);
			}
		}

		private void BuildListContextMenu_SyncContentOnly_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				StartWorkspaceUpdate(_contextMenuChange.Number, WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.ContentOnly);
			}
		}

		private void BuildListContextMenu_SyncOnlyThisChange_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				StartWorkspaceUpdate(_contextMenuChange.Number, WorkspaceUpdateOptions.SyncSingleChange);
			}
		}

		private void BuildListContextMenu_Build_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(_workspace.CurrentChangeNumber, WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.Build);
		}

		private void BuildListContextMenu_Rebuild_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(_workspace.CurrentChangeNumber, WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.Clean);
		}

		private void BuildListContextMenu_GenerateProjectFiles_Click(object sender, EventArgs e)
		{
			StartWorkspaceUpdate(_workspace.CurrentChangeNumber, WorkspaceUpdateOptions.GenerateProjectFiles);
		}

		private void BuildListContextMenu_CancelSync_Click(object sender, EventArgs e)
		{
			CancelWorkspaceUpdate();
		}

		private void BuildListContextMenu_MoreInfo_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				Program.SpawnP4Vc(String.Format("{0} change {1}", _perforceSettings.GetArgumentsForExternalProgram(true), _contextMenuChange.Number));
			}
		}

		private void BuildListContextMenu_ViewInSwarm_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				string? swarmUrl;
				if (TryGetProjectSetting(_perforceMonitor.LatestProjectConfigFile, "SwarmURL", out swarmUrl))
				{
					Utility.OpenUrl(String.Format("{0}/changes/{1}", swarmUrl, _contextMenuChange.Number));
				}
				else
				{
					MessageBox.Show("Swarm URL is not configured.");
				}
			}
		}

		private void BuildListContextMenu_CopyChangelistNumber_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				try
				{
					Clipboard.SetText(_contextMenuChange.Number.ToString());
				}
				catch
				{
					MessageBox.Show("Unable to copy data to clipboard; another process may have the clipboard locked.");
				}
			}
		}

		private void BuildListContextMenu_AddStar_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				if (MessageBox.Show("Starred builds are meant to convey a stable, verified build to the rest of the team. Do not star a build unless it has been fully tested.\n\nAre you sure you want to star this build?", "Confirm star", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					_eventMonitor.PostEvent(_contextMenuChange.Number, EventType.Starred);
				}
			}
		}

		private void BuildListContextMenu_RemoveStar_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				EventSummary? summary = _eventMonitor.GetSummaryForChange(_contextMenuChange.Number);
				if (summary != null && summary.LastStarReview != null && summary.LastStarReview.Type == EventType.Starred)
				{
					string message = String.Format("This change was starred by {0}. Are you sure you want to remove it?", FormatUserName(summary.LastStarReview.UserName));
					if (MessageBox.Show(message, "Confirm removing star", MessageBoxButtons.YesNo) == DialogResult.Yes)
					{
						_eventMonitor.PostEvent(_contextMenuChange.Number, EventType.Unstarred);
					}
				}
			}
		}

		private void BuildListContextMenu_LaunchEditor_Click(object sender, EventArgs e)
		{
			LaunchEditor();
		}

		private void BuildListContextMenu_StartInvestigating_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				string message = String.Format("All changes from {0} onwards will be marked as bad while you are investigating an issue.\n\nAre you sure you want to continue?", _contextMenuChange.Number);
				if (MessageBox.Show(message, "Confirm investigating", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					_eventMonitor.StartInvestigating(_contextMenuChange.Number);
				}
			}
		}

		private void BuildListContextMenu_FinishInvestigating_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				int startChangeNumber = _eventMonitor.GetInvestigationStartChangeNumber(_contextMenuChange.Number);
				if (startChangeNumber != -1)
				{
					if (_contextMenuChange.Number > startChangeNumber)
					{
						string message = String.Format("Mark all changes between {0} and {1} as bad?", startChangeNumber, _contextMenuChange.Number);
						if (MessageBox.Show(message, "Finish investigating", MessageBoxButtons.YesNo) == DialogResult.Yes)
						{
							foreach (ChangesRecord change in _perforceMonitor.GetChanges())
							{
								if (change.Number >= startChangeNumber && change.Number < _contextMenuChange.Number)
								{
									_eventMonitor.PostEvent(change.Number, EventType.Bad);
								}
							}
						}
					}
					_eventMonitor.FinishInvestigating(_contextMenuChange.Number);
				}
			}
		}

		private void OnlyShowReviewedCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateBuildList();
			ShrinkNumRequestedBuilds();
		}

		public void Activate()
		{
			UpdateSyncActionCheckboxes();

			_perforceMonitor?.Refresh();

			if (DesiredTaskbarState.Item1 == TaskbarState.Error)
			{
				DesiredTaskbarState = Tuple.Create(TaskbarState.NoProgress, 0.0f);
				_owner.UpdateProgress();
			}
		}

		public void Deactivate()
		{
			ShrinkNumRequestedBuilds();
		}

		private void BuildList_ItemMouseHover(object sender, ListViewItemMouseHoverEventArgs args)
		{
			Point clientPoint = BuildList.PointToClient(Cursor.Position);
			if (args.Item.SubItems.Count >= 6)
			{
				if (args.Item.Bounds.Contains(clientPoint))
				{
					ChangesRecord change = (ChangesRecord)args.Item.Tag;
					if (change == null)
					{
						return;
					}

					if (CISColumn.Index < args.Item.SubItems.Count)
					{
						Rectangle cisBounds = args.Item.SubItems[CISColumn.Index].Bounds;
						if (cisBounds.Contains(clientPoint))
						{
							return;
						}
					}

					if (DescriptionColumn.Index < args.Item.SubItems.Count)
					{
						Rectangle descriptionBounds = args.Item.SubItems[DescriptionColumn.Index].Bounds;
						if (descriptionBounds.Contains(clientPoint))
						{
							ChangeLayoutInfo layoutInfo = GetChangeLayoutInfo(change);
							if (layoutInfo._descriptionBadges.Count == 0 || clientPoint.X < GetBadgeListLocation(layoutInfo._descriptionBadges, descriptionBounds, HorizontalAlign.Right, VerticalAlignment.Middle).X - 2)
							{
								BuildListToolTip.Show(change.Description, BuildList, new Point(descriptionBounds.Left, descriptionBounds.Bottom + 2));
								return;
							}
						}
					}

					EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);
					if (summary != null)
					{
						StringBuilder summaryText = new StringBuilder();
						if (summary.Comments.Count > 0)
						{
							foreach (CommentData comment in summary.Comments)
							{
								if (!String.IsNullOrWhiteSpace(comment.Text))
								{
									summaryText.AppendFormat("{0}: \"{1}\"\n", FormatUserName(comment.UserName), comment.Text);
								}
							}
							if (summaryText.Length > 0)
							{
								summaryText.Append('\n');
							}
						}
						AppendUserList(summaryText, "\n", "Compiled by {0}.", summary.Reviews.Where(x => x.Type == EventType.Compiles));
						AppendUserList(summaryText, "\n", "Failed to compile for {0}.", summary.Reviews.Where(x => x.Type == EventType.DoesNotCompile));
						AppendUserList(summaryText, "\n", "Marked good by {0}.", summary.Reviews.Where(x => x.Type == EventType.Good));
						AppendUserList(summaryText, "\n", "Marked bad by {0}.", summary.Reviews.Where(x => x.Type == EventType.Bad));
						if (summary.LastStarReview != null)
						{
							AppendUserList(summaryText, "\n", "Starred by {0}.", new EventData[] { summary.LastStarReview });
						}
						if (summaryText.Length > 0 && StatusColumn.Index < args.Item.SubItems.Count)
						{
							Rectangle summaryBounds = args.Item.SubItems[StatusColumn.Index].Bounds;
							BuildListToolTip.Show(summaryText.ToString(), BuildList, new Point(summaryBounds.Left, summaryBounds.Bottom));
							return;
						}
					}
				}
			}

			BuildListToolTip.Hide(BuildList);
		}

		private void BuildList_MouseLeave(object sender, EventArgs e)
		{
			BuildListToolTip.Hide(BuildList);

			if (_hoverBadgeUniqueId != null)
			{
				_hoverBadgeUniqueId = null;
				BuildList.Invalidate();
			}

			if (_mouseOverExpandLink)
			{
				Cursor = Cursors.Arrow;
				_mouseOverExpandLink = false;
				BuildList.Invalidate();
			}
		}

		private void OptionsContextMenu_ShowLog_Click(object sender, EventArgs e)
		{
			ToggleLogVisibility();
		}

		private void ToggleLogVisibility()
		{
			Splitter.SetLogVisibility(!Splitter.IsLogVisible());
		}

		private void Splitter_OnVisibilityChanged(bool visible)
		{
			_settings.ShowLogWindow = visible;
			_settings.Save(_logger);

			UpdateStatusPanel();
		}

		private void OptionsContextMenu_AutoResolveConflicts_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AutoResolveConflicts.Checked ^= true;
			_settings.Global.AutoResolveConflicts = OptionsContextMenu_AutoResolveConflicts.Checked;
			_settings.Save(_logger);
		}

		private void OptionsContextMenu_AlwaysClobberFiles_Click(object sender, EventArgs e)
		{
			OptionsContextMenu_AlwaysClobberFiles.Checked ^= true;
			_settings.Global.AlwaysClobberFiles = OptionsContextMenu_AlwaysClobberFiles.Checked;
			_settings.Save(_logger);
		}

		private void OptionsContextMenu_EditorArguments_Click(object sender, EventArgs e)
		{
			ModifyEditorArguments();
		}

		private bool ModifyEditorArguments()
		{
			using ArgumentsWindow arguments = new ArgumentsWindow(_settings.EditorArguments, _settings.EditorArgumentsPrompt);
			if (arguments.ShowDialog(this) == DialogResult.OK)
			{
				_settings.EditorArguments.Clear();
				_settings.EditorArguments.AddRange(arguments.GetItems());
				_settings.EditorArgumentsPrompt = arguments.PromptBeforeLaunch;
				_settings.Save(_logger);
				return true;
			}
			return false;
		}

		private void BuildListContextMenu_OpenVisualStudio_Click(object sender, EventArgs e)
		{
			OpenSolution();
		}

		private void OpenSolution()
		{
			string primaryProjectName = "UE4";
			if (FileReference.Exists(FileReference.Combine(BranchDirectoryName, "UE5.sln")))
			{
				primaryProjectName = "UE5";
			}

			FileReference primaryProjectNameFileName = FileReference.Combine(BranchDirectoryName, "Engine", "Intermediate", "ProjectFiles", "PrimaryProjectName.txt");
			if (!FileReference.Exists(primaryProjectNameFileName))
			{
				// Old path, needs to be maintained while projects prior to UE5.1 are supported
				primaryProjectNameFileName = FileReference.Combine(BranchDirectoryName, "Engine", "Intermediate", "ProjectFiles", "MasterProjectName.txt");
			}
			if (FileReference.Exists(primaryProjectNameFileName))
			{
				try
				{
					primaryProjectName = FileReference.ReadAllText(primaryProjectNameFileName).Trim();
				}
				catch (Exception ex)
				{
					_logger.LogError(ex, "Unable to read '{File}'", primaryProjectNameFileName);
				}
			}

			FileReference solutionFileName = FileReference.Combine(BranchDirectoryName, primaryProjectName + ".sln");
			if (!FileReference.Exists(solutionFileName))
			{
				MessageBox.Show(String.Format("Couldn't find solution at {0}", solutionFileName));
			}
			else
			{
				ProcessStartInfo startInfo = new ProcessStartInfo(solutionFileName.FullName);
				startInfo.UseShellExecute = true;
				startInfo.WorkingDirectory = BranchDirectoryName.FullName;
				SafeProcessStart(startInfo);
			}
		}

		private void OptionsContextMenu_BuildConfig_Debug_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.Debug);
		}

		private void OptionsContextMenu_BuildConfig_DebugGame_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.DebugGame);
		}

		private void OptionsContextMenu_BuildConfig_Development_Click(object sender, EventArgs e)
		{
			UpdateBuildConfig(BuildConfig.Development);
		}

		void UpdateBuildConfig(BuildConfig newConfig)
		{
			_settings.CompiledEditorBuildConfig = newConfig;
			_settings.Save(_logger);
			UpdateCheckedBuildConfig();
		}

		void UpdateCheckedBuildConfig()
		{
			BuildConfig editorBuildConfig = GetEditorBuildConfig();

			OptionsContextMenu_BuildConfig_Debug.Checked = (editorBuildConfig == BuildConfig.Debug);
			OptionsContextMenu_BuildConfig_Debug.Enabled = (!ShouldSyncPrecompiledEditor || editorBuildConfig == BuildConfig.Debug);

			OptionsContextMenu_BuildConfig_DebugGame.Checked = (editorBuildConfig == BuildConfig.DebugGame);
			OptionsContextMenu_BuildConfig_DebugGame.Enabled = (!ShouldSyncPrecompiledEditor || editorBuildConfig == BuildConfig.DebugGame);

			OptionsContextMenu_BuildConfig_Development.Checked = (editorBuildConfig == BuildConfig.Development);
			OptionsContextMenu_BuildConfig_Development.Enabled = (!ShouldSyncPrecompiledEditor || editorBuildConfig == BuildConfig.Development);
		}

		private void OptionsContextMenu_ScheduleSync_Click(object sender, EventArgs e)
		{
			_owner.SetupScheduledSync();
		}

		public void ScheduleTimerElapsed()
		{
			if (_settings.ScheduleEnabled)
			{
				// Check the project config if they have a scheduled setting selected.
				string? scheduledSyncTypeId = null;
				UserSelectedProjectSettings? projectSetting = _settings.ScheduleProjects.FirstOrDefault(x => x.LocalPath != null && x.LocalPath.Equals(SelectedProject.LocalPath, StringComparison.OrdinalIgnoreCase));
				if (projectSetting != null)
				{
					scheduledSyncTypeId = projectSetting.ScheduledSyncTypeId;
				}

				_logger.LogInformation("Scheduled sync at {Time} for {Type}.",
					DateTime.Now,
					scheduledSyncTypeId ?? "Default");

				int changeNumber;
				if (!FindChangeToSync(scheduledSyncTypeId, out changeNumber))
				{
					SyncLog.AppendLine("Couldn't find any matching change");
				}
				else if (_workspace.CurrentChangeNumber >= changeNumber)
				{
					_logger.LogInformation("Sync ignored; already at or ahead of CL ({CurrentChange} >= {Change})", _workspace.CurrentChangeNumber, changeNumber);
				}
				else
				{
					WorkspaceUpdateOptions options = WorkspaceUpdateOptions.Sync | WorkspaceUpdateOptions.SyncArchives | WorkspaceUpdateOptions.GenerateProjectFiles | WorkspaceUpdateOptions.Build | WorkspaceUpdateOptions.ScheduledBuild;
					if (_settings.Global.AutoResolveConflicts)
					{
						options |= WorkspaceUpdateOptions.AutoResolveChanges;
					}
					StartWorkspaceUpdate(changeNumber, options);
				}
			}
		}

		bool FindChangeToSync(string? changeTypeId, out int changeNumber)
		{
			if (changeTypeId != null)
			{
				LatestChangeType? changeTypeToSyncTo = null;
				foreach (LatestChangeType changeType in GetCustomLatestChangeTypes())
				{
					if (changeType.Name == changeTypeId)
					{
						changeTypeToSyncTo = changeType;
						break;
					}
				}

				if (changeTypeToSyncTo != null)
				{
					return FindChangeToSync(changeTypeToSyncTo, out changeNumber);
				}
			}

			SyncLog.AppendLine("Invalid or not set Scheduled Sync type using (Sync to Latest Good)");

			return FindChangeToSync(LatestChangeType.LatestGoodChange(), out changeNumber);
		}

		bool FindChangeToSync(LatestChangeType changeType, out int changeNumber)
		{
			if (!String.IsNullOrEmpty(changeType.ReadFrom))
			{
				async Task<int> ReadChangeToSync(IPerforceConnection perforceConnection, CancellationToken cancellationToken)
				{
					PerforceResponse<PrintRecord<byte[]>> record = await perforceConnection.TryPrintDataAsync(changeType.ReadFrom, cancellationToken);
					record.EnsureSuccess();

					string contents = Encoding.UTF8.GetString(record.Data.Contents ?? Array.Empty<byte>());
					return Int32.Parse(contents.Trim());
				}

				ModalTask<int>? task = PerforceModalTask.Execute(this, "Reading change", "Reading latest change...", _perforceSettings, ReadChangeToSync, _logger);
				if (task == null || !task.Succeeded || task.Result <= 0)
				{
					changeNumber = 0;
					return false;
				}
				else
				{
					changeNumber = task.Result;
					return true;
				}
			}

			for (int idx = 0; idx < BuildList.Items.Count; idx++)
			{
				ChangesRecord change = (ChangesRecord)BuildList.Items[idx].Tag;
				if (change != null)
				{
					EventSummary? summary = _eventMonitor.GetSummaryForChange(change.Number);
					if (CanSyncChange(change.Number)
						&& CanSyncChangeType(changeType, change, summary))
					{
						if (changeType.FindNewestGoodContent)
						{
							changeNumber = FindNewestGoodContentChange(change.Number);
						}
						else
						{
							changeNumber = change.Number;
						}

						return true;
					}
				}
			}

			changeNumber = -1;
			return false;
		}

		int FindNewestGoodContentChange(int changeNumber)
		{
			int index = _sortedChangeNumbers.BinarySearch(changeNumber);
			if (index <= 0)
			{
				return changeNumber;
			}

			for (int nextIndex = index + 1; nextIndex < _sortedChangeNumbers.Count; nextIndex++)
			{
				int nextChangeNumber = _sortedChangeNumbers[nextIndex];

				PerforceChangeDetails? details;
				if (!_perforceMonitor.TryGetChangeDetails(nextChangeNumber, out details) || details.ContainsCode)
				{
					break;
				}

				EventSummary? summary = _eventMonitor.GetSummaryForChange(nextChangeNumber);
				if (summary != null && summary.Verdict == ReviewVerdict.Bad)
				{
					break;
				}

				index = nextIndex;
			}

			return _sortedChangeNumbers[index];
		}

		private void BuildListContextMenu_LeaveOrEditComment_Click(object sender, EventArgs e)
		{
			if (_contextMenuChange != null)
			{
				using LeaveCommentWindow leaveComment = new LeaveCommentWindow();

				string? commentText;
				if (_eventMonitor.GetCommentByCurrentUser(_contextMenuChange.Number, out commentText))
				{
					leaveComment.CommentTextBox.Text = commentText;
				}

				if (leaveComment.ShowDialog() == System.Windows.Forms.DialogResult.OK)
				{
					_eventMonitor.PostComment(_contextMenuChange.Number, leaveComment.CommentTextBox.Text);
				}
			}
		}

		private void BuildListContextMenu_ShowServerTimes_Click(object sender, EventArgs e)
		{
			_settings.ShowLocalTimes = false;
			_settings.Save(_logger);
			BuildList.Items.Clear(); // Need to clear list to rebuild groups, populate time column again
			UpdateBuildList();
		}

		private void BuildListContextMenu_ShowLocalTimes_Click(object sender, EventArgs e)
		{
			_settings.ShowLocalTimes = true;
			_settings.Save(_logger);
			BuildList.Items.Clear(); // Need to clear list to rebuild groups, populate time column again
			UpdateBuildList();
		}

		private void MoreToolsContextMenu_CleanWorkspace_Click(object sender, EventArgs e)
		{
			if (!WaitForProgramsToFinish())
			{
				return;
			}

			string? extraSafeToDeleteFolders;
			if (!TryGetProjectSetting(_workspace.ProjectConfigFile, "SafeToDeleteFolders", out extraSafeToDeleteFolders))
			{
				extraSafeToDeleteFolders = "";
			}

			string? extraSafeToDeleteExtensions;
			if (!TryGetProjectSetting(_workspace.ProjectConfigFile, "SafeToDeleteExtensions", out extraSafeToDeleteExtensions))
			{
				extraSafeToDeleteExtensions = "";
			}

			string[] combinedSyncFilter = UserSettings.GetCombinedSyncFilter(GetSyncCategories(), _settings.Global.Filter, _workspaceSettings.Filter, _perforceMonitor.LatestPerforceConfigSection());
			List<string> syncPaths = WorkspaceUpdate.GetSyncPaths(_workspace.Project, _workspaceSettings.Filter.AllProjects ?? _settings.Global.Filter.AllProjects ?? false, combinedSyncFilter);

			bool performedClean = CleanWorkspaceWindow.DoClean(ParentForm, _perforceSettings, BranchDirectoryName, _workspace.Project.ClientRootPath, syncPaths, extraSafeToDeleteFolders.Split('\n'), extraSafeToDeleteExtensions.Split('\n'), _serviceProvider.GetRequiredService<ILogger<CleanWorkspaceWindow>>());

			if (performedClean)
			{
				_workspace.ModifyState(x =>
				{
					x.LastSyncEditorArchive = "0";
				});
			}
		}

		private void MoreToolsContextMenu_UpdateTools_Click(object sender, EventArgs e)
		{
			_logger.LogInformation("Checking for tools updates...");
			_owner.ToolUpdateMonitor.UpdateNow();
		}

		private void UpdateBuildSteps()
		{
			_hasBuildSteps = false;

			int moreToolsItemCount = MoreToolsContextMenu.Items.IndexOf(MoreActionsContextMenu_CustomToolSeparator);
			while (moreToolsItemCount > 0)
			{
				MoreToolsContextMenu.Items.RemoveAt(--moreToolsItemCount);
			}

			for (int idx = 0; idx < _customStatusPanelMenus.Count; idx++)
			{
				components.Remove(_customStatusPanelMenus[idx]);
				_customStatusPanelMenus[idx].Dispose();
			}

			_customStatusPanelLinks.Clear();
			_customStatusPanelMenus.Clear();

			if (_workspace != null)
			{
				ConfigFile projectConfigFile = _workspace.ProjectConfigFile;
				if (projectConfigFile != null)
				{
					Dictionary<Guid, ConfigObject> projectBuildStepObjects = GetProjectBuildStepObjects(projectConfigFile);
					List<BuildStep> userSteps = GetUserBuildSteps(projectBuildStepObjects);

					Dictionary<string, ContextMenuStrip> nameToMenu = new Dictionary<string, ContextMenuStrip>();
					foreach (BuildStep step in userSteps)
					{
						if (!String.IsNullOrEmpty(step.StatusPanelLink) && CanRunStep(step))
						{
							int baseMenuIdx = step.StatusPanelLink.IndexOf('|', StringComparison.Ordinal);
							if (baseMenuIdx == -1)
							{
								_customStatusPanelLinks.Add((step.StatusPanelLink, (p, r) => RunCustomTool(step, userSteps)));
							}
							else
							{
								string menuName = step.StatusPanelLink.Substring(0, baseMenuIdx);
								string itemName = step.StatusPanelLink.Substring(baseMenuIdx + 1).Replace("&", "&&", StringComparison.Ordinal);

#pragma warning disable CA2000 // warning CA2000: Use recommended dispose pattern to ensure that object created by 'new ToolStripMenuItem(itemName)' is disposed on all paths. If possible, wrap the creation within a 'using' statement or a 'using' declaration. Otherwise, use a try-finally pattern, with a dedicated local variable declared before the try region and an unconditional Dispose invocation on non-null value in the 'finally' region, say 'x?.Dispose()'. If the object is explicitly disposed within the try region or the dispose ownership is transfered to another object or method, assign 'null' to the local variable just after such an operation to prevent double dispose in 'finally'.
								ToolStripMenuItem newMenuItem = new ToolStripMenuItem(itemName);
								newMenuItem.Click += new EventHandler((sender, e) => { RunCustomTool(step, userSteps); });

								if (menuName == "More...")
								{
									MoreToolsContextMenu.Items.Insert(moreToolsItemCount++, newMenuItem);
								}
								else
								{
									ContextMenuStrip? menu;
									if (!nameToMenu.TryGetValue(menuName, out menu))
									{
										menu = new ContextMenuStrip();
										nameToMenu.Add(menuName, menu);
										_customStatusPanelLinks.Add(($"{menuName} \u25BE", (p, r) => ShowToolContextMenu(r, menu)));
										_customStatusPanelMenus.Add(menu);
										components.Add(menu);
									}
									menu.Items.Add(newMenuItem);
								}
#pragma warning restore CA2000
							}
						}

						_hasBuildSteps |= step.NormalSync;
					}
				}
			}

			MoreActionsContextMenu_CustomToolSeparator.Visible = (moreToolsItemCount > 0);
		}

		private bool CanRunStep(BuildStep step)
		{
			if (step.ToolId != Guid.Empty)
			{
				string? toolName = _owner.ToolUpdateMonitor.GetToolName(step.ToolId);
				if (toolName == null)
				{
					return false;
				}
				if (_owner.ToolUpdateMonitor.GetToolPath(toolName) == null)
				{
					return false;
				}
			}
			return true;
		}

		private void RunCustomTool(BuildStep step, List<BuildStep> allSteps)
		{
			if (_workspace != null)
			{
				if (_workspace.IsBusy())
				{
					MessageBox.Show("Please retry after the current sync has finished.", "Sync in Progress");
				}
				else
				{
					HashSet<Guid> stepSet = new HashSet<Guid> { step.UniqueId };

					ConfigFile projectConfigFile = _workspace.ProjectConfigFile;
					if (projectConfigFile != null && step.Requires.Count > 0)
					{
						Stack<Guid> stack = new Stack<Guid>(stepSet);
						while (stack.Count > 0)
						{
							Guid id = stack.Pop();
							BuildStep? nextStep = allSteps.FirstOrDefault(x => x.UniqueId == id);
							if (nextStep != null)
							{
								foreach (Guid requiresId in nextStep.Requires)
								{
									if (stepSet.Add(requiresId))
									{
										stack.Push(requiresId);
									}
								}
							}
						}
					}

					WorkspaceUpdateContext context = new WorkspaceUpdateContext(_workspace.CurrentChangeNumber, WorkspaceUpdateOptions.Build, GetEditorBuildConfig(), null, _projectSettings.BuildSteps, stepSet);

					if (step.ToolId != Guid.Empty)
					{
						string? toolName = _owner.ToolUpdateMonitor.GetToolName(step.ToolId);
						if (toolName != null)
						{
							DirectoryReference? toolPath = _owner.ToolUpdateMonitor.GetToolPath(toolName);
							if (toolPath != null)
							{
								context.AdditionalVariables["ToolDir"] = toolPath.FullName;
							}
						}
					}

					StartWorkspaceUpdate(context, null);
				}
			}
		}

		private void OptionsContextMenu_EditBuildSteps_Click(object sender, EventArgs e)
		{
			ConfigFile projectConfigFile = _workspace.ProjectConfigFile;
			if (_workspace != null && projectConfigFile != null)
			{
				// Find all the target names for this project
				List<string> targetNames = new List<string>();
				if (SelectedFileName != null && SelectedFileName.HasExtension(".uproject"))
				{
					DirectoryInfo sourceDirectory = DirectoryReference.Combine(SelectedFileName.Directory, "Source").ToDirectoryInfo();
					if (sourceDirectory.Exists)
					{
						foreach (FileInfo targetFile in sourceDirectory.EnumerateFiles("*.target.cs", SearchOption.TopDirectoryOnly))
						{
							targetNames.Add(targetFile.Name.Substring(0, targetFile.Name.IndexOf('.', StringComparison.Ordinal)));
						}
					}
				}

				// Get all the task objects
				Dictionary<Guid, ConfigObject> projectBuildStepObjects = GetProjectBuildStepObjects(projectConfigFile);
				List<BuildStep> userSteps = GetUserBuildSteps(projectBuildStepObjects);

				// Show the dialog
				Dictionary<string, string> variables = _workspace.GetVariables(GetEditorBuildConfig());
				using ModifyBuildStepsWindow editStepsWindow = new ModifyBuildStepsWindow(targetNames, userSteps, new HashSet<Guid>(projectBuildStepObjects.Keys), BranchDirectoryName, variables);
				editStepsWindow.ShowDialog();

				// Update the user settings
				_projectSettings.BuildSteps.Clear();
				foreach (BuildStep step in userSteps)
				{
					if (step.IsValid())
					{
						ConfigObject? defaultObject;
						projectBuildStepObjects.TryGetValue(step.UniqueId, out defaultObject);

						ConfigObject? userConfigObject = step.ToConfigObject(defaultObject);
						if (userConfigObject != null && userConfigObject.Pairs.Any(x => x.Key != "UniqueId"))
						{
							_projectSettings.BuildSteps.Add(userConfigObject);
						}
					}
				}

				// Save the settings
				_projectSettings.Save(_logger);

				// Update the custom tools menu, because we might have changed it
				UpdateBuildSteps();
				UpdateStatusPanel();
				UpdateSyncActionCheckboxes();
			}
		}

		private static void AddOrUpdateBuildStep(Dictionary<Guid, ConfigObject> steps, ConfigObject obj)
		{
			Guid uniqueId;
			if (Guid.TryParse(obj.GetValue(BuildStep.UniqueIdKey, ""), out uniqueId))
			{
				// Add or apply Object to the list of steps in Steps. Do not modify Object; make a copy first.
				ConfigObject newObject = new ConfigObject(obj);

				ConfigObject? defaultObject;
				if (steps.TryGetValue(uniqueId, out defaultObject))
				{
					newObject.SetDefaults(defaultObject);
				}

				steps[uniqueId] = newObject;
			}
		}

		private bool ShouldSyncPrecompiledEditor => _settings.Archives.Any(x => x.Enabled && x.Type == IArchiveChannel.EditorArchiveType) && GetArchiveChannels().Any(x => x.Type == "Editor");

		public BuildConfig GetEditorBuildConfig()
		{
			return ShouldSyncPrecompiledEditor ? BuildConfig.Development : _settings.CompiledEditorBuildConfig;
		}

		private Dictionary<Guid, ConfigObject> GetProjectBuildStepObjects(ConfigFile projectConfigFile)
		{
			FileReference editorTargetFile = ConfigUtils.GetEditorTargetFile(ProjectInfo, projectConfigFile);
			string editorTargetName = editorTargetFile.GetFileNameWithoutAnyExtensions();

			Dictionary<Guid, ConfigObject> projectBuildSteps = ConfigUtils.GetDefaultBuildStepObjects(ProjectInfo, editorTargetName, _settings.CompiledEditorBuildConfig, projectConfigFile, ShouldSyncPrecompiledEditor);
			foreach (string line in projectConfigFile.GetValues("Build.Step", Array.Empty<string>()))
			{
				AddOrUpdateBuildStep(projectBuildSteps, new ConfigObject(line));
			}
			return projectBuildSteps;
		}

		private List<BuildStep> GetUserBuildSteps(Dictionary<Guid, ConfigObject> projectBuildStepObjects)
		{
			// Read all the user-defined build tasks and modifications to the default list
			Dictionary<Guid, ConfigObject> userBuildStepObjects = projectBuildStepObjects.ToDictionary(x => x.Key, y => new ConfigObject(y.Value));
			foreach (ConfigObject userBuildStep in _projectSettings.BuildSteps)
			{
				AddOrUpdateBuildStep(userBuildStepObjects, userBuildStep);
			}

			// Create the expanded task objects
			return userBuildStepObjects.Values.Select(x => new BuildStep(x)).OrderBy(x => (x.OrderIndex == -1) ? 10000 : x.OrderIndex).ToList();
		}

		private void OptionsContextMenu_SyncPrecompiledBinaries_Click(object sender, EventArgs e)
		{
			if (OptionsContextMenu_SyncPrecompiledBinaries.DropDownItems.Count == 0)
			{
				IArchiveChannel? editorArchive = GetArchiveChannels().FirstOrDefault(x => x.Type == IArchiveChannel.EditorArchiveType);
				if (editorArchive != null)
				{
					SetSelectedArchive(editorArchive, !OptionsContextMenu_SyncPrecompiledBinaries.Checked);
				}
			}
		}

		private void BuildList_SelectedIndexChanged(object sender, EventArgs e)
		{
			_pendingSelectedChangeNumber = -1;
		}

		private void OptionsContextMenu_Diagnostics_Click(object sender, EventArgs e)
		{
			StringBuilder diagnosticsText = new StringBuilder();
			diagnosticsText.AppendFormat("Application version: {0}\n", Program.GetVersionString());
			diagnosticsText.AppendFormat("Synced from: {0}\n", Program.SyncVersion ?? "(unknown)");
			diagnosticsText.AppendFormat("Selected file: {0}\n", (SelectedFileName == null) ? "(none)" : SelectedFileName.FullName);
			if (_workspace != null)
			{
				diagnosticsText.AppendFormat("P4 server: {0}\n", _perforceSettings.ServerAndPort ?? "(default)");
				diagnosticsText.AppendFormat("P4 user: {0}\n", _perforceSettings.UserName);
				diagnosticsText.AppendFormat("P4 workspace: {0}\n", _perforceSettings.ClientName);
			}
			diagnosticsText.AppendFormat("Perforce monitor: {0}\n", (_perforceMonitor == null) ? "(inactive)" : _perforceMonitor.LastStatusMessage);
			diagnosticsText.AppendFormat("Event monitor: {0}\n", (_eventMonitor == null) ? "(inactive)" : _eventMonitor.LastStatusMessage);
			diagnosticsText.AppendFormat("Issue monitor: {0}\n", (_issueMonitor == null) ? "(inactive)" : _issueMonitor.LastStatusMessage);

			using DiagnosticsWindow diagnostics = new DiagnosticsWindow(_appDataFolder, _workspaceDataFolder, diagnosticsText.ToString(), _settings.GetCachedFilePaths());
			diagnostics.ShowDialog(this);
		}

		private void OptionsContextMenu_SyncFilter_Click(object sender, EventArgs e)
		{
			using SyncFilter filter = new SyncFilter(GetSyncCategories(), _settings.Global.Filter, _workspaceSettings.Filter, _perforceMonitor.LatestPerforceConfigSection());
			if (filter.ShowDialog() == DialogResult.OK)
			{
				_settings.Global.Filter = filter.GlobalFilter;
				_settings.Save(_logger);

				_workspaceSettings.Filter = filter.WorkspaceFilter;
				_workspaceSettings.Save(_logger);
			}
		}

		/// <summary>
		/// Used to load CustomLatestChangeTypes config data into the windows form.
		/// </summary>
		private void RefreshSyncMenuItems()
		{
			SyncContextMenu.Items.Clear();
			SyncContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
				SyncContexMenu_EnterChangelist,
				toolStripSeparator8
			});

			SyncContexMenu_EnterChangelist.Image = s_hackToolStripMenuGutter;

			foreach (LatestChangeType changeType in GetCustomLatestChangeTypes())
			{
				System.Windows.Forms.ToolStripMenuItem menuItem = new System.Windows.Forms.ToolStripMenuItem();
				menuItem.Name = changeType.Name;
				menuItem.Text = changeType.Description;
				menuItem.Size = new System.Drawing.Size(189, 22);
				menuItem.Click += (sender, e) => SyncContextMenu_LatestChangeType_Click(changeType.Name);

				SyncContextMenu.Items.Add(menuItem);
			}
		}

		private void ShowSyncMenu(Rectangle bounds)
		{
			RefreshSyncMenuItems();

			string? nameOfLatestChangeType = null;
			foreach (LatestChangeType changeType in GetCustomLatestChangeTypes())
			{
				if (changeType.Name == _settings.SyncTypeId)
				{
					nameOfLatestChangeType = changeType.Name;
					break;
				}
			}

			if (nameOfLatestChangeType != null)
			{
				foreach (int value in Enumerable.Range(0, SyncContextMenu.Items.Count))
				{
					System.Windows.Forms.ToolStripMenuItem? item = SyncContextMenu.Items[value] as System.Windows.Forms.ToolStripMenuItem;
					if (item != null)
					{
						if (item.Name == nameOfLatestChangeType)
						{
							item.Checked = true;
						}
						else
						{
							item.Checked = false;
						}
					}
				}
			}

			SyncContextMenu.Show(StatusPanel, new Point(bounds.Left, bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void SyncContextMenu_LatestChangeType_Click(string syncTypeId)
		{
			_settings.SyncTypeId = syncTypeId;
			_settings.Save(_logger);
		}

		private void SyncContextMenu_EnterChangelist_Click(object sender, EventArgs e)
		{
			if (!WaitForProgramsToFinish())
			{
				return;
			}

			using ChangelistWindow childWindow = new ChangelistWindow((_workspace == null) ? -1 : _workspace.CurrentChangeNumber);
			if (childWindow.ShowDialog() == DialogResult.OK)
			{
				StartSync(childWindow.ChangeNumber);
			}
		}

		private void BuildList_KeyDown(object sender, KeyEventArgs args)
		{
			if (args.KeyCode == Keys.F5 || (args.Control && args.KeyCode == Keys.R))
			{
				_perforceMonitor?.Refresh();
			}

			if (args.Control && args.KeyCode == Keys.C && BuildList.SelectedItems.Count > 0)
			{
				int selectedChange = ((ChangesRecord)BuildList.SelectedItems[0].Tag).Number;
				Clipboard.SetText(String.Format("{0}", selectedChange));
			}
		}

		private void OptionsContextMenu_ApplicationSettings_Click(object sender, EventArgs e)
		{
			_owner.ModifyApplicationSettings();
		}

		private void OptionsContextMenu_TabNames_Stream_Click(object sender, EventArgs e)
		{
			_owner.SetTabNames(TabLabels.Stream);
		}

		private void OptionsContextMenu_TabNames_WorkspaceName_Click(object sender, EventArgs e)
		{
			_owner.SetTabNames(TabLabels.WorkspaceName);
		}

		private void OptionsContextMenu_TabNames_WorkspaceRoot_Click(object sender, EventArgs e)
		{
			_owner.SetTabNames(TabLabels.WorkspaceRoot);
		}

		private void OptionsContextMenu_TabNames_ProjectFile_Click(object sender, EventArgs e)
		{
			_owner.SetTabNames(TabLabels.ProjectFile);
		}

		private void SelectRecentProject(Rectangle bounds)
		{
			while (RecentMenu.Items[2] != RecentMenu_Separator)
			{
				RecentMenu.Items.RemoveAt(2);
			}

#pragma warning disable CA2000 //  warning CA2000: Call System.IDisposable.Dispose on object created by 'new ToolStripMenuItem(recentProject.ToString(), null, new EventHandler((o, e) => _owner.RequestProjectChange(this, recentProject, true)))' before all references to it are out of scope
			foreach (UserSelectedProjectSettings recentProject in _settings.RecentProjects)
			{
				ToolStripMenuItem item = new ToolStripMenuItem(recentProject.ToString(), null, new EventHandler((o, e) => _owner.RequestProjectChange(this, recentProject, true)));
				RecentMenu.Items.Insert(RecentMenu.Items.Count - 2, item);
			}
#pragma warning restore CA2000

			RecentMenu_Separator.Visible = (_settings.RecentProjects.Count > 0);
			RecentMenu.Show(StatusPanel, new Point(bounds.Left, bounds.Bottom), ToolStripDropDownDirection.BelowRight);
		}

		private void RecentMenu_Browse_Click(object sender, EventArgs e)
		{
			_owner.EditSelectedProject(this);
		}

		private void RecentMenu_ClearList_Click(object sender, EventArgs e)
		{
			_settings.RecentProjects.Clear();
			_settings.Save(_logger);
		}

		private void OptionsContextMenu_ShowChanges_ShowUnreviewed_Click(object sender, EventArgs e)
		{
			_settings.ShowUnreviewedChanges ^= true;
			_settings.Save(_logger);

			UpdateBuildList();
			ShrinkNumRequestedBuilds();
		}

		private void OptionsContextMenu_ShowChanges_ShowAutomated_Click(object sender, EventArgs e)
		{
			_settings.ShowAutomatedChanges ^= true;
			_settings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void BuildList_ColumnWidthChanging(object sender, ColumnWidthChangingEventArgs e)
		{
			UpdateMaxBuildBadgeChars();
		}

		private void BuildList_ColumnWidthChanged(object sender, ColumnWidthChangedEventArgs e)
		{
			if (_columnWidths != null && _minColumnWidths != null)
			{
				int newWidth = BuildList.Columns[e.ColumnIndex].Width;
				if (newWidth < _minColumnWidths[e.ColumnIndex])
				{
					newWidth = _minColumnWidths[e.ColumnIndex];
					BuildList.Columns[e.ColumnIndex].Width = newWidth;
				}
				_columnWidths[e.ColumnIndex] = newWidth;
			}
			UpdateMaxBuildBadgeChars();
		}

		private void BuildList_Resize(object sender, EventArgs e)
		{
			int prevBuildListWidth = _buildListWidth;

			_buildListWidth = BuildList.Width;

			if (prevBuildListWidth != 0 && _buildListWidth != prevBuildListWidth && _columnWidths != null)
			{
				float safeWidth = _columnWidths.Sum() + 50.0f;
				if (_buildListWidth < prevBuildListWidth)
				{
					if (_buildListWidth <= safeWidth)
					{
						ResizeColumns(_columnWidths.Sum() + (_buildListWidth - Math.Max(prevBuildListWidth, safeWidth)));
					}
				}
				else
				{
					if (_buildListWidth >= safeWidth)
					{
						ResizeColumns(_columnWidths.Sum() + (_buildListWidth - Math.Max(prevBuildListWidth, safeWidth)));
					}
				}
			}
		}

		void ResizeColumns(float nextTotalWidth)
		{
			float[] targetColumnWidths = GetTargetColumnWidths(nextTotalWidth);

			// Get the current total width of the columns, and the new space that we'll aim to fill
			float prevTotalWidth = _columnWidths.Sum();
			float totalDelta = Math.Abs(nextTotalWidth - prevTotalWidth);

			float totalColumnDelta = 0.0f;
			for (int idx = 0; idx < BuildList.Columns.Count; idx++)
			{
				totalColumnDelta += Math.Abs(targetColumnWidths[idx] - _columnWidths[idx]);
			}

			if (totalColumnDelta > 0.5f)
			{
				float[] nextColumnWidths = new float[BuildList.Columns.Count];
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					float maxColumnDelta = totalDelta * Math.Abs(targetColumnWidths[idx] - _columnWidths[idx]) / totalColumnDelta;
					nextColumnWidths[idx] = Math.Max(Math.Min(targetColumnWidths[idx], _columnWidths[idx] + maxColumnDelta), _columnWidths[idx] - maxColumnDelta);
				}

				// Update the control
				BuildList.BeginUpdate();
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					BuildList.Columns[idx].Width = (int)nextColumnWidths[idx];
				}
				_columnWidths = nextColumnWidths;
				BuildList.EndUpdate();
			}
		}

		float[] GetTargetColumnWidths(float nextTotalWidth)
		{
			// Array to store the output list
			float[] targetColumnWidths = new float[BuildList.Columns.Count];

			// Array of flags to store columns which are clamped into position. We try to respect proportional resizing as well as clamping to min/max sizes,
			// and remaining space can be distributed via non-clamped columns via another iteration.
			bool[] constrainedColumns = new bool[BuildList.Columns.Count];

			// Keep track of the remaining width that we have to distribute between columns. Does not include the required minimum size of each column.
			float remainingWidth = Math.Max(nextTotalWidth - _minColumnWidths.Sum(), 0.0f);

			// Keep track of the sum of the remaining column weights. Used to proportionally allocate remaining space.
			float remainingTotalWeight = _columnWeights.Sum();

			// Handle special cases for shrinking/expanding
			float prevTotalWidth = _columnWidths.Sum();
			if (nextTotalWidth < prevTotalWidth)
			{
				// If target size is less than current size, keep it at the current size
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					if (!constrainedColumns[idx])
					{
						float targetColumnWidth = _minColumnWidths[idx] + (remainingWidth * _columnWeights[idx] / remainingTotalWeight);
						if (targetColumnWidth > _columnWidths[idx])
						{
							targetColumnWidths[idx] = _columnWidths[idx];
							constrainedColumns[idx] = true;

							remainingWidth -= Math.Max(targetColumnWidths[idx] - _minColumnWidths[idx], 0.0f);
							remainingTotalWeight -= _columnWeights[idx];

							idx = -1;
							continue;
						}
					}
				}
			}
			else
			{
				// If target size is greater than desired size, clamp it to that
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					if (!constrainedColumns[idx])
					{
						float targetColumnWidth = _minColumnWidths[idx] + (remainingWidth * _columnWeights[idx] / remainingTotalWeight);
						if (targetColumnWidth > _desiredColumnWidths[idx])
						{
							// Don't allow this column to expand above the maximum desired size
							targetColumnWidths[idx] = _desiredColumnWidths[idx];
							constrainedColumns[idx] = true;

							remainingWidth -= Math.Max(targetColumnWidths[idx] - _minColumnWidths[idx], 0.0f);
							remainingTotalWeight -= _columnWeights[idx];

							idx = -1;
							continue;
						}
					}
				}

				// If current size is greater than target size, keep it that way
				for (int idx = 0; idx < BuildList.Columns.Count; idx++)
				{
					if (!constrainedColumns[idx])
					{
						float targetColumnWidth = _minColumnWidths[idx] + (remainingWidth * _columnWeights[idx] / remainingTotalWeight);
						if (targetColumnWidth < _columnWidths[idx])
						{
							targetColumnWidths[idx] = _columnWidths[idx];
							constrainedColumns[idx] = true;

							remainingWidth -= Math.Max(targetColumnWidths[idx] - _minColumnWidths[idx], 0.0f);
							remainingTotalWeight -= _columnWeights[idx];

							idx = -1;
							continue;
						}
					}
				}
			}

			// Allocate the remaining space equally
			for (int idx = 0; idx < BuildList.Columns.Count; idx++)
			{
				if (!constrainedColumns[idx])
				{
					targetColumnWidths[idx] = _minColumnWidths[idx] + (remainingWidth * _columnWeights[idx] / remainingTotalWeight);
				}
			}

			return targetColumnWidths;
		}

		private void BuidlListMultiContextMenu_Bisect_Click(object sender, EventArgs e)
		{
			EnableBisectMode();
		}

		private bool IsBisectModeEnabled()
		{
			return _workspace.State.BisectChanges.Count >= 2;
		}

		private void EnableBisectMode()
		{
			if (BuildList.SelectedItems.Count >= 2)
			{
				Dictionary<int, BisectState> changeNumberToBisectState = new Dictionary<int, BisectState>();
				foreach (ListViewItem? selectedItem in BuildList.SelectedItems)
				{
					if (selectedItem != null)
					{
						ChangesRecord change = (ChangesRecord)selectedItem.Tag;
						changeNumberToBisectState[change.Number] = BisectState.Include;
					}
				}

				changeNumberToBisectState[changeNumberToBisectState.Keys.Min()] = BisectState.Pass;
				changeNumberToBisectState[changeNumberToBisectState.Keys.Max()] = BisectState.Fail;

				_workspace.ModifyState(state =>
				{
					state.BisectChanges.Clear();
					state.BisectChanges.AddRange(changeNumberToBisectState.Select(x => new BisectEntry { Change = x.Key, State = x.Value }));
				});
			}
		}

		private void CancelBisectMode()
		{
			_workspace.ModifyState(x => x.BisectChanges.Clear());
		}

		private void SetBisectStateForSelection(BisectState state)
		{
			_workspace.ModifyState(x =>
			{
				foreach (ListViewItem? selectedItem in BuildList.SelectedItems)
				{
					ChangesRecord? change = selectedItem?.Tag as ChangesRecord;
					if (change != null)
					{
						x.SetBisectState(change.Number, state);
					}
				}
			});
		}

		private int GetBisectChangeNumber()
		{
			int passChangeNumber;
			int failChangeNumber;
			GetRemainingBisectRange(out passChangeNumber, out failChangeNumber);

			List<int> changeNumbers = new List<int>();
			foreach (BisectEntry entry in _workspace.State.BisectChanges)
			{
				if (entry.State == BisectState.Include && entry.Change > passChangeNumber && entry.Change < failChangeNumber)
				{
					changeNumbers.Add(entry.Change);
				}
			}

			changeNumbers.Sort();

			return (changeNumbers.Count > 0) ? changeNumbers[changeNumbers.Count / 2] : -1;
		}

		private void SyncBisectChange()
		{
			int bisectChange = GetBisectChangeNumber();
			if (bisectChange != -1)
			{
				_owner.ShowAndActivate();
				SelectChange(bisectChange);
				StartSync(bisectChange);
			}
		}

		private void BuildListContextMenu_Bisect_Pass_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Pass);
		}

		private void BuildListContextMenu_Bisect_Fail_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Fail);
		}

		private void BuildListContextMenu_Bisect_Exclude_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Exclude);
		}

		private void BuildListContextMenu_Bisect_Include_Click(object sender, EventArgs e)
		{
			SetBisectStateForSelection(BisectState.Include);
		}

		private void WorkspaceControl_VisibleChanged(object sender, EventArgs e)
		{
			if (_perforceMonitor != null && _perforceMonitor.IsActive != Visible)
			{
				_perforceMonitor.IsActive = Visible;
			}
		}

		private void FilterButton_Click(object sender, EventArgs e)
		{
			// Hack for toolstrip dpi scaling
			foreach (ToolStripMenuItem item in FilterContextMenu.Items.OfType<ToolStripMenuItem>())
			{
				item.Image = null;
			}

			FilterContextMenu_Default.Checked = !_settings.ShowAutomatedChanges && _projectSettings.FilterType == FilterType.None && _projectSettings.FilterBadges.Count == 0;

			FilterContextMenu_Type.Checked = _projectSettings.FilterType != FilterType.None;
			FilterContextMenu_Type_ShowAll.Checked = _projectSettings.FilterType == FilterType.None;
			FilterContextMenu_Type_Code.Checked = _projectSettings.FilterType == FilterType.Code;
			FilterContextMenu_Type_Content.Checked = _projectSettings.FilterType == FilterType.Content;

			FilterContextMenu_Badges.DropDownItems.Clear();
			FilterContextMenu_Badges.Checked = _projectSettings.FilterBadges.Count > 0;

			HashSet<string> badgeNames = new HashSet<string>(_projectSettings.FilterBadges, StringComparer.OrdinalIgnoreCase);
			badgeNames.ExceptWith(_badgeNameAndGroupPairs.Select(x => x.Key));

			List<KeyValuePair<string, string>> displayBadgeNameAndGroupPairs = new List<KeyValuePair<string, string>>(_badgeNameAndGroupPairs);
			displayBadgeNameAndGroupPairs.AddRange(badgeNames.Select(x => new KeyValuePair<string, string>(x, "User")));

			string? lastGroup = null;
			foreach (KeyValuePair<string, string> badgeNameAndGroupPair in displayBadgeNameAndGroupPairs)
			{
				if (lastGroup != badgeNameAndGroupPair.Value)
				{
					if (lastGroup != null)
					{
						FilterContextMenu_Badges.DropDownItems.Add(new ToolStripSeparator());
					}
					lastGroup = badgeNameAndGroupPair.Value;
				}

				ToolStripMenuItem item = new ToolStripMenuItem(badgeNameAndGroupPair.Key);
				item.Checked = _projectSettings.FilterBadges.Contains(badgeNameAndGroupPair.Key, StringComparer.OrdinalIgnoreCase);
				item.Click += (sender, args) => FilterContextMenu_Badge_Click(badgeNameAndGroupPair.Key);
				FilterContextMenu_Badges.DropDownItems.Add(item);
			}

			FilterContextMenu_Badges.Enabled = FilterContextMenu_Badges.DropDownItems.Count > 0;

			FilterContextMenu_ShowBuildMachineChanges.Checked = _settings.ShowAutomatedChanges;

			// Set checks if any robomerge filters are applied
			FilterContextMenu_Robomerge.Checked = _settings.ShowRobomerge != UserSettings.RobomergeShowChangesOption.All || _settings.AnnotateRobmergeChanges;
			FilterContextMenu_Robomerge_ShowAll.Checked = _settings.ShowRobomerge == UserSettings.RobomergeShowChangesOption.All;
			FilterContextMenu_Robomerge_ShowBadged.Checked = _settings.ShowRobomerge == UserSettings.RobomergeShowChangesOption.Badged;
			FilterContextMenu_Robomerge_Annotate.Checked = _settings.AnnotateRobmergeChanges;

			// Set checks if an author filter string is set
			FilterContextMenu_Author.Checked = !String.IsNullOrEmpty(_authorFilterText) && _authorFilterText != AuthorFilterPlaceholderText;

			// Hack for toolstrip dpi scaling
			foreach (ToolStripMenuItem item in FilterContextMenu.Items.OfType<ToolStripMenuItem>())
			{
				if (!item.Checked)
				{
					item.Image = s_hackToolStripMenuGutter;
					break;
				}
			}

			FilterContextMenu.Show(FilterButton, new Point(0, FilterButton.Height));
		}

		private void FilterContextMenu_Badge_Click(string badgeName)
		{
			if (_projectSettings.FilterBadges.Contains(badgeName))
			{
				_projectSettings.FilterBadges.Remove(badgeName);
			}
			else
			{
				_projectSettings.FilterBadges.Add(badgeName);
			}
			_settings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Default_Click(object sender, EventArgs e)
		{
			_projectSettings.FilterBadges.Clear();
			_projectSettings.FilterType = FilterType.None;
			_projectSettings.Save(_logger);

			_settings.ShowAutomatedChanges = false;
			_settings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_ShowBuildMachineChanges_Click(object sender, EventArgs e)
		{
			_settings.ShowAutomatedChanges ^= true;
			_settings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Robomerge_ShowAll_Click(object sender, EventArgs e)
		{
			_settings.ShowRobomerge = UserSettings.RobomergeShowChangesOption.All;
			_settings.Save(_logger);
			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Robomerge_ShowBadged_Click(object sender, EventArgs e)
		{
			_settings.ShowRobomerge = UserSettings.RobomergeShowChangesOption.Badged;
			_settings.Save(_logger);
			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Robomerge_ShowNone_Click(object sender, EventArgs e)
		{
			_settings.ShowRobomerge = UserSettings.RobomergeShowChangesOption.None;
			_settings.Save(_logger);
			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Robomerge_Annotate_Click(object sender, EventArgs e)
		{
			_settings.AnnotateRobmergeChanges ^= true;
			_settings.Save(_logger);
			BuildList.Items.Clear();    // need to reload to update user names

			UpdateBuildListFilter();
		}

		/// <summary>
		/// Handler for the edit box of the author filtering taking focus. This clears any
		/// placeholder text and sets the foreground color to black so it shows as active
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void FilterContextMenu_Author_Name_GotFocus(object sender, EventArgs e)
		{
			if (FilterContextMenu_Author_Name.Text == AuthorFilterPlaceholderText)
			{
				FilterContextMenu_Author_Name.Text = "";
				FilterContextMenu_Author_Name.TextBox.ForeColor = Color.Black;
			}
		}

		/// <summary>
		/// Handler for the edit box of the author filter losing focus. If the filter box
		/// is empty we restore the placeholder text and state
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void FilterContextMenu_Author_Name_LostFocus(object sender, EventArgs e)
		{
			if (String.IsNullOrEmpty(FilterContextMenu_Author_Name.Text))
			{
				FilterContextMenu_Author_Name.Text = AuthorFilterPlaceholderText;
				FilterContextMenu_Author_Name.TextBox.ForeColor = Color.DarkGray;
			}
		}

		/// <summary>
		/// Handler for the author name changing. We save off the text and update the 
		/// filter string
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void FilterContextMenu_Author_Name_Changed(object sender, EventArgs e)
		{
			_authorFilterText = FilterContextMenu_Author_Name.Text;
			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Type_ShowAll_Click(object sender, EventArgs e)
		{
			_projectSettings.FilterType = FilterType.None;
			_projectSettings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Type_Code_Click(object sender, EventArgs e)
		{
			_projectSettings.FilterType = FilterType.Code;
			_projectSettings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void FilterContextMenu_Type_Content_Click(object sender, EventArgs e)
		{
			_projectSettings.FilterType = FilterType.Content;
			_projectSettings.Save(_logger);

			UpdateBuildListFilter();
		}

		private void UpdateBuildListFilter()
		{
			UpdateBuildList();
			ShrinkNumRequestedBuilds();
		}

		public Color? TintColor
		{
			get;
			private set;
		}

		private void EditorConfigWatcher_Changed(object sender, FileSystemEventArgs e)
		{
			UpdateTintColor();
		}

		private void EditorConfigWatcher_Renamed(object sender, RenamedEventArgs e)
		{
			UpdateTintColor();
		}

		private Color? GetTintColor()
		{
			try
			{
				return GetNewTintColor() ?? GetLegacyTintColor();
			}
			catch
			{
				return null;
			}
		}

		private Color? GetNewTintColor()
		{
			FileReference newConfigFile = FileReference.Combine(SelectedFileName.Directory, "Saved", "Config", "WindowsEditor", "EditorPerProjectUserSettings.ini");
			if (!FileReference.Exists(newConfigFile))
			{
				return null;
			}

			ConfigFile config = new ConfigFile();
			config.Load(newConfigFile);

			ConfigSection? section = config.FindSection("/Script/EditorStyle.EditorStyleSettings");
			if (section == null)
			{
				return null;
			}

			string? enableColor = section.GetValue("bEnableEditorWindowBackgroundColor", null);
			if (enableColor == null || !String.Equals(enableColor, "True", StringComparison.OrdinalIgnoreCase))
			{
				return null;
			}

			string? backgroundColor = section.GetValue("EditorWindowBackgroundColor", null);
			if (backgroundColor == null)
			{
				return null;
			}

			ConfigObject overrideObject = new ConfigObject(backgroundColor);

			float r, g, b;
			if (!Single.TryParse(overrideObject.GetValue("R", ""), out r) || !Single.TryParse(overrideObject.GetValue("G", ""), out g) || !Single.TryParse(overrideObject.GetValue("B", ""), out b))
			{
				return null;
			}

			return Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));
		}

		private Color? GetLegacyTintColor()
		{
			FileReference fileName = FileReference.Combine(SelectedFileName.Directory, "Saved", "Config", "Windows", "EditorPerProjectUserSettings.ini");
			if (!FileReference.Exists(fileName))
			{
				return null;
			}

			ConfigFile config = new ConfigFile();
			config.Load(fileName);

			ConfigSection? section = config.FindSection("/Script/EditorStyle.EditorStyleSettings");
			if (section == null)
			{
				return null;
			}

			string? overrideValue = section.GetValue("EditorMainWindowBackgroundOverride", null);
			if (overrideValue == null)
			{
				return null;
			}

			ConfigObject overrideObject = new ConfigObject(overrideValue);

			string? tintColorValue = overrideObject.GetValue("TintColor");
			if (tintColorValue == null)
			{
				return null;
			}

			ConfigObject tintColorObject = new ConfigObject(tintColorValue);
			if (tintColorObject.GetValue("ColorUseRule", "") != "UseColor_Specified")
			{
				return null;
			}

			string? specifiedColorValue = tintColorObject.GetValue("SpecifiedColor");
			if (specifiedColorValue == null)
			{
				return null;
			}

			ConfigObject specifiedColorObject = new ConfigObject(specifiedColorValue);

			float r, g, b;
			if (!Single.TryParse(specifiedColorObject.GetValue("R", ""), out r) || !Single.TryParse(specifiedColorObject.GetValue("G", ""), out g) || !Single.TryParse(specifiedColorObject.GetValue("B", ""), out b))
			{
				return null;
			}

			return Color.FromArgb((int)(255.0f * r), (int)(255.0f * g), (int)(255.0f * b));
		}

		private void UpdateTintColor()
		{
			Color? newTintColor = GetTintColor();
			if (TintColor != newTintColor)
			{
				TintColor = newTintColor;
				_owner.UpdateTintColors();
			}
		}
	}

	public class UncontrolledChangelist
	{
		public string Guid { get; set; } = String.Empty;
		public string Name { get; set; } = String.Empty;
		public List<string> Files { get; } = new List<string>();
	}

	public class UncontrolledChangelistPersistency
	{
		public int Version { get; set; }
		public List<UncontrolledChangelist> Changelists { get; } = new List<UncontrolledChangelist>();
	}

	interface IWorkspaceControlOwner
	{
		ToolUpdateMonitor ToolUpdateMonitor { get; }

		void EditSelectedProject(WorkspaceControl workspace);
		void RequestProjectChange(WorkspaceControl workspace, UserSelectedProjectSettings project, bool modal);
		void ShowAndActivate();
		void StreamChanged(WorkspaceControl workspace);
		void SetTabNames(TabLabels tabNames);
		void SetupScheduledSync();
		void UpdateProgress();
		void ModifyApplicationSettings();
		void UpdateAlertWindows();
		void UpdateTintColors();

		IssueMonitor CreateIssueMonitor(string? apiUrl, string userName);
		void ReleaseIssueMonitor(IssueMonitor issueMonitor);
	}

	delegate void WorkspaceStartupCallback(WorkspaceControl workspace, bool cancel);
	delegate void WorkspaceUpdateCallback(WorkspaceUpdateResult result);
}
