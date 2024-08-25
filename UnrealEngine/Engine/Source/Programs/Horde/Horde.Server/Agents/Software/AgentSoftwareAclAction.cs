// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Agents.Software
{
	/// <summary>
	/// ACL actions for agent software
	/// </summary>
	public static class AgentSoftwareAclAction
	{
		/// <summary>
		/// Ability to upload new versions of the agent software
		/// </summary>
		public static AclAction UploadSoftware { get; } = new AclAction("UploadSoftware");

		/// <summary>
		/// Ability to download the agent software
		/// </summary>
		public static AclAction DownloadSoftware { get; } = new AclAction("DownloadSoftware");

		/// <summary>
		/// Ability to delete agent software
		/// </summary>
		public static AclAction DeleteSoftware { get; } = new AclAction("DeleteSoftware");
	}
}
