// Copyright Epic Games, Inc. All Rights Reserved.

using System.Collections.Generic;
using System.Diagnostics.CodeAnalysis;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Parsers
{
	/// <summary>
	/// UCLASS parser
	/// </summary>
	[UnrealHeaderTool]
	public static class UhtClassParser
	{

		#region Keywords
		[UhtKeyword(Extends = UhtTableNames.Global)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtParseResult UCLASSKeyword(UhtParsingScope topScope, UhtParsingScope actionScope, ref UhtToken token)
		{
			return ParseUClass(topScope, ref token);
		}

		[UhtKeyword(Extends = UhtTableNames.Class)]
		[UhtKeyword(Extends = UhtTableNames.Class, Keyword = "GENERATED_BODY")]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtParseResult GENERATED_UCLASS_BODYKeyword(UhtParsingScope topScope, UhtParsingScope actionScope, ref UhtToken token)
		{
			UhtClass classObj = (UhtClass)topScope.ScopeType;

			if (token.IsValue("GENERATED_BODY"))
			{
				classObj.HasGeneratedBody = true;
				classObj.GeneratedBodyAccessSpecifier = topScope.AccessSpecifier;
			}
			else
			{
				classObj.ClassExportFlags |= UhtClassExportFlags.UsesGeneratedBodyLegacy;
				topScope.AccessSpecifier = UhtAccessSpecifier.Public;
			}

			UhtParserHelpers.ParseCompileVersionDeclaration(topScope.TokenReader, topScope.Session.Config!, classObj);

			classObj.GeneratedBodyLineNumber = topScope.TokenReader.InputLine;

			// C++ UHT TODO - In the C++ version, we don't skip any trailing ';'.  ParseStatement will generate an error
			//topScope.TokenReader.Optional(';');
			return UhtParseResult.Handled;
		}
		#endregion

		private static UhtParseResult ParseUClass(UhtParsingScope parentScope, ref UhtToken token)
		{
			UhtClass classObj = new(parentScope.ScopeType, token.InputLine);
			{
				using UhtParsingScope topScope = new(parentScope, classObj, parentScope.Session.GetKeywordTable(UhtTableNames.Class), UhtAccessSpecifier.Private);
				const string ScopeName = "class";

				{
					using UhtMessageContext tokenContext = new(ScopeName);
					// Parse the specifiers
					UhtSpecifierContext specifierContext = new(topScope, topScope.TokenReader, classObj.MetaData);
					UhtSpecifierParser specifiers = UhtSpecifierParser.GetThreadInstance(specifierContext, ScopeName, parentScope.Session.GetSpecifierTable(UhtTableNames.Class));
					specifiers.ParseSpecifiers();
					classObj.PrologLineNumber = topScope.TokenReader.InputLine;
					classObj.ClassFlags |= EClassFlags.Native;

					topScope.AddFormattedCommentsAsTooltipMetaData();

					// Consume the class specifier
					topScope.TokenReader.Require("class");

					topScope.TokenReader.OptionalAttributes(true);

					// Read the class name and possible API macro name
					topScope.TokenReader.TryOptionalAPIMacro(out UhtToken apiMacroToken);
					classObj.SourceName = topScope.TokenReader.GetIdentifier().Value.ToString();

					// Update the context for better error messages
					tokenContext.Reset($"class '{classObj.SourceName}'");

					// Split the source name into the different parts
					UhtEngineNameParts nameParts = UhtUtilities.GetEngineNameParts(classObj.SourceName);
					classObj.EngineName = nameParts.EngineName.ToString();

					// Check for an empty engine name
					if (classObj.EngineName.Length == 0)
					{
						topScope.TokenReader.LogError($"When compiling class definition for '{classObj.SourceName}', attempting to strip prefix results in an empty name. Did you leave off a prefix?");
					}

					// Skip optional final keyword
					topScope.TokenReader.Optional("final");

					// Parse the inheritance
					UhtParserHelpers.ParseInheritance(parentScope.HeaderParser, topScope.Session.Config!, out UhtToken superIdentifier, out List<UhtToken[]>? baseIdentifiers);
					classObj.SuperIdentifier = superIdentifier;
					classObj.BaseIdentifiers = baseIdentifiers;

					if (apiMacroToken)
					{
						classObj.ClassFlags |= EClassFlags.RequiredAPI;
					}

					specifiers.ParseDeferred();

					classObj.Outer?.AddChild(classObj);

					topScope.AddModuleRelativePathToMetaData();

					topScope.HeaderParser.ParseStatements('{', '}', true);

					topScope.TokenReader.Require(';');

					if (classObj.GeneratedBodyLineNumber == -1)
					{
						topScope.TokenReader.LogError("Expected a GENERATED_BODY() at the start of the class");
					}
				}
			}

			return UhtParseResult.Handled;
		}
	}
}
