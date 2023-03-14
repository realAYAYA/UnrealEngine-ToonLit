// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Storage.Controllers;
using Jupiter.Implementation;
using Microsoft.Extensions.Options;
using Serilog;

namespace Horde.Storage.Implementation
{
    public class RefResponse
    {
        public RefResponse()
        {
            Name = null!;
            LastAccessTime = null!;
            Metadata = null!;
            ContentHash = null!;
            BlobIdentifiers = null!;
        }

        public RefResponse(string name, DateTime? lastAccessTime, ContentHash contentHash, BlobIdentifier[] blobIdentifiers, Dictionary<string, object>? metadata)
        {
            Name = name;
            LastAccessTime = lastAccessTime;
            Metadata = metadata;
            ContentHash = contentHash;
            BlobIdentifiers = blobIdentifiers;
        }

        [CbField("name")]
        public string Name { get; set; }

        [CbField("lastAccessTime")]
        public DateTime? LastAccessTime { get; set; }

        [System.Diagnostics.CodeAnalysis.SuppressMessage("Usage", "CA2227:Collection properties should be read only", Justification = "Required by serialization")]
        public Dictionary<string, object>? Metadata { get; set; }

        [CbField("contentHash")]
        public ContentHash ContentHash { get; set; }

        [CbField("blobIdentifiers")]
        public BlobIdentifier[] BlobIdentifiers { get; set; }

        [CbField("blob")]
        public byte[]? Blob { get; set; }
    }

    public interface IDDCRefService
    {
        Task<(RefResponse, BlobContents?)> Get(NamespaceId ns, BucketId bucket, KeyId key, string[] fields);

        Task<RefRecord> Exists(NamespaceId ns, BucketId bucket, KeyId key);

        Task<PutRequestResponse> Put(NamespaceId ns, BucketId bucket, KeyId key, ContentHash blobHash, byte[] blob);

        /// <summary>
        /// Adds a ref key for which all blobs are already expected to be uploaded
        /// </summary>
        /// <param name="ns"></param>
        /// <param name="bucket"></param>
        /// <param name="key"></param>
        /// <param name="refRequest"></param>
        /// <returns></returns>
        Task<long> PutIndirect(NamespaceId ns, BucketId bucket, KeyId key, RefRequest refRequest);

        Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key);
    }

    public class DDCRefService : IDDCRefService
    {
        private readonly IBlobService _blobStore;
        private readonly IRefsStore _refsStore;
        private readonly ITransactionLogWriter _transactionLog;
        private readonly ILastAccessTracker<RefRecord> _lastAccessTracker;
        private readonly IDiagnosticContext _diagnosticContext;
        private readonly HordeStorageSettings _settings;
        private readonly ILogger _logger = Log.ForContext<DDCRefService>();

        public DDCRefService(IBlobService blobStore, IRefsStore refsStore, ITransactionLogWriter transactionLog, ILastAccessTracker<RefRecord> lastAccessTracker,
            IDiagnosticContext diagnosticContext, IOptionsMonitor<HordeStorageSettings> settings)
        {
            _blobStore = blobStore;
            _refsStore = refsStore;
            _transactionLog = transactionLog;
            _lastAccessTracker = lastAccessTracker;
            _diagnosticContext = diagnosticContext;
            _settings = settings.CurrentValue;
        }

        private static bool HasFieldFilter(string[] fields)
        {
            return fields.Length != 0 && fields.Any(field => !string.IsNullOrEmpty(field));
        }

        public async Task<(RefResponse, BlobContents?)> Get(NamespaceId ns, BucketId bucket, KeyId key, string[] fields) { 
            bool needsBlob = !HasFieldFilter(fields) || fields.Contains("blob", StringComparer.InvariantCultureIgnoreCase);
            bool needsLastAccess = !HasFieldFilter(fields) || fields.Contains("lastAccessTime", StringComparer.InvariantCultureIgnoreCase);
            bool needsMetadata = !HasFieldFilter(fields) || fields.Contains("metadata", StringComparer.InvariantCultureIgnoreCase);

            IRefsStore.ExtraFieldsFlag flags = IRefsStore.ExtraFieldsFlag.None;
            if (needsLastAccess)
            {
                flags |= IRefsStore.ExtraFieldsFlag.LastAccess;
            }

            if (needsMetadata)
            {
                flags |= IRefsStore.ExtraFieldsFlag.Metadata;
            }

            string resource = $"{ns}.{bucket}.{key}";
            RefRecord? record;
            using (IScope scope = Tracer.Instance.StartActive("ref.get"))
            {
                scope.Span.ResourceName = resource;
                record = await _refsStore.Get(ns, bucket, key, flags);
                if (record == null)
                {
                    throw new RefRecordNotFoundException(ns, bucket, key);
                }
            }

            // its not critical that this finishes, so we just log errors in case it fails but never await
            {
                Task _ = _lastAccessTracker.TrackUsed(record).ContinueWith((task, _) =>
                {
                    if (task.Exception != null)
                    {
                        _logger.Error(task.Exception, "Exception when tracking last access record");
                    }
                }, null, TaskScheduler.Current);
            }

            BlobContents? maybeBlob = null;

            if (needsBlob)
            {
                using IScope scope = Tracer.Instance.StartActive("blob.get");
                scope.Span.ResourceName = resource;
                scope.Span.SetTag("BlobCount", record.Blobs.Length.ToString());

                if (record.Blobs.Length == 1)
                {
                    // stream directly to the output if the blob is not chunked
                    maybeBlob = await _blobStore.GetObject(ns, record.Blobs[0]);
                }
                else
                {
                    maybeBlob = await _blobStore.GetObjects(ns, record.Blobs);
                }
                // add content length to the current span so we can determine if the request was working on a large payload
                scope?.Span.SetTag("Content-Length", maybeBlob.Length.ToString());

                _diagnosticContext.Set("Content-Length", maybeBlob.Length);
            }

            RefResponse response = new RefResponse(
                name: $"{record.Namespace}.{record.Bucket}.{record.RefName}",
                contentHash: record.ContentHash,
                lastAccessTime: record.LastAccessTime,
                blobIdentifiers: record.Blobs,
                metadata: record.Metadata
            );
            return (response, maybeBlob);
        }

        public async Task<RefRecord> Exists(
            NamespaceId ns,
            BucketId bucket,
            KeyId key)
        {

            RefRecord? record = await _refsStore.Get(ns, bucket, key, IRefsStore.ExtraFieldsFlag.None);
            if (record == null)
            {
                throw new RefRecordNotFoundException(ns, bucket, key);
            }

            // we have to verify the blobs are available locally, as the record of the key is replicated a head of the content
            BlobIdentifier[] unknownBlobs = await _blobStore.FilterOutKnownBlobs(ns, record.Blobs);
            if (unknownBlobs.Length != 0)
            {
                throw new MissingBlobsException(ns, bucket, key, unknownBlobs);
            }

            return record;
        }

        public async Task<PutRequestResponse> Put(
            NamespaceId ns,
            BucketId bucket,
            KeyId key,
            ContentHash blobHash,
            byte[] blob)
        {
            BlobIdentifier[] blobReferences;
            Memory<byte>[]? slices;
            if (!_settings.MaxSingleBlobSize.HasValue || blob.Length < _settings.MaxSingleBlobSize)
            {
                // we do not need to chunk, lets just use the hash we have already calculated
                blobReferences = new[] { BlobIdentifier.FromContentHash(blobHash) };

                // we should not use the sliced version since we can just pass the whole byte[] instead which if faster
                slices = null;
            }
            else
            {
                _logger.Debug("Blob was to large, starting partitioning");
                // partition the blob
                Memory<byte> memory = blob.AsMemory();

                int countOfSlices = (int) (blob.LongLength / _settings.MaxSingleBlobSize);
                // if there was any re-meaning buffer we need to allocate another slice to fill that.
                countOfSlices += blob.LongLength % _settings.MaxSingleBlobSize != 0 ? 1 : 0;

                blobReferences = new BlobIdentifier[countOfSlices];
                slices = new Memory<byte>[countOfSlices];
                int maxSingleBlobSize = _settings.MaxSingleBlobSize.Value;
                for (int i = 0; i < countOfSlices; i++)
                {
                    int blobStart = maxSingleBlobSize * i;
                    int length = Math.Min(maxSingleBlobSize, val2: blob.Length - blobStart);
                    slices[i] = memory.Slice(blobStart, length);
                    BlobIdentifier hashIdentifier = BlobIdentifier.FromBlob(slices[i]);
                    blobReferences[i] = hashIdentifier;
                }
                _logger.Debug("Blobs partitioned into {PartitionCount} slices with {Identifiers}", countOfSlices, blobReferences);
            }
            int countOfBlobs = blobReferences.Length;
            if (countOfBlobs == 0)
            {
                throw new ArgumentException("No blobs found when determining partitioning");
            }

            // as the body is just a binary blob we are unable to receive metadata for this route
            // TODO: We could accept metadata from query parameters
            RefRecord record = new RefRecord(ns, bucket, key, metadata: null, blobs: blobReferences,
                lastAccessTime: DateTime.Now, contentHash: blobHash);

            // we have to insert (and wait for flush) into the transaction log before we attempt to upload the blobs to avoid race conditions when cleaning up
            long transactionId = await _transactionLog.Add(ns, record.ToAddTransactionEvent());

            Task blobInsertTask;
            if (countOfBlobs == 1)
            {
                // use the faster byte[] path if we do not need to partition the blob
                blobInsertTask = _blobStore.PutObject(ns, blob, blobReferences[0]);
            }
            else
            {
                if (slices == null)
                {
                    throw new Exception("Slices was never set when uploading partitioned blob.");
                }

                Task[] insertTasks = new Task[countOfBlobs];
                for (int i = 0; i < countOfBlobs; i++)
                {
                    insertTasks[i] = _blobStore.PutObject(ns, slices[i].ToArray(), blobReferences[i]);
                }

                blobInsertTask = Task.WhenAll(insertTasks);
            }

            Task addRefStore = _refsStore.Add(record);
            await Task.WhenAll(addRefStore, blobInsertTask);
            return new PutRequestResponse(transactionId);
        }

        /// <inheritdoc/>
        public async Task<long> PutIndirect(
            NamespaceId ns,
            BucketId bucket,
            KeyId key,
            RefRequest refRequest)
        {
            if (refRequest.BlobReferences == null)
            {
                throw new Exception("BlobReferences missing from request");
            }

            // TODO: should they just inline the blobs instead so we can submit them like we do for the blob put?

            BlobIdentifier[] unknownBlobs = await _blobStore.FilterOutKnownBlobs(ns, refRequest.BlobReferences);
            if (unknownBlobs.Length != 0)
            {
               throw new MissingBlobsException(ns, bucket, key, unknownBlobs);
            }

            await using BlobContents blobContents = refRequest.BlobReferences.Length == 1
                ?
                // avoid reallocating the stream if its a single object that is not chunked
                await _blobStore.GetObject(ns, refRequest.BlobReferences[0])
                : await _blobStore.GetObjects(ns, refRequest.BlobReferences);

            // TODO: This is downloading the blob from the store just to verify that the content hash is correct
            // this is quite expensive and slow, the blob is content hashed so we know the hash is correct for it
            // but if chunked we need to verify the concatenated blob is correct
            await using MemoryStream ms = new MemoryStream();
            await blobContents.Stream.CopyToAsync(ms);
            ContentHash actualHash = ContentHash.FromBlob(ms.ToArray());
            ContentHash fullHash = refRequest.ContentHash;

            if (!fullHash.Equals(actualHash))
            {
                throw new HashMismatchException(fullHash, actualHash);
            }

            RefRecord record = new RefRecord(ns, bucket, key,
                refRequest.BlobReferences, DateTime.Now, actualHash, refRequest.Metadata);
            long transactionId = await _transactionLog.Add(ns, record.ToAddTransactionEvent());
            await _refsStore.Add(record);

            return transactionId;
        }

        public async Task<long> Delete(NamespaceId ns, BucketId bucket, KeyId key)
        {
            long deleteCount = await _refsStore.Delete(ns, bucket, key);
            if (deleteCount != 0)
            {
                await _transactionLog.Delete(ns, bucket, key);
            }

            return deleteCount;
        }
    }
}
