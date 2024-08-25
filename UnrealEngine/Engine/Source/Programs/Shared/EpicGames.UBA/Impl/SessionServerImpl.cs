// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Concurrent;
using System.Collections.Generic;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	internal class SessionServerCreateInfoImpl : ISessionServerCreateInfo
	{
		IntPtr _handle = IntPtr.Zero;
		readonly IStorageServer _storage;
		readonly IServer _client;
		readonly ILogger _logger;

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr CreateSessionServerCreateInfo(IntPtr storage, IntPtr client, IntPtr logger, string rootDir, string traceOutputFile,
			bool disableCustomAllocator, bool launchVisualizer, bool resetCas, bool writeToDisk, bool detailedTrace, bool allowWaitOnMem, bool allowKillOnMem);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroySessionServerCreateInfo(IntPtr server);
		#endregion

		public SessionServerCreateInfoImpl(IStorageServer storage, IServer client, ILogger logger, SessionServerCreateInfo info)
		{
			_storage = storage;
			_client = client;
			_logger = logger;
			_handle = CreateSessionServerCreateInfo(_storage.GetHandle(), _client.GetHandle(), _logger.GetHandle(), info.RootDirectory, info.TraceOutputFile, info.DisableCustomAllocator, info.LaunchVisualizer, info.ResetCas, info.WriteToDisk, info.DetailedTrace, info.AllowWaitOnMem, info.AllowKillOnMem);
		}

		#region IDisposable
		~SessionServerCreateInfoImpl() => Dispose(false);

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
				DestroySessionServerCreateInfo(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region ISessionServerCreateInfo
		public IntPtr GetHandle() => _handle;
		#endregion
	}

	internal class SessionServerImpl : ISessionServer
	{
		delegate void RemoteProcessSlotAvailableCallback();
		delegate void RemoteProcessReturnedCallback(IntPtr handle);

		IntPtr _handle = IntPtr.Zero;
		readonly ISessionServerCreateInfo _info;
		readonly RemoteProcessSlotAvailableCallback _remoteProcessSlotAvailableCallbackDelegate;
		readonly RemoteProcessReturnedCallback _remoteProcessReturnedCallbackDelegate;
		readonly ConcurrentDictionary<ulong, IProcess> _remoteProcesses = new();

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr CreateSessionServer(IntPtr info);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_SetRemoteProcessAvailable(IntPtr server, RemoteProcessSlotAvailableCallback func);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_SetRemoteProcessReturned(IntPtr server, RemoteProcessReturnedCallback func);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_RefreshDirectory(IntPtr server, string directory);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_RegisterNewFile(IntPtr server, string filename);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern uint SessionServer_BeginExternalProcess(IntPtr server, string description);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_EndExternalProcess(IntPtr server, uint id, uint exitCode);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr SessionServer_RunProcess(IntPtr server, IntPtr info, bool async, bool enableDetour);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr SessionServer_RunProcessRemote(IntPtr server, IntPtr info, float weight, byte[]? knownInputs, uint knownInputsCount);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_SetMaxRemoteProcessCount(IntPtr server, uint count);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_DisableRemoteExecution(IntPtr server);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_SetCustomCasKeyFromTrackedInputs(IntPtr server, IntPtr process, string filename, string workingdir);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_PrintSummary(IntPtr server);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void SessionServer_CancelAll(IntPtr server);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroySessionServer(IntPtr server);
		#endregion

		public SessionServerImpl(ISessionServerCreateInfo info)
		{
			_info = info;
			_remoteProcessSlotAvailableCallbackDelegate = RaiseRemoteProcessSlotAvailable;
			_remoteProcessReturnedCallbackDelegate = RaiseRemoteProcessReturned;
			_handle = CreateSessionServer(_info.GetHandle());
			SessionServer_SetRemoteProcessAvailable(_handle, _remoteProcessSlotAvailableCallbackDelegate);
			SessionServer_SetRemoteProcessReturned(_handle, _remoteProcessReturnedCallbackDelegate);
		}

		#region IDisposable
		~SessionServerImpl() => Dispose(false);

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
				DestroySessionServer(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region ISessionServer
		public IntPtr GetHandle() => _handle;

		public event ISessionServer.RemoteProcessSlotAvailableEventHandler? RemoteProcessSlotAvailable;

		public event ISessionServer.RemoteProcessReturnedEventHandler? RemoteProcessReturned;

		public IProcess RunProcess(ProcessStartInfo info, bool async, IProcess.ExitedEventHandler? exitedEventHandler, bool enableDetour)
		{
			IProcessStartInfo startInfo = IProcessStartInfo.CreateProcessStartInfo(info, exitedEventHandler != null);
			IntPtr processPtr = SessionServer_RunProcess(_handle, startInfo.GetHandle(), async, enableDetour);
			IProcess process = IProcess.CreateProcess(processPtr, startInfo, exitedEventHandler, info.UserData);
			return process;
		}

		public IProcess RunProcessRemote(ProcessStartInfo info, IProcess.ExitedEventHandler? exitedEventHandler, double weight, byte[]? knownInputs, uint knownInputsCount)
		{
			IProcessStartInfo startInfo = IProcessStartInfo.CreateProcessStartInfo(info, exitedEventHandler != null);
			IntPtr processPtr = SessionServer_RunProcessRemote(_handle, startInfo.GetHandle(), (float)weight, knownInputs, knownInputsCount);
			IProcess process = IProcess.CreateProcess(processPtr, startInfo, exitedEventHandler, info.UserData);
			_remoteProcesses.AddOrUpdate(process.Hash, process, (k, v) => process);
			return process;
		}

		public void DisableRemoteExecution() => SessionServer_DisableRemoteExecution(_handle);

		public void SetMaxRemoteProcessCount(uint count) => SessionServer_SetMaxRemoteProcessCount(_handle, count);

		public void RefreshDirectories(params string[] directories) => Array.ForEach(directories, (directory) => SessionServer_RefreshDirectory(_handle, directory));

		public void RegisterNewFiles(params string[] files) => Array.ForEach(files, (file) => SessionServer_RegisterNewFile(_handle, file));

		public uint BeginExternalProcess(string description) => SessionServer_BeginExternalProcess(_handle, description);
		public void EndExternalProcess(uint id, uint exitCode) => SessionServer_EndExternalProcess(_handle, id, exitCode);

		public void SetCustomCasKeyFromTrackedInputs(string file, string workingDirectory, IProcess process) => SessionServer_SetCustomCasKeyFromTrackedInputs(_handle, process.GetHandle(), file, workingDirectory);

		public void PrintSummary() => SessionServer_PrintSummary(_handle);
		
		public void CancelAll() => SessionServer_CancelAll(_handle);
		#endregion

		void RaiseRemoteProcessSlotAvailable()
		{
			RemoteProcessSlotAvailable?.Invoke(this, new RemoteProcessSlotAvailableEventArgs());
		}

		void RaiseRemoteProcessReturned(IntPtr handle)
		{
			IProcess? process = null;
			try
			{
				if (_remoteProcesses.Remove((ulong)handle.ToInt64(), out process))
				{
					RemoteProcessReturned?.Invoke(this, new RemoteProcessReturnedEventArgs(process));
				}
			}
			finally
			{
				process?.Dispose();
			}
		}
	}
}
