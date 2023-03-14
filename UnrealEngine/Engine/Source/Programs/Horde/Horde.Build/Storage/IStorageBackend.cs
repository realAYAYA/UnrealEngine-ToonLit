// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Buffers;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;

namespace Horde.Build.Storage
{
	/// <summary>
	/// Interface for a traditional location-addressed storage provider
	/// </summary>
	public interface IStorageBackend
	{
		/// <summary>
		/// Opens a read stream for the given path.
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<Stream?> ReadAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a stream to the given path. If the stream throws an exception during read, the write will be aborted.
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="stream">Stream to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Tests whether the given path exists
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Deletes a file with the given path
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(string path, CancellationToken cancellationToken = default);
	}

	/// <summary>
	/// Generic version of IStorageBackend, to allow for dependency injection of different singletons
	/// </summary>
	/// <typeparam name="T">Type distinguishing different singletons</typeparam>
	public interface IStorageBackend<T> : IStorageBackend
	{
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageBackend"/>
	/// </summary>
	public static class StorageBackendExtensions
	{
		/// <summary>
		/// Wrapper for <see cref="IStorageBackend"/>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		class StorageBackend<T> : IStorageBackend<T>
		{
			readonly IStorageBackend _inner;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="inner"></param>
			public StorageBackend(IStorageBackend inner) => _inner = inner;

			/// <inheritdoc/>
			public Task<Stream?> ReadAsync(string path, CancellationToken cancellationToken) => _inner.ReadAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken) => _inner.WriteAsync(path, stream, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken) => _inner.DeleteAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken) => _inner.ExistsAsync(path, cancellationToken);
		}

		/// <summary>
		/// Creates a typed wrapper around the given storage backend
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="backend"></param>
		/// <returns></returns>
		public static IStorageBackend<T> ForType<T>(this IStorageBackend backend)
		{
			return new StorageBackend<T>(backend);
		}

		/// <summary>
		/// Writes a block of memory to storage
		/// </summary>
		/// <param name="storageBackend"></param>
		/// <param name="path"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<ReadOnlyMemory<byte>?> ReadBytesAsync(this IStorageBackend storageBackend, string path, CancellationToken cancellationToken = default)
		{
			using (Stream? inputStream = await storageBackend.ReadAsync(path, cancellationToken))
			{
				if (inputStream == null)
				{
					return null;
				}

				using (MemoryStream outputStream = new MemoryStream())
				{
					await inputStream.CopyToAsync(outputStream, cancellationToken);
					return outputStream.ToArray();
				}
			}
		}

		/// <summary>
		/// Writes a block of memory to storage
		/// </summary>
		/// <param name="storageBackend"></param>
		/// <param name="path"></param>
		/// <param name="data"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task WriteBytesAsync(this IStorageBackend storageBackend, string path, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default)
		{
			using (ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data))
			{
				await storageBackend.WriteAsync(path, stream, cancellationToken);
			}
		}
	}
}
