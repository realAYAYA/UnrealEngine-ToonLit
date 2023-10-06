// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Acls;

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
		public static readonly AclAction CreatePool = new AclAction("CreatePool");

		/// <summary>
		/// Modify an agent pool
		/// </summary>
		public static readonly AclAction UpdatePool = new AclAction("UpdatePool");

		/// <summary>
		/// Delete an agent pool
		/// </summary>
		public static readonly AclAction DeletePool = new AclAction("DeletePool");

		/// <summary>
		/// Ability to view a pool
		/// </summary>
		public static readonly AclAction ViewPool = new AclAction("ViewPool");

		/// <summary>
		/// View all the available agent pools
		/// </summary>
		public static readonly AclAction ListPools = new AclAction("ListPools");
	}
}
