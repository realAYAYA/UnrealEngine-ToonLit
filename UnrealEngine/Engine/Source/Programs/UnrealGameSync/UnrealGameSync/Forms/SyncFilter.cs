// Copyright Epic Games, Inc. All Rights Reserved.

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
	partial class SyncFilter : Form
	{
		Dictionary<Guid, WorkspaceSyncCategory> _uniqueIdToCategory;
		public FilterSettings GlobalFilter;
		public FilterSettings WorkspaceFilter;

		public SyncFilter(Dictionary<Guid, WorkspaceSyncCategory> inUniqueIdToCategory, FilterSettings inGlobalFilter, FilterSettings inWorkspaceFilter)
		{
			InitializeComponent();

			_uniqueIdToCategory = inUniqueIdToCategory;
			GlobalFilter = inGlobalFilter;
			WorkspaceFilter = inWorkspaceFilter;

			Dictionary<Guid, bool> syncCategories = WorkspaceSyncCategory.GetDefault(_uniqueIdToCategory.Values);

			WorkspaceSyncCategory.ApplyDelta(syncCategories, GlobalFilter.GetCategories());
			GlobalControl.SetView(GlobalFilter.View.ToArray());
			SetExcludedCategories(GlobalControl.CategoriesCheckList, _uniqueIdToCategory, syncCategories);
			GlobalControl.SyncAllProjects.Checked = GlobalFilter.AllProjects ?? false;
			GlobalControl.IncludeAllProjectsInSolution.Checked = GlobalFilter.AllProjectsInSln ?? false;

			WorkspaceSyncCategory.ApplyDelta(syncCategories, WorkspaceFilter.GetCategories());
			WorkspaceControl.SetView(WorkspaceFilter.View.ToArray());
			SetExcludedCategories(WorkspaceControl.CategoriesCheckList, _uniqueIdToCategory, syncCategories);
			WorkspaceControl.SyncAllProjects.Checked = WorkspaceFilter.AllProjects ?? GlobalFilter.AllProjects ?? false;
			WorkspaceControl.IncludeAllProjectsInSolution.Checked = WorkspaceFilter.AllProjectsInSln ?? GlobalFilter.AllProjectsInSln ?? false;

			GlobalControl.CategoriesCheckList.ItemCheck += GlobalControl_CategoriesCheckList_ItemCheck;
			GlobalControl.SyncAllProjects.CheckStateChanged += GlobalControl_SyncAllProjects_CheckStateChanged;
			GlobalControl.IncludeAllProjectsInSolution.CheckStateChanged += GlobalControl_IncludeAllProjectsInSolution_CheckStateChanged;
		}

		private void GlobalControl_CategoriesCheckList_ItemCheck(object? sender, ItemCheckEventArgs e)
		{
			WorkspaceControl.CategoriesCheckList.SetItemCheckState(e.Index, e.NewValue);
		}

		private void GlobalControl_SyncAllProjects_CheckStateChanged(object? sender, EventArgs e)
		{
			WorkspaceControl.SyncAllProjects.Checked = GlobalControl.SyncAllProjects.Checked;
		}

		private void GlobalControl_IncludeAllProjectsInSolution_CheckStateChanged(object? sender, EventArgs e)
		{
			WorkspaceControl.IncludeAllProjectsInSolution.Checked = GlobalControl.IncludeAllProjectsInSolution.Checked;
		}

		private static void SetExcludedCategories(CheckedListBox listBox, Dictionary<Guid, WorkspaceSyncCategory> uniqueIdToFilter, Dictionary<Guid, bool> categoryIdToSetting)
		{
			listBox.Items.Clear();
			foreach(WorkspaceSyncCategory filter in uniqueIdToFilter.Values)
			{
				if (!filter.Hidden)
				{
					CheckState state = CheckState.Checked;
					if (!categoryIdToSetting[filter.UniqueId])
					{
						state = CheckState.Unchecked;
					}
					listBox.Items.Add(filter, state);
				}
			}
		}

		private void GetSettings(out FilterSettings newGlobalFilter, out FilterSettings newWorkspaceFilter)
		{
			Dictionary<Guid, bool> defaultSyncCategories = WorkspaceSyncCategory.GetDefault(_uniqueIdToCategory.Values);

			newGlobalFilter = new FilterSettings();
			newGlobalFilter.View = GlobalControl.GetView().ToList();
			newGlobalFilter.AllProjects = GlobalControl.SyncAllProjects.Checked;
			newGlobalFilter.AllProjectsInSln = GlobalControl.IncludeAllProjectsInSolution.Checked;

			Dictionary<Guid, bool> globalSyncCategories = GetCategorySettings(GlobalControl.CategoriesCheckList, GlobalFilter.GetCategories());
			newGlobalFilter.SetCategories(WorkspaceSyncCategory.GetDelta(defaultSyncCategories, globalSyncCategories));

			newWorkspaceFilter = new FilterSettings();
			newWorkspaceFilter.View = WorkspaceControl.GetView().ToList();
			newWorkspaceFilter.AllProjects = (WorkspaceControl.SyncAllProjects.Checked == newGlobalFilter.AllProjects) ? (bool?)null : WorkspaceControl.SyncAllProjects.Checked;
			newWorkspaceFilter.AllProjectsInSln = (WorkspaceControl.IncludeAllProjectsInSolution.Checked == newGlobalFilter.AllProjectsInSln) ? (bool?)null : WorkspaceControl.IncludeAllProjectsInSolution.Checked;

			Dictionary<Guid, bool> workspaceSyncCategories = GetCategorySettings(WorkspaceControl.CategoriesCheckList, WorkspaceFilter.GetCategories());
			newWorkspaceFilter.SetCategories(WorkspaceSyncCategory.GetDelta(globalSyncCategories, workspaceSyncCategories));
		}

		private Dictionary<Guid, bool> GetCategorySettings(CheckedListBox listBox, IEnumerable<KeyValuePair<Guid, bool>> originalSettings)
		{
			Dictionary<Guid, bool> result = new Dictionary<Guid, bool>();
			for(int idx = 0; idx < listBox.Items.Count; idx++)
			{
				Guid uniqueId = ((WorkspaceSyncCategory)listBox.Items[idx]).UniqueId;
				if (!result.ContainsKey(uniqueId))
				{
					result[uniqueId] = listBox.GetItemCheckState(idx) == CheckState.Checked;
				}
			}
			foreach (KeyValuePair<Guid, bool> originalSetting in originalSettings)
			{
				if (!_uniqueIdToCategory.ContainsKey(originalSetting.Key))
				{
					result[originalSetting.Key] = originalSetting.Value;
				}
			}
			return result;
		}

		private static string[] GetView(TextBox filterText)
		{
			List<string> newLines = new List<string>(filterText.Lines);
			while (newLines.Count > 0 && newLines.Last().Trim().Length == 0)
			{
				newLines.RemoveAt(newLines.Count - 1);
			}
			return newLines.Count > 0 ? filterText.Lines : new string[0];
		}

		private void OkButton_Click(object sender, EventArgs e)
		{
			GetSettings(out FilterSettings newGlobalFilter, out FilterSettings newWorkspaceFilter);

			if(newGlobalFilter.View.Any(x => x.Contains("//")) || newWorkspaceFilter.View.Any(x => x.Contains("//")))
			{
				if(MessageBox.Show(this, "Custom views should be relative to the stream root (eg. -/Engine/...).\r\n\r\nFull depot paths (eg. //depot/...) will not match any files.\r\n\r\nAre you sure you want to continue?", "Invalid view", MessageBoxButtons.OKCancel) != System.Windows.Forms.DialogResult.OK)
				{
					return;
				}
			}

			GlobalFilter = newGlobalFilter;
			WorkspaceFilter = newWorkspaceFilter;

			DialogResult = DialogResult.OK;
		}

		private void CancButton_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
		}

		private void ShowCombinedView_Click(object sender, EventArgs e)
		{
			GetSettings(out FilterSettings newGlobalFilter, out FilterSettings newWorkspaceFilter);

			string[] filter = UserSettings.GetCombinedSyncFilter(_uniqueIdToCategory, newGlobalFilter, newWorkspaceFilter);
			if(filter.Length == 0)
			{
				filter = new string[]{ "All files will be synced." };
			}
			MessageBox.Show(String.Join("\r\n", filter), "Combined View");
		}
	}
}
