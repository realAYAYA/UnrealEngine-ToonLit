// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Artifacts;
using EpicGames.Horde.Storage;
using EpicGames.Horde.Streams;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Server.Artifacts
{
	/// <summary>
	/// Implementation of <see cref="IArtifactCollection"/>
	/// </summary>
	class ArtifactCollection : IArtifactCollection
	{
		class Artifact : IArtifact
		{
			[BsonRequired, BsonId]
			public ArtifactId Id { get; set; }

			[BsonElement("nam")]
			public ArtifactName Name { get; set; }

			[BsonElement("typ")]
			public ArtifactType Type { get; set; }

			[BsonElement("dsc"), BsonIgnoreIfNull]
			public string? Description { get; set; }

			[BsonElement("str")]
			public StreamId StreamId { get; set; }

			[BsonElement("chg")]
			public int Change { get; set; }

			[BsonElement("key")]
			public List<string> Keys { get; set; } = new List<string>();

			IReadOnlyList<string> IArtifact.Keys => Keys;

			[BsonElement("met")]
			public List<string> Metadata { get; set; } = new List<string>();

			IReadOnlyList<string> IArtifact.Metadata => Metadata;

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("ref")]
			public RefName RefName { get; set; }

			[BsonElement("scp")]
			public AclScopeName AclScope { get; set; }

			[BsonElement("cre")]
			public DateTime CreatedAtUtc { get; set; }

			[BsonElement("upd")]
			public int UpdateIndex { get; set; }

			DateTime IArtifact.CreatedAtUtc => (CreatedAtUtc == default) ? BinaryIdUtils.ToObjectId(Id.Id).CreationTime : CreatedAtUtc;

			[BsonConstructor]
			private Artifact()
			{
			}

			public Artifact(ArtifactId id, ArtifactName name, ArtifactType type, string? description, StreamId streamId, int change, IEnumerable<string> keys, IEnumerable<string> metadata, NamespaceId namespaceId, RefName refName, DateTime createdAtUtc, AclScopeName scopeName)
			{
				Id = id;
				Name = name;
				Type = type;
				Description = description;
				StreamId = streamId;
				Change = change;
				Keys.AddRange(keys);
				Metadata.AddRange(metadata);
				NamespaceId = namespaceId;
				RefName = refName;
				CreatedAtUtc = createdAtUtc;
				AclScope = scopeName;
			}
		}

		readonly IMongoCollection<Artifact> _artifacts;
		readonly IClock _clock;

		/// <summary>
		/// Constructor
		/// </summary>
		public ArtifactCollection(MongoService mongoService, IClock clock)
		{
			List<MongoIndex<Artifact>> indexes = new List<MongoIndex<Artifact>>();
			indexes.Add(keys => keys.Ascending(x => x.Keys));
			indexes.Add(keys => keys.Ascending(x => x.Type).Descending(x => x.Id));
			indexes.Add(keys => keys.Ascending(x => x.StreamId).Descending(x => x.Change).Ascending(x => x.Name).Descending(x => x.Id));
			_artifacts = mongoService.GetCollection<Artifact>("ArtifactsV2", indexes);

			_clock = clock;
		}

#pragma warning disable CA1308 // Expect ansi-only keys here
		static string NormalizeKey(string key)
			=> key.ToLowerInvariant();
#pragma warning restore CA1308

		/// <summary>
		/// Gets the base path for a set of artifacts
		/// </summary>
		public static string GetArtifactPath(StreamId streamId, ArtifactName name, ArtifactType type) => $"{streamId}/{name}/{type}";

		/// <inheritdoc/>
		public async Task<IArtifact> AddAsync(ArtifactName name, ArtifactType type, string? description, StreamId streamId, int change, IEnumerable<string> keys, IEnumerable<string> metadata, AclScopeName scopeName, CancellationToken cancellationToken)
		{
			ArtifactId id = new ArtifactId(BinaryIdUtils.CreateNew());

			NamespaceId namespaceId = Namespace.Artifacts;
			RefName refName = new RefName($"{GetArtifactPath(streamId, name, type)}/{change}/{id}");

			Artifact artifact = new Artifact(id, name, type, description, streamId, change, keys.Select(x => NormalizeKey(x)), metadata, namespaceId, refName, _clock.UtcNow, scopeName);
			await _artifacts.InsertOneAsync(artifact, null, cancellationToken);
			return artifact;
		}

		/// <inheritdoc/>
		public async Task DeleteAsync(IEnumerable<ArtifactId> ids, CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.In(x => x.Id, ids);
			await _artifacts.DeleteManyAsync(filter, cancellationToken);
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IArtifact> FindAsync(StreamId? streamId = null, int? minChange = null, int? maxChange = null, ArtifactName? name = null, ArtifactType? type = null, IEnumerable<string>? keys = null, int maxResults = 100, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = FilterDefinition<Artifact>.Empty;
			if (streamId != null)
			{
				filter &= Builders<Artifact>.Filter.Eq(x => x.StreamId, streamId.Value);
			}
			if (minChange != null)
			{
				filter &= Builders<Artifact>.Filter.Gte(x => x.Change, minChange.Value);
			}
			if (maxChange != null)
			{
				filter &= Builders<Artifact>.Filter.Lte(x => x.Change, maxChange.Value);
			}
			if (name != null)
			{
				filter &= Builders<Artifact>.Filter.Eq(x => x.Name, name.Value);
			}
			if (type != null)
			{
				filter &= Builders<Artifact>.Filter.Eq(x => x.Type, type.Value);
			}
			if (keys != null && keys.Any())
			{
				filter &= Builders<Artifact>.Filter.All(x => x.Keys, keys.Select(x => NormalizeKey(x)));
			}

			using (IAsyncCursor<Artifact> cursor = await _artifacts.Find(filter).SortByDescending(x => x.Change).ThenByDescending(x => x.Id).Limit(maxResults).ToCursorAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					foreach (Artifact artifact in cursor.Current)
					{
						yield return artifact;
					}
				}
			}
		}

		/// <inheritdoc/>
		public async IAsyncEnumerable<IEnumerable<IArtifact>> FindExpiredAsync(ArtifactType type, DateTime? expireAtUtc, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.Eq(x => x.Type, type);
			if (expireAtUtc != null)
			{
				filter &= Builders<Artifact>.Filter.Lt(x => x.Id, new ArtifactId(BinaryIdUtils.FromObjectId(ObjectId.GenerateNewId(expireAtUtc.Value))));
			}

			IFindFluent<Artifact, Artifact> query = _artifacts.Find(filter).SortByDescending(x => x.Id);
			using (IAsyncCursor<Artifact> cursor = await query.ToCursorAsync(cancellationToken))
			{
				while (await cursor.MoveNextAsync(cancellationToken))
				{
					yield return cursor.Current;
				}
			}
		}

		/// <inheritdoc/>
		public async Task<IArtifact?> GetAsync(ArtifactId artifactId, CancellationToken cancellationToken)
		{
			return await _artifacts.Find(x => x.Id == artifactId).FirstOrDefaultAsync(cancellationToken);
		}
	}
}
