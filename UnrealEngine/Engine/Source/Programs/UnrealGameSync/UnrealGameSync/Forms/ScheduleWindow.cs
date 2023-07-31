// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class ScheduleWindow : Form
	{

		Dictionary<UserSelectedProjectSettings, List<LatestChangeType>> _projectToLatestChangeTypes;

		public ScheduleWindow(
			bool inEnabled,
			TimeSpan inTime,
			bool anyOpenProject,
			IEnumerable<UserSelectedProjectSettings> scheduledProjects,
			IEnumerable<UserSelectedProjectSettings> openProjects,
			Dictionary<UserSelectedProjectSettings, List<LatestChangeType>> inProjectToLatestChangeTypes)
		{
			InitializeComponent();

			EnableCheckBox.Checked = inEnabled;

			_projectToLatestChangeTypes = inProjectToLatestChangeTypes;

			DateTime currentTime = DateTime.Now;
			TimePicker.CustomFormat = CultureInfo.CurrentCulture.DateTimeFormat.ShortTimePattern;
			TimePicker.Value = new DateTime(currentTime.Year, currentTime.Month, currentTime.Day, inTime.Hours, inTime.Minutes, inTime.Seconds);

			ProjectListBox.Items.Add("Any open projects", anyOpenProject);

			Dictionary<string, UserSelectedProjectSettings> localFileToProject = new Dictionary<string, UserSelectedProjectSettings>(StringComparer.InvariantCultureIgnoreCase);
			AddProjects(scheduledProjects, localFileToProject);
			AddProjects(openProjects, localFileToProject);

			foreach(UserSelectedProjectSettings project in localFileToProject.Values.OrderBy(x => x.ToString()))
			{
				bool enabled = scheduledProjects.Any(x => x.LocalPath == project.LocalPath);
				ProjectListBox.Items.Add(project, enabled);
			}

			ProjectListBox.ItemCheck += ProjectListBox_ItemCheck;

			SyncTypeDropDown.Closed += SyncTypeDropDown_Closed;

			UpdateEnabledControls();
		}

		private void UpdateSyncTypeDropDownWithProject(UserSelectedProjectSettings? selectedProject)
		{
			SyncTypeDropDown.Items.Clear();

			if (selectedProject != null
				&& _projectToLatestChangeTypes.ContainsKey(selectedProject))
			{
				foreach (LatestChangeType changeType in _projectToLatestChangeTypes[selectedProject])
				{
					System.Windows.Forms.ToolStripMenuItem menuItem = new System.Windows.Forms.ToolStripMenuItem();
					menuItem.Name = changeType.Name;
					menuItem.Text = changeType.Description;
					menuItem.Size = new System.Drawing.Size(189, 22);
					menuItem.Click += (sender, e) => SyncTypeDropDown_Click(sender, e, changeType.Name);

					SyncTypeDropDown.Items.Add(menuItem);
				}
			}
		}

		private void ProjectListBox_ItemCheck(object sender, ItemCheckEventArgs e)
		{
			bool isAnyOpenProjectIndex = e.Index == 0;
			if (e.NewValue == CheckState.Checked && !isAnyOpenProjectIndex)
			{
				UpdateSyncTypeDropDownWithProject(ProjectListBox.Items[e.Index] as UserSelectedProjectSettings);

				SyncTypeDropDown.Show(
					ProjectListBox,
					ProjectListBox.GetItemRectangle(e.Index).Location,
					ToolStripDropDownDirection.BelowRight);
			}
		}

		private void SyncTypeDropDown_Closed(object sender, ToolStripDropDownClosedEventArgs e)
		{
			if (e.CloseReason != ToolStripDropDownCloseReason.ItemClicked)
			{
				ProjectListBox.SetItemChecked(ProjectListBox.SelectedIndex, false);
			}
		}

		private void SyncTypeDropDown_Click(object? sender, EventArgs e, string syncTypeId)
		{
			UserSelectedProjectSettings? projectSetting = ProjectListBox.Items[ProjectListBox.SelectedIndex] as UserSelectedProjectSettings;
			if (projectSetting != null)
			{
				projectSetting.ScheduledSyncTypeId = syncTypeId;
			}
			SyncTypeDropDown.Close();
		}

		private void AddProjects(IEnumerable<UserSelectedProjectSettings> projects, Dictionary<string, UserSelectedProjectSettings> localFileToProject)
		{
			foreach(UserSelectedProjectSettings project in projects)
			{
				if(project.LocalPath != null)
				{
					localFileToProject[project.LocalPath] = project;
				}
			}
		}

		public void CopySettings(out bool outEnabled, out TimeSpan outTime, out bool outAnyOpenProject, out List<UserSelectedProjectSettings> outScheduledProjects)
		{
			outEnabled = EnableCheckBox.Checked;
			outTime = TimePicker.Value.TimeOfDay;

			outAnyOpenProject = false;

			List<UserSelectedProjectSettings> scheduledProjects = new List<UserSelectedProjectSettings>();
			foreach(int index in ProjectListBox.CheckedIndices.OfType<int>())
			{
				if(index == 0)
				{
					outAnyOpenProject = true;
				}
				else
				{
					scheduledProjects.Add((UserSelectedProjectSettings)ProjectListBox.Items[index]);
				}
			}
			outScheduledProjects = scheduledProjects;
		}

		private void EnableCheckBox_CheckedChanged(object sender, EventArgs e)
		{
			UpdateEnabledControls();
		}

		private void UpdateEnabledControls()
		{
			TimePicker.Enabled = EnableCheckBox.Checked;
			ProjectListBox.Enabled = EnableCheckBox.Checked;
		}
	}
}
