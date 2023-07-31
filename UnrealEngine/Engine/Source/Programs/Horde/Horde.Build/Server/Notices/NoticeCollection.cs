// Copyright Epic Games, Inc. All Rights Reserved.

using MongoDB.Bson.Serialization.Attributes;
using System.Threading.Tasks;
using MongoDB.Driver;
using System;
using System.Collections.Generic;
using System.Linq;
using Horde.Build.Utilities;
using MongoDB.Bson;
using Horde.Build.Users;

namespace Horde.Build.Server.Notices
{
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Collection of notice documents
	/// </summary>
	public class NoticeCollection : INoticeCollection
	{

		/// <summary>
		/// Document representing a notice 
		/// </summary>
		class NoticeDocument : INotice
		{
			[BsonRequired, BsonId]
			public ObjectId Id { get; set; }

			[BsonIgnoreIfNull]
			public UserId? UserId { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? StartTime { get; set; }

			[BsonIgnoreIfNull]
			public DateTime? FinishTime { get; set; }

			public string Message { get; set; } = String.Empty;

			[BsonConstructor]
			private NoticeDocument()
			{
			}

			public NoticeDocument(ObjectId id, string message, UserId? userId, DateTime? startTime, DateTime? finishTime)
			{
				Id = id;
				Message = message;
				UserId = userId;
				StartTime = startTime;
				FinishTime = finishTime;
			}
		}

		readonly IMongoCollection<NoticeDocument> _notices;

		/// <summary>
		/// Constructor
		/// </summary>
		public NoticeCollection(MongoService mongoService)
		{
			_notices = mongoService.GetCollection<NoticeDocument>("Notices");
		}

		/// <inheritdoc/>
		public async Task<INotice?> AddNoticeAsync(string message, UserId? userId, DateTime? startTime, DateTime? finishTime)
		{
			NoticeDocument newNotice = new NoticeDocument(ObjectId.GenerateNewId(), message, userId, startTime, finishTime);
			await _notices.InsertOneAsync(newNotice);
			return newNotice;
		}

		/// <inheritdoc/>
		public async Task<bool> UpdateNoticeAsync(ObjectId id, string? message, DateTime? startTime, DateTime? finishTime)
		{

			UpdateDefinitionBuilder<NoticeDocument> updateBuilder = Builders<NoticeDocument>.Update;
			List<UpdateDefinition<NoticeDocument>> updates = new List<UpdateDefinition<NoticeDocument>>();

			if (message != null)
			{
				updates.Add(updateBuilder.Set(x => x.Message, message));
			}

			updates.Add(updateBuilder.Set(x => x.StartTime, startTime));

			updates.Add(updateBuilder.Set(x => x.FinishTime, finishTime));

			NoticeDocument? document = await _notices.FindOneAndUpdateAsync(x => x.Id == id, updateBuilder.Combine(updates));

			return document != null;
		}

		/// <inheritdoc/>
		public async Task<INotice?> GetNoticeAsync(ObjectId noticeId)
		{
			return await _notices.Find(x => x.Id == noticeId).FirstOrDefaultAsync();
		}

		/// <inheritdoc/>
		public async Task<List<INotice>> GetNoticesAsync()
		{
			List<NoticeDocument> results = await _notices.Find(x => true).ToListAsync();
			return results.Select<NoticeDocument, INotice>(x => x).ToList();
		}

		/// <inheritdoc/>
		public async Task<bool> RemoveNoticeAsync(ObjectId id)
		{
			DeleteResult result = await _notices.DeleteOneAsync(x => x.Id == id);
			return result.DeletedCount > 0;
		}
	}
}

