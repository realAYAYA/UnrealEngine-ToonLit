// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	internal class ServerImpl : IServer
	{
		IntPtr _handle = IntPtr.Zero;
		readonly ILogger _logger;

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr CreateServer(IntPtr logger, int workerCount, int sendSize, int receiveTimeoutSeconds, bool useQuic);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroyServer(IntPtr server);

		// Server Imports
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern bool Server_StartListen(IntPtr server, int port, string ip, string crypto);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern bool Server_AddClient(IntPtr server, string ip, int port, string crypto);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern bool Server_AddNamedConnection(IntPtr server, string name);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern bool Server_Stop(IntPtr server);

		const int DEFAULT_PORT = 1345;
		#endregion

		public ServerImpl(int maxWorkers, int sendSize, ILogger logger, bool useQuic)
		{
			_logger = logger;
			_handle = CreateServer(_logger.GetHandle(), maxWorkers, sendSize, 60, useQuic); // We know this code is using 5s pings.. so 60 seconds timeout on recv socket is good
		}

		#region IDisposable
		~ServerImpl() => Dispose(false);

		/// <inheritdoc/>
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
				DestroyServer(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region IServer
		/// <inheritdoc/>
		public IntPtr GetHandle() => _handle;

		/// <inheritdoc/>
		public bool StartServer(string ip = "", int port = -1, string crypto = "") => Server_StartListen(_handle, port > 0 ? port : DEFAULT_PORT, ip, crypto);

		/// <inheritdoc/>
		public void StopServer() => Server_Stop(_handle);

		/// <inheritdoc/>
		public bool AddClient(string ip, int port, string crypto = "") => Server_AddClient(_handle, ip, port, crypto);

		/// <inheritdoc/>
		public bool AddNamedConnection(string name) => Server_AddNamedConnection(_handle, name);
		#endregion
	}
}
