// Copyright Epic Games, Inc. All Rights Reserved.

using Horde.Server.Acls;

namespace Horde.Server.Storage
{
	/// <summary>
	/// ACL actions which apply to storage
	/// </summary>
	public static class StorageAclAction
	{
		/// <summary>
		/// Ability to read blobs from the storage service
		/// </summary>
		public static readonly AclAction ReadBlobs = new AclAction("ReadBlobs");

		/// <summary>
		/// Ability to write blobs to the storage service
		/// </summary>
		public static readonly AclAction WriteBlobs = new AclAction("WriteBlobs");

		/// <summary>
		/// Ability to read refs from the storage service
		/// </summary>
		public static readonly AclAction ReadRefs = new AclAction("ReadRefs");

		/// <summary>
		/// Ability to write refs to the storage service
		/// </summary>
		public static readonly AclAction WriteRefs = new AclAction("WriteRefs");

		/// <summary>
		/// Ability to delete refs
		/// </summary>
		public static readonly AclAction DeleteRefs = new AclAction("DeleteRefs");
	}
}
