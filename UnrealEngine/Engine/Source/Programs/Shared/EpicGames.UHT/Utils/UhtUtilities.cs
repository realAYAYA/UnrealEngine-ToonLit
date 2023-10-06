// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using EpicGames.Core;

namespace EpicGames.UHT.Utils
{

	/// <summary>
	/// Extensions to List that provides some uniqueness support to list elements 
	/// </summary>
	public static class UhtListExtensions
	{

		/// <summary>
		/// Add the given value if it isn't already contained within the list
		/// </summary>
		/// <param name="container">Destination container</param>
		/// <param name="value">Value to be added</param>
		/// <returns>True if the value was added, false if it was already present</returns>
		public static bool AddUnique(this List<string> container, string value)
		{
			if (container.Contains(value, StringComparer.OrdinalIgnoreCase))
			{
				return false;
			}
			container.Add(value);
			return true;
		}

		/// <summary>
		/// Add the given values if they aren't already contained within the list
		/// </summary>
		/// <param name="container">Destination container</param>
		/// <param name="values">Values to be added</param>
		public static void AddUniqueRange(this List<string> container, IEnumerable<StringView>? values)
		{
			if (values != null)
			{
				foreach (StringView value in values)
				{
					AddUnique(container, value.ToString());
				}
			}
		}

		/// <summary>
		/// Remove the given value but swap the last entry into the eliminated slot
		/// </summary>
		/// <param name="container">Container being modified</param>
		/// <param name="value">Value to be removed</param>
		/// <returns>True if the value was removed, false if not</returns>
		public static bool RemoveSwap(this List<string> container, string value)
		{
			int index = container.FindIndex(n => value.Equals(n, StringComparison.OrdinalIgnoreCase));
			if (index >= 0)
			{
				if (index != container.Count - 1)
				{
					container[index] = container[^1];
				}
				container.RemoveAt(container.Count - 1);
				return true;
			}
			return false;
		}

		/// <summary>
		/// Remove a range of values from a container using swapping
		/// </summary>
		/// <param name="container">Container to be modified</param>
		/// <param name="values">List of values to be removed</param>
		public static void RemoveSwapRange(this List<string> container, IEnumerable<StringView>? values)
		{
			if (values != null)
			{
				foreach (StringView value in values)
				{
					RemoveSwap(container, value.ToString());
				}
			}
		}
	}

	/// <summary>
	/// UnrealEngine names often differ from the names in the source file.  The following 
	/// structure represents the different parts of the name
	/// </summary>
	public struct UhtEngineNameParts
	{

		/// <summary>
		/// Any prefix removed from the source name to create the engine name
		/// </summary>
		public StringView Prefix { get; set; }

		/// <summary>
		/// The name to be used by the Unreal Engine.
		/// </summary>
		public StringView EngineName { get; set; }

		/// <summary>
		/// The name contained the "DEPRECATED" text which has been removed from the engine name
		/// </summary>
		public bool IsDeprecated { get; set; }
	}

	/// <summary>
	/// Assorted utility functions
	/// </summary>
	public static class UhtUtilities
	{

		/// <summary>
		/// Given a collection of names, return a string containing the text of those names concatenated.
		/// </summary>
		/// <param name="typeNames">Collect of names to be merged</param>
		/// <param name="andOr">Text used to separate the names</param>
		/// <param name="quote">If true, add quotes around the names</param>
		/// <returns>Merged names</returns>
		public static string MergeTypeNames(IEnumerable<string> typeNames, string andOr, bool quote = false)
		{
			List<string> local = new(typeNames);

			if (local.Count == 0)
			{
				return "";
			}

			local.Sort();

			StringBuilder builder = new();
			for (int index = 0; index < local.Count; ++index)
			{
				if (index != 0)
				{
					builder.Append(", ");
					if (index == local.Count - 1)
					{
						builder.Append(andOr);
						builder.Append(' ');
					}
				}
				if (quote)
				{
					builder.Append('\'');
					builder.Append(local[index]);
					builder.Append('\'');
				}
				else
				{
					builder.Append(local[index]);
				}
			}
			return builder.ToString();
		}

		/// <summary>
		/// Split the given source name into the engine name parts
		/// </summary>
		/// <param name="sourceName">Source name</param>
		/// <returns>Resulting engine name parts</returns>
		public static UhtEngineNameParts GetEngineNameParts(StringView sourceName)
		{
			if (sourceName.Span.Length == 0)
			{
				return new UhtEngineNameParts { Prefix = new StringView(String.Empty), EngineName = new StringView(String.Empty), IsDeprecated = false };
			}

			switch (sourceName.Span[0])
			{
				case 'I':
				case 'A':
				case 'U':
					// If it is a class prefix, check for deprecated class prefix also
					if (sourceName.Span.Length > 12 && sourceName.Span[1..].StartsWith("DEPRECATED_"))
					{
						return new UhtEngineNameParts { Prefix = new StringView(sourceName, 0, 12), EngineName = new StringView(sourceName, 12), IsDeprecated = true };
					}
					else
					{
						return new UhtEngineNameParts { Prefix = new StringView(sourceName, 0, 1), EngineName = new StringView(sourceName, 1), IsDeprecated = false };
					}

				case 'F':
				case 'T':
					// Struct prefixes are also fine.
					return new UhtEngineNameParts { Prefix = new StringView(sourceName, 0, 1), EngineName = new StringView(sourceName, 1), IsDeprecated = false };

				default:
					// If it's not a class or struct prefix, it's invalid
					return new UhtEngineNameParts { Prefix = new StringView(String.Empty), EngineName = new StringView(sourceName), IsDeprecated = false };
			}
		}
	}

	/// <summary>
	/// String builder class that has support for StringView so that if a single instance of
	/// a StringView is appended, it is returned.
	/// </summary>
	public class StringViewBuilder
	{

		/// <summary>
		/// When only a string view has been appended, this references that StringView
		/// </summary>
		private StringView _stringView = new();

		/// <summary>
		/// Represents more complex data being appended
		/// </summary>
		private StringBuilder? _stringBuilder = null;

		/// <summary>
		/// Set to true when the builder has switched to a StringBuilder (NOTE: This can probably be removed)
		/// </summary>
		private bool _useStringBuilder = false;

		/// <summary>
		/// The length of the appended data
		/// </summary>
		public int Length
		{
			get
			{
				if (_useStringBuilder && _stringBuilder != null)
				{
					return _stringBuilder.Length;
				}
				else
				{
					return _stringView.Span.Length;
				}
			}
		}

		/// <summary>
		/// Return the appended data as a StringView
		/// </summary>
		/// <returns>Contents of the builder</returns>
		public StringView ToStringView()
		{
			return _useStringBuilder ? new StringView(_stringBuilder!.ToString()) : _stringView;
		}

		/// <summary>
		/// Return the appended data as a string
		/// </summary>
		/// <returns>Contents of the builder</returns>
		public override string ToString()
		{
			return _useStringBuilder ? _stringBuilder!.ToString() : _stringView.ToString();
		}

		/// <summary>
		/// Append a StringView
		/// </summary>
		/// <param name="text">Text to be appended</param>
		/// <returns>The string builder</returns>
		public StringViewBuilder Append(StringView text)
		{
			if (_useStringBuilder)
			{
				_stringBuilder!.Append(text.Span);
			}
			else if (_stringView.Span.Length > 0)
			{
				SwitchToStringBuilder();
				_stringBuilder!.Append(text.Span);
			}
			else
			{
				_stringView = text;
			}
			return this;
		}

		/// <summary>
		/// Append a character
		/// </summary>
		/// <param name="c">Character to be appended</param>
		/// <returns>The string builder</returns>
		public StringViewBuilder Append(char c)
		{
			SwitchToStringBuilder();
			_stringBuilder!.Append(c);
			return this;
		}

		/// <summary>
		/// If not already done, switch the builder to using a StringBuilder
		/// </summary>
		private void SwitchToStringBuilder()
		{
			if (!_useStringBuilder)
			{
				_stringBuilder ??= new StringBuilder();
				_useStringBuilder = true;
				_stringBuilder.Append(_stringView.Span);
			}
		}
	}
}
