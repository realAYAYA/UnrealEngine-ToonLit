// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using EpicGames.Horde.Storage;
using Horde.Storage.Implementation.Blob;
using Jupiter;
using Jupiter.Common;
using Jupiter.Common.Implementation;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using KeyNotFoundException = System.Collections.Generic.KeyNotFoundException;

namespace Horde.Storage.Implementation
{
    public class AmazonS3Store : IBlobStore
    {
        private readonly IAmazonS3 _amazonS3;
        private readonly IBlobIndex _blobIndex;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly S3Settings _settings;
        private readonly HashSet<string> _bucketAccessPolicyApplied = new HashSet<string>();
        private readonly HashSet<string> _bucketExistenceChecked = new HashSet<string>();

        public AmazonS3Store(IAmazonS3 amazonS3, IOptionsMonitor<S3Settings> settings, IBlobIndex blobIndex, INamespacePolicyResolver namespacePolicyResolver)
        {
            _amazonS3 = amazonS3;
            _blobIndex = blobIndex;
            _namespacePolicyResolver = namespacePolicyResolver;
            _settings = settings.CurrentValue;
        }

        // interact with a S3 compatible object store
        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> content, BlobIdentifier objectName)
        {
            await using MemoryStream stream = new MemoryStream(content.ToArray());
            return await PutObject(ns, stream, objectName);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream stream, BlobIdentifier objectName)
        {
            string bucketName = GetBucketName(ns);
            if (_settings.CreateBucketIfMissing)
            {
                if (!_bucketExistenceChecked.Contains(bucketName))
                {
                    bool bucketExist = await _amazonS3.DoesS3BucketExistAsync(bucketName);
                    if (!bucketExist)
                    {
                        PutBucketRequest putBucketRequest = new PutBucketRequest
                        {
                            BucketName = bucketName,
                            UseClientRegion = true
                        };

                        await _amazonS3.PutBucketAsync(putBucketRequest);
                    }
                    _bucketExistenceChecked.Add(bucketName);
                }
            }

            if (_settings.SetBucketPolicies && !_bucketAccessPolicyApplied.Contains(bucketName))
            {
                // block all public access to the bucket
                try
                {
                    await _amazonS3.PutPublicAccessBlockAsync(new PutPublicAccessBlockRequest
                    {
                        BucketName = bucketName,
                        PublicAccessBlockConfiguration = new PublicAccessBlockConfiguration()
                        {
                            RestrictPublicBuckets = true,
                            BlockPublicAcls = true,
                            BlockPublicPolicy = true,
                            IgnorePublicAcls = true,
                        }
                    });

                    _bucketAccessPolicyApplied.Add(bucketName);
                }
                catch (AmazonS3Exception e)
                {
                    // if a conflicting operation is being applied to the public access block we just ignore it, as it will get reset the next time we run
                    if (e.StatusCode != HttpStatusCode.Conflict)
                    {
                        throw;
                    }
                }
            }
            PutObjectRequest request = new PutObjectRequest
            {
                BucketName = bucketName,
                Key = objectName.AsS3Key(),
                InputStream = new InstrumentedStream(stream, "s3-put"),
            };

            try
            {
                await _amazonS3.PutObjectAsync(request);
            }
            catch (AmazonS3Exception e)
            {
                // if the same object is added twice S3 will raise a error, as we are content addressed we can just accept whichever of the objects so we can ignore that error
                if (e.StatusCode == HttpStatusCode.Conflict)
                {
                    return objectName;
                }

                if (e.StatusCode == HttpStatusCode.TooManyRequests)
                {
                    throw new ResourceHasToManyRequestsException(e);
                }

                throw;
            }

            return objectName;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, byte[] content, BlobIdentifier objectName)
        {
            await using MemoryStream stream = new MemoryStream(content);
            return await PutObject(ns, stream, objectName);
        }

        private string GetBucketName(NamespaceId ns)
        {
            try
            {
                string storagePool = _namespacePolicyResolver.GetPoliciesForNs(ns).StoragePool;
                string storagePoolSuffix = string.IsNullOrEmpty(storagePool) ? "" : $"-{storagePool}";
                return $"{_settings.BucketName}{storagePoolSuffix}";
            }
            catch (KeyNotFoundException)
            {
                throw new NamespaceNotFoundException(ns);
            }
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking)
        {
            string bucketName = GetBucketName(ns);
            GetObjectResponse response;
            try
            {
                response = await _amazonS3.GetObjectAsync(bucketName, blob.AsS3Key());
            }
            catch (AmazonS3Exception e)
            {
                if (e.ErrorCode == "NoSuchKey")
                {
                    throw new BlobNotFoundException(ns, blob);
                }

                if (e.ErrorCode == "NoSuchBucket")
                {
                    throw new NamespaceNotFoundException(ns);
                }
                throw;
            }
            return new BlobContents(new InstrumentedStream(response.ResponseStream, "s3-get"), response.ContentLength);
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blobIdentifier, bool forceCheck)
        {
            NamespacePolicy policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
            if (_settings.UseBlobIndexForExistsCheck && policies.UseBlobIndexForSlowExists && !forceCheck)
            {
                return await _blobIndex.BlobExistsInRegion(ns, blobIdentifier);
            }

            string bucketName = GetBucketName(ns);
            try
            {
                await _amazonS3.GetObjectMetadataAsync(bucketName, blobIdentifier.AsS3Key());
            }
            catch (AmazonS3Exception e)
            {
                // if the object does not exist we get a not found status code
                if (e.StatusCode == HttpStatusCode.NotFound)
                {
                    return false;
                }

                // something else happened, lets just process it as usual
            }

            return true;
        }

        public async Task DeleteNamespace(NamespaceId ns)
        {
            string bucketName = GetBucketName(ns);
            try
            {
                await _amazonS3.DeleteBucketAsync(bucketName);
            }
            catch (AmazonS3Exception e)
            {
                // if the bucket does not exist we get a not found status code
                if (e.StatusCode == HttpStatusCode.NotFound)
                {
                    // deleting a none existent bucket is a success
                    return;
                }

                // something else happened, lets just process it as usual
            }
        }

        public async IAsyncEnumerable<(BlobIdentifier, DateTime)> ListObjects(NamespaceId ns)
        {
            ListObjectsV2Request request = new ListObjectsV2Request
            {
                BucketName = GetBucketName(ns)
            };

            ListObjectsV2Response response;
            do
            {
                response = await _amazonS3.ListObjectsV2Async(request);
                foreach (S3Object obj in response.S3Objects)
                {
                    string key = obj.Key;
                    string identifierString = key.Substring(key.LastIndexOf("/", StringComparison.Ordinal)+1);

                    yield return (new BlobIdentifier(identifierString), obj.LastModified);
                }

                request.ContinuationToken = response.NextContinuationToken;
            } while (response.IsTruncated);

        }

        public async Task DeleteObject(NamespaceId ns, BlobIdentifier blobIdentifier)
        {
            string bucketName = GetBucketName(ns);
            await _amazonS3.DeleteObjectAsync(bucketName, blobIdentifier.AsS3Key());
        }
    }

    public static class BlobIdentifierExtensions
    {
        public static string AsS3Key(this BlobIdentifier blobIdentifier)
        {
            string s = blobIdentifier.ToString();
            string prefix = s.Substring(0, 4);
            return $"{prefix}/{s}";
        }
    }
}
