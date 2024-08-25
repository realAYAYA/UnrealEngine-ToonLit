// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;
using EpicGames.Perforce;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Logging;

#nullable enable

namespace UnrealGameSync
{
	partial class OpenProjectWindow : Form
	{
		string? _serverAndPortOverride;
		string? _userNameOverride;
		OpenProjectInfo? _openProjectInfo;
		readonly IPerforceSettings _defaultSettings;
		readonly IServiceProvider _serviceProvider;
		readonly UserSettings _settings;
		readonly ILogger _logger;

		delegate bool DetectProjectSettingsEvent(OpenProjectInfo openProjectInfo, ILogger logger, [NotNullWhen(false)] out string? error);

#pragma warning disable CS0649 // Avoid unused private fields
		// Hook called once the project settings have been established. Can be overridden in a specific deployment to log telemetry or perform additional validation.
		static readonly DetectProjectSettingsEvent? s_onDetectProjectSettings;
#pragma warning restore CS0649 // Avoid unused private fields

		private OpenProjectWindow(UserSelectedProjectSettings? project, UserSettings settings, IPerforceSettings defaultSettings, IServiceProvider serviceProvider, ILogger logger)
		{
			InitializeComponent();
			Font = new System.Drawing.Font("Segoe UI", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));

			_settings = settings;
			_openProjectInfo = null;
			_defaultSettings = defaultSettings;
			_serviceProvider = serviceProvider;
			_logger = logger;

			if (project == null)
			{
				LocalFileRadioBtn.Checked = true;
			}
			else
			{
				if (!String.IsNullOrWhiteSpace(project.ServerAndPort))
				{
					_serverAndPortOverride = project.ServerAndPort;
				}
				if (!String.IsNullOrWhiteSpace(project.UserName))
				{
					_userNameOverride = project.UserName;
				}

				if (project.ClientPath != null && project.ClientPath.StartsWith("//", StringComparison.Ordinal))
				{
					int slashIdx = project.ClientPath.IndexOf('/', 2);
					if (slashIdx != -1)
					{
						WorkspaceNameTextBox.Text = project.ClientPath.Substring(2, slashIdx - 2);
						WorkspacePathTextBox.Text = project.ClientPath.Substring(slashIdx);
					}
				}

				if (project.LocalPath != null)
				{
					LocalFileTextBox.Text = project.LocalPath;
				}

				if (project.Type == UserSelectedProjectType.Client)
				{
					WorkspaceRadioBtn.Checked = true;
				}
				else
				{
					LocalFileRadioBtn.Checked = true;
				}
			}

			UpdateEnabledControls();
			UpdateServerLabel();
			UpdateWorkspacePathBrowseButton();
			UpdateP4ConfigCheckBox();
			UpdateOkButton();
		}

		private IPerforceSettings Perforce => Utility.OverridePerforceSettings(_defaultSettings, _serverAndPortOverride, _userNameOverride);

		public static OpenProjectInfo? ShowModal(IWin32Window owner, UserSelectedProjectSettings? project, UserSettings settings, IPerforceSettings defaultPerforceSettings, IServiceProvider serviceProvider, ILogger logger)
		{
			using OpenProjectWindow window = new OpenProjectWindow(project, settings, defaultPerforceSettings, serviceProvider, logger);
			if (window.ShowDialog(owner) == DialogResult.OK)
			{
				return window._openProjectInfo;
			}
			else
			{
				return null;
			}
		}

		private void UpdateEnabledControls()
		{
			Color workspaceTextColor = WorkspaceRadioBtn.Checked ? SystemColors.ControlText : SystemColors.GrayText;
			WorkspaceNameLabel.ForeColor = workspaceTextColor;
			WorkspaceNameTextBox.ForeColor = workspaceTextColor;
			WorkspaceNameNewBtn.ForeColor = workspaceTextColor;
			WorkspaceNameBrowseBtn.ForeColor = workspaceTextColor;
			WorkspacePathLabel.ForeColor = workspaceTextColor;
			WorkspacePathTextBox.ForeColor = workspaceTextColor;
			WorkspacePathBrowseBtn.ForeColor = workspaceTextColor;

			Color localFileTextColor = LocalFileRadioBtn.Checked ? SystemColors.ControlText : SystemColors.GrayText;
			LocalFileLabel.ForeColor = localFileTextColor;
			LocalFileTextBox.ForeColor = localFileTextColor;
			LocalFileBrowseBtn.ForeColor = localFileTextColor;

			UpdateWorkspacePathBrowseButton();
		}

		public static string GetServerLabelText(IPerforceSettings defaultSettings, string? serverAndPort, string? userName)
		{
			if (serverAndPort == null && userName == null)
			{
				return String.Format("Using default connection settings (user '{0}' on server '{1}').", defaultSettings.UserName, defaultSettings.ServerAndPort);
			}
			else
			{
				StringBuilder text = new StringBuilder("Connecting as ");
				if (userName == null)
				{
					text.Append("default user");
				}
				else
				{
					text.AppendFormat("user '{0}'", userName);
				}
				text.Append(" on ");
				if (serverAndPort == null)
				{
					text.Append("default server.");
				}
				else
				{
					text.AppendFormat("server '{0}'.", serverAndPort);
				}
				return text.ToString();
			}
		}

		private void UpdateServerLabel()
		{
			ServerLabel.Text = GetServerLabelText(_defaultSettings, _serverAndPortOverride, _userNameOverride);
		}

		private void UpdateWorkspacePathBrowseButton()
		{
			WorkspacePathBrowseBtn.Enabled = TryGetWorkspaceName(out _);
		}

		private void UpdateP4ConfigCheckBox()
		{
			bool isP4ConfigSet = !String.IsNullOrEmpty(PerforceEnvironment.Default.GetValue("P4CONFIG"));

			GenerateP4ConfigCheckbox.Visible = isP4ConfigSet;
			GenerateP4ConfigCheckbox.Checked = isP4ConfigSet;
		}

		private void UpdateOkButton()
		{
			OkBtn.Enabled = WorkspaceRadioBtn.Checked ? TryGetClientPath(out _) : TryGetLocalPath(out _);
		}

		private void WorkspaceNewBtn_Click(object sender, EventArgs e)
		{
			WorkspaceRadioBtn.Checked = true;

			string? workspaceName;
			if (NewWorkspaceWindow.ShowModal(this, Perforce, null, WorkspaceNameTextBox.Text, _serviceProvider, out workspaceName))
			{
				WorkspaceNameTextBox.Text = workspaceName;
				UpdateOkButton();
			}
		}

		private void WorkspaceBrowseBtn_Click(object sender, EventArgs e)
		{
			WorkspaceRadioBtn.Checked = true;

			string? workspaceName = WorkspaceNameTextBox.Text;
			if (SelectWorkspaceWindow.ShowModal(this, Perforce, workspaceName, _serviceProvider, out workspaceName))
			{
				WorkspaceNameTextBox.Text = workspaceName;
			}
		}

		private void WorkspacePathBrowseBtn_Click(object sender, EventArgs e)
		{
			WorkspaceRadioBtn.Checked = true;

			string? workspaceName;
			if (TryGetWorkspaceName(out workspaceName))
			{
				string? workspacePath = WorkspacePathTextBox.Text.Trim();
				if (SelectProjectFromWorkspaceWindow.ShowModal(this, Perforce, workspaceName, workspacePath, _serviceProvider, out workspacePath))
				{
					WorkspacePathTextBox.Text = workspacePath;
					UpdateOkButton();
				}
			}
		}

		private bool TryGetWorkspaceName([NotNullWhen(true)] out string? workspaceName)
		{
			string text = WorkspaceNameTextBox.Text.Trim();
			if (text.Length == 0)
			{
				workspaceName = null;
				return false;
			}

			workspaceName = text;
			return true;
		}

		private bool TryGetClientPath([NotNullWhen(true)] out string? clientPath)
		{
			string? workspaceName;
			if (!TryGetWorkspaceName(out workspaceName))
			{
				clientPath = null;
				return false;
			}

			string workspacePath = WorkspacePathTextBox.Text.Trim();
			if (workspacePath.Length == 0 || workspacePath[0] != '/')
			{
				clientPath = null;
				return false;
			}

			clientPath = String.Format("//{0}{1}", workspaceName, workspacePath);
			return true;
		}

		private bool TryGetLocalPath(out string? localPath)
		{
			string localFile = LocalFileTextBox.Text.Trim();
			if (localFile.Length == 0)
			{
				localPath = null;
				return false;
			}

			localPath = Path.GetFullPath(localFile);
			return true;
		}

		private bool TryGetSelectedProject([NotNullWhen(true)] out UserSelectedProjectSettings? project)
		{
			if (WorkspaceRadioBtn.Checked)
			{
				string? clientPath;
				if (TryGetClientPath(out clientPath))
				{
					project = new UserSelectedProjectSettings(_serverAndPortOverride, _userNameOverride, UserSelectedProjectType.Client, clientPath, null);
					return true;
				}
			}
			else
			{
				string? localPath;
				if (TryGetLocalPath(out localPath))
				{
					project = new UserSelectedProjectSettings(_serverAndPortOverride, _userNameOverride, UserSelectedProjectType.Local, null, localPath);
					return true;
				}
			}

			project = null;
			return false;
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			UserSelectedProjectSettings? selectedProject;
			if (TryGetSelectedProject(out selectedProject))
			{
				ILogger<OpenProjectInfo> logger = _serviceProvider.GetRequiredService<ILogger<OpenProjectInfo>>();

				PerforceSettings newPerforceSettings = Utility.OverridePerforceSettings(Perforce, selectedProject.ServerAndPort, selectedProject.UserName);

				ModalTask<OpenProjectInfo>? newOpenProjectInfo = PerforceModalTask.Execute(this, "Opening project", "Opening project, please wait...", newPerforceSettings, (x, y) => DetectSettingsAsync(x, selectedProject, _settings, GenerateP4ConfigCheckbox.Checked, logger, y), logger);
				if (newOpenProjectInfo != null && newOpenProjectInfo.Succeeded)
				{
					_openProjectInfo = newOpenProjectInfo.Result;
					DialogResult = DialogResult.OK;
					Close();
				}
			}
		}

		public static async Task<OpenProjectInfo> DetectSettingsAsync(IPerforceConnection perforce, UserSelectedProjectSettings selectedProject, UserSettings userSettings, bool GenerateP4Config, ILogger<OpenProjectInfo> logger, CancellationToken cancellationToken)
		{
			OpenProjectInfo settings = await OpenProjectInfo.CreateAsync(perforce, selectedProject, userSettings, GenerateP4Config, logger, cancellationToken);
			if (s_onDetectProjectSettings != null)
			{
				string? message;
				if (!s_onDetectProjectSettings(settings, logger, out message))
				{
					throw new UserErrorException(message);
				}
			}
			return settings;
		}

		private void ChangeLink_LinkClicked(object sender, LinkLabelLinkClickedEventArgs e)
		{
			if (ConnectWindow.ShowModal(this, _defaultSettings, ref _serverAndPortOverride, ref _userNameOverride, _serviceProvider))
			{
				UpdateServerLabel();
			}
		}

		private void LocalFileBrowseBtn_Click(object sender, EventArgs e)
		{
			LocalFileRadioBtn.Checked = true;

			using OpenFileDialog dialog = new OpenFileDialog();
			dialog.Filter = "Project files (*.uproject)|*.uproject|Project directory lists (*.uprojectdirs)|*.uprojectdirs|All supported files (*.uproject;*.uprojectdirs)|*.uproject;*.uprojectdirs|All files (*.*)|*.*";
			dialog.FilterIndex = _settings.FilterIndex;

			if (!String.IsNullOrEmpty(LocalFileTextBox.Text))
			{
				try
				{
					dialog.InitialDirectory = Path.GetDirectoryName(LocalFileTextBox.Text);
				}
				catch
				{
				}
			}

			if (dialog.ShowDialog(this) == DialogResult.OK)
			{
				string fullName = Path.GetFullPath(dialog.FileName);

				_settings.FilterIndex = dialog.FilterIndex;
				_settings.Save(_logger);

				LocalFileTextBox.Text = fullName;
				UpdateOkButton();
			}
		}

		private void WorkspaceNameTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
			UpdateWorkspacePathBrowseButton();
		}

		private void WorkspacePathTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
		}

		private void LocalFileTextBox_TextChanged(object sender, EventArgs e)
		{
			UpdateOkButton();
		}

		private void WorkspaceRadioBtn_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void LocalFileRadioBtn_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void LocalFileTextBox_Enter(object sender, EventArgs e)
		{
			LocalFileRadioBtn.Checked = true;
		}

		private void WorkspaceNameTextBox_Enter(object sender, EventArgs e)
		{
			WorkspaceRadioBtn.Checked = true;
		}

		private void WorkspacePathTextBox_Enter(object sender, EventArgs e)
		{
			WorkspaceRadioBtn.Checked = true;
		}
	}
}
