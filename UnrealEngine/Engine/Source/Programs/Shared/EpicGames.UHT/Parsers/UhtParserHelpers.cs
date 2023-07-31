// Copyright Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using EpicGames.Core;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Parsers
{
	/// <summary>
	/// Collection of helper methods
	/// </summary>
	public static class UhtParserHelpers
	{

		/// <summary>
		/// Parse the inheritance 
		/// </summary>
		/// <param name="headerFileParser">Header file being parsed</param>
		/// <param name="config">Configuration</param>
		/// <param name="superIdentifier">Output super identifier</param>
		/// <param name="baseIdentifiers">Output base identifiers</param>
		public static void ParseInheritance(UhtHeaderFileParser headerFileParser, IUhtConfig config, out UhtToken superIdentifier, out List<UhtToken[]>? baseIdentifiers)
		{

			// TODO: C++ UHT doesn't allow preprocessor statements inside of inheritance lists
			string? restrictedPreprocessorContext = headerFileParser.RestrictedPreprocessorContext;
			headerFileParser.RestrictedPreprocessorContext = "parsing inheritance list";

			try
			{
				UhtToken superIdentifierTemp = new();
				List<UhtToken[]>? baseIdentifiersTemp = null;
				headerFileParser.TokenReader.OptionalInheritance(
					(ref UhtToken identifier) =>
					{
						config.RedirectTypeIdentifier(ref identifier);
						superIdentifierTemp = identifier;
					},
					(UhtTokenList identifier) =>
					{
						if (baseIdentifiersTemp == null)
						{
							baseIdentifiersTemp = new List<UhtToken[]>();
						}
						baseIdentifiersTemp.Add(identifier.ToArray());
					});
				superIdentifier = superIdentifierTemp;
				baseIdentifiers = baseIdentifiersTemp;
			}
			finally
			{
				headerFileParser.RestrictedPreprocessorContext = restrictedPreprocessorContext;
			}
		}

		/// <summary>
		/// Parse compiler version declaration
		/// </summary>
		/// <param name="tokenReader">Token reader</param>
		/// <param name="config">Configuration</param>
		/// <param name="structObj">Struct being parsed</param>
		public static void ParseCompileVersionDeclaration(IUhtTokenReader tokenReader, IUhtConfig config, UhtStruct structObj)
		{

			// Fetch the default generation code version. If supplied, then package code version overrides the default.
			EGeneratedCodeVersion version = structObj.Package.Module.GeneratedCodeVersion;
			if (version == EGeneratedCodeVersion.None)
			{
				version = config.DefaultGeneratedCodeVersion;
			}

			// Fetch the code version from header file
			tokenReader
				.Require('(')
				.OptionalIdentifier((ref UhtToken identifier) =>
				{
					if (!Enum.TryParse(identifier.Value.ToString(), true, out version))
					{
						version = EGeneratedCodeVersion.None;
					}
				})
				.Require(')');

			// Save the results
			structObj.GeneratedCodeVersion = version;
		}
	}
}
