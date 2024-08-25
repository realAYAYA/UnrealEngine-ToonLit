// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Ddc
{
	/// <summary>
	/// ACL actions for DDC
	/// </summary>
	public static class JupiterAclAction
	{
		/// <summary>
		/// General read access to refs / blobs and so on
		/// </summary>
		public static AclAction ReadObject { get; } = new AclAction("DdcReadObject");

		/// <summary>
		/// General write access to upload refs / blobs etc
		/// </summary>
		public static AclAction WriteObject { get; } = new AclAction("DdcWriteObject");

		/// <summary>
		/// Access to delete blobs / refs etc
		/// </summary>
		public static AclAction DeleteObject { get; } = new AclAction("DdcDeleteObject");

		/// <summary>
		/// Access to delete a particular bucket
		/// </summary>
		public static AclAction DeleteBucket { get; } = new AclAction("DdcDeleteBucket");

		/// <summary>
		/// Access to delete a whole namespace
		/// </summary>
		public static AclAction DeleteNamespace { get; } = new AclAction("DdcDeleteNamespace");

		/// <summary>
		/// Access to read the transaction log
		/// </summary>
		public static AclAction ReadTransactionLog { get; } = new AclAction("DdcReadTransactionLog");

		/// <summary>
		/// Access to write the transaction log
		/// </summary>
		public static AclAction WriteTransactionLog { get; } = new AclAction("DdcWriteTransactionLog");

		/// <summary>
		/// Access to perform administrative task
		/// </summary>
		public static AclAction AdminAction { get; } = new AclAction("DdcAdminAction");
	}
}
