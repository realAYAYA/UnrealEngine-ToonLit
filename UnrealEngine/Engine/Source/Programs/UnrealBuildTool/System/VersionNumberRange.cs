// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Text;

namespace UnrealBuildTool
{
	/// <summary>
	/// Range of version numbers
	/// </summary>
	class VersionNumberRange
	{
		/// <summary>
		/// Minimum version number
		/// </summary>
		public VersionNumber Min { get; }

		/// <summary>
		/// Maximum version number
		/// </summary>
		public VersionNumber Max { get; }

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Min"></param>
		/// <param name="Max"></param>
		public VersionNumberRange(VersionNumber Min, VersionNumber Max)
		{
			this.Min = Min;
			this.Max = Max;
		}

		/// <summary>
		/// Tests whether this range contains the given version
		/// </summary>
		/// <param name="Version"></param>
		/// <returns></returns>
		public bool Contains(VersionNumber Version)
		{
			return Version >= Min && Version <= Max;
		}

		/// <summary>
		/// Parse a version range from two strings
		/// </summary>
		/// <param name="MinText"></param>
		/// <param name="MaxText"></param>
		/// <returns></returns>
		public static VersionNumberRange Parse(string MinText, string MaxText)
		{
			return new VersionNumberRange(VersionNumber.Parse(MinText), VersionNumber.Parse(MaxText));
		}

		/// <inheritdoc/>
		public override string ToString()
		{
			return String.Format("{0}-{1}", Min, Max);
		}
	}
}
