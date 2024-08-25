// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation
{
	public enum LastAccessTrackingFlags
	{
		DoTracking = 0,
		SkipTracking = 1,
	}

	public interface IBlobStore
	{
		Task<BlobId> PutObjectAsync(NamespaceId ns, byte[] blob, BlobId identifier);
		Task<BlobId> PutObjectAsync(NamespaceId ns, ReadOnlyMemory<byte> blob, BlobId identifier);
		Task<BlobId> PutObjectAsync(NamespaceId ns, Stream content, BlobId identifier);

		Task<BlobContents> GetObjectAsync(NamespaceId ns, BlobId blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, bool supportsRedirectUri = false);
		Task<bool> ExistsAsync(NamespaceId ns, BlobId blob, bool forceCheck = false);

		// Delete a object
		Task DeleteObjectAsync(NamespaceId ns, BlobId blob);

		// delete the whole namespace
		Task DeleteNamespaceAsync(NamespaceId ns);

		IAsyncEnumerable<(BlobId,DateTime)> ListObjectsAsync(NamespaceId ns);
		Task<Uri?> PutObjectWithRedirectAsync(NamespaceId ns, BlobId identifier);
		Task<Uri?> GetObjectByRedirectAsync(NamespaceId ns, BlobId blob);
		Task<BlobMetadata> GetObjectMetadataAsync(NamespaceId ns, BlobId blobId);
	}

	public interface IStorageBackend
	{
		Task WriteAsync(string path, Stream content, CancellationToken cancellationToken = default);
		Task<BlobContents?> TryReadAsync(string path, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, CancellationToken cancellationToken = default);
		Task<bool> ExistsAsync(string path, CancellationToken cancellationToken = default);
		Task DeleteAsync(string path, CancellationToken cancellationToken = default);
		IAsyncEnumerable<(string, DateTime)> ListAsync(CancellationToken cancellationToken = default);
	}

	public class BlobNotFoundException : Exception
	{
		public NamespaceId Ns { get; }
		public BlobId Blob { get; }

		public BlobNotFoundException(NamespaceId ns, BlobId blob) : base($"No Blob in Namespace {ns} with id {blob}")
		{
			Ns = ns;
			Blob = blob;
		}

		public BlobNotFoundException(NamespaceId ns, BlobId blob, string message) : base(message)
		{
			Ns = ns;
			Blob = blob;
		}
	}

	public class BlobReplicationException : BlobNotFoundException
	{
		public BlobReplicationException(NamespaceId ns, BlobId blob, string message) : base(ns, blob, message)
		{
		}
	}

	public class BlobToLargeException : Exception
	{
		public BlobId Blob { get; }

		public BlobToLargeException(BlobId blob) : base($"Blob {blob} was to large to cache")
		{
			Blob = blob;
		}
	}

	public class ResourceHasToManyRequestsException : Exception
	{
		public ResourceHasToManyRequestsException(Exception originalException) : base($"To many requests to resource", originalException)
		{
		}
	}
}
