// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Horde.Build.Server;
using Horde.Build.Streams;
using Horde.Build.Users;
using Horde.Build.Utilities;
using MongoDB.Bson;
using MongoDB.Bson.Serialization.Attributes;
using MongoDB.Driver;

namespace Horde.Build.Perforce
{
	using CommitId = ObjectId<ICommit>;
	using StreamId = StringId<IStream>;
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Concrete implementation of ICommitCollection
	/// </summary>
	class CommitCollection : ICommitCollection
	{
		class Commit : ICommit
		{
			[BsonIgnoreIfDefault]
			public CommitId Id { get; set; }

			[BsonElement("s")]
			public StreamId StreamId { get; set; }

			[BsonElement("c")]
			public int Change { get; set; }

			[BsonElement("oc"), BsonIgnoreIfNull]
			public int? OriginalChange { get; set; }

			[BsonElement("a")]
			public UserId AuthorId { get; set; }

			[BsonElement("o"), BsonIgnoreIfNull]
			public UserId? OwnerId { get; set; }

			[BsonElement("d")]
			public string Description { get; set; } = String.Empty;

			[BsonElement("p")]
			public string BasePath { get; set; } = String.Empty;

			[BsonElement("t")]
			public DateTime DateUtc { get; set; }

			int ICommit.OriginalChange => OriginalChange ?? Change;
			UserId ICommit.OwnerId => OwnerId ?? AuthorId;

			public Commit()
			{
			}

			public Commit(NewCommit newCommit)
			{
				Change = newCommit.Change;
				if (newCommit.OriginalChange != newCommit.Change)
				{
					OriginalChange = newCommit.OriginalChange;
				}
				StreamId = newCommit.StreamId;
				AuthorId = newCommit.AuthorId;
				if (newCommit.OwnerId != newCommit.AuthorId)
				{
					OwnerId = newCommit.OwnerId;
				}
				Description = newCommit.Description;
				BasePath = newCommit.BasePath;
				DateUtc = newCommit.DateUtc;
			}
		}

		readonly IMongoCollection<Commit> _commits;

		/// <summary>
		/// Constructor
		/// </summary>
		public CommitCollection(MongoService mongoService)
		{
			_commits = mongoService.GetCollection<Commit>("Commits", keys => keys.Ascending(x => x.StreamId).Descending(x => x.Change), unique: true);
		}

		/// <inheritdoc/>
		public async Task<ICommit> AddOrReplaceAsync(NewCommit newCommit)
		{
			Commit commit = new Commit(newCommit);
			FilterDefinition<Commit> filter = Builders<Commit>.Filter.Expr(x => x.StreamId == newCommit.StreamId && x.Change == newCommit.Change);
			return await _commits.FindOneAndReplaceAsync(filter, new Commit(newCommit), new FindOneAndReplaceOptions<Commit> { IsUpsert = true, ReturnDocument = ReturnDocument.After });
		}

		/// <inheritdoc/>
		public async Task<ICommit?> GetCommitAsync(CommitId id)
		{
			return await _commits.Find(x => x.Id == id).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<ICommit>> FindCommitsAsync(StreamId streamId, int? minChange = null, int? maxChange = null, int? index = null, int? count = null)
		{
			FilterDefinition<Commit> filter = Builders<Commit>.Filter.Eq(x => x.StreamId, streamId);
			if (maxChange != null)
			{
				filter &= Builders<Commit>.Filter.Lte(x => x.Change, maxChange.Value);
			}
			if (minChange != null)
			{
				filter &= Builders<Commit>.Filter.Lte(x => x.Change, minChange.Value);
			}
			return await _commits.Find(filter).SortByDescending(x => x.Change).Range(index, count).ToListAsync<Commit, ICommit>();
		}
	}
}
