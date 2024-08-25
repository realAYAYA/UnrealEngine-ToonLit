// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation.Blob
{
	public class BucketStats
	{
		public NamespaceId Namespace { get; set; }
		public BucketId Bucket { get; set; }
		public long CountOfRefs { get; set; }
		public long CountOfBlobs { get; set; }
		public long TotalSize { get; set; }
		public double AvgSize { get; set; }
		public long LargestBlob { get; set; }
		public long SmallestBlobFound { get; set; }
	}

	public interface IBlobIndex
	{
		Task AddBlobToIndexAsync(NamespaceId ns, BlobId id, string? region = null);

		Task RemoveBlobFromRegionAsync(NamespaceId ns, BlobId id, string? region = null);

		Task<bool> BlobExistsInRegionAsync(NamespaceId ns, BlobId blobIdentifier, string? region = null);
		IAsyncEnumerable<(NamespaceId, BlobId)> GetAllBlobsAsync();

		IAsyncEnumerable<BaseBlobReference> GetBlobReferencesAsync(NamespaceId ns, BlobId id);
		Task AddRefToBlobsAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId[] blobs);

		Task RemoveReferencesAsync(NamespaceId ns, BlobId id, List<BaseBlobReference>? referencesToRemove);
		Task<List<string>> GetBlobRegionsAsync(NamespaceId ns, BlobId blob);
		Task AddBlobReferencesAsync(NamespaceId ns, BlobId sourceBlob, BlobId targetBlob);
		
		Task AddBlobToBucketListAsync(NamespaceId ns, BucketId bucket, RefId key, BlobId blobId, long blobSize);
		Task RemoveBlobFromBucketListAsync(NamespaceId ns, BucketId bucket, RefId key, List<BlobId> blobIds);
		Task<BucketStats> CalculateBucketStatisticsAsync(NamespaceId ns, BucketId bucket);
	}

	public abstract class BaseBlobReference
	{

	}

	public class RefBlobReference : BaseBlobReference
	{
		public RefBlobReference(BucketId bucket, RefId key)
		{
			Bucket = bucket;
			Key = key;
		}

		public BucketId Bucket { get; set; }
		public RefId Key { get; set;}
	}

	public class BlobToBlobReference : BaseBlobReference
	{
		public BlobToBlobReference(BlobId blob)
		{
			Blob = blob;
		}

		public BlobId Blob { get; set; }
	}
}
