// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Linq;
using System.Threading.Tasks;
using Cassandra;
using Cassandra.Mapping;
using EpicGames.Horde.Storage;
using OpenTelemetry.Trace;

namespace Jupiter.Implementation
{
    public class ScyllaContentIdStore : IContentIdStore
    {
        private readonly ISession _session;
        private readonly IBlobService _blobStore;
        private readonly Tracer _tracer;
        private readonly Mapper _mapper;
        private readonly IScyllaSessionManager _scyllaSessionManager;

        public ScyllaContentIdStore(IScyllaSessionManager scyllaSessionManager, IBlobService blobStore, Tracer tracer)
        {
            _scyllaSessionManager = scyllaSessionManager;
            _session = scyllaSessionManager.GetSessionForReplicatedKeyspace();
            _blobStore = blobStore;
            _tracer = tracer;

            _mapper = new Mapper(_session);

            string blobType = scyllaSessionManager.IsCassandra ? "blob" : "frozen<blob_identifier>";
            _session.Execute(new SimpleStatement(@$"CREATE TABLE IF NOT EXISTS content_id (
                content_id {blobType},
                content_weight int, 
                chunks set<{blobType}>, 
                PRIMARY KEY ((content_id), content_weight)
            );"
            ));
        }

        public async Task<BlobIdentifier[]?> Resolve(NamespaceId ns, ContentId contentId, bool mustBeContentId)
        {
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("ScyllaContentIdStore.ResolveContentId").SetAttribute("resource.name", contentId.ToString());

            BlobIdentifier contentIdBlob = contentId.AsBlobIdentifier();
            Task<bool>? blobStoreExistsTask = null;
            if (!mustBeContentId)
            {
                blobStoreExistsTask = _blobStore.Exists(ns, contentIdBlob);
            }

            {
                using TelemetrySpan contentIdFetchScope = _tracer.BuildScyllaSpan("ScyllaContentIdStore.FetchContentId").SetAttribute("resource.name", contentId.ToString());

                if (_scyllaSessionManager.IsScylla)
                {
                    // lower content_weight means its a better candidate to resolve to
                    foreach (ScyllaContentId? resolvedContentId in await _mapper.FetchAsync<ScyllaContentId>("WHERE content_id = ? ORDER BY content_weight ASC", new ScyllaBlobIdentifier(contentId)))
                    {
                        if (resolvedContentId == null)
                        {
                            throw new InvalidContentIdException(contentId);
                        }

                        BlobIdentifier[] blobs = resolvedContentId.Chunks.Select(b => b.AsBlobIdentifier()).ToArray();

                        {
                            using TelemetrySpan _ = _tracer.StartActiveSpan("ScyllaContentIdStore.FindMissingBlobs").SetAttribute("operation.name", "ScyllaContentIdStore.FindMissingBlobs");

                            BlobIdentifier[] missingBlobs = await _blobStore.FilterOutKnownBlobs(ns, blobs);
                            if (missingBlobs.Length == 0)
                            {
                                return blobs;
                            }
                        }
                        // blobs are missing continue testing with the next content id in the weighted list as that might exist
                    }
                }
                else
                {
                    // lower content_weight means its a better candidate to resolve to
                    foreach (CassandraContentId? resolvedContentId in await _mapper.FetchAsync<CassandraContentId>("WHERE content_id = ? ORDER BY content_weight ASC", contentId.HashData))
                    {
                        if (resolvedContentId == null)
                        {
                            throw new InvalidContentIdException(contentId);
                        }

                        BlobIdentifier[] blobs = resolvedContentId.Chunks.Select(b => new BlobIdentifier(b!)).ToArray();

                        {
                            using TelemetrySpan _ = _tracer.StartActiveSpan("ScyllaContentIdStore.FindMissingBlobs").SetAttribute("operation.name", "ScyllaContentIdStore.FindMissingBlobs");

                            BlobIdentifier[] missingBlobs = await _blobStore.FilterOutKnownBlobs(ns, blobs);
                            if (missingBlobs.Length == 0)
                            {
                                return blobs;
                            }
                        }
                        // blobs are missing continue testing with the next content id in the weighted list as that might exist
                    }
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
            using TelemetrySpan scope = _tracer.BuildScyllaSpan("ScyllaContentIdStore.PutContentId");
            if (_scyllaSessionManager.IsScylla)
            {
                await _mapper.UpdateAsync<ScyllaContentId>("SET chunks = ? WHERE content_id = ? AND content_weight = ?", new [] {new ScyllaBlobIdentifier(blobIdentifier)}, new ScyllaBlobIdentifier(contentId), contentWeight);

            }
            else
            {
                await _mapper.UpdateAsync<CassandraContentId>("SET chunks = ? WHERE content_id = ? AND content_weight = ?", new [] {blobIdentifier.HashData}, contentId.HashData, contentWeight);
            }
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

    
    [Cassandra.Mapping.Attributes.Table("content_id")]
    public class CassandraContentId
    {
        public CassandraContentId()
        {

        }

        public CassandraContentId(BlobIdentifier contentId, BlobIdentifier[] chunks, int contentWeight)
        {
            ContentId = contentId.HashData;
            ContentWeight = contentWeight;
            Chunks = chunks.Select(b => b.HashData).ToArray();
        }

        [Cassandra.Mapping.Attributes.PartitionKey]
        [Cassandra.Mapping.Attributes.Column("content_id")]
        public byte[]? ContentId { get; set; }

        [Cassandra.Mapping.Attributes.ClusteringKey]
        [Cassandra.Mapping.Attributes.Column("content_weight")]
        public int? ContentWeight { get; set; }

        public byte[][] Chunks { get; set; } = Array.Empty<byte[]>();
    }
}
