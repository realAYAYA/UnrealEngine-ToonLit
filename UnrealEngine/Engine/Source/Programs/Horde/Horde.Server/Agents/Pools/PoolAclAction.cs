// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Agents.Pools
{
	/// <summary>
	/// ACL actions for pools
	/// </summary>
	public static class PoolAclAction
	{
		/// <summary>
		/// Create a global pool of agents
		/// </summary>
		public static AclAction CreatePool { get; } = new AclAction("CreatePool");

		/// <summary>
		/// Modify an agent pool
		/// </summary>
		public static AclAction UpdatePool { get; } = new AclAction("UpdatePool");

		/// <summary>
		/// Delete an agent pool
		/// </summary>
		public static AclAction DeletePool { get; } = new AclAction("DeletePool");

		/// <summary>
		/// Ability to view a pool
		/// </summary>
		public static AclAction ViewPool { get; } = new AclAction("ViewPool");

		/// <summary>
		/// View all the available agent pools
		/// </summary>
		public static AclAction ListPools { get; } = new AclAction("ListPools");
	}
}
