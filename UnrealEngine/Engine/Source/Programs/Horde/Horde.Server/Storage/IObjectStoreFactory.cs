// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Storage;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Service which resolves storage backend instances
	/// </summary>
	public interface IObjectStoreFactory
	{
		/// <summary>
		/// Creates a new storage backend with the given identifier
		/// </summary>
		/// <param name="backendConfig">Configuration object for the storage backend</param>
		/// <returns>New backend instance. This instance should be disposed when the client has finished with it.</returns>
		IObjectStore CreateObjectStore(BackendConfig backendConfig);
	}
}
