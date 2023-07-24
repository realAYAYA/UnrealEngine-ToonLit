// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;

namespace EpicGames.Horde
{
	/// <summary>
	/// Normalized string identifier for a resource
	/// </summary>
	public struct StringId : IEquatable<StringId>
	{
		/// <summary>
		/// The text representing this id
		/// </summary>
		public Utf8String Text { get; }

		/// <summary>
		/// Accessor for the string bytes
		/// </summary>
		public ReadOnlySpan<byte> Span => Text.Span;

		/// <summary>
		/// Accessor for the string bytes
		/// </summary>
		public ReadOnlyMemory<byte> Memory => Text.Memory;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="text">Unique id for the string</param>
		public StringId(Utf8String text)
		{
			Text = ValidateArgument(text, nameof(text));
		}

		/// <summary>
		/// Checks whether this StringId is set
		/// </summary>
		public bool IsEmpty => Text.IsEmpty;

		/// <summary>
		/// Validates the given string as a StringId, normalizing it if necessary.
		/// </summary>
		/// <param name="text">Text to validate as a StringId</param>
		/// <param name="paramName">Name of the parameter to show if invalid characters are returned.</param>
		/// <returns></returns>
		public static Utf8String ValidateArgument(Utf8String text, string paramName)
		{
			if (text.Length == 0)
			{
				throw new ArgumentException("String id may not be empty", paramName);
			}

			const int MaxLength = 64;
			if (text.Length > MaxLength)
			{
				throw new ArgumentException($"String id may not be longer than {MaxLength} characters", paramName);
			}

			for (int idx = 0; idx < text.Length; idx++)
			{
				char character = (char)text[idx];
				if (!IsValidCharacter(character))
				{
					if (character >= 'A' && character <= 'Z')
					{
						text = ToLower(text);
					}
					else
					{
						throw new ArgumentException($"{text} is not a valid string id", paramName);
					}
				}
			}

			return text;
		}

		/// <summary>
		/// Converts a utf8 string to lowercase
		/// </summary>
		/// <param name="text"></param>
		/// <returns></returns>
		static Utf8String ToLower(Utf8String text)
		{
			byte[] output = new byte[text.Length];
			for (int idx = 0; idx < text.Length; idx++)
			{
				byte character = text[idx];
				if (character >= 'A' && character <= 'Z')
				{
					character = (byte)((character - 'A') + 'a');
				}
				output[idx] = character;
			}
			return new Utf8String(output);
		}

		/// <summary>
		/// Checks whether the given character is valid within a string id
		/// </summary>
		/// <param name="character">The character to check</param>
		/// <returns>True if the character is valid</returns>
		static bool IsValidCharacter(char character)
		{
			if (character >= 'a' && character <= 'z')
			{
				return true;
			}
			if (character >= '0' && character <= '9')
			{
				return true;
			}
			if (character == '-' || character == '_' || character == '.')
			{
				return true;
			}
			return false;
		}

		/// <inheritdoc/>
		public override bool Equals(object? obj) => obj is StringId id && Equals(id);

		/// <inheritdoc/>
		public override int GetHashCode() => Text.GetHashCode();

		/// <inheritdoc/>
		public bool Equals(StringId other) => Text.Equals(other.Text);

		/// <inheritdoc/>
		public override string ToString() => Text.ToString();

		/// <summary>
		/// Compares two string ids for equality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are equal</returns>
		public static bool operator ==(StringId left, StringId right) => left.Equals(right);

		/// <summary>
		/// Compares two string ids for inequality
		/// </summary>
		/// <param name="left">The first string id</param>
		/// <param name="right">Second string id</param>
		/// <returns>True if the two string ids are not equal</returns>
		public static bool operator !=(StringId left, StringId right) => !left.Equals(right);
	}
}
