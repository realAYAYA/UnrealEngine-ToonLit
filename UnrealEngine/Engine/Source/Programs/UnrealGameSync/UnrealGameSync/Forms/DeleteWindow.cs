// Copyright Epic Games, Inc. All Rights Reserved.

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
	public partial class DeleteWindow : Form
	{
		Dictionary<string, bool> _filesToDelete;

		public DeleteWindow(Dictionary<string, bool> inFilesToDelete)
		{
			InitializeComponent();

			_filesToDelete = inFilesToDelete;

			foreach(KeyValuePair<string, bool> fileToDelete in _filesToDelete)
			{
				ListViewItem item = new ListViewItem(fileToDelete.Key);
				item.Tag = fileToDelete.Key;
				item.Checked = fileToDelete.Value;
				FileList.Items.Add(item);
			}
		}

		private void UncheckAll_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem? item in FileList.Items)
			{
				item!.Checked = false;
			}
		}

		private void CheckAll_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem? item in FileList.Items)
			{
				item!.Checked = true;
			}
		}

		private void ContinueButton_Click(object sender, EventArgs e)
		{
			foreach(ListViewItem? item in FileList.Items)
			{
				_filesToDelete[(string)item!.Tag] = item.Checked;
			}
		}
	}
}
