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
	public partial class PasswordWindow : Form
	{
		public string Password;

		public PasswordWindow(string prompt, string password)
		{
			InitializeComponent();

			this.Password = password;

			PromptLabel.Text = prompt;
			PasswordTextBox.Text = password;
		}

		private void OkBtn_Click(object sender, EventArgs e)
		{
			Password = PasswordTextBox.Text;
		}

		private void CancelBtn_Click(object sender, EventArgs e)
		{

		}
	}
}
