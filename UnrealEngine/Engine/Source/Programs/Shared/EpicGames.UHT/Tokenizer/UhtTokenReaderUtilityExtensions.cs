// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Tokenizer
{

	/// <summary>
	/// Collection of assorted utility token reader extensions
	/// </summary>
	public static class UhtTokenReaderUtilityExtensions
	{
		private static readonly HashSet<StringView> s_skipDeclarationWarningStrings = new()
		{
			"GENERATED_BODY",
			"GENERATED_IINTERFACE_BODY",
			"GENERATED_UCLASS_BODY",
			"GENERATED_UINTERFACE_BODY",
			"GENERATED_USTRUCT_BODY",
			// Leaving these disabled ATM since they can exist in the code without causing compile issues
			//"RIGVM_METHOD",
			//"UCLASS",
			//"UDELEGATE",
			//"UENUM",
			//"UFUNCTION",
			//"UINTERFACE",
			//"UPROPERTY",
			//"USTRUCT",
		};

		/// <summary>
		/// When processing type, make sure that the next token is the expected token
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="expectedIdentifier">Expected identifier</param>
		/// <param name="isMember">If true, log an error if the type begins with const</param>
		/// <returns>true if there could be more header to process, false if the end was reached.</returns>
		public static bool SkipExpectedType(this IUhtTokenReader tokenReader, StringView expectedIdentifier, bool isMember)
		{
			if (tokenReader.TryOptional(expectedIdentifier))
			{
				return true;
			}
			if (isMember && tokenReader.TryOptional("const"))
			{
				tokenReader.LogError("Const properties are not supported.");
			}
			else
			{
				tokenReader.LogError($"Inappropriate keyword '{tokenReader.PeekToken().Value}' on variable of type '{expectedIdentifier}'");
			}
			return false;
		}

		/// <summary>
		/// Try to parse an optional _API macro
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="apiMacroToken">_API macro parsed</param>
		/// <returns>True if an _API macro was parsed</returns>
		public static bool TryOptionalAPIMacro(this IUhtTokenReader tokenReader, out UhtToken apiMacroToken)
		{
			ref UhtToken token = ref tokenReader.PeekToken();
			if (token.IsIdentifier() && token.Value.Span.EndsWith("_API"))
			{
				apiMacroToken = token;
				tokenReader.ConsumeToken();
				return true;
			}
			apiMacroToken = new UhtToken();
			return false;
		}

		/// <summary>
		/// Parse an optional single inheritance
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="superClassDelegate">Invoked with the inherited type name</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader OptionalInheritance(this IUhtTokenReader tokenReader, UhtTokenDelegate superClassDelegate)
		{
			tokenReader.Optional(':', () =>
			{
				tokenReader
					.Require("public", "public access modifier")
					.RequireIdentifier(superClassDelegate);
			});
			return tokenReader;
		}

		/// <summary>
		/// Parse an optional inheritance
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="superClassDelegate">Invoked with the inherited type name</param>
		/// <param name="baseClassDelegate">Invoked when other base classes are parsed</param>
		/// <returns>Token reader</returns>
		public static IUhtTokenReader OptionalInheritance(this IUhtTokenReader tokenReader, UhtTokenDelegate superClassDelegate, UhtTokenListDelegate baseClassDelegate)
		{
			tokenReader.Optional(':', () =>
			{
				tokenReader
					.Require("public", "public access modifier")
					.RequireIdentifier(superClassDelegate)
					.While(',', () =>
					{
						tokenReader
							.Require("public", "public interface access specifier")
							.RequireCppIdentifier(UhtCppIdentifierOptions.AllowTemplates, baseClassDelegate);
					});
			});
			return tokenReader;
		}

		/// <summary>
		/// Given a declaration/statement that starts with the given token, skip the declaration in the header.
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="token">Token that started the process</param>
		/// <returns>true if there could be more header to process, false if the end was reached.</returns>
		public static bool SkipDeclaration(this IUhtTokenReader tokenReader, ref UhtToken token)
		{
			// Consume all tokens until the end of declaration/definition has been found.
			int nestedScopes = 0;
			bool endOfDeclarationFound = false;

			// Store the current value of PrevComment so it can be restored after we parsed everything.
			{
				using UhtTokenDisableComments disableComments = new(tokenReader);

				// Check if this is a class/struct declaration in which case it can be followed by member variable declaration.	
				bool possiblyClassDeclaration = token.IsIdentifier() && (token.IsValue("class") || token.IsValue("struct"));

				// (known) macros can end without ; or } so use () to find the end of the declaration.
				// However, we don't want to use it with DECLARE_FUNCTION, because we need it to be treated like a function.
				bool macroDeclaration = IsProbablyAMacro(token.Value) && !token.IsIdentifier("DECLARE_FUNCTION");

				bool definitionFound = false;
				char openingBracket = macroDeclaration ? '(' : '{';
				char closingBracket = macroDeclaration ? ')' : '}';

				bool retestCurrentToken = false;
				while (true)
				{
					if (!retestCurrentToken)
					{
						token = tokenReader.GetToken();
						if (token.TokenType.IsEndType())
						{
							break;
						}
					}
					else
					{
						retestCurrentToken = false;
					}
					ReadOnlySpan<char> span = token.Value.Span;

					// If this is a macro, consume it
					// If we find parentheses at top-level and we think it's a class declaration then it's more likely
					// to be something like: class UThing* GetThing();
					if (possiblyClassDeclaration && nestedScopes == 0 && token.IsSymbol() && span.Length == 1 && span[0] == '(')
					{
						possiblyClassDeclaration = false;
					}

					if (token.IsSymbol() && span.Length == 1 && span[0] == ';' && nestedScopes == 0)
					{
						endOfDeclarationFound = true;
						break;
					}

					if (token.IsIdentifier())
					{
						// Use a trivial pre-filter to avoid doing the search on things that aren't UE keywords we care about
						if (span[0] == 'G' || span[0] == 'R' || span[0] == 'U')
						{
							if (s_skipDeclarationWarningStrings.Contains(token.Value))
							{
								tokenReader.LogWarning($"The identifier \'{token.Value}\' was detected in a block being skipped. Was this intentional?");
							}
						}
					}

					if (!macroDeclaration && token.IsIdentifier() && span.Equals("PURE_VIRTUAL", StringComparison.Ordinal) && nestedScopes == 0)
					{
						openingBracket = '(';
						closingBracket = ')';
					}

					if (token.IsSymbol() && span.Length == 1 && span[0] == openingBracket)
					{
						// This is a function definition or class declaration.
						definitionFound = true;
						nestedScopes++;
					}
					else if (token.IsSymbol() && span.Length == 1 && span[0] == closingBracket)
					{
						nestedScopes--;
						if (nestedScopes == 0)
						{
							// Could be a class declaration in all capitals, and not a macro
							bool reallyEndDeclaration = true;
							if (macroDeclaration)
							{
								reallyEndDeclaration = !tokenReader.TryPeekOptional('{');
							}

							if (reallyEndDeclaration)
							{
								endOfDeclarationFound = true;
								break;
							}
						}

						if (nestedScopes < 0)
						{
							throw new UhtException(tokenReader, token.InputLine, $"Unexpected '{closingBracket}'. Did you miss a semi-colon?");
						}
					}
					else if (macroDeclaration && nestedScopes == 0)
					{
						macroDeclaration = false;
						openingBracket = '{';
						closingBracket = '}';
						retestCurrentToken = true;
					}
				}
				if (endOfDeclarationFound)
				{
					// Member variable declaration after class declaration (see bPossiblyClassDeclaration).
					if (possiblyClassDeclaration && definitionFound)
					{
						// Should syntax errors be also handled when someone declares a variable after function definition?
						// Consume the variable name.
						if (tokenReader.IsEOF)
						{
							return true;
						}
						if (tokenReader.TryOptionalIdentifier())
						{
							tokenReader.Require(';');
						}
					}

					// C++ allows any number of ';' after member declaration/definition.
					while (tokenReader.TryOptional(';'))
					{
					}
				}
			}

			// Successfully consumed C++ declaration unless mismatched pair of brackets has been found.
			return nestedScopes == 0 && endOfDeclarationFound;
		}

		private static bool IsProbablyAMacro(StringView identifier)
		{
			ReadOnlySpan<char> span = identifier.Span;
			if (span.Length == 0)
			{
				return false;
			}

			// Macros must start with a capitalized alphanumeric character or underscore
			char firstChar = span[0];
			if (firstChar != '_' && (firstChar < 'A' || firstChar > 'Z'))
			{
				return false;
			}

			// Test for known delegate and event macros.
			if (span.StartsWith("DECLARE_MULTICAST_DELEGATE") ||
				span.StartsWith("DECLARE_DELEGATE") ||
				span.StartsWith("DECLARE_EVENT"))
			{
				return true;
			}

			// Failing that, we'll guess about it being a macro based on it being a fully-capitalized identifier.
			foreach (char ch in span[1..])
			{
				if (ch != '_' && (ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9'))
				{
					return false;
				}
			}

			return true;
		}
	}
}
