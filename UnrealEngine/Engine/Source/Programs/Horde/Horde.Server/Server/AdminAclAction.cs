// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Server
{
	/// <summary>
	/// ACL actions for admin operations
	/// </summary>
	public static class AdminAclAction
	{
		/// <summary>
		/// Ability to read any data from the server. Always inherited.
		/// </summary>
		public static AclAction AdminRead { get; } = new AclAction("AdminRead");

		/// <summary>
		/// Ability to write any data to the server.
		/// </summary>
		public static AclAction AdminWrite { get; } = new AclAction("AdminWrite");
	}
}
