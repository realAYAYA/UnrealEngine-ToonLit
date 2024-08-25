// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace EpicGames.ProjectStore
{
	/// <summary>
	/// A specification of storage details with zenserver
	/// </summary>
	public class ZenServerStoreData
	{
		/// <summary>
		/// Whether zenserver is running locally
		/// </summary>
		public bool IsLocalHost { get; set; } = true;
		/// <summary>
		/// The primary or local host name for zenserver
		/// </summary>
		public string? HostName { get; set; } = "localhost";
		/// <summary>
		/// The list of remote host names for zenserver
		/// </summary>
		public List<string> RemoteHostNames { get; } = new List<string>();
		/// <summary>
		/// The port for zenserver on the host
		/// </summary>
		public int HostPort { get; set; } = 8558;
		/// <summary>
		/// The project identifier for the stored data in zenserver
		/// </summary>
		public string? ProjectId { get; set; }
		/// <summary>
		/// The oplog identifier for the stored data in zenserver
		/// </summary>
		public string? OplogId { get; set; }
	}
	/// <summary>
	/// A desctiptor of a project store
	/// </summary>
	public class ProjectStoreData
	{
		/// <summary>
		/// The details for zenserver for this project store
		/// </summary>
		public ZenServerStoreData? ZenServer { get; set; }
	}
}
