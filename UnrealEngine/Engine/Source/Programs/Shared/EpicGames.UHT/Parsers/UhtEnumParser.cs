// Copyright Epic Games, Inc. All Rights Reserved.

using System.Diagnostics.CodeAnalysis;
using EpicGames.Core;
using EpicGames.UHT.Tables;
using EpicGames.UHT.Tokenizer;
using EpicGames.UHT.Types;
using EpicGames.UHT.Utils;

namespace EpicGames.UHT.Parsers
{
	/// <summary>
	/// UENUM parser
	/// </summary>
	[UnrealHeaderTool]
	public static class UhtEnumParser
	{
		#region Keywords
		[UhtKeyword(Extends = UhtTableNames.Global)]
		[SuppressMessage("CodeQuality", "IDE0051:Remove unused private members", Justification = "Attribute accessed method")]
		[SuppressMessage("Style", "IDE0060:Remove unused parameter", Justification = "Attribute accessed method")]
		private static UhtParseResult UENUMKeyword(UhtParsingScope topScope, UhtParsingScope actionScope, ref UhtToken token)
		{
			return ParseUEnum(topScope, token);
		}
		#endregion

		private static UhtParseResult ParseUEnum(UhtParsingScope parentScope, UhtToken keywordToken)
		{
			UhtEnum enumObject = new(parentScope.ScopeType, keywordToken.InputLine);
			{
				using UhtParsingScope topScope = new(parentScope, enumObject, parentScope.Session.GetKeywordTable(UhtTableNames.Enum), UhtAccessSpecifier.Public);
				const string ScopeName = "UENUM";

				{
					using UhtMessageContext tokenContext = new(ScopeName);

					// Parse the specifiers
					UhtSpecifierContext specifierContext = new(topScope, topScope.TokenReader, enumObject.MetaData);
					UhtSpecifierParser specifiers = UhtSpecifierParser.GetThreadInstance(specifierContext, ScopeName, parentScope.Session.GetSpecifierTable(UhtTableNames.Enum));
					specifiers.ParseSpecifiers();

					// Read the name and the CPP type
					switch (topScope.TokenReader.TryOptional(new string[] { "namespace", "enum" }))
					{
						case 0: // namespace
							enumObject.CppForm = UhtEnumCppForm.Namespaced;
							topScope.TokenReader.SkipDeprecatedMacroIfNecessary();
							break;
						case 1: // enum
							enumObject.CppForm = topScope.TokenReader.TryOptional(new string[] { "class", "struct" }) >= 0 ? UhtEnumCppForm.EnumClass : UhtEnumCppForm.Regular;
							topScope.TokenReader.SkipAlignasAndDeprecatedMacroIfNecessary();
							break;
						default:
							throw new UhtTokenException(topScope.TokenReader, topScope.TokenReader.PeekToken(), null);
					}

					UhtToken enumToken = topScope.TokenReader.GetIdentifier("enumeration name");

					enumObject.SourceName = enumToken.Value.ToString();

					specifiers.ParseFieldMetaData();
					specifiers.ParseDeferred();

					if (enumObject.Outer != null)
					{
						enumObject.Outer.AddChild(enumObject);
					}

					if ((topScope.HeaderParser.GetCurrentCompositeCompilerDirective() & UhtCompilerDirective.WithEditorOnlyData) != 0)
					{
						enumObject.IsEditorOnly = true;
					}

					// Read base for enum class
					if (enumObject.CppForm == UhtEnumCppForm.EnumClass)
					{
						if (topScope.TokenReader.TryOptional(':'))
						{
							UhtToken enumType = topScope.TokenReader.GetIdentifier("enumeration base");

							if (!System.Enum.TryParse<UhtEnumUnderlyingType>(enumType.Value.ToString(), out UhtEnumUnderlyingType underlyingType) || underlyingType == UhtEnumUnderlyingType.Unspecified)
							{
								topScope.TokenReader.LogError(enumType.InputLine, $"Unsupported enum class base type '{enumType.Value}'");
							}
							enumObject.UnderlyingType = underlyingType;
						}
						else
						{
							enumObject.UnderlyingType = UhtEnumUnderlyingType.Unspecified;
						}
					}
					else
					{
						if (enumObject.CppForm == UhtEnumCppForm.Regular)
						{
							if (topScope.TokenReader.TryOptional(":"))
							{
								UhtToken enumType = topScope.TokenReader.GetIdentifier("enumeration base");

								if (enumType.Value != "int")
								{
									topScope.TokenReader.LogError($"Regular enums only support 'int' as the value size");
								}
							}
						}
						if ((enumObject.EnumFlags & EEnumFlags.Flags) != 0)
						{
							topScope.TokenReader.LogError("The 'Flags' specifier can only be used on enum classes");
						}
					}

					if (enumObject.UnderlyingType != UhtEnumUnderlyingType.uint8 && enumObject.MetaData.ContainsKey("BlueprintType"))
					{
						topScope.TokenReader.LogError("Invalid BlueprintType enum base - currently only uint8 supported");
					}

					//EnumDef.GetDefinitionRange().Start = &Input[InputPos];

					// Get the opening brace
					topScope.TokenReader.Require('{');

					switch (enumObject.CppForm)
					{
						case UhtEnumCppForm.Namespaced:
							// Now handle the inner true enum portion
							topScope.TokenReader.Require("enum");
							topScope.TokenReader.SkipAlignasAndDeprecatedMacroIfNecessary();

							UhtToken innerEnumToken = topScope.TokenReader.GetIdentifier("enumeration type name");

							if (topScope.TokenReader.TryOptional(":"))
							{
								UhtToken enumType = topScope.TokenReader.GetIdentifier("enumeration base");

								if (enumType.Value != "int")
								{
									topScope.TokenReader.LogError($"Namespace enums only support 'int' as the value size");
								}
							}

							topScope.TokenReader.Require('{');
							enumObject.CppType = $"{enumObject.SourceName}::{innerEnumToken.Value}";
							break;

						case UhtEnumCppForm.EnumClass:
						case UhtEnumCppForm.Regular:
							enumObject.CppType = enumObject.SourceName;
							break;
					}

					tokenContext.Reset($"UENUM {enumObject.SourceName}");

					topScope.AddModuleRelativePathToMetaData();
					topScope.AddFormattedCommentsAsTooltipMetaData();

					// Parse all enum tags
					bool hasUnparsedValue = false;
					long currentEnumValue = 0;

					topScope.TokenReader.RequireList('}', ',', true, () =>
					{
						UhtToken tagToken = topScope.TokenReader.GetIdentifier();
						if (tagToken.IsValue("true", true) || tagToken.IsValue("false", true))
						{
							// C++ UHT compatibility - TODO
							topScope.TokenReader.LogError("Enumerations can't have any elements named 'true' or 'false' regardless of case");
						}

						StringView fullEnumName;
						switch (enumObject.CppForm)
						{
							case UhtEnumCppForm.Namespaced:
							case UhtEnumCppForm.EnumClass:
								fullEnumName = new StringView($"{enumObject.SourceName}::{tagToken.Value}");
								break;

							case UhtEnumCppForm.Regular:
								fullEnumName = tagToken.Value;
								break;

							default:
								throw new UhtIceException("Unexpected EEnumCppForm value");
						}

						// Save the new tag with a default value.  This will be replaced later
						int enumIndex = enumObject.AddEnumValue(tagToken.Value.ToString(), 0);
						topScope.AddFormattedCommentsAsTooltipMetaData(enumIndex);

						// Skip any deprecation
						topScope.TokenReader.SkipDeprecatedMacroIfNecessary();

						// Try to read an optional explicit enum value specification
						if (topScope.TokenReader.TryOptional('='))
						{
							bool parsedValue = topScope.TokenReader.TryOptionalConstLong(out long scatchValue);
							if (parsedValue)
							{
								currentEnumValue = scatchValue;
							}
							else
							{
								// We didn't parse a literal, so set an invalid value
								currentEnumValue = -1;
								hasUnparsedValue = true;
							}

							// Skip tokens until we encounter a comma, a closing brace or a UMETA declaration
							// There are tokens after the initializer so it's not a standalone literal,
							// so set it to an invalid value.
							int skippedCount = topScope.TokenReader.ConsumeUntil(new string[] { ",", "}", "UMETA" });
							if (skippedCount == 0 && !parsedValue)
							{
								throw new UhtTokenException(topScope.TokenReader, topScope.TokenReader.PeekToken(), "enumerator initializer");
							}
							if (skippedCount > 0)
							{
								currentEnumValue = -1;
								hasUnparsedValue = true;
							}
						}

						// Save the value and auto increment
						UhtEnumValue value = enumObject.EnumValues[enumIndex];
						value.Value = currentEnumValue;
						enumObject.EnumValues[enumIndex] = value;
						if (currentEnumValue != -1)
						{
							++currentEnumValue;
						}

						enumObject.MetaData.Add(UhtNames.Name, enumIndex, enumObject.EnumValues[enumIndex].Name.ToString());

						// check for metadata on this enum value
						specifierContext.MetaNameIndex = enumIndex;
						specifiers.ParseFieldMetaData();
					});

					// Trailing brace and semicolon for the enum
					topScope.TokenReader.Require(';');
					if (enumObject.CppForm == UhtEnumCppForm.Namespaced)
					{
						topScope.TokenReader.Require('}');
					}

					if (!hasUnparsedValue && !enumObject.IsValidEnumValue(0) && enumObject.MetaData.ContainsKey(UhtNames.BlueprintType))
					{
						enumObject.LogWarning($"'{enumObject.SourceName}' does not have a 0 entry! (This is a problem when the enum is initialized by default)");
					}
				}

				return UhtParseResult.Handled;
			}
		}
	}
}
