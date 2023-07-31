// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Perforce
{
	/// <summary>
	/// Action performed to the file during a sync
	/// </summary>
	public enum SyncAction
	{
		/// <summary>
		/// Unknown value
		/// </summary>
		[PerforceEnum("none")]
		Unknown,

		/// <summary>
		/// The file was added during the sync
		/// </summary>
		[PerforceEnum("added")]
		Added,

		/// <summary>
		/// The file was updated during the sync
		/// </summary>
		[PerforceEnum("updated")]
		Updated,

		/// <summary>
		/// The file was deleted during the sync
		/// </summary>
		[PerforceEnum("deleted")]
		Deleted,
	}
}
