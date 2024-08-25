// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;

namespace Jupiter.Implementation
{
	public interface IRefService
	{
		Task<(RefRecord, BlobContents?)> GetAsync(NamespaceId ns, BucketId bucket, RefId key, string[] fields, bool doLastAccessTracking = true);
		Task<(ContentId[], BlobId[])> PutAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash, CbObject payload);
		Task<(ContentId[], BlobId[])> FinalizeAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobHash);

		IAsyncEnumerable<NamespaceId> GetNamespacesAsync();

		Task<bool> DeleteAsync(NamespaceId ns, BucketId bucket, RefId key);
		Task<long> DropNamespaceAsync(NamespaceId ns);
		Task<long> DeleteBucketAsync(NamespaceId ns, BucketId bucket);

		Task<bool> ExistsAsync(NamespaceId ns, BucketId bucket, RefId key);
		Task<List<BlobId>> GetReferencedBlobsAsync(NamespaceId ns, BucketId bucket, RefId key, bool ignoreMissingBlobs);
	}

	public class ObjectHashMismatchException : Exception
	{
		public ObjectHashMismatchException(NamespaceId ns, BucketId bucket, RefId name, BlobId suppliedHash, BlobId actualHash) : base($"Object {name} in bucket {bucket} and namespace {ns} did not reference hash {suppliedHash} was referencing {actualHash}")
		{
		}
	}
}
