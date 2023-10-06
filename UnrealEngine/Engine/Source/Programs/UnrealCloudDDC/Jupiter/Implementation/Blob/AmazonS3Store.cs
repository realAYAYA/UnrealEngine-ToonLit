// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Net;
using System.Threading.Tasks;
using Amazon.S3;
using Amazon.S3.Model;
using EpicGames.Horde.Storage;
using Jupiter.Implementation.Blob;
using Jupiter.Common;
using Microsoft.Extensions.Options;
using KeyNotFoundException = System.Collections.Generic.KeyNotFoundException;
using System.Threading;
using System.Runtime.CompilerServices;
using System.Collections.Concurrent;
using Microsoft.Extensions.Logging;
using Microsoft.Extensions.DependencyInjection;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
    public class AmazonS3Store : IBlobStore
    {
        private readonly IAmazonS3 _amazonS3;
        private readonly IBlobIndex _blobIndex;
        private readonly INamespacePolicyResolver _namespacePolicyResolver;
        private readonly ILogger<AmazonS3Store> _logger;
        private readonly IServiceProvider _provider;
        private readonly S3Settings _settings;
        private readonly ConcurrentDictionary<NamespaceId, AmazonStorageBackend> _backends = new ConcurrentDictionary<NamespaceId, AmazonStorageBackend>();

        public AmazonS3Store(IAmazonS3 amazonS3, IOptionsMonitor<S3Settings> settings, IBlobIndex blobIndex, INamespacePolicyResolver namespacePolicyResolver, ILogger<AmazonS3Store> logger, IServiceProvider provider)
        {
            _amazonS3 = amazonS3;
            _blobIndex = blobIndex;
            _namespacePolicyResolver = namespacePolicyResolver;
            _logger = logger;
            _provider = provider;
            _settings = settings.CurrentValue;
        }

        AmazonStorageBackend GetBackend(NamespaceId ns)
        {
            return _backends.GetOrAdd(ns, x => ActivatorUtilities.CreateInstance<AmazonStorageBackend>(_provider, GetBucketName(x)));
        }

        public async Task<Uri?> GetObjectByRedirect(NamespaceId ns, BlobIdentifier identifier)
        {
            Uri? uri = await GetBackend(ns).GetReadRedirectAsync(identifier.AsS3Key());

            return uri;
        }

        public async Task<Uri?> PutObjectWithRedirect(NamespaceId ns, BlobIdentifier identifier)
        {
            Uri? uri = await GetBackend(ns).GetWriteRedirectAsync(identifier.AsS3Key());

            return uri;
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, ReadOnlyMemory<byte> content, BlobIdentifier objectName)
        {
            await using MemoryStream stream = new MemoryStream(content.ToArray());
            return await PutObject(ns, stream, objectName);
        }

        public async Task<BlobIdentifier> PutObject(NamespaceId ns, Stream stream, BlobIdentifier objectName)
        {
            await GetBackend(ns).WriteAsync(objectName.AsS3Key(), stream, CancellationToken.None);
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
                NamespacePolicy policy = _namespacePolicyResolver.GetPoliciesForNs(ns);
                string storagePool = policy.StoragePool;
                // if the bucket to use for the storage pool has been overriden we use the override
                if (_settings.StoragePoolBucketOverride.TryGetValue(storagePool, out string? containerOverride))
                {
                    return containerOverride;
                }
                // by default we use the storage pool as a suffix to determine the bucket for that pool
                string storagePoolSuffix = string.IsNullOrEmpty(storagePool) ? "" : $"-{storagePool}";
                return $"{_settings.BucketName}{storagePoolSuffix}";
            }
            catch (KeyNotFoundException)
            {
                throw new NamespaceNotFoundException(ns);
            }
        }

        public async Task<BlobContents> GetObject(NamespaceId ns, BlobIdentifier blob, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, bool supportsRedirectUri = false)
        {
            NamespacePolicy policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
            try
            {
                if (supportsRedirectUri && policies.AllowRedirectUris)
                {
                    Uri? redirectUri = await GetBackend(ns).GetReadRedirectAsync(blob.AsS3Key());
                    if (redirectUri != null)
                    {
                        return new BlobContents(redirectUri);
                    }
                }
            
                BlobContents? contents = await GetBackend(ns).TryReadAsync(blob.AsS3Key(), flags);
                if (contents == null)
                {
                    throw new BlobNotFoundException(ns, blob);
                }
                return contents;
            }
            catch (AmazonS3Exception e)
            {
                // log information about the failed request except for 404 as its valid to not find objects in S3
                if (e.StatusCode != HttpStatusCode.NotFound)
                {
                    _logger.LogWarning("Exception raised from S3 {Exception}. {RequestId} {Id}", e, e.RequestId, e.AmazonId2);
                }

                // rethrow the exception, we just wanted to log more information about the failed request for further debugging
                throw;
            }
        }

        public async Task<bool> Exists(NamespaceId ns, BlobIdentifier blobIdentifier, bool forceCheck)
        {
            NamespacePolicy policies = _namespacePolicyResolver.GetPoliciesForNs(ns);
            if (_settings.UseBlobIndexForExistsCheck && policies.UseBlobIndexForSlowExists && !forceCheck)
            {
                return await _blobIndex.BlobExistsInRegion(ns, blobIdentifier);
            }
            else
            {
                return await GetBackend(ns).ExistsAsync(blobIdentifier.AsS3Key(), CancellationToken.None);
            }
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
            IStorageBackend backend = GetBackend(ns);
            await foreach ((string path, DateTime time) in backend.ListAsync())
            {
                string identifierString = path.Substring(path.LastIndexOf("/", StringComparison.Ordinal) + 1);
                yield return (new BlobIdentifier(identifierString), time);
            }
        }

        public async Task DeleteObject(NamespaceId ns, BlobIdentifier blobIdentifier)
        {
            IStorageBackend backend = GetBackend(ns);
            await backend.DeleteAsync(blobIdentifier.AsS3Key());
        }
    }

    public class AmazonStorageBackend : IStorageBackend
    {
        private readonly IAmazonS3 _amazonS3;
        private readonly string _bucketName;
        private readonly IOptionsMonitor<S3Settings> _settings;
        private readonly Tracer _tracer;
        private readonly ILogger<AmazonStorageBackend> _logger;
        private bool _bucketExistenceChecked;
        private bool _bucketAccessPolicyApplied;

        public AmazonStorageBackend(IAmazonS3 amazonS3, string bucketName, IOptionsMonitor<S3Settings> settings, Tracer tracer, ILogger<AmazonStorageBackend> logger)
        {
            _amazonS3 = amazonS3;
            _bucketName = bucketName;
            _settings = settings;
            _tracer = tracer;
            _logger = logger;
        }

        public async Task WriteAsync(string path, Stream stream, CancellationToken cancellationToken)
        {
            if (_settings.CurrentValue.CreateBucketIfMissing)
            {
                if (!_bucketExistenceChecked)
                {
                    bool bucketExist = await _amazonS3.DoesS3BucketExistAsync(_bucketName);
                    if (!bucketExist)
                    {
                        PutBucketRequest putBucketRequest = new PutBucketRequest
                        {
                            BucketName = _bucketName,
                            UseClientRegion = true
                        };

                        await _amazonS3.PutBucketAsync(putBucketRequest, cancellationToken);
                    }
                    _bucketExistenceChecked = true;
                }
            }

            if (_settings.CurrentValue.SetBucketPolicies && !_bucketAccessPolicyApplied)
            {
                // block all public access to the bucket
                try
                {
                    await _amazonS3.PutPublicAccessBlockAsync(new PutPublicAccessBlockRequest
                    {
                        BucketName = _bucketName,
                        PublicAccessBlockConfiguration = new PublicAccessBlockConfiguration()
                        {
                            RestrictPublicBuckets = true,
                            BlockPublicAcls = true,
                            BlockPublicPolicy = true,
                            IgnorePublicAcls = true,
                        }
                    }, cancellationToken);

                    _bucketAccessPolicyApplied = true;
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
                BucketName = _bucketName,
                Key = path,
                InputStream = stream
            };

            try
            {
                await _amazonS3.PutObjectAsync(request, cancellationToken);
            }
            catch (AmazonS3Exception e)
            {
                // if the same object is added twice S3 will raise a error, as we are content addressed we can just accept whichever of the objects so we can ignore that error
                if (e.StatusCode == HttpStatusCode.Conflict)
                {
                    return;
                }

                if (e.StatusCode == HttpStatusCode.TooManyRequests)
                {
                    throw new ResourceHasToManyRequestsException(e);
                }

                throw;
            }
        }

        public async Task<BlobContents?> TryReadAsync(string path, LastAccessTrackingFlags flags = LastAccessTrackingFlags.DoTracking, CancellationToken cancellationToken = default)
        {
            GetObjectResponse response;
            try
            {
                response = await _amazonS3.GetObjectAsync(_bucketName, path, cancellationToken);
            }
            catch (AmazonS3Exception e)
            {
                if (e.ErrorCode == "NoSuchKey")
                {
                    return null;
                }

                if (e.ErrorCode == "NoSuchBucket")
                {
                    return null;
                }
                throw;
            }
            return new BlobContents(response.ResponseStream, response.ContentLength);
        }

        public async Task<bool> ExistsAsync(string path, CancellationToken cancellationToken)
        {
            try
            {
                await _amazonS3.GetObjectMetadataAsync(_bucketName, path, cancellationToken);
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

        public async IAsyncEnumerable<(string, DateTime)> ListAsync([EnumeratorCancellation] CancellationToken cancellationToken)
        {
            ListObjectsV2Request request = new ListObjectsV2Request
            {
                BucketName = _bucketName
            };

            if (!await _amazonS3.DoesS3BucketExistAsync(_bucketName))
            {
                yield break;
            }

            ListObjectsV2Response response;
            do
            {
                response = await _amazonS3.ListObjectsV2Async(request, cancellationToken);
                foreach (S3Object obj in response.S3Objects)
                {
                    yield return (obj.Key, obj.LastModified);
                }

                request.ContinuationToken = response.NextContinuationToken;
            } while (response.IsTruncated);

        }

        public async Task DeleteAsync(string path, CancellationToken cancellationToken)
        {
            await _amazonS3.DeleteObjectAsync(_bucketName, path, cancellationToken);
        }

        public ValueTask<Uri?> GetReadRedirectAsync(string path)
        {
            return new ValueTask<Uri?>(GetPresignedUrl(path, HttpVerb.GET));
        }

        public ValueTask<Uri?> GetWriteRedirectAsync(string path)
        {
            return new ValueTask<Uri?>(GetPresignedUrl(path, HttpVerb.PUT));
        }

        /// <summary>
        /// Helper method to generate a presigned URL for a request
        /// </summary>
        Uri? GetPresignedUrl(string path, HttpVerb verb)
        {
            using TelemetrySpan span = _tracer.StartActiveSpan("s3.BuildPresignedUrl")
                .SetAttribute("Path", path)
            ;

            try
            {
                GetPreSignedUrlRequest newGetRequest = new GetPreSignedUrlRequest();
                newGetRequest.BucketName = _bucketName;
                newGetRequest.Key = path;
                newGetRequest.Verb = verb;
                newGetRequest.Protocol = _settings.CurrentValue.AssumeHttpForRedirectUri ? Protocol.HTTP : Protocol.HTTPS;
                newGetRequest.Expires = DateTime.UtcNow.AddHours(3.0);

                string url = _amazonS3.GetPreSignedURL(newGetRequest);

                return new Uri(url);
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Unable to get presigned url for {Path} from S3", path);
                return null;
            }
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
