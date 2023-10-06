// Copyright Epic Games, Inc. All Rights Reserved.

namespace EpicGames.Perforce
{
	/// <summary>
	/// The type of a changelist
	/// </summary>
	public enum ChangeType
	{
		/// <summary>
		/// When creating a new changelist, leaves the changelist type unspecified.
		/// </summary>
		Unspecified,

		/// <summary>
		/// This change is visible to anyone
		/// </summary>
		[PerforceEnum("public")]
		Public,

		/// <summary>
		/// This change is restricted
		/// </summary>
		[PerforceEnum("restricted")]
		Restricted,
	}
}
