// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Notifications
{
	/// <summary>
	/// ACL actions which apply to notifications
	/// </summary>
	public static class NotificationAclAction
	{
		/// <summary>
		/// Ability to subscribe to notifications
		/// </summary>
		public static AclAction CreateSubscription { get; } = new AclAction("CreateSubscription");
	}
}
