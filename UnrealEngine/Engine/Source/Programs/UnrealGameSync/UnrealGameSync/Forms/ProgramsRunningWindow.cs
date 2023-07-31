// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace UnrealGameSync
{
	public partial class ProgramsRunningWindow : Form
	{
		object _syncObject = new object();
		FileReference[] _programs;
		Func<FileReference[]> _enumeratePrograms;
		ManualResetEvent? _terminateEvent;
		Thread? _backgroundThread;

		public ProgramsRunningWindow(Func<FileReference[]> enumeratePrograms, FileReference[] programs)
		{
			InitializeComponent();

			this._programs = programs.OrderBy(x => x).ToArray();
			this._enumeratePrograms = enumeratePrograms;
			this.ProgramListBox.Items.AddRange(programs);
		}

		private void ProgramsRunningWindow_Load(object sender, EventArgs e)
		{
			_terminateEvent = new ManualResetEvent(false);

			_backgroundThread = new Thread(() => ExecuteBackgroundWork());
			_backgroundThread.IsBackground = true;
			_backgroundThread.Start();
		}

		private void ExecuteBackgroundWork()
		{
			for(;;)
			{
				if(_terminateEvent!.WaitOne(TimeSpan.FromSeconds(2.0)))
				{
					break;
				}

				FileReference[] newPrograms = _enumeratePrograms().OrderBy(x => x).ToArray();
				lock(_syncObject)
				{
					if(!Enumerable.SequenceEqual(_programs, newPrograms))
					{
						_programs = newPrograms;
						BeginInvoke(new MethodInvoker(() => UpdatePrograms()));
					}
				}
			}
		}

		private void UpdatePrograms()
		{
			ProgramListBox.Items.Clear();
			ProgramListBox.Items.AddRange(_programs);

			if(_programs.Length == 0)
			{
				DialogResult = DialogResult.OK;
				Close();
			}
		}

		private void ProgramsRunningWindow_FormClosed(object sender, FormClosedEventArgs e)
		{
			if(_backgroundThread != null)
			{
				_terminateEvent!.Set();

				_backgroundThread.Join();
				_backgroundThread = null;

				_terminateEvent.Dispose();
				_terminateEvent = null;
			}
		}
	}
}
