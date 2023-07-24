// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Users;
using Horde.Build.Utilities;
using MongoDB.Bson;

namespace Horde.Build.Server.Notices
{
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Notice service
	/// </summary>
	public sealed class NoticeService 
	{
		/// <summary>
		/// Notice collection
		/// </summary>
		readonly INoticeCollection _notices;

		/// <summary>
		/// Device service constructor
		/// </summary>
		public NoticeService(INoticeCollection notices)
		{
			_notices = notices;
		}

		/// <summary>
		/// Add a notice
		/// </summary>
		/// <param name="message"></param>
		/// <param name="userId"></param>
		/// <param name="startTime"></param>
		/// <param name="finishTime"></param>
		/// <returns></returns>
		public Task<INotice?> AddNoticeAsync(string message, UserId? userId, DateTime? startTime, DateTime? finishTime)
		{
			return _notices.AddNoticeAsync(message, userId, startTime, finishTime);
		}

		/// <summary>
		/// Update an existing notice
		/// </summary>
		/// <param name="id"></param>
		/// <param name="message"></param>
		/// <param name="startTime"></param>
		/// <param name="finishTime"></param>
		/// <returns></returns>
		public Task<bool> UpdateNoticeAsync(ObjectId id, string? message, DateTime? startTime, DateTime? finishTime)
		{
			return _notices.UpdateNoticeAsync(id, message, startTime, finishTime);
		}

		/// <summary>
		/// Get a notice by id
		/// </summary>
		/// <param name="noticeId"></param>
		/// <returns></returns>
		public Task<INotice?> GetNoticeAsync(ObjectId noticeId)
		{
			return _notices.GetNoticeAsync(noticeId);
		}

		/// <summary>
		/// Get all notices
		/// </summary>
		/// <returns></returns>
		public Task<List<INotice>> GetNoticesAsync()
		{
			return _notices.GetNoticesAsync();
		}

		/// <summary>
		/// Remove an existing notice
		/// </summary>
		/// <param name="id"></param>
		/// <returns></returns>
		public Task<bool> RemoveNoticeAsync(ObjectId id)
		{
			return _notices.RemoveNoticeAsync(id);
		}
	}
}