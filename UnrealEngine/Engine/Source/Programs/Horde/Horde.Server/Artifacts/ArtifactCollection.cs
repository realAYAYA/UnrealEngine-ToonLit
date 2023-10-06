// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Runtime.CompilerServices;
using System.Threading;
using System.Threading.Tasks;
using EpicGames.Horde.Storage;
using Horde.Server.Acls;
using Horde.Server.Server;
using Horde.Server.Storage;
using Horde.Server.Utilities;
using HordeCommon;
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

			[BsonElement("typ")]
			public ArtifactType Type { get; set; }

			[BsonElement("key")]
			public List<string> Keys { get; set; } = new List<string>();

			IReadOnlyList<string> IArtifact.Keys => Keys;

			[BsonElement("ns")]
			public NamespaceId NamespaceId { get; set; }

			[BsonElement("ref")]
			public RefName RefName { get; set; }

			[BsonElement("scp")]
			public AclScopeName AclScope { get; set; }

			[BsonElement("exp")]
			public DateTime? ExpireAtUtc { get; set; }

			[BsonElement("upd")]
			public int UpdateIndex { get; set; }

			[BsonConstructor]
			private Artifact()
			{
			}

			public Artifact(ArtifactId id, ArtifactType type, IEnumerable<string> keys, NamespaceId namespaceId, RefName refName, DateTime? expireAtUtc, AclScopeName scopeName)
			{
				Id = id;
				Type = type;
				Keys.AddRange(keys);
				NamespaceId = namespaceId;
				RefName = refName;
				ExpireAtUtc = expireAtUtc;
				AclScope = scopeName;
			}
		}

		private readonly IMongoCollection<Artifact> _artifacts;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="mongoService">The database service</param>
		public ArtifactCollection(MongoService mongoService)
		{
			List<MongoIndex<Artifact>> indexes = new List<MongoIndex<Artifact>>();
			indexes.Add(keys => keys.Ascending(x => x.Keys));
			indexes.Add(keys => keys.Ascending(x => x.ExpireAtUtc), sparse: true);
			_artifacts = mongoService.GetCollection<Artifact>("ArtifactsV2", indexes);
		}

		/// <inheritdoc/>
		public async Task<IArtifact> AddAsync(ArtifactId id, ArtifactType type, IEnumerable<string> keys, NamespaceId namespaceId, RefName refName, DateTime? expireAtUtc, AclScopeName scopeName, CancellationToken cancellationToken)
		{
			Artifact artifact = new Artifact(id, type, keys, namespaceId, refName, expireAtUtc, scopeName);
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
		public async IAsyncEnumerable<IArtifact> FindAsync(IEnumerable<ArtifactId>? ids = null, IEnumerable<string>? keys = null, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			FilterDefinition<Artifact> filter = FilterDefinition<Artifact>.Empty;
			if (ids != null && ids.Any())
			{
				filter = filter & Builders<Artifact>.Filter.In(x => x.Id, ids);
			}
			if (keys != null && keys.Any())
			{
				filter = filter & Builders<Artifact>.Filter.AnyIn(x => x.Keys, keys);
			}

			using (IAsyncCursor<Artifact> cursor = await _artifacts.Find(filter).ToCursorAsync(cancellationToken))
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

		/// <summary>
		/// Finds artifacts which are ready for expiry
		/// </summary>
		/// <param name="utcNow">Current time for expiring artifacts</param>
		/// <param name="cancellationToken">Cancellation token for the operation</param>
		/// <returns>Sequence of artifacts</returns>
		public async IAsyncEnumerable<IEnumerable<IArtifact>> FindExpiredAsync(DateTime utcNow, [EnumeratorCancellation] CancellationToken cancellationToken = default)
		{
			using (IAsyncCursor<Artifact> cursor = await _artifacts.Find(x => x.ExpireAtUtc!.Value < utcNow).ToCursorAsync(cancellationToken))
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

		/// <inheritdoc/>
		public async Task<IArtifact?> TryUpdateAsync(IArtifact artifact, DateTime? expireAtUtc, CancellationToken cancellationToken)
		{
			List<UpdateDefinition<Artifact>> updates = new List<UpdateDefinition<Artifact>>();
			if (expireAtUtc != null)
			{
				updates.Add(Builders<Artifact>.Update.SetOrUnsetNull(x => x.ExpireAtUtc, expireAtUtc));
			}
			if (updates.Count == 0)
			{
				return artifact;
			}

			Artifact artifactDoc = (Artifact)artifact;
			FilterDefinition<Artifact> filter = Builders<Artifact>.Filter.Expr(x => x.Id == artifact.Id && x.UpdateIndex == artifactDoc.UpdateIndex);
			UpdateDefinition<Artifact> update = Builders<Artifact>.Update.Combine(updates);

			return await _artifacts.FindOneAndUpdateAsync(filter, update, new FindOneAndUpdateOptions<Artifact> { ReturnDocument = ReturnDocument.After }, cancellationToken);
		}
	}
}
