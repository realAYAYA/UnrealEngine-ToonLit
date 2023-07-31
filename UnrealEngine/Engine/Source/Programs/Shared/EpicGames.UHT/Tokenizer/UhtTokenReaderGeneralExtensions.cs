// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using EpicGames.Core;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Tokenizer
{

	/// <summary>
	/// Collection of general token reader extensions
	/// </summary>
	public static class UhtTokenReaderGeneralExtensions
	{

		/// <summary>
		/// Try to parse the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>True if the text matched</returns>
		public static bool TryOptional(this IUhtTokenReader tokenReader, string text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier(text) || token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
				return true;
			}
			return false;
		}

		/// <summary>
		/// Try to parse the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>True if the text matched</returns>
		public static bool TryOptional(this IUhtTokenReader tokenReader, StringView text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier(text) || token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
				return true;
			}
			return false;
		}

		/// <summary>
		/// Try to parse the given text.  However, the matching token will not be consumed.
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>True if the text matched</returns>
		public static bool TryPeekOptional(this IUhtTokenReader tokenReader, string text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			return token.IsIdentifier(text) || token.IsSymbol(text);
		}

		/// <summary>
		/// Test to see if the next token is one of the given strings.
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">List of keywords to test</param>
		/// <returns>Index of matched string or -1 if nothing matched</returns>
		public static int TryOptional(this IUhtTokenReader tokenReader, string[] text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			for (int index = 0, endIndex = text.Length; index < endIndex; ++index)
			{
				if (token.IsIdentifier(text[index]) || token.IsSymbol(text[index]))
				{
					tokenReader.ConsumeToken();
					return index;
				}
			}
			return -1;
		}

		/// <summary>
		/// Try to parse the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>True if the text matched</returns>
		public static bool TryOptional(this IUhtTokenReader tokenReader, char text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
				return true;
			}
			return false;
		}

		/// <summary>
		/// Try to parse the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <param name="outToken">Open that was matched</param>
		/// <returns>True if the text matched</returns>
		public static bool TryOptional(this IUhtTokenReader tokenReader, char text, out UhtToken outToken)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsSymbol(text))
			{
				outToken = token;
				tokenReader.ConsumeToken();
				return true;
			}
			outToken = new UhtToken();
			return false;
		}

		/// <summary>
		/// Try to parse the given text.  However, the matching token will not be consumed.
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>True if the text matched</returns>
		public static bool TryPeekOptional(this IUhtTokenReader tokenReader, char text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			return token.IsSymbol(text);
		}

		/// <summary>
		/// Parse optional text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader Optional(this IUhtTokenReader tokenReader, string text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier(text) || token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
			}
			return tokenReader;
		}

		/// <summary>
		/// Parse optional text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <param name="action">Action to invoke if the text was found</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader Optional(this IUhtTokenReader tokenReader, string text, Action action)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier(text) || token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
				action();
			}
			return tokenReader;
		}

		/// <summary>
		/// Parse optional text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader Optional(this IUhtTokenReader tokenReader, char text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
			}
			return tokenReader;
		}

		/// <summary>
		/// Parse optional text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <param name="action">Action to invoke if the text was found</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader Optional(this IUhtTokenReader tokenReader, char text, Action action)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
				action();
			}
			return tokenReader;
		}

		/// <summary>
		/// Parse optional token that starts with the given text 
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader OptionalStartsWith(this IUhtTokenReader tokenReader, string text)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier() && token.ValueStartsWith(text))
			{
				tokenReader.ConsumeToken();
			}
			return tokenReader;
		}

		/// <summary>
		/// Parse optional token that starts with the given text 
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Text to match</param>
		/// <param name="tokenDelegate">Delegate to invoke on a match</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader OptionalStartsWith(this IUhtTokenReader tokenReader, string text, UhtTokenDelegate tokenDelegate)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier() && token.ValueStartsWith(text))
			{
				UhtToken currentToken = token;
				tokenReader.ConsumeToken();
				tokenDelegate(ref currentToken);
			}
			return tokenReader;
		}

		/// <summary>
		/// Require the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Required text</param>
		/// <param name="exceptionContext">Extra exception context</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Thrown if text is not found</exception>
		public static IUhtTokenReader Require(this IUhtTokenReader tokenReader, string text, object? exceptionContext = null)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier(text) || token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
			}
			else
			{
				throw new UhtTokenException(tokenReader, token, text, exceptionContext);
			}
			return tokenReader;
		}

		/// <summary>
		/// Require the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Required text</param>
		/// <param name="tokenDelegate">Delegate to invoke on a match</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Thrown if text is not found</exception>
		public static IUhtTokenReader Require(this IUhtTokenReader tokenReader, string text, UhtTokenDelegate tokenDelegate)
		{
			return tokenReader.Require(text, null, tokenDelegate);
		}

		/// <summary>
		/// Require the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Required text</param>
		/// <param name="exceptionContext">Extra exception context</param>
		/// <param name="tokenDelegate">Delegate to invoke on a match</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Thrown if text is not found</exception>
		public static IUhtTokenReader Require(this IUhtTokenReader tokenReader, string text, object? exceptionContext, UhtTokenDelegate tokenDelegate)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier(text) || token.IsSymbol(text))
			{
				UhtToken currentToken = token;
				tokenReader.ConsumeToken();
				tokenDelegate(ref currentToken);
			}
			else
			{
				throw new UhtTokenException(tokenReader, token, text, exceptionContext);
			}
			return tokenReader;
		}

		/// <summary>
		/// Require the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Required text</param>
		/// <param name="exceptionContext">Extra exception context</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Thrown if text is not found</exception>
		public static IUhtTokenReader Require(this IUhtTokenReader tokenReader, char text, object? exceptionContext = null)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsSymbol(text))
			{
				tokenReader.ConsumeToken();
			}
			else
			{
				throw new UhtTokenException(tokenReader, token, text, exceptionContext);
			}
			return tokenReader;
		}

		/// <summary>
		/// Require the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Required text</param>
		/// <param name="tokenDelegate">Delegate to invoke on a match</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Thrown if text is not found</exception>
		public static IUhtTokenReader Require(this IUhtTokenReader tokenReader, char text, UhtTokenDelegate tokenDelegate)
		{
			return tokenReader.Require(text, null, tokenDelegate);
		}

		/// <summary>
		/// Require the given text
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="text">Required text</param>
		/// <param name="exceptionContext">Extra exception context</param>
		/// <param name="tokenDelegate">Delegate to invoke on a match</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Thrown if text is not found</exception>
		public static IUhtTokenReader Require(this IUhtTokenReader tokenReader, char text, object? exceptionContext, UhtTokenDelegate tokenDelegate)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsSymbol(text))
			{
				UhtToken currentToken = token;
				tokenReader.ConsumeToken();
				tokenDelegate(ref currentToken);
			}
			else
			{
				throw new UhtTokenException(tokenReader, token, text, exceptionContext);
			}
			return tokenReader;
		}
	}
}
