// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Acls;

namespace Horde.Server.Compute
{
	/// <summary>
	/// ACL actions for compute service
	/// </summary>
	public static class ComputeAclAction
	{
		/// <summary>
		/// User can add tasks to the compute cluster
		/// </summary>
		public static AclAction AddComputeTasks { get; } = new AclAction("AddComputeTasks");
	}
}
