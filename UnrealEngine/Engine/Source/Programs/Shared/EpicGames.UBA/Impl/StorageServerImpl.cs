// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Runtime.InteropServices;

namespace EpicGames.UBA
{
	internal class StorageServerImpl : IStorageServer
	{
		IntPtr _handle = IntPtr.Zero;
		readonly IServer _server;
		readonly ILogger _logger;

		#region DllImport
		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern IntPtr CreateStorageServer(IntPtr server, string rootDir, ulong casCapacityBytes, bool storeCompressed, IntPtr logger, string zone);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void DestroyStorageServer(IntPtr server);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void StorageServer_SaveCasTable(IntPtr server);

		[DllImport("UbaHost", CharSet = CharSet.Auto)]
		static extern void StorageServer_RegisterDisallowedPath(IntPtr server, string path);
		#endregion

		public StorageServerImpl(IServer server, ILogger logger, StorageServerCreateInfo info)
		{
			_server = server;
			_logger = logger;
			_handle = CreateStorageServer(_server.GetHandle(), info.RootDirectory, info.CapacityBytes, info.StoreCompressed, _logger.GetHandle(), info.Zone);
			Utils.DisallowedPaths().ToList().ForEach(x => RegisterDisallowedPath(x));
		}

		#region IDisposable
		~StorageServerImpl() => Dispose(false);

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
				DestroyStorageServer(_handle);
				_handle = IntPtr.Zero;
			}
		}
		#endregion

		#region IStorageServer
		public IntPtr GetHandle() => _handle;

		public void SaveCasTable() => StorageServer_SaveCasTable(_handle);

		public void RegisterDisallowedPath(string path) => StorageServer_RegisterDisallowedPath(_handle, path);
		#endregion
	}
}
