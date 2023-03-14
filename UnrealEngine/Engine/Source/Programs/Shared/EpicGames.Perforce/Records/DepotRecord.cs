// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Information about a file opened for delete
	/// </summary>
	public class DepotRecord
	{
		/// <summary>
		/// Depot name
		/// </summary>
		[PerforceTag("Depot")]
		public string Depot { get; set; } = String.Empty;

		/// <summary>
		/// Owner of the depot
		/// </summary>
		[PerforceTag("Owner", Optional = true)]
		public string Owner { get; set; } = String.Empty;

		/// <summary>
		/// Creation time
		/// </summary>
		[PerforceTag("Date", Optional = true)]
		public DateTime Date { get; set; }

		/// <summary>
		/// Creation time
		/// </summary>
		[PerforceTag("Type", Optional = true)]
		public string Type { get; set; } = String.Empty;

		/// <summary>
		/// Creation time
		/// </summary>
		[PerforceTag("Description", Optional = true)]
		public string Description { get; set; } = String.Empty;
	}
}
