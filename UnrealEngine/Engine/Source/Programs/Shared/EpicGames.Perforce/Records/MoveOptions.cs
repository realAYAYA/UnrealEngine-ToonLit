// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Options for the p4 edit command
	/// </summary>
	[Flags]
	public enum MoveOptions
	{
		/// <summary>
		/// No options specified
		/// </summary>
		None = 0,

		/// <summary>
		/// Keep existing workspace files; mark the file as open for edit even if the file is not in the client view. 
		/// </summary>
		KeepWorkspaceFiles = 1,

		/// <summary>
		/// Rename existing files without altering content or type to a new target.
		/// </summary>
		RenameOnly = 2,

		/// <summary>
		/// Preview which files would be opened for edit, without actually changing any files or metadata.
		/// </summary>
		PreviewOnly = 4,
	}
}
