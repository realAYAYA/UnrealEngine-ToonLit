// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores a numeric version consisting of any number of components.
	/// </summary>
	[Serializable]
	class VersionNumber : IComparable<VersionNumber>
	{
		/// <summary>
		/// Set of delimiters for version numbers
		/// </summary>
		static readonly char[] Delimiters = { '.', ',' };

		/// <summary>
		/// The individual version components
		/// </summary>
		int[] Components;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Components">The individual version components. At least one value must be given.</param>
		public VersionNumber(params int[] Components)
		{
			if(Components.Length == 0)
			{
				throw new InvalidOperationException("Version number must have at least one component");
			}
			this.Components = Components;
		}

		/// <summary>
		/// Returns the component at the given index
		/// </summary>
		/// <param name="Idx">The zero-based component index to return</param>
		/// <returns>The component at the given index</returns>
		public int GetComponent(int Idx)
		{
			return Components[Idx];
		}

		/// <summary>
		/// Tests two objects for equality. VersionNumber behaves like a value type.
		/// </summary>
		/// <param name="Obj">Object to compare against</param>
		/// <returns>True if the objects are equal, false otherwise.</returns>
		public override bool Equals(object? Obj)
		{
			VersionNumber? Version = Obj as VersionNumber;
			return !ReferenceEquals(Version, null) && this == Version;
		}

		/// <summary>
		/// Returns a hash of the version number.
		/// </summary>
		/// <returns>A hash value for the version number.</returns>
		public override int GetHashCode()
		{
			int Result = 5831;
			for(int Idx = 0; Idx < Components.Length; Idx++)
			{
				Result = (Result * 33) + Components[Idx];
			}
			return Result;
		}

		/// <summary>
		/// Compares whether two versions are equal.
		/// </summary>
		/// <param name="Lhs">The first version number</param>
		/// <param name="Rhs">The second version number</param>
		/// <returns>True if the versions are equal.</returns>
		public static bool operator==(VersionNumber? Lhs, VersionNumber? Rhs)
		{
			if(Object.ReferenceEquals(Lhs, null))
			{
				return Object.ReferenceEquals(Rhs, null);
			}
			else
			{
				return !Object.ReferenceEquals(Rhs, null) && Compare(Lhs, Rhs) == 0;
			}
		}

		/// <summary>
		/// Compares whether two versions are not equal.
		/// </summary>
		/// <param name="Lhs">The first version number</param>
		/// <param name="Rhs">The second version number</param>
		/// <returns>True if the versions are not equal.</returns>
		public static bool operator!=(VersionNumber? Lhs, VersionNumber? Rhs)
		{
			return !(Lhs == Rhs);
		}

		/// <summary>
		/// Compares whether one version is less than another.
		/// </summary>
		/// <param name="Lhs">The first version number</param>
		/// <param name="Rhs">The second version number</param>
		/// <returns>True if the first version is less than the second.</returns>
		public static bool operator<(VersionNumber Lhs, VersionNumber Rhs)
		{
			return Compare(Lhs, Rhs) < 0;
		}

		/// <summary>
		/// Compares whether one version is less or equal to another.
		/// </summary>
		/// <param name="Lhs">The first version number</param>
		/// <param name="Rhs">The second version number</param>
		/// <returns>True if the first version is less or equal to the second.</returns>
		public static bool operator<=(VersionNumber Lhs, VersionNumber Rhs)
		{
			return Compare(Lhs, Rhs) <= 0;
		}

		/// <summary>
		/// Compares whether one version is greater than another.
		/// </summary>
		/// <param name="Lhs">The first version number</param>
		/// <param name="Rhs">The second version number</param>
		/// <returns>True if the first version is greater than the second.</returns>
		public static bool operator>(VersionNumber Lhs, VersionNumber Rhs)
		{
			return Compare(Lhs, Rhs) > 0;
		}

		/// <summary>
		/// Compares whether one version is greater or equal to another.
		/// </summary>
		/// <param name="Lhs">The first version number</param>
		/// <param name="Rhs">The second version number</param>
		/// <returns>True if the first version is greater or equal to the second.</returns>
		public static bool operator>=(VersionNumber Lhs, VersionNumber Rhs)
		{
			return Compare(Lhs, Rhs) >= 0;
		}

		/// <summary>
		/// Comparison function for IComparable
		/// </summary>
		/// <param name="Other">Other version number to compare to</param>
		/// <returns>A negative value if this version is before Other, a positive value if this version is after Other, and zero otherwise.</returns>
		public int CompareTo(VersionNumber? Other)
		{
			return ReferenceEquals(Other, null)? 1 : Compare(this, Other);
		}

		/// <summary>
		/// Compares two version numbers and returns an integer indicating their order
		/// </summary>
		/// <param name="Lhs">The first version to check</param>
		/// <param name="Rhs">The second version to check</param>
		/// <returns>A negative value if Lhs is before Rhs, a positive value if Lhs is after Rhs, and zero otherwise.</returns>
		public static int Compare(VersionNumber Lhs, VersionNumber Rhs)
		{
			for(int Idx = 0;;Idx++)
			{
				if(Idx == Lhs.Components.Length)
				{
					if(Idx == Rhs.Components.Length)
					{
						return 0;
					}
					else
					{
						return -1;
					}
				}
				else
				{
					if(Idx == Rhs.Components.Length)
					{
						return +1;
					}
					else if(Lhs.Components[Idx] != Rhs.Components[Idx])
					{
						return Lhs.Components[Idx] - Rhs.Components[Idx];
					}
				}
			}
		}

		/// <summary>
		/// Parses the version number from a string
		/// </summary>
		/// <param name="Text">The string to parse</param>
		/// <returns>A version number object</returns>
		public static VersionNumber Parse(string Text)
		{
			List<int> Components = new List<int>();
			foreach(string TextElement in Text.Split(Delimiters))
			{
				Components.Add(int.Parse(TextElement));
			}
			return new VersionNumber(Components.ToArray());
		}

		/// <summary>
		/// Parses the version number from a string
		/// </summary>
		/// <param name="Text">The string to parse</param>
		/// <param name="OutNumber">Variable to receive the parsed version number</param>
		/// <returns>A version number object</returns>
		public static bool TryParse(string Text, [NotNullWhen(true)] out VersionNumber? OutNumber)
		{
			List<int> Components = new List<int>();
			foreach(string TextElement in Text.Split(Delimiters))
			{
				int Component;
				if(!int.TryParse(TextElement, out Component))
				{
					OutNumber = null;
					return false;
				}
				Components.Add(Component);
			}

			OutNumber = new VersionNumber(Components.ToArray());
			return true;
		}

		/// <summary>
		/// Returns a string version number, eg. 1.4
		/// </summary>
		/// <returns>The stringized version number</returns>
		public override string ToString()
		{
			StringBuilder Result = new StringBuilder();
			if(Components.Length > 0)
			{
				Result.Append(Components[0]);
				for(int Idx = 1; Idx < Components.Length; Idx++)
				{
					Result.Append('.');
					Result.Append(Components[Idx]);
				}
			}
			return Result.ToString();
		}
	}
}
