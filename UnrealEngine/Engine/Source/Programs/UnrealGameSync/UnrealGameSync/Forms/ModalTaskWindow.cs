// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	partial class ModalTaskWindow : Form
	{
		Task _task;
		CancellationTokenSource _cancellationSource;

		public ModalTaskWindow(string inTitle, string inMessage, FormStartPosition inStartPosition, Task inTask, CancellationTokenSource inCancellationSource)//, Func<CancellationToken, Task> InTaskFunc)
		{
			InitializeComponent();

			Text = inTitle;
			MessageLabel.Text = inMessage;
			StartPosition = inStartPosition;
			_task = inTask;
			_cancellationSource = inCancellationSource;
		}

		public void ShowAndActivate()
		{
			Show();
			Activate();
		}

		private void ModalTaskWindow_Load(object sender, EventArgs e)
		{
			SynchronizationContext syncContext = SynchronizationContext.Current!;
			_task.ContinueWith(x => syncContext.Post(y => Close(), null));
		}

		private void ModalTaskWindow_FormClosing(object sender, FormClosingEventArgs e)
		{
			_cancellationSource.Cancel();
			if (!_task.IsCompleted)
			{
				e.Cancel = true;
			}
		}
	}
}
