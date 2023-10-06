// Copyright Epic Games, Inc. All Rights Reserved.

using System;

namespace EpicGames.Box
{
	/// <summary>
	/// Box utils
	/// </summary>
	public static partial class Utils
	{
		static Lazy<bool> Available { get; set; } = new Lazy<bool>(() => false);

		/// <summary>
		/// Is box available?
		/// </summary>
		public static bool IsAvailable() => Available.Value;
	}
}
