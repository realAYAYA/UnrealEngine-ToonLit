// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics;
using System.IO.Pipes;
using EpicGames.Core;
using Horde.Agent.TrayApp.Properties;

namespace Horde.Agent.TrayApp
{
	static class Program
	{
		const string MutexName = "Horde.Agent.TrayApp-Mutex";
		const string EventName = "Horde.Agent.TrayApp-Exit";

		[STAThread]
		public static int Main(string[] args)
		{
			using EventWaitHandle closeEvent = new EventWaitHandle(false, EventResetMode.AutoReset, EventName);
			using SingleInstanceMutex mutex = new SingleInstanceMutex(MutexName);

			if (args.Any(x => x.Equals("-close", StringComparison.OrdinalIgnoreCase)))
			{
				closeEvent.Set();
				return mutex.Wait(5000)? 0 : 1;
			}

			if (!mutex.Wait(0))
			{
				return 1;
			}

			MainAsync(closeEvent).GetAwaiter().GetResult();
			return 0;
		}

		static async Task MainAsync(EventWaitHandle closeEvent)
		{
			ApplicationConfiguration.Initialize();

			await using (CustomApplicationContext appContext = new CustomApplicationContext(closeEvent))
			{
				Application.Run(appContext);
			}
		}
	}

	class SingleInstanceMutex : IDisposable
	{
		readonly Mutex _mutex;
		bool _locked;

		public SingleInstanceMutex(string name)
		{
			_mutex = new Mutex(true, name);
		}

		public bool Wait(int timeout)
		{
			if (!_locked)
			{
				try
				{
					_locked = _mutex.WaitOne(timeout);
				}
				catch (AbandonedMutexException)
				{
					_locked = true;
				}
			}
			return _locked;
		}

		public void Dispose()
		{
			if (_locked)
			{
				_mutex.ReleaseMutex();
			}
			_mutex.Dispose();
		}
	}

	public class CustomApplicationContext : ApplicationContext, IAsyncDisposable
	{
		readonly NotifyIcon _trayIcon;
		readonly BackgroundTask _clientTask;
		readonly BackgroundTask _waitForExitTask;
		readonly Control _mainThreadInvokeTarget;

		bool _disposed;

		public CustomApplicationContext(EventWaitHandle eventHandle)
		{
			ToolStripMenuItem logsMenuItem = new ToolStripMenuItem("View logs");
			logsMenuItem.Click += OnOpenLogs;

			ToolStripMenuItem exitMenuItem = new ToolStripMenuItem("Exit");
			exitMenuItem.Click += OnExit;

			ContextMenuStrip menu = new ContextMenuStrip();
			menu.Items.Add(logsMenuItem);
			menu.Items.Add(new ToolStripSeparator());
			menu.Items.Add(exitMenuItem);

			_mainThreadInvokeTarget = new Control();
			_mainThreadInvokeTarget.CreateControl();

			_trayIcon = new NotifyIcon()
			{
				Icon = Resources.StatusNormal,
				ContextMenuStrip = menu,
				Visible = true
			};

			_clientTask = BackgroundTask.StartNew(StatusTask);
			_waitForExitTask = BackgroundTask.StartNew(ctx => WaitForExitTask(eventHandle, ctx));
		}

		protected override void Dispose(bool disposing)
		{
			base.Dispose(disposing);

			if (disposing)
			{
				_mainThreadInvokeTarget.Dispose();
				_trayIcon.Dispose();
				_disposed = true;
			}
		}

		public async ValueTask DisposeAsync()
		{
			await _waitForExitTask.DisposeAsync();
			await _clientTask.DisposeAsync();

			Dispose();
			GC.SuppressFinalize(this);
		}

		private void OnOpenLogs(object? sender, EventArgs e)
		{
			DirectoryReference? programDataDir = DirectoryReference.GetSpecialFolder(Environment.SpecialFolder.CommonApplicationData);
			if (programDataDir != null)
			{
				DirectoryReference logsDir = DirectoryReference.Combine(programDataDir, "HordeAgent");
				Process.Start(new ProcessStartInfo { FileName = logsDir.FullName, UseShellExecute = true });
			}
		}

		private void OnExit(object? sender, EventArgs e)
		{
			ExitThread();
		}

		void SetStatus(AgentStatusMessage status)
		{
			_mainThreadInvokeTarget.BeginInvoke(() => SetStatus_MainThread(status));
		}

		void SetStatus_MainThread(AgentStatusMessage status)
		{
			if (!_disposed)
			{
				if (!status.Healthy)
				{
					_trayIcon.Icon = Resources.StatusError;
					_trayIcon.Text = String.IsNullOrEmpty(status.Detail) ? "Error. Check logs." : (status.Detail.Length > 100) ? status.Detail.Substring(0, 100) : status.Detail;
				}
				else if (status.NumLeases > 0)
				{
					_trayIcon.Icon = Resources.StatusBusy;
					_trayIcon.Text = (status.NumLeases == 1) ? "Currently handling 1 lease" : $"Currently handling {status.NumLeases} leases";
				}
				else
				{
					_trayIcon.Icon = Resources.StatusNormal;
					_trayIcon.Text = "Agent is operating normally";
				}
			}
		}

		async Task StatusTask(CancellationToken cancellationToken)
		{
			SetStatus(AgentStatusMessage.Starting);
			for (; ; )
			{
				try
				{
					await PollForStatusUpdatesAsync(cancellationToken);
				}
				catch (OperationCanceledException) when (cancellationToken.IsCancellationRequested)
				{
					break;
				}
				catch
				{
					SetStatus(new AgentStatusMessage(false, 0, "Unable to connect to Agent. Check the service is running."));
					await Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken);
				}
			}
		}

		async Task WaitForExitTask(EventWaitHandle eventHandle, CancellationToken cancellationToken)
		{
			await eventHandle.WaitOneAsync(cancellationToken);
			_mainThreadInvokeTarget.BeginInvoke(() => Exit_MainThread());
		}

		async Task PollForStatusUpdatesAsync(CancellationToken cancellationToken)
		{
			AgentMessageBuffer message = new AgentMessageBuffer();
			using (NamedPipeClientStream pipeClient = new NamedPipeClientStream(".", AgentMessagePipe.PipeName, PipeDirection.InOut))
			{
				SetStatus(new AgentStatusMessage(false, 0, "Connecting to agent..."));
				await pipeClient.ConnectAsync(cancellationToken);

				SetStatus(new AgentStatusMessage(false, 0, "Waiting for status update."));
				for (; ; )
				{
					message.Set(AgentMessageType.GetStatusRequest);
					await message.SendAsync(pipeClient, cancellationToken);

					if (!await message.TryReadAsync(pipeClient, cancellationToken))
					{
						break;
					}					
					
					switch (message.Type)
					{
						case AgentMessageType.GetStatusResponse:
							AgentStatusMessage status = message.Parse<AgentStatusMessage>();
							SetStatus(status);
							break;
					}

					await Task.Delay(TimeSpan.FromSeconds(5.0), cancellationToken);
				}
			}
		}

		void Exit_MainThread()
		{
			if (!_disposed)
			{
				_trayIcon.Visible = false;
				Application.Exit();
			}
		}

		void Exit(object sender, EventArgs e)
		{
			Exit_MainThread();
		}
	}
}
