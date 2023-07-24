// Copyright Epic Games, Inc. All Rights Reserved.

using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Tokenizer
{
	/// <summary>
	/// Series of extensions to token reading that are far too specialized to be included in the reader.
	/// </summary>
	public static class UhtTokenReaderSkipExtensions
	{

		/// <summary>
		/// Skip a token regardless of the type.
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader SkipOne(this IUhtTokenReader tokenReader)
		{
			tokenReader.PeekToken();
			tokenReader.ConsumeToken();
			return tokenReader;
		}

		/// <summary>
		/// Skip an alignas expression
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader SkipAlignasIfNecessary(this IUhtTokenReader tokenReader)
		{
			const string Identifier = "alignas";
			if (tokenReader.TryOptional(Identifier))
			{
				tokenReader
					.Require('(', Identifier)
					.RequireConstInt(Identifier)
					.Require(')', Identifier);
			}
			return tokenReader;
		}

		/// <summary>
		/// Skip deprecation macro
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader SkipDeprecatedMacroIfNecessary(this IUhtTokenReader tokenReader)
		{
			if (tokenReader.TryOptional(new string[] { "DEPRECATED", "UE_DEPRECATED" }) >= 0)
			{
				tokenReader
					.Require('(', "deprecation macro")
					.RequireConstFloat("version in deprecation macro")
					.Require(',', "deprecation macro")
					.RequireConstString("message in deprecation macro")
					.Require(')', "deprecation macro");
			}
			return tokenReader;
		}

		/// <summary>
		/// Skip alignas and/or deprecation macros
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader SkipAlignasAndDeprecatedMacroIfNecessary(this IUhtTokenReader tokenReader)
		{
			// alignas() can come before or after the deprecation macro.
			// We can't have both, but the compiler will catch that anyway.
			return tokenReader
				.SkipAlignasIfNecessary()
				.SkipDeprecatedMacroIfNecessary()
				.SkipAlignasIfNecessary();
		}

		/// <summary>
		/// Skip any block of tokens wrapped by the given token symbols
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="initiator">Initiating token (i.e. &quot;(&quot;)</param>
		/// <param name="terminator">Terminating token (i.e. &quot;)&quot;)</param>
		/// <param name="initialNesting">If true, start with an initial nesting count of one (assume we already parsed an initiator)</param>
		/// <param name="exceptionContext">Extra context for any errors</param>
		/// <returns>Token reader</returns>
		/// <exception cref="UhtTokenException">Throw if end of file is reached</exception>
		public static IUhtTokenReader SkipBrackets(this IUhtTokenReader tokenReader, char initiator, char terminator, int initialNesting, object? exceptionContext = null)
		{
			int nesting = initialNesting;
			if (nesting == 0)
			{
				tokenReader.Require(initiator, exceptionContext);
				++nesting;
			}

			do
			{
				UhtToken skipToken = tokenReader.GetToken();
				if (skipToken.TokenType.IsEndType())
				{
					throw new UhtTokenException(tokenReader, skipToken, terminator, exceptionContext);
				}
				else if (skipToken.IsSymbol(initiator))
				{
					++nesting;
				}
				else if (skipToken.IsSymbol(terminator))
				{
					--nesting;
				}
			} while (nesting != 0);
			return tokenReader;
		}

		/// <summary>
		/// Skip tokens until the given terminator is found.  The terminator will not be consumed.
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="terminator">Terminator to skip until</param>
		/// <param name="exceptionContext">Extra context for any exceptions</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader SkipUntil(this IUhtTokenReader tokenReader, char terminator, object? exceptionContext = null)
		{
			while (true)
			{
				ref UhtToken skipToken = ref tokenReader.PeekToken();
				if (skipToken.TokenType.IsEndType())
				{
					throw new UhtTokenException(tokenReader, skipToken, terminator, exceptionContext);
				}
				else if (skipToken.IsSymbol(terminator))
				{
					break;
				}
				tokenReader.ConsumeToken();
			}
			return tokenReader;
		}
	}
}
