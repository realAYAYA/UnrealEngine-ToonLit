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
	/// A collection of user specified notices
	/// </summary>
	public interface INoticeCollection
	{
		/// <summary>
		/// Add a notice to the collection
		/// </summary>
		/// <param name="message"></param>
		/// <param name="userId"></param>
		/// <param name="startTime"></param>
		/// <param name="finishTime"></param>
		/// <returns></returns>
		Task<INotice?> AddNoticeAsync(string message, UserId? userId, DateTime? startTime, DateTime? finishTime);

		/// <summary>
		/// Update an existing notice
		/// </summary>
		/// <param name="id"></param>
		/// <param name="message"></param>
		/// <param name="startTime"></param>
		/// <param name="finishTime"></param>
		/// <returns></returns>
		Task<bool> UpdateNoticeAsync(ObjectId id, string? message, DateTime? startTime, DateTime? finishTime);

		/// <summary>
		/// Get a notice by id
		/// </summary>
		/// <param name="noticeId"></param>
		/// <returns></returns>
		Task<INotice?> GetNoticeAsync(ObjectId noticeId);

		/// <summary>
		/// Get all notices
		/// </summary>
		/// <returns></returns>
		Task<List<INotice>> GetNoticesAsync();

		/// <summary>
		/// Remove a notice
		/// </summary>
		/// <param name="id"></param>
		/// <returns></returns>
		Task<bool> RemoveNoticeAsync(ObjectId id);
	}
}