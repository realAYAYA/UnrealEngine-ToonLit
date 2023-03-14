// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using EpicGames.OIDC;
using UnrealGameSync.Forms;

#nullable enable

namespace UnrealGameSync
{
	class ProgramApplicationContext : ApplicationContext
	{
		SynchronizationContext _mainThreadSynchronizationContext;

		IPerforceSettings _defaultPerforceSettings;
		UpdateMonitor _updateMonitor;
		string? _apiUrl;
		DirectoryReference _dataFolder;
		DirectoryReference _cacheFolder;
		bool _restoreState;
		string? _updateSpawn;
		bool _preview;
		bool _isClosing;
		string? _uri;

		IServiceProvider _serviceProvider;
		ILogger _logger;
		UserSettings _settings;
		ActivationListener _activationListener;

		Container _components = new Container();
		NotifyIcon _notifyIcon;
		ContextMenuStrip _notifyMenu;
		ToolStripMenuItem _notifyMenuOpenUnrealGameSync;
		ToolStripSeparator _notifyMenuOpenUnrealGameSyncSeparator;
		ToolStripMenuItem _notifyMenuSyncNow;
		ToolStripMenuItem _notifyMenuLaunchEditor;
		ToolStripSeparator _notifyMenuExitSeparator;
		ToolStripMenuItem _notifyMenuExit;

		CancellationTokenSource _startupCancellationSource = new CancellationTokenSource();
		Task _startupTask;
		ModalTaskWindow? _startupWindow;
		MainWindow? _mainWindowInstance;

		ITokenStore? _tokenStore;
		OidcTokenManager? _oidcTokenManager;

		public ProgramApplicationContext(IPerforceSettings defaultPerforceSettings, UpdateMonitor updateMonitor, string? apiUrl, DirectoryReference dataFolder, EventWaitHandle activateEvent, bool restoreState, string? updateSpawn, string? projectFileName, bool preview, IServiceProvider serviceProvider, string? uri)
		{
			this._defaultPerforceSettings = defaultPerforceSettings;
			this._updateMonitor = updateMonitor;
			this._apiUrl = apiUrl;
			this._dataFolder = dataFolder;
			this._cacheFolder = DirectoryReference.Combine(dataFolder, "Cache");
			this._restoreState = restoreState;
			this._updateSpawn = updateSpawn;
			this._preview = preview;
			this._serviceProvider = serviceProvider;
			this._logger = serviceProvider.GetRequiredService<ILogger<ProgramApplicationContext>>();
			this._uri = uri;

			// Create the directories
			DirectoryReference.CreateDirectory(dataFolder);
			DirectoryReference.CreateDirectory(_cacheFolder);

			// Make sure a synchronization context is set. We spawn a bunch of threads (eg. UpdateMonitor) at startup, and need to make sure we can post messages 
			// back to the main thread at any time.
			if(SynchronizationContext.Current == null)
			{
				SynchronizationContext.SetSynchronizationContext(new WindowsFormsSynchronizationContext());
			}

			// Capture the main thread's synchronization context for callbacks
			_mainThreadSynchronizationContext = SynchronizationContext.Current!;

			// Read the user's settings
			_settings = UserSettings.Create(dataFolder, serviceProvider.GetRequiredService<ILogger<UserSettings>>());
			if(!String.IsNullOrEmpty(projectFileName))
			{
				string fullProjectFileName = Path.GetFullPath(projectFileName);
				if(!_settings.OpenProjects.Any(x => x.LocalPath != null && String.Compare(x.LocalPath, fullProjectFileName, StringComparison.InvariantCultureIgnoreCase) == 0))
				{
					_settings.OpenProjects.Add(new UserSelectedProjectSettings(null, null, UserSelectedProjectType.Local, null, fullProjectFileName));
				}
			}

			// Update the settings to the latest version
			if(_settings.Version < UserSettingsVersion.Latest)
			{
				// Clear out the server settings for anything using the default server
				if(_settings.Version < UserSettingsVersion.DefaultServerSettings)
				{
					_logger.LogInformation("Clearing project settings for default server");
					for (int idx = 0; idx < _settings.OpenProjects.Count; idx++)
					{
						_settings.OpenProjects[idx] = UpgradeSelectedProjectSettings(_settings.OpenProjects[idx]);
					}
					for (int idx = 0; idx < _settings.RecentProjects.Count; idx++)
					{
						_settings.RecentProjects[idx] = UpgradeSelectedProjectSettings(_settings.RecentProjects[idx]);
					}
				}

				// Save the new settings
				_settings.Version = UserSettingsVersion.Latest;
				_settings.Save(_logger);
			}

			// Register the update listener
			updateMonitor.OnUpdateAvailable += OnUpdateAvailableCallback;

			// Create the activation listener
			_activationListener = new ActivationListener(activateEvent);
			_activationListener.Start();
			_activationListener.OnActivate += OnActivationListenerAsyncCallback;

			// Create the notification menu items
			_notifyMenuOpenUnrealGameSync = new ToolStripMenuItem();
			_notifyMenuOpenUnrealGameSync.Name = nameof(_notifyMenuOpenUnrealGameSync);
			_notifyMenuOpenUnrealGameSync.Size = new Size(196, 22);
			_notifyMenuOpenUnrealGameSync.Text = "Open UnrealGameSync";
			_notifyMenuOpenUnrealGameSync.Click += new EventHandler(NotifyMenu_OpenUnrealGameSync_Click);
			_notifyMenuOpenUnrealGameSync.Font = new Font(_notifyMenuOpenUnrealGameSync.Font, FontStyle.Bold);

			_notifyMenuOpenUnrealGameSyncSeparator = new ToolStripSeparator();
			_notifyMenuOpenUnrealGameSyncSeparator.Name = nameof(_notifyMenuOpenUnrealGameSyncSeparator);
			_notifyMenuOpenUnrealGameSyncSeparator.Size = new Size(193, 6);

			_notifyMenuSyncNow = new ToolStripMenuItem();
			_notifyMenuSyncNow.Name = nameof(_notifyMenuSyncNow);
			_notifyMenuSyncNow.Size = new Size(196, 22);
			_notifyMenuSyncNow.Text = "Sync Now";
			_notifyMenuSyncNow.Click += new EventHandler(NotifyMenu_SyncNow_Click);

			_notifyMenuLaunchEditor = new ToolStripMenuItem();
			_notifyMenuLaunchEditor.Name = nameof(_notifyMenuLaunchEditor);
			_notifyMenuLaunchEditor.Size = new Size(196, 22);
			_notifyMenuLaunchEditor.Text = "Launch Editor";
			_notifyMenuLaunchEditor.Click += new EventHandler(NotifyMenu_LaunchEditor_Click);

			_notifyMenuExitSeparator = new ToolStripSeparator();
			_notifyMenuExitSeparator.Name = nameof(_notifyMenuExitSeparator);
			_notifyMenuExitSeparator.Size = new Size(193, 6);

			_notifyMenuExit = new ToolStripMenuItem();
			_notifyMenuExit.Name = nameof(_notifyMenuExit);
			_notifyMenuExit.Size = new Size(196, 22);
			_notifyMenuExit.Text = "Exit";
			_notifyMenuExit.Click += new EventHandler(NotifyMenu_Exit_Click);

			// Create the notification menu
			_notifyMenu = new ContextMenuStrip(_components);
			_notifyMenu.Name = nameof(_notifyMenu);
			_notifyMenu.Size = new System.Drawing.Size(197, 104);
			_notifyMenu.SuspendLayout();
			_notifyMenu.Items.Add(_notifyMenuOpenUnrealGameSync);
			_notifyMenu.Items.Add(_notifyMenuOpenUnrealGameSyncSeparator);
			_notifyMenu.Items.Add(_notifyMenuSyncNow);
			_notifyMenu.Items.Add(_notifyMenuLaunchEditor);
			_notifyMenu.Items.Add(_notifyMenuExitSeparator);
			_notifyMenu.Items.Add(_notifyMenuExit);
			_notifyMenu.ResumeLayout(false);

			// Create the notification icon
			_notifyIcon = new NotifyIcon(_components);
			_notifyIcon.ContextMenuStrip = _notifyMenu;
			_notifyIcon.Icon = Properties.Resources.Icon;
			_notifyIcon.Text = "UnrealGameSync";
			_notifyIcon.Visible = true;
			_notifyIcon.DoubleClick += new EventHandler(NotifyIcon_DoubleClick);
			_notifyIcon.MouseDown += new MouseEventHandler(NotifyIcon_MouseDown);

			// Create the startup tasks
			List<(UserSelectedProjectSettings, ModalTask<OpenProjectInfo>)> startupTasks = new List<(UserSelectedProjectSettings, ModalTask<OpenProjectInfo>)>();
			foreach (UserSelectedProjectSettings projectSettings in _settings.OpenProjects)
			{
				ILogger<OpenProjectInfo> logger = serviceProvider.GetRequiredService<ILogger<OpenProjectInfo>>();
				Task<OpenProjectInfo> startupTask = Task.Run(() => OpenProjectInfo.CreateAsync(defaultPerforceSettings, projectSettings, _settings, logger, _startupCancellationSource.Token), _startupCancellationSource.Token);
				startupTasks.Add((projectSettings, new ModalTask<OpenProjectInfo>(startupTask)));
			}
			_startupTask = Task.Run(() => WaitForStartupTasks(startupTasks));

			_startupWindow = new ModalTaskWindow("Opening Projects", "Opening projects, please wait...", FormStartPosition.CenterScreen, _startupTask, _startupCancellationSource);
			_components.Add(_startupWindow);

			if(restoreState)
			{
				if(_settings.WindowVisible)
				{
					_startupWindow.Show();
				}
			}
			else
			{
				_startupWindow.Show();
				_startupWindow.Activate();
			}
			_startupWindow.FormClosed += (s, e) => OnStartupComplete(startupTasks);
		}

		static async Task WaitForStartupTasks(List<(UserSelectedProjectSettings, ModalTask<OpenProjectInfo>)> startupTasks)
		{
			foreach ((_, ModalTask<OpenProjectInfo> modalTask) in startupTasks)
			{
				try
				{
					await modalTask.Task;
				}
				catch (PerforceException)
				{
				}
				catch (OperationCanceledException)
				{
				}
				catch (UserErrorException)
				{
				}
				catch (Exception ex)
				{
					Program.CaptureException(ex);
				}
			}
		}

		private UserSelectedProjectSettings UpgradeSelectedProjectSettings(UserSelectedProjectSettings project)
		{
			if (project.ServerAndPort == null || String.Compare(project.ServerAndPort, _defaultPerforceSettings.ServerAndPort, StringComparison.OrdinalIgnoreCase) == 0)
			{
				if (project.UserName == null || String.Compare(project.UserName, _defaultPerforceSettings.UserName, StringComparison.OrdinalIgnoreCase) == 0)
				{
					project = new UserSelectedProjectSettings(null, null, project.Type, project.ClientPath, project.LocalPath);
				}
			}
			return project;
		}

		private void OnStartupComplete(List<(UserSelectedProjectSettings, ModalTask<OpenProjectInfo>)> startupTasks)
		{
			// Close the startup window
			bool visible = _startupWindow!.Visible;
			_startupWindow = null;

			// Clear out the cache folder
			Utility.ClearPrintCache(_cacheFolder);

			_tokenStore = TokenStoreFactory.CreateTokenStore();
			List<(DirectoryInfo, DirectoryInfo?)> configurationLocations = new List<(DirectoryInfo, DirectoryInfo?)>();
			List<string> allowedProviders = new List<string>();
			foreach ((UserSelectedProjectSettings, ModalTask<OpenProjectInfo>) StartupTask in startupTasks)
			{
				try
				{

					ConfigFile ConfigFile = StartupTask.Item2.Result.LatestProjectConfigFile;
					if (ConfigFile == null)
					{
						continue;
					}

					ConfigSection ProviderSection = ConfigFile.FindSection("OIDCProvider");
					if (ProviderSection == null)
					{
						continue;
					}

					string[] oidcAllowedProviders = ProviderSection.GetValues("OidcProviderAllowList", Array.Empty<string>());
					if (oidcAllowedProviders.Length == 0)
					{
						continue;
					}

					allowedProviders.AddRange(oidcAllowedProviders);
					
					ProjectInfo projectInfo = StartupTask.Item2.Result.ProjectInfo;
					configurationLocations.Add((projectInfo.EngineDir.ToDirectoryInfo(), projectInfo.ProjectDir?.ToDirectoryInfo()));
				}
				catch (Exception)
				{
					// ignore any projects that failed to load
				}
			}

			_oidcTokenManager = OidcTokenManager.CreateTokenManager(
				ProviderConfigurationFactory.MergeConfiguration(configurationLocations), 
				_tokenStore,
				allowedProviders
			);
			// Verify that none of the projects we are opening needs a OIDC login, if they do prompt for the login
			if (_oidcTokenManager?.HasUnfinishedLogin() ?? false)
			{
				OidcLoginWindow loginDialog = new OidcLoginWindow(_oidcTokenManager);
				loginDialog.ShowDialog();

				_tokenStore.Save();
			}

			// Get the application path
			string originalExe = Assembly.GetExecutingAssembly().Location;
			if (Path.GetExtension(originalExe).Equals(".dll", StringComparison.OrdinalIgnoreCase))
			{
				string newExecutable = Path.ChangeExtension(originalExe, ".exe");
				if (File.Exists(newExecutable))
				{
					originalExe = newExecutable;
				}
			}

			// Create the main window 
			_mainWindowInstance = new MainWindow(_updateMonitor, _apiUrl, _dataFolder, _cacheFolder, _restoreState, _updateSpawn ?? originalExe, _preview, startupTasks, _defaultPerforceSettings, _serviceProvider, _settings, _uri, _oidcTokenManager);
			if(visible)
			{
				_mainWindowInstance.Show();
				if(!_restoreState)
				{
					_mainWindowInstance.Activate();
				}
			}
			_mainWindowInstance.FormClosed += MainWindowInstance_FormClosed;
		}

		private void MainWindowInstance_FormClosed(object sender, FormClosedEventArgs e)
		{
			ExitThread();
		}

		private void OnActivationListenerCallback()
		{
			if(_mainWindowInstance != null && !_mainWindowInstance.IsDisposed)
			{
				_mainWindowInstance.ShowAndActivate();
			}
		}

		private void OnActivationListenerAsyncCallback()
		{
			_mainThreadSynchronizationContext.Post((o) => OnActivationListenerCallback(), null);
		}

		private void OnUpdateAvailable(UpdateType type)
		{
			if(_mainWindowInstance != null && !_isClosing)
			{
				if(type == UpdateType.UserInitiated || _mainWindowInstance.CanPerformUpdate())
				{
					_isClosing = true;
					_mainWindowInstance.ForceClose();
					_mainWindowInstance = null;
				}
			}
		}

		private void OnUpdateAvailableCallback(UpdateType type)
		{ 
			_mainThreadSynchronizationContext.Post((o) => OnUpdateAvailable(type), null);
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if(_updateMonitor != null)
			{
				_updateMonitor.Dispose();
				_updateMonitor = null!;
			}

			if(_activationListener != null)
			{
				_activationListener.OnActivate -= OnActivationListenerAsyncCallback;
				_activationListener.Stop();
				_activationListener.Dispose();
				_activationListener = null!;
			}

			if (_components != null)
			{
				_components.Dispose();
				_components = null!;
			}

			if (_notifyIcon != null)
			{
				_notifyIcon.Dispose();
				_notifyIcon = null!;
			}

			if(_mainWindowInstance != null)
			{
				_mainWindowInstance.ForceClose();
				_mainWindowInstance = null!;
			}

			if(_startupWindow != null)
			{
				_startupWindow.Close();
				_startupWindow = null;
			}

			if (_startupCancellationSource != null)
			{
				_startupCancellationSource.Dispose();
				_startupCancellationSource = null!;
			}
		}

		private void NotifyIcon_MouseDown(object sender, MouseEventArgs e)
		{
			// Have to set up this stuff here, because the menu is laid out before Opening() is called on it after mouse-up.
			bool canSyncNow = _mainWindowInstance != null && _mainWindowInstance.CanSyncNow();
			bool canLaunchEditor = _mainWindowInstance != null && _mainWindowInstance.CanLaunchEditor();
			_notifyMenuSyncNow.Visible = canSyncNow;
			_notifyMenuLaunchEditor.Visible = canLaunchEditor;
			_notifyMenuExitSeparator.Visible = canSyncNow || canLaunchEditor;

			// Show the startup window, if not already visible
			if(_startupWindow != null)
			{
				_startupWindow.Show();
			}
		}

		private void NotifyIcon_DoubleClick(object? sender, EventArgs e)
		{
			if(_mainWindowInstance != null)
			{
				_mainWindowInstance.ShowAndActivate();
			}
		}

		private void NotifyMenu_OpenUnrealGameSync_Click(object? sender, EventArgs e)
		{
			if(_startupWindow != null)
			{
				_startupWindow.ShowAndActivate();
			}
			if(_mainWindowInstance != null)
			{
				_mainWindowInstance.ShowAndActivate();
			}
		}

		private void NotifyMenu_SyncNow_Click(object? sender, EventArgs e)
		{
			if(_mainWindowInstance != null)
			{
				_mainWindowInstance.SyncLatestChange();
			}
		}

		private void NotifyMenu_LaunchEditor_Click(object? sender, EventArgs e)
		{
			if(_mainWindowInstance != null)
			{
				_mainWindowInstance.LaunchEditor();
			}
		}

		private void NotifyMenu_Exit_Click(object? sender, EventArgs e)
		{
			if (_mainWindowInstance != null && !_mainWindowInstance.ConfirmClose())
			{
				return;
			}

			if (_startupWindow != null)
			{
				_startupWindow.Close();
				_startupWindow = null;
			}

			if(_mainWindowInstance != null)
			{
				_mainWindowInstance.ForceClose();
				_mainWindowInstance = null;
			}

			ExitThread();
		}

		protected override void ExitThreadCore()
		{
			base.ExitThreadCore();

			if(_notifyIcon != null)
			{
				_notifyIcon.Visible = false;
			}
		}
	}
}
