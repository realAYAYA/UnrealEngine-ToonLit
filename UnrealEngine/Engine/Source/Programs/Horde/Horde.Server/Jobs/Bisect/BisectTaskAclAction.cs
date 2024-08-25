// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Jobs.Bisect
{
	/// <summary>
	/// ACL actions valid for manipulating jobs
	/// </summary>
	public static class BisectTaskAclAction
	{
		/// <summary>
		/// Ability to start new bisect tasks
		/// </summary>
		public static AclAction CreateBisectTask { get; } = new AclAction("CreateBisectTask");

		/// <summary>
		/// Ability to update a bisect task
		/// </summary>
		public static AclAction UpdateBisectTask { get; } = new AclAction("UpdateBisectTask");

		/// <summary>
		/// Ability to view a bisect task
		/// </summary>
		public static AclAction ViewBisectTask { get; } = new AclAction("ViewBisectTask");
	}
}
