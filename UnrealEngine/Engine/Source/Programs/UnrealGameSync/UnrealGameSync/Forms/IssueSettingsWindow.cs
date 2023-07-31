// Copyright Epic Games, Inc. All Rights Reserved.

using Microsoft.Extensions.Logging;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class IssueSettingsWindow : Form
	{
		UserSettings _settings;
		ILogger _logger;

		public IssueSettingsWindow(UserSettings settings, string currentProject, ILogger logger)
		{
			this._settings = settings;
			this._logger = logger;

			InitializeComponent();

			if (settings.NotifyProjects.Count == 0)
			{
				NotifyProjectsCheckBox.Checked = false;
				NotifyProjectsTextBox.Text = currentProject;
			}
			else
			{
				NotifyProjectsCheckBox.Checked = true;
				NotifyProjectsTextBox.Text = String.Join(" ", settings.NotifyProjects);
			}

			if(settings.NotifyUnassignedMinutes < 0)
			{
				NotifyUnassignedCheckBox.Checked = false;
				NotifyUnassignedTextBox.Text = "5";
			}
			else
			{
				NotifyUnassignedCheckBox.Checked = true;
				NotifyUnassignedTextBox.Text = settings.NotifyUnassignedMinutes.ToString();
			}

			if (settings.NotifyUnacknowledgedMinutes < 0)
			{
				NotifyUnacknowledgedCheckBox.Checked = false;
				NotifyUnacknowledgedTextBox.Text = "5";
			}
			else
			{
				NotifyUnacknowledgedCheckBox.Checked = true;
				NotifyUnacknowledgedTextBox.Text = settings.NotifyUnacknowledgedMinutes.ToString();
			}

			if (settings.NotifyUnresolvedMinutes < 0)
			{
				NotifyUnresolvedCheckBox.Checked = false;
				NotifyUnresolvedTextBox.Text = "20";
			}
			else
			{
				NotifyUnresolvedCheckBox.Checked = true;
				NotifyUnresolvedTextBox.Text = settings.NotifyUnresolvedMinutes.ToString();
			}

			UpdateEnabledTextBoxes();
		}

		private void UpdateEnabledTextBoxes()
		{
			NotifyProjectsTextBox.Enabled = NotifyProjectsCheckBox.Checked;
			NotifyUnassignedTextBox.Enabled = NotifyUnassignedCheckBox.Checked;
			NotifyUnacknowledgedTextBox.Enabled = NotifyUnacknowledgedCheckBox.Checked;
			NotifyUnresolvedTextBox.Enabled = NotifyUnresolvedCheckBox.Checked;
		}

		private void NotifyProjectsCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledTextBoxes();
		}

		private void NotifyUnassignedCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledTextBoxes();
		}

		private void NotifyUnacknowledgedCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledTextBoxes();
		}

		private void NotifyUnresolvedCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledTextBoxes();
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			List<string> newNotifyProjects = new List<string>();
			if (NotifyProjectsCheckBox.Checked)
			{
				newNotifyProjects.AddRange(NotifyProjectsTextBox.Text.Split(new char[] { ' ' }, StringSplitOptions.RemoveEmptyEntries));
			}

			int newNotifyUnresolvedMinutes = -1;
			if(NotifyUnresolvedCheckBox.Checked)
			{
				ushort newNotifyUnresolvedMinutesValue;
				if(!ushort.TryParse(NotifyUnresolvedTextBox.Text, out newNotifyUnresolvedMinutesValue))
				{
					MessageBox.Show("Invalid time");
					return;
				}
				newNotifyUnresolvedMinutes = newNotifyUnresolvedMinutesValue;
			}

			int newNotifyUnacknowledgedMinutes = -1;
			if (NotifyUnacknowledgedCheckBox.Checked)
			{
				ushort newNotifyUnacknowledgedMinutesValue;
				if (!ushort.TryParse(NotifyUnacknowledgedTextBox.Text, out newNotifyUnacknowledgedMinutesValue))
				{
					MessageBox.Show("Invalid time");
					return;
				}
				newNotifyUnacknowledgedMinutes = newNotifyUnacknowledgedMinutesValue;
			}

			int newNotifyUnassignedMinutes = -1;
			if(NotifyUnassignedCheckBox.Checked)
			{
				ushort newNotifyUnassignedMinutesValue;
				if(!ushort.TryParse(NotifyUnassignedTextBox.Text, out newNotifyUnassignedMinutesValue))
				{
					MessageBox.Show("Invalid time");
					return;
				}
				newNotifyUnassignedMinutes = newNotifyUnassignedMinutesValue;
			}

			_settings.NotifyProjects = newNotifyProjects;
			_settings.NotifyUnresolvedMinutes = newNotifyUnresolvedMinutes;
			_settings.NotifyUnacknowledgedMinutes = newNotifyUnacknowledgedMinutes;
			_settings.NotifyUnassignedMinutes = newNotifyUnassignedMinutes;
			_settings.Save(_logger);

			DialogResult = DialogResult.OK;
			Close();
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
