// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation
{
	public interface IReferencesStore
	{
		Task<RefRecord> GetAsync(NamespaceId ns, BucketId bucket, RefId key, FieldFlags fieldFlags, OperationFlags opFlags );

		[Flags]
		public enum FieldFlags
		{
			None = 0,
			IncludePayload = 1,
			All = IncludePayload
		}

		[Flags]
		public enum OperationFlags
		{
			None = 0,
			BypassCache = 1
		}

		Task PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, byte[] blob, bool isFinalized);
		Task FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobIdentifier);

		Task<DateTime?> GetLastAccessTimeAsync(NamespaceId ns, BucketId bucket, RefId key);
		Task UpdateLastAccessTimeAsync(NamespaceId ns, BucketId bucket, RefId key, DateTime newLastAccessTime);
		IAsyncEnumerable<(NamespaceId, BucketId, RefId, DateTime)> GetRecordsAsync();

		IAsyncEnumerable<(NamespaceId, BucketId, RefId)> GetRecordsWithoutAccessTimeAsync();

		IAsyncEnumerable<(RefId, BlobId)> GetRecordsInBucketAsync(NamespaceId ns, BucketId bucket);

		IAsyncEnumerable<NamespaceId> GetNamespacesAsync();
		IAsyncEnumerable<BucketId> GetBuckets(NamespaceId ns);
		Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key);
		Task<long> DropNamespaceAsync(NamespaceId ns);
		Task<long> DeleteBucketAsync(NamespaceId ns, BucketId bucket);
	}

	public class RefRecord
	{
		public RefRecord(NamespaceId ns, BucketId bucket, RefId name, DateTime lastAccess, byte[]? inlinePayload, BlobId blobIdentifier, bool isFinalized)
		{
			Namespace = ns;
			Bucket = bucket;
			Name = name;
			LastAccess = lastAccess;
			InlinePayload = inlinePayload;
			BlobIdentifier = blobIdentifier;
			IsFinalized = isFinalized;
		}

		public NamespaceId Namespace { get; }
		public BucketId Bucket { get; }
		public RefId Name { get; }
		public DateTime LastAccess { get; }
		public byte[]? InlinePayload { get; set; }
		public BlobId BlobIdentifier { get; set; }
		public bool IsFinalized {get;}
	}

	public class RefNotFoundException : Exception
	{
		public RefNotFoundException(NamespaceId ns, BucketId bucket, RefId key) : base($"Object not found {key} in bucket {bucket} namespace {ns}")
		{
			Namespace = ns;
			Bucket = bucket;
			Key = key;
		}

		public NamespaceId Namespace { get; }
		public BucketId Bucket { get; }
		public RefId Key { get; }
	}
}
