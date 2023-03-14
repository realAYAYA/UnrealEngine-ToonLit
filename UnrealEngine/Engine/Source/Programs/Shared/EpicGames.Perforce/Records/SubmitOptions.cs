// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Options for the 'submit' command
	/// </summary>
	[Flags]
	public enum SubmitOptions
	{
		/// <summary>
		/// No options specified
		/// </summary>
		None = 0,

		/// <summary>
		/// Reopen files for edit in the default changelist after submission.
		/// </summary>
		ReopenAsEdit = 1,
	}
}
