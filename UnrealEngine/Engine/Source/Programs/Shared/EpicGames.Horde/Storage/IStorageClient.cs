// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Core;
using EpicGames.Serialization;
using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;

namespace EpicGames.Horde.Storage
{
	/// <summary>
	/// Base class for exceptions related to store
	/// </summary>
	public class StorageException : Exception
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public StorageException(string message, Exception? innerException)
			: base(message, innerException)
		{
		}
	}

	/// <summary>
	/// Base class for blob exceptions
	/// </summary>
	public class BlobException : StorageException
	{
		/// <summary>
		/// Namespace containing the blob
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// Hash of the blob
		/// </summary>
		public IoHash Hash { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public BlobException(NamespaceId namespaceId, IoHash hash, string message, Exception? innerException = null)
			: base(message, innerException)
		{
			NamespaceId = namespaceId;
			Hash = hash;
		}
	}

	/// <summary>
	/// Exception thrown for missing blobs
	/// </summary>
	public sealed class BlobNotFoundException : BlobException
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public BlobNotFoundException(NamespaceId namespaceId, IoHash hash, Exception? innerException = null)
			: base(namespaceId, hash, $"Unable to find blob {hash} in {namespaceId}", innerException)
		{
		}
	}

	/// <summary>
	/// Base class for ref exceptions
	/// </summary>
	public class RefException : StorageException
	{
		/// <summary>
		/// Namespace containing the ref
		/// </summary>
		public NamespaceId NamespaceId { get; }

		/// <summary>
		/// Bucket containing the ref
		/// </summary>
		public BucketId BucketId { get; }

		/// <summary>
		/// Identifier for the ref
		/// </summary>
		public RefId RefId { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		public RefException(NamespaceId namespaceId, BucketId bucketId, RefId refId, string message, Exception? innerException = null)
			: base(message, innerException)
		{
			NamespaceId = namespaceId;
			BucketId = bucketId;
			RefId = refId;
		}
	}

	/// <summary>
	/// Indicates that a named reference wasn't found
	/// </summary>
	public sealed class RefNotFoundException : RefException
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public RefNotFoundException(NamespaceId namespaceId, BucketId bucketId, RefId refId, Exception? innerException = null)
			: base(namespaceId, bucketId, refId, $"Ref {namespaceId}/{bucketId}/{refId} not found", innerException)
		{
		}
	}

	/// <summary>
	/// Indicates that a ref cannot be finalized due to a missing blob
	/// </summary>
	public sealed class RefMissingBlobException : RefException
	{
		/// <summary>
		/// Constructor
		/// </summary>
		public RefMissingBlobException(NamespaceId namespaceId, BucketId bucketId, RefId refId, List<IoHash> missingBlobs, Exception? innerException = null)
			: base(namespaceId, bucketId, refId, $"Ref {namespaceId}/{bucketId}/{refId} cannot be finalized; missing {missingBlobs.Count} blobs ({missingBlobs[0]}...)", innerException)
		{
		}
	}

	/// <summary>
	/// Interface for an object reference
	/// </summary>
	public interface IRef
	{
		/// <summary>
		/// Namespace identifier
		/// </summary>
		NamespaceId NamespaceId { get; }

		/// <summary>
		/// Bucket identifier
		/// </summary>
		BucketId BucketId { get; }

		/// <summary>
		/// Ref identifier
		/// </summary>
		RefId RefId { get; }

		/// <summary>
		/// The value stored for this ref
		/// </summary>
		CbObject Value { get; }
	}

	/// <summary>
	/// Base interface for a storage client that only records blobs.
	/// </summary>
	public interface IStorageClient
	{
		#region Blobs

		/// <summary>
		/// Opens a blob read stream
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="hash">Hash of the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the blob, or null if it does not exist</returns>
		Task<Stream> ReadBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default);
		
		/// <summary>
		/// Opens a blob read stream for compressed blobs
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="uncompressedHash">Hash of the uncompressed blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Stream for the blob, or null if it does not exist</returns>
		Task<Stream> ReadCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a blob to storage
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="hash">Hash of the blob</param>
		/// <param name="stream">The stream to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task WriteBlobAsync(NamespaceId namespaceId, IoHash hash, Stream stream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a blob to storage and calculates hash
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="stream">The stream to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Hash of the blob written</returns>
		Task<IoHash> WriteBlobAsync(NamespaceId namespaceId, Stream stream, CancellationToken cancellationToken = default);

		/// <summary>
		/// Writes a compressed blob to storage
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="uncompressedHash">Hash of the blob</param>
		/// <param name="compressedStream">Compressed stream to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		Task WriteCompressedBlobAsync(NamespaceId namespaceId, IoHash uncompressedHash, Stream compressedStream, CancellationToken cancellationToken = default);
		
		/// <summary>
		/// Writes a compressed blob to storage and calculates hash
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="compressedStream">Compressed stream to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Hash of uncompressed blob</returns>
		Task<IoHash> WriteCompressedBlobAsync(NamespaceId namespaceId, Stream compressedStream, CancellationToken cancellationToken = default);
		
		/// <summary>
		/// Checks if the given blob exists
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="hash">Hash of the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the blob exists, false if it did not exist</returns>
		Task<bool> HasBlobAsync(NamespaceId namespaceId, IoHash hash, CancellationToken cancellationToken = default);

		/// <summary>
		/// Checks if a list of blobs exist, returning the set of missing blobs
		/// </summary>
		/// <param name="namespaceId">Namespace to operate on</param>
		/// <param name="hashes">Set of hashes to check</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>A set of the missing hashes</returns>
		Task<HashSet<IoHash>> FindMissingBlobsAsync(NamespaceId namespaceId, HashSet<IoHash> hashes, CancellationToken cancellationToken = default);

		#endregion
		#region Refs

		/// <summary>
		/// Gets the given reference
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="bucketId">Bucket identifier</param>
		/// <param name="refId">Name of the reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The reference data if the ref exists</returns>
		Task<IRef> GetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Checks if the given reference exists
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="bucketId">Bucket identifier</param>
		/// <param name="refId">Ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		Task<bool> HasRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default);

		/// <summary>
		/// Determines which refs are missing
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="bucketId">Bucket identifier</param>
		/// <param name="refIds">Names of the references</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of missing references</returns>
		Task<List<RefId>> FindMissingRefsAsync(NamespaceId namespaceId, BucketId bucketId, List<RefId> refIds, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to sets the given reference, returning a list of missing objects on failure.
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="bucketId">Bucket identifier</param>
		/// <param name="refId">Ref identifier</param>
		/// <param name="value">New value for the reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of missing references</returns>
		Task<List<IoHash>> TrySetRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken = default);

		/// <summary>
		/// Attempts to finalize a reference, turning its references into hard references
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="bucketId">Bucket identifier</param>
		/// <param name="refId">Ref identifier</param>
		/// <param name="hash">Hash of the referenced object</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		Task<List<IoHash>> TryFinalizeRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash hash, CancellationToken cancellationToken = default);

		/// <summary>
		/// Removes the given reference
		/// </summary>
		/// <param name="namespaceId">Namespace identifier</param>
		/// <param name="bucketId">Bucket identifier</param>
		/// <param name="refId">Ref identifier</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref was deleted, false if it did not exist</returns>
		Task<bool> DeleteRefAsync(NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default);

		#endregion
	}

	/// <summary>
	/// Extension methods for <see cref="IStorageClient"/>
	/// </summary>
	public static class StorageClientExtensions
	{
		const int DefaultMaxInMemoryBlobLength = 128 * 1024 * 1024;

		/// <summary>
		/// Gets a blob as a byte array
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="hash">Hash of the blob to read</param>
		/// <param name="maxInMemoryBlobLength">Maximum allowed memory allocation to store the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Data for the blob that was read. Throws an exception if the blob was not found.</returns>
		public static Task<byte[]> ReadBlobToMemoryAsync(this IStorageClient storageClient, NamespaceId namespaceId, IoHash hash, int maxInMemoryBlobLength = DefaultMaxInMemoryBlobLength, CancellationToken cancellationToken = default)
		{
			return ReadBlobToMemoryAsync(storageClient, false, namespaceId, hash, maxInMemoryBlobLength, cancellationToken);
		}
		
		/// <summary>
		/// Gets a compressed blob as a byte array
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="hash">Hash of the blob to read</param>
		/// <param name="maxInMemoryBlobLength">Maximum allowed memory allocation to store the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Data for the blob that was read. Throws an exception if the blob was not found.</returns>
		public static Task<byte[]> ReadCompressedBlobToMemoryAsync(this IStorageClient storageClient, NamespaceId namespaceId, IoHash hash, int maxInMemoryBlobLength = DefaultMaxInMemoryBlobLength, CancellationToken cancellationToken = default)
		{
			return ReadBlobToMemoryAsync(storageClient, true, namespaceId, hash, maxInMemoryBlobLength, cancellationToken);
		}
		
		private static async Task<byte[]> ReadBlobToMemoryAsync(IStorageClient storageClient, bool isBlobCompressed, NamespaceId namespaceId, IoHash hash, int maxInMemoryBlobLength = DefaultMaxInMemoryBlobLength, CancellationToken cancellationToken = default)
		{
			using Stream stream = isBlobCompressed
				? await storageClient.ReadCompressedBlobAsync(namespaceId, hash, cancellationToken)
				: await storageClient.ReadBlobAsync(namespaceId, hash, cancellationToken);

			long length = stream.Length;
			if (length > maxInMemoryBlobLength)
			{
				throw new BlobException(namespaceId, hash, $"Blob {hash} is too large ({length} > {maxInMemoryBlobLength})");
			}

			byte[] buffer = new byte[length];
			for (int offset = 0; offset < length;)
			{
				int count = await stream.ReadAsync(buffer, offset, (int)length - offset, cancellationToken);
				if (count == 0)
				{
					throw new BlobException(namespaceId, hash, $"Unexpected end of stream reading blob {hash}");
				}
				offset += count;
			}

			return buffer;
		}

		/// <summary>
		/// Writes a blob from memory to storage
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="data">Data to write</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<IoHash> WriteBlobFromMemoryAsync(this IStorageClient storageClient, NamespaceId namespaceId, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default)
		{
			IoHash hash = IoHash.Compute(data.Span);
			await WriteBlobFromMemoryAsync(storageClient, namespaceId, hash, data, cancellationToken);
			return hash;
		}

		/// <summary>
		/// Writes a blob from memory to storage
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="hash">Hash of the data</param>
		/// <param name="data">The data to be written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task WriteBlobFromMemoryAsync(this IStorageClient storageClient, NamespaceId namespaceId, IoHash hash, ReadOnlyMemory<byte> data, CancellationToken cancellationToken = default)
		{
			using ReadOnlyMemoryStream stream = new ReadOnlyMemoryStream(data);
			await storageClient.WriteBlobAsync(namespaceId, hash, stream, cancellationToken);
		}

		/// <summary>
		/// Reads a blob and deserializes it as the given compact-binary encoded type
		/// </summary>
		/// <typeparam name="T">Type of object to deserialize</typeparam>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="hash">Hash of the blob to read</param>
		/// <param name="maxInMemoryBlobLength">Maximum allowed memory allocation to store the blob</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The decoded object</returns>
		public static async Task<T> ReadBlobAsync<T>(this IStorageClient storageClient, NamespaceId namespaceId, IoHash hash, int maxInMemoryBlobLength = DefaultMaxInMemoryBlobLength, CancellationToken cancellationToken = default)
		{
			ReadOnlyMemory<byte> data = await ReadBlobToMemoryAsync(storageClient, namespaceId, hash, maxInMemoryBlobLength, cancellationToken);
			return CbSerializer.Deserialize<T>(data);
		}

		/// <summary>
		/// Serializes an object to compact binary format and writes it to storage
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the blob</param>
		/// <param name="blob">The object to be written</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns></returns>
		public static async Task<IoHash> WriteBlobAsync<T>(this IStorageClient storageClient, NamespaceId namespaceId, T blob, CancellationToken cancellationToken = default)
		{
			CbWriter writer = new CbWriter();
			CbSerializer.Serialize<T>(writer, blob);

			IoHash hash = writer.ComputeHash();
			await storageClient.WriteBlobAsync(namespaceId, hash, writer.AsStream(), cancellationToken);
			return hash;
		}

		/// <summary>
		/// Gets the given reference
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="qualifiedRefId">Name of the reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>The reference data if the ref exists</returns>
		public static Task<IRef> GetRefAsync(this IStorageClient storageClient, QualifiedRefId qualifiedRefId, CancellationToken cancellationToken = default)
		{
			return storageClient.GetRefAsync(qualifiedRefId.NamespaceId, qualifiedRefId.BucketId, qualifiedRefId.RefId, cancellationToken);
		}

		/// <summary>
		/// Checks if the given reference exists
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="qualifiedRefId">Name of the reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref exists, false if it did not exist</returns>
		public static Task<bool> HasRefAsync(this IStorageClient storageClient, QualifiedRefId qualifiedRefId, CancellationToken cancellationToken = default)
		{
			return storageClient.HasRefAsync(qualifiedRefId.NamespaceId, qualifiedRefId.BucketId, qualifiedRefId.RefId, cancellationToken);
		}

		/// <summary>
		/// Attempts to sets the given reference, returning a list of missing objects on failure.
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="qualifiedRefId">Name of the reference</param>
		/// <param name="value">New value for the reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of missing references</returns>
		public static Task<List<IoHash>> TrySetRefAsync(this IStorageClient storageClient, QualifiedRefId qualifiedRefId, CbObject value, CancellationToken cancellationToken = default)
		{
			return storageClient.TrySetRefAsync(qualifiedRefId.NamespaceId, qualifiedRefId.BucketId, qualifiedRefId.RefId, value, cancellationToken);
		}

		/// <summary>
		/// Reads a reference as a specific type
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="bucketId">Bucket containing the ref</param>
		/// <param name="refId">The ref id</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Deserialized object for the given ref</returns>
		public static async Task<T> GetRefAsync<T>(this IStorageClient storageClient, NamespaceId namespaceId, BucketId bucketId, RefId refId, CancellationToken cancellationToken = default)
		{
			IRef item = await storageClient.GetRefAsync(namespaceId, bucketId, refId, cancellationToken);
			return CbSerializer.Deserialize<T>(item.Value);
		}

		/// <summary>
		/// Sets a ref to a particular value
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="bucketId">Bucket containing the ref</param>
		/// <param name="refId">The ref id</param>
		/// <param name="value">The new object for the ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static Task SetRefAsync<T>(this IStorageClient storageClient, NamespaceId namespaceId, BucketId bucketId, RefId refId, T value, CancellationToken cancellationToken = default)
		{
			CbObject objectValue = CbSerializer.Serialize<T>(value);
			return SetRefAsync(storageClient, namespaceId, bucketId, refId, objectValue, cancellationToken);
		}

		/// <summary>
		/// Sets a ref to a particular value
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="bucketId">Bucket containing the ref</param>
		/// <param name="refId">The ref id</param>
		/// <param name="value">The new object for the ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task SetRefAsync(this IStorageClient storageClient, NamespaceId namespaceId, BucketId bucketId, RefId refId, CbObject value, CancellationToken cancellationToken = default)
		{
			List<IoHash> missingHashes = await storageClient.TrySetRefAsync(namespaceId, bucketId, refId, value, cancellationToken);
			if (missingHashes.Count > 0)
			{
				throw new RefMissingBlobException(namespaceId, bucketId, refId, missingHashes);
			}
		}

		/// <summary>
		/// Attempts to set a ref to a particular value
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="bucketId">Bucket containing the ref</param>
		/// <param name="refId">The ref id</param>
		/// <param name="value">The new object for the ref</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>List of missing blob hashes</returns>
		public static Task<List<IoHash>> TrySetRefAsync<T>(this IStorageClient storageClient, NamespaceId namespaceId, BucketId bucketId, RefId refId, T value, CancellationToken cancellationToken = default)
		{
			CbObject objectValue = CbSerializer.Serialize<T>(value);
			return storageClient.TrySetRefAsync(namespaceId, bucketId, refId, objectValue, cancellationToken);
		}

		/// <summary>
		/// Finalize a ref, throwing an exception if finalization fails
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="namespaceId">Namespace containing the ref</param>
		/// <param name="bucketId">Bucket containing the ref</param>
		/// <param name="refId">The ref id</param>
		/// <param name="valueHash">Hash of the ref value</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		public static async Task FinalizeRefAsync(this IStorageClient storageClient, NamespaceId namespaceId, BucketId bucketId, RefId refId, IoHash valueHash, CancellationToken cancellationToken = default)
		{
			List<IoHash> missingHashes = await storageClient.TryFinalizeRefAsync(namespaceId, bucketId, refId, valueHash, cancellationToken);
			if (missingHashes.Count > 0)
			{
				throw new RefMissingBlobException(namespaceId, bucketId, refId, missingHashes);
			}
		}

		/// <summary>
		/// Removes the given reference
		/// </summary>
		/// <param name="storageClient">The storage interface</param>
		/// <param name="qualifiedRefId">Name of the reference</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>True if the ref was deleted, false if it did not exist</returns>
		public static Task<bool> DeleteRefAsync(this IStorageClient storageClient, QualifiedRefId qualifiedRefId, CancellationToken cancellationToken = default)
		{
			return storageClient.DeleteRefAsync(qualifiedRefId.NamespaceId, qualifiedRefId.BucketId, qualifiedRefId.RefId, cancellationToken);
		}
	}
}
