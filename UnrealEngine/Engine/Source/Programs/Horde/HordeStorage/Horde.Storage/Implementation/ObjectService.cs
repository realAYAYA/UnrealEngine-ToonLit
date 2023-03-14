// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using async_enumerable_dotnet;
using Datadog.Trace;
using EpicGames.AspNet;
using EpicGames.Horde.Storage;
using EpicGames.Serialization;
using Horde.Storage.Implementation.Blob;
using Jupiter.Implementation;
using Jupiter.Utils;
using Microsoft.AspNetCore.Http;
using Microsoft.Extensions.DependencyInjection;
using Serilog;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.Implementation
{
    public interface IObjectService
    {
        Task<(ObjectRecord, BlobContents?)> Get(NamespaceId ns, BucketId bucket, IoHashKey key, string[] fields, bool doLastAccessTracking = true);
        Task<(ContentId[], BlobIdentifier[])> Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, CbObject payload);
        Task<(ContentId[], BlobIdentifier[])> Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash);

        IAsyncEnumerable<NamespaceId> GetNamespaces();

        Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key);
        Task<long> DropNamespace(NamespaceId ns);
        Task<long> DeleteBucket(NamespaceId ns, BucketId bucket);
    }

    public class ObjectService : IObjectService
    {
        private readonly IHttpContextAccessor _httpContextAccessor;
        private readonly IReferencesStore _referencesStore;
        private readonly IBlobService _blobService;
        private readonly IReferenceResolver _referenceResolver;
        private readonly IReplicationLog _replicationLog;
        private readonly IBlobIndex _blobIndex;
        private readonly ILastAccessTracker<LastAccessRecord> _lastAccessTracker;
        private readonly ILogger _logger = Log.ForContext<ObjectService>();

        public ObjectService(IHttpContextAccessor httpContextAccessor, IReferencesStore referencesStore, IBlobService blobService, IReferenceResolver referenceResolver, IReplicationLog replicationLog, IBlobIndex blobIndex, ILastAccessTracker<LastAccessRecord> lastAccessTracker)
        {
            _httpContextAccessor = httpContextAccessor;
            _referencesStore = referencesStore;
            _blobService = blobService;
            _referenceResolver = referenceResolver;
            _replicationLog = replicationLog;
            _blobIndex = blobIndex;
            _lastAccessTracker = lastAccessTracker;
        }

        public async Task<(ObjectRecord, BlobContents?)> Get(NamespaceId ns, BucketId bucket, IoHashKey key, string[]? fields = null, bool doLastAccessTracking = true)
        {
            // if no field filtering is being used we assume everything is needed
            IReferencesStore.FieldFlags flags = IReferencesStore.FieldFlags.All;
            if (fields != null)
            {
                // empty array means fetch all fields
                if (fields.Length == 0)
                {
                    flags = IReferencesStore.FieldFlags.All;
                }
                else
                {
                    flags = fields.Contains("payload")
                        ? IReferencesStore.FieldFlags.IncludePayload
                        : IReferencesStore.FieldFlags.None;
                }
            }

            IServerTiming? serverTiming = _httpContextAccessor.HttpContext?.RequestServices.GetService<IServerTiming>();

            ObjectRecord o;
            {
                using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("ref.get", "Fetching Ref from DB");

                o = await _referencesStore.Get(ns, bucket, key, flags);
            }

            if (doLastAccessTracking)
            {
                // we do not wait for the last access tracking as it does not matter when it completes
                Task lastAccessTask = _lastAccessTracker.TrackUsed(new LastAccessRecord(ns, bucket, key)).ContinueWith((task, _) =>
                {
                    if (task.Exception != null)
                    {
                        _logger.Error(task.Exception, "Exception when tracking last access record");
                    }
                }, null, TaskScheduler.Current);
            }

            BlobContents? blobContents = null;
            if ((flags & IReferencesStore.FieldFlags.IncludePayload) != 0)
            {
                if (o.InlinePayload != null && o.InlinePayload.Length != 0)
                {
#pragma warning disable CA2000 // Dispose objects before losing scope , ownership is transfered to caller
                    blobContents = new BlobContents(o.InlinePayload);
#pragma warning restore CA2000 // Dispose objects before losing scope
                }
                else
                {
                    using ServerTimingMetricScoped? serverTimingScope = serverTiming?.CreateServerTimingMetricScope("blob.get", "Downloading blob from store");

                    blobContents = await _blobService.GetObject(ns, o.BlobIdentifier);
                }
            }

            return (o, blobContents);
        }

        public async Task<(ContentId[], BlobIdentifier[])> Put(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, CbObject payload)
        {
            bool hasReferences = HasAttachments(payload);

            // if we have no references we are always finalized, e.g. there are no referenced blobs to upload
            bool isFinalized = !hasReferences;

            Task objectStorePut = _referencesStore.Put(ns, bucket, key, blobHash, payload.GetView().ToArray(), isFinalized);

            Task<BlobIdentifier> blobStorePut = _blobService.PutObject(ns, payload.GetView().ToArray(), blobHash);
            
            await Task.WhenAll(objectStorePut, blobStorePut);

            return await DoFinalize(ns, bucket, key, blobHash, payload);
        }

        private bool HasAttachments(CbObject payload)
        {
            bool FieldHasAttachments(CbField field)
            {
                if (field.IsObject())
                {
                    bool hasAttachment = HasAttachments(field.AsObject());
                    if (hasAttachment)
                    {
                        return true;
                    }
                }

                if (field.IsArray())
                {
                    foreach (CbField subField in field.AsArray())
                    {
                        bool hasAttachment = FieldHasAttachments(subField);
                        if (hasAttachment)
                        {
                            return true;
                        }
                    }
                }

                return field.IsAttachment();
            }

            return payload.Any(FieldHasAttachments);
        }

        public async Task<(ContentId[], BlobIdentifier[])> Finalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash)
        {
            (ObjectRecord o, BlobContents? blob) = await Get(ns, bucket, key);
            if (blob == null)
            {
                throw new InvalidOperationException("No blob when attempting to finalize");
            }

            byte[] blobContents = await blob.Stream.ToByteArray();
            CbObject payload = new CbObject(blobContents);

            if (!o.BlobIdentifier.Equals(blobHash))
            {
                throw new ObjectHashMismatchException(ns, bucket, key, blobHash, o.BlobIdentifier);
            }

            return await DoFinalize(ns, bucket, key, blobHash, payload);
        }

        
        private async Task<(ContentId[], BlobIdentifier[])> DoFinalize(NamespaceId ns, BucketId bucket, IoHashKey key, BlobIdentifier blobHash, CbObject payload)
        {
            Task addRefToBlobsTask = _blobIndex.AddRefToBlobs(ns, bucket, key, new [] {blobHash});

            ContentId[] missingReferences = Array.Empty<ContentId>();
            BlobIdentifier[] missingBlobs = Array.Empty<BlobIdentifier>();
            bool hasReferences = HasAttachments(payload);
            if (hasReferences)
            {
                using IScope _ = Tracer.Instance.StartActive("ObjectService.ResolveReferences");
                try
                {
                    IAsyncEnumerable<BlobIdentifier> references = _referenceResolver.GetReferencedBlobs(ns, payload);
                    BlobIdentifier[] referencesArray = await references.ToArrayAsync();
                    Task addRefsTask = _blobIndex.AddRefToBlobs(ns, bucket, key, referencesArray);
                    missingBlobs = await _blobService.FilterOutKnownBlobs(ns, referencesArray);
                    await addRefsTask;
                }
                catch (PartialReferenceResolveException e)
                {
                    missingReferences = e.UnresolvedReferences.ToArray();
                }
                catch (ReferenceIsMissingBlobsException e)
                {
                    missingBlobs = e.MissingBlobs.ToArray();
                }
            }

            await addRefToBlobsTask;

            if (missingReferences.Length == 0 && missingBlobs.Length == 0)
            {
                await _referencesStore.Finalize(ns, bucket, key, blobHash);
                await _replicationLog.InsertAddEvent(ns, bucket, key, blobHash);
            }

            return (missingReferences, missingBlobs);
        }

        public IAsyncEnumerable<NamespaceId> GetNamespaces()
        {
            return _referencesStore.GetNamespaces();
        }

        public Task<bool> Delete(NamespaceId ns, BucketId bucket, IoHashKey key)
        {
            return _referencesStore.Delete(ns, bucket, key);
        }

        public Task<long> DropNamespace(NamespaceId ns)
        {
            return _referencesStore.DropNamespace(ns);
        }

        public Task<long> DeleteBucket(NamespaceId ns, BucketId bucket)
        {
            return _referencesStore.DeleteBucket(ns, bucket);
        }
    }
}
