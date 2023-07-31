// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Options for the merge command
	/// </summary>
	[Flags]
	public enum MergeOptions
	{
		/// <summary>
		/// No options
		/// </summary>
		None = 0,

		/// <summary>
		/// Preview the merge
		/// </summary>
		Preview = 1,
	}
}
