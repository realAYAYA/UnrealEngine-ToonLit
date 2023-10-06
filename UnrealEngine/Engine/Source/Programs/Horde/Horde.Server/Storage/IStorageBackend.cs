// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Core;
using EpicGames.Horde.Storage;

namespace Horde.Server.Storage
{
	/// <summary>
	/// Interface for a traditional location-addressed storage provider
	/// </summary>
	public interface IStorageBackend : IDisposable
	{
		/// <summary>
		/// Whether this storage backend supports redirects or not
		/// </summary>
		bool SupportsRedirects { get; }

		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<Stream?> TryReadAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to open a read stream for the given path.
		/// </summary>
		/// <param name="path">Relative path within the bucket</param>
		/// <param name="offset">Offset to start reading from</param>
		/// <param name="length">Length of data to read</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<Stream?> TryReadAsync(string path, int offset, int length, CancellationToken cancellationToken = default);

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

		/// <summary>
		/// Enumerates all the objects in the store
		/// </summary>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of object paths</returns>
		IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a HTTP redirect for a read request
		/// </summary>
		/// <param name="path">Path to read from</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to upload the data to</returns>
		ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default);

		/// <summary>
		/// Gets a HTTP redirect for a write request
		/// </summary>
		/// <param name="path">Path to write to</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Path to upload the data to</returns>
		ValueTask<Uri?> TryGetWriteRedirectAsync(string path, CancellationToken cancellationToken = default);
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
	public static class StorageBackend
	{
		/// <summary>
		/// Wrapper for <see cref="IStorageBackend"/>
		/// </summary>
		/// <typeparam name="T"></typeparam>
		sealed class TypedStorageBackend<T> : IStorageBackend<T>
		{
			readonly IStorageBackend _inner;

			/// <inheritdoc/>
			public bool SupportsRedirects => _inner.SupportsRedirects;

			/// <summary>
			/// Constructor
			/// </summary>
			/// <param name="inner"></param>
			public TypedStorageBackend(IStorageBackend inner) => _inner = inner;

			/// <inheritdoc/>
			public void Dispose() => _inner.Dispose();

			/// <inheritdoc/>
			public Task<Stream?> TryReadAsync(string path, CancellationToken cancellationToken) => _inner.TryReadAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task<Stream?> TryReadAsync(string path, int offset, int length, CancellationToken cancellationToken) => _inner.TryReadAsync(path, offset, length, cancellationToken);

			/// <inheritdoc/>
			public Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken) => _inner.WriteAsync(path, stream, cancellationToken);

			/// <inheritdoc/>
			public Task DeleteAsync(string path, CancellationToken cancellationToken) => _inner.DeleteAsync(path, cancellationToken);

			/// <inheritdoc/>
			public Task<bool> ExistsAsync(string path, CancellationToken cancellationToken) => _inner.ExistsAsync(path, cancellationToken);

			/// <inheritdoc/>
			public IAsyncEnumerable<string> EnumerateAsync(CancellationToken cancellationToken = default) => _inner.EnumerateAsync(cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetReadRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetReadRedirectAsync(path, cancellationToken);

			/// <inheritdoc/>
			public ValueTask<Uri?> TryGetWriteRedirectAsync(string path, CancellationToken cancellationToken = default) => _inner.TryGetWriteRedirectAsync(path, cancellationToken);
		}

		/// <summary>
		/// Extension for blob files
		/// </summary>
		public const string BlobExtension = ".blob";

		/// <summary>
		/// Gets the blob id from a path
		/// </summary>
		/// <param name="path">Path to the blob</param>
		/// <returns>Path to the blob</returns>
		public static BlobId GetBlobIdFromPath(string path)
		{
			BlobId blobId;
			if (!TryGetBlobIdFromPath(path, out blobId))
			{
				throw new ArgumentException("Path is not a valid blob identifier", nameof(path));
			}
			return blobId;
		}

		/// <summary>
		/// Gets the path to a blob
		/// </summary>
		/// <param name="blobId">Blob identifier</param>
		/// <returns>Path to the blob</returns>
		public static string GetBlobPath(BlobId blobId) => $"{blobId}{BlobExtension}";

		/// <summary>
		/// Gets a blob id from a path within the storage backend
		/// </summary>
		/// <param name="path">Path to the file</param>
		/// <param name="blobId">Receives the blob id on success</param>
		/// <returns>True on success</returns>
		public static bool TryGetBlobIdFromPath(string path, out BlobId blobId)
		{
			if (path.EndsWith(BlobExtension, StringComparison.Ordinal))
			{
				blobId = new BlobId(path.Substring(0, path.Length - BlobExtension.Length));
				return true;
			}
			else
			{
				blobId = default;
				return false;
			}
		}

		/// <summary>
		/// Creates a typed wrapper around the given storage backend
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="backend"></param>
		/// <returns></returns>
		public static IStorageBackend<T> ForType<T>(this IStorageBackend backend)
		{
			return new TypedStorageBackend<T>(backend);
		}

		/// <summary>
		/// Writes a block of memory to storage
		/// </summary>
		/// <param name="storageBackend"></param>
		/// <param name="path"></param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<Stream> ReadAsync(this IStorageBackend storageBackend, string path, CancellationToken cancellationToken = default)
		{
			Stream? stream = await storageBackend.TryReadAsync(path, cancellationToken);
			if (stream == null)
			{
				throw new FileNotFoundException($"Unable to read from path {path}");
			}
			return stream;
		}

		/// <summary>
		/// Writes a block of memory to storage
		/// </summary>
		/// <param name="storageBackend"></param>
		/// <param name="path"></param>
		/// <param name="offset">Offset of the data to read</param>
		/// <param name="length">Length of the data to read</param>
		/// <param name="cancellationToken"></param>
		/// <returns></returns>
		public static async Task<Stream> ReadAsync(this IStorageBackend storageBackend, string path, int offset, int length, CancellationToken cancellationToken = default)
		{
			Stream? stream = await storageBackend.TryReadAsync(path, offset, length, cancellationToken);
			if (stream == null)
			{
				throw new FileNotFoundException($"Unable to read from path {path}");
			}
			return stream;
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
			using (Stream? inputStream = await storageBackend.TryReadAsync(path, cancellationToken))
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
