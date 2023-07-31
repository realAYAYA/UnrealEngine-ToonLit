// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace EpicGames.Perforce
{
	/// <summary>
	/// Wrapper for a list of filespecs. Allows implicit conversion from string (a single entry) or list.
	/// </summary>
	public struct FileSpecList
	{
		/// <summary>
		/// Empty filespec list
		/// </summary>
		public static FileSpecList Empty { get; } = new FileSpecList(new List<string>());

		/// <summary>
		/// Matches any files in the depot
		/// </summary>
		public static FileSpecList Any { get; } = new FileSpecList(new List<string> { "//..." });

		/// <summary>
		/// The list of filespecs
		/// </summary>
		public IReadOnlyList<string> List { get; }

		/// <summary>
		/// Private constructor. Use implicit conversion operators below instead.
		/// </summary>
		/// <param name="fileSpecList">List of filespecs</param>
		private FileSpecList(IReadOnlyList<string> fileSpecList)
		{
			List = fileSpecList;
		}

		/// <summary>
		/// Implicit conversion operator from a list of filespecs
		/// </summary>
		/// <param name="list">The list to construct from</param>
		public static implicit operator FileSpecList(List<string> list)
		{
			return new FileSpecList(list);
		}

		/// <summary>
		/// Implicit conversion operator from an array of filespecs
		/// </summary>
		/// <param name="array">The array to construct from</param>
		public static implicit operator FileSpecList(string[] array)
		{
			return new FileSpecList(array);
		}

		/// <summary>
		/// Implicit conversion operator from a single filespec
		/// </summary>
		/// <param name="fileSpec">The single filespec to construct from</param>
		public static implicit operator FileSpecList(string fileSpec)
		{
			return new FileSpecList(new string[] { fileSpec });
		}
	}
}
