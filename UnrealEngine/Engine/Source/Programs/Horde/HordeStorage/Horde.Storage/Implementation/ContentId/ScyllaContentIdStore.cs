// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading.Tasks;
using Cassandra;
using Cassandra.Mapping;
using Datadog.Trace;
using EpicGames.Horde.Storage;
using Jupiter.Implementation;
using ContentId = Jupiter.Implementation.ContentId;

namespace Horde.Storage.Implementation
{
    public class ScyllaContentIdStore : IContentIdStore
    {
        private readonly ISession _session;
        private readonly IBlobService _blobStore;
        private readonly Mapper _mapper;

        public ScyllaContentIdStore(IScyllaSessionManager scyllaSessionManager, IBlobService blobStore)
        {
            _session = scyllaSessionManager.GetSessionForReplicatedKeyspace();
            _blobStore = blobStore;

            _mapper = new Mapper(_session);

            _session.Execute(new SimpleStatement(@"CREATE TABLE IF NOT EXISTS content_id (
                content_id frozen<blob_identifier>,
                content_weight int, 
                chunks set<frozen<blob_identifier>>, 
                PRIMARY KEY ((content_id), content_weight)
            );"
            ));
        }

        public async Task<BlobIdentifier[]?> Resolve(NamespaceId ns, ContentId contentId, bool mustBeContentId)
        {
            using IScope scope = Tracer.Instance.StartActive("ScyllaContentIdStore.ResolveContentId");
            scope.Span.ResourceName = contentId.ToString();

            BlobIdentifier contentIdBlob = contentId.AsBlobIdentifier();
            Task<bool>? blobStoreExistsTask = null;
            if (!mustBeContentId)
            {
                blobStoreExistsTask = _blobStore.Exists(ns, contentIdBlob);
            }

            {
                using IScope contentIdFetchScope = Tracer.Instance.StartActive("ScyllaContentIdStore.FetchContentId");
                scope.Span.ResourceName = contentId.ToString();

                // lower content_weight means its a better candidate to resolve to
                foreach (ScyllaContentId? resolvedContentId in await _mapper.FetchAsync<ScyllaContentId>("WHERE content_id = ? ORDER BY content_weight DESC", new ScyllaBlobIdentifier(contentId)))
                {
                    if (resolvedContentId == null)
                    {
                        throw new InvalidContentIdException(contentId);
                    }

                    BlobIdentifier[] blobs = resolvedContentId.Chunks.Select(b => b.AsBlobIdentifier()).ToArray();

                    {
                        using IScope _ = Tracer.Instance.StartActive("ScyllaContentIdStore.FindMissingBlobs");

                        BlobIdentifier[] missingBlobs = await _blobStore.FilterOutKnownBlobs(ns, blobs);
                        if (missingBlobs.Length == 0)
                        {
                            return blobs;
                        }
                    }
                    // blobs are missing continue testing with the next content id in the weighted list as that might exist
                }
            }
            

            if (!mustBeContentId)
            {
                // if no content id is found, but we have a blob that matches the content id (so a unchunked and uncompressed version of the data) we use that instead
                bool contentIdBlobExists = await blobStoreExistsTask!;

                if (contentIdBlobExists)
                {
                    return new[] { contentIdBlob };
                }
            }
            
            // unable to resolve the content id
            return null;
        }

        public async Task Put(NamespaceId ns, ContentId contentId, BlobIdentifier blobIdentifier, int contentWeight)
        {
            await _mapper.UpdateAsync<ScyllaContentId>("SET chunks = ? WHERE content_id = ? AND content_weight = ?", new [] {new ScyllaBlobIdentifier(blobIdentifier)}, new ScyllaBlobIdentifier(contentId), contentWeight);
        }
    }

    [Cassandra.Mapping.Attributes.Table("content_id")]
    public class ScyllaContentId
    {
        public ScyllaContentId()
        {

        }

        public ScyllaContentId(BlobIdentifier contentId, BlobIdentifier[] chunks, int contentWeight)
        {
            ContentId = new ScyllaBlobIdentifier(contentId);
            ContentWeight = contentWeight;
            Chunks = chunks.Select(b => new ScyllaBlobIdentifier(b)).ToArray();
        }

        [Cassandra.Mapping.Attributes.PartitionKey]
        [Cassandra.Mapping.Attributes.Column("content_id")]
        public ScyllaBlobIdentifier? ContentId { get; set; }

        [Cassandra.Mapping.Attributes.ClusteringKey]
        [Cassandra.Mapping.Attributes.Column("content_weight")]
        public int? ContentWeight { get; set; }

        public ScyllaBlobIdentifier[] Chunks { get; set; } = Array.Empty<ScyllaBlobIdentifier>();
    }
}
