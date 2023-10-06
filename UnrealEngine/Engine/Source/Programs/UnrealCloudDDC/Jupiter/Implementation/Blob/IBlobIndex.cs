// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;

namespace Jupiter.Implementation.Blob
{

    public interface IBlobIndex
    {
        Task AddBlobToIndex(NamespaceId ns, BlobIdentifier id, string? region = null);

        Task RemoveBlobFromRegion(NamespaceId ns, BlobIdentifier id, string? region = null);

        Task<bool> BlobExistsInRegion(NamespaceId ns, BlobIdentifier blobIdentifier, string? region = null);
        IAsyncEnumerable<(NamespaceId, BlobIdentifier)> GetAllBlobs();

        IAsyncEnumerable<BaseBlobReference> GetBlobReferences(NamespaceId ns, BlobIdentifier id);
        Task AddRefToBlobs(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier[] blobs);

        Task RemoveReferences(NamespaceId ns, BlobIdentifier id, List<BaseBlobReference> referencesToRemove);
        Task<List<string>> GetBlobRegions(NamespaceId ns, BlobIdentifier blob);
        Task AddBlobReferences(NamespaceId ns, BlobIdentifier sourceBlob, BlobIdentifier targetBlob);
    }

    public abstract class BaseBlobReference
    {

    }

    public class RefBlobReference : BaseBlobReference
    {
        public RefBlobReference(BucketId bucket, IoHashKey key)
        {
            Bucket = bucket;
            Key = key;
        }

        public BucketId Bucket { get; set; }
        public IoHashKey Key { get; set;}
    }

    public class BlobToBlobReference : BaseBlobReference
    {
        public BlobToBlobReference(BlobIdentifier blob)
        {
            Blob = blob;
        }

        public BlobIdentifier Blob { get; set; }
    }
}
