// Copyright Epic Games, Inc. All Rights Reserved.

using System.Windows.Forms;

namespace UnrealGameSync
{
	public partial class LogWindow : Form
	{
		public LogWindow(string text)
		{
			InitializeComponent();

			LogTextBox.Text = text;
			LogTextBox.Select(text.Length, 0);
		}
	}
}
