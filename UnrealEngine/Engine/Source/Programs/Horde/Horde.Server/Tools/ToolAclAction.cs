// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Tools
{
	/// <summary>
	/// ACL actions which apply to tools
	/// </summary>
	public static class ToolAclAction
	{
		/// <summary>
		/// Ability to download a tool
		/// </summary>
		public static AclAction DownloadTool { get; } = new AclAction("DownloadTool");

		/// <summary>
		/// Ability to upload new tool versions
		/// </summary>
		public static AclAction UploadTool { get; } = new AclAction("UploadTool");
	}
}
