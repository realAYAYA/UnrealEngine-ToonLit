// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Threading.Tasks;
using Horde.Build.Notifications;
using Horde.Build.Users;
using Horde.Build.Utilities;
using MongoDB.Bson;

namespace Horde.Build.Notifications
{
	using UserId = ObjectId<IUser>;

	/// <summary>
	/// Collection of notification triggers
	/// </summary>
	public interface INotificationTriggerCollection
	{
		/// <summary>
		/// Finds or adds a trigger
		/// </summary>
		/// <param name="triggerId">The trigger id</param>
		/// <returns>New trigger document</returns>
		Task<INotificationTrigger> FindOrAddAsync(ObjectId triggerId);

		/// <summary>
		/// Finds an existing trigger with the given id, or adds one if it does not exist
		/// </summary>
		/// <param name="triggerId"></param>
		/// <returns></returns>
		Task<INotificationTrigger?> GetAsync(ObjectId triggerId);

		/// <summary>
		/// Deletes a trigger
		/// </summary>
		/// <param name="triggerId">The unique trigger id</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(ObjectId triggerId);

		/// <summary>
		/// Deletes a trigger
		/// </summary>
		/// <param name="triggerIds">The unique trigger id</param>
		/// <returns>Async task</returns>
		Task DeleteAsync(List<ObjectId> triggerIds);

		/// <summary>
		/// Fires the trigger, and marks notifications has having been sent
		/// </summary>
		/// <param name="trigger">The trigger to fire</param>
		/// <returns>The trigger document</returns>
		Task<INotificationTrigger?> FireAsync(INotificationTrigger trigger);

		/// <summary>
		/// Adds a subscriber to a particular trigger
		/// </summary>
		/// <param name="trigger">The trigger to subscribe to</param>
		/// <param name="userId">The user name</param>
		/// <param name="email">Whether to receive email notifications</param>
		/// <param name="slack">Whether to receive Slack notifications</param>
		/// <returns>The new trigger state</returns>
		Task<INotificationTrigger?> UpdateSubscriptionsAsync(INotificationTrigger trigger, UserId userId, bool? email, bool? slack);
	}
}
