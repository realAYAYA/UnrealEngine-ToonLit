// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Threading;

namespace EpicGames.UBA
{
	internal class ProcessStartInfoImpl : IProcessStartInfo
	{
		IntPtr _handle = IntPtr.Zero;
		public delegate void ExitCallback(IntPtr userData, IntPtr handle);
		public event ExitCallback? ExitCallbackDelegate;
		public ProcessImpl? Process { get; set; }

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr CreateProcessStartInfo(string application, string arguments, string workingDir, string description, uint priorityClass, ulong outputStatsThresholdMs, bool trackInputs, string logFile, ExitCallback? exit);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroyProcessStartInfo(IntPtr server);
		#endregion

		public ProcessStartInfoImpl(ProcessStartInfo info, bool needCallback)
		{
			if (needCallback)
			{
				ExitCallbackDelegate = RaiseExited;
			}

			_handle = CreateProcessStartInfo(info.Application, info.Arguments, info.WorkingDirectory, info.Description, (uint)info.Priority, info.OutputStatsThresholdMs, info.TrackInputs, info.LogFile?? String.Empty, ExitCallbackDelegate);
		}

		void RaiseExited(IntPtr userData, IntPtr handle)
		{
			while (Process == null)
			{
				Thread.Sleep(1); // This should never happen in practice. If c# is garbage collecting while c++ do a full network turnaround with an executed action it could theoretically happen
			}

			Process.RaiseExited();
		}

		#region IDisposable
		~ProcessStartInfoImpl() => Dispose(false);

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
			}

			if (_handle != IntPtr.Zero)
			{
				DestroyProcessStartInfo(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region IProcessStartInfo
		public IntPtr GetHandle() => _handle;
		#endregion
	}

	internal class ProcessImpl : IProcess
	{
		IntPtr _handle = IntPtr.Zero;
		[System.Diagnostics.CodeAnalysis.SuppressMessage("CodeQuality", "IDE0052:Remove unread private members", Justification = "maintain unmanaged reference")]
		readonly IProcessStartInfo _processStartInfo;

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern uint ProcessHandle_GetExitCode(IntPtr process);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr ProcessHandle_GetExecutingHost(IntPtr process);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr ProcessHandle_GetLogLine(IntPtr process, uint index);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern ulong ProcessHandle_GetTotalProcessorTime(IntPtr process);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern ulong ProcessHandle_GetTotalWallTime(IntPtr process);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern ulong ProcessHandle_GetHash(IntPtr process);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void ProcessHandle_Cancel(IntPtr process, bool terminate);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroyProcessHandle(IntPtr process);
		#endregion

		public ProcessImpl(IntPtr handle, IProcessStartInfo info, IProcess.ExitedEventHandler? exitedEventHandler, object? userData)
		{
			_handle = handle;
			_processStartInfo = info;
			UserData = userData;
			if (exitedEventHandler != null)
			{
				Exited += exitedEventHandler;
			}
			((ProcessStartInfoImpl)info).Process = this;
		}

		#region IDisposable
		~ProcessImpl() => Dispose(false);

		public void Dispose()
		{
			Dispose(true);
			GC.SuppressFinalize(this);
		}

		protected virtual void Dispose(bool disposing)
		{
			if (disposing)
			{
			}

			if (_handle != IntPtr.Zero)
			{
				DestroyProcessHandle(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region IServer
		public IntPtr GetHandle() => _handle;
		public event IProcess.ExitedEventHandler? Exited;

		public int ExitCode => (int)ProcessHandle_GetExitCode(_handle);

		public string? ExecutingHost => Marshal.PtrToStringAuto(ProcessHandle_GetExecutingHost(_handle));

		public List<string> LogLines
		{
			get
			{
				List<string> logLines = new();
				string? line = Marshal.PtrToStringAuto(ProcessHandle_GetLogLine(_handle, 0));
				while (line != null)
				{
					logLines.Add(line);
					line = Marshal.PtrToStringAuto(ProcessHandle_GetLogLine(_handle, (uint)logLines.Count));
				}
				return logLines;
			}
		}

		public TimeSpan TotalProcessorTime => new((long)ProcessHandle_GetTotalProcessorTime(_handle));

		public TimeSpan TotalWallTime => new((long)ProcessHandle_GetTotalWallTime(_handle));

		public ulong Hash => ProcessHandle_GetHash(_handle);

		public object? UserData { get; init; }

		public void Cancel(bool terminate) => ProcessHandle_Cancel(_handle, terminate);
		#endregion

		internal void RaiseExited()
		{
			if (ExitCode != 99999) // See ProcessCancelExitCode
			{
				Exited?.Invoke(this, new ExitedEventArgs(this));
			}
		}
	}
}
