// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Agents.Leases
{
	/// <summary>
	/// ACL actions which apply to leases
	/// </summary>
	public static class LeaseAclAction
	{
		/// <summary>
		/// View all the leases that an agent has worked on
		/// </summary>
		public static AclAction ViewLeases { get; } = new AclAction("ViewLeases");

		/// <summary>
		/// View the task data for a lease
		/// </summary>
		public static AclAction ViewLeaseTasks { get; } = new AclAction("ViewLeaseTasks");
	}
}
