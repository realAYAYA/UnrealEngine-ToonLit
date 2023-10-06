// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Acls;

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
	}
}
