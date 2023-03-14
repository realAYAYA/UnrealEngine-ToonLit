// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Perforce;
using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class ConnectWindow : Form
	{
		IPerforceSettings _defaultPerforceSettings;
		string? _serverAndPortOverride;
		string? _userNameOverride;
		IServiceProvider _serviceProvider;

		private ConnectWindow(IPerforceSettings defaultPerforceSettings, string? serverAndPortOverride, string? userNameOverride, IServiceProvider serviceProvider)
		{
			InitializeComponent();

			this._defaultPerforceSettings = defaultPerforceSettings;
			this._serviceProvider = serviceProvider;

			if(!String.IsNullOrWhiteSpace(serverAndPortOverride))
			{
				this._serverAndPortOverride = serverAndPortOverride.Trim();
			}
			if(!String.IsNullOrEmpty(userNameOverride))
			{
				this._userNameOverride = userNameOverride.Trim();
			}

			ServerAndPortTextBox.CueBanner = defaultPerforceSettings.ServerAndPort;
			ServerAndPortTextBox.Text = this._serverAndPortOverride ?? defaultPerforceSettings.ServerAndPort;
			UserNameTextBox.CueBanner = defaultPerforceSettings.UserName;
			UserNameTextBox.Text = this._userNameOverride ?? defaultPerforceSettings.UserName;
			UseDefaultConnectionSettings.Checked = this._serverAndPortOverride == null && this._userNameOverride == null;

			UpdateEnabledControls();
		}

		public static bool ShowModal(IWin32Window owner, IPerforceSettings defaultSettings, ref string? serverAndPortOverride, ref string? userNameOverride, IServiceProvider serviceProvider)
		{
			ConnectWindow connect = new ConnectWindow(defaultSettings, serverAndPortOverride, userNameOverride, serviceProvider);
			if(connect.ShowDialog(owner) == DialogResult.OK)
			{
				serverAndPortOverride = connect._serverAndPortOverride;
				userNameOverride = connect._userNameOverride;
				return true;
			}
			return false;
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			if(UseDefaultConnectionSettings.Checked)
			{
				_serverAndPortOverride = null;
				_userNameOverride = null;
			}
			else
			{
				_serverAndPortOverride = ServerAndPortTextBox.Text.Trim();
				if(_serverAndPortOverride.Length == 0)
				{
					_serverAndPortOverride = _defaultPerforceSettings.ServerAndPort;
				}

				_userNameOverride = UserNameTextBox.Text.Trim();
				if(_userNameOverride.Length == 0)
				{
					_userNameOverride = _defaultPerforceSettings.UserName;
				}
			}

			DialogResult = DialogResult.OK;
			Close();
		}

		private void UseCustomSettings_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void UpdateEnabledControls()
		{
			bool useDefaultSettings = UseDefaultConnectionSettings.Checked;

			ServerAndPortLabel.Enabled = !useDefaultSettings;
			ServerAndPortTextBox.Enabled = !useDefaultSettings;

			UserNameLabel.Enabled = !useDefaultSettings;
			UserNameTextBox.Enabled = !useDefaultSettings;
		}

		private void BrowseUserBtn_Click(object sender, EventArgs e)
		{
			string? newUserName;
			if(SelectUserWindow.ShowModal(this, new PerforceSettings(_defaultPerforceSettings) { UserName = UserNameTextBox.Text, ServerAndPort = ServerAndPortTextBox.Text }, _serviceProvider, out newUserName))
			{
				UserNameTextBox.Text = newUserName;
			}
		}
	}
}
