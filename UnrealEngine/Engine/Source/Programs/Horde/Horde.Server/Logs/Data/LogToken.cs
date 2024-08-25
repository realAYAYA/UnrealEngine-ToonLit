// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;

namespace Horde.Server.Logs.Data
{
	/// <summary>
	/// Functionality for decomposing log text into tokens
	/// </summary>
	public static class LogToken
	{
		/// <summary>
		/// Maximum number of bytes in each token
		/// </summary>
		public const int MaxTokenBytes = 4;

		/// <summary>
		/// Number of bits in each token
		/// </summary>
		public const int MaxTokenBits = MaxTokenBytes * 8;

		/// <summary>
		/// Lookup from input byte to token type
		/// </summary>
		static readonly byte[] s_tokenTypes = GetTokenTypes();

		/// <summary>
		/// Lookup from input byte to token char
		/// </summary>
		static readonly byte[] s_tokenChars = GetTokenChars();

		/// <summary>
		/// Get a set of tokens in a trie
		/// </summary>
		/// <param name="text">Text to scan</param>
		/// <returns></returns>
		public static ReadOnlyTrie GetTrie(ReadOnlySpan<byte> text)
		{
			ReadOnlyTrieBuilder builder = new ReadOnlyTrieBuilder();
			GetTokens(text, builder.Add);
			return builder.Build();
		}

		/// <summary>
		/// Gets a single token
		/// </summary>
		/// <param name="text">The text to parse</param>
		/// <returns>The token value</returns>
		public static ulong GetToken(ReadOnlySpan<byte> text)
		{
			ulong token = 0;
			GetTokens(text, x => token = x);
			return token;
		}

		/// <summary>
		/// Get a set of tokens in a trie
		/// </summary>
		/// <param name="text">Text to scan</param>
		/// <returns></returns>
		public static HashSet<ulong> GetTokenSet(ReadOnlySpan<byte> text)
		{
			HashSet<ulong> tokens = new HashSet<ulong>();
			GetTokens(text, x => tokens.Add(x));
			return tokens;
		}

		/// <summary>
		/// Decompose a span of text into tokens
		/// </summary>
		/// <param name="text">Text to scan</param>
		/// <param name="addToken">Receives a set of tokens</param>
		public static void GetTokens(ReadOnlySpan<byte> text, Action<ulong> addToken)
		{
			if (text.Length > 0)
			{
				int type = s_tokenTypes[text[0]];
				int numTokenBits = 8;
				ulong token = s_tokenChars[text[0]];

				for (int textIdx = 1; textIdx < text.Length; textIdx++)
				{
					byte nextChar = s_tokenChars[text[textIdx]];
					int nextType = s_tokenTypes[nextChar];
					if (type != nextType || numTokenBits + 8 > MaxTokenBits)
					{
						addToken(token << (MaxTokenBits - numTokenBits));
						token = 0;
						numTokenBits = 0;
						type = nextType;
					}
					token = (token << 8) | nextChar;
					numTokenBits += 8;
				}

				addToken(token << (MaxTokenBits - numTokenBits));
			}
		}

		/// <summary>
		/// Gets the length of the first token in the given span
		/// </summary>
		/// <param name="text">The text to search</param>
		/// <param name="pos">Start position for the search</param>
		/// <returns>Length of the first token</returns>
		public static ReadOnlySpan<byte> GetTokenText(ReadOnlySpan<byte> text, int pos)
		{
			int type = s_tokenTypes[text[pos]];
			for (int end = pos + 1; ; end++)
			{
				if (end == text.Length || s_tokenTypes[text[end]] != type)
				{
					return text.Slice(pos, end - pos);
				}
			}
		}

		/// <summary>
		/// Gets the length of the first token in the given span
		/// </summary>
		/// <param name="text">The text to search</param>
		/// <param name="offset">Offset of the window to read from the token</param>
		/// <returns>Length of the first token</returns>
		public static ulong GetWindowedTokenValue(ReadOnlySpan<byte> text, int offset)
		{
			ulong token = 0;
			for (int idx = 0; idx < MaxTokenBytes; idx++)
			{
				token <<= 8;
				if (offset >= 0 && offset < text.Length)
				{
					token |= s_tokenChars[text[offset]];
				}
				offset++;
			}
			return token;
		}

		/// <summary>
		/// Gets the length of the first token in the given span
		/// </summary>
		/// <param name="text">The text to search</param>
		/// <param name="offset">Offset of the window to read from the token</param>
		/// <param name="allowPartialMatch">Whether to allow only matching the start of the string</param>
		/// <returns>Length of the first token</returns>
		public static ulong GetWindowedTokenMask(ReadOnlySpan<byte> text, int offset, bool allowPartialMatch)
		{
			ulong token = 0;
			for (int idx = 0; idx < MaxTokenBytes; idx++)
			{
				token <<= 8;
				if (offset >= 0 && (offset < text.Length || !allowPartialMatch))
				{
					token |= 0xff;
				}
				offset++;
			}
			return token;
		}

		/// <summary>
		/// Build the lookup table for token types
		/// </summary>
		/// <returns>Array whose elements map from an input byte to token type</returns>
		static byte[] GetTokenTypes()
		{
			byte[] charTypes = new byte[256];
			for (int idx = 'a'; idx <= 'z'; idx++)
			{
				charTypes[idx] = 1;
			}
			for (int idx = 'A'; idx <= 'Z'; idx++)
			{
				charTypes[idx] = 1;
			}
			for (int idx = '0'; idx <= '9'; idx++)
			{
				charTypes[idx] = 2;
			}
			charTypes[' '] = 3;
			charTypes['\t'] = 3;
			charTypes['\n'] = 4;
			return charTypes;
		}

		/// <summary>
		/// Build the lookup table for token types
		/// </summary>
		/// <returns>Array whose elements map from an input byte to token type</returns>
		static byte[] GetTokenChars()
		{
			byte[] chars = new byte[256];
			for (int idx = 0; idx < 256; idx++)
			{
				chars[idx] = (byte)idx;
			}
			for (int idx = 'A'; idx <= 'Z'; idx++)
			{
				chars[idx] = (byte)('a' + idx - 'A');
			}
			return chars;
		}
	}
}
