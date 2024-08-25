// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.Horde.Acls;

namespace Horde.Server.Logs
{
	/// <summary>
	/// ACL actions for logs
	/// </summary>
	public static class LogAclAction
	{
		/// <summary>
		/// Ability to create a log. Implicitly granted to agents.
		/// </summary>
		public static AclAction CreateLog { get; } = new AclAction("CreateLog");

		/// <summary>
		/// Ability to update log metadata
		/// </summary>
		public static AclAction UpdateLog { get; } = new AclAction("UpdateLog");

		/// <summary>
		/// Ability to view a log contents
		/// </summary>
		public static AclAction ViewLog { get; } = new AclAction("ViewLog");

		/// <summary>
		/// Ability to write log data
		/// </summary>
		public static AclAction WriteLogData { get; } = new AclAction("WriteLogData");

		/// <summary>
		/// Ability to create events
		/// </summary>
		public static AclAction CreateEvent { get; } = new AclAction("CreateEvent");

		/// <summary>
		/// Ability to view events
		/// </summary>
		public static AclAction ViewEvent { get; } = new AclAction("ViewEvent");
	}
}
