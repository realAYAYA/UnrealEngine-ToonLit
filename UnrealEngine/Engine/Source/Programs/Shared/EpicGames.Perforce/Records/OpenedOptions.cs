// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Options for the p4 opened command
	/// </summary>
	[Flags]
	public enum OpenedOptions
	{
		/// <summary>
		/// No options specified
		/// </summary>
		None = 0,

		/// <summary>
		/// List open files in all client workspaces
		/// </summary>
		AllWorkspaces = 1,

		/// <summary>
		/// Short output; do not output the revision number or file type. This option is more efficient, particularly when using the -a (all-workspaces) option at large sites.
		/// </summary>
		ShortOutput = 2,
	}
}
