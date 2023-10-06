// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Acls;

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
		public static readonly AclAction CreateBisectTask = new AclAction("CreateBisectTask");

		/// <summary>
		/// Ability to update a bisect task
		/// </summary>
		public static readonly AclAction UpdateBisectTask = new AclAction("UpdateBisectTask");

		/// <summary>
		/// Ability to view a bisect task
		/// </summary>
		public static readonly AclAction ViewBisectTask = new AclAction("ViewBisectTask");
	}
}
