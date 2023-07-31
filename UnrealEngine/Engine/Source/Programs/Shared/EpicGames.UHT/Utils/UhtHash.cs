// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;

namespace EpicGames.UHT.Utils
{

	/// <summary>
	/// Support for generating hash values using the existing UHT algorithms
	/// </summary>
	public struct UhtHash
	{
		private ulong _hash;

		/// <summary>
		/// Start the hash computation
		/// </summary>
		public void Begin()
		{
			_hash = 0;
		}

		/// <summary>
		/// Add a span of text to the current hash value
		/// </summary>
		/// <param name="text"></param>
		public void Add(ReadOnlySpan<char> text)
		{
			while (text.Length != 0)
			{
				int crPos = text.IndexOf('\r');
				if (crPos == -1)
				{
					HashText(text[..]);
					break;
				}
				else
				{
					HashText(text[..crPos]);
					text = text[(crPos + 1)..];
				}
			}
		}

		/// <summary>
		/// Return the final hash value
		/// </summary>
		/// <returns>Final hash value</returns>
		public uint End()
		{
			return (uint)(_hash + (_hash >> 32));
		}

		/// <summary>
		/// Generate the hash value for a block of text
		/// </summary>
		/// <param name="text">Text to hash</param>
		/// <returns>Hash value</returns>
		public static uint GenenerateTextHash(ReadOnlySpan<char> text)
		{
			UhtHash hash = new();
			hash.Begin();
			hash.Add(text);
			return hash.End();
		}

		private void HashText(ReadOnlySpan<char> text)
		{
			if (text.Length > 0)
			{
				unsafe
				{
					fixed (char* textPtr = text)
					{
						_hash = CityHash.CityHash64WithSeed((byte*)textPtr, (uint)(text.Length * sizeof(char)), _hash);
					}
				}
			}
		}
	}
}
