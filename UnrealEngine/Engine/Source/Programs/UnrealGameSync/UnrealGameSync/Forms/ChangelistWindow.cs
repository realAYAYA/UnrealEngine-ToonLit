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
	public partial class ChangelistWindow : Form
	{
		public int ChangeNumber;

		public ChangelistWindow(int inChangeNumber)
		{
			InitializeComponent();

			this.MaximumSize = new System.Drawing.Size(32768, Height);
			this.MinimumSize = new System.Drawing.Size(Width, Height);

			ChangeNumber = inChangeNumber;
			ChangeNumberTextBox.Text = (ChangeNumber > 0)? ChangeNumber.ToString() : "";
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			int newChangeNumber;
			if(int.TryParse(ChangeNumberTextBox.Text.Trim(), out newChangeNumber))
			{
				int deltaChangeNumber = newChangeNumber - ChangeNumber;
				if((deltaChangeNumber > -10000 && deltaChangeNumber < 1000) || ChangeNumber <= 0 || MessageBox.Show(String.Format("Changelist {0} is {1} changes away from your current sync. Was that intended?", newChangeNumber, Math.Abs(deltaChangeNumber)), "Changelist is far away", MessageBoxButtons.YesNo) == DialogResult.Yes)
				{
					ChangeNumber = newChangeNumber;
					DialogResult = DialogResult.OK;
					Close();
				}
			}
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{
			DialogResult = DialogResult.Cancel;
			Close();
		}
	}
}
