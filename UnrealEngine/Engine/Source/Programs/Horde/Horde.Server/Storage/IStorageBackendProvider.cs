// Copyright Epic Games, Inc. All Rights Reserved.

namespace Horde.Server.Storage
{
	/// <summary>
	/// Service which resolves storage backend instances
	/// </summary>
	public interface IStorageBackendProvider
	{
		/// <summary>
		/// Creates a new storage backend with the given identifier
		/// </summary>
		/// <param name="backendConfig">Configuration object for the storage backend</param>
		/// <returns>New backend instance. This instance should be disposed when the client has finished with it.</returns>
		IStorageBackend CreateBackend(BackendConfig backendConfig);
	}
}
