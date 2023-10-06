// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;

namespace EpicGames.BuildGraph
{
	/// <summary>
	/// Defines an artifact produced by the build
	/// </summary>
	public class BgArtifactDef
	{
		/// <summary>
		/// Name of this artifact
		/// </summary>
		public string Name { get; }

		/// <summary>
		/// Tag to use for the artifact. Uses the artifact name by default.
		/// </summary>
		public string Tag { get; }

		/// <summary>
		/// Keys that can be used to find the artifact
		/// </summary>
		public IReadOnlyList<string> Keys { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="name">Name of the artifact</param>
		/// <param name="tag">Name of the tag producing this artifact</param>
		/// <param name="keys">Keys that can be used to find the artifact</param>
		public BgArtifactDef(string name, string tag, IReadOnlyList<string> keys)
		{
			Name = name;
			Tag = tag;
			Keys = keys;
		}

		/// <summary>
		/// Get the name of this badge
		/// </summary>
		/// <returns>The name of this badge</returns>
		public override string ToString()
		{
			return Name;
		}
	}
}
