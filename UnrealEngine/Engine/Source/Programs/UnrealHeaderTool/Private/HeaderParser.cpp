// Copyright Epic Games, Inc. All Rights Reserved.


#include "HeaderParser.h"
#include "UnrealHeaderTool.h"
#include "PropertyTypes.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "UObject/Interface.h"
#include "GeneratedCodeVersion.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "NativeClassExporter.h"
#include "EngineAPI.h"
#include "ClassMaps.h"
#include "StringUtils.h"
#include "Misc/DefaultValueHelper.h"
#include "Manifest.h"
#include "Math/UnitConversion.h"
#include "Exceptions.h"
#include "UnrealTypeDefinitionInfo.h"
#include "Containers/EnumAsByte.h"
#include "Algo/AllOf.h"
#include "Algo/Find.h"
#include "Algo/FindSortedStringCaseInsensitive.h"
#include "Misc/ScopeExit.h"
#include "Misc/Timespan.h"
#include "HAL/PlatformTime.h"
#include <atomic>

#include "Specifiers/CheckedMetadataSpecifiers.h"
#include "Specifiers/EnumSpecifiers.h"
#include "Specifiers/FunctionSpecifiers.h"
#include "Specifiers/InterfaceSpecifiers.h"
#include "Specifiers/StructSpecifiers.h"
#include "Specifiers/VariableSpecifiers.h"

// Globals for common class definitions
extern FUnrealClassDefinitionInfo* GUObjectDef;
extern FUnrealClassDefinitionInfo* GUClassDef;
extern FUnrealClassDefinitionInfo* GUInterfaceDef;

/*-----------------------------------------------------------------------------
	Constants & declarations.
-----------------------------------------------------------------------------*/

enum {MAX_ARRAY_SIZE=2048};

namespace
{
	static const FName NAME_Comment(TEXT("Comment"));
	static const FName NAME_ToolTip(TEXT("ToolTip"));
	static const FName NAME_DocumentationPolicy(TEXT("DocumentationPolicy"));
	static const FName NAME_AllowPrivateAccess(TEXT("AllowPrivateAccess"));
	static const FName NAME_ExposeOnSpawn(TEXT("ExposeOnSpawn"));
	static const FName NAME_NativeConst(TEXT("NativeConst"));
	static const FName NAME_NativeConstTemplateArg(TEXT("NativeConstTemplateArg"));
	static const FName NAME_BlueprintInternalUseOnly(TEXT("BlueprintInternalUseOnly"));
	static const FName NAME_DeprecatedFunction(TEXT("DeprecatedFunction"));
	static const FName NAME_BlueprintSetter(TEXT("BlueprintSetter"));
	static const FName NAME_BlueprintGetter(TEXT("BlueprintGetter"));
	static const FName NAME_Category(TEXT("Category"));
	static const FName NAME_ReturnValue(TEXT("ReturnValue"));
	static const FName NAME_CppFromBpEvent(TEXT("CppFromBpEvent"));
	static const FName NAME_CustomThunk(TEXT("CustomThunk"));
	static const FName NAME_EditInline(TEXT("EditInline"));
	static const FName NAME_IncludePath(TEXT("IncludePath"));
	static const FName NAME_ModuleRelativePath(TEXT("ModuleRelativePath"));
	static const FName NAME_IsBlueprintBase(TEXT("IsBlueprintBase"));
	static const FName NAME_CannotImplementInterfaceInBlueprint(TEXT("CannotImplementInterfaceInBlueprint"));
	static const FName NAME_UIMin(TEXT("UIMin"));
	static const FName NAME_UIMax(TEXT("UIMax"));
}

const FName FHeaderParserNames::NAME_HideCategories(TEXT("HideCategories"));
const FName FHeaderParserNames::NAME_ShowCategories(TEXT("ShowCategories"));
const FName FHeaderParserNames::NAME_SparseClassDataTypes(TEXT("SparseClassDataTypes"));
const FName FHeaderParserNames::NAME_IsConversionRoot(TEXT("IsConversionRoot"));
const FName FHeaderParserNames::NAME_BlueprintType(TEXT("BlueprintType"));
const FName FHeaderParserNames::NAME_AutoCollapseCategories(TEXT("AutoCollapseCategories"));
const FName FHeaderParserNames::NAME_HideFunctions(TEXT("HideFunctions"));
const FName FHeaderParserNames::NAME_AutoExpandCategories(TEXT("AutoExpandCategories"));
const FName FHeaderParserNames::NAME_PrioritizeCategories(TEXT("PrioritizeCategories"));

TArray<FString> FHeaderParser::PropertyCPPTypesRequiringUIRanges = { TEXT("float"), TEXT("double") };
TArray<FString> FHeaderParser::ReservedTypeNames = { TEXT("none") };

// Busy wait support for holes in the include graph issues
extern bool bGoWide;
extern std::atomic<bool> GSourcesConcurrent;
extern std::atomic<int> GSourcesToParse;
extern std::atomic<int> GSourcesParsing;
extern std::atomic<int> GSourcesCompleted;
extern std::atomic<int> GSourcesStalled;

/*-----------------------------------------------------------------------------
	Utility functions.
-----------------------------------------------------------------------------*/

namespace
{
	bool ProbablyAMacro(const FStringView& Identifier)
	{
		if (Identifier.Len() == 0)
		{
			return false;

		}
		// Macros must start with a capitalized alphanumeric character or underscore
		TCHAR FirstChar = Identifier[0];
		if (FirstChar != TEXT('_') && (FirstChar < TEXT('A') || FirstChar > TEXT('Z')))
		{
			return false;
		}

		// Test for known delegate and event macros.
		if (Identifier.StartsWith(TEXT("DECLARE_MULTICAST_DELEGATE")) ||
			Identifier.StartsWith(TEXT("DECLARE_DELEGATE")) ||
			Identifier.StartsWith(TEXT("DECLARE_EVENT")))
		{
			return true;
		}

		// Failing that, we'll guess about it being a macro based on it being a fully-capitalized identifier.
		for (TCHAR Ch : Identifier.RightChop(1))
		{
			if (Ch != TEXT('_') && (Ch < TEXT('A') || Ch > TEXT('Z')) && (Ch < TEXT('0') || Ch > TEXT('9')))
			{
				return false;
			}
		}

		return true;
	}

	/**
	 * Tests if an identifier looks like a macro which doesn't have a following open parenthesis.
	 *
	 * @param HeaderParser  The parser to retrieve the next token.
	 * @param Token         The token to test for being callable-macro-like.
	 *
	 * @return true if it looks like a non-callable macro, false otherwise.
	 */
	bool ProbablyAnUnknownObjectLikeMacro(FHeaderParser& HeaderParser, FToken Token)
	{
		// Non-identifiers are not macros
		if (!Token.IsIdentifier())
		{
			return false;
		}

		// Macros must start with a capitalized alphanumeric character or underscore
		TCHAR FirstChar = Token.Value[0];
		if (FirstChar != TEXT('_') && (FirstChar < TEXT('A') || FirstChar > TEXT('Z')))
		{
			return false;
		}

		// We'll guess about it being a macro based on it being fully-capitalized with at least one underscore.
		int32 UnderscoreCount = 0;
		for (TCHAR Ch : Token.Value.RightChop(1))
		{
			if (Ch == TEXT('_'))
			{
				++UnderscoreCount;
			}
			else if ((Ch < TEXT('A') || Ch > TEXT('Z')) && (Ch < TEXT('0') || Ch > TEXT('9')))
			{
				return false;
			}
		}

		// We look for at least one underscore as a convenient way of allowing many known macros
		// like FORCEINLINE and CONSTEXPR, and non-macros like FPOV and TCHAR.
		if (UnderscoreCount == 0)
		{
			return false;
		}

		// Identifiers which end in _API are known
		if (Token.Value.Len() > 4 && Token.Value.EndsWith(FStringView(TEXT("_API"), 4)))
		{
			return false;
		}

		// Ignore certain known macros or identifiers that look like macros.
		if (Token.IsValue(TEXT("FORCEINLINE_DEBUGGABLE"), ESearchCase::CaseSensitive) ||
			Token.IsValue(TEXT("FORCEINLINE_STATS"), ESearchCase::CaseSensitive) ||
			Token.IsValue(TEXT("SIZE_T"), ESearchCase::CaseSensitive))
		{
			return false;
		}

		// Check if there's an open parenthesis following the token.
		//
		// Rather than ungetting the bracket token, we unget the original identifier token,
		// then get it again, so we don't lose any comments which may exist between the token 
		// and the non-bracket.
		FToken PossibleBracketToken;
		HeaderParser.GetToken(PossibleBracketToken);
		HeaderParser.UngetToken(Token);
		HeaderParser.GetToken(Token);

		return !PossibleBracketToken.IsSymbol(TEXT('('));
	}

	/**
	 * Parse and validate an array of identifiers (inside FUNC_NetRequest, FUNC_NetResponse) 
	 * @param FuncInfo function info for the current function
	 * @param Identifiers identifiers inside the net service declaration
	 */
	void ParseNetServiceIdentifiers(FHeaderParser& HeaderParser, FFuncInfo& FuncInfo, const TArray<FString>& Identifiers)
	{
		static const auto& IdTag          = TEXT("Id");
		static const auto& ResponseIdTag  = TEXT("ResponseId");
		static const auto& JSBridgePriTag = TEXT("Priority");

		for (const FString& Identifier : Identifiers)
		{
			const TCHAR* IdentifierPtr = *Identifier;

			if (const TCHAR* Equals = FCString::Strchr(IdentifierPtr, TEXT('=')))
			{
				// It's a tag with an argument

				if (FCString::Strnicmp(IdentifierPtr, IdTag, UE_ARRAY_COUNT(IdTag) - 1) == 0)
				{
					int32 TempInt = FCString::Atoi(Equals + 1);
					if (TempInt <= 0 || TempInt > MAX_uint16)
					{
						HeaderParser.Throwf(TEXT("Invalid network identifier %s for function"), IdentifierPtr);
					}
					FuncInfo.RPCId = (uint16)TempInt;
				}
				else if (FCString::Strnicmp(IdentifierPtr, ResponseIdTag, UE_ARRAY_COUNT(ResponseIdTag) - 1) == 0 ||
					FCString::Strnicmp(IdentifierPtr, JSBridgePriTag, UE_ARRAY_COUNT(JSBridgePriTag) - 1) == 0)
				{
					int32 TempInt = FCString::Atoi(Equals + 1);
					if (TempInt <= 0 || TempInt > MAX_uint16)
					{
						HeaderParser.Throwf(TEXT("Invalid network identifier %s for function"), IdentifierPtr);
					}
					FuncInfo.RPCResponseId = (uint16)TempInt;
				}
			}
			else
			{
				// Assume it's an endpoint name

				if (FuncInfo.EndpointName.Len())
				{
					HeaderParser.Throwf(TEXT("Function should not specify multiple endpoints - '%s' found but already using '%s'"), *Identifier);
				}

				FuncInfo.EndpointName = Identifier;
			}
		}
	}

	/**
	 * Processes a set of UFUNCTION or UDELEGATE specifiers into an FFuncInfo struct.
	 *
	 * @param FuncInfo   - The FFuncInfo object to populate.
	 * @param Specifiers - The specifiers to process.
	 */
	void ProcessFunctionSpecifiers(FHeaderParser& HeaderParser, FFuncInfo& FuncInfo, const TArray<FPropertySpecifier>& Specifiers, TMap<FName, FString>& MetaData)
	{
		bool bSpecifiedUnreliable = false;
		bool bSawPropertyAccessor = false;

		for (const FPropertySpecifier& Specifier : Specifiers)
		{
			switch ((EFunctionSpecifier)Algo::FindSortedStringCaseInsensitive(*Specifier.Key, GFunctionSpecifierStrings))
			{
				default:
				{
					HeaderParser.Throwf(TEXT("Unknown function specifier '%s'"), *Specifier.Key);
				}
				break;

				case EFunctionSpecifier::BlueprintNativeEvent:
				{
					if (FuncInfo.FunctionFlags & FUNC_Net)
					{
						HeaderParser.LogError(TEXT("BlueprintNativeEvent functions cannot be replicated!") );
					}
					else if ( (FuncInfo.FunctionFlags & FUNC_BlueprintEvent) && !(FuncInfo.FunctionFlags & FUNC_Native) )
					{
						// already a BlueprintImplementableEvent
						HeaderParser.LogError(TEXT("A function cannot be both BlueprintNativeEvent and BlueprintImplementableEvent!") );
					}
					else if (bSawPropertyAccessor)
					{
						HeaderParser.LogError(TEXT("A function cannot be both BlueprintNativeEvent and a Blueprint Property accessor!"));
					}
					else if ( (FuncInfo.FunctionFlags & FUNC_Private) )
					{
						HeaderParser.LogError(TEXT("A Private function cannot be a BlueprintNativeEvent!") );
					}

					FuncInfo.FunctionFlags |= FUNC_Event;
					FuncInfo.FunctionFlags |= FUNC_BlueprintEvent;
				}
				break;

				case EFunctionSpecifier::BlueprintImplementableEvent:
				{
					if (FuncInfo.FunctionFlags & FUNC_Net)
					{
						HeaderParser.LogError(TEXT("BlueprintImplementableEvent functions cannot be replicated!") );
					}
					else if ( (FuncInfo.FunctionFlags & FUNC_BlueprintEvent) && (FuncInfo.FunctionFlags & FUNC_Native) )
					{
						// already a BlueprintNativeEvent
						HeaderParser.LogError(TEXT("A function cannot be both BlueprintNativeEvent and BlueprintImplementableEvent!") );
					}
					else if (bSawPropertyAccessor)
					{
						HeaderParser.LogError(TEXT("A function cannot be both BlueprintImplementableEvent and a Blueprint Property accessor!"));
					}
					else if ( (FuncInfo.FunctionFlags & FUNC_Private) )
					{
						HeaderParser.LogError(TEXT("A Private function cannot be a BlueprintImplementableEvent!") );
					}

					FuncInfo.FunctionFlags |= FUNC_Event;
					FuncInfo.FunctionFlags |= FUNC_BlueprintEvent;
					FuncInfo.FunctionFlags &= ~FUNC_Native;
				}
				break;

				case EFunctionSpecifier::Exec:
				{
					FuncInfo.FunctionFlags |= FUNC_Exec;
					if( FuncInfo.FunctionFlags & FUNC_Net )
					{
						HeaderParser.LogError(TEXT("Exec functions cannot be replicated!") );
					}
				}
				break;

				case EFunctionSpecifier::SealedEvent:
				{
					FuncInfo.bSealedEvent = true;
				}
				break;

				case EFunctionSpecifier::Server:
				{
					if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
					{
						HeaderParser.Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as Client or Server"));
					}

					FuncInfo.FunctionFlags |= FUNC_Net;
					FuncInfo.FunctionFlags |= FUNC_NetServer;

					if (Specifier.Values.Num())
					{
						FuncInfo.CppImplName = Specifier.Values[0];
					}

					if( FuncInfo.FunctionFlags & FUNC_Exec )
					{
						HeaderParser.LogError(TEXT("Exec functions cannot be replicated!") );
					}
				}
				break;

				case EFunctionSpecifier::Client:
				{
					if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
					{
						HeaderParser.Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as Client or Server"));
					}

					FuncInfo.FunctionFlags |= FUNC_Net;
					FuncInfo.FunctionFlags |= FUNC_NetClient;

					if (Specifier.Values.Num())
					{
						FuncInfo.CppImplName = Specifier.Values[0];
					}
				}
				break;

				case EFunctionSpecifier::NetMulticast:
				{
					if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
					{
						HeaderParser.Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as Multicast"));
					}

					FuncInfo.FunctionFlags |= FUNC_Net;
					FuncInfo.FunctionFlags |= FUNC_NetMulticast;
				}
				break;

				case EFunctionSpecifier::ServiceRequest:
				{
					if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
					{
						HeaderParser.Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as a ServiceRequest"));
					}

					FuncInfo.FunctionFlags |= FUNC_Net;
					FuncInfo.FunctionFlags |= FUNC_NetReliable;
					FuncInfo.FunctionFlags |= FUNC_NetRequest;
					FuncInfo.FunctionExportFlags |= FUNCEXPORT_CustomThunk;

					ParseNetServiceIdentifiers(HeaderParser, FuncInfo, Specifier.Values);

					if (FuncInfo.EndpointName.Len() == 0)
					{
						HeaderParser.Throwf(TEXT("ServiceRequest needs to specify an endpoint name"));
					}
				}
				break;

				case EFunctionSpecifier::ServiceResponse:
				{
					if ((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0)
					{
						HeaderParser.Throwf(TEXT("BlueprintImplementableEvent or BlueprintNativeEvent functions cannot be declared as a ServiceResponse"));
					}

					FuncInfo.FunctionFlags |= FUNC_Net;
					FuncInfo.FunctionFlags |= FUNC_NetReliable;
					FuncInfo.FunctionFlags |= FUNC_NetResponse;

					ParseNetServiceIdentifiers(HeaderParser, FuncInfo, Specifier.Values);

					if (FuncInfo.EndpointName.Len() == 0)
					{
						HeaderParser.Throwf(TEXT("ServiceResponse needs to specify an endpoint name"));
					}
				}
				break;

				case EFunctionSpecifier::Reliable:
				{
					FuncInfo.FunctionFlags |= FUNC_NetReliable;
				}
				break;

				case EFunctionSpecifier::Unreliable:
				{
					bSpecifiedUnreliable = true;
				}
				break;

				case EFunctionSpecifier::CustomThunk:
				{
					FuncInfo.FunctionExportFlags |= FUNCEXPORT_CustomThunk;
				}
				break;

				case EFunctionSpecifier::BlueprintCallable:
				{
					FuncInfo.FunctionFlags |= FUNC_BlueprintCallable;
				}
				break;

				case EFunctionSpecifier::BlueprintGetter:
				{
					if (FuncInfo.FunctionFlags & FUNC_Event)
					{
						HeaderParser.LogError(TEXT("Function cannot be a blueprint event and a blueprint getter."));
					}

					bSawPropertyAccessor = true;
					FuncInfo.FunctionFlags |= FUNC_BlueprintCallable;
					FuncInfo.FunctionFlags |= FUNC_BlueprintPure;
					MetaData.Add(NAME_BlueprintGetter);
				}
				break;

				case EFunctionSpecifier::BlueprintSetter:
				{
					if (FuncInfo.FunctionFlags & FUNC_Event)
					{
						HeaderParser.LogError(TEXT("Function cannot be a blueprint event and a blueprint setter."));
					}

					bSawPropertyAccessor = true;
					FuncInfo.FunctionFlags |= FUNC_BlueprintCallable;
					MetaData.Add(NAME_BlueprintSetter);
				}
				break;

				case EFunctionSpecifier::BlueprintPure:
				{
					bool bIsPure = true;
					if (Specifier.Values.Num() == 1)
					{
						FString IsPureStr = Specifier.Values[0];
						bIsPure = IsPureStr.ToBool();
					}

					// This function can be called, and is also pure.
					FuncInfo.FunctionFlags |= FUNC_BlueprintCallable;

					if (bIsPure)
					{
						FuncInfo.FunctionFlags |= FUNC_BlueprintPure;
					}
					else
					{
						FuncInfo.bForceBlueprintImpure = true;
					}
				}
				break;

				case EFunctionSpecifier::BlueprintAuthorityOnly:
				{
					FuncInfo.FunctionFlags |= FUNC_BlueprintAuthorityOnly;
				}
				break;

				case EFunctionSpecifier::BlueprintCosmetic:
				{
					FuncInfo.FunctionFlags |= FUNC_BlueprintCosmetic;
				}
				break;

				case EFunctionSpecifier::WithValidation:
				{
					FuncInfo.FunctionFlags |= FUNC_NetValidate;

					if (Specifier.Values.Num())
					{
						FuncInfo.CppValidationImplName = Specifier.Values[0];
					}
				}
				break;
				case EFunctionSpecifier::FieldNotify:
				{
					FuncInfo.bFieldNotify = true;
				}
				break;
			}
		}

		if (FuncInfo.FunctionFlags & FUNC_Net)
		{
			// Network replicated functions are always events
			FuncInfo.FunctionFlags |= FUNC_Event;

			check(!(FuncInfo.FunctionFlags & (FUNC_BlueprintEvent | FUNC_Exec)));

			bool bIsNetService  = !!(FuncInfo.FunctionFlags & (FUNC_NetRequest | FUNC_NetResponse));
			bool bIsNetReliable = !!(FuncInfo.FunctionFlags & FUNC_NetReliable);

			if (FuncInfo.FunctionFlags & FUNC_Static)
			{
				HeaderParser.LogError(TEXT("Static functions can't be replicated"));
			}

			if (!bIsNetReliable && !bSpecifiedUnreliable && !bIsNetService)
			{
				HeaderParser.LogError(TEXT("Replicated function: 'reliable' or 'unreliable' is required"));
			}

			if (bIsNetReliable && bSpecifiedUnreliable && !bIsNetService)
			{
				HeaderParser.LogError(TEXT("'reliable' and 'unreliable' are mutually exclusive"));
			}
		}
		else if (FuncInfo.FunctionFlags & FUNC_NetReliable)
		{
			HeaderParser.LogError(TEXT("'reliable' specified without 'client' or 'server'"));
		}
		else if (bSpecifiedUnreliable)
		{
			HeaderParser.LogError(TEXT("'unreliable' specified without 'client' or 'server'"));
		}

		if (FuncInfo.bSealedEvent && !(FuncInfo.FunctionFlags & FUNC_Event))
		{
			HeaderParser.LogError(TEXT("SealedEvent may only be used on events"));
		}

		if (FuncInfo.bSealedEvent && FuncInfo.FunctionFlags & FUNC_BlueprintEvent)
		{
			HeaderParser.LogError(TEXT("SealedEvent cannot be used on Blueprint events"));
		}

		if (FuncInfo.bForceBlueprintImpure && (FuncInfo.FunctionFlags & FUNC_BlueprintPure) != 0)
		{
			HeaderParser.LogError(TEXT("BlueprintPure (or BlueprintPure=true) and BlueprintPure=false should not both appear on the same function, they are mutually exclusive"));
		}
	}

	const TCHAR* GetHintText(FHeaderParser& HeaderParser, EVariableCategory VariableCategory)
	{
		switch (VariableCategory)
		{
			case EVariableCategory::ReplicatedParameter:
			case EVariableCategory::RegularParameter:
				return TEXT("Function parameter");

			case EVariableCategory::Return:
				return TEXT("Function return type");

			case EVariableCategory::Member:
				return TEXT("Member variable declaration");

			default:
				HeaderParser.Throwf(TEXT("Unknown variable category"));
		}

		// Unreachable
		check(false);
		return nullptr;
	}

	void SkipAlignasIfNecessary(FBaseParser& Parser)
	{
		if (Parser.MatchIdentifier(TEXT("alignas"), ESearchCase::CaseSensitive))
		{
			Parser.RequireSymbol(TEXT('('), TEXT("'alignas'"));
			Parser.RequireAnyConstInt(TEXT("'alignas'"));
			Parser.RequireSymbol(TEXT(')'), TEXT("'alignas'"));
		}
	}

	void SkipDeprecatedMacroIfNecessary(FBaseParser& Parser)
	{
		FToken MacroToken;
		if (!Parser.GetToken(MacroToken))
		{
			return;
		}

		if (!MacroToken.IsIdentifier() || 
			(!MacroToken.IsValue(TEXT("DEPRECATED"), ESearchCase::CaseSensitive) && !MacroToken.IsValue(TEXT("UE_DEPRECATED"), ESearchCase::CaseSensitive)))
		{
			Parser.UngetToken(MacroToken);
			return;
		}

		auto ErrorMessageGetter = [&MacroToken]() { return FString::Printf(TEXT("%s macro"), *MacroToken.GetTokenValue()); };

		Parser.RequireSymbol(TEXT('('), ErrorMessageGetter);

		FToken Token;
		if (Parser.GetToken(Token) && !Token.IsConstFloat())
		{
			Parser.Throwf(TEXT("Expected engine version in %s macro"), *MacroToken.GetTokenValue());
		}

		Parser.RequireSymbol(TEXT(','), ErrorMessageGetter);
		if (Parser.GetToken(Token) && !Token.IsConstString())
		{
			Parser.Throwf(TEXT("Expected deprecation message in %s macro"), *MacroToken.GetTokenValue());
		}

		Parser.RequireSymbol(TEXT(')'), ErrorMessageGetter);
	}

	void SkipAlignasAndDeprecatedMacroIfNecessary(FBaseParser& Parser)
	{
		// alignas() can come before or after the deprecation macro.
		// We can't have both, but the compiler will catch that anyway.
		SkipAlignasIfNecessary(Parser);
		SkipDeprecatedMacroIfNecessary(Parser);
		SkipAlignasIfNecessary(Parser);
	}

	inline EPointerMemberBehavior ToPointerMemberBehavior(const FString& InString)
	{
		if (InString.Compare(TEXT("Disallow")) == 0)
		{
			return EPointerMemberBehavior::Disallow;
		}

		if (InString.Compare(TEXT("AllowSilently")) == 0)
		{
			return EPointerMemberBehavior::AllowSilently;
		}

		if (InString.Compare(TEXT("AllowAndLog")) == 0)
		{
			return EPointerMemberBehavior::AllowAndLog;
		}

		FUHTMessage(FString(TEXT("Configuration"))).Throwf(TEXT("Unrecognized native pointer member behavior: %s"), *InString);
		return EPointerMemberBehavior::Disallow;
	}

	static const TCHAR* GLayoutMacroNames[] = {
		TEXT("LAYOUT_ARRAY"),
		TEXT("LAYOUT_ARRAY_EDITORONLY"),
		TEXT("LAYOUT_BITFIELD"),
		TEXT("LAYOUT_BITFIELD_EDITORONLY"),
		TEXT("LAYOUT_FIELD"),
		TEXT("LAYOUT_FIELD_EDITORONLY"),
		TEXT("LAYOUT_FIELD_INITIALIZED"),
	};

	FCriticalSection GlobalDelegatesLock;
	TMap<FString, FUnrealFunctionDefinitionInfo*> GlobalDelegates;

	template <typename LambdaType>
	bool BusyWait(LambdaType&& Lambda)
	{
		constexpr double TimeoutInSeconds = 5.0;

		// Try it first
		if (Lambda())
		{
			return true;
		}

		// If we aren't doing concurrent processing, then there isn't any reason to continue, we will never make any progress.
		if (!bGoWide)
		{
			return false;
		}

		// Mark that we are stalled
		ON_SCOPE_EXIT
		{
			--GSourcesStalled;
		};
		++GSourcesStalled;

		int SourcesToParse = GSourcesToParse;
		int SourcesParsing = GSourcesParsing;
		int SourcesCompleted = GSourcesCompleted;
		int SourcesStalled = GSourcesStalled;
		double StartTime = FPlatformTime::Seconds();
		for (;;)
		{
			FPlatformProcess::YieldCycles(10000);
			if (Lambda())
			{
				return true;
			}

			int NewSourcesParsing = GSourcesParsing;
			int NewSourcesCompleted = GSourcesCompleted;
			int NewSourcesStalled = GSourcesStalled;

			// If everything is stalled, then we have no hope
			if (NewSourcesStalled + NewSourcesCompleted == SourcesToParse)
			{
				return false;
			}

			// If something has completed, then reset the timeout
			double CurrentTime = FPlatformTime::Seconds();
			if (NewSourcesCompleted != SourcesCompleted || NewSourcesParsing != SourcesParsing)
			{
				StartTime = CurrentTime;
			}

			// Otherwise, check for long timeout
			else if (CurrentTime - StartTime > TimeoutInSeconds)
			{
				return false;
			}

			// Remember state
			SourcesParsing = NewSourcesParsing;
			SourcesCompleted = NewSourcesCompleted;
			SourcesStalled = NewSourcesStalled;
		}
	}
}

void AddEditInlineMetaData(TMap<FName, FString>& MetaData)
{
	MetaData.Add(NAME_EditInline, TEXT("true"));
}

FUHTConfig::FUHTConfig()
{
	// Read Ini options, GConfig must exist by this point
	check(GConfig);

	const FName TypeRedirectsKey(TEXT("TypeRedirects"));
	const FName StructsWithNoPrefixKey(TEXT("StructsWithNoPrefix"));
	const FName StructsWithTPrefixKey(TEXT("StructsWithTPrefix"));
	const FName DelegateParameterCountStringsKey(TEXT("DelegateParameterCountStrings"));
	const FName GeneratedCodeVersionKey(TEXT("GeneratedCodeVersion"));
	const FName EngineNativePointerMemberBehaviorKey(TEXT("EngineNativePointerMemberBehavior"));
	const FName EngineObjectPtrMemberBehaviorKey(TEXT("EngineObjectPtrMemberBehavior"));
	const FName EnginePluginNativePointerMemberBehaviorKey(TEXT("EnginePluginNativePointerMemberBehavior"));
	const FName EnginePluginObjectPtrMemberBehaviorKey(TEXT("EnginePluginObjectPtrMemberBehavior"));
	const FName NonEngineNativePointerMemberBehaviorKey(TEXT("NonEngineNativePointerMemberBehavior"));
	const FName NonEngineObjectPtrMemberBehaviorKey(TEXT("NonEngineObjectPtrMemberBehavior"));

	FConfigSection* ConfigSection = GConfig->GetSectionPrivate(TEXT("UnrealHeaderTool"), false, true, GEngineIni);
	if (ConfigSection)
	{
		for (FConfigSection::TIterator It(*ConfigSection); It; ++It)
		{
			if (It.Key() == TypeRedirectsKey)
			{
				FString OldType;
				FString NewType;

				FParse::Value(*It.Value().GetValue(), TEXT("OldType="), OldType);
				FParse::Value(*It.Value().GetValue(), TEXT("NewType="), NewType);

				TypeRedirectMap.Add(MoveTemp(OldType), MoveTemp(NewType));
			}
			else if (It.Key() == StructsWithNoPrefixKey)
			{
				StructsWithNoPrefix.Add(It.Value().GetValue());
			}
			else if (It.Key() == StructsWithTPrefixKey)
			{
				StructsWithTPrefix.Add(It.Value().GetValue());
			}
			else if (It.Key() == DelegateParameterCountStringsKey)
			{
				DelegateParameterCountStrings.Add(It.Value().GetValue());
			}
			else if (It.Key() == GeneratedCodeVersionKey)
			{
				DefaultGeneratedCodeVersion = ToGeneratedCodeVersion(It.Value().GetValue());
			}
			else if (It.Key() == EngineNativePointerMemberBehaviorKey)
			{
				EngineNativePointerMemberBehavior = ToPointerMemberBehavior(It.Value().GetValue());
			}
			else if (It.Key() == EngineObjectPtrMemberBehaviorKey)
			{
				EngineObjectPtrMemberBehavior = ToPointerMemberBehavior(It.Value().GetValue());
			}
			else if (It.Key() == EnginePluginNativePointerMemberBehaviorKey)
			{
				EnginePluginNativePointerMemberBehavior = ToPointerMemberBehavior(It.Value().GetValue());
			}
			else if (It.Key() == EnginePluginObjectPtrMemberBehaviorKey)
			{
				EnginePluginObjectPtrMemberBehavior = ToPointerMemberBehavior(It.Value().GetValue());
			}
			else if (It.Key() == NonEngineNativePointerMemberBehaviorKey)
			{
				NonEngineNativePointerMemberBehavior = ToPointerMemberBehavior(It.Value().GetValue());
			}
			else if (It.Key() == NonEngineObjectPtrMemberBehaviorKey)
			{
				NonEngineObjectPtrMemberBehavior = ToPointerMemberBehavior(It.Value().GetValue());
			}
		}
	}
}

const FUHTConfig& FUHTConfig::Get()
{
	static FUHTConfig UHTConfig;
	return UHTConfig;
}


/////////////////////////////////////////////////////
// FHeaderParser

/*-----------------------------------------------------------------------------
	Code emitting.
-----------------------------------------------------------------------------*/


//
// Get a qualified class.
//
FUnrealClassDefinitionInfo* FHeaderParser::GetQualifiedClass(const TCHAR* Thing)
{
	FToken Token;
	if (GetIdentifier(Token))
	{
		RedirectTypeIdentifier(Token);
		return FUnrealClassDefinitionInfo::FindScriptClassOrThrow(*this, FString(Token.Value));
	}
	else
	{
		Throwf(TEXT("%s: Missing class name"), Thing);
	}
}

/*-----------------------------------------------------------------------------
	Fields.
-----------------------------------------------------------------------------*/

/**
 * Find a function in the specified context.  Starts with the specified scope, then iterates
 * through the Outer chain until the field is found.
 * 
 * @param	InScope				scope to start searching for the field in 
 * @param	InIdentifier		name of the field we're searching for
 * @param	bIncludeParents		whether to allow searching in the scope of a parent struct
 * @param	Thing				hint text that will be used in the error message if an error is encountered
 *
 * @return	a pointer to a UField with a name matching InIdentifier, or NULL if it wasn't found
 */
FUnrealFunctionDefinitionInfo* FHeaderParser::FindFunction
(

	const FUnrealStructDefinitionInfo& InScope,
	const TCHAR*	InIdentifier,
	bool			bIncludeParents,
	const TCHAR*	Thing
)
{
	check(InIdentifier);
	FName InName(InIdentifier, FNAME_Find);
	if (InName != NAME_None)
	{
		for (const FUnrealStructDefinitionInfo* Scope = &InScope; Scope; Scope = UHTCast<FUnrealStructDefinitionInfo>(Scope->GetOuter()))
		{
			for (FUnrealPropertyDefinitionInfo* PropertyDef : TUHTFieldRange<FUnrealPropertyDefinitionInfo>(*Scope))
			{
				if (PropertyDef->GetFName() == InName)
				{
					if (Thing)
					{
						InScope.Throwf(TEXT("%s: expecting function or delegate, got a property"), Thing);
					}
					return nullptr;
				}
			}
			for (FUnrealFunctionDefinitionInfo* FunctionDef : TUHTFieldRange<FUnrealFunctionDefinitionInfo>(*Scope))
			{
				if (FunctionDef->GetFName() == InName)
				{
					return FunctionDef;
				}
			}

			if (!bIncludeParents)
			{
				break;
			}
		}
	}

	return nullptr;
}

FUnrealPropertyDefinitionInfo* FHeaderParser::FindProperty(const FUnrealStructDefinitionInfo& InScope, const TCHAR* InIdentifier, bool bIncludeParents, const TCHAR* Thing)
{
	check(InIdentifier);
	FName InName(InIdentifier, FNAME_Find);
	if (InName != NAME_None)
	{
		for (const FUnrealStructDefinitionInfo* Scope = &InScope; Scope; Scope = UHTCast<FUnrealStructDefinitionInfo>(Scope->GetOuter()))
		{
			for (FUnrealFunctionDefinitionInfo* FunctionDef : TUHTFieldRange<FUnrealFunctionDefinitionInfo>(*Scope))
			{
				if (FunctionDef->GetFName() == InName)
				{
					if (Thing)
					{
						InScope.Throwf(TEXT("%s: expecting a property, got a function or delegate"), Thing);
					}
					return nullptr;
				}
			}
			for (FUnrealPropertyDefinitionInfo* PropertyDef : TUHTFieldRange<FUnrealPropertyDefinitionInfo>(*Scope))
			{
				if (PropertyDef->GetFName() == InName)
				{
					return PropertyDef;
				}
			}

			if (!bIncludeParents)
			{
				break;
			}
		}
	}
	return nullptr;
}

/**
 * Adds source file's include path to given metadata.
 *
 * @param Type Type for which to add include path.
 * @param MetaData Meta data to fill the information.
 */
void AddIncludePathToMetadata(FUnrealFieldDefinitionInfo& FieldDef, TMap<FName, FString> &MetaData)
{
	// Add metadata for the include path.
	MetaData.Add(NAME_IncludePath, FieldDef.GetUnrealSourceFile().GetIncludePath());
}

/**
 * Adds module's relative path from given file.
 *
 * @param SourceFile Given source file.
 * @param MetaData Meta data to fill the information.
 */
void AddModuleRelativePathToMetadata(FUnrealSourceFile& SourceFile, TMap<FName, FString> &MetaData)
{
	MetaData.Add(NAME_ModuleRelativePath, SourceFile.GetModuleRelativePath());
}

/**
 * Adds module's relative path to given metadata.
 *
 * @param Type Type for which to add module's relative path.
 * @param MetaData Meta data to fill the information.
 */
void AddModuleRelativePathToMetadata(FUnrealTypeDefinitionInfo& Type, TMap<FName, FString> &MetaData)
{
	// Don't add module relative paths to functions.
	if (Type.AsFunction() == nullptr && Type.HasSource())
	{
		MetaData.Add(NAME_ModuleRelativePath, Type.GetUnrealSourceFile().GetModuleRelativePath());
	}
}

/*-----------------------------------------------------------------------------
	Variables.
-----------------------------------------------------------------------------*/

//
// Compile an enumeration definition.
//
FUnrealEnumDefinitionInfo& FHeaderParser::CompileEnum()
{
	TSharedPtr<FFileScope> Scope = SourceFile.GetScope();

	CheckAllow( TEXT("'Enum'"), ENestAllowFlags::TypeDecl );

	// Get the enum specifier list
	FToken                     EnumToken;
	FMetaData				   EnumMetaData;
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Enum"), EnumMetaData);

	// Check enum type. This can be global 'enum', 'namespace' or 'enum class' enums.
	bool            bReadEnumName = false;
	UEnum::ECppForm CppForm       = UEnum::ECppForm::Regular;
	EEnumFlags      Flags         = EEnumFlags::None;
	if (!GetIdentifier(EnumToken))
	{
		Throwf(TEXT("Missing identifier after UENUM()") );
	}

	if (EnumToken.IsValue(TEXT("namespace"), ESearchCase::CaseSensitive))
	{
		CppForm = UEnum::ECppForm::Namespaced;

		SkipDeprecatedMacroIfNecessary(*this);

		bReadEnumName = GetIdentifier(EnumToken);
	}
	else if (EnumToken.IsValue(TEXT("enum"), ESearchCase::CaseSensitive))
	{
		if (!GetIdentifier(EnumToken))
		{
			Throwf(TEXT("Missing identifier after enum") );
		}

		if (EnumToken.IsValue(TEXT("class"), ESearchCase::CaseSensitive) || EnumToken.IsValue(TEXT("struct"), ESearchCase::CaseSensitive))
		{
			CppForm = UEnum::ECppForm::EnumClass;
		}
		else
		{
			// Put whatever token we found back so that we can correctly skip below
			UngetToken(EnumToken);

			CppForm = UEnum::ECppForm::Regular;
		}

		SkipAlignasAndDeprecatedMacroIfNecessary(*this);

		bReadEnumName = GetIdentifier(EnumToken);
	}
	else
	{
		Throwf(TEXT("UENUM() should be followed by \'enum\' or \'namespace\' keywords.") );
	}

	// Get enumeration name.
	if (!bReadEnumName)
	{
		Throwf(TEXT("Missing enumeration name") );
	}

	FTokenValue EnumIdentifier = EnumToken.GetTokenValue();
	ParseFieldMetaData(EnumMetaData, *EnumIdentifier);

	// Create enum definition.
	FUnrealEnumDefinitionInfo& EnumDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealEnumDefinitionInfo>(*EnumIdentifier);

	Scope->AddType(EnumDef);

	if (CppForm != EnumDef.GetCppForm())
	{
		Throwf(TEXT("ICE: Mismatch of enum CPP form between pre-parser and parser"));
	}

	for (const FPropertySpecifier& Specifier : SpecifiersFound)
	{
		switch ((EEnumSpecifier)Algo::FindSortedStringCaseInsensitive(*Specifier.Key, GEnumSpecifierStrings))
		{
		default:
			Throwf(TEXT("Unknown enum specifier '%s'"), *Specifier.Key);

		case EEnumSpecifier::Flags:
			Flags |= EEnumFlags::Flags;
			break;
		}
	}

	if ((GetCurrentCompilerDirective() & ECompilerDirective::WithEditorOnlyData) != 0)
	{
		EnumDef.MakeEditorOnly();
	}

	// Validate the metadata for the enum
	EnumDef.ValidateMetaDataFormat(EnumMetaData);

	// Read base for enum class
	EUnderlyingEnumType UnderlyingType = EUnderlyingEnumType::uint8;
	if (CppForm == UEnum::ECppForm::EnumClass)
	{
		UnderlyingType = ParseUnderlyingEnumType();
		if (UnderlyingType != EnumDef.GetUnderlyingType())
		{
			Throwf(TEXT("ICE: Mismatch of underlying enum type between pre-parser and parser"));
		}
	}
	else if (CppForm == UEnum::ECppForm::Regular)
	{
		if (MatchSymbol(TEXT(':')))
		{
			FToken BaseToken;
			if (!GetIdentifier(BaseToken))
			{
				Throwf(TEXT("Missing enum base"));
			}

			if (!BaseToken.IsValue(TEXT("int"), ESearchCase::CaseSensitive))
			{
				LogError(TEXT("Regular enums only support 'int' as the value size"));
			}
		}
	}
	else
	{
		if (EnumHasAnyFlags(Flags, EEnumFlags::Flags))
		{
			Throwf(TEXT("The 'Flags' specifier can only be used on enum classes"));
		}
	}

	if (UnderlyingType != EUnderlyingEnumType::uint8 && EnumMetaData.Contains(FHeaderParserNames::NAME_BlueprintType))
	{
		Throwf(TEXT("Invalid BlueprintType enum base - currently only uint8 supported"));
	}

	EnumDef.GetDefinitionRange().Start = &Input[InputPos];

	// Get opening brace.
	RequireSymbol( TEXT('{'), TEXT("'Enum'") );

	switch (CppForm)
	{
		case UEnum::ECppForm::Namespaced:
		{
			// Now handle the inner true enum portion
			RequireIdentifier(TEXT("enum"), ESearchCase::CaseSensitive, TEXT("'Enum'"));

			SkipAlignasAndDeprecatedMacroIfNecessary(*this);

			FToken InnerEnumToken;
			if (!GetIdentifier(InnerEnumToken))
			{
				Throwf(TEXT("Missing enumeration name") );
			}

			EnumDef.SetCppType(FString::Printf(TEXT("%s::%s"), *EnumIdentifier, *InnerEnumToken.GetTokenValue()));

			if (MatchSymbol(TEXT(':')))
			{
				FToken BaseToken;
				if (!GetIdentifier(BaseToken))
				{
					Throwf(TEXT("Missing enum base"));
				}

				if (!BaseToken.IsValue(TEXT("int"), ESearchCase::CaseSensitive))
				{
					LogError(TEXT("Namespace enums only support 'int' as the value size"));
				}
			}

			RequireSymbol( TEXT('{'), TEXT("'Enum'") );
		}
		break;

		case UEnum::ECppForm::Regular:
		case UEnum::ECppForm::EnumClass:
		{
			EnumDef.SetCppType(*EnumIdentifier);
		}
		break;
	}

	// List of all metadata generated for this enum
	TMap<FName,FString> EnumValueMetaData = EnumMetaData;

	AddModuleRelativePathToMetadata(EnumDef, EnumValueMetaData);
	AddFormattedPrevCommentAsTooltipMetaData(EnumValueMetaData);

	// Parse all enums tags.
	FToken TagToken;
	FMetaData TagMetaData;
	TArray<TMap<FName, FString>> EntryMetaData;

	bool bHasUnparsedValue = false;
	TArray<TPair<FName, int64>> EnumNames;
	int64 CurrentEnumValue = 0;
	while (GetIdentifier(TagToken))
	{
		FTokenValue TagIdentifier = TagToken.GetTokenValue();
		AddFormattedPrevCommentAsTooltipMetaData(TagMetaData);

		SkipDeprecatedMacroIfNecessary(*this);

		// Try to read an optional explicit enum value specification
		if (MatchSymbol(TEXT('=')))
		{
			FToken InitToken;
			if (!GetToken(InitToken))
			{
				Throwf(TEXT("UENUM: missing enumerator initializer"));
			}

			int64 NewEnumValue = -1;
			if (!InitToken.GetConstInt64(NewEnumValue))
			{
				// We didn't parse a literal, so set an invalid value
				NewEnumValue = -1;
				bHasUnparsedValue = true;
			}

			// Skip tokens until we encounter a comma, a closing brace or a UMETA declaration
			for (;;)
			{
				if (!GetToken(InitToken))
				{
					Throwf(TEXT("Enumerator: end of file encountered while parsing the initializer"));
				}

				if (InitToken.IsSymbol(TEXT(',')) || InitToken.IsSymbol(TEXT('}')) || InitToken.IsIdentifier(TEXT("UMETA"), ESearchCase::IgnoreCase))
				{
					UngetToken(InitToken);
					break;
				}

				// There are tokens after the initializer so it's not a standalone literal,
				// so set it to an invalid value.
				NewEnumValue = -1;
				bHasUnparsedValue = true;
			}

			CurrentEnumValue = NewEnumValue;
		}

		FName NewTag;
		switch (CppForm)
		{
			case UEnum::ECppForm::Namespaced:
			case UEnum::ECppForm::EnumClass:
			{
				NewTag = FName(*FString::Printf(TEXT("%s::%s"), *EnumIdentifier, *TagIdentifier), FNAME_Add);
			}
			break;

			case UEnum::ECppForm::Regular:
			{
				NewTag = FName(TagToken.Value, FNAME_Add);
			}
			break;
		}

		// Save the new tag
		EnumNames.Emplace(NewTag, CurrentEnumValue);

		// Autoincrement the current enumeration value
		if (CurrentEnumValue != -1)
		{
			++CurrentEnumValue;
		}

		TagMetaData.Add(NAME_Name, NewTag.ToString());
		EntryMetaData.Add(TagMetaData);

		// check for metadata on this enum value
		ParseFieldMetaData(TagMetaData, *TagIdentifier);
		if (TagMetaData.Num() > 0)
		{
			// special case for enum value metadata - we need to prepend the key name with the enum value name
			const FString TokenString = *TagIdentifier;
			for (const auto& MetaData : TagMetaData)
			{
				FString KeyString = TokenString + TEXT(".") + MetaData.Key.ToString();
				EnumValueMetaData.Emplace(*KeyString, MetaData.Value);
			}

			// now clear the metadata because we're going to reuse this token for parsing the next enum value
			TagMetaData.Empty();
		}

		if (!MatchSymbol(TEXT(',')))
		{
			FToken ClosingBrace;
			if (!GetToken(ClosingBrace))
			{
				Throwf(TEXT("UENUM: end of file encountered"));
			}

			if (ClosingBrace.IsSymbol(TEXT('}')))
			{
				UngetToken(ClosingBrace);
				break;
			}
		}
	}

	// Trailing brace and semicolon for the enum
	RequireSymbol( TEXT('}'), TEXT("'Enum'") );
	MatchSemi();

	if (CppForm == UEnum::ECppForm::Namespaced)
	{
		// Trailing brace for the namespace.
		RequireSymbol( TEXT('}'), TEXT("'Enum'") );
	}

	EnumDef.GetDefinitionRange().End = &Input[InputPos];

	// Register the list of enum names.
	if (!EnumDef.SetEnums(EnumNames, CppForm, Flags))
	{
		//@TODO - Two issues with this, first the SetEnums always returns true.  That was the case even with the original code.
		// Second issue is that the code to generate the max string doesn't work in many cases (i.e. EPixelFormat)
		const FName MaxEnumItem = *(EnumDef.GenerateEnumPrefix() + TEXT("_MAX"));
		const int32 MaxEnumItemIndex = EnumDef.GetIndexByName(MaxEnumItem);
		if (MaxEnumItemIndex != INDEX_NONE)
		{
			Throwf(TEXT("Illegal enumeration tag specified.  Conflicts with auto-generated tag '%s'"), *MaxEnumItem.ToString());
		}

		Throwf(TEXT("Unable to generate enum MAX entry '%s' due to name collision"), *MaxEnumItem.ToString());
	}

	CheckDocumentationPolicyForEnum(EnumDef, EnumValueMetaData, EntryMetaData);

	if (!EnumDef.IsValidEnumValue(0) && EnumMetaData.Contains(FHeaderParserNames::NAME_BlueprintType) && !bHasUnparsedValue)
	{
		EnumDef.LogWarning(TEXT("'%s' does not have a 0 entry! (This is a problem when the enum is initalized by default)"), *EnumDef.GetName());
	}

	// Add the metadata gathered for the enum to the package
	if (EnumValueMetaData.Num() > 0)
	{
		EnumDef.AddMetaData(MoveTemp(EnumValueMetaData));
	}
	return EnumDef;
}

/**
 * Checks if a string is made up of all the same character.
 *
 * @param  Str The string to check for all
 * @param  Ch  The character to check for
 *
 * @return True if the string is made up only of Ch characters.
 */
bool IsAllSameChar(const TCHAR* Str, TCHAR Ch)
{
	check(Str);

	while (TCHAR StrCh = *Str++)
	{
		if (StrCh != Ch)
		{
			return false;
		}
	}

	return true;
}

/**
 * @param		Input		An input string, expected to be a script comment.
 * @return					The input string, reformatted in such a way as to be appropriate for use as a tooltip.
 */
FString FHeaderParser::FormatCommentForToolTip(const FString& Input)
{
	// Return an empty string if there are no alpha-numeric characters or a Unicode characters above 0xFF
	// (which would be the case for pure CJK comments) in the input string.
	bool bFoundAlphaNumericChar = false;
	for ( int32 i = 0 ; i < Input.Len() ; ++i )
	{
		if ( FChar::IsAlnum(Input[i]) || (Input[i] > 0xFF) )
		{
			bFoundAlphaNumericChar = true;
			break;
		}
	}

	if ( !bFoundAlphaNumericChar )
	{
		return FString();
	}

	FString Result(Input);

	// Sweep out comments marked to be ignored.
	{
		int32 CommentStart, CommentEnd;
		// Block comments go first
		for (CommentStart = Result.Find(TEXT("/*~"), ESearchCase::CaseSensitive); CommentStart != INDEX_NONE; CommentStart = Result.Find(TEXT("/*~"), ESearchCase::CaseSensitive))
		{
			CommentEnd = Result.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CommentStart);
			if (CommentEnd != INDEX_NONE)
			{
				Result.RemoveAt(CommentStart, (CommentEnd + 2) - CommentStart, false);
			}
			else
			{
				// This looks like an error - an unclosed block comment.
				break;
			}
		}
		// Leftover line comments go next
		for (CommentStart = Result.Find(TEXT("//~"), ESearchCase::CaseSensitive); CommentStart != INDEX_NONE; CommentStart = Result.Find(TEXT("//~"), ESearchCase::CaseSensitive))
		{
			CommentEnd = Result.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CommentStart);
			if (CommentEnd != INDEX_NONE)
			{
				Result.RemoveAt(CommentStart, (CommentEnd + 1) - CommentStart, false);
			}
			else
			{
				Result.RemoveAt(CommentStart, Result.Len() - CommentStart, false);
				break;
			}
		}
		// Finish by shrinking if anything was removed, since we deferred this during the search.
		Result.Shrink();
	}

	// Check for known commenting styles.
	const bool bJavaDocStyle = Result.Contains(TEXT("/**"), ESearchCase::CaseSensitive);
	const bool bCStyle = Result.Contains(TEXT("/*"), ESearchCase::CaseSensitive);
	const bool bCPPStyle = Result.StartsWith(TEXT("//"), ESearchCase::CaseSensitive);

	if ( bJavaDocStyle || bCStyle)
	{
		// Remove beginning and end markers.
		if (bJavaDocStyle)
		{
			Result.ReplaceInline(TEXT("/**"), TEXT(""), ESearchCase::CaseSensitive);
		}
		if (bCStyle)
		{
			Result.ReplaceInline(TEXT("/*"), TEXT(""), ESearchCase::CaseSensitive);
		}
		Result.ReplaceInline(TEXT("*/"), TEXT(""), ESearchCase::CaseSensitive);
	}

	if ( bCPPStyle )
	{
		// Remove c++-style comment markers.  Also handle javadoc-style comments 
		Result.ReplaceInline(TEXT("///"), TEXT(""), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("//"), TEXT(""), ESearchCase::CaseSensitive);

		// Parser strips cpptext and replaces it with "// (cpptext)" -- prevent
		// this from being treated as a comment on variables declared below the
		// cpptext section
		Result.ReplaceInline(TEXT("(cpptext)"), TEXT(""));
	}

	// Get rid of carriage return or tab characters, which mess up tooltips.
	Result.ReplaceInline(TEXT( "\r" ), TEXT( "" ), ESearchCase::CaseSensitive);

	//wx widgets has a hard coded tab size of 8
	{
		const int32 SpacesPerTab = 8;
		Result.ConvertTabsToSpacesInline(SpacesPerTab);
	}

	// get rid of uniform leading whitespace and all trailing whitespace, on each line
	TArray<FString> Lines;
	Result.ParseIntoArray(Lines, TEXT("\n"), false);

	for (FString& Line : Lines)
	{
		// Remove trailing whitespace
		Line.TrimEndInline();

		// Remove leading "*" and "* " in javadoc comments.
		if (bJavaDocStyle)
		{
			// Find first non-whitespace character
			int32 Pos = 0;
			while (Pos < Line.Len() && FChar::IsWhitespace(Line[Pos]))
			{
				++Pos;
			}

			// Is it a *?
			if (Pos < Line.Len() && Line[Pos] == '*')
			{
				// Eat next space as well
				if (Pos+1 < Line.Len() && FChar::IsWhitespace(Line[Pos+1]))
				{
					++Pos;
				}

				Line.RightChopInline(Pos + 1, false);
			}
		}
			}

	auto IsWhitespaceOrLineSeparator = [](const FString& Line)
	{
		int32 LineLength = Line.Len();
		int32 WhitespaceCount = 0;
		while (WhitespaceCount < LineLength && FChar::IsWhitespace(Line[WhitespaceCount]))
		{
			++WhitespaceCount;
		}

		if (WhitespaceCount == LineLength)
		{
			return true;
	}

		const TCHAR* Str = (*Line) + WhitespaceCount;
		return IsAllSameChar(Str, TEXT('-')) || IsAllSameChar(Str, TEXT('=')) || IsAllSameChar(Str, TEXT('*'));
	};

	// Find first meaningful line
	int32 FirstIndex = 0;
	for (const FString& Line : Lines)
	{
		if (!IsWhitespaceOrLineSeparator(Line))
	{
			break;
		}

		++FirstIndex;
	}

	int32 LastIndex = Lines.Num();
	while (LastIndex != FirstIndex)
	{
		const FString& Line = Lines[LastIndex - 1];

		if (!IsWhitespaceOrLineSeparator(Line))
		{
			break;
		}

		--LastIndex;
	}

	Result.Reset();

	if (FirstIndex != LastIndex)
	{
		FString& FirstLine = Lines[FirstIndex];

		// Figure out how much whitespace is on the first line
		int32 MaxNumWhitespaceToRemove;
		for (MaxNumWhitespaceToRemove = 0; MaxNumWhitespaceToRemove < FirstLine.Len(); MaxNumWhitespaceToRemove++)
		{
			if (!FChar::IsLinebreak(FirstLine[MaxNumWhitespaceToRemove]) && !FChar::IsWhitespace(FirstLine[MaxNumWhitespaceToRemove]))
			{
				break;
			}
		}

		for (int32 Index = FirstIndex; Index != LastIndex; ++Index)
		{
			FString& Line = Lines[Index];

			int32 TemporaryMaxWhitespace = MaxNumWhitespaceToRemove;

			// Allow eating an extra tab on subsequent lines if it's present
			if ((Index > FirstIndex) && (Line.Len() > 0) && (Line[0] == '\t'))
			{
				TemporaryMaxWhitespace++;
			}

			// Advance past whitespace
			int32 Pos = 0;
			while (Pos < TemporaryMaxWhitespace && Pos < Line.Len() && FChar::IsWhitespace(Line[Pos]))
			{
				++Pos;
			}

			if (Pos > 0)
			{
				Line.RightChopInline(Pos, false);
			}

			if (Index > FirstIndex)
			{
				Result += TEXT("\n");
			}

			if (Line.Len() && !IsAllSameChar(*Line, TEXT('=')))
			{
				Result += Line;
			}
		}
	}

	//@TODO: UCREMOVAL: Really want to trim an arbitrary number of newlines above and below, but keep multiple newlines internally
	// Make sure it doesn't start with a newline
	if (!Result.IsEmpty() && FChar::IsLinebreak(Result[0]))
	{
		Result.RightChopInline(1, false);
	}

	// Make sure it doesn't end with a dead newline
	if (!Result.IsEmpty() && FChar::IsLinebreak(Result[Result.Len() - 1]))
	{
		Result.LeftInline(Result.Len() - 1, false);
	}

	// Done.
	return Result;
}

TMap<FName, FString> FHeaderParser::GetParameterToolTipsFromFunctionComment(const FString& Input)
{
	SCOPE_SECONDS_COUNTER_UHT(DocumentationPolicy);

	TMap<FName, FString> Map;
	if (Input.IsEmpty())
	{
		return Map;
	}
	
	TArray<FString> Params;
	static const auto& ParamTag = TEXT("@param");
	static const auto& ReturnTag = TEXT("@return");
	static const auto& ReturnParamPrefix = TEXT("ReturnValue ");

	/**
	 * Search for @param / @return followed by a section until a line break.
	 * For example: "@param Test MyTest Variable" becomes "Test", "MyTest Variable"
	 * These pairs are then later split and stored as the parameter tooltips.
	 * Once we don't find either @param or @return we break from the loop.
	 */
	int32 Offset = 0;
	while (Offset < Input.Len())
	{
		const TCHAR* ParamPrefix = TEXT("");
		int32 ParamStart = Input.Find(ParamTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, Offset);
		if(ParamStart != INDEX_NONE)
		{
			ParamStart = ParamStart + UE_ARRAY_COUNT(ParamTag);
			Offset = ParamStart;
		}
		else
		{
			ParamStart = Input.Find(ReturnTag, ESearchCase::CaseSensitive, ESearchDir::FromStart, Offset);
			if (ParamStart != INDEX_NONE)
			{
				ParamStart = ParamStart + UE_ARRAY_COUNT(ReturnTag);
				Offset = ParamStart;
				ParamPrefix = ReturnParamPrefix;
			}
			else
			{
				// no @param, no @return?
				break;
			}
		}

		int32 ParamEnd = Input.Find(TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ParamStart);
		if (ParamEnd == INDEX_NONE)
		{
			ParamEnd = Input.Len();
		}
		Offset = ParamEnd;

		Params.Add(ParamPrefix + Input.Mid(ParamStart, ParamEnd - ParamStart - 1));
	}

	for (FString& Param : Params)
	{
		Param.ConvertTabsToSpacesInline(4);
		Param.TrimStartAndEndInline();

		int32 FirstSpaceIndex = -1;
		if (!Param.FindChar(TEXT(' '), FirstSpaceIndex))
		{
			continue;
		}

		FString ParamToolTip = Param.Mid(FirstSpaceIndex + 1);
		ParamToolTip.TrimStartInline();

		Param.LeftInline(FirstSpaceIndex);

		Map.Add(*Param, MoveTemp(ParamToolTip));
	}

	return Map;
}


void FHeaderParser::AddFormattedPrevCommentAsTooltipMetaData(TMap<FName, FString>& MetaData)
{
	// Don't add a tooltip if one already exists.
	if (MetaData.Find(NAME_ToolTip))
	{
		return;
	}

	// Add the comment if it is not empty
	if (!PrevComment.IsEmpty())
	{
		MetaData.Add(NAME_Comment, *PrevComment);
	}

	// Don't add a tooltip if the comment is empty after formatting.
	FString FormattedComment = FormatCommentForToolTip(PrevComment);
	if (!FormattedComment.Len())
	{
		return;
	}

	MetaData.Add(NAME_ToolTip, *FormattedComment);

	// We've already used this comment as a tooltip, so clear it so that it doesn't get used again
	PrevComment.Empty();
}

static const TCHAR* GetAccessSpecifierName(EAccessSpecifier AccessSpecifier)
{
	switch (AccessSpecifier)
	{
		case ACCESS_Public:
			return TEXT("public");
		case ACCESS_Protected:
			return TEXT("protected");
		case ACCESS_Private:
			return TEXT("private");
		default:
			check(0);
	}
	return TEXT("");
}

// Tries to parse the token as an access protection specifier (public:, protected:, or private:)
EAccessSpecifier FHeaderParser::ParseAccessProtectionSpecifier(const FToken& Token)
{
	if (!Token.IsIdentifier())
	{
		return ACCESS_NotAnAccessSpecifier;
	}

	for (EAccessSpecifier Test = EAccessSpecifier(ACCESS_NotAnAccessSpecifier + 1); Test != ACCESS_Num; Test = EAccessSpecifier(Test + 1))
	{
		if (Token.IsValue(GetAccessSpecifierName(Test), ESearchCase::CaseSensitive))
		{
			auto ErrorMessageGetter = [&Token]() { return FString::Printf(TEXT("after %s"), *Token.GetTokenValue());  };

			// Consume the colon after the specifier
			RequireSymbol(TEXT(':'), ErrorMessageGetter);
			return Test;
		}
	}
	return ACCESS_NotAnAccessSpecifier;
}


/**
 * Compile a struct definition.
 */
FUnrealScriptStructDefinitionInfo& FHeaderParser::CompileStructDeclaration()
{
	int32 DeclInputLine = InputLine;
	TSharedPtr<FFileScope> Scope = SourceFile.GetScope();

	// Make sure structs can be declared here.
	CheckAllow( TEXT("'struct'"), ENestAllowFlags::TypeDecl );

	bool IsNative = false;
	bool IsExport = false;
	bool IsTransient = false;
	TMap<FName, FString> MetaData;

	// Get the struct specifier list
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Struct"), MetaData);

	// Consume the struct keyword
	RequireIdentifier(TEXT("struct"), ESearchCase::CaseSensitive, TEXT("Struct declaration specifier"));

	// The struct name as parsed in script and stripped of it's prefix
	FString StructNameInScript;

	// The struct name stripped of it's prefix
	FString StructNameStripped;

	// The required API module for this struct, if any
	FString RequiredAPIMacroIfPresent;

	SkipAlignasAndDeprecatedMacroIfNecessary(*this);

	// Read the struct name
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ StructNameInScript, /*out*/ RequiredAPIMacroIfPresent, TEXT("struct"));

	StructNameStripped = GetClassNameWithPrefixRemoved(StructNameInScript);

	// Effective struct name
	const FString EffectiveStructName = *StructNameStripped;

	// Locate the structure
	FUnrealScriptStructDefinitionInfo& StructDef = GTypeDefinitionInfoMap.FindByNameChecked<FUnrealScriptStructDefinitionInfo>(*StructNameStripped);

	if (StructDef.HasAnyStructFlags(STRUCT_Immutable))
	{
		if (!FPaths::IsSamePath(Filename, GUObjectDef->GetUnrealSourceFile().GetFilename()))
		{
			LogError(TEXT("Immutable is being phased out in favor of SerializeNative, and is only legal on the mirror structs declared in UObject"));
		}
	}

	// Skip optional final keyword
	MatchIdentifier(TEXT("final"), ESearchCase::CaseSensitive);

	// Parse the inheritance list
	ParseInheritance(TEXT("struct"), [](const TCHAR* StructName, bool bIsSuperClass) {}); // Eat the results, already been parsed

	// if we have a base struct, propagate inherited struct flags now
	if (!StructDef.GetSuperStructInfo().Name.IsEmpty())
	{
		FUnrealScriptStructDefinitionInfo* BaseStructDef = nullptr;

		const FString& ParentStructNameInScript = StructDef.GetSuperStructInfo().Name;
		FString ParentStructNameStripped;
		bool bOverrideParentStructName = false;

		if (!UHTConfig.StructsWithNoPrefix.Contains(ParentStructNameInScript))
		{
			bOverrideParentStructName = true;
			ParentStructNameStripped = GetClassNameWithPrefixRemoved(ParentStructNameInScript);
		}

		// If we're expecting a prefix, first try finding the correct field with the stripped struct name
		if (bOverrideParentStructName)
		{
			BaseStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(Scope->FindTypeByName(*ParentStructNameStripped));
		}

		// If it wasn't found, try to find the literal name given
		if (BaseStructDef == nullptr)
		{
			BaseStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(Scope->FindTypeByName(*ParentStructNameInScript));
		}

		// Try to locate globally  //@TODO: UCREMOVAL: This seems extreme
		if (BaseStructDef == nullptr)
		{
			if (bOverrideParentStructName)
			{
				BaseStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*ParentStructNameStripped);
			}

			if (BaseStructDef == nullptr)
			{
				BaseStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*ParentStructNameInScript);
			}
		}

		// If the struct still wasn't found, throw an error
		if (BaseStructDef == nullptr)
		{
			Throwf(TEXT("'struct': Can't find struct '%s'"), *ParentStructNameInScript);
		}

		// If the struct was found, confirm it adheres to the correct syntax. This should always fail if we were expecting an override that was not found.
		if (bOverrideParentStructName)
		{
			const TCHAR* PrefixCPP = UHTConfig.StructsWithTPrefix.Contains(ParentStructNameStripped) ? TEXT("T") : BaseStructDef->GetPrefixCPP();
			if (ParentStructNameInScript != FString::Printf(TEXT("%s%s"), PrefixCPP, *ParentStructNameStripped))
			{
				Throwf(TEXT("Parent Struct '%s' is missing a valid Unreal prefix, expecting '%s'"), *ParentStructNameInScript, *FString::Printf(TEXT("%s%s"), PrefixCPP, *BaseStructDef->GetName()));
			}
		}

		StructDef.GetSuperStructInfo().Struct = BaseStructDef->AsStruct();
		StructDef.SetStructFlags(EStructFlags(BaseStructDef->GetStructFlags() & STRUCT_Inherit));
		if (StructDef.GetScriptStructSafe() != nullptr && BaseStructDef->GetScriptStructSafe())
		{
			StructDef.GetScriptStructSafe()->SetSuperStruct(BaseStructDef->GetScriptStructSafe());
		}
	}

	Scope->AddType(StructDef);

	AddModuleRelativePathToMetadata(StructDef, MetaData);

	// Check to make sure the syntactic native prefix was set-up correctly.
	// If this check results in a false positive, it will be flagged as an identifier failure.
	FString DeclaredPrefix = GetClassPrefix( StructNameInScript );
	if( DeclaredPrefix == StructDef.GetPrefixCPP() || DeclaredPrefix == TEXT("T") )
	{
		// Found a prefix, do a basic check to see if it's valid
		const TCHAR* ExpectedPrefixCPP = UHTConfig.StructsWithTPrefix.Contains(StructNameStripped) ? TEXT("T") : StructDef.GetPrefixCPP();
		FString ExpectedStructName = FString::Printf(TEXT("%s%s"), ExpectedPrefixCPP, *StructNameStripped);
		if (StructNameInScript != ExpectedStructName)
		{
			Throwf(TEXT("Struct '%s' has an invalid Unreal prefix, expecting '%s'"), *StructNameInScript, *ExpectedStructName);
		}
	}
	else
	{
		const TCHAR* ExpectedPrefixCPP = UHTConfig.StructsWithTPrefix.Contains(StructNameInScript) ? TEXT("T") : StructDef.GetPrefixCPP();
		FString ExpectedStructName = FString::Printf(TEXT("%s%s"), ExpectedPrefixCPP, *StructNameInScript);
		Throwf(TEXT("Struct '%s' is missing a valid Unreal prefix, expecting '%s'"), *StructNameInScript, *ExpectedStructName);
	}

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	// Register the metadata
	FUHTMetaData::RemapAndAddMetaData(StructDef, MoveTemp(MetaData));

	StructDef.GetDefinitionRange().Start = &Input[InputPos];

	// Get opening brace.
	RequireSymbol( TEXT('{'), TEXT("'struct'") );

	// Members of structs have a default public access level in c++
	// Assume that, but restore the parser state once we finish parsing this struct
	TGuardValue<EAccessSpecifier> HoldFromClass(CurrentAccessSpecifier, ACCESS_Public);

	int32 SavedLineNumber = InputLine;

	// Clear comment before parsing body of the struct.
	

	// Parse all struct variables.
	FToken Token;
	while (1)
	{
		ClearComment();
		GetToken( Token );
		int StartingLine = InputLine;

		if (EAccessSpecifier AccessSpecifier = ParseAccessProtectionSpecifier(Token))
		{
			CurrentAccessSpecifier = AccessSpecifier;
		}
		else if (Token.IsIdentifier(TEXT("UPROPERTY"), ESearchCase::CaseSensitive))
		{
			CompileVariableDeclaration(StructDef);
		}
		else if (Token.IsIdentifier(TEXT("UFUNCTION"), ESearchCase::CaseSensitive))
		{
			Throwf(TEXT("USTRUCTs cannot contain UFUNCTIONs."));
		}
		else if (Token.IsIdentifier(TEXT("RIGVM_METHOD"), ESearchCase::CaseSensitive))
		{
			CompileRigVMMethodDeclaration(StructDef);
		}
		else if (Token.IsIdentifier(TEXT("GENERATED_USTRUCT_BODY"), ESearchCase::CaseSensitive) || Token.IsIdentifier(TEXT("GENERATED_BODY"), ESearchCase::CaseSensitive))
		{
			// Match 'GENERATED_USTRUCT_BODY' '(' [StructName] ')' or 'GENERATED_BODY' '(' [StructName] ')'
			if (CurrentAccessSpecifier != ACCESS_Public)
			{
				Throwf(TEXT("%s must be in the public scope of '%s', not private or protected."), *Token.GetTokenValue(), *StructNameInScript);
			}

			if (StructDef.GetMacroDeclaredLineNumber() != INDEX_NONE)
			{
				Throwf(TEXT("Multiple %s declarations found in '%s'"), *Token.GetTokenValue(), *StructNameInScript);
			}

			StructDef.SetMacroDeclaredLineNumber(InputLine);
			RequireSymbol(TEXT('('), TEXT("'struct'"));

			CompileVersionDeclaration(StructDef);

			RequireSymbol(TEXT(')'), TEXT("'struct'"));

			// Eat a semicolon if present (not required)
			SafeMatchSymbol(TEXT(';'));
		}
		else if ( Token.IsSymbol(TEXT('#')) && MatchIdentifier(TEXT("ifdef"), ESearchCase::CaseSensitive) )
		{
			PushCompilerDirective(ECompilerDirective::Insignificant);
		}
		else if ( Token.IsSymbol(TEXT('#')) && MatchIdentifier(TEXT("ifndef"), ESearchCase::CaseSensitive) )
		{
			PushCompilerDirective(ECompilerDirective::Insignificant);
		}
		else if (Token.IsSymbol(TEXT('#')) && MatchIdentifier(TEXT("endif"), ESearchCase::CaseSensitive))
		{
			PopCompilerDirective();
			// Do nothing and hope that the if code below worked out OK earlier

			// skip it and skip over the text, it is not recorded or processed
			if (StartingLine == InputLine)
			{
				TCHAR c;
				while (!IsEOL(c = GetChar()))
				{
				}
				ClearComment();
			}
		}
		else if ( Token.IsSymbol(TEXT('#')) && MatchIdentifier(TEXT("if"), ESearchCase::CaseSensitive) )
		{
			//@TODO: This parsing should be combined with CompileDirective and probably happen much much higher up!
			bool bInvertConditional = MatchSymbol(TEXT('!'));
			bool bConsumeAsCppText = false;

			if (MatchIdentifier(TEXT("WITH_EDITORONLY_DATA"), ESearchCase::CaseSensitive) )
			{
				if (bInvertConditional)
				{
					Throwf(TEXT("Cannot use !WITH_EDITORONLY_DATA"));
				}

				PushCompilerDirective(ECompilerDirective::WithEditorOnlyData);
			}
			else if (MatchIdentifier(TEXT("WITH_EDITOR"), ESearchCase::CaseSensitive) )
			{
				if (bInvertConditional)
				{
					Throwf(TEXT("Cannot use !WITH_EDITOR"));
				}
				PushCompilerDirective(ECompilerDirective::WithEditor);
			}
			else if (MatchIdentifier(TEXT("CPP"), ESearchCase::CaseSensitive) || MatchConstInt(TEXT("0")) || MatchConstInt(TEXT("1")) || MatchIdentifier(TEXT("WITH_HOT_RELOAD"), ESearchCase::CaseSensitive) || MatchIdentifier(TEXT("WITH_HOT_RELOAD_CTORS"), ESearchCase::CaseSensitive))
			{
				bConsumeAsCppText = !bInvertConditional;
				PushCompilerDirective(ECompilerDirective::Insignificant);
			}
			else
			{
				Throwf(TEXT("'struct': Unsupported preprocessor directive inside a struct.") );
			}

			if (bConsumeAsCppText)
			{
				// Skip over the text, it is not recorded or processed
				int32 nest = 1;
				while (nest > 0)
				{
					TCHAR ch = GetChar(1);

					if ( ch==0 )
					{
						Throwf(TEXT("Unexpected end of struct definition %s"), *StructDef.GetName());
					}
					else if ( ch=='{' || (ch=='#' && (PeekIdentifier(TEXT("if"), ESearchCase::CaseSensitive) || PeekIdentifier(TEXT("ifdef"), ESearchCase::CaseSensitive))) )
					{
						nest++;
					}
					else if ( ch=='}' || (ch=='#' && PeekIdentifier(TEXT("endif"), ESearchCase::CaseSensitive)) )
					{
						nest--;
					}

					if (nest==0)
					{
						RequireIdentifier(TEXT("endif"), ESearchCase::CaseSensitive, TEXT("'if'"));
					}
				}
			}
		}
		else if (Token.IsSymbol(TEXT('#')) && MatchIdentifier(TEXT("pragma"), ESearchCase::CaseSensitive))
		{
			// skip it and skip over the text, it is not recorded or processed
			TCHAR c;
			while (!IsEOL(c = GetChar()))
			{
			}
		}
		else if (ProbablyAnUnknownObjectLikeMacro(*this, Token))
		{
			// skip it
		}
		else
		{
			if (!Token.IsSymbol( TEXT('}')))
			{
				// Skip declaration will destroy data in Token, so cache off the identifier in case we need to provfide an error
				FToken FirstToken = Token;
				if (!SkipDeclaration(Token))
				{
					Throwf(TEXT("'struct': Unexpected '%s'"), *FirstToken.GetTokenValue());
				}	
			}
			else
			{
				MatchSemi();
				break;
			}
		}
	}

	StructDef.GetDefinitionRange().End = &Input[InputPos];

	// Validation
	bool bStructBodyFound = StructDef.GetMacroDeclaredLineNumber() != INDEX_NONE;
	if (!bStructBodyFound && StructDef.HasAnyStructFlags(STRUCT_Native))
	{
		// Roll the line number back to the start of the struct body and error out
		InputLine = SavedLineNumber;
		Throwf(TEXT("Expected a GENERATED_BODY() at the start of struct"));
	}

	// check if the struct is marked as deprecated and does not implement the upgrade path
	if(StructDef.HasMetaData(TEXT("Deprecated")))
	{
		const FRigVMStructInfo& StructRigVMInfo = StructDef.GetRigVMInfo();
		if(!StructRigVMInfo.bHasGetUpgradeInfoMethod && !StructRigVMInfo.Methods.IsEmpty())
		{
			LogError(TEXT(
				"RigVMStruct '%s' is marked as deprecated but is missing GetUpgradeInfo method.\n"
				"Please implement a method like below:\n\n"
				"RIGVM_METHOD()\n"
				"virtual FRigVMStructUpgradeInfo GetUpgradeInfo() const override;"),
				*StructDef.GetNameCPP());
		}
	}

	// Validate sparse class data
	CheckSparseClassData(StructDef);
	return StructDef;
}

/*-----------------------------------------------------------------------------
	Retry management.
-----------------------------------------------------------------------------*/

/**
 * Remember the current compilation points, both in the source being
 * compiled and the object code being emitted.
 *
 * @param	Retry	[out] filled in with current compiler position information
 */
void FHeaderParser::InitScriptLocation( FScriptLocation& Retry )
{
	Retry.Input = Input;
	Retry.InputPos = InputPos;
	Retry.InputLine	= InputLine;
}

/**
 * Return to a previously-saved retry point.
 *
 * @param	Retry	the point to return to
 * @param	Binary	whether to modify the compiled bytecode
 * @param	bText	whether to modify the compiler's current location in the text
 */
void FHeaderParser::ReturnToLocation(const FScriptLocation& Retry, bool Binary, bool bText)
{
	if (bText)
	{
		Input = Retry.Input;
		InputPos = Retry.InputPos;
		InputLine = Retry.InputLine;
	}
}

/*-----------------------------------------------------------------------------
	Nest information.
-----------------------------------------------------------------------------*/

//
// Return the name for a nest type.
//
const TCHAR *FHeaderParser::NestTypeName( ENestType NestType )
{
	switch( NestType )
	{
		case ENestType::GlobalScope:
			return TEXT("Global Scope");
		case ENestType::Class:
			return TEXT("Class");
		case ENestType::NativeInterface:
		case ENestType::Interface:
			return TEXT("Interface");
		case ENestType::FunctionDeclaration:
			return TEXT("Function");
		default:
			check(false);
			return TEXT("Unknown");
	}
}

// Checks to see if a particular kind of command is allowed on this nesting level.
bool FHeaderParser::IsAllowedInThisNesting(ENestAllowFlags AllowFlags)
{
	return (TopNest->Allow & AllowFlags) != ENestAllowFlags::None;
}

//
// Make sure that a particular kind of command is allowed on this nesting level.
// If it's not, issues a compiler error referring to the token and the current
// nesting level.
//
void FHeaderParser::CheckAllow( const TCHAR* Thing, ENestAllowFlags AllowFlags )
{
	if (!IsAllowedInThisNesting(AllowFlags))
	{
		if (TopNest->NestType == ENestType::GlobalScope)
		{
			Throwf(TEXT("%s is not allowed before the Class definition"), Thing );
		}
		else
		{
			Throwf(TEXT("%s is not allowed here"), Thing );
		}
	}
}

/*-----------------------------------------------------------------------------
	Nest management.
-----------------------------------------------------------------------------*/

void FHeaderParser::PushNest(ENestType NestType, FUnrealStructDefinitionInfo* InNodeDef, FUnrealSourceFile* InSourceFile)
{
	// Update pointer to top nesting level.
	TopNest = &Nest[NestLevel++];
	TopNest->SetScope(NestType == ENestType::GlobalScope ? &InSourceFile->GetScope().Get() : &InNodeDef->GetScope().Get());
	TopNest->NestType = NestType;

	// Prevent overnesting.
	if (NestLevel >= MAX_NEST_LEVELS)
	{
		Throwf(TEXT("Maximum nesting limit exceeded"));
	}

	// Inherit info from stack node above us.
	if (NestLevel > 1 && NestType == ENestType::GlobalScope)
	{
		// Use the existing stack node.
		TopNest->SetScope(TopNest[-1].GetScope());
	}

	// NestType specific logic.
	switch (NestType)
	{
	case ENestType::GlobalScope:
		TopNest->Allow = ENestAllowFlags::Class | ENestAllowFlags::TypeDecl | ENestAllowFlags::ImplicitDelegateDecl;
		break;

	case ENestType::Class:
		TopNest->Allow = ENestAllowFlags::VarDecl | ENestAllowFlags::Function | ENestAllowFlags::ImplicitDelegateDecl;
		break;

	case ENestType::NativeInterface:
	case ENestType::Interface:
		TopNest->Allow = ENestAllowFlags::Function;
		break;

	case ENestType::FunctionDeclaration:
		TopNest->Allow = ENestAllowFlags::VarDecl;

		break;

	default:
		Throwf(TEXT("Internal error in PushNest, type %i"), (uint8)NestType);
		break;
	}
}

/**
 * Decrease the nesting level and handle any errors that result.
 *
 * @param	NestType	nesting type of the current node
 * @param	Descr		text to use in error message if any errors are encountered
 */
void FHeaderParser::PopNest(ENestType NestType, const TCHAR* Descr)
{
	// Validate the nesting state.
	if (NestLevel <= 0)
	{
		Throwf(TEXT("Unexpected '%s' at global scope"), Descr, NestTypeName(NestType));
	}
	else if (TopNest->NestType != NestType)
	{
		Throwf(TEXT("Unexpected end of %s in '%s' block"), Descr, NestTypeName(TopNest->NestType));
	}

	if (NestType != ENestType::GlobalScope && NestType != ENestType::Class && NestType != ENestType::Interface && NestType != ENestType::NativeInterface && NestType != ENestType::FunctionDeclaration)
	{
		Throwf(TEXT("Bad first pass NestType %i"), (uint8)NestType);
	}

	// Pop the nesting level.
	NestType = TopNest->NestType;
	NestLevel--;
	if (NestLevel == 0)
	{
		TopNest = nullptr;
	}
	else
	{
		TopNest--;
		check(TopNest >= Nest);

	}
}

void FHeaderParser::FixupDelegateProperties(FUnrealStructDefinitionInfo& StructDef, FScope& Scope, TMap<FName, FUnrealFunctionDefinitionInfo*>& DelegateCache )
{
	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : StructDef.GetProperties())
	{
		FPropertyBase& PropertyBase = PropertyDef->GetPropertyBase();

		// Ignore any containers types but static arrays
		if (PropertyBase.ArrayType != EArrayType::None && PropertyBase.ArrayType != EArrayType::Static)
		{
			continue;
		}

		// At this point, we can check to see if we are a delegate or multicast delegate directly off the property definition
		bool bIsDelegate = PropertyBase.Type == CPT_Delegate;
		bool bIsMulticastDelegate = PropertyBase.Type == CPT_MulticastDelegate;
		if (!bIsDelegate && !bIsMulticastDelegate)
		{
			continue;
		}

		// this FDelegateProperty corresponds to an actual delegate variable (i.e. delegate<SomeDelegate> Foo); we need to lookup the token data for
		// this property and verify that the delegate property's "type" is an actual delegate function
		FPropertyBase& DelegatePropertyBase = PropertyDef->GetPropertyBase();

		// attempt to find the delegate function in the map of functions we've already found
		FUnrealFunctionDefinitionInfo* SourceDelegateFunctionDef = DelegateCache.FindRef(DelegatePropertyBase.DelegateName);
		if (SourceDelegateFunctionDef == nullptr)
		{
			FString NameOfDelegateFunction = DelegatePropertyBase.DelegateName.ToString() + FString( HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX );
			if ( !NameOfDelegateFunction.Contains(TEXT(".")) )
			{
				// an unqualified delegate function name - search for a delegate function by this name within the current scope
				if (FUnrealFieldDefinitionInfo* FoundDef = Scope.FindTypeByName(*NameOfDelegateFunction))
				{
					SourceDelegateFunctionDef = FoundDef->AsFunction();
				}
				if (SourceDelegateFunctionDef == nullptr && DelegatePropertyBase.DelegateSignatureOwnerClassDef != nullptr)
				{
					SourceDelegateFunctionDef = FindFunction(*DelegatePropertyBase.DelegateSignatureOwnerClassDef, *NameOfDelegateFunction, true, nullptr);
				}
				if (SourceDelegateFunctionDef == nullptr)
				{
					FScopeLock Lock(&GlobalDelegatesLock);
					SourceDelegateFunctionDef = GlobalDelegates.FindRef(NameOfDelegateFunction);
				}
				if (SourceDelegateFunctionDef == nullptr)
				{
					// convert this into a fully qualified path name for the error message.
					NameOfDelegateFunction = Scope.GetName().ToString() + TEXT(".") + NameOfDelegateFunction;
				}
			}
			else
			{
				FString DelegateClassName, DelegateName;
				NameOfDelegateFunction.Split(TEXT("."), &DelegateClassName, &DelegateName);

				// verify that we got a valid string for the class name
				if ( DelegateClassName.Len() == 0 )
				{
					UngetToken(PropertyDef->GetLineNumber(), PropertyDef->GetParsePosition());
					Throwf(TEXT("Invalid scope specified in delegate property function reference: '%s'"), *NameOfDelegateFunction);
				}

				// verify that we got a valid string for the name of the function
				if ( DelegateName.Len() == 0 )
				{
					UngetToken(PropertyDef->GetLineNumber(), PropertyDef->GetParsePosition());
					Throwf(TEXT("Invalid delegate name specified in delegate property function reference '%s'"), *NameOfDelegateFunction);
				}

				// make sure that the class that contains the delegate can be referenced here
				FUnrealClassDefinitionInfo* DelegateOwnerClassDef = FUnrealClassDefinitionInfo::FindScriptClassOrThrow(*this, DelegateClassName);
				if (DelegateOwnerClassDef->GetScope()->FindTypeByName(*DelegateName) != nullptr)
				{
					Throwf(TEXT("Inaccessible type: '%s'"), *DelegateOwnerClassDef->GetPathName());
				}
				SourceDelegateFunctionDef = FindFunction(*DelegateOwnerClassDef, *DelegateName, false, nullptr);
			}

			if (SourceDelegateFunctionDef == NULL )
			{
				UngetToken(PropertyDef->GetLineNumber(), PropertyDef->GetParsePosition());
				Throwf(TEXT("Failed to find delegate function '%s'"), *NameOfDelegateFunction);
			}
			else if (!SourceDelegateFunctionDef->HasAnyFunctionFlags(FUNC_Delegate))
			{
				UngetToken(PropertyDef->GetLineNumber(), PropertyDef->GetParsePosition());
				Throwf(TEXT("Only delegate functions can be used as the type for a delegate property; '%s' is not a delegate."), *NameOfDelegateFunction);
			}
		}

		// successfully found the delegate function that this delegate property corresponds to

		// save this into the delegate cache for faster lookup later
		DelegateCache.Add(DelegatePropertyBase.DelegateName, SourceDelegateFunctionDef);

		// bind it to the delegate property
		if (PropertyBase.Type == CPT_Delegate)
		{
			if( !SourceDelegateFunctionDef->HasAnyFunctionFlags( FUNC_MulticastDelegate ) )
			{
				PropertyDef->SetDelegateFunctionSignature(*SourceDelegateFunctionDef);
			}
			else
			{
				Throwf(TEXT("Unable to declare a single-cast delegate property for a multi-cast delegate type '%s'.  Either add a 'multicast' qualifier to the property or change the delegate type to be single-cast as well."), *SourceDelegateFunctionDef->GetName());
			}
		}
		else if (PropertyBase.Type == CPT_MulticastDelegate)
		{
			if (SourceDelegateFunctionDef->HasAnyFunctionFlags( FUNC_MulticastDelegate ))
			{
				PropertyDef->SetDelegateFunctionSignature(*SourceDelegateFunctionDef);
			}
			else
			{
				Throwf(TEXT("Unable to declare a multi-cast delegate property for a single-cast delegate type '%s'.  Either remove the 'multicast' qualifier from the property or change the delegate type to be 'multicast' as well."), *SourceDelegateFunctionDef->GetName());
			}
		}
	}

	// Functions might have their own delegate properties which need to be validated
	for (TSharedRef<FUnrealFunctionDefinitionInfo> Function : StructDef.GetFunctions())
	{
		FixupDelegateProperties(*Function, Scope, DelegateCache);
	}

	CheckDocumentationPolicyForStruct(StructDef);

	ParseRigVMMethodParameters(StructDef);
}

void FHeaderParser::CheckSparseClassData(const FUnrealStructDefinitionInfo& StructDef)
{
	// we're looking for classes that have sparse class data structures
	const FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(StructDef);

	// make sure we don't try to have sparse class data inside of a struct instead of a class
	if (StructDef.HasMetaData(FHeaderParserNames::NAME_SparseClassDataTypes))
	{
		if (ClassDef == nullptr)
		{
			StructDef.Throwf(TEXT("%s contains sparse class data but is not a class."), *StructDef.GetName());
		}
	}
	else
	{
		return;
	}

	TArray<FString> SparseClassDataTypes;
	ClassDef->GetSparseClassDataTypes(SparseClassDataTypes);

	// for now we only support one sparse class data structure per class
	if (SparseClassDataTypes.Num() > 1)
	{
		StructDef.Throwf(TEXT("Class %s contains multiple sparse class data types."), *ClassDef->GetName());
		return;
	}
	if (SparseClassDataTypes.Num() == 0)
	{
		StructDef.Throwf(TEXT("Class %s has sparse class metadata but does not specify a type."), *ClassDef->GetName());
		return;
	}

	for (const FString& SparseClassDataTypeName : SparseClassDataTypes)
	{
		FUnrealScriptStructDefinitionInfo* SparseClassDataStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*SparseClassDataTypeName);

		// make sure the sparse class data struct actually exists
		if (!SparseClassDataStructDef)
		{
			StructDef.Throwf(TEXT("Unable to find sparse data type %s for class %s."), *SparseClassDataTypeName, *ClassDef->GetName());
			return;
		}

		// check the data struct for invalid properties
		for (FUnrealPropertyDefinitionInfo* PropertyDef : TUHTFieldRange<FUnrealPropertyDefinitionInfo>(*SparseClassDataStructDef))
		{
			if (PropertyDef->HasAnyPropertyFlags(CPF_BlueprintAssignable))
			{
				StructDef.Throwf(TEXT("Sparse class data types can not contain blueprint assignable delegates. Type '%s' Delegate '%s'"), *SparseClassDataStructDef->GetName(), *PropertyDef->GetName());
			}

			// all sparse properties should have EditDefaultsOnly
			if (!PropertyDef->HasAllPropertyFlags(CPF_Edit | CPF_DisableEditOnInstance))
			{
				StructDef.Throwf(TEXT("Sparse class data types must be VisibleDefaultsOnly or EditDefaultsOnly. Type '%s' Property '%s'"), *SparseClassDataStructDef->GetName(), *PropertyDef->GetName());
			}

			// no sparse properties should have BlueprintReadWrite
			if (PropertyDef->HasAllPropertyFlags(CPF_BlueprintVisible) && !PropertyDef->HasAllPropertyFlags(CPF_BlueprintReadOnly))
			{
				StructDef.Throwf(TEXT("Sparse class data types must not be BlueprintReadWrite. Type '%s' Property '%s'"), *SparseClassDataStructDef->GetName(), *PropertyDef->GetName());
			}
		}

		// if the class's parent has a sparse class data struct then the current class must also use the same struct or one that inherits from it

		const FUnrealClassDefinitionInfo* ParentClassDef = ClassDef->GetSuperClass();
		TArray<FString> ParentSparseClassDataTypeNames;
		ClassDef->GetSuperClass()->GetSparseClassDataTypes(ParentSparseClassDataTypeNames);
		for (FString& ParentSparseClassDataTypeName : ParentSparseClassDataTypeNames)
		{
			if (FUnrealScriptStructDefinitionInfo* ParentSparseClassDataStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*ParentSparseClassDataTypeName))
			{
				if (!SparseClassDataStructDef->IsChildOf(*ParentSparseClassDataStructDef))
				{
					StructDef.Throwf(TEXT("Class %s is a child of %s but its sparse class data struct, %s, does not inherit from %s."), *ClassDef->GetName(), *ParentClassDef->GetName(), *SparseClassDataStructDef->GetName(), *ParentSparseClassDataStructDef->GetName());
				}
			}
		}
	}
}

void FHeaderParser::ValidateClassFlags(const FUnrealClassDefinitionInfo& ToValidate)
{
	if (ToValidate.HasAnyClassFlags(CLASS_NeedsDeferredDependencyLoading) && !ToValidate.IsChildOf(*GUClassDef))
	{
		// CLASS_NeedsDeferredDependencyLoading can only be set on classes derived from UClass
		ToValidate.Throwf(TEXT("NeedsDeferredDependencyLoading is set on %s but the flag can only be used with classes derived from UClass."), *ToValidate.GetName());
	}
}

void FHeaderParser::VerifyBlueprintPropertyGetter(FUnrealPropertyDefinitionInfo& PropertyDef, FUnrealFunctionDefinitionInfo* TargetFuncDef)
{
	check(TargetFuncDef);

	const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = TargetFuncDef->GetProperties();
	FUnrealPropertyDefinitionInfo* ReturnPropDef = TargetFuncDef->GetReturn();
	if (Properties.Num() > 1 || (Properties.Num() == 1 && ReturnPropDef == nullptr))
	{
		LogError(TEXT("Blueprint Property getter function %s must not have parameters."), *TargetFuncDef->GetName());
	}

	if (ReturnPropDef == nullptr || !PropertyDef.SameType(*ReturnPropDef))
	{
		FString ExtendedCPPType;
		FString CPPType = PropertyDef.GetCPPType(&ExtendedCPPType);
		LogError(TEXT("Blueprint Property getter function %s must have return value of type %s%s."), *TargetFuncDef->GetName(), *CPPType, *ExtendedCPPType);
	}

	if (TargetFuncDef->HasAnyFunctionFlags(FUNC_Event))
	{
		LogError(TEXT("Blueprint Property setter function cannot be a blueprint event."));
	}
	else if (!TargetFuncDef->HasAnyFunctionFlags(FUNC_BlueprintPure))
	{
		LogError(TEXT("Blueprint Property getter function must be pure."));
	}
}

void FHeaderParser::VerifyBlueprintPropertySetter(FUnrealPropertyDefinitionInfo& PropertyDef, FUnrealFunctionDefinitionInfo* TargetFuncDef)
{
	check(TargetFuncDef);
	FUnrealPropertyDefinitionInfo* ReturnPropDef = TargetFuncDef->GetReturn();

	if (ReturnPropDef)
	{
		LogError(TEXT("Blueprint Property setter function %s must not have a return value."), *TargetFuncDef->GetName());
	}
	else
	{
		const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = TargetFuncDef->GetProperties();
		if (TargetFuncDef->GetProperties().Num() != 1 || !PropertyDef.SameType(*Properties[0]))
		{
			FString ExtendedCPPType;
			FString CPPType = PropertyDef.GetCPPType(&ExtendedCPPType);
			LogError(TEXT("Blueprint Property setter function %s must have exactly one parameter of type %s%s."), *TargetFuncDef->GetName(), *CPPType, *ExtendedCPPType);
		}
	}

	if (TargetFuncDef->HasAnyFunctionFlags(FUNC_Event))
	{
		LogError(TEXT("Blueprint Property setter function cannot be a blueprint event."));
	}
	else if (!TargetFuncDef->HasAnyFunctionFlags(FUNC_BlueprintCallable))
	{
		LogError(TEXT("Blueprint Property setter function must be blueprint callable."));
	}
	else if (TargetFuncDef->HasAnyFunctionFlags(FUNC_BlueprintPure))
	{
		LogError(TEXT("Blueprint Property setter function must not be pure."));
	}
}

void FHeaderParser::VerifyRepNotifyCallback(FUnrealPropertyDefinitionInfo& PropertyDef, FUnrealFunctionDefinitionInfo* TargetFuncDef)
{
	if (TargetFuncDef)
	{
		if (TargetFuncDef->GetReturn())
		{
			LogError(TEXT("Replication notification function %s must not have return value."), *TargetFuncDef->GetName());
		}

		const bool bIsArrayProperty = PropertyDef.IsStaticArray() || PropertyDef.IsDynamicArray();
		const int32 MaxParms = bIsArrayProperty ? 2 : 1;

		const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = TargetFuncDef->GetProperties();
		if (Properties.Num() > MaxParms)
		{
			LogError(TEXT("Replication notification function %s has too many parameters."), *TargetFuncDef->GetName());
		}

		if (Properties.Num() >= 1)
		{
			const FUnrealPropertyDefinitionInfo* ParmDef = &*Properties[0];
			// First parameter is always the old value:
			if (!PropertyDef.SameType(*ParmDef))
			{
				FString ExtendedCPPType;
				FString CPPType = ParmDef->GetCPPType(&ExtendedCPPType);
				LogError(TEXT("Replication notification function %s has invalid parameter for property %s. First (optional) parameter must be of type %s%s."), *TargetFuncDef->GetName(), *ParmDef->GetName(), *CPPType, *ExtendedCPPType);
			}
		}

		if (TargetFuncDef->GetProperties().Num() >= 2)
		{
			FUnrealPropertyDefinitionInfo* ParmDef = &*Properties[1];
			FPropertyBase& ParmBase = ParmDef->GetPropertyBase();
			// A 2nd parameter for arrays can be specified as a const TArray<uint8>&. This is a list of element indices that have changed
			if (!(ParmBase.Type == CPT_Byte && ParmBase.ArrayType == EArrayType::Dynamic && ParmDef->HasAllPropertyFlags(CPF_ConstParm | CPF_ReferenceParm)))
			{
				LogError(TEXT("Replication notification function %s (optional) second parameter must be of type 'const TArray<uint8>&'"), *TargetFuncDef->GetName());
			}
		}
	}
	else
	{
		// Couldn't find a valid function...
		LogError(TEXT("Replication notification function %s not found"), *PropertyDef.GetRepNotifyFunc().ToString() );
	}
}

void FHeaderParser::VerifyGetterSetterAccessorProperties(const FUnrealClassDefinitionInfo& TargetClassDef, FUnrealPropertyDefinitionInfo& PropertyDef)
{
	FPropertyBase& PropertyToken = PropertyDef.GetPropertyBase();
	if (!PropertyToken.SetterName.IsEmpty())
	{
		if (PropertyToken.SetterName != TEXT("None") && !PropertyToken.bSetterFunctionFound)
		{
			// The function can be a UFunction (not parsed the same way as the other accessors)
			const TSharedRef<FUnrealFunctionDefinitionInfo>* FoundFunction = TargetClassDef.GetFunctions().FindByPredicate([&PropertyToken](const TSharedRef<FUnrealFunctionDefinitionInfo>& Other) { return Other->GetName() == PropertyToken.SetterName; });
			if (FoundFunction)
			{
				const FUnrealFunctionDefinitionInfo& Function = FoundFunction->Get();
				if (!Function.HasAllFunctionFlags(FUNC_Native))
				{
					LogError(TEXT("Property %s setter function %s has to be native."), *PropertyDef.GetName(), *PropertyToken.SetterName);
				}
				if (Function.HasAnyFunctionFlags(FUNC_Net | FUNC_Event))
				{
					LogError(TEXT("Property %s setter function %s cannot be a net or an event"), *PropertyDef.GetName(), *PropertyToken.SetterName);
				}
				else if (Function.HasAllFunctionFlags(FUNC_EditorOnly) != PropertyDef.HasAllPropertyFlags(CPF_EditorOnly))
				{
					LogError(TEXT("Property %s setter function %s must both be EditorOnly or both not EditorOnly"), *PropertyDef.GetName(), *PropertyToken.SetterName);
				}
				else if (Function.GetProperties().Num() != 1)
				{
					LogError(TEXT("Property %s setter function %s must have a single argument"), *PropertyDef.GetName(), *PropertyToken.SetterName);
				}
				else
				{
					TSharedPtr<FUnrealPropertyDefinitionInfo> FoundArgument;
					for (const TSharedRef<FUnrealPropertyDefinitionInfo>& Arguments : Function.GetProperties())
					{
						if (Arguments->HasAllPropertyFlags(CPF_Parm) && !Arguments->HasAnyPropertyFlags(CPF_ReturnParm))
						{
							FoundArgument = Arguments;
							break;
						}
					}
					if (!FoundArgument.IsValid())
					{
						LogError(TEXT("Property %s setter function %s does not have a valid argument."), *PropertyDef.GetName(), *PropertyToken.SetterName);
					}
					else if (!FoundArgument->SameType(PropertyDef))
					{
						LogError(TEXT("Property %s setter function %s argument is not the same type."), *PropertyDef.GetName(), *PropertyToken.SetterName);
					}
					else
					{
						PropertyToken.bSetterFunctionFound = true;
					}
				}
			}
			if (!PropertyToken.bSetterFunctionFound)
			{
				LogError(TEXT("Property %s setter function %s not found"), *PropertyDef.GetName(), *PropertyToken.SetterName);
			}
		}
	}
	if (!PropertyToken.GetterName.IsEmpty())
	{
		if (PropertyToken.GetterName != TEXT("None") && !PropertyToken.bGetterFunctionFound)
		{
			// The function can be a UFunction (not parsed the same way as the other accessors)
			const TSharedRef<FUnrealFunctionDefinitionInfo>* FoundFunction = TargetClassDef.GetFunctions().FindByPredicate([&PropertyToken](const TSharedRef<FUnrealFunctionDefinitionInfo>& Other) { return Other->GetName() == PropertyToken.GetterName; });
			if (FoundFunction)
			{
				const FUnrealFunctionDefinitionInfo& Function = FoundFunction->Get();
				if (!Function.HasAllFunctionFlags(FUNC_Native))
				{
					LogError(TEXT("Property %s getter function %s has to be native."), *PropertyDef.GetName(), *PropertyToken.GetterName);
				}
				if (!Function.HasAllFunctionFlags(FUNC_Const))
				{
					LogError(TEXT("Property %s getter function %s has to be const."), *PropertyDef.GetName(), *PropertyToken.GetterName);
				}
				if (Function.HasAnyFunctionFlags(FUNC_Net | FUNC_Event))
				{
					LogError(TEXT("Property %s getter function %s cannot be a net or an event"), *PropertyDef.GetName(), *PropertyToken.GetterName);
				}
				else if (Function.HasAllFunctionFlags(FUNC_EditorOnly) != PropertyDef.HasAllPropertyFlags(CPF_EditorOnly))
				{
					LogError(TEXT("Property %s getter function %s must both be EditorOnly or both not EditorOnly"), *PropertyDef.GetName(), *PropertyToken.GetterName);
				}
				else if (Function.GetProperties().Num() != 1)
				{
					LogError(TEXT("Property %s getter function %s must have a single return value"), *PropertyDef.GetName(), *PropertyToken.GetterName);
				}
				else
				{
					TSharedPtr<FUnrealPropertyDefinitionInfo> FoundArgument;
					for (const TSharedRef<FUnrealPropertyDefinitionInfo>& Arguments : Function.GetProperties())
					{
						if (Arguments->HasAllPropertyFlags(CPF_Parm) && Arguments->HasAnyPropertyFlags(CPF_ReturnParm))
						{
							FoundArgument = Arguments;
							break;
						}
					}
					if (!FoundArgument.IsValid())
					{
						LogError(TEXT("Property %s getter function %s does not have a return value."), *PropertyDef.GetName(), *PropertyToken.GetterName);
					}
					else if (!FoundArgument->SameType(PropertyDef))
					{
						LogError(TEXT("Property %s getter function %s return value is not the same type."), *PropertyDef.GetName(), *PropertyToken.GetterName);
					}
					else
					{
						PropertyToken.bGetterFunctionFound = true;
					}
				}
			}

			if (!PropertyToken.bGetterFunctionFound)
			{
				LogError(TEXT("Property %s getter function %s not found"), *PropertyDef.GetName(), *PropertyToken.GetterName);
			}
		}
	}
}

void FHeaderParser::VerifyNotifyValueChangedProperties(FUnrealPropertyDefinitionInfo& PropertyDef)
{
	FUnrealTypeDefinitionInfo* Info = PropertyDef.GetOuter();
	if (Info == nullptr)
	{
		LogError(TEXT("FieldNofity property %s does not have a outer."), *PropertyDef.GetName());
		return;
	}
	
	FUnrealClassDefinitionInfo* ClassInfo = Info->AsClass();
	if (ClassInfo == nullptr)
	{
		LogError(TEXT("FieldNofity property are only valid as UClass member variable."));
		return;
	}
	if (ClassInfo->IsInterface())
	{
		LogError(TEXT("FieldNofity are not valid on UInterface."));
		return;
	}

	ClassInfo->MarkHasFieldNotify();
}

void FHeaderParser::VerifyNotifyValueChangedFunction(const FUnrealFunctionDefinitionInfo& TargetFuncDef)
{
	FUnrealTypeDefinitionInfo* Info = TargetFuncDef.GetOuter();
	if (Info == nullptr)
	{
		LogError(TEXT("FieldNofity function %s does not have a outer."), *TargetFuncDef.GetName());
		return;
	}

	FUnrealClassDefinitionInfo* ClassInfo = Info->AsClass();
	if (ClassInfo == nullptr)
	{
		LogError(TEXT("FieldNofity function %s are only valid as UClass member function."), *TargetFuncDef.GetName());
		return;
	}

	const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = TargetFuncDef.GetProperties();
	FUnrealPropertyDefinitionInfo* ReturnPropDef = TargetFuncDef.GetReturn();
	if (Properties.Num() > 1 || (Properties.Num() == 1 && ReturnPropDef == nullptr))
	{
		LogError(TEXT("FieldNotify function %s must not have parameters."), *TargetFuncDef.GetName());
		return;
	}
	if (ReturnPropDef == nullptr)
	{
		LogError(TEXT("FieldNotify function %s must return a value."), *TargetFuncDef.GetName());
		return;
	}
	if (TargetFuncDef.HasAnyFunctionFlags(FUNC_Event))
	{
		LogError(TEXT("FieldNotify function %s cannot be a blueprint event."), *TargetFuncDef.GetName());
		return;
	}
	if (!TargetFuncDef.HasAnyFunctionFlags(FUNC_BlueprintPure))
	{
		LogError(TEXT("FieldNotify function %s must be pure."), *TargetFuncDef.GetName());
		return;
	}

	ClassInfo->MarkHasFieldNotify();
}

void FHeaderParser::VerifyPropertyMarkups(FUnrealClassDefinitionInfo& TargetClassDef)
{
	// Iterate over all properties, looking for those flagged as CPF_RepNotify
	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : TargetClassDef.GetProperties())
	{
		auto FindTargetFunction = [&](const FName FuncName) -> FUnrealFunctionDefinitionInfo*
		{
			// Search through this class and its super classes looking for the specified callback
			for (FUnrealClassDefinitionInfo* SearchClassDef = &TargetClassDef; SearchClassDef; SearchClassDef = SearchClassDef->GetSuperClass())
			{
				// Since the function map is not valid yet, we have to iterate over the fields to look for the function
				for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : SearchClassDef->GetFunctions())
				{
					if (FunctionDef->GetFName() == FuncName)
					{
						return &*FunctionDef;
					}
				}
			}
			return nullptr;
		};

		TGuardValue<int32> GuardedInputPos(InputPos, PropertyDef->GetParsePosition());
		TGuardValue<int32> GuardedInputLine(InputLine, PropertyDef->GetLineNumber());

		if (PropertyDef->HasAnyPropertyFlags(CPF_RepNotify))
		{
			VerifyRepNotifyCallback(*PropertyDef, FindTargetFunction(PropertyDef->GetRepNotifyFunc()));
		}

		if (PropertyDef->HasAnyPropertyFlags(CPF_BlueprintVisible))
		{
			const FString& GetterFuncName = PropertyDef->GetMetaData(NAME_BlueprintGetter);
			if (!GetterFuncName.IsEmpty())
			{
				if (FUnrealFunctionDefinitionInfo* TargetFuncDef = FindTargetFunction(*GetterFuncName))
				{
					VerifyBlueprintPropertyGetter(*PropertyDef, TargetFuncDef);
				}
				else
				{
					// Couldn't find a valid function...
					LogError(TEXT("Blueprint Property getter function %s not found"), *GetterFuncName);
				}
			}

			if (!PropertyDef->HasAnyPropertyFlags(CPF_BlueprintReadOnly))
			{
				const FString& SetterFuncName = PropertyDef->GetMetaData(NAME_BlueprintSetter);
				if (!SetterFuncName.IsEmpty())
				{
					if (FUnrealFunctionDefinitionInfo* TargetFuncDef = FindTargetFunction(*SetterFuncName))
					{
						VerifyBlueprintPropertySetter(*PropertyDef, TargetFuncDef);
					}
					else
					{
						// Couldn't find a valid function...
						LogError(TEXT("Blueprint Property setter function %s not found"), *SetterFuncName);
					}
				}
			}
		}

		// Verify if native property setter and getter functions could actually be found while parsing class header
		VerifyGetterSetterAccessorProperties(TargetClassDef, *PropertyDef);

		if (PropertyDef->GetPropertyBase().bFieldNotify)
		{
			VerifyNotifyValueChangedProperties(*PropertyDef);
		}
	}
}

void FHeaderParser::VerifyFunctionsMarkups(FUnrealClassDefinitionInfo& TargetClassDef)
{
	for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : TargetClassDef.GetFunctions())
	{
		if (FunctionDef->GetFunctionData().bFieldNotify)
		{
			VerifyNotifyValueChangedFunction(*FunctionDef);
		}
	}
}


/*-----------------------------------------------------------------------------
	Compiler directives.
-----------------------------------------------------------------------------*/

//
// Process a compiler directive.
//
void FHeaderParser::CompileDirective()
{
	FToken Directive;

	int32 LineAtStartOfDirective = InputLine;
	// Define directive are skipped but they can be multiline.
	bool bDefineDirective = false;

	if (!GetIdentifier(Directive))
	{
		Throwf(TEXT("Missing compiler directive after '#'") );
	}
	else if (Directive.IsValue(TEXT("error"), ESearchCase::CaseSensitive))
	{
		Throwf(TEXT("#error directive encountered") );
	}
	else if (Directive.IsValue(TEXT("pragma"), ESearchCase::CaseSensitive))
	{
		// Ignore all pragmas
	}
	else if (Directive.IsValue(TEXT("linenumber"), ESearchCase::CaseSensitive))
	{
		FToken Number;
		if (!GetToken(Number) || !Number.IsConstInt())
		{
			Throwf(TEXT("Missing line number in line number directive"));
		}

		int32 newInputLine;
		if ( Number.GetConstInt(newInputLine) )
		{
			InputLine = newInputLine;
		}
	}
	else if (Directive.IsValue(TEXT("include"), ESearchCase::CaseSensitive))
	{
		FToken IncludeName;
		if (GetToken(IncludeName) && IncludeName.IsConstString())
		{
			FTokenString InludeNameString = IncludeName.GetTokenString();
			const FString& ExpectedHeaderName = SourceFile.GetGeneratedHeaderFilename();
			if (FCString::Stricmp(*InludeNameString, *ExpectedHeaderName) == 0)
			{
				bSpottedAutogeneratedHeaderInclude = true;
			}
		}
	}
	else if (Directive.IsValue(TEXT("if"), ESearchCase::CaseSensitive))
	{
		// Eat the ! if present
		bool bNotDefined = MatchSymbol(TEXT('!'));

		int32 TempInt;
		const bool bParsedInt = GetConstInt(TempInt);
		if (bParsedInt && (TempInt == 0 || TempInt == 1))
		{
			PushCompilerDirective(ECompilerDirective::Insignificant);
		}
		else
		{
			FToken Define;
			if (!GetIdentifier(Define))
			{
				Throwf(TEXT("Missing define name '#if'") );
			}

			if ( Define.IsValue(TEXT("WITH_EDITORONLY_DATA"), ESearchCase::CaseSensitive) )
			{
				PushCompilerDirective(ECompilerDirective::WithEditorOnlyData);
			}
			else if ( Define.IsValue(TEXT("WITH_EDITOR"), ESearchCase::CaseSensitive) )
			{
				PushCompilerDirective(ECompilerDirective::WithEditor);
			}
			else if (Define.IsValue(TEXT("WITH_HOT_RELOAD"), ESearchCase::CaseSensitive) || Define.IsValue(TEXT("WITH_HOT_RELOAD_CTORS"), ESearchCase::CaseSensitive) || Define.IsSymbol(TEXT('1'))) // @TODO: This symbol test can never work after a GetIdentifier
			{
				PushCompilerDirective(ECompilerDirective::Insignificant);
			}
			else if ( Define.IsValue(TEXT("CPP"), ESearchCase::CaseSensitive) && bNotDefined)
			{
				PushCompilerDirective(ECompilerDirective::Insignificant);
			}
			else
			{
				Throwf(TEXT("Unknown define '#if %s' in class or global scope"), *Define.GetTokenValue());
			}
		}
	}
	else if (Directive.IsValue(TEXT("endif"), ESearchCase::CaseSensitive))
	{
		PopCompilerDirective();
	}
	else if (Directive.IsValue(TEXT("define"), ESearchCase::CaseSensitive))
	{
		// Ignore the define directive (can be multiline).
		bDefineDirective = true;
	}
	else if (Directive.IsValue(TEXT("ifdef"), ESearchCase::CaseSensitive) || Directive.IsValue(TEXT("ifndef"), ESearchCase::CaseSensitive))
	{
		PushCompilerDirective(ECompilerDirective::Insignificant);
	}
	else if (Directive.IsValue(TEXT("undef"), ESearchCase::CaseSensitive) || Directive.IsValue(TEXT("else"), ESearchCase::CaseSensitive))
	{
		// Ignore. UHT can only handle #if directive
	}
	else
	{
		Throwf(TEXT("Unrecognized compiler directive %s"), *Directive.GetTokenValue() );
	}

	// Skip to end of line (or end of multiline #define).
	if (LineAtStartOfDirective == InputLine)
	{
		TCHAR LastCharacter = TEXT('\0');
		TCHAR c;
		do
		{			
			while ( !IsEOL( c=GetChar() ) )
			{
				LastCharacter = c;
			}
		} 
		// Continue until the entire multiline directive has been skipped.
		while (LastCharacter == '\\' && bDefineDirective);

		if (c == 0)
		{
			UngetChar();
		}

		// Remove any comments parsed to prevent any trailing comments from being picked up
		ClearComment();
	}
}

/*-----------------------------------------------------------------------------
	Variable declaration parser.
-----------------------------------------------------------------------------*/

void FHeaderParser::GetVarType(
	FScope*                         Scope,
	EGetVarTypeOptions				Options,
	FPropertyBase&                  VarProperty,
	EPropertyFlags                  Disallow,
	EUHTPropertyType				OuterPropertyType,
	EPropertyFlags					OuterPropertyFlags,
	EPropertyDeclarationStyle::Type PropertyDeclarationStyle,
	EVariableCategory               VariableCategory,
	FIndexRange*                    ParsedVarIndexRange,
	ELayoutMacroType*               OutLayoutMacroType
)
{
	FUnrealStructDefinitionInfo* OwnerStructDef = Scope->IsFileScope() ? nullptr : &((FStructScope*)Scope)->GetStructDef();
	FUnrealScriptStructDefinitionInfo* OwnerScriptStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(OwnerStructDef);
	FUnrealClassDefinitionInfo* OwnerClassDef = UHTCast<FUnrealClassDefinitionInfo>(OwnerStructDef);
	FName RepCallbackName = FName(NAME_None);
	FString SetterName;
	FString GetterName;
	bool bHasSetterTag = false;
	bool bHasGetterTag = false;
	bool bFieldNotify = false;

	// Get flags.
	EPropertyFlags Flags        = CPF_None;
	EPropertyFlags ImpliedFlags = CPF_None;

	// force members to be 'blueprint read only' if in a const class
	if (VariableCategory == EVariableCategory::Member)
	{
		if (OwnerClassDef)
		{
			if (OwnerClassDef->HasAnyClassFlags(CLASS_Const))
			{
				ImpliedFlags |= CPF_BlueprintReadOnly;
			}
		}
	}
	uint32 ExportFlags = PROPEXPORT_Public;

	// Build up a list of specifiers
	TArray<FPropertySpecifier> SpecifiersFound;

	TMap<FName, FString> MetaDataFromNewStyle;
	bool bNativeConst = false;
	bool bNativeConstTemplateArg = false;

	const bool bIsParamList = (VariableCategory != EVariableCategory::Member) && MatchIdentifier(TEXT("UPARAM"), ESearchCase::CaseSensitive);

	// No specifiers are allowed inside a TArray
	if (OuterPropertyType != EUHTPropertyType::DynamicArray)
	{
		// New-style UPROPERTY() syntax 
		if (PropertyDeclarationStyle == EPropertyDeclarationStyle::UPROPERTY || bIsParamList)
		{
			ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Variable"), MetaDataFromNewStyle);
		}
	}

	if (VariableCategory != EVariableCategory::Member)
	{
		// const before the variable type support (only for params)
		if (MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
		{
			Flags |= CPF_ConstParm;
			bNativeConst = true;
		}
	}

	uint32 CurrentCompilerDirective = GetCurrentCompilerDirective();
	if ((CurrentCompilerDirective & ECompilerDirective::WithEditorOnlyData) != 0)
	{
		Flags |= CPF_EditorOnly;
	}
	else if ((CurrentCompilerDirective & ECompilerDirective::WithEditor) != 0)
	{
		// Checking for this error is a bit tricky given legacy code.  
		// 1) If already wrapped in WITH_EDITORONLY_DATA (see above), then we ignore the error via the else 
		// 2) Ignore any module that is an editor module
		const FManifestModule& Module = Scope->GetFileScope()->GetSourceFile()->GetPackageDef().GetModule();
		const bool bIsEditorModule =
			Module.ModuleType == EBuildModuleType::EngineEditor ||
			Module.ModuleType == EBuildModuleType::GameEditor || 
			Module.ModuleType == EBuildModuleType::EngineUncooked || 
			Module.ModuleType == EBuildModuleType::GameUncooked;
		if (VariableCategory == EVariableCategory::Member && !bIsEditorModule)
		{
			LogError(TEXT("UProperties should not be wrapped by WITH_EDITOR, use WITH_EDITORONLY_DATA instead."));
		}
	}

	// Store the start and end positions of the parsed type
	if (ParsedVarIndexRange)
	{
		ParsedVarIndexRange->StartIndex = InputPos;
	}

	// Process the list of specifiers
	bool bSeenEditSpecifier = false;
	bool bSeenBlueprintWriteSpecifier = false;
	bool bSeenBlueprintReadOnlySpecifier = false;
	bool bSeenBlueprintGetterSpecifier = false;
	for (const FPropertySpecifier& Specifier : SpecifiersFound)
	{
		EVariableSpecifier SpecID = (EVariableSpecifier)Algo::FindSortedStringCaseInsensitive(*Specifier.Key, GVariableSpecifierStrings);
		if (VariableCategory == EVariableCategory::Member)
		{
			switch (SpecID)
			{
				case EVariableSpecifier::EditAnywhere:
				{
					if (bSeenEditSpecifier)
					{
						LogError(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier.Key);
					}
					Flags |= CPF_Edit;
					bSeenEditSpecifier = true;
				}
				break;

				case EVariableSpecifier::EditInstanceOnly:
				{
					if (bSeenEditSpecifier)
					{
						LogError(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier.Key);
					}
					Flags |= CPF_Edit | CPF_DisableEditOnTemplate;
					bSeenEditSpecifier = true;
				}
				break;

				case EVariableSpecifier::EditDefaultsOnly:
				{
					if (bSeenEditSpecifier)
					{
						LogError(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier.Key);
					}
					Flags |= CPF_Edit | CPF_DisableEditOnInstance;
					bSeenEditSpecifier = true;
				}
				break;

				case EVariableSpecifier::VisibleAnywhere:
				{
					if (bSeenEditSpecifier)
					{
						LogError(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier.Key);
					}
					Flags |= CPF_Edit | CPF_EditConst;
					bSeenEditSpecifier = true;
				}
				break;

				case EVariableSpecifier::VisibleInstanceOnly:
				{
					if (bSeenEditSpecifier)
					{
						LogError(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier.Key);
					}
					Flags |= CPF_Edit | CPF_EditConst | CPF_DisableEditOnTemplate;
					bSeenEditSpecifier = true;
				}
				break;

				case EVariableSpecifier::VisibleDefaultsOnly:
				{
					if (bSeenEditSpecifier)
					{
						LogError(TEXT("Found more than one edit/visibility specifier (%s), only one is allowed"), *Specifier.Key);
					}
					Flags |= CPF_Edit | CPF_EditConst | CPF_DisableEditOnInstance;
					bSeenEditSpecifier = true;
				}
				break;

				case EVariableSpecifier::BlueprintReadWrite:
				{
					if (bSeenBlueprintReadOnlySpecifier)
					{
						LogError(TEXT("Cannot specify a property as being both BlueprintReadOnly and BlueprintReadWrite."));
					}

					const FString* PrivateAccessMD = MetaDataFromNewStyle.Find(NAME_AllowPrivateAccess);  // FBlueprintMetadata::MD_AllowPrivateAccess
					const bool bAllowPrivateAccess = PrivateAccessMD ? (*PrivateAccessMD != TEXT("false")) : false;
					if (CurrentAccessSpecifier == ACCESS_Private && !bAllowPrivateAccess)
					{
						LogError(TEXT("BlueprintReadWrite should not be used on private members"));
					}

					if ((Flags & CPF_EditorOnly) != 0 && OwnerScriptStructDef != nullptr)
					{
						LogError(TEXT("Blueprint exposed struct members cannot be editor only"));
					}

					Flags |= CPF_BlueprintVisible;
					bSeenBlueprintWriteSpecifier = true;
				}
				break;

				case EVariableSpecifier::BlueprintSetter:
				{
					if (bSeenBlueprintReadOnlySpecifier)
					{
						LogError(TEXT("Cannot specify a property as being both BlueprintReadOnly and having a BlueprintSetter."));
					}

					if (OwnerScriptStructDef != nullptr)
					{
						LogError(TEXT("Cannot specify BlueprintSetter for a struct member."));
					}

					const FString BlueprintSetterFunction = RequireExactlyOneSpecifierValue(*this, Specifier);
					MetaDataFromNewStyle.Add(NAME_BlueprintSetter, BlueprintSetterFunction);

					Flags |= CPF_BlueprintVisible;
					bSeenBlueprintWriteSpecifier = true;
				}
				break;

				case EVariableSpecifier::BlueprintReadOnly:
				{
					if (bSeenBlueprintWriteSpecifier)
					{
						LogError(TEXT("Cannot specify both BlueprintReadOnly and BlueprintReadWrite or BlueprintSetter."), *Specifier.Key);
					}

					const FString* PrivateAccessMD = MetaDataFromNewStyle.Find(NAME_AllowPrivateAccess);  // FBlueprintMetadata::MD_AllowPrivateAccess
					const bool bAllowPrivateAccess = PrivateAccessMD ? (*PrivateAccessMD != TEXT("false")) : false;
					if (CurrentAccessSpecifier == ACCESS_Private && !bAllowPrivateAccess)
					{
						LogError(TEXT("BlueprintReadOnly should not be used on private members"));
					}

					if ((Flags & CPF_EditorOnly) != 0 && OwnerScriptStructDef != nullptr)
					{
						LogError(TEXT("Blueprint exposed struct members cannot be editor only"));
					}

					Flags        |= CPF_BlueprintVisible | CPF_BlueprintReadOnly;
					ImpliedFlags &= ~CPF_BlueprintReadOnly;
					bSeenBlueprintReadOnlySpecifier = true;
				}
				break;

				case EVariableSpecifier::BlueprintGetter:
				{
					if (OwnerScriptStructDef != nullptr)
					{
						LogError(TEXT("Cannot specify BlueprintGetter for a struct member."));
					}

					const FString BlueprintGetterFunction = RequireExactlyOneSpecifierValue(*this, Specifier);
					MetaDataFromNewStyle.Add(NAME_BlueprintGetter, BlueprintGetterFunction);

					Flags        |= CPF_BlueprintVisible;
					bSeenBlueprintGetterSpecifier = true;
				}
				break;

				case EVariableSpecifier::Config:
				{
					Flags |= CPF_Config;
				}
				break;

				case EVariableSpecifier::GlobalConfig:
				{
					Flags |= CPF_GlobalConfig | CPF_Config;
				}
				break;

				case EVariableSpecifier::Localized:
				{
					LogError(TEXT("The Localized specifier is deprecated"));
				}
				break;

				case EVariableSpecifier::Transient:
				{
					Flags |= CPF_Transient;
				}
				break;

				case EVariableSpecifier::DuplicateTransient:
				{
					Flags |= CPF_DuplicateTransient;
				}
				break;

				case EVariableSpecifier::TextExportTransient:
				{
					Flags |= CPF_TextExportTransient;
				}
				break;

				case EVariableSpecifier::NonPIETransient:
				{
					LogWarning(TEXT("NonPIETransient is deprecated - NonPIEDuplicateTransient should be used instead"));
					Flags |= CPF_NonPIEDuplicateTransient;
				}
				break;

				case EVariableSpecifier::NonPIEDuplicateTransient:
				{
					Flags |= CPF_NonPIEDuplicateTransient;
				}
				break;

				case EVariableSpecifier::Export:
				{
					Flags |= CPF_ExportObject;
				}
				break;

				case EVariableSpecifier::EditInline:
				{
					LogError(TEXT("EditInline is deprecated. Remove it, or use Instanced instead."));
				}
				break;

				case EVariableSpecifier::NoClear:
				{
					Flags |= CPF_NoClear;
				}
				break;

				case EVariableSpecifier::EditFixedSize:
				{
					Flags |= CPF_EditFixedSize;
				}
				break;

				case EVariableSpecifier::Replicated:
				case EVariableSpecifier::ReplicatedUsing:
				{
					if (OwnerScriptStructDef != nullptr)
					{
						LogError(TEXT("Struct members cannot be replicated"));
					}

					Flags |= CPF_Net;

					// See if we've specified a rep notification function
					if (SpecID == EVariableSpecifier::ReplicatedUsing)
					{
						RepCallbackName = FName(*RequireExactlyOneSpecifierValue(*this, Specifier));
						Flags |= CPF_RepNotify;
					}
				}
				break;

				case EVariableSpecifier::NotReplicated:
				{
					if (OwnerScriptStructDef == nullptr)
					{
						LogError(TEXT("Only Struct members can be marked NotReplicated"));
					}

					Flags |= CPF_RepSkip;
				}
				break;

				case EVariableSpecifier::RepRetry:
				{
					LogError(TEXT("'RepRetry' is deprecated."));
				}
				break;

				case EVariableSpecifier::Interp:
				{
					Flags |= CPF_Edit;
					Flags |= CPF_BlueprintVisible;
					Flags |= CPF_Interp;
				}
				break;

				case EVariableSpecifier::NonTransactional:
				{
					Flags |= CPF_NonTransactional;
				}
				break;

				case EVariableSpecifier::Instanced:
				{
					Flags |= CPF_PersistentInstance | CPF_ExportObject | CPF_InstancedReference;
					AddEditInlineMetaData(MetaDataFromNewStyle);
				}
				break;

				case EVariableSpecifier::BlueprintAssignable:
				{
					Flags |= CPF_BlueprintAssignable;
				}
				break;

				case EVariableSpecifier::BlueprintCallable:
				{
					Flags |= CPF_BlueprintCallable;
				}
				break;

				case EVariableSpecifier::BlueprintAuthorityOnly:
				{
					Flags |= CPF_BlueprintAuthorityOnly;
				}
				break;

				case EVariableSpecifier::AssetRegistrySearchable:
				{
					Flags |= CPF_AssetRegistrySearchable;
				}
				break;

				case EVariableSpecifier::SimpleDisplay:
				{
					Flags |= CPF_SimpleDisplay;
				}
				break;

				case EVariableSpecifier::AdvancedDisplay:
				{
					Flags |= CPF_AdvancedDisplay;
				}
				break;

				case EVariableSpecifier::SaveGame:
				{
					Flags |= CPF_SaveGame;
				}
				break;

				case EVariableSpecifier::SkipSerialization:
				{
					Flags |= CPF_SkipSerialization;
				}
				break;

				case EVariableSpecifier::Setter:
				{
					bHasSetterTag = true;
					if (Specifier.Values.Num() == 1)
					{
						SetterName = Specifier.Values[0];
					}
					else if (Specifier.Values.Num() > 1)
					{
						Throwf(TEXT("The specifier '%s' must be given exactly one value"), *Specifier.Key);
					}
					
				}
				break;

				case EVariableSpecifier::Getter:
				{
					bHasGetterTag = true;
					if (Specifier.Values.Num() == 1)
					{
						GetterName = Specifier.Values[0];
					}
					else if (Specifier.Values.Num() > 1)
					{
						Throwf(TEXT("The specifier '%s' must be given exactly one value"), *Specifier.Key);
					}
				}
				break;
				
				case EVariableSpecifier::FieldNotify:
				{
					bFieldNotify = true;
				}
				break;

				default:
				{
					LogError(TEXT("Unknown variable specifier '%s'"), *Specifier.Key);
				}
				break;
			}
		}
		else
		{
			switch (SpecID)
			{
				case EVariableSpecifier::Const:
				{
					Flags |= CPF_ConstParm;
				}
				break;

				case EVariableSpecifier::Ref:
				{
					Flags |= CPF_OutParm | CPF_ReferenceParm;
				}
				break;

				case EVariableSpecifier::NotReplicated:
				{
					if (VariableCategory == EVariableCategory::ReplicatedParameter)
					{
						VariableCategory = EVariableCategory::RegularParameter;
						Flags |= CPF_RepSkip;
					}
					else
					{
						LogError(TEXT("Only parameters in service request functions can be marked NotReplicated"));
					}
				}
				break;

				default:
				{
					LogError(TEXT("Unknown variable specifier '%s'"), *Specifier.Key);
				}
				break;
			}
		}
	}

	// If we saw a BlueprintGetter but did not see BlueprintSetter or 
	// or BlueprintReadWrite then treat as BlueprintReadOnly
	if (bSeenBlueprintGetterSpecifier && !bSeenBlueprintWriteSpecifier)
	{
		Flags |= CPF_BlueprintReadOnly;
		ImpliedFlags &= ~CPF_BlueprintReadOnly;
	}

	{
		const FString* ExposeOnSpawnStr = MetaDataFromNewStyle.Find(NAME_ExposeOnSpawn);
		const bool bExposeOnSpawn = (NULL != ExposeOnSpawnStr);
		if (bExposeOnSpawn)
		{
			if (0 != (CPF_DisableEditOnInstance & Flags))
			{
				LogWarning(TEXT("Property cannot have both 'DisableEditOnInstance' and 'ExposeOnSpawn' flags"));
			}
			if (0 == (CPF_BlueprintVisible & Flags))
			{
				LogWarning(TEXT("Property cannot have 'ExposeOnSpawn' without 'BlueprintVisible' flag."));
			}
			Flags |= CPF_ExposeOnSpawn;
		}
	}

	if (CurrentAccessSpecifier == ACCESS_Public || VariableCategory != EVariableCategory::Member)
	{
		Flags       &= ~CPF_Protected;
		ExportFlags |= PROPEXPORT_Public;
		ExportFlags &= ~(PROPEXPORT_Private|PROPEXPORT_Protected);

		Flags &= ~CPF_NativeAccessSpecifiers;
		Flags |= CPF_NativeAccessSpecifierPublic;
	}
	else if (CurrentAccessSpecifier == ACCESS_Protected)
	{
		Flags       |= CPF_Protected;
		ExportFlags |= PROPEXPORT_Protected;
		ExportFlags &= ~(PROPEXPORT_Public|PROPEXPORT_Private);

		Flags &= ~CPF_NativeAccessSpecifiers;
		Flags |= CPF_NativeAccessSpecifierProtected;
	}
	else if (CurrentAccessSpecifier == ACCESS_Private)
	{
		Flags       &= ~CPF_Protected;
		ExportFlags |= PROPEXPORT_Private;
		ExportFlags &= ~(PROPEXPORT_Public|PROPEXPORT_Protected);

		Flags &= ~CPF_NativeAccessSpecifiers;
		Flags |= CPF_NativeAccessSpecifierPrivate;
	}
	else
	{
		Throwf(TEXT("Unknown access level"));
	}

	// Swallow inline keywords
	if (VariableCategory == EVariableCategory::Return)
	{
		FToken InlineToken;
		if (!GetIdentifier(InlineToken, true))
		{
			Throwf(TEXT("%s: Missing variable type"), GetHintText(*this, VariableCategory));
		}

		if (!InlineToken.IsValue(TEXT("inline"), ESearchCase::CaseSensitive) &&
			!InlineToken.IsValue(TEXT("FORCENOINLINE"), ESearchCase::CaseSensitive) &&
			!InlineToken.ValueStartsWith(TEXT("FORCEINLINE"), ESearchCase::CaseSensitive))
		{
			UngetToken(InlineToken);
		}
	}

	// Get variable type.
	bool bUnconsumedStructKeyword = false;
	bool bUnconsumedClassKeyword  = false;
	bool bUnconsumedEnumKeyword   = false;
	bool bUnconsumedConstKeyword  = false;

	// Handle MemoryLayout.h macros
	ELayoutMacroType LayoutMacroType     = ELayoutMacroType::None;
	bool             bHasWrapperBrackets = false;
	ON_SCOPE_EXIT
	{
		if (OutLayoutMacroType)
		{
			*OutLayoutMacroType = LayoutMacroType;
			if (bHasWrapperBrackets)
			{
				RequireSymbol(TEXT(')'), GLayoutMacroNames[(int32)LayoutMacroType]);
			}
		}
	};

	if (OutLayoutMacroType)
	{
		*OutLayoutMacroType = ELayoutMacroType::None;

		FToken LayoutToken;
		if (GetToken(LayoutToken))
		{
			if (LayoutToken.IsIdentifier())
			{
				FTokenValue Name = LayoutToken.GetTokenValue();
				LayoutMacroType = (ELayoutMacroType)Algo::FindSortedStringCaseInsensitive(*Name, GLayoutMacroNames, UE_ARRAY_COUNT(GLayoutMacroNames));
				if (LayoutMacroType != ELayoutMacroType::None)
				{
					RequireSymbol(TEXT('('), GLayoutMacroNames[(int32)LayoutMacroType]);
					if (LayoutMacroType == ELayoutMacroType::ArrayEditorOnly || LayoutMacroType == ELayoutMacroType::FieldEditorOnly || LayoutMacroType == ELayoutMacroType::BitfieldEditorOnly)
					{
						Flags |= CPF_EditorOnly;
					}
					bHasWrapperBrackets = MatchSymbol(TEXT("("));
				}
				else
				{
					UngetToken(LayoutToken);
				}
			}
		}
	}

	if (MatchIdentifier(TEXT("mutable"), ESearchCase::CaseSensitive))
	{
		//@TODO: Should flag as settable from a const context, but this is at least good enough to allow use for C++ land
	}

	const int32 VarStartPos = InputPos;

	if (MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
	{
		//@TODO: UCREMOVAL: Should use this to set the new (currently non-existent) CPF_Const flag appropriately!
		bUnconsumedConstKeyword = true;
		bNativeConst = true;
	}

	if (MatchIdentifier(TEXT("struct"), ESearchCase::CaseSensitive))
	{
		bUnconsumedStructKeyword = true;
	}
	else if (MatchIdentifier(TEXT("class"), ESearchCase::CaseSensitive))
	{
		bUnconsumedClassKeyword = true;
	}
	else if (MatchIdentifier(TEXT("enum"), ESearchCase::CaseSensitive))
	{
		if (VariableCategory == EVariableCategory::Member)
		{
			Throwf(TEXT("%s: Cannot declare enum at variable declaration"), GetHintText(*this, VariableCategory));
		}

		bUnconsumedEnumKeyword = true;
	}

	//
	FToken VarType;
	if ( !GetIdentifier(VarType, true) )
	{
		Throwf(TEXT("%s: Missing variable type"), GetHintText(*this, VariableCategory));
	}

	RedirectTypeIdentifier(VarType);

	if ( VariableCategory == EVariableCategory::Return && VarType.IsValue(TEXT("void"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_None);
	}
	else if ( VarType.IsValue(TEXT("int8"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_Int8);
	}
	else if ( VarType.IsValue(TEXT("int16"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_Int16);
	}
	else if ( VarType.IsValue(TEXT("int32"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_Int);
	}
	else if ( VarType.IsValue(TEXT("int64"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_Int64);
	}
	else if ( VarType.IsValue(TEXT("uint64"), ESearchCase::CaseSensitive) && IsBitfieldProperty(LayoutMacroType) )
	{
		// 64-bit bitfield (bool) type, treat it like 8 bit type
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.IsValue(TEXT("uint32"), ESearchCase::CaseSensitive) && IsBitfieldProperty(LayoutMacroType) )
	{
		// 32-bit bitfield (bool) type, treat it like 8 bit type
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.IsValue(TEXT("uint16"), ESearchCase::CaseSensitive) && IsBitfieldProperty(LayoutMacroType) )
	{
		// 16-bit bitfield (bool) type, treat it like 8 bit type.
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.IsValue(TEXT("uint8"), ESearchCase::CaseSensitive) && IsBitfieldProperty(LayoutMacroType) )
	{
		// 8-bit bitfield (bool) type
		VarProperty = FPropertyBase(CPT_Bool8);
	}
	else if ( VarType.IsValue(TEXT("int"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_Int, EIntType::Unsized);
	}
	else if ( VarType.IsValue(TEXT("signed"), ESearchCase::CaseSensitive) )
	{
		MatchIdentifier(TEXT("int"), ESearchCase::CaseSensitive);
		VarProperty = FPropertyBase(CPT_Int, EIntType::Unsized);
	}
	else if (VarType.IsValue(TEXT("unsigned"), ESearchCase::CaseSensitive))
	{
		MatchIdentifier(TEXT("int"), ESearchCase::CaseSensitive);
		VarProperty = FPropertyBase(CPT_UInt32, EIntType::Unsized);
	}
	else if ( VarType.IsValue(TEXT("bool"), ESearchCase::CaseSensitive) )
	{
		if (IsBitfieldProperty(LayoutMacroType))
		{
			LogError(TEXT("bool bitfields are not supported."));
		}
		// C++ bool type
		VarProperty = FPropertyBase(CPT_Bool);
	}
	else if ( VarType.IsValue(TEXT("uint8"), ESearchCase::CaseSensitive) )
	{
		// Intrinsic Byte type.
		VarProperty = FPropertyBase(CPT_Byte);
	}
	else if ( VarType.IsValue(TEXT("uint16"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_UInt16);
	}
	else if ( VarType.IsValue(TEXT("uint32"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_UInt32);
	}
	else if ( VarType.IsValue(TEXT("uint64"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_UInt64);
	}
	else if ( VarType.IsValue(TEXT("float"), ESearchCase::CaseSensitive) )
	{
		// Intrinsic single precision floating point type.
		VarProperty = FPropertyBase(CPT_Float);
	}
	else if ( VarType.IsValue(TEXT("double"), ESearchCase::CaseSensitive) )
	{
		// Intrinsic double precision floating point type type.
		VarProperty = FPropertyBase(CPT_Double);
	}
	else if (VarType.IsValue(TEXT("FLargeWorldCoordinatesReal"), ESearchCase::CaseSensitive))		// LWC_TODO: Remove FLargeWorldCoordinatesReal?
	{
		if (!SourceFile.IsNoExportTypes())
		{
			Throwf(TEXT("FLargeWorldCoordinatesReal is intended for LWC support only and should not be used outside of NoExportTypes.h"));
		}
		VarProperty = FPropertyBase(CPT_FLargeWorldCoordinatesReal);
	}
	else if ( VarType.IsValue(TEXT("FName"), ESearchCase::CaseSensitive) )
	{
		// Intrinsic Name type.
		VarProperty = FPropertyBase(CPT_Name);
	}
	else if ( VarType.IsValue(TEXT("TArray"), ESearchCase::CaseSensitive) )
	{
		RequireSymbol( TEXT('<'), TEXT("'tarray'") );

		GetVarType(Scope, Options, VarProperty, Disallow, EUHTPropertyType::DynamicArray, Flags, EPropertyDeclarationStyle::None, VariableCategory);
		if (VarProperty.IsContainer())
		{
			Throwf(TEXT("Nested containers are not supported.") );
		}
		// TODO: Prevent sparse delegate types from being used in a container

		if (VarProperty.MetaData.Find(NAME_NativeConst))
		{
			bNativeConstTemplateArg = true;
		}

		VarProperty.ArrayType = EArrayType::Dynamic;

		FToken CloseTemplateToken;
		if (!GetToken(CloseTemplateToken, /*bNoConsts=*/ true, ESymbolParseOption::CloseTemplateBracket))
		{
			Throwf(TEXT("Missing token while parsing TArray."));
		}

		if (!CloseTemplateToken.IsSymbol(TEXT('>')))
		{
			// If we didn't find a comma, report it
			if (!CloseTemplateToken.IsSymbol(TEXT(',')))
			{
				Throwf(TEXT("Expected '>' but found '%s'"), *CloseTemplateToken.GetTokenValue());
			}

			// If we found a comma, read the next thing, assume it's an allocator, and report that
			FToken AllocatorToken;
			if (!GetToken(AllocatorToken, /*bNoConsts=*/ true, ESymbolParseOption::CloseTemplateBracket))
			{
				Throwf(TEXT("Unexpected end of file when parsing TArray allocator."));
			}

			if (!AllocatorToken.IsIdentifier())
			{
				Throwf(TEXT("Found '%s' - expected a '>' or ','."), *AllocatorToken.GetTokenValue());
			}

			if (AllocatorToken.IsValue(TEXT("FMemoryImageAllocator"), ESearchCase::CaseSensitive))
			{
				if (EnumHasAnyFlags(Flags, CPF_Net))
				{
					Throwf(TEXT("Replicated arrays with MemoryImageAllocators are not yet supported"));
				}

				RequireSymbol(TEXT('>'), TEXT("TArray template arguments"), ESymbolParseOption::CloseTemplateBracket);

				VarProperty.AllocatorType = EAllocatorType::MemoryImage;
			}
			else if (AllocatorToken.IsValue(TEXT("TMemoryImageAllocator"), ESearchCase::CaseSensitive))
			{
				if (EnumHasAnyFlags(Flags, CPF_Net))
				{
					Throwf(TEXT("Replicated arrays with MemoryImageAllocators are not yet supported"));
				}

				RequireSymbol(TEXT('<'), TEXT("TMemoryImageAllocator template arguments"));

				FToken SkipToken;
				for (;;)
				{
					if (!GetToken(SkipToken, /*bNoConsts=*/ false, ESymbolParseOption::CloseTemplateBracket))
					{
						Throwf(TEXT("Unexpected end of file when parsing TMemoryImageAllocator template arguments."));
					}

					if (SkipToken.IsSymbol(TEXT('>')))
					{
						RequireSymbol(TEXT('>'), TEXT("TArray template arguments"), ESymbolParseOption::CloseTemplateBracket);
						VarProperty.AllocatorType = EAllocatorType::MemoryImage;
						break;
					}
				}
			}
			else
			{
				Throwf(TEXT("Found '%s' - explicit allocators are not supported in TArray properties."), *AllocatorToken.GetTokenValue());
			}
		}
	}
	else if ( VarType.IsValue(TEXT("TMap"), ESearchCase::CaseSensitive) )
	{
		RequireSymbol( TEXT('<'), TEXT("'tmap'") );

		FPropertyBase MapKeyType(CPT_None);
		GetVarType(Scope, Options, MapKeyType, Disallow, EUHTPropertyType::Map, Flags, EPropertyDeclarationStyle::None, VariableCategory);
		if (MapKeyType.IsContainer())
		{
			Throwf(TEXT("Nested containers are not supported.") );
		}
		// TODO: Prevent sparse delegate types from being used in a container

		if (MapKeyType.Type == CPT_Interface)
		{
			Throwf(TEXT("UINTERFACEs are not currently supported as key types."));
		}

		if (MapKeyType.Type == CPT_Text)
		{
			Throwf(TEXT("FText is not currently supported as a key type."));
		}
		
		if (EnumHasAnyFlags(Flags, CPF_Net))
		{
			LogError(TEXT("Replicated maps are not supported."));
		}

		FToken CommaToken;
		if (!GetToken(CommaToken, /*bNoConsts=*/ true) || !CommaToken.IsSymbol(TEXT(',')))
		{
			Throwf(TEXT("Missing value type while parsing TMap."));
		}

		GetVarType(Scope, Options, VarProperty, Disallow, EUHTPropertyType::Map, Flags, EPropertyDeclarationStyle::None, VariableCategory);
		if (VarProperty.IsContainer())
		{
			Throwf(TEXT("Nested containers are not supported.") );
		}
		// TODO: Prevent sparse delegate types from being used in a container
		MapKeyType.PropertyFlags = (MapKeyType.PropertyFlags & CPF_UObjectWrapper); // Make sure the 'UObjectWrapper' flag is maintained so that 'TMap<TSubclassOf<...>, ...>' works
		VarProperty.MapKeyProp = MakeShared<FPropertyBase>(MoveTemp(MapKeyType));
		VarProperty.MapKeyProp->DisallowFlags = ~(CPF_ContainsInstancedReference | CPF_InstancedReference | CPF_UObjectWrapper);

		FToken CloseTemplateToken;
		if (!GetToken(CloseTemplateToken, /*bNoConsts=*/ true, ESymbolParseOption::CloseTemplateBracket))
		{
			Throwf(TEXT("Missing token while parsing TMap."));
		}

		if (!CloseTemplateToken.IsSymbol(TEXT('>')))
		{
			// If we didn't find a comma, report it
			if (!CloseTemplateToken.IsSymbol(TEXT(',')))
			{
				Throwf(TEXT("Expected '>' but found '%s'"), *CloseTemplateToken.GetTokenValue());
			}

			// If we found a comma, read the next thing, assume it's an allocator, and report that
			FToken AllocatorToken;
			if (!GetToken(AllocatorToken, /*bNoConsts=*/ true, ESymbolParseOption::CloseTemplateBracket))
			{
				Throwf(TEXT("Unexpected end of file when parsing TArray allocator."));
			}

			if (!AllocatorToken.IsIdentifier())
			{
				Throwf(TEXT("Found '%s' - expected a '>' or ','."), *AllocatorToken.GetTokenValue());
			}

			if (AllocatorToken.IsIdentifier(TEXT("FMemoryImageSetAllocator"), ESearchCase::CaseSensitive))
			{
				RequireSymbol(TEXT('>'), TEXT("TMap template arguments"), ESymbolParseOption::CloseTemplateBracket);

				VarProperty.AllocatorType = EAllocatorType::MemoryImage;
			}
			else
			{
				Throwf(TEXT("Found '%s' - explicit allocators are not supported in TMap properties."), *AllocatorToken.GetTokenValue());
			}
		}
	}
	else if ( VarType.IsValue(TEXT("TSet"), ESearchCase::CaseSensitive) )
	{
		RequireSymbol( TEXT('<'), TEXT("'tset'") );

		GetVarType(Scope, Options, VarProperty, Disallow, EUHTPropertyType::Set, Flags, EPropertyDeclarationStyle::None, VariableCategory);
		if (VarProperty.IsContainer())
		{
			Throwf(TEXT("Nested containers are not supported.") );
		}
		// TODO: Prevent sparse delegate types from being used in a container

		if (VarProperty.Type == CPT_Interface)
		{
			Throwf(TEXT("UINTERFACEs are not currently supported as element types."));
		}

		if (VarProperty.Type == CPT_Text)
		{
			Throwf(TEXT("FText is not currently supported as an element type."));
		}

		if (EnumHasAnyFlags(Flags, CPF_Net))
		{
			LogError(TEXT("Replicated sets are not supported."));
		}

		VarProperty.ArrayType = EArrayType::Set;

		FToken CloseTemplateToken;
		if (!GetToken(CloseTemplateToken, /*bNoConsts=*/ true, ESymbolParseOption::CloseTemplateBracket))
		{
			Throwf(TEXT("Missing token while parsing TArray."));
		}

		if (!CloseTemplateToken.IsSymbol(TEXT('>')))
		{
			// If we didn't find a comma, report it
			if (!CloseTemplateToken.IsSymbol(TEXT(',')))
			{
				Throwf(TEXT("Expected '>' but found '%s'"), *CloseTemplateToken.GetTokenValue());
			}

			// If we found a comma, read the next thing, assume it's a keyfuncs, and report that
			FToken AllocatorToken;
			if (!GetToken(AllocatorToken, /*bNoConsts=*/ true, ESymbolParseOption::CloseTemplateBracket))
			{
				Throwf(TEXT("Expected '>' but found '%s'"), *CloseTemplateToken.GetTokenValue());
			}

			Throwf(TEXT("Found '%s' - explicit KeyFuncs are not supported in TSet properties."), *AllocatorToken.GetTokenValue());
		}
	}
	else if ( VarType.IsValue(TEXT("FString"), ESearchCase::CaseSensitive) || VarType.IsValue(TEXT("FMemoryImageString"), ESearchCase::CaseSensitive))
	{
		VarProperty = FPropertyBase(CPT_String);

		if (VariableCategory != EVariableCategory::Member)
		{
			if (MatchSymbol(TEXT('&')))
			{
				if (Flags & CPF_ConstParm)
				{
					// 'const FString& Foo' came from 'FString' in .uc, no flags
					Flags &= ~CPF_ConstParm;

					// We record here that we encountered a const reference, because we need to remove that information from flags for code generation purposes.
					VarProperty.RefQualifier = ERefQualifier::ConstRef;
				}
				else
				{
					// 'FString& Foo' came from 'out FString' in .uc
					Flags |= CPF_OutParm;

					// And we record here that we encountered a non-const reference here too.
					VarProperty.RefQualifier = ERefQualifier::NonConstRef;
				}
			}
		}
	}
	else if ( VarType.IsValue(TEXT("Text"), ESearchCase::IgnoreCase) )
	{
		Throwf(TEXT("%s' is missing a prefix, expecting 'FText'"), *VarType.GetTokenValue());
	}
	else if ( VarType.IsValue(TEXT("FText"), ESearchCase::CaseSensitive) )
	{
		VarProperty = FPropertyBase(CPT_Text);
	}
	else if (VarType.IsValue(TEXT("TEnumAsByte"), ESearchCase::CaseSensitive))
	{
		RequireSymbol(TEXT('<'), VarType.Value);

		// Eat the forward declaration enum text if present
		MatchIdentifier(TEXT("enum"), ESearchCase::CaseSensitive);

		bool bFoundEnum = false;

		FToken InnerEnumType;
		if (GetIdentifier(InnerEnumType, true))
		{
			if (FUnrealEnumDefinitionInfo* EnumDef = GTypeDefinitionInfoMap.FindByName<FUnrealEnumDefinitionInfo>(*InnerEnumType.GetTokenValue()))
			{
				// In-scope enumeration.
				VarProperty = FPropertyBase(*EnumDef, CPT_Byte);
				bFoundEnum  = true;
			}
		}

		// Try to handle namespaced enums
		// Note: We do not verify the scoped part is correct, and trust in the C++ compiler to catch that sort of mistake
		if (MatchSymbol(TEXT("::")))
		{
			FToken ScopedTrueEnumName;
			if (!GetIdentifier(ScopedTrueEnumName, true))
			{
				Throwf(TEXT("Expected a namespace scoped enum name.") );
			}
		}

		if (!bFoundEnum)
		{
			Throwf(TEXT("Expected the name of a previously defined enum"));
		}

		RequireSymbol(TEXT('>'), VarType.Value, ESymbolParseOption::CloseTemplateBracket);
	}
	else if (VarType.IsValue(TEXT("TFieldPath"), ESearchCase::CaseSensitive ))
	{
		RequireSymbol( TEXT('<'), TEXT("'TFieldPath'") );

		FName FieldClassName = NAME_None;
		FToken PropertyTypeToken;
		if (!GetToken(PropertyTypeToken, /* bNoConsts = */ true))
		{
			Throwf(TEXT("Expected the property type"));
		}
		else
		{
			FieldClassName = FName(PropertyTypeToken.Value.RightChop(1), FNAME_Add);
			if (!FPropertyTraits::IsValidFieldClass(FieldClassName))
			{
				Throwf(TEXT("Undefined property type: %s"), *PropertyTypeToken.GetTokenValue());
			}
		}

		RequireSymbol(TEXT('>'), VarType.Value, ESymbolParseOption::CloseTemplateBracket);

		VarProperty = FPropertyBase(FieldClassName, CPT_FieldPath);
	}
	else if (FUnrealEnumDefinitionInfo* EnumDef = GTypeDefinitionInfoMap.FindByName<FUnrealEnumDefinitionInfo>(*VarType.GetTokenValue()))
	{
		EPropertyType UnderlyingType = CPT_Byte;

		if (VariableCategory == EVariableCategory::Member)
		{
			if (EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
			{
				Throwf(TEXT("You cannot use the raw enum name as a type for member variables, instead use TEnumAsByte or a C++11 enum class with an explicit underlying type."), *EnumDef->GetCppType());
			}
		}

		// Try to handle namespaced enums
		// Note: We do not verify the scoped part is correct, and trust in the C++ compiler to catch that sort of mistake
		if (MatchSymbol(TEXT("::")))
		{
			FToken ScopedTrueEnumName;
			if (!GetIdentifier(ScopedTrueEnumName, true))
			{
				Throwf(TEXT("Expected a namespace scoped enum name.") );
			}
		}

		// In-scope enumeration.
		VarProperty            = FPropertyBase(*EnumDef, UnderlyingType);
		bUnconsumedEnumKeyword = false;
	}
	else
	{
		// Check for structs/classes
		bool bHandledType = false;
		FString IdentifierStripped = GetClassNameWithPrefixRemoved(FString(VarType.Value));
		bool bStripped = false;
		FUnrealScriptStructDefinitionInfo* StructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*VarType.GetTokenValue());
		if (!StructDef)
		{
			StructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*IdentifierStripped);
			bStripped = true;
		}

		auto SetDelegateType = [&](FUnrealFunctionDefinitionInfo& InFunctionDef, const FString& InIdentifierStripped)
		{
			bHandledType = true;

			VarProperty = FPropertyBase(InFunctionDef.HasAnyFunctionFlags(FUNC_MulticastDelegate) ? CPT_MulticastDelegate : CPT_Delegate);
			VarProperty.DelegateName = *InIdentifierStripped;
			VarProperty.FunctionDef = &InFunctionDef;
		};

		if (!StructDef && MatchSymbol(TEXT("::")))
		{
			FToken DelegateName;
			if (GetIdentifier(DelegateName))
			{
				if (FUnrealClassDefinitionInfo* LocalOwnerClassDef = FUnrealClassDefinitionInfo::FindClass(*IdentifierStripped))
				{
					if (!BusyWait([LocalOwnerClassDef]() 
						{
							return LocalOwnerClassDef->IsParsed() || LocalOwnerClassDef->GetUnrealSourceFile().IsParsed();
						}))
					{
						Throwf(TEXT("Timeout waiting for '%s' to be parsed.  Make sure that '%s' directly references the include file or that all intermediate include files contain some type of UCLASS, USTRUCT, or UENUM."),
							*LocalOwnerClassDef->GetUnrealSourceFile().GetFilename(), *SourceFile.GetFilename());
					}
					const FString DelegateIdentifierStripped = GetClassNameWithPrefixRemoved(FString(DelegateName.Value));
					if (FUnrealFieldDefinitionInfo* FoundDef = LocalOwnerClassDef->GetScope()->FindTypeByName(*(DelegateIdentifierStripped + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX)))
					{
						if (FUnrealFunctionDefinitionInfo* DelegateFuncDef = FoundDef->AsFunction())
						{
							SetDelegateType(*DelegateFuncDef, DelegateIdentifierStripped);
							VarProperty.DelegateSignatureOwnerClassDef = LocalOwnerClassDef;
						}
					}
				}
				else
				{
					Throwf(TEXT("Cannot find class '%s', to resolve delegate '%s'"), *IdentifierStripped, *DelegateName.GetTokenValue());
				}
			}
		}

		if (bHandledType)
		{
		}
		else if (StructDef)
		{
			if (bStripped)
			{
				const TCHAR* PrefixCPP = UHTConfig.StructsWithTPrefix.Contains(IdentifierStripped) ? TEXT("T") : StructDef->GetPrefixCPP();
				FString ExpectedStructName = FString::Printf(TEXT("%s%s"), PrefixCPP, *StructDef->GetName());
				if (!VarType.IsValue(*ExpectedStructName, ESearchCase::CaseSensitive))
				{
					Throwf(TEXT("Struct '%s' is missing or has an incorrect prefix, expecting '%s'"), *VarType.GetTokenValue(), *ExpectedStructName);
				}
			}
			else if (!UHTConfig.StructsWithNoPrefix.Contains(VarType.Value))
			{
				const TCHAR* PrefixCPP = UHTConfig.StructsWithTPrefix.Contains(VarType.Value) ? TEXT("T") : StructDef->GetPrefixCPP();
				Throwf(TEXT("Struct '%s' is missing a prefix, expecting '%s'"), *VarType.GetTokenValue(), *FString::Printf(TEXT("%s%s"), PrefixCPP, *StructDef->GetName()));
			}

			bHandledType = true;

			VarProperty = FPropertyBase(*StructDef);

			// Struct keyword in front of a struct is legal, we 'consume' it
			bUnconsumedStructKeyword = false;
		}
		else if (GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*IdentifierStripped) != nullptr)
		{
			bHandledType = true;

			// Struct keyword in front of a struct is legal, we 'consume' it
			bUnconsumedStructKeyword = false;
		}
		else
		{
			FUnrealFunctionDefinitionInfo* DelegateFuncDef = UHTCast<FUnrealFunctionDefinitionInfo>(Scope->FindTypeByName(*(IdentifierStripped + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX)));

			if (DelegateFuncDef)
			{
				SetDelegateType(*DelegateFuncDef, IdentifierStripped);
			}
			else
			{
				// An object reference of some type (maybe a restricted class?)
				FUnrealClassDefinitionInfo* TempClassDef = nullptr;

				EPropertyType PropertyType = CPT_ObjectReference;
				const bool bIsSoftObjectPtrTemplate = VarType.IsValue(TEXT("TSoftObjectPtr"), ESearchCase::CaseSensitive);

				bool bWeakIsAuto = false;

				if (VarType.IsValue(TEXT("TSubclassOf"), ESearchCase::CaseSensitive))
				{
					TempClassDef = GUClassDef;
				}
				else if (VarType.IsValue(TEXT("FScriptInterface"), ESearchCase::CaseSensitive))
				{
					TempClassDef = GUInterfaceDef;
					Flags |= CPF_UObjectWrapper;
				}
				else if (VarType.IsValue(TEXT("TSoftClassPtr"), ESearchCase::CaseSensitive))
				{
					TempClassDef = GUClassDef;
					PropertyType = CPT_SoftObjectReference;
				}
				else
				{
					const bool bIsLazyPtrTemplate = VarType.IsValue(TEXT("TLazyObjectPtr"), ESearchCase::CaseSensitive);
					const bool bIsObjectPtrTemplate = VarType.IsValue(TEXT("TObjectPtr"), ESearchCase::CaseSensitive);
					const bool bIsWeakPtrTemplate = VarType.IsValue(TEXT("TWeakObjectPtr"), ESearchCase::CaseSensitive);
					const bool bIsAutoweakPtrTemplate = VarType.IsValue(TEXT("TAutoWeakObjectPtr"), ESearchCase::CaseSensitive);
					const bool bIsScriptInterfaceWrapper = VarType.IsValue(TEXT("TScriptInterface"), ESearchCase::CaseSensitive);

					if (bIsLazyPtrTemplate || bIsObjectPtrTemplate || bIsWeakPtrTemplate || bIsAutoweakPtrTemplate || bIsScriptInterfaceWrapper || bIsSoftObjectPtrTemplate)
					{
						RequireSymbol(TEXT('<'), VarType.Value);

						// Also consume const
						bNativeConstTemplateArg |= MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive);

						// Consume a forward class declaration 'class' if present
						MatchIdentifier(TEXT("class"), ESearchCase::CaseSensitive);

						// Find the lazy/weak class
						FToken InnerClass;
						if (GetIdentifier(InnerClass))
						{
							RedirectTypeIdentifier(InnerClass);

							TempClassDef = FUnrealClassDefinitionInfo::FindScriptClass(FString(InnerClass.Value));
							if (TempClassDef == nullptr)
							{
								FTokenValue InnerClassName = InnerClass.GetTokenValue();
								Throwf(TEXT("Unrecognized type '%s' (in expression %s<%s>) - type must be a UCLASS"), *InnerClassName, *VarType.GetTokenValue(), *InnerClassName);
							}

							if (bIsAutoweakPtrTemplate)
							{
								PropertyType = CPT_WeakObjectReference;
								bWeakIsAuto = true;
							}
							else if (bIsLazyPtrTemplate)
							{
								PropertyType = CPT_LazyObjectReference;
							}
							else if (bIsObjectPtrTemplate)
							{
								PropertyType = CPT_ObjectPtrReference;
							}
							else if (bIsWeakPtrTemplate)
							{
								PropertyType = CPT_WeakObjectReference;
							}
							else if (bIsSoftObjectPtrTemplate)
							{
								PropertyType = CPT_SoftObjectReference;
							}

							Flags |= CPF_UObjectWrapper;
						}
						else
						{
							Throwf(TEXT("%s: Missing template type"), *VarType.GetTokenValue());
						}

						// Const after template argument type but before end of template symbol
						bNativeConstTemplateArg |= MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive);

						RequireSymbol(TEXT('>'), VarType.Value, ESymbolParseOption::CloseTemplateBracket);
					}
					else
					{
						TempClassDef = FUnrealClassDefinitionInfo::FindScriptClass(*VarType.GetTokenValue());
					}
				}

				if (TempClassDef != NULL)
				{
					bHandledType = true;

					if ((PropertyType == CPT_WeakObjectReference) && (Disallow & CPF_AutoWeak)) // if it is not allowing anything, force it strong. this is probably a function arg
					{
						PropertyType = CPT_ObjectReference;
					}

					// if this is an interface class, we use the FInterfaceProperty class instead of FObjectProperty
					if ((PropertyType == CPT_ObjectReference) && TempClassDef->HasAnyClassFlags(CLASS_Interface))
					{
						PropertyType = CPT_Interface;
					}

					VarProperty = FPropertyBase(*TempClassDef, PropertyType, bWeakIsAuto);
					if (TempClassDef->IsChildOf(*GUClassDef))
					{
						if (MatchSymbol(TEXT('<')))
						{
							Flags |= CPF_UObjectWrapper;

							// Consume a forward class declaration 'class' if present
							MatchIdentifier(TEXT("class"), ESearchCase::CaseSensitive);

							// Get the actual class type to restrict this to
							FToken Limitor;
							if (!GetIdentifier(Limitor))
							{
								Throwf(TEXT("'class': Missing class limitor"));
							}

							RedirectTypeIdentifier(Limitor);

							VarProperty.MetaClassDef = FUnrealClassDefinitionInfo::FindScriptClassOrThrow(*this, FString(Limitor.Value));

							RequireSymbol(TEXT('>'), TEXT("'class limitor'"), ESymbolParseOption::CloseTemplateBracket);
						}
						else
						{
							VarProperty.MetaClassDef = GUObjectDef;
						}

						if (PropertyType == CPT_WeakObjectReference)
						{
							Throwf(TEXT("Class variables cannot be weak, they are always strong."));
						}
						else if (PropertyType == CPT_LazyObjectReference)
						{
							Throwf(TEXT("Class variables cannot be lazy, they are always strong."));
						}

						if (bIsSoftObjectPtrTemplate)
						{
							Throwf(TEXT("Class variables cannot be stored in TSoftObjectPtr, use TSoftClassPtr instead."));
						}
					}

					// Eat the star that indicates this is a pointer to the UObject
					if (!(Flags & CPF_UObjectWrapper))
					{
						// Const after variable type but before pointer symbol
						bNativeConst |= MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive);

						RequireSymbol(TEXT('*'), TEXT("Expected a pointer type"));

						// Optionally emit messages about native pointer members and swallow trailing 'const' after pointer properties
						if (VariableCategory == EVariableCategory::Member)
						{
							EPointerMemberBehavior PointerMemberBehavior = UHTConfig.NonEngineNativePointerMemberBehavior;
							if (bIsCurrentModulePartOfEngine)
							{
								if (PackageDef.GetModule().BaseDirectory.Contains(TEXT("/Plugins/")))
								{
									PointerMemberBehavior = UHTConfig.EnginePluginNativePointerMemberBehavior;
								}
								else
								{
									PointerMemberBehavior = UHTConfig.EngineNativePointerMemberBehavior;
								}
							}

							ConditionalLogPointerUsage(PointerMemberBehavior,
								TEXT("Native pointer"), FString(InputPos - VarStartPos, Input + VarStartPos).TrimStartAndEnd().ReplaceCharWithEscapedChar(), TEXT("TObjectPtr"));

							MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive);
						}

						VarProperty.PointerType = EPointerType::Native;
					}
					else if ((PropertyType == CPT_ObjectPtrReference) && (VariableCategory == EVariableCategory::Member))
					{
						EPointerMemberBehavior PointerMemberBehavior = UHTConfig.NonEngineObjectPtrMemberBehavior;
						if (bIsCurrentModulePartOfEngine)
						{
							if (PackageDef.GetModule().BaseDirectory.Contains(TEXT("/Plugins/")))
							{
								PointerMemberBehavior = UHTConfig.EnginePluginObjectPtrMemberBehavior;
							}
							else
							{
								PointerMemberBehavior = UHTConfig.EngineObjectPtrMemberBehavior;
							}
						}

						ConditionalLogPointerUsage(PointerMemberBehavior,
							TEXT("ObjectPtr"), FString(InputPos - VarStartPos, Input + VarStartPos).TrimStartAndEnd().ReplaceCharWithEscapedChar(), nullptr);
					}

					// Imply const if it's a parameter that is a pointer to a const class
					// NOTE: We shouldn't be automatically adding const param because in some cases with functions and blueprint native event, the 
					// generated code won't match.  For now, just disabled the auto add in that case and check for the error in the validation code.
					// Otherwise, the user is not warned and they will get compile errors.
					if (VariableCategory != EVariableCategory::Member && (TempClassDef != NULL) && (TempClassDef->HasAnyClassFlags(CLASS_Const)))
					{
						if (!EnumHasAnyFlags(Options, EGetVarTypeOptions::NoAutoConst))
						{
							Flags |= CPF_ConstParm;
						}
					}

					// Class keyword in front of a class is legal, we 'consume' it
					bUnconsumedClassKeyword = false;
					bUnconsumedConstKeyword = false;
				}
			}

			// Resolve delegates declared in another class  //@TODO: UCREMOVAL: This seems extreme
			if (!bHandledType)
			{
				FString FullName = IdentifierStripped + HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;
				bHandledType = BusyWait([&SetDelegateType, &IdentifierStripped, &FullName]()
					{
						FUnrealFunctionDefinitionInfo* DelegateDef = nullptr;
						{
							FScopeLock Lock(&GlobalDelegatesLock);
							DelegateDef = GlobalDelegates.FindRef(FullName);
						}
						if (DelegateDef)
						{
							SetDelegateType(*DelegateDef, IdentifierStripped);
							return true;
						}
						return false;
					});

				if (!bHandledType)
				{
					Throwf(TEXT("Unrecognized type '%s' - type must be a UCLASS, USTRUCT, UENUM, or global delegate."), *VarType.GetTokenValue());
				}
			}
		}
	}

	if (VariableCategory != EVariableCategory::Member)
	{
		// const after the variable type support (only for params)
		if (MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
		{
			Flags |= CPF_ConstParm;
			bNativeConst = true;
		}
	}

	if (bUnconsumedConstKeyword)
	{
		if (VariableCategory == EVariableCategory::Member)
		{
			Throwf(TEXT("Const properties are not supported."));
		}
		else
		{
			Throwf(TEXT("Inappropriate keyword 'const' on variable of type '%s'"), *VarType.GetTokenValue());
		}
	}

	if (bUnconsumedClassKeyword)
	{
		Throwf(TEXT("Inappropriate keyword 'class' on variable of type '%s'"), *VarType.GetTokenValue());
	}

	if (bUnconsumedStructKeyword)
	{
		Throwf(TEXT("Inappropriate keyword 'struct' on variable of type '%s'"), *VarType.GetTokenValue());
	}

	if (bUnconsumedEnumKeyword)
	{
		Throwf(TEXT("Inappropriate keyword 'enum' on variable of type '%s'"), *VarType.GetTokenValue());
	}

	if (MatchSymbol(TEXT('*')))
	{
		Throwf(TEXT("Inappropriate '*' on variable of type '%s', cannot have an exposed pointer to this type."), *VarType.GetTokenValue());
	}

	//@TODO: UCREMOVAL: 'const' member variables that will get written post-construction by defaultproperties
	if (VariableCategory == EVariableCategory::Member && OwnerClassDef != nullptr && OwnerClassDef->HasAnyClassFlags(CLASS_Const))
	{
		// Eat a 'not quite truthful' const after the type; autogenerated for member variables of const classes.
		bNativeConst |= MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive);
	}

	// Arrays are passed by reference but are only implicitly so; setting it explicitly could cause a problem with replicated functions
	if (MatchSymbol(TEXT('&')))
	{
		switch (VariableCategory)
		{
			case EVariableCategory::RegularParameter:
			case EVariableCategory::Return:
			{
				Flags |= CPF_OutParm;

				//@TODO: UCREMOVAL: How to determine if we have a ref param?
				if (Flags & CPF_ConstParm)
				{
					Flags |= CPF_ReferenceParm;
				}
			}
			break;

			case EVariableCategory::ReplicatedParameter:
			{
				if (!(Flags & CPF_ConstParm))
				{
					Throwf(TEXT("Replicated %s parameters cannot be passed by non-const reference"), *VarType.GetTokenValue());
				}

				Flags |= CPF_ReferenceParm;
			}
			break;

			default:
			{
			}
			break;
		}

		if (Flags & CPF_ConstParm)
		{
			VarProperty.RefQualifier = ERefQualifier::ConstRef;
		}
		else
		{
			VarProperty.RefQualifier = ERefQualifier::NonConstRef;
		}
	}

	// Set FPropertyBase info.
	VarProperty.PropertyFlags        |= Flags | ImpliedFlags;
	VarProperty.ImpliedPropertyFlags |= ImpliedFlags;
	VarProperty.PropertyExportFlags = ExportFlags;
	VarProperty.DisallowFlags = Disallow;

	// Set the RepNotify name, if the variable needs it
	if( VarProperty.PropertyFlags & CPF_RepNotify )
	{
		if( RepCallbackName != NAME_None )
		{
			VarProperty.RepNotifyName = RepCallbackName;
		}
		else
		{
			Throwf(TEXT("Must specify a valid function name for replication notifications"));
		}
	}

	VarProperty.bSetterTagFound = bHasSetterTag;
	VarProperty.SetterName = SetterName;
	VarProperty.bGetterTagFound = bHasGetterTag;
	VarProperty.GetterName = GetterName;

	VarProperty.bFieldNotify = bFieldNotify;

	// Perform some more specific validation on the property flags
	if (VarProperty.PropertyFlags & CPF_PersistentInstance)
	{
		if ((VarProperty.Type == CPT_ObjectReference) || (VarProperty.Type == CPT_ObjectPtrReference))
		{
			if (VarProperty.ClassDef->IsChildOf(*GUClassDef))
			{
				Throwf(TEXT("'Instanced' cannot be applied to class properties (UClass* or TSubclassOf<>)"));
			}
		}
		else
		{
			Throwf(TEXT("'Instanced' is only allowed on an object property, an array of objects, a set of objects, or a map with an object value type."));
		}
	}

	if ( VarProperty.IsObjectOrInterface() && VarProperty.Type != CPT_SoftObjectReference && VarProperty.MetaClassDef == nullptr && (VarProperty.PropertyFlags&CPF_Config) != 0 )
	{
		Throwf(TEXT("Not allowed to use 'config' with object variables"));
	}

	if ((VarProperty.PropertyFlags & CPF_BlueprintAssignable) && VarProperty.Type != CPT_MulticastDelegate)
	{
		Throwf(TEXT("'BlueprintAssignable' is only allowed on multicast delegate properties"));
	}

	if ((VarProperty.PropertyFlags & CPF_BlueprintCallable) && VarProperty.Type != CPT_MulticastDelegate)
	{
		Throwf(TEXT("'BlueprintCallable' is only allowed on a property when it is a multicast delegate"));
	}

	if ((VarProperty.PropertyFlags & CPF_BlueprintAuthorityOnly) && VarProperty.Type != CPT_MulticastDelegate)
	{
		Throwf(TEXT("'BlueprintAuthorityOnly' is only allowed on a property when it is a multicast delegate"));
	}
	
	if (VariableCategory != EVariableCategory::Member)
	{
		// These conditions are checked externally for struct/member variables where the flag can be inferred later on from the variable name itself
		ValidatePropertyIsDeprecatedIfNecessary(EnumHasAnyFlags(Options, EGetVarTypeOptions::OuterTypeDeprecated), VariableCategory, VarProperty, OuterPropertyType, OuterPropertyFlags);
	}

	// Check for invalid transients
	EPropertyFlags Transients = VarProperty.PropertyFlags & (CPF_DuplicateTransient | CPF_TextExportTransient | CPF_NonPIEDuplicateTransient);
	if (Transients && OwnerClassDef == nullptr)
	{
		TArray<const TCHAR*> FlagStrs = ParsePropertyFlags(Transients);
		Throwf(TEXT("'%s' specifier(s) are only allowed on class member variables"), *FString::Join(FlagStrs, TEXT(", ")));
	}

	// Make sure the overrides are allowed here.
	if( VarProperty.PropertyFlags & Disallow )
	{
		Throwf(TEXT("Specified type modifiers not allowed here") );
	}

	// For now, copy the flags that a TMap value has to the key
	if (FPropertyBase* KeyProp = VarProperty.MapKeyProp.Get())
	{
		// Make sure the 'UObjectWrapper' flag is maintained so that both 'TMap<TSubclassOf<...>, ...>' and 'TMap<UClass*, TSubclassOf<...>>' works correctly
		KeyProp->PropertyFlags = (VarProperty.PropertyFlags & ~CPF_UObjectWrapper) | (KeyProp->PropertyFlags & CPF_UObjectWrapper);
	}

	VarProperty.MetaData = MetaDataFromNewStyle;
	if (bNativeConst)
	{
		VarProperty.MetaData.Add(NAME_NativeConst, FString());
	}
	if (bNativeConstTemplateArg)
	{
		VarProperty.MetaData.Add(NAME_NativeConstTemplateArg, FString());
	}
	
	if (ParsedVarIndexRange)
	{
		ParsedVarIndexRange->Count = InputPos - ParsedVarIndexRange->StartIndex;
	}

	// Setup additional property as well as script struct def flags
	// for structs / properties being used for the RigVM.
	// The Input / Output / Constant metadata tokens can be used to mark
	// up an input / output pin of a RigVMNode. To allow authoring of those
	// nodes we'll mark up the property as accessible in Blueprint / Python
	// as well as make the struct a blueprint type.
	if(OwnerScriptStructDef)
	{
		const EPropertyFlags OriginalFlags = VarProperty.PropertyFlags; 
		if(VarProperty.MetaData.Contains(NAME_ConstantText))
		{
			VarProperty.PropertyFlags |= CPF_Edit | CPF_EditConst | CPF_BlueprintVisible;
		}
		if(VarProperty.MetaData.Contains(NAME_InputText) ||
			VarProperty.MetaData.Contains(NAME_VisibleText))
		{
			VarProperty.PropertyFlags |= CPF_Edit | CPF_BlueprintVisible;
		}
		if(VarProperty.MetaData.Contains(NAME_OutputText))
		{
			if((VarProperty.PropertyFlags & CPF_BlueprintVisible) == 0)
			{
				VarProperty.PropertyFlags |= CPF_BlueprintVisible | CPF_BlueprintReadOnly;
			}
		}
		
		if(OriginalFlags != VarProperty.PropertyFlags &&
			(((VarProperty.PropertyFlags & CPF_BlueprintVisible) != 0) ||
			((VarProperty.PropertyFlags & CPF_BlueprintReadOnly) != 0)))
		{
			if(!OwnerScriptStructDef->GetBoolMetaDataHierarchical(FHeaderParserNames::NAME_BlueprintType))
			{
				OwnerScriptStructDef->SetMetaData(FHeaderParserNames::NAME_BlueprintType, TEXT("true"));
			}
			
			if(!VarProperty.MetaData.Contains(NAME_Category))
			{
				static constexpr TCHAR PinsCategory[] = TEXT("Pins");
				VarProperty.MetaData.Add(NAME_Category, PinsCategory);
			}
		}
	}
}

FUnrealPropertyDefinitionInfo& FHeaderParser::GetVarNameAndDim
(
	FUnrealStructDefinitionInfo& ParentStruct,
	FPropertyBase& VarProperty,
	EVariableCategory VariableCategory,
	ELayoutMacroType        LayoutMacroType
)
{
	const TCHAR* HintText = GetHintText(*this, VariableCategory);

	AddModuleRelativePathToMetadata(ParentStruct, VarProperty.MetaData);
	FStringView Identifier;

	// Get variable name.
	if (VariableCategory == EVariableCategory::Return)
	{
		// Hard-coded variable name, such as with return value.
		Identifier = FStringView(TEXT("ReturnValue"));
	}
	else
	{
		FToken VarToken;
		if (!GetIdentifier(VarToken))
		{
			Throwf(TEXT("Missing variable name") );
		}

		switch (LayoutMacroType)
		{
			case ELayoutMacroType::Array:
			case ELayoutMacroType::ArrayEditorOnly:
			case ELayoutMacroType::Bitfield:
			case ELayoutMacroType::BitfieldEditorOnly:
			case ELayoutMacroType::FieldInitialized:
				RequireSymbol(TEXT(','), GLayoutMacroNames[(int32)LayoutMacroType]);
				break;

			default:
				break;
		}

		Identifier = VarToken.Value;
	}

	// Check to see if the variable is deprecated, and if so set the flag
	{
		const int32 DeprecatedIndex = Identifier.Find(TEXT("_DEPRECATED"));
		const int32 NativizedPropertyPostfixIndex = Identifier.Find(TEXT("__pf")); //TODO: check OverrideNativeName in Meta Data, to be sure it's not a random occurrence of the "__pf" string.
		bool bIgnoreDeprecatedWord = (NativizedPropertyPostfixIndex != INDEX_NONE) && (NativizedPropertyPostfixIndex > DeprecatedIndex);
		if ((DeprecatedIndex != INDEX_NONE) && !bIgnoreDeprecatedWord)
		{
			if (DeprecatedIndex != Identifier.Len() - 11)
			{
				Throwf(TEXT("Deprecated variables must end with _DEPRECATED"));
			}

			// We allow deprecated properties in blueprints that have getters and setters assigned as they may be part of a backwards compatibility path
			const bool bBlueprintVisible = (VarProperty.PropertyFlags & CPF_BlueprintVisible) > 0;
			const bool bWarnOnGetter = bBlueprintVisible && !VarProperty.MetaData.Contains(NAME_BlueprintGetter);
			const bool bWarnOnSetter = bBlueprintVisible && !(VarProperty.PropertyFlags & CPF_BlueprintReadOnly) && !VarProperty.MetaData.Contains(NAME_BlueprintSetter);

			if (bWarnOnGetter)
			{
				LogWarning(TEXT("%s: Deprecated property '%s' should not be marked as blueprint visible without having a BlueprintGetter"), HintText, *FString(Identifier));
			}

			if (bWarnOnSetter)
			{
				LogWarning(TEXT("%s: Deprecated property '%s' should not be marked as blueprint writeable without having a BlueprintSetter"), HintText, *FString(Identifier));
			}


			// Warn if a deprecated property is visible
			if (VarProperty.PropertyFlags & (CPF_Edit | CPF_EditConst) || // Property is marked as editable
				(!bBlueprintVisible && (VarProperty.PropertyFlags & CPF_BlueprintReadOnly) && !(VarProperty.ImpliedPropertyFlags & CPF_BlueprintReadOnly)) ) // Is BPRO, but not via Implied Flags and not caught by Getter/Setter path above
			{
				LogWarning(TEXT("%s: Deprecated property '%s' should not be marked as visible or editable"), HintText, *FString(Identifier));
			}

			VarProperty.PropertyFlags |= CPF_Deprecated;
			Identifier.MidInline(0, DeprecatedIndex);
		}
	}

	// Make sure it doesn't conflict.
	FString VarName(Identifier);
	FUnrealFunctionDefinitionInfo* ParentFunction = UHTCast<FUnrealFunctionDefinitionInfo>(&ParentStruct);
	if (VariableCategory == EVariableCategory::Member || (ParentFunction != nullptr && ParentFunction->GetFunctionType() == EFunctionType::Function))
	{
		FUnrealFunctionDefinitionInfo* ExistingFunctionDef = FindFunction(ParentStruct, *VarName, true, nullptr);
		FUnrealPropertyDefinitionInfo* ExistingPropertyDef = FindProperty(ParentStruct, *VarName, true);

		if (ExistingFunctionDef != nullptr || ExistingPropertyDef != nullptr)
		{
			bool bErrorDueToShadowing = true;

			if (ExistingFunctionDef && (VariableCategory != EVariableCategory::Member))
			{
				// A function parameter with the same name as a method is allowed
				bErrorDueToShadowing = false;
			}

			//@TODO: This exception does not seem sound either, but there is enough existing code that it will need to be
			// fixed up first before the exception it is removed.
			if (ExistingPropertyDef)
			{
				const bool bExistingPropDeprecated = ExistingPropertyDef->HasAnyPropertyFlags(CPF_Deprecated);
				const bool bNewPropDeprecated = (VariableCategory == EVariableCategory::Member) && ((VarProperty.PropertyFlags & CPF_Deprecated) != 0);
				if (bNewPropDeprecated || bExistingPropDeprecated)
				{
					// if this is a property and one of them is deprecated, ignore it since it will be removed soon
					bErrorDueToShadowing = false;
				}
			}

			if (bErrorDueToShadowing)
			{
				Throwf(TEXT("%s: '%s' cannot be defined in '%s' as it is already defined in scope '%s' (shadowing is not allowed)"),
					HintText,
					*FString(Identifier),
					*ParentStruct.GetName(),
					ExistingFunctionDef ? *ExistingFunctionDef->GetOuter()->GetName() : *ExistingPropertyDef->GetFullName());
			}
		}
	}

	// Get optional dimension immediately after name.
	FTokenString Dimensions;
	if ((LayoutMacroType == ELayoutMacroType::None && MatchSymbol(TEXT('['))) || LayoutMacroType == ELayoutMacroType::Array || LayoutMacroType == ELayoutMacroType::ArrayEditorOnly)
	{
		switch (VariableCategory)
		{
			case EVariableCategory::Return:
			{
				Throwf(TEXT("Arrays aren't allowed as return types"));
			}

			case EVariableCategory::RegularParameter:
			case EVariableCategory::ReplicatedParameter:
			{
				Throwf(TEXT("Arrays aren't allowed as function parameters"));
			}
		}

		if (VarProperty.IsContainer())
		{
			Throwf(TEXT("Static arrays of containers are not allowed"));
		}

		if (VarProperty.IsBool())
		{
			Throwf(TEXT("Bool arrays are not allowed") );
		}

		if (LayoutMacroType == ELayoutMacroType::None)
		{
			// Ignore how the actual array dimensions are actually defined - we'll calculate those with the compiler anyway.
			if (!GetRawString(Dimensions, TEXT(']')))
			{
				Throwf(TEXT("%s %s: Missing ']'"), HintText, *FString(Identifier));
			}
		}
		else
		{
			// Ignore how the actual array dimensions are actually defined - we'll calculate those with the compiler anyway.
			if (!GetRawString(Dimensions, TEXT(')')))
			{
				Throwf(TEXT("%s %s: Missing ']'"), HintText, *FString(Identifier));
			}
		}

		// Only static arrays are declared with [].  Dynamic arrays use TArray<> instead.
		VarProperty.ArrayType = EArrayType::Static;

		if (LayoutMacroType == ELayoutMacroType::None)
		{
			MatchSymbol(TEXT(']'));
		}
	}

	// Try gathering metadata for member fields
	if (VariableCategory == EVariableCategory::Member)
	{
		ParseFieldMetaData(VarProperty.MetaData, *VarName);
		AddFormattedPrevCommentAsTooltipMetaData(VarProperty.MetaData);
	}
	// validate UFunction parameters
	else
	{
		// UFunctions with a smart pointer as input parameter wont compile anyway, because of missing P_GET_... macro.
		// UFunctions with a smart pointer as return type will crash when called via blueprint, because they are not supported in VM.
		// WeakPointer is supported by VM as return type (see UObject::execLetWeakObjPtr), but there is no P_GET_... macro for WeakPointer.
		if (VarProperty.Type == CPT_LazyObjectReference)
		{
			Throwf(TEXT("UFunctions cannot take a lazy pointer as a parameter."));
		}
		else if (VarProperty.Type == CPT_ObjectPtrReference)
		{
			// @TODO: OBJPTR: Investigate TObjectPtr support for UFunction parameters.
			Throwf(TEXT("UFunctions cannot take a TObjectPtr as a parameter."));
		}
	}

	// Create the property
	FName PropertyName(Identifier);
	TSharedRef<FUnrealPropertyDefinitionInfo> PropDefRef = FPropertyTraits::CreateProperty(VarProperty, ParentStruct, PropertyName, VariableCategory, CurrentAccessSpecifier, Dimensions.String, SourceFile, InputLine, InputPos);

	// Add the property to the parent
	ParentStruct.AddProperty(PropDefRef);
	return *PropDefRef;
}

/*-----------------------------------------------------------------------------
	Statement compiler.
-----------------------------------------------------------------------------*/

//
// Compile a declaration in Token. Returns 1 if compiled, 0 if not.
//
bool FHeaderParser::CompileDeclaration(TArray<FUnrealFunctionDefinitionInfo*>& DelegatesToFixup, FToken& Token)
{
	EAccessSpecifier AccessSpecifier = ParseAccessProtectionSpecifier(Token);
	if (AccessSpecifier)
	{
		if (!IsAllowedInThisNesting(ENestAllowFlags::VarDecl) && !IsAllowedInThisNesting(ENestAllowFlags::Function))
		{
			Throwf(TEXT("Access specifier %s not allowed here."), *Token.GetTokenValue());
		}
		check(TopNest->NestType == ENestType::Class || TopNest->NestType == ENestType::Interface || TopNest->NestType == ENestType::NativeInterface);
		CurrentAccessSpecifier = AccessSpecifier;
		return true;
	}

	if (Token.IsIdentifier(TEXT("class"), ESearchCase::CaseSensitive) && (TopNest->NestType == ENestType::GlobalScope))
	{
		// Make sure the previous class ended with valid nesting.
		if (bEncounteredNewStyleClass_UnmatchedBrackets)
		{
			Throwf(TEXT("Missing } at end of class"));
		}

		// Start parsing the second class
		bEncounteredNewStyleClass_UnmatchedBrackets = true;
		CurrentAccessSpecifier = ACCESS_Private;

		if (!TryParseIInterfaceClass())
		{
			bEncounteredNewStyleClass_UnmatchedBrackets = false;
			UngetToken(Token);
			return SkipDeclaration(Token);
		}
		return true;
	}

	if (Token.IsIdentifier(TEXT("GENERATED_IINTERFACE_BODY"), ESearchCase::CaseSensitive) || (Token.IsIdentifier(TEXT("GENERATED_BODY"), ESearchCase::CaseSensitive) && TopNest->NestType == ENestType::NativeInterface))
	{
		if (TopNest->NestType != ENestType::NativeInterface)
		{
			Throwf(TEXT("%s must occur inside the native interface definition"), *Token.GetTokenValue());
		}
		RequireSymbol(TEXT('('), Token.Value);
		CompileVersionDeclaration(GetCurrentClassDef());
		RequireSymbol(TEXT(')'), Token.Value);

		FUnrealClassDefinitionInfo& ClassDef = GetCurrentClassDef();
		if (ClassDef.GetParsedInterfaceState() == EParsedInterface::NotAnInterface)
		{
			FString CurrentClassName = ClassDef.GetName();
			Throwf(TEXT("Could not find the associated 'U%s' class while parsing 'I%s' - it could be missing or malformed"), *CurrentClassName, *CurrentClassName);
		}

		if (ClassDef.GetParsedInterfaceState() == EParsedInterface::ParsedIInterface)
		{
			FString CurrentClassName = ClassDef.GetName();
			Throwf(TEXT("Duplicate IInterface definition found while parsing 'I%s'"), *CurrentClassName);
		}

		check(ClassDef.GetParsedInterfaceState() == EParsedInterface::ParsedUInterface);

		ClassDef.SetParsedInterfaceState(EParsedInterface::ParsedIInterface);
		ClassDef.SetGeneratedBodyMacroAccessSpecifier(CurrentAccessSpecifier);
		ClassDef.SetInterfaceGeneratedBodyLine(InputLine);

		bClassHasGeneratedIInterfaceBody = true;

		if (Token.IsIdentifier(TEXT("GENERATED_IINTERFACE_BODY"), ESearchCase::CaseSensitive))
		{
			CurrentAccessSpecifier = ACCESS_Public;
		}

		if (Token.IsIdentifier(TEXT("GENERATED_BODY"), ESearchCase::CaseSensitive))
		{
			GetCurrentClassDef().MarkGeneratedBody();
		}
		return true;
	}

	if (Token.IsIdentifier(TEXT("GENERATED_UINTERFACE_BODY"), ESearchCase::CaseSensitive) || (Token.IsIdentifier(TEXT("GENERATED_BODY"), ESearchCase::CaseSensitive) && TopNest->NestType == ENestType::Interface))
	{
		if (TopNest->NestType != ENestType::Interface)
		{
			Throwf(TEXT("%s must occur inside the interface definition"), *Token.GetTokenValue());
		}
		RequireSymbol(TEXT('('), Token.Value);
		CompileVersionDeclaration(GetCurrentClassDef());
		RequireSymbol(TEXT(')'), Token.Value);

		FUnrealClassDefinitionInfo& ClassDef = GetCurrentClassDef();

		ClassDef.SetGeneratedBodyMacroAccessSpecifier(CurrentAccessSpecifier);
		ClassDef.SetGeneratedBodyLine(InputLine);

		bClassHasGeneratedUInterfaceBody = true;

		if (Token.IsIdentifier(TEXT("GENERATED_UINTERFACE_BODY"), ESearchCase::CaseSensitive))
		{
			CurrentAccessSpecifier = ACCESS_Public;
		}
		return true;
	}

	if (Token.IsIdentifier(TEXT("GENERATED_UCLASS_BODY"), ESearchCase::CaseSensitive) || (Token.IsIdentifier(TEXT("GENERATED_BODY"), ESearchCase::CaseSensitive) && TopNest->NestType == ENestType::Class))
	{
		if (TopNest->NestType != ENestType::Class)
		{
			Throwf(TEXT("%s must occur inside the class definition"), *Token.GetTokenValue());
		}

		FUnrealClassDefinitionInfo& ClassDef = GetCurrentClassDef();

		if (Token.IsIdentifier(TEXT("GENERATED_BODY"), ESearchCase::CaseSensitive))
		{
			GetCurrentClassDef().MarkGeneratedBody();

			ClassDef.SetGeneratedBodyMacroAccessSpecifier(CurrentAccessSpecifier);
		}
		else
		{
			CurrentAccessSpecifier = ACCESS_Public;
		}

		RequireSymbol(TEXT('('), Token.Value);
		CompileVersionDeclaration(GetCurrentClassDef());
		RequireSymbol(TEXT(')'), Token.Value);

		ClassDef.SetGeneratedBodyLine(InputLine);

		bClassHasGeneratedBody = true;
		return true;
	}

	if (Token.IsIdentifier(TEXT("UCLASS"), ESearchCase::CaseSensitive))
	{
		bHaveSeenUClass = true;
		bEncounteredNewStyleClass_UnmatchedBrackets = true;
		CompileClassDeclaration();
		return true;
	}

	if (Token.IsIdentifier(TEXT("UINTERFACE"), ESearchCase::CaseSensitive))
	{
		bHaveSeenUClass = true;
		bEncounteredNewStyleClass_UnmatchedBrackets = true;
		CompileInterfaceDeclaration();
		return true;
	}

	if (Token.IsIdentifier(TEXT("UFUNCTION"), ESearchCase::CaseSensitive))
	{
		FUnrealFunctionDefinitionInfo& FunctionDef = CompileFunctionDeclaration();
		return true;
	}

	if (Token.IsIdentifier(TEXT("UDELEGATE"), ESearchCase::CaseSensitive))
	{
		FUnrealFunctionDefinitionInfo& DelegateDef = CompileDelegateDeclaration(Token.Value, EDelegateSpecifierAction::Parse);
		DelegatesToFixup.Add(&DelegateDef);
		return true;
	}

	if (IsValidDelegateDeclaration(Token)) // Legacy delegate parsing - it didn't need a UDELEGATE
	{
		FUnrealFunctionDefinitionInfo& DelegateDef = CompileDelegateDeclaration(Token.Value);
		DelegatesToFixup.Add(&DelegateDef);
		return true;
	}

	if (Token.IsIdentifier(TEXT("UPROPERTY"), ESearchCase::CaseSensitive))
	{
		CheckAllow(TEXT("'Member variable declaration'"), ENestAllowFlags::VarDecl);
		check(TopNest->NestType == ENestType::Class);

		CompileVariableDeclaration(GetCurrentClassDef());
		return true;
	}

	if (Token.IsIdentifier(TEXT("UENUM"), ESearchCase::CaseSensitive))
	{
		// Enumeration definition.
		CompileEnum();
		return true;
	}

	if (Token.IsIdentifier(TEXT("USTRUCT"), ESearchCase::CaseSensitive))
	{
		// Struct definition.
		CompileStructDeclaration();
		return true;
	}

	if (Token.IsSymbol(TEXT('#')))
	{
		// Compiler directive.
		CompileDirective();
		return true;
	}

	if (bEncounteredNewStyleClass_UnmatchedBrackets && Token.IsSymbol(TEXT('}')))
	{
		FUnrealClassDefinitionInfo& CurrentClassDef = GetCurrentClassDef();
		CurrentClassDef.GetDefinitionRange().End = &Input[InputPos];
		MatchSemi();

		// Closing brace for class declaration
		//@TODO: This is a very loose approximation of what we really need to do
		// Instead, the whole statement-consumer loop should be in a nest
		bEncounteredNewStyleClass_UnmatchedBrackets = false;

		// Pop nesting here to allow other non UClass declarations in the header file.
		// Make sure we treat UInterface as a class and not an interface
		if (CurrentClassDef.HasAllClassFlags(CLASS_Interface) && &CurrentClassDef != GUInterfaceDef)
		{
			checkf(TopNest->NestType == ENestType::Interface || TopNest->NestType == ENestType::NativeInterface, TEXT("Unexpected end of interface block."));
			PopNest(TopNest->NestType, TEXT("'Interface'"));
			PostPopNestInterface(CurrentClassDef);

			// Ensure the UINTERFACE classes have a GENERATED_BODY declaration
			if (bHaveSeenUClass && !bClassHasGeneratedUInterfaceBody)
			{
				Throwf(TEXT("Expected a GENERATED_BODY() at the start of class"));
			}

			// Ensure the non-UINTERFACE interface classes have a GENERATED_BODY declaration
			if (!bHaveSeenUClass && !bClassHasGeneratedIInterfaceBody)
			{
				Throwf(TEXT("Expected a GENERATED_BODY() at the start of class"));
			}
		}
		else
		{
			if (&CurrentClassDef == GUInterfaceDef && TopNest->NestType == ENestType::NativeInterface)
			{
				PopNest(TopNest->NestType, TEXT("'Interface'"));
			}
			else
			{
				PopNest(ENestType::Class, TEXT("'Class'"));
			}
			PostPopNestClass(CurrentClassDef);

			// Ensure classes have a GENERATED_BODY declaration
			if (bHaveSeenUClass && !bClassHasGeneratedBody)
			{
				Throwf(TEXT("Expected a GENERATED_BODY() at the start of class"));
			}
		}

		bHaveSeenUClass                  = false;
		bClassHasGeneratedBody           = false;
		bClassHasGeneratedUInterfaceBody = false;
		bClassHasGeneratedIInterfaceBody = false;

		GetCurrentScope()->AddType(CurrentClassDef);
		return true;
	}

	if (Token.IsSymbol(TEXT(';')))
	{
		if (GetToken(Token))
		{
			Throwf(TEXT("Extra ';' before '%s'"), *Token.GetTokenValue());
		}
		else
		{
			Throwf(TEXT("Extra ';' before end of file"));
		}
	}

	// Skip anything that looks like a macro followed by no bracket that we don't know about
	if (ProbablyAnUnknownObjectLikeMacro(*this, Token))
	{
		return true;
	}

	FRecordTokens RecordTokens(*this, bEncounteredNewStyleClass_UnmatchedBrackets && IsInAClass() ? &GetCurrentClassDef() : nullptr, &Token);
	bool Result = SkipDeclaration(Token);
	if (RecordTokens.Stop())
	{
		FUnrealStructDefinitionInfo& StructDef = GetCurrentClassDef();
		const FDeclaration& Declaration = StructDef.GetDeclarations().Last();
		if (CheckForConstructor(StructDef, Declaration))
		{
		}
		else if (CheckForDestructor(StructDef, Declaration))
		{
		}
		else if (TopNest->NestType == ENestType::Class)
		{
			if (CheckForSerialize(StructDef, Declaration))
			{
			}
			else if (CheckForPropertySetterFunction(StructDef, Declaration))
			{
			}
			else if (CheckForPropertyGetterFunction(StructDef, Declaration))
			{
			}
		}
	}
	return Result;
}

bool FHeaderParser::CheckForConstructor(FUnrealStructDefinitionInfo& StructDef, const FDeclaration& Declaration)
{
	FTokenReplay Tokens(Declaration.Tokens);
	FUnrealClassDefinitionInfo& ClassDef = StructDef.AsClassChecked();

	FToken Token;
	if (!Tokens.GetToken(Token))
	{
		return false;
	}

	// Allow explicit constructors
	bool bFoundExplicit = Token.IsIdentifier(TEXT("explicit"), ESearchCase::CaseSensitive);
	if (bFoundExplicit)
	{
		Tokens.GetToken(Token);
	}

	bool bSkippedAPIToken = false;
	if (Token.Value.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		if (!bFoundExplicit)
		{
			// Explicit can come before or after an _API
			Tokens.MatchIdentifier(TEXT("explicit"), ESearchCase::CaseSensitive);
		}

		Tokens.GetToken(Token);
		bSkippedAPIToken = true;
	}

	if (!Token.IsIdentifier(*ClassDef.GetAlternateNameCPP(), ESearchCase::IgnoreCase))
	{
		return false;
	}

	Tokens.GetToken(Token);
	if (!Token.IsSymbol(TEXT('(')))
	{
		return false;
	}

	bool bOICtor = false;
	bool bVTCtor = false;

	if (!ClassDef.IsDefaultConstructorDeclared() && Tokens.MatchSymbol(TEXT(')')))
	{
		ClassDef.MarkDefaultConstructorDeclared();
	}
	else if (!ClassDef.IsObjectInitializerConstructorDeclared()
		|| !ClassDef.IsCustomVTableHelperConstructorDeclared())
	{
		bool bIsConst = false;
		bool bIsRef = false;
		int32 ParenthesesNestingLevel = 1;

		while (ParenthesesNestingLevel && Tokens.GetToken(Token))
		{
			// Template instantiation or additional parameter excludes ObjectInitializer constructor.
			if (Token.IsSymbol(TEXT(',')) || Token.IsSymbol(TEXT('<')))
			{
				bOICtor = false;
				bVTCtor = false;
				break;
			}

			if (Token.IsSymbol(TEXT('(')))
			{
				ParenthesesNestingLevel++;
				continue;
			}

			if (Token.IsSymbol(TEXT(')')))
			{
				ParenthesesNestingLevel--;
				continue;
			}

			if (Token.IsIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
			{
				bIsConst = true;
				continue;
			}

			if (Token.IsSymbol(TEXT('&')))
			{
				bIsRef = true;
				continue;
			}

			if (Token.IsIdentifier(TEXT("FObjectInitializer"), ESearchCase::CaseSensitive)
				|| Token.IsIdentifier(TEXT("FPostConstructInitializeProperties"), ESearchCase::CaseSensitive) // Deprecated, but left here, so it won't break legacy code.
				)
			{
				bOICtor = true;
			}

			if (Token.IsIdentifier(TEXT("FVTableHelper"), ESearchCase::CaseSensitive))
			{
				bVTCtor = true;
			}
		}

		// Parse until finish.
		while (ParenthesesNestingLevel && Tokens.GetToken(Token))
		{
			if (Token.IsSymbol(TEXT('(')))
			{
				ParenthesesNestingLevel++;
				continue;
			}

			if (Token.IsSymbol(TEXT(')')))
			{
				ParenthesesNestingLevel--;
				continue;
			}
		}

		if (bOICtor && bIsRef && bIsConst)
		{
			ClassDef.MarkObjectInitializerConstructorDeclared();
		}
		if (bVTCtor && bIsRef)
		{
			ClassDef.MarkCustomVTableHelperConstructorDeclared();
		}
	}

	if (!bVTCtor)
	{
		ClassDef.MarkConstructorDeclared();
	}

	return false;
}

bool FHeaderParser::CheckForDestructor(FUnrealStructDefinitionInfo& StructDef, const FDeclaration& Declaration)
{
	FUnrealClassDefinitionInfo& ClassDef = StructDef.AsClassChecked();

	if (ClassDef.IsDestructorDeclared())
	{
		return false;
	}

	FTokenReplay Tokens(Declaration.Tokens);

	FToken Token;
	if (!Tokens.GetToken(Token))
	{
		return false;
	}

	while (Token.IsIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive) || Token.Value.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);
	}

	if (!Token.IsSymbol('~'))
	{
		return false;
	}

	Tokens.GetToken(Token);

	if (!Token.IsIdentifier(*ClassDef.GetAlternateNameCPP(), ESearchCase::IgnoreCase))
	{
		return false;
	}

	ClassDef.MarkDestructorDeclared();

	return false;
}

bool FHeaderParser::CheckForSerialize(FUnrealStructDefinitionInfo& StructDef, const FDeclaration& Declaration)
{
	FTokenReplay Tokens(Declaration.Tokens);
	FUnrealClassDefinitionInfo& ClassDef = StructDef.AsClassChecked();

	FToken Token;
	if (!Tokens.GetToken(Token))
	{
		return false;
	}

	while (Token.IsIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive) || Token.Value.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);
	}

	if (!Token.IsIdentifier(TEXT("void"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	Tokens.GetToken(Token);
	if (!Token.IsIdentifier(TEXT("Serialize"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	Tokens.GetToken(Token);
	if (!Token.IsSymbol(TEXT('(')))
	{
		return false;
	}

	Tokens.GetToken(Token);

	ESerializerArchiveType ArchiveType = ESerializerArchiveType::None;
	if (Token.IsIdentifier(TEXT("FArchive"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);
		if (Token.IsSymbol(TEXT('&')))
		{
			Tokens.GetToken(Token);

			// Allow the declaration to not define a name for the archive parameter
			if (!Token.IsSymbol(TEXT(')')))
			{
				Tokens.GetToken(Token);
			}

			if (Token.IsSymbol(TEXT(')')))
			{
				ArchiveType = ESerializerArchiveType::Archive;
			}
		}
	}
	else if (Token.IsIdentifier(TEXT("FStructuredArchive"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);
		if (Token.IsSymbol(TEXT("::"), ESearchCase::CaseSensitive))
		{
			Tokens.GetToken(Token);

			if (Token.IsIdentifier(TEXT("FRecord"), ESearchCase::CaseSensitive))
			{
				Tokens.GetToken(Token);

				// Allow the declaration to not define a name for the slot parameter
				if (!Token.IsSymbol(TEXT(')')))
				{
					Tokens.GetToken(Token);
				}

				if (Token.IsSymbol(TEXT(')')))
				{
					ArchiveType = ESerializerArchiveType::StructuredArchiveRecord;
				}
			}
		}
	}
	else if (Token.IsIdentifier(TEXT("FStructuredArchiveRecord"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);

		// Allow the declaration to not define a name for the slot parameter
		if (!Token.IsSymbol(TEXT(')')))
		{
			Tokens.GetToken(Token);
		}

		if (Token.IsSymbol(TEXT(')')))
		{
			ArchiveType = ESerializerArchiveType::StructuredArchiveRecord;
		}
	}

	if (ArchiveType != ESerializerArchiveType::None)
	{
		// Found what we want!
		if (Declaration.CurrentCompilerDirective == 0 || Declaration.CurrentCompilerDirective == ECompilerDirective::WithEditorOnlyData)
		{
			FString EnclosingDefine = Declaration.CurrentCompilerDirective != 0 ? TEXT("WITH_EDITORONLY_DATA") : TEXT("");

			ClassDef.AddArchiveType(ArchiveType);
			ClassDef.SetEnclosingDefine(MoveTemp(EnclosingDefine));
		}
		else
		{
			FUHTMessage(StructDef.GetUnrealSourceFile(), Token.InputLine).Throwf(TEXT("Serialize functions must not be inside preprocessor blocks, except for WITH_EDITORONLY_DATA"));
		}
		return true;
	}
	
	return false;
}
static const TArray<FStringView> GSkipDeclarationWarningStrings =
{
	TEXT("GENERATED_BODY"),
	TEXT("GENERATED_IINTERFACE_BODY"),
	TEXT("GENERATED_UCLASS_BODY"),
	TEXT("GENERATED_UINTERFACE_BODY"),
	TEXT("GENERATED_USTRUCT_BODY"),
	// Leaving these disabled ATM since they can exist in the code without causing compile issues
	//TEXT("RIGVM_METHOD"),
	//TEXT("UCLASS"),
	//TEXT("UDELEGATE"),
	//TEXT("UENUM"),
	//TEXT("UFUNCTION"),
	//TEXT("UINTERFACE"),
	//TEXT("UPROPERTY"),
	//TEXT("USTRUCT"),
};

static bool ParseAccessorType(FString& OutAccessorType, FTokenReplay& Tokens)
{
	FToken Token;
	Tokens.GetToken(Token);
	if (!Token.IsIdentifier())
	{
		return false;
	}
	if (Token.IsIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
	{
		// skip const. We allow const in paramater and return values even if the property is not const
		Tokens.GetToken(Token);
		if (!Token.IsIdentifier())
		{
			return false;
		}
	}

	OutAccessorType = Token.Value;

	// parse enum type declared as a namespace
	Tokens.GetToken(Token);
	while (Token.IsSymbol(TEXT("::"), ESearchCase::CaseSensitive))
	{
		OutAccessorType += Token.Value;
		Tokens.GetToken(Token);
		if (!Token.IsIdentifier())
		{
			return false;
		}
		OutAccessorType += Token.Value;
		Tokens.GetToken(Token);
	}

	if (Token.IsSymbol(TEXT("<"), ESearchCase::CaseSensitive))
	{
		// template type - add everything until the matching '>' into the type string
		OutAccessorType += Token.Value;
		int32 TemplateNestCount = 1;
		while (TemplateNestCount > 0)
		{
			Tokens.GetToken(Token);
			if (Token.TokenType == ETokenType::None)
			{
				return false;
			}

			OutAccessorType += Token.Value;
			if (Token.IsSymbol(TEXT("<"), ESearchCase::CaseSensitive))
			{
				TemplateNestCount++;
			}
			else if (Token.IsSymbol(TEXT(">"), ESearchCase::CaseSensitive))
			{
				TemplateNestCount--;
			}
		}
		Tokens.GetToken(Token);
	}
	// Skip '&', 'const' and retain '*'
	do
	{
		if (Token.TokenType == ETokenType::None)
		{
			return false;
		}

		if (Token.IsIdentifier())
		{
			if (!Token.IsIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
			{
				break;
			}
			// else skip const
		}
		// Support for passing values by ref for setters
		else if (!Token.IsSymbol(TEXT("&"), ESearchCase::CaseSensitive))
		{
			OutAccessorType += Token.Value;
		}
	} while (Tokens.GetToken(Token));

	// Return the last token since it's either ')' in case of setters or the getter name which will be parsed by the caller
	Tokens.UngetToken(Token);

	return true;
}

bool FHeaderParser::CheckForPropertySetterFunction(FUnrealStructDefinitionInfo& StructDef, const FDeclaration& Declaration)
{
	FTokenReplay Tokens(Declaration.Tokens);
	FUnrealClassDefinitionInfo& ClassDef = StructDef.AsClassChecked();

	FToken Token;
	if (!Tokens.GetToken(Token))
	{
		return false;
	}

	// Skip virtual keyword or any API macros
	while (Token.IsIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive) || Token.Value.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);
	}

	// Setters can only return void
	if (!Token.IsIdentifier(TEXT("void"), ESearchCase::CaseSensitive))
	{
		return false;
	}

	FToken SetterNameToken;
	Tokens.GetToken(SetterNameToken);
	if (!SetterNameToken.IsIdentifier())
	{
		return false;
	}

	FUnrealPropertyDefinitionInfo* PropertyWithSetter = nullptr;
	FString SetterFunctionName;
	TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = ClassDef.GetProperties();
	bool bExplicitSetter = false;

	// First check if there are any properties that already specify this function name as a setter
	for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : Properties)
	{
		FPropertyBase& BaseProp = Prop->GetPropertyBase();
		if (!BaseProp.bSetterFunctionFound && !BaseProp.SetterName.IsEmpty() && BaseProp.SetterName != TEXT("None"))
		{
			if (SetterNameToken.Value.Compare(Prop->GetPropertyBase().SetterName, ESearchCase::CaseSensitive) == 0)
			{
				SetterFunctionName = Prop->GetPropertyBase().SetterName;
				PropertyWithSetter = &Prop.Get();
				bExplicitSetter = true;
				break;
			}
		}
	}

	if (!PropertyWithSetter)
	{
		return false;
	}

	Tokens.GetToken(Token);
	if (!Token.IsSymbol(TEXT('(')))
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Expected '(' when parsing property %s setter function %s"), *PropertyWithSetter->GetName(), *SetterFunctionName);
		}
		return false;
	}

	FString ParameterType;
	if (!ParseAccessorType(ParameterType, Tokens))
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Error when parsing parameter type for property %s setter function %s"), *PropertyWithSetter->GetName(), *SetterFunctionName);
		}
		return false;
	}

	// Skip parameter name
	Tokens.GetToken(Token);
	if (!Token.IsIdentifier())
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Expected parameter name when parsing property %s setter function %s"), *PropertyWithSetter->GetName(), *SetterFunctionName);
		}
		return false;
	}

	// Setter has only one parameter
	Tokens.GetToken(Token);
	if (!Token.IsSymbol(TEXT(')')))
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Expected ')' when parsing property %s setter function %s"), *PropertyWithSetter->GetName(), *SetterFunctionName);
		}
		return false;
	}

	// Parameter type and property type must match
	FString ExtendedPropertyType;
	FString PropertyType = PropertyWithSetter->GetCPPType(&ExtendedPropertyType, CPPF_Implementation | CPPF_ArgumentOrReturnValue | CPPF_NoRef);
	ExtendedPropertyType.RemoveSpacesInline();
	PropertyType += ExtendedPropertyType;
	if (PropertyWithSetter->GetArrayDimensions())
	{
		PropertyType += TEXT("*");
	}
	if (ParameterType.Compare(PropertyType, ESearchCase::CaseSensitive) != 0)
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Paramater type %s unsupported for property %s setter function %s (expected: %s)"),
				*ParameterType, *PropertyWithSetter->GetName(), *SetterFunctionName, *PropertyType);
		}
		return false;
	}

	if ((GetCurrentCompilerDirective() & ECompilerDirective::WithEditor) != 0)
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Property %s setter function %s cannot be declared within WITH_EDITOR block. Use WITH_EDITORONLY_DATA instead."),
				*PropertyWithSetter->GetName(), *SetterFunctionName);
		}
		return false;
	}

	if ((GetCurrentCompilerDirective() & ECompilerDirective::WithEditorOnlyData) != 0 && (PropertyWithSetter->GetPropertyFlags() & CPF_EditorOnly) == 0)
	{
		if (bExplicitSetter)
		{
			LogError(TEXT("Property %s is not editor-only but its setter function %s is."),
				*PropertyWithSetter->GetName(), *SetterFunctionName);
		}
		return false;
	}

	PropertyWithSetter->GetPropertyBase().SetterName = *SetterFunctionName;
	PropertyWithSetter->GetPropertyBase().bSetterFunctionFound = true;

	return true;
}

bool FHeaderParser::CheckForPropertyGetterFunction(FUnrealStructDefinitionInfo& StructDef, const FDeclaration& Declaration)
{
	FTokenReplay Tokens(Declaration.Tokens);
	FUnrealClassDefinitionInfo& ClassDef = StructDef.AsClassChecked();

	FToken Token;
	if (!Tokens.GetToken(Token))
	{
		return false;
	}

	// Skip virtual keyword and any API macros
	while (Token.IsIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive) || Token.Value.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		Tokens.GetToken(Token);
	}

	Tokens.UngetToken(Token);
	FString GetterType;
	if (!ParseAccessorType(GetterType, Tokens))
	{
		// Unable to print a meaningful error before parsing the Getter name
		return false;
	}

	FToken GetterNameToken;
	Tokens.GetToken(GetterNameToken);
	if (!GetterNameToken.IsIdentifier())
	{
		// Unable to print a meaningful error before parsing the Getter name
		return false;
	}

	FUnrealPropertyDefinitionInfo* PropertyWithGetter = nullptr;
	TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = ClassDef.GetProperties();
	FString GetterFunctionName;
	bool bExplicitGetter = false;

	// First check if there are any properties that already specify this function name as a getter
	for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : Properties)
	{
		FPropertyBase& BaseProp = Prop->GetPropertyBase();
		if (!BaseProp.bGetterFunctionFound && !BaseProp.GetterName.IsEmpty() && BaseProp.GetterName != TEXT("None"))
		{
			if (GetterNameToken.Value.Compare(Prop->GetPropertyBase().GetterName, ESearchCase::CaseSensitive) == 0)
			{
				GetterFunctionName = Prop->GetPropertyBase().GetterName;
				PropertyWithGetter = &Prop.Get();
				bExplicitGetter = true;
				break;
			}
		}
	}

	if (!PropertyWithGetter)
	{
		return false;
	}

	FString ExtendedPropertyType;
	FString PropertyType = PropertyWithGetter->GetCPPType(&ExtendedPropertyType, CPPF_Implementation | CPPF_ArgumentOrReturnValue);
	PropertyType += ExtendedPropertyType;
	if (PropertyWithGetter->GetArrayDimensions())
	{
		PropertyType += TEXT("*");
	}
	if (GetterType.Compare(PropertyType, ESearchCase::CaseSensitive) != 0)
	{
		if (bExplicitGetter)
		{
			LogError(TEXT("Paramater type %s unsupported for property %s getter function %s (expected: %s)"),
				*GetterType, *PropertyWithGetter->GetName(), *GetterFunctionName, *PropertyType);
		}
		return false;
	}

	// Getter is a function that takes no arguments
	Tokens.GetToken(Token);
	if (!Token.IsSymbol(TEXT('(')))
	{
		if (bExplicitGetter)
		{
			LogError(TEXT("Expected '(' when parsing property %s getter function %s"), *PropertyWithGetter->GetName(), *GetterFunctionName);
		}
		return false;
	}

	Tokens.GetToken(Token);
	if (!Token.IsSymbol(TEXT(')')))
	{
		if (bExplicitGetter)
		{
			LogError(TEXT("Expected ')' when parsing property %s getter function %s"), *PropertyWithGetter->GetName(), *GetterFunctionName);
		}
		return false;
	}

	// Getters should be const functions
	Tokens.GetToken(Token);
	if (!Token.IsIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
	{
		if (bExplicitGetter)
		{
			LogError(TEXT("Property %s getter function %s must be const"), *PropertyWithGetter->GetName(), *GetterFunctionName);
		}
		return false;
	}

	if ((GetCurrentCompilerDirective() & ECompilerDirective::WithEditor) != 0)
	{
		if (bExplicitGetter)
		{
			LogError(TEXT("Property %s setter function %s cannot be declared within WITH_EDITOR block. Use WITH_EDITORONLY_DATA instead."),
				*PropertyWithGetter->GetName(), *GetterFunctionName);
		}
		return false;
	}

	if ((GetCurrentCompilerDirective() & ECompilerDirective::WithEditorOnlyData) != 0 && (PropertyWithGetter->GetPropertyFlags() & CPF_EditorOnly) == 0)
	{
		if (bExplicitGetter)
		{
			LogError(TEXT("Property %s is not editor-only but its getter function %s is."),
				*PropertyWithGetter->GetName(), *GetterFunctionName);
		}
		return false;
	}

	PropertyWithGetter->GetPropertyBase().GetterName = GetterFunctionName;
	PropertyWithGetter->GetPropertyBase().bGetterFunctionFound = true;

	return true;
}


bool FHeaderParser::SkipDeclaration(FToken& Token)
{
	// Store the current value of PrevComment so it can be restored after we parsed everything.
	FString OldPrevComment(PrevComment);
	// Consume all tokens until the end of declaration/definition has been found.
	int32 NestedScopes = 0;
	// Check if this is a class/struct declaration in which case it can be followed by member variable declaration.	
	bool bPossiblyClassDeclaration = Token.IsIdentifier(TEXT("class"), ESearchCase::CaseSensitive) || Token.IsIdentifier(TEXT("struct"), ESearchCase::CaseSensitive);
	// (known) macros can end without ; or } so use () to find the end of the declaration.
	// However, we don't want to use it with DECLARE_FUNCTION, because we need it to be treated like a function.
	bool bMacroDeclaration      = ProbablyAMacro(Token.Value) && !Token.IsIdentifier(TEXT("DECLARE_FUNCTION"), ESearchCase::CaseSensitive);
	bool bEndOfDeclarationFound = false;
	bool bDefinitionFound       = false;
	TCHAR OpeningBracket = bMacroDeclaration ? TEXT('(') : TEXT('{');
	TCHAR ClosingBracket = bMacroDeclaration ? TEXT(')') : TEXT('}');
	bool bRetestCurrentToken = false;
	while (bRetestCurrentToken || GetToken(Token))
	{
		// If we find parentheses at top-level and we think it's a class declaration then it's more likely
		// to be something like: class UThing* GetThing();
		if (bPossiblyClassDeclaration && NestedScopes == 0 && Token.IsSymbol(TEXT('(')))
		{
			bPossiblyClassDeclaration = false;
		}

		bRetestCurrentToken = false;
		if (Token.IsSymbol(TEXT(';')) && NestedScopes == 0)
		{
			bEndOfDeclarationFound = true;
			break;
		}

		if (Token.IsIdentifier())
		{
			// Use a trivial prefilter to avoid doing the search on things that aren't UE keywords we care about
			if (Token.Value[0] == 'G' || Token.Value[0] == 'R' || Token.Value[0] == 'U')
			{
				if (Algo::BinarySearch(GSkipDeclarationWarningStrings, Token.Value, 
					[](const FStringView& Lhs, const FStringView& Rhs) { return Lhs.Compare(Rhs, ESearchCase::CaseSensitive) < 0; }) >= 0)
				{
					LogWarning(FString::Printf(TEXT("The identifier \'%s\' was detected in a block being skipped. Was this intentional?"), *FString(Token.Value)));
				}
			}
		}

		if (!bMacroDeclaration && Token.IsIdentifier(TEXT("PURE_VIRTUAL"), ESearchCase::CaseSensitive) && NestedScopes == 0)
		{
			OpeningBracket = TEXT('(');
			ClosingBracket = TEXT(')');
		}

		if (Token.IsSymbol(OpeningBracket))
		{
			// This is a function definition or class declaration.
			bDefinitionFound = true;
			NestedScopes++;
		}
		else if (Token.IsSymbol(ClosingBracket))
		{
			NestedScopes--;
			if (NestedScopes == 0)
			{
				// Could be a class declaration in all capitals, and not a macro
				bool bReallyEndDeclaration = true;
				if (bMacroDeclaration)
				{
					FToken PossibleBracketToken;
					GetToken(PossibleBracketToken);
					UngetToken(Token);
					GetToken(Token);

					// If Strcmp returns 0, it is probably a class, else a macro.
					bReallyEndDeclaration = !PossibleBracketToken.IsSymbol(TEXT('{'));
				}

				if (bReallyEndDeclaration)
				{
					bEndOfDeclarationFound = true;
					break;
				}
			}

			if (NestedScopes < 0)
			{
				Throwf(TEXT("Unexpected '}'. Did you miss a semi-colon?"));
			}
		}
		else if (bMacroDeclaration && NestedScopes == 0)
		{
			bMacroDeclaration = false;
			OpeningBracket = TEXT('{');
			ClosingBracket = TEXT('}');
			bRetestCurrentToken = true;
		}
	}
	if (bEndOfDeclarationFound)
	{
		// Member variable declaration after class declaration (see bPossiblyClassDeclaration).
		if (bPossiblyClassDeclaration && bDefinitionFound)
		{
			// Should syntax errors be also handled when someone declares a variable after function definition?
			// Consume the variable name.
			FToken VariableName;
			if( !GetToken(VariableName, true) )
			{
				return false;
			}
			if (!VariableName.IsIdentifier())
			{
				// Not a variable name.
				UngetToken(VariableName);
			}
			else if (!SafeMatchSymbol(TEXT(';')))
			{
				Throwf(TEXT("Unexpected '%s'. Did you miss a semi-colon?"), *VariableName.GetTokenValue());
			}
		}

		// C++ allows any number of ';' after member declaration/definition.
		while (SafeMatchSymbol(TEXT(';')));
	}

	PrevComment = OldPrevComment;
	// clear the current value for comment
	//ClearComment();

	// Successfully consumed C++ declaration unless mismatched pair of brackets has been found.
	return NestedScopes == 0 && bEndOfDeclarationFound;
}

bool FHeaderParser::SafeMatchSymbol( const TCHAR Match )
{
	// Remember the position before the next token (this can include comments before the next symbol).
	FScriptLocation LocationBeforeNextSymbol;
	InitScriptLocation(LocationBeforeNextSymbol);

	FToken Token;
	if (GetToken(Token, /*bNoConsts=*/ true))
	{
		if (Token.IsSymbol(Match))
		{
			return true;
		}
		UngetToken(Token);
	}

	// Return to the stored position.
	ReturnToLocation(LocationBeforeNextSymbol);
	return false;
}

FUnrealClassDefinitionInfo& FHeaderParser::ParseClassNameDeclaration(FString& DeclaredClassName, FString& RequiredAPIMacroIfPresent)
{
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ DeclaredClassName, /*out*/ RequiredAPIMacroIfPresent, TEXT("class"));

	FUnrealClassDefinitionInfo* ClassDef = FUnrealClassDefinitionInfo::FindClass(*GetClassNameWithPrefixRemoved(*DeclaredClassName));
	check(ClassDef);
	ClassDef->SetClassCastFlags(ClassCastFlagMap::Get().GetCastFlag(DeclaredClassName));
	ClassDef->SetClassCastFlags(ClassCastFlagMap::Get().GetCastFlag(FString(TEXT("F")) + DeclaredClassName.RightChop(1))); // For properties, check alternate name

	// Skip optional final keyword
	MatchIdentifier(TEXT("final"), ESearchCase::CaseSensitive);

	// Parse the inheritance list
	ParseInheritance(TEXT("class"), [](const TCHAR* ClassName, bool bIsSuperClass) {}); // Eat the results, already been parsed

	// Collect data about the super and base classes
	if (FUnrealClassDefinitionInfo* SuperClassDef = UHTCast<FUnrealClassDefinitionInfo>(ClassDef->GetSuperStructInfo().Struct))
	{
		ClassDef->SetClassCastFlags(SuperClassDef->GetClassCastFlags());
	}

	// Add the class flags from the interfaces
	for (FUnrealStructDefinitionInfo::FBaseStructInfo& BaseClassInfo : ClassDef->GetBaseStructInfos())
	{
		if (FUnrealClassDefinitionInfo* BaseClassDef = UHTCast<FUnrealClassDefinitionInfo>(BaseClassInfo.Struct))
		{
			if (!BaseClassDef->HasAnyClassFlags(CLASS_Interface))
			{
				Throwf(TEXT("Implements: Class %s is not an interface; Can only inherit from non-UObjects or UInterface derived interfaces"), *BaseClassDef->GetName());
			}

			// Propagate the inheritable ClassFlags
			ClassDef->SetClassFlags(BaseClassDef->GetClassFlags() & ClassDef->GetInheritClassFlags());
		}
	}
	return *ClassDef;
}

/**
 * Setups basic class settings after parsing.
 */
void PostParsingClassSetup(FUnrealClassDefinitionInfo& ClassDef)
{
	//@TODO: Move to post parse finalization?

	// Since this flag is computed in this method, we have to re-propagate the flag from the super
	// just in case they were defined in this source file.
	if (FUnrealClassDefinitionInfo* SuperClassDef = ClassDef.GetSuperClass())
	{
		ClassDef.SetClassFlags(SuperClassDef->GetClassFlags() & CLASS_Config);
	}

	// Set the class config flag if any properties have config
	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : ClassDef.GetProperties())
	{
		if (PropertyDef->HasAnyPropertyFlags(CPF_Config))
		{
			ClassDef.SetClassFlags(CLASS_Config);
			break;
		}
	}

	// Class needs to specify which ini file is going to be used if it contains config variables.
	if (ClassDef.HasAnyClassFlags(CLASS_Config) && ClassDef.GetClassConfigName() == NAME_None)
	{
		// Inherit config setting from base class.
		ClassDef.SetClassConfigName(ClassDef.GetSuperClass() ? ClassDef.GetSuperClass()->GetClassConfigName() : NAME_None);
		if (ClassDef.GetClassConfigName() == NAME_None)
		{
			ClassDef.Throwf(TEXT("Classes with config / globalconfig member variables need to specify config file."));
			ClassDef.SetClassConfigName(NAME_Engine);
		}
	}
}

/**
 * Compiles a class declaration.
 */
FUnrealClassDefinitionInfo& FHeaderParser::CompileClassDeclaration()
{
	// Start of a class block.
	CheckAllow(TEXT("'class'"), ENestAllowFlags::Class);

	// New-style UCLASS() syntax
	TMap<FName, FString> MetaData;

	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Class"), MetaData);

	const int32 PrologFinishLine = InputLine;

	// Members of classes have a default private access level in c++
	// Setting this directly should be ok as we don't support nested classes, so the outer scope access should not need restoring
	CurrentAccessSpecifier = ACCESS_Private;

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	// New style files have the class name / extends afterwards
	RequireIdentifier(TEXT("class"), ESearchCase::CaseSensitive, TEXT("Class declaration"));

	SkipAlignasAndDeprecatedMacroIfNecessary(*this);

	FString DeclaredClassName;
	FString RequiredAPIMacroIfPresent;
	
	FUnrealClassDefinitionInfo& ClassDef = ParseClassNameDeclaration(/*out*/ DeclaredClassName, /*out*/ RequiredAPIMacroIfPresent);

	ClassDef.GetDefinitionRange().Start = &Input[InputPos];

	// If we have existing class flags (preexisting engine objects), make sure we have some shared flags.
	check(ClassDef.GetClassFlags() == 0 || (ClassDef.GetClassFlags() & ClassDef.GetParsedClassFlags()) != 0);

	ClassDef.MarkParsed();

	PushNest(ENestType::Class, &ClassDef);
	
	ResetClassData();

	// Make sure our parent classes is parsed.
	for (FUnrealClassDefinitionInfo* TempDef = ClassDef.GetSuperClass(); TempDef; TempDef = TempDef->GetSuperClass())
	{
		bool bIsParsed = TempDef->IsParsed();
		bool bIsIntrinsic = TempDef->HasAnyClassFlags(CLASS_Intrinsic);
		if (!(bIsParsed || bIsIntrinsic))
		{
			Throwf(TEXT("'%s' can't be compiled: Parent class '%s' has errors"), *ClassDef.GetName(), *TempDef->GetName());
		}
	}

	// Merge with categories inherited from the parent.
	ClassDef.MergeClassCategories();

	// Class attributes.
	ClassDef.SetPrologLine(PrologFinishLine);
	ClassDef.MergeAndValidateClassFlags(DeclaredClassName);
	ClassDef.SetInternalFlags(EInternalObjectFlags::Native);

	// Class metadata
	MetaData.Append(ClassDef.GetParsedMetaData());
	ClassDef.MergeCategoryMetaData(MetaData);
	AddIncludePathToMetadata(ClassDef, MetaData);
	AddModuleRelativePathToMetadata(ClassDef, MetaData);

	// Register the metadata
	FUHTMetaData::RemapAndAddMetaData(ClassDef, MoveTemp(MetaData));

	// Handle the start of the rest of the class
	RequireSymbol( TEXT('{'), TEXT("'Class'") );

	// Copy properties from parent class.
	if (FUnrealClassDefinitionInfo* SuperClassDef = ClassDef.GetSuperClass())
	{
		ClassDef.SetPropertiesSize(SuperClassDef->GetPropertiesSize());
	}

	// Validate sparse class data
	CheckSparseClassData(ClassDef);

	return ClassDef;
}

FUnrealClassDefinitionInfo* FHeaderParser::ParseInterfaceNameDeclaration(FString& DeclaredInterfaceName, FString& RequiredAPIMacroIfPresent)
{
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ DeclaredInterfaceName, /*out*/ RequiredAPIMacroIfPresent, TEXT("interface"));

	FUnrealClassDefinitionInfo* ClassDef = FUnrealClassDefinitionInfo::FindClass(*GetClassNameWithPrefixRemoved(*DeclaredInterfaceName));
	if (ClassDef == nullptr)
	{
		return nullptr;
	}

	// Get super interface
	bool bSpecifiesParentClass = MatchSymbol(TEXT(':'));
	if (!bSpecifiesParentClass)
	{
		return ClassDef;
	}

	RequireIdentifier(TEXT("public"), ESearchCase::CaseSensitive, TEXT("class inheritance"));

	// verify if our super class is an interface class
	// the super class should have been marked as CLASS_Interface at the importing stage, if it were an interface
	FUnrealClassDefinitionInfo* TempClassDef = GetQualifiedClass(TEXT("'extends'"));
	check(TempClassDef);
	if (!TempClassDef->HasAnyClassFlags(CLASS_Interface))
	{
		// UInterface is special and actually extends from UObject, which isn't an interface
		if (DeclaredInterfaceName != TEXT("UInterface"))
		{
			Throwf(TEXT("Interface class '%s' cannot inherit from non-interface class '%s'"), *DeclaredInterfaceName, *TempClassDef->GetName());
		}
	}

	// The class super should have already been set by now
	FUnrealClassDefinitionInfo* SuperClassDef = ClassDef->GetSuperClass();
	check(SuperClassDef);
	if (SuperClassDef != TempClassDef)
	{
		Throwf(TEXT("%s's superclass must be %s, not %s"), *ClassDef->GetPathName(), *SuperClassDef->GetPathName(), *TempClassDef->GetPathName());
	}

	return ClassDef;
}

bool FHeaderParser::TryParseIInterfaceClass()
{
	// 'class' was already matched by the caller

	// Get a class name
	FString DeclaredInterfaceName;
	FString RequiredAPIMacroIfPresent;
	if (ParseInterfaceNameDeclaration(/*out*/ DeclaredInterfaceName, /*out*/ RequiredAPIMacroIfPresent) == nullptr)
	{
		return false;
	}

	if (MatchSymbol(TEXT(';')))
	{
		// Forward declaration.
		return false;
	}

	if (DeclaredInterfaceName[0] != 'I')
	{
		return false;
	}

	FUnrealClassDefinitionInfo* FoundClassDef = nullptr;
	if ((FoundClassDef = FUnrealClassDefinitionInfo::FindClass(*DeclaredInterfaceName.Mid(1))) == nullptr)
	{
		return false;
	}

	// Continue parsing the second class as if it were a part of the first (for reflection data purposes, it is)
	RequireSymbol(TEXT('{'), TEXT("C++ interface mix-in class declaration"));

	// Push the interface class nesting again.
	PushNest(ENestType::NativeInterface, FoundClassDef);

	CurrentAccessSpecifier = ACCESS_Private;
	return true;
}

/**
 *  compiles Java or C# style interface declaration
 */
void FHeaderParser::CompileInterfaceDeclaration()
{
	// Start of an interface block. Since Interfaces and Classes are always at the same nesting level,
	// whereever a class declaration is allowed, an interface declaration is also allowed.
	CheckAllow( TEXT("'interface'"), ENestAllowFlags::Class );

	CurrentAccessSpecifier = ACCESS_Private;

	FString DeclaredInterfaceName;
	FString RequiredAPIMacroIfPresent;
	TMap<FName, FString> MetaData;

	// Build up a list of interface specifiers
	TArray<FPropertySpecifier> SpecifiersFound;

	// New-style UINTERFACE() syntax
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Interface"), MetaData);

	int32 PrologFinishLine = InputLine;

	// New style files have the interface name / extends afterwards
	RequireIdentifier(TEXT("class"), ESearchCase::CaseSensitive, TEXT("Interface declaration"));
	FUnrealClassDefinitionInfo* InterfaceClassDef = ParseInterfaceNameDeclaration(/*out*/ DeclaredInterfaceName, /*out*/ RequiredAPIMacroIfPresent);
	check(InterfaceClassDef);
	InterfaceClassDef->GetDefinitionRange().Start = &Input[InputPos];

	// Record that this interface is RequiredAPI if the CORE_API style macro was present
	if (!RequiredAPIMacroIfPresent.IsEmpty())
	{
		InterfaceClassDef->SetClassFlags(CLASS_RequiredAPI);
	}

	// Set the appropriate interface class flags
	InterfaceClassDef->SetClassFlags(CLASS_Interface | CLASS_Abstract);
	if (FUnrealClassDefinitionInfo* InterfaceSuperClassDef = InterfaceClassDef->GetSuperClass())
	{
		InterfaceClassDef->SetClassCastFlags(InterfaceSuperClassDef->GetClassCastFlags());
		// All classes that are parsed are expected to be native
		if (!InterfaceSuperClassDef->HasAnyClassFlags(CLASS_Native))
		{
			Throwf(TEXT("Native classes cannot extend non-native classes"));
		}
		InterfaceClassDef->SetClassWithin(InterfaceSuperClassDef->GetClassWithin());
	}
	else
	{
		InterfaceClassDef->SetClassWithin(GUObjectDef);
	}
	InterfaceClassDef->SetInternalFlags(EInternalObjectFlags::Native);
	InterfaceClassDef->SetClassFlags(CLASS_Native);

	// Process all of the interface specifiers
	for (const FPropertySpecifier& Specifier : SpecifiersFound)
	{
		switch ((EInterfaceSpecifier)Algo::FindSortedStringCaseInsensitive(*Specifier.Key, GInterfaceSpecifierStrings))
		{
			default:
			{
				Throwf(TEXT("Unknown interface specifier '%s'"), *Specifier.Key);
			}
			break;

			case EInterfaceSpecifier::DependsOn:
			{
				Throwf(TEXT("The dependsOn specifier is deprecated. Please use #include \"ClassHeaderFilename.h\" instead."));
			}
			break;

			case EInterfaceSpecifier::MinimalAPI:
			{
				InterfaceClassDef->SetClassFlags(CLASS_MinimalAPI);
			}
			break;

			case EInterfaceSpecifier::ConversionRoot:
			{
				MetaData.Add(FHeaderParserNames::NAME_IsConversionRoot, TEXT("true"));
			}
			break;
		}
	}

	// All classes must start with a valid Unreal prefix
	const FString ExpectedInterfaceName = InterfaceClassDef->GetNameWithPrefix(EEnforceInterfacePrefix::U);
	if (DeclaredInterfaceName != ExpectedInterfaceName)
	{
		Throwf(TEXT("Interface name '%s' is invalid, the first class should be identified as '%s'"), *DeclaredInterfaceName, *ExpectedInterfaceName );
	}

	// Try parsing metadata for the interface
	InterfaceClassDef->SetPrologLine(PrologFinishLine);

	// Register the metadata
	AddModuleRelativePathToMetadata(*InterfaceClassDef, MetaData);
	FUHTMetaData::RemapAndAddMetaData(*InterfaceClassDef, MoveTemp(MetaData));

	// Handle the start of the rest of the interface
	RequireSymbol( TEXT('{'), TEXT("'Class'") );

	// Push the interface class nesting.
	// we need a more specific set of allow flags for ENestType::Interface, only function declaration is allowed, no other stuff are allowed
	PushNest(ENestType::Interface, InterfaceClassDef);
}

void FHeaderParser::CompileRigVMMethodDeclaration(FUnrealStructDefinitionInfo& StructDef)
{
	if (!MatchSymbol(TEXT("(")))
	{
		Throwf(TEXT("Bad RIGVM_METHOD definition"));
	}

	// find the next close brace
	while (!MatchSymbol(TEXT(")")))
	{
		FToken Token;
		if (!GetToken(Token))
		{
			break;
		}
	}

	FToken PrefixToken, ReturnTypeToken, NameToken, PostfixToken;
	if (!GetToken(PrefixToken))
	{
		return;
	}

	if (PrefixToken.IsIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive))
	{
		if (!GetToken(ReturnTypeToken))
		{
			return;
		}
	}
	else
	{
		ReturnTypeToken = PrefixToken;
	}

	if (!GetToken(NameToken))
	{
		return;
	}

	if (!MatchSymbol(TEXT("(")))
	{
		Throwf(TEXT("Bad RIGVM_METHOD definition"));
	}

	TArray<FString> ParamsContent;
	while (!MatchSymbol(TEXT(")")))
	{
		FToken Token;
		if (!GetToken(Token))
		{
			break;
		}
		ParamsContent.Add(FString(Token.Value));
	}

	while (!PostfixToken.IsSymbol(TEXT(';')))
	{
		if (!GetToken(PostfixToken))
		{
			return;
		}
	}

	FRigVMMethodInfo MethodInfo;
	MethodInfo.ReturnType = FString(ReturnTypeToken.Value);
	MethodInfo.Name = FString(NameToken.Value);

	// look out for the upgrade info method
	static const FString GetUpgradeInfoString = TEXT("GetUpgradeInfo");
	static const FString RigVMStructUpgradeInfoString = TEXT("FRigVMStructUpgradeInfo");
	if(MethodInfo.ReturnType == RigVMStructUpgradeInfoString && MethodInfo.Name == GetUpgradeInfoString)
	{
		FRigVMStructInfo& StructRigVMInfo = StructDef.GetRigVMInfo();
		StructRigVMInfo.bHasGetUpgradeInfoMethod = true;
		return;
	}

	// look out for the next aggregate name method
	static const FString GetNextAggregateNameString = TEXT("GetNextAggregateName");
	if(MethodInfo.Name == GetNextAggregateNameString)
	{
		FRigVMStructInfo& StructRigVMInfo = StructDef.GetRigVMInfo();
		StructRigVMInfo.bHasGetNextAggregateNameMethod = true;
		return;
	}

	FString ParamString = FString::Join(ParamsContent, TEXT(" "));
	if (!ParamString.IsEmpty())
	{
		FString ParamPrev, ParamLeft, ParamRight;
		ParamPrev = ParamString;
		while (ParamPrev.Contains(TEXT(",")))
		{
			ParamPrev.Split(TEXT(","), &ParamLeft, &ParamRight);
			FRigVMParameter Parameter;
			Parameter.Name = ParamLeft.TrimStartAndEnd();
			MethodInfo.Parameters.Add(Parameter);
			ParamPrev = ParamRight;
		}

		ParamPrev = ParamPrev.TrimStartAndEnd();
		if (!ParamPrev.IsEmpty())
		{
			FRigVMParameter Parameter;
			Parameter.Name = ParamPrev.TrimStartAndEnd();
			MethodInfo.Parameters.Add(Parameter);
		}
	}

	for (FRigVMParameter& Parameter : MethodInfo.Parameters)
	{
		FString FullParameter = Parameter.Name;

		int32 LastEqual = INDEX_NONE;
		if (FullParameter.FindLastChar(TCHAR('='), LastEqual))
		{
			FullParameter = FullParameter.Mid(0, LastEqual);
		}

		FullParameter.TrimStartAndEndInline();

		FString ParameterType = FullParameter;
		FString ParameterName = FullParameter;

		int32 LastSpace = INDEX_NONE;
		if (FullParameter.FindLastChar(TCHAR(' '), LastSpace))
		{
			Parameter.Type = FullParameter.Mid(0, LastSpace);
			Parameter.Name = FullParameter.Mid(LastSpace + 1);
			Parameter.Type.TrimStartAndEndInline();
			Parameter.Name.TrimStartAndEndInline();
		}
	}

	FRigVMStructInfo& StructRigVMInfo = StructDef.GetRigVMInfo();
	StructRigVMInfo.bHasRigVM = true;
	StructRigVMInfo.Name = StructDef.GetName();
	StructRigVMInfo.Methods.Add(MethodInfo);
}

const FName FHeaderParser::NAME_InputText(TEXT("Input"));
const FName FHeaderParser::NAME_OutputText(TEXT("Output"));
const FName FHeaderParser::NAME_ConstantText(TEXT("Constant"));
const FName FHeaderParser::NAME_VisibleText(TEXT("Visible"));

const FName FHeaderParser::NAME_SingletonText(TEXT("Singleton"));

const TCHAR* FHeaderParser::TArrayText = TEXT("TArray");
const TCHAR* FHeaderParser::TEnumAsByteText = TEXT("TEnumAsByte");
const TCHAR* FHeaderParser::GetRefText = TEXT("GetRef");

const TCHAR* FHeaderParser::FTArrayText = TEXT("TArray");
const TCHAR* FHeaderParser::FTArrayViewText = TEXT("TArrayView");
const TCHAR* FHeaderParser::GetArrayText = TEXT("GetArray");
const TCHAR* FHeaderParser::GetArrayViewText = TEXT("GetArrayView");
const TCHAR* FHeaderParser::FTScriptInterfaceText = TEXT("TScriptInterface");

void FHeaderParser::ParseRigVMMethodParameters(FUnrealStructDefinitionInfo& StructDef)
{
	FRigVMStructInfo& StructRigVMInfo = StructDef.GetRigVMInfo();
	if (!StructRigVMInfo.bHasRigVM)
	{
		return;
	}

	// validate the property types for this struct
	for (FUnrealPropertyDefinitionInfo* PropertyDef : TUHTFieldRange<FUnrealPropertyDefinitionInfo>(StructDef))
	{
		FString MemberCPPType;
		FString ExtendedCPPType;
		MemberCPPType = PropertyDef->GetCPPType(&ExtendedCPPType);

		if (ExtendedCPPType.IsEmpty() && MemberCPPType.StartsWith(TEnumAsByteText))
		{
			MemberCPPType = MemberCPPType.LeftChop(1).RightChop(12);
		}

		FRigVMParameter Parameter;
		Parameter.Name = PropertyDef->GetName();
		Parameter.Type = MemberCPPType + ExtendedCPPType;
		Parameter.bConstant = PropertyDef->HasMetaData(NAME_ConstantText);
		Parameter.bInput = PropertyDef->HasMetaData(NAME_InputText);
		Parameter.bOutput = PropertyDef->HasMetaData(NAME_OutputText);
		Parameter.Getter = GetRefText;
		Parameter.bEditorOnly = PropertyDef->IsEditorOnlyProperty();
		Parameter.bSingleton = PropertyDef->HasMetaData(NAME_SingletonText);
		Parameter.bIsEnum = PropertyDef->GetPropertyBase().IsEnum();

		if (PropertyDef->HasMetaData(NAME_VisibleText))
		{
			Parameter.bConstant = true;
			Parameter.bInput = true;
			Parameter.bOutput = false;
		}

		if (Parameter.bEditorOnly)
		{
			LogError(TEXT("RigVM Struct '%s' - Member '%s' is editor only - WITH_EDITORONLY_DATA not allowed on structs with RIGVM_METHOD."), *StructDef.GetName(), *Parameter.Name, *MemberCPPType);
		}

#if !UE_RIGVM_UOBJECT_PROPERTIES_ENABLED
		if(PropertyDef->GetPropertyBase().IsObject())
		{
			LogError(TEXT("RigVM Struct '%s' - Member '%s' is a UObject - object types are not allowed on structs with RIGVM_METHOD."), *StructDef.GetName(), *Parameter.Name);
		}
#endif
#if !UE_RIGVM_UINTERFACE_PROPERTIES_ENABLED
		if (PropertyDef->GetPropertyBase().IsInterface())
		{
			LogError(TEXT("RigVM Struct '%s' - Member '%s' is a UInterface - interface types are not allowed on structs with RIGVM_METHOD."), *StructDef.GetName(), *Parameter.Name);
		}
#endif

		if (!ExtendedCPPType.IsEmpty())
		{
			// we only support arrays & script interfaces - no maps or similar data structures
			if (MemberCPPType != TArrayText && MemberCPPType != TEnumAsByteText && MemberCPPType != FTScriptInterfaceText)
			{
				LogError(TEXT("RigVM Struct '%s' - Member '%s' type '%s' not supported by RigVM."), *StructDef.GetName(), *Parameter.Name, *MemberCPPType);
				continue;
			}
		}

		if (MemberCPPType.StartsWith(TArrayText, ESearchCase::CaseSensitive))
		{
			ExtendedCPPType = FString::Printf(TEXT("<%s>"), *ExtendedCPPType.LeftChop(1).RightChop(1));
			
			if(Parameter.IsConst())
			{
				ExtendedCPPType = FString::Printf(TEXT("<const %s>"), *ExtendedCPPType.LeftChop(1).RightChop(1));
				Parameter.CastName = FString::Printf(TEXT("%s_%d_Array"), *Parameter.Name, StructRigVMInfo.Members.Num());
				Parameter.CastType = FString::Printf(TEXT("%s%s"), FTArrayViewText, *ExtendedCPPType);
			}
		}

		StructRigVMInfo.Members.Add(MoveTemp(Parameter));
	}

	if (StructRigVMInfo.Members.Num() == 0)
	{
		LogError(TEXT("RigVM Struct '%s' - has zero members - invalid RIGVM_METHOD."), *StructDef.GetName());
	}

	if (StructRigVMInfo.Members.Num() > 64)
	{
		LogError(TEXT("RigVM Struct '%s' - has %d members (64 is the limit)."), *StructDef.GetName(), StructRigVMInfo.Members.Num());
	}
}

// Returns true if the token is a dynamic delegate declaration
bool FHeaderParser::IsValidDelegateDeclaration(const FToken& Token) const
{
	return Token.IsIdentifier() && Token.ValueStartsWith(TEXT("DECLARE_DYNAMIC_"), ESearchCase::CaseSensitive);
}

// Parse the parameter list of a function or delegate declaration
void FHeaderParser::ParseParameterList(FUnrealFunctionDefinitionInfo& FunctionDef, bool bExpectCommaBeforeName, TMap<FName, FString>* MetaData, EGetVarTypeOptions Options)
{
	// Get parameter list.
	if (MatchSymbol(TEXT(')')))
	{
		return;
	}

	FAdvancedDisplayParameterHandler AdvancedDisplay(MetaData);
	do
	{
		// Get parameter type.
		FPropertyBase Property(CPT_None);
		EVariableCategory VariableCategory = FunctionDef.HasAnyFunctionFlags(FUNC_Net) ? EVariableCategory::ReplicatedParameter : EVariableCategory::RegularParameter;
		GetVarType(GetCurrentScope(), Options, Property, ~(CPF_ParmFlags | CPF_AutoWeak | CPF_RepSkip | CPF_UObjectWrapper | CPF_NativeAccessSpecifiers), EUHTPropertyType::None, CPF_None, EPropertyDeclarationStyle::None, VariableCategory);
		Property.PropertyFlags |= CPF_Parm;

		if (bExpectCommaBeforeName)
		{
			RequireSymbol(TEXT(','), TEXT("Delegate definitions require a , between the parameter type and parameter name"));
		}

		FUnrealPropertyDefinitionInfo& PropDef = GetVarNameAndDim(FunctionDef, Property, VariableCategory);

		if( AdvancedDisplay.CanMarkMore() && AdvancedDisplay.ShouldMarkParameter(PropDef.GetName()) )
		{
			PropDef.SetPropertyFlags(CPF_AdvancedDisplay);
		}

		// Check parameters.
		if (FunctionDef.HasAnyFunctionFlags(FUNC_Net))
		{
			if (Property.MapKeyProp.IsValid())
			{
				if (!FunctionDef.HasAnyFunctionFlags(FUNC_NetRequest | FUNC_NetResponse))
				{
					LogError(TEXT("Maps are not supported in an RPC."));
				}
			}
			else if (Property.ArrayType == EArrayType::Set)
			{
				if (!FunctionDef.HasAnyFunctionFlags(FUNC_NetRequest | FUNC_NetResponse))
				{
					LogError(TEXT("Sets are not supported in an RPC."));
				}
			}

			if (Property.Type == CPT_Struct)
			{
				if (!FunctionDef.HasAnyFunctionFlags(FUNC_NetRequest | FUNC_NetResponse))
				{
					ValidateScriptStructOkForNet(Property.ScriptStructDef->GetName(), *Property.ScriptStructDef);
				}
			}

			if (!FunctionDef.HasAnyFunctionFlags(FUNC_NetRequest))
			{
				if (Property.PropertyFlags & CPF_OutParm)
				{
					LogError(TEXT("Replicated functions cannot contain out parameters"));
				}

				if (Property.PropertyFlags & CPF_RepSkip)
				{
					LogError(TEXT("Only service request functions cannot contain NoReplication parameters"));
				}

				if (PropDef.GetPropertyBase().IsDelegateOrDelegateStaticArray())
				{
					LogError(TEXT("Replicated functions cannot contain delegate parameters (this would be insecure)"));
				}

				if (Property.Type == CPT_String && Property.RefQualifier != ERefQualifier::ConstRef && !PropDef.IsStaticArray())
				{
					LogError(TEXT("Replicated FString parameters must be passed by const reference"));
				}

				if (Property.ArrayType == EArrayType::Dynamic && Property.RefQualifier != ERefQualifier::ConstRef && !PropDef.IsStaticArray())
				{
					LogError(TEXT("Replicated TArray parameters must be passed by const reference"));
				}
			}
			else
			{
				if (!(Property.PropertyFlags & CPF_RepSkip) && (Property.PropertyFlags & CPF_OutParm))
				{
					LogError(TEXT("Service request functions cannot contain out parameters, unless marked NotReplicated"));
				}

				if (!(Property.PropertyFlags & CPF_RepSkip) && PropDef.GetPropertyBase().IsDelegateOrDelegateStaticArray())
				{
					LogError(TEXT("Service request functions cannot contain delegate parameters, unless marked NotReplicated"));
				}
			}
		}
		if (FunctionDef.HasAnyFunctionFlags(FUNC_BlueprintEvent|FUNC_BlueprintCallable))
		{
			if (Property.Type == CPT_Byte && Property.IsPrimitiveOrPrimitiveStaticArray())
			{
				if (FUnrealEnumDefinitionInfo* EnumDef = Property.AsEnum())
				{
					if (EnumDef->GetUnderlyingType() != EUnderlyingEnumType::uint8 &&
						EnumDef->GetUnderlyingType() != EUnderlyingEnumType::Unspecified)
					{
						Throwf(TEXT("Invalid enum param for Blueprints - currently only uint8 supported"));
					}
				}
			}

			// Check that the parameter name is valid and does not conflict with pre-defined types
			{
				const static TArray<FString> InvalidParamNames =
				{
					TEXT("self"),
				};

				for (const FString& InvalidName : InvalidParamNames)
				{
					if (FCString::Stricmp(*PropDef.GetNameCPP(), *InvalidName) == 0)
					{
						LogError(TEXT("Paramater name '%s' in function is invalid, '%s' is a reserved name."), *InvalidName, *InvalidName);
					}
				}
			}
		}

		// Default value.
		if (MatchSymbol(TEXT('=')))
		{
			// Skip past the native specified default value; we make no attempt to parse it
			FToken SkipToken;
			int32 ParenthesisNestCount=0;
			int32 StartPos=-1;
			int32 EndPos=-1;
			while ( GetToken(SkipToken) )
			{
				if (StartPos == -1)
				{
					StartPos = SkipToken.StartPos;
				}
				if ( ParenthesisNestCount == 0
					&& (SkipToken.IsSymbol(TEXT(')')) || SkipToken.IsSymbol(TEXT(','))) )
				{
					// went too far
					UngetToken(SkipToken);
					break;
				}
				EndPos = InputPos;
				if ( SkipToken.IsSymbol(TEXT('(')) )
				{
					ParenthesisNestCount++;
				}
				else if ( SkipToken.IsSymbol(TEXT(')')) )
				{
					ParenthesisNestCount--;
				}
			}

			// allow exec functions to be added to the metaData, this is so we can have default params for them.
			const bool bStoreCppDefaultValueInMetaData = FunctionDef.HasAnyFunctionFlags(FUNC_BlueprintCallable | FUNC_Exec);
				
			if((EndPos > -1) && bStoreCppDefaultValueInMetaData) 
			{
				FString DefaultArgText(EndPos - StartPos, Input + StartPos);
				FString Key(TEXT("CPP_Default_"));
				Key += PropDef.GetName();
				FName KeyName = FName(*Key);
				if (!MetaData->Contains(KeyName))
				{
					FString InnerDefaultValue;
					const bool bDefaultValueParsed = FPropertyTraits::DefaultValueStringCppFormatToInnerFormat(PropDef, DefaultArgText, InnerDefaultValue);
					if (!bDefaultValueParsed)
					{
						Throwf(TEXT("C++ Default parameter not parsed: %s \"%s\" "), *PropDef.GetName(), *DefaultArgText);
					}

					MetaData->Add(KeyName, InnerDefaultValue);
					UE_LOG(LogCompile, Verbose, TEXT("C++ Default parameter parsed: %s \"%s\" -> \"%s\" "), *PropDef.GetName(), *DefaultArgText, *InnerDefaultValue );
				}
			}
		}
	} while( MatchSymbol(TEXT(',')) );
	RequireSymbol( TEXT(')'), TEXT("parameter list") );
}
FUnrealFunctionDefinitionInfo& FHeaderParser::CompileDelegateDeclaration(const FStringView& DelegateIdentifier, EDelegateSpecifierAction::Type SpecifierAction)
{
	const TCHAR* CurrentScopeName = TEXT("Delegate Declaration");

	TMap<FName, FString> MetaData;
	AddModuleRelativePathToMetadata(SourceFile, MetaData);

	FFuncInfo            FuncInfo;

	// If this is a UDELEGATE, parse the specifiers first
	FString DelegateMacro;
	if (SpecifierAction == EDelegateSpecifierAction::Parse)
	{
		TArray<FPropertySpecifier> SpecifiersFound;
		ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Delegate"), MetaData);

		ProcessFunctionSpecifiers(*this, FuncInfo, SpecifiersFound, MetaData);

		// Get the next token and ensure it looks like a delegate
		FToken Token;
		GetToken(Token);
		if (!IsValidDelegateDeclaration(Token))
		{
			Throwf(TEXT("Unexpected token following UDELEGATE(): %s"), *Token.GetTokenValue());
		}

		DelegateMacro = FString(Token.Value);
		CheckAllow(CurrentScopeName, ENestAllowFlags::TypeDecl);
	}
	else
	{
		DelegateMacro = DelegateIdentifier;
		CheckAllow(CurrentScopeName, ENestAllowFlags::ImplicitDelegateDecl);
	}

	EGetVarTypeOptions Options = EGetVarTypeOptions::None;
	if (MetaData.Contains(NAME_DeprecatedFunction))
	{
		Options |= EGetVarTypeOptions::OuterTypeDeprecated;
	}

	// Break the delegate declaration macro down into parts
	const bool bHasReturnValue = DelegateMacro.Contains(TEXT("_RetVal"), ESearchCase::CaseSensitive);
	const bool bDeclaredConst  = DelegateMacro.Contains(TEXT("_Const"), ESearchCase::CaseSensitive);
	const bool bIsMulticast    = DelegateMacro.Contains(TEXT("_MULTICAST"), ESearchCase::CaseSensitive);
	const bool bIsSparse       = DelegateMacro.Contains(TEXT("_SPARSE"), ESearchCase::CaseSensitive);

	// Determine the parameter count
	const FString* FoundParamCount = UHTConfig.DelegateParameterCountStrings.FindByPredicate([&](const FString& Str){ return DelegateMacro.Contains(Str); });

	// Try reconstructing the string to make sure it matches our expectations
	FString ExpectedOriginalString = FString::Printf(TEXT("DECLARE_DYNAMIC%s%s_DELEGATE%s%s%s"),
		bIsMulticast ? TEXT("_MULTICAST") : TEXT(""),
		bIsSparse ? TEXT("_SPARSE") : TEXT(""),
		bHasReturnValue ? TEXT("_RetVal") : TEXT(""),
		FoundParamCount ? **FoundParamCount : TEXT(""),
		bDeclaredConst ? TEXT("_Const") : TEXT(""));

	if (DelegateMacro != ExpectedOriginalString)
	{
		Throwf(TEXT("Unable to parse delegate declaration; expected '%s' but found '%s'."), *ExpectedOriginalString, *DelegateMacro);
	}

	// Multi-cast delegate function signatures are not allowed to have a return value
	if (bHasReturnValue && bIsMulticast)
	{
		LogError(TEXT("Multi-cast delegates function signatures must not return a value"));
	}

	// Delegate signature
	FuncInfo.FunctionFlags |= FUNC_Public | FUNC_Delegate;

	if (bIsMulticast)
	{
		FuncInfo.FunctionFlags |= FUNC_MulticastDelegate;
	}

	const TCHAR* StartPos = &Input[InputPos];

	// Now parse the macro body
	RequireSymbol(TEXT('('), CurrentScopeName);

	// Parse the return value type
	FPropertyBase ReturnType( CPT_None );

	if (bHasReturnValue)
	{
		GetVarType(GetCurrentScope(), Options, ReturnType, CPF_None, EUHTPropertyType::None, CPF_None, EPropertyDeclarationStyle::None, EVariableCategory::Return);
		RequireSymbol(TEXT(','), CurrentScopeName);
	}

	// Skip whitespaces to get InputPos exactly on beginning of function name.
	SkipWhitespaceAndComments();

	FuncInfo.InputPos = InputPos;

	// Get the delegate name
	FToken FuncNameToken;
	if (!GetIdentifier(FuncNameToken))
	{
		Throwf(TEXT("Missing name for %s"), CurrentScopeName );
	}
	FString FuncName(FuncNameToken.Value);

	// If this is a delegate function then go ahead and mangle the name so we don't collide with
	// actual functions or properties
	{
		//@TODO: UCREMOVAL: Eventually this mangling shouldn't occur

		// Remove the leading F

		if (!FuncName.StartsWith(TEXT("F"), ESearchCase::CaseSensitive))
		{
			Throwf(TEXT("Delegate type declarations must start with F"));
		}

		FuncName.RightChopInline(1, false);

		// Append the signature goo
		FuncName += HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX;
	}

	FUnrealFunctionDefinitionInfo& DelegateSignatureFunctionDef = CreateFunction(*FuncName, MoveTemp(FuncInfo), bIsSparse ? EFunctionType::SparseDelegate : EFunctionType::Delegate);
	DelegateSignatureFunctionDef.GetDefinitionRange().Start = StartPos;

	// determine whether this function should be 'const'
	if (bDeclaredConst)
	{
		DelegateSignatureFunctionDef.SetFunctionFlags(FUNC_Const);
	}

	if (bIsSparse)
	{
		FToken OwningClass;

		RequireSymbol(TEXT(','), TEXT("Delegate Declaration"));

		if (!GetIdentifier(OwningClass))
		{
			Throwf(TEXT("Missing OwningClass specifier."));
		}
		RequireSymbol(TEXT(','), TEXT("Delegate Declaration"));
		
		FToken DelegateName;
		if (!GetIdentifier(DelegateName))
		{
			Throwf(TEXT("Missing Delegate Name."));
		}

		DelegateSignatureFunctionDef.SetSparseOwningClassName(*GetClassNameWithoutPrefix(FString(OwningClass.Value)));
		DelegateSignatureFunctionDef.SetSparseDelegateName(FName(DelegateName.Value, FNAME_Add));
	}

	DelegateSignatureFunctionDef.SetLineNumber(InputLine);

	// Get parameter list.
	if (FoundParamCount)
	{
		RequireSymbol(TEXT(','), CurrentScopeName);

		ParseParameterList(DelegateSignatureFunctionDef, /*bExpectCommaBeforeName=*/ true);

		// Check the expected versus actual number of parameters
		int32 ParamCount = UE_PTRDIFF_TO_INT32(FoundParamCount - UHTConfig.DelegateParameterCountStrings.GetData()) + 1;
		if (DelegateSignatureFunctionDef.GetProperties().Num() != ParamCount)
		{
			Throwf(TEXT("Expected %d parameters but found %d parameters"), ParamCount, DelegateSignatureFunctionDef.GetProperties().Num());
		}
	}
	else
	{
		// Require the closing paren even with no parameter list
		RequireSymbol(TEXT(')'), TEXT("Delegate Declaration"));
	}

	DelegateSignatureFunctionDef.GetDefinitionRange().End = &Input[InputPos];

	// The macro line must be set here
	DelegateSignatureFunctionDef.GetFunctionData().MacroLine = InputLine;

	// Create the return value property
	if (bHasReturnValue)
	{
		ReturnType.PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
		FUnrealPropertyDefinitionInfo& PropDef = GetVarNameAndDim(DelegateSignatureFunctionDef, ReturnType, EVariableCategory::Return);
	}

	// Try parsing metadata for the function
	ParseFieldMetaData(MetaData, *DelegateSignatureFunctionDef.GetName());

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	FUHTMetaData::RemapAndAddMetaData(DelegateSignatureFunctionDef, MoveTemp(MetaData));

	// Optionally consume a semicolon, it's not required for the delegate macro since it contains one internally
	MatchSemi();

	// End the nesting
	PostPopFunctionDeclaration(DelegateSignatureFunctionDef);

	// Don't allow delegate signatures to be redefined.
	auto FunctionIterator = GetCurrentScope()->GetTypeIterator<FUnrealFunctionDefinitionInfo>();
	while (FunctionIterator.MoveNext())
	{
		FUnrealFunctionDefinitionInfo* TestFuncDef = *FunctionIterator;
		if (TestFuncDef != &DelegateSignatureFunctionDef && TestFuncDef->GetFName() == DelegateSignatureFunctionDef.GetFName())
		{
			Throwf(TEXT("Can't override delegate signature function '%s'"), *DelegateSignatureFunctionDef.GetNameCPP());
		}
	}

	return DelegateSignatureFunctionDef;
}

/**
 * Parses and compiles a function declaration
 */
FUnrealFunctionDefinitionInfo& FHeaderParser::CompileFunctionDeclaration()
{
	CheckAllow(TEXT("'Function'"), ENestAllowFlags::Function);

	TMap<FName, FString> MetaData;
	AddModuleRelativePathToMetadata(SourceFile, MetaData);

	// New-style UFUNCTION() syntax 
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, TEXT("Function"), MetaData);

	FUnrealClassDefinitionInfo& OuterClassDef = GetCurrentClassDef();
	if (!OuterClassDef.HasAnyClassFlags(CLASS_Native))
	{
		Throwf(TEXT("Should only be here for native classes!"));
	}

	// Process all specifiers.
	const TCHAR* TypeOfFunction = TEXT("function");

	bool bAutomaticallyFinal = true;

	FFuncInfo FuncInfo;
	FuncInfo.MacroLine = InputLine;
	FuncInfo.FunctionFlags = FUNC_Native;

	// Infer the function's access level from the currently declared C++ access level
	if (CurrentAccessSpecifier == ACCESS_Public)
	{
		FuncInfo.FunctionFlags |= FUNC_Public;
	}
	else if (CurrentAccessSpecifier == ACCESS_Protected)
	{
		FuncInfo.FunctionFlags |= FUNC_Protected;
	}
	else if (CurrentAccessSpecifier == ACCESS_Private)
	{
		FuncInfo.FunctionFlags |= FUNC_Private;
		FuncInfo.FunctionFlags |= FUNC_Final;

		// This is automatically final as well, but in a different way and for a different reason
		bAutomaticallyFinal = false;
	}
	else
	{
		Throwf(TEXT("Unknown access level"));
	}

	// non-static functions in a const class must be const themselves
	if (OuterClassDef.HasAnyClassFlags(CLASS_Const))
	{
		FuncInfo.FunctionFlags |= FUNC_Const;
	}

	if (MatchIdentifier(TEXT("static"), ESearchCase::CaseSensitive))
	{
		FuncInfo.FunctionFlags |= FUNC_Static;
		FuncInfo.FunctionExportFlags |= FUNCEXPORT_CppStatic;
	}

	if (MetaData.Contains(NAME_CppFromBpEvent))
	{
		FuncInfo.FunctionFlags |= FUNC_Event;
	}

	if ((GetCurrentCompilerDirective() & ECompilerDirective::WithEditor) != 0)
	{
		FuncInfo.FunctionFlags |= FUNC_EditorOnly;
	}

	ProcessFunctionSpecifiers(*this, FuncInfo, SpecifiersFound, MetaData);

	if ((0 != (FuncInfo.FunctionExportFlags & FUNCEXPORT_CustomThunk)) && !MetaData.Contains(NAME_CustomThunk))
	{
		MetaData.Add(NAME_CustomThunk, TEXT("true"));
	}

	if ((FuncInfo.FunctionFlags & FUNC_BlueprintPure) && OuterClassDef.HasAnyClassFlags(CLASS_Interface))
	{
		// Until pure interface casts are supported, we don't allow pures in interfaces
		LogError(TEXT("BlueprintPure specifier is not allowed for interface functions"));
	}

	if (FuncInfo.FunctionFlags & FUNC_Net)
	{
		// Network replicated functions are always events, and are only final if sealed
		TypeOfFunction = TEXT("event");
		bAutomaticallyFinal = false;
	}

	if (FuncInfo.FunctionFlags & FUNC_BlueprintEvent)
	{
		TypeOfFunction = (FuncInfo.FunctionFlags & FUNC_Native) ? TEXT("BlueprintNativeEvent") : TEXT("BlueprintImplementableEvent");
		bAutomaticallyFinal = false;
	}

	// Record the tokens so we can detect this function as a declaration later (i.e. RPC)
	FRecordTokens RecordTokens(*this, &GetCurrentClassDef(), nullptr);

	bool bSawVirtual = false;

	if (MatchIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive))
	{
		bSawVirtual = true;
	}

	FString*   InternalPtr = MetaData.Find(NAME_BlueprintInternalUseOnly); // FBlueprintMetadata::MD_BlueprintInternalUseOnly
	const bool bInternalOnly = InternalPtr && *InternalPtr == TEXT("true");

	const bool bDeprecated = MetaData.Contains(NAME_DeprecatedFunction); // FBlueprintMetadata::MD_DeprecatedFunction

	// If this function is blueprint callable or blueprint pure, require a category 
	if ((FuncInfo.FunctionFlags & (FUNC_BlueprintCallable | FUNC_BlueprintPure)) != 0) 
	{ 
		const bool bBlueprintAccessor = MetaData.Contains(NAME_BlueprintSetter) || MetaData.Contains(NAME_BlueprintGetter); // FBlueprintMetadata::MD_BlueprintSetter, // FBlueprintMetadata::MD_BlueprintGetter
		const bool bHasMenuCategory = MetaData.Contains(NAME_Category);                 // FBlueprintMetadata::MD_FunctionCategory

		if (!bHasMenuCategory && !bInternalOnly && !bDeprecated && !bBlueprintAccessor) 
		{ 
			// To allow for quick iteration, don't enforce the requirement that game functions have to be categorized
			if (bIsCurrentModulePartOfEngine)
			{
				LogError(TEXT("An explicit Category specifier is required for Blueprint accessible functions in an Engine module."));
			}
		}
	}

	// Verify interfaces with respect to their blueprint accessible functions
	if (OuterClassDef.HasAnyClassFlags(CLASS_Interface))
	{
		// Interface with blueprint data should declare explicitly Blueprintable or NotBlueprintable to be clear
		// In the backward compatible case where they declare neither, both of these bools are false
		const bool bCanImplementInBlueprints = OuterClassDef.GetBoolMetaData(NAME_IsBlueprintBase);
		const bool bCannotImplementInBlueprints = (!bCanImplementInBlueprints && OuterClassDef.HasMetaData(NAME_IsBlueprintBase))
			|| OuterClassDef.HasMetaData(NAME_CannotImplementInterfaceInBlueprint);

		if((FuncInfo.FunctionFlags & FUNC_BlueprintEvent) != 0 && !bInternalOnly)
		{
			// Ensure that blueprint events are only allowed in implementable interfaces. Internal only functions allowed
			if (bCannotImplementInBlueprints)
			{
				LogError(TEXT("Interfaces that are not implementable in blueprints cannot have Blueprint Event members."));
			}
			if (!bCanImplementInBlueprints)
			{
				// We do not currently warn about this case as there are a large number of existing interfaces that do not specify
				// LogWarning(TEXT("Interfaces with Blueprint Events should declare Blueprintable on the interface."));
			}
		}
		
		if (((FuncInfo.FunctionFlags & FUNC_BlueprintCallable) != 0) && (((~FuncInfo.FunctionFlags) & FUNC_BlueprintEvent) != 0))
		{
			// Ensure that if this interface contains blueprint callable functions that are not blueprint defined, that it must be implemented natively
			if (bCanImplementInBlueprints)
			{
				LogError(TEXT("Blueprint implementable interfaces cannot contain BlueprintCallable functions that are not BlueprintImplementableEvents. Add NotBlueprintable to the interface if you wish to keep this function."));
			}
			if (!bCannotImplementInBlueprints)
			{
				// Lowered this case to a warning instead of error, they will not show up as blueprintable unless they also have events
				LogWarning(TEXT("Interfaces with BlueprintCallable functions but no events should explicitly declare NotBlueprintable on the interface."));
			}
		}
	}

	// Peek ahead to look for a CORE_API style DLL import/export token if present
	FString APIMacroIfPresent;
	{
		FToken Token;
		if (GetToken(Token, true))
		{
			bool bThrowTokenBack = true;
			if (Token.IsIdentifier())
			{
				FString RequiredAPIMacroIfPresent(Token.Value);
				if (RequiredAPIMacroIfPresent.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
				{
					//@TODO: Validate the module name for RequiredAPIMacroIfPresent
					bThrowTokenBack = false;

					if (OuterClassDef.HasAnyClassFlags(CLASS_RequiredAPI))
					{
						Throwf(TEXT("'%s' must not be used on methods of a class that is marked '%s' itself."), *RequiredAPIMacroIfPresent, *RequiredAPIMacroIfPresent);
					}
					FuncInfo.FunctionFlags |= FUNC_RequiredAPI;
					FuncInfo.FunctionExportFlags |= FUNCEXPORT_RequiredAPI;

					APIMacroIfPresent = RequiredAPIMacroIfPresent;
				}
			}

			if (bThrowTokenBack)
			{
				UngetToken(Token);
			}
		}
	}

	// Look for static again, in case there was an ENGINE_API token first
	if (!APIMacroIfPresent.IsEmpty() && MatchIdentifier(TEXT("static"), ESearchCase::CaseSensitive))
	{
		Throwf(TEXT("Unexpected API macro '%s'. Did you mean to put '%s' after the static keyword?"), *APIMacroIfPresent, *APIMacroIfPresent);
	}

	// Look for virtual again, in case there was an ENGINE_API token first
	if (MatchIdentifier(TEXT("virtual"), ESearchCase::CaseSensitive))
	{
		bSawVirtual = true;
	}

	// Process the virtualness
	if (bSawVirtual)
	{
		// Remove the implicit final, the user can still specifying an explicit final at the end of the declaration
		bAutomaticallyFinal = false;

		// if this is a BlueprintNativeEvent or BlueprintImplementableEvent in an interface, make sure it's not "virtual"
		if (FuncInfo.FunctionFlags & FUNC_BlueprintEvent)
		{
			if (OuterClassDef.HasAnyClassFlags(CLASS_Interface))
			{
				Throwf(TEXT("BlueprintImplementableEvents in Interfaces must not be declared 'virtual'"));
			}

			// if this is a BlueprintNativeEvent, make sure it's not "virtual"
			else if (FuncInfo.FunctionFlags & FUNC_Native)
			{
				LogError(TEXT("BlueprintNativeEvent functions must be non-virtual."));
			}

			else
			{
				LogWarning(TEXT("BlueprintImplementableEvents should not be virtual. Use BlueprintNativeEvent instead."));
			}
		}
	}
	else
	{
		// if this is a function in an Interface, it must be marked 'virtual' unless it's an event
		if (OuterClassDef.HasAnyClassFlags(CLASS_Interface) && !(FuncInfo.FunctionFlags & FUNC_BlueprintEvent))
		{
			Throwf(TEXT("Interface functions that are not BlueprintImplementableEvents must be declared 'virtual'"));
		}
	}

	// Handle the initial implicit/explicit final
	// A user can still specify an explicit final after the parameter list as well.
	if (bAutomaticallyFinal || FuncInfo.bSealedEvent)
	{
		FuncInfo.FunctionFlags |= FUNC_Final;
		FuncInfo.FunctionExportFlags |= FUNCEXPORT_Final;

		if (OuterClassDef.HasAnyClassFlags(CLASS_Interface))
		{
			LogError(TEXT("Interface functions cannot be declared 'final'"));
		}
	}

	EGetVarTypeOptions Options = EGetVarTypeOptions::None;
	if (bDeprecated)
	{
		Options |= EGetVarTypeOptions::OuterTypeDeprecated;
	}
	if (EnumHasAllFlags(FuncInfo.FunctionFlags, FUNC_BlueprintEvent | FUNC_Native))
	{
		Options |= EGetVarTypeOptions::NoAutoConst;
	}

	// Get return type.
	FPropertyBase ReturnType( CPT_None );

	// C++ style functions always have a return value type, even if it's void
	GetVarType(GetCurrentScope(), Options, ReturnType, CPF_None, EUHTPropertyType::None, CPF_None, EPropertyDeclarationStyle::None, EVariableCategory::Return);
	bool bHasReturnValue = ReturnType.Type != CPT_None;

	// Skip whitespaces to get InputPos exactly on beginning of function name.
	SkipWhitespaceAndComments();

	FuncInfo.InputPos = InputPos;

	// Get function or operator name.
	FToken FuncNameToken;
	if (!GetIdentifier(FuncNameToken))
	{
		Throwf(TEXT("Missing %s name"), TypeOfFunction);
	}
	FString FuncName(FuncNameToken.Value);

	const TCHAR* StartPos = &Input[InputPos];

	if ( !MatchSymbol(TEXT('(')) )
	{
		Throwf(TEXT("Bad %s definition"), TypeOfFunction);
	}

	if (FuncInfo.FunctionFlags & FUNC_Net)
	{
		bool bIsNetService = !!(FuncInfo.FunctionFlags & (FUNC_NetRequest | FUNC_NetResponse));
		if (bHasReturnValue && !bIsNetService)
		{
			Throwf(TEXT("Replicated functions can't have return values"));
		}

		if (FuncInfo.RPCId > 0)
		{
			if (FString* ExistingFunc = UsedRPCIds.Find(FuncInfo.RPCId))
			{
				Throwf(TEXT("Function %s already uses identifier %d"), **ExistingFunc, FuncInfo.RPCId);
			}

			UsedRPCIds.Add(FuncInfo.RPCId, FuncName);
			if (FuncInfo.FunctionFlags & FUNC_NetResponse)
			{
				// Look for another function expecting this response
				if (FString* ExistingFunc = RPCsNeedingHookup.Find(FuncInfo.RPCId))
				{
					// If this list isn't empty at end of class, throw error
					RPCsNeedingHookup.Remove(FuncInfo.RPCId);
				}
			}
		}

		if (FuncInfo.RPCResponseId > 0 && FuncInfo.EndpointName != TEXT("JSBridge"))
		{
			// Look for an existing response function
			FString* ExistingFunc = UsedRPCIds.Find(FuncInfo.RPCResponseId);
			if (ExistingFunc == NULL)
			{
				// If this list isn't empty at end of class, throw error
				RPCsNeedingHookup.Add(FuncInfo.RPCResponseId, FuncName);
			}
		}
	}

	FUnrealFunctionDefinitionInfo& FuncDef = CreateFunction(*FuncName, MoveTemp(FuncInfo), EFunctionType::Function);
	FuncDef.GetDefinitionRange().Start = StartPos;
	const FFuncInfo& FuncDefFuncInfo = FuncDef.GetFunctionData();
	FUnrealFunctionDefinitionInfo* SuperFuncDef = FuncDef.GetSuperFunction();

	// Get parameter list.
	ParseParameterList(FuncDef, false, &MetaData, Options);

	// Get return type, if any.
	if (bHasReturnValue)
	{
		ReturnType.PropertyFlags |= CPF_Parm | CPF_OutParm | CPF_ReturnParm;
		FUnrealPropertyDefinitionInfo& PropDef = GetVarNameAndDim(FuncDef, ReturnType, EVariableCategory::Return);
	}

	// determine if there are any outputs for this function
	bool bHasAnyOutputs = bHasReturnValue;
	if (!bHasAnyOutputs)
	{
		for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : FuncDef.GetProperties())
		{
			if (PropertyDef->HasSpecificPropertyFlags(CPF_ReturnParm | CPF_OutParm, CPF_OutParm))
			{
				bHasAnyOutputs = true;
				break;
			}
		}
	}

	// Check to see if there is a function in the super class with the same name
	FUnrealStructDefinitionInfo* SuperStructDef = &GetCurrentClassDef();
	if (SuperStructDef)
	{
		SuperStructDef = SuperStructDef->GetSuperStruct();
	}
	if (SuperStructDef)
	{
		if (FUnrealFunctionDefinitionInfo* OverriddenFunctionDef = FindFunction(*SuperStructDef, *FuncDef.GetNameCPP(), true, nullptr))
		{
			// Native function overrides should be done in CPP text, not in a UFUNCTION() declaration (you can't change flags, and it'd otherwise be a burden to keep them identical)
			LogError(TEXT("%s: Override of UFUNCTION in parent class (%s) cannot have a UFUNCTION() declaration above it; it will use the same parameters as the original declaration."), *FuncDef.GetNameCPP(), *OverriddenFunctionDef->GetOuter()->GetName());
		}
	}

	if (!bHasAnyOutputs && FuncDef.HasAnyFunctionFlags(FUNC_BlueprintPure))
	{
		LogError(TEXT("BlueprintPure specifier is not allowed for functions with no return value and no output parameters."));
	}


	// determine whether this function should be 'const'
	if ( MatchIdentifier(TEXT("const"), ESearchCase::CaseSensitive) )
	{
		if (FuncDef.HasAnyFunctionFlags(FUNC_Native))
		{
			// @TODO: UCREMOVAL Reconsider?
			//Throwf(TEXT("'const' may only be used for native functions"));
		}

		FuncDef.SetFunctionFlags(FUNC_Const);

		// @todo: the presence of const and one or more outputs does not guarantee that there are
		// no side effects. On GCC and clang we could use __attribure__((pure)) or __attribute__((const))
		// or we could just rely on the use marking things BlueprintPure. Either way, checking the C++
		// const identifier to determine purity is not desirable. We should remove the following logic:

		// If its a const BlueprintCallable function with some sort of output and is not being marked as an BlueprintPure=false function, mark it as BlueprintPure as well
		if (bHasAnyOutputs && FuncDef.HasAnyFunctionFlags(FUNC_BlueprintCallable) && !FuncDefFuncInfo.bForceBlueprintImpure)
		{
			FuncDef.SetFunctionFlags(FUNC_BlueprintPure);
		}
	}

	// Try parsing metadata for the function
	ParseFieldMetaData(MetaData, *FuncDef.GetName());

	AddFormattedPrevCommentAsTooltipMetaData(MetaData);

	FUHTMetaData::RemapAndAddMetaData(FuncDef, MoveTemp(MetaData));

	// 'final' and 'override' can appear in any order before an optional '= 0' pure virtual specifier
	bool bFoundFinal    = MatchIdentifier(TEXT("final"), ESearchCase::CaseSensitive);
	bool bFoundOverride = MatchIdentifier(TEXT("override"), ESearchCase::CaseSensitive);
	if (!bFoundFinal && bFoundOverride)
	{
		bFoundFinal = MatchIdentifier(TEXT("final"), ESearchCase::CaseSensitive);
	}

	// Handle C++ style functions being declared as abstract
	if (MatchSymbol(TEXT('=')))
	{
		int32 ZeroValue = 1;
		bool bGotZero = GetConstInt(/*out*/ZeroValue);
		bGotZero = bGotZero && (ZeroValue == 0);

		if (!bGotZero)
		{
			Throwf(TEXT("Expected 0 to indicate function is abstract"));
		}
	}

	// Look for the final keyword to indicate this function is sealed
	if (bFoundFinal)
	{
		// This is a final (prebinding, non-overridable) function
		FuncDef.SetFunctionFlags(FUNC_Final);
		FuncDef.SetFunctionExportFlags(FUNCEXPORT_Final);
		if (GetCurrentClassDef().HasAnyClassFlags(CLASS_Interface))
		{
			Throwf(TEXT("Interface functions cannot be declared 'final'"));
		}
		else if (FuncDef.HasAnyFunctionFlags(FUNC_BlueprintEvent))
		{
			Throwf(TEXT("Blueprint events cannot be declared 'final'"));
		}
	}

	// Make sure that the replication flags set on an overridden function match the parent function
	if (SuperFuncDef != nullptr)
	{
		if ((FuncDef.GetFunctionFlags() & FUNC_NetFuncFlags) != (SuperFuncDef->GetFunctionFlags() & FUNC_NetFuncFlags))
		{
			Throwf(TEXT("Overridden function '%s': Cannot specify different replication flags when overriding a function."), *FuncDef.GetName());
		}
	}

	// if this function is an RPC in state scope, verify that it is an override
	// this is required because the networking code only checks the class for RPCs when initializing network data, not any states within it
	if (FuncDef.HasAnyFunctionFlags(FUNC_Net) && SuperFuncDef == nullptr && UHTCast<FUnrealClassDefinitionInfo>(FuncDef.GetOuter()) == nullptr)
	{
		Throwf(TEXT("Function '%s': Base implementation of RPCs cannot be in a state. Add a stub outside state scope."), *FuncDef.GetName());
	}

	FuncDef.GetDefinitionRange().End = &Input[InputPos];

	// Just declaring a function, so end the nesting.
	PostPopFunctionDeclaration(FuncDef);

	// See what's coming next
	FToken Token;
	if (!GetToken(Token))
	{
		Throwf(TEXT("Unexpected end of file"));
	}

	// Optionally consume a semicolon
	// This is optional to allow inline function definitions
	if (Token.IsSymbol(TEXT(';')))
	{
		// Do nothing (consume it)
	}
	else if (Token.IsSymbol(TEXT('{')))
	{
		// Skip inline function bodies
		UngetToken(Token);
		SkipDeclaration(Token);
	}
	else
	{
		// Put the token back so we can continue parsing as normal
		UngetToken(Token);
	}

	// perform documentation policy tests
	CheckDocumentationPolicyForFunc(GetCurrentClassDef(), FuncDef);

	return FuncDef;
}

/** Parses optional metadata text. */
void FHeaderParser::ParseFieldMetaData(TMap<FName, FString>& MetaData, const TCHAR* FieldName)
{
	FTokenString PropertyMetaData;
	bool bMetadataPresent = false;
	if (MatchIdentifier(TEXT("UMETA"), ESearchCase::CaseSensitive))
	{
		auto ErrorMessageGetter = [FieldName]() { return FString::Printf(TEXT("' %s metadata'"), FieldName); };

		bMetadataPresent = true;
		RequireSymbol( TEXT('('), ErrorMessageGetter );
		if (!GetRawStringRespectingQuotes(PropertyMetaData, TCHAR(')')))
		{
			Throwf(TEXT("'%s': No metadata specified"), FieldName);
		}
		RequireSymbol( TEXT(')'), ErrorMessageGetter);
	}

	if (bMetadataPresent)
	{
		// parse apart the string
		TArray<FString> Pairs;

		//@TODO: UCREMOVAL: Convert to property token reading
		// break apart on | to get to the key/value pairs
		FString NewData(PropertyMetaData.String);
		bool bInString = false;
		int32 LastStartIndex = 0;
		int32 CharIndex;
		for (CharIndex = 0; CharIndex < NewData.Len(); ++CharIndex)
		{
			TCHAR Ch = NewData.GetCharArray()[CharIndex];
			if (Ch == '"')
			{
				bInString = !bInString;
			}

			if ((Ch == ',') && !bInString)
			{
				if (LastStartIndex != CharIndex)
				{
					Pairs.Add(NewData.Mid(LastStartIndex, CharIndex - LastStartIndex));
				}
				LastStartIndex = CharIndex + 1;
			}
		}

		if (LastStartIndex != CharIndex)
		{
			Pairs.Add(MoveTemp(NewData).Mid(LastStartIndex, CharIndex - LastStartIndex));
		}

		// go over all pairs
		for (int32 PairIndex = 0; PairIndex < Pairs.Num(); PairIndex++)
		{
			// break the pair into a key and a value
			FString Token = MoveTemp(Pairs[PairIndex]);
			FString Key;
			// by default, not value, just a key (allowed)
			FString Value;

			// look for a value after an =
			const int32 Equals = Token.Find(TEXT("="), ESearchCase::CaseSensitive);
			// if we have an =, break up the string
			if (Equals != INDEX_NONE)
			{
				Key = Token.Left(Equals);
				Value = MoveTemp(Token);
				Value.RightInline((Value.Len() - Equals) - 1, false);
			}
			else
			{
				Key = MoveTemp(Token);
			}

			InsertMetaDataPair(MetaData, MoveTemp(Key), MoveTemp(Value));
		}
	}
}

bool FHeaderParser::IsBitfieldProperty(ELayoutMacroType LayoutMacroType)
{
	if (LayoutMacroType == ELayoutMacroType::Bitfield || LayoutMacroType == ELayoutMacroType::BitfieldEditorOnly)
	{
		return true;
	}

	bool bIsBitfield = false;

	// The current token is the property type (uin32, uint16, etc).
	// Check the property name and then check for ':'
	FToken TokenVarName;
	if (GetToken(TokenVarName, /*bNoConsts=*/ true))
	{
		FToken Token;
		if (GetToken(Token, /*bNoConsts=*/ true))
		{
			if (Token.IsSymbol(TEXT(':')))
			{
				bIsBitfield = true;
			}
			UngetToken(Token);
		}
		UngetToken(TokenVarName);
	}

	return bIsBitfield;
}

void FHeaderParser::ValidateTypeIsDeprecated(EVariableCategory VariableCategory, FUnrealTypeDefinitionInfo* TypeDef)
{
	if (FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(TypeDef); ClassDef != nullptr && ClassDef->HasAnyClassFlags(CLASS_Deprecated))
	{
		if (VariableCategory == EVariableCategory::Member)
		{
			LogError(TEXT("Property is using a deprecated class: %s.  Property should be marked deprecated as well."), *ClassDef->GetPathName());
		}
		else
		{
			LogError(TEXT("Function is using a deprecated class: %s.  Function should be marked deprecated as well."), *ClassDef->GetPathName());
		}
	}
}


void FHeaderParser::ValidatePropertyIsDeprecatedIfNecessary(bool bOuterTypeDeprecated, EVariableCategory VariableCategory, const FPropertyBase& VarProperty, EUHTPropertyType OuterPropertyType, EPropertyFlags OuterPropertyFlags)
{
	// If the outer object being parsed is deprecated,  then we don't need to do any further tests 
	if (bOuterTypeDeprecated)
	{
		return;
	}

	// If the property is in a container that has been deprecated, then we don't need to do any further tests 
	if (OuterPropertyType != EUHTPropertyType::None && (OuterPropertyFlags & CPF_Deprecated) != 0)
	{
		return;
	}

	// If the property is already marked deprecated, then we don't need to do any further tests 
	if ((VarProperty.PropertyFlags & CPF_Deprecated) != 0)
	{
		return;
	}

	ValidateTypeIsDeprecated(VariableCategory, VarProperty.MetaClassDef);
	ValidateTypeIsDeprecated(VariableCategory, VarProperty.TypeDef);
}

bool FHeaderParser::ValidateScriptStructOkForNet(const FString& OriginStructName, FUnrealScriptStructDefinitionInfo& InStructDef)
{
	if (ScriptStructsValidForNet.Contains(&InStructDef))
	{
		return true;
	}

	bool bIsStructValid = true;

	if (FUnrealScriptStructDefinitionInfo* SuperScriptStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(InStructDef.GetSuperStructInfo().Struct))
	{
		if (!ValidateScriptStructOkForNet(OriginStructName, *SuperScriptStructDef))
		{
			bIsStructValid = false;
		}
	}

	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : InStructDef.GetProperties())
	{
		if (PropertyDef->IsSet())
		{
			if (!PropertyDef->HasAnyPropertyFlags(CPF_RepSkip))
			{
				bIsStructValid = false;
				LogError(TEXT("Sets are not supported for Replication or RPCs.  Set %s in %s.  Origin %s"), *PropertyDef->GetName(), *PropertyDef->GetOuter()->GetName(), *OriginStructName);
			}
		}
		else if (PropertyDef->IsMap())
		{
			if (!PropertyDef->HasAnyPropertyFlags(CPF_RepSkip))
			{
				bIsStructValid = false;
				LogError(TEXT("Maps are not supported for Replication or RPCs.  Map %s in %s.  Origin %s"), *PropertyDef->GetName(), *PropertyDef->GetOuter()->GetName(), *OriginStructName);
			}
		}
		else if (PropertyDef->IsStructOrStructStaticArray())
		{
			if (!ValidateScriptStructOkForNet(OriginStructName, *PropertyDef->GetPropertyBase().ScriptStructDef))
			{
				bIsStructValid = false;
			}
		}
	}

	if (bIsStructValid)
	{
		ScriptStructsValidForNet.Add(&InStructDef);
	}

	return bIsStructValid;
}

struct FExposeOnSpawnValidator
{
	// Keep this function synced with UEdGraphSchema_K2::FindSetVariableByNameFunction
	static bool IsSupported(const FPropertyBase& Property)
	{
		bool ProperNativeType = false;
		switch (Property.Type)
		{
		case CPT_Int:
		case CPT_Int64:
		case CPT_Byte:
		case CPT_Float:
		case CPT_Bool:
		case CPT_Bool8:
		case CPT_ObjectReference:
		case CPT_ObjectPtrReference:
		case CPT_String:
		case CPT_Text:
		case CPT_Name:
		case CPT_Interface:
		case CPT_SoftObjectReference:
			ProperNativeType = true;
		}

		if (!ProperNativeType && (CPT_Struct == Property.Type) && Property.ScriptStructDef)
		{
			ProperNativeType |= Property.ScriptStructDef->GetBoolMetaData(FHeaderParserNames::NAME_BlueprintType);
		}

		return ProperNativeType;
	}
};

void FHeaderParser::CompileVariableDeclaration(FUnrealStructDefinitionInfo& StructDef)
{
	EPropertyFlags DisallowFlags = CPF_ParmFlags;
	EPropertyFlags EdFlags       = CPF_None;
	FUnrealScriptStructDefinitionInfo* ScriptStructDef = UHTCast< FUnrealScriptStructDefinitionInfo>(StructDef);

	// Get variable type.
	FPropertyBase OriginalPropertyBase(CPT_None);
	FIndexRange TypeRange;
	ELayoutMacroType LayoutMacroType = ELayoutMacroType::None;
	GetVarType(&*StructDef.GetScope(), EGetVarTypeOptions::None, OriginalPropertyBase, DisallowFlags, EUHTPropertyType::None, CPF_None, EPropertyDeclarationStyle::UPROPERTY, EVariableCategory::Member, &TypeRange, &LayoutMacroType);
	OriginalPropertyBase.PropertyFlags |= EdFlags;

	FString* Category = OriginalPropertyBase.MetaData.Find(NAME_Category);

	// First check if the category was specified at all and if the property was exposed to the editor.
	if (!Category && (OriginalPropertyBase.PropertyFlags & (CPF_Edit|CPF_BlueprintVisible)))
	{
		if ((&StructDef.GetPackageDef() != nullptr) && !bIsCurrentModulePartOfEngine)
		{
			Category = &OriginalPropertyBase.MetaData.Add(NAME_Category, StructDef.GetName());
		}
		else
		{
			LogError(TEXT("An explicit Category specifier is required for any property exposed to the editor or Blueprints in an Engine module."));
		}
	}

	// Validate that pointer properties are not interfaces (which are not GC'd and so will cause runtime errors)
	if (OriginalPropertyBase.PointerType == EPointerType::Native && OriginalPropertyBase.ClassDef->AsClass() != nullptr && OriginalPropertyBase.ClassDef->IsInterface())
	{
		// Get the name of the type, removing the asterisk representing the pointer
		FString TypeName = FString(TypeRange.Count, Input + TypeRange.StartIndex).TrimStartAndEnd().LeftChop(1).TrimEnd();
		Throwf(TEXT("UPROPERTY pointers cannot be interfaces - did you mean TScriptInterface<%s>?"), *TypeName);
	}

	// If the category was specified explicitly, it wins
	if (Category && !(OriginalPropertyBase.PropertyFlags & (CPF_Edit|CPF_BlueprintVisible|CPF_BlueprintAssignable|CPF_BlueprintCallable)))
	{
		LogWarning(TEXT("Property has a Category set but is not exposed to the editor or Blueprints with EditAnywhere, BlueprintReadWrite, VisibleAnywhere, BlueprintReadOnly, BlueprintAssignable, BlueprintCallable keywords.\r\n"));
	}

	// Make sure that editblueprint variables are editable
	if(!(OriginalPropertyBase.PropertyFlags & CPF_Edit))
	{
		if (OriginalPropertyBase.PropertyFlags & CPF_DisableEditOnInstance)
		{
			LogError(TEXT("Property cannot have 'DisableEditOnInstance' without being editable"));
		}

		if (OriginalPropertyBase.PropertyFlags & CPF_DisableEditOnTemplate)
		{
			LogError(TEXT("Property cannot have 'DisableEditOnTemplate' without being editable"));
		}
	}

	// Validate.
	if (OriginalPropertyBase.PropertyFlags & CPF_ParmFlags)
	{
		Throwf(TEXT("Illegal type modifiers in member variable declaration") );
	}

	if (FString* ExposeOnSpawnValue = OriginalPropertyBase.MetaData.Find(NAME_ExposeOnSpawn))
	{
		if ((*ExposeOnSpawnValue == TEXT("true")) && !FExposeOnSpawnValidator::IsSupported(OriginalPropertyBase))
		{
			LogError(TEXT("ExposeOnSpawn - Property cannot be exposed"));
		}
	}

	if (LayoutMacroType != ELayoutMacroType::None)
	{
		RequireSymbol(TEXT(','), GLayoutMacroNames[(int32)LayoutMacroType]);
	}

	// If Property is a Replicated Struct check to make sure there are no Properties that are not allowed to be Replicated in the Struct 
	if (OriginalPropertyBase.Type == CPT_Struct && OriginalPropertyBase.PropertyFlags & CPF_Net && OriginalPropertyBase.ScriptStructDef)
	{
		ValidateScriptStructOkForNet(OriginalPropertyBase.ScriptStructDef->GetName(), *OriginalPropertyBase.ScriptStructDef);
	}

	// Process all variables of this type.
	TArray<FUnrealPropertyDefinitionInfo*> NewProperties;
	for (;;)
	{
		FPropertyBase PropertyBase = OriginalPropertyBase;
		FUnrealPropertyDefinitionInfo& NewPropDef = GetVarNameAndDim(StructDef, PropertyBase, EVariableCategory::Member, LayoutMacroType);

		// Optionally consume the :1 at the end of a bitfield boolean declaration
		if (PropertyBase.IsBool())
		{
			if (LayoutMacroType == ELayoutMacroType::Bitfield || LayoutMacroType == ELayoutMacroType::BitfieldEditorOnly || MatchSymbol(TEXT(':')))
			{
				int32 BitfieldSize = 0;
				if (!GetConstInt(/*out*/ BitfieldSize) || (BitfieldSize != 1))
				{
					Throwf(TEXT("Bad or missing bitfield size for '%s', must be 1."), *NewPropDef.GetName());
				}
			}
		}

		// Deprecation validation
		ValidatePropertyIsDeprecatedIfNecessary(false, EVariableCategory::Member, PropertyBase, EUHTPropertyType::None, CPF_None);

		if (TopNest->NestType != ENestType::FunctionDeclaration)
		{
			if (NewProperties.Num())
			{
				Throwf(TEXT("Comma delimited properties cannot be converted %s.%s\n"), *StructDef.GetName(), *NewPropDef.GetName());
			}
		}

		// If the accessor tag was found but need to be autogenerated.
		if (NewPropDef.GetPropertyBase().bSetterTagFound && NewPropDef.GetPropertyBase().SetterName.IsEmpty())
		{
			NewPropDef.GetPropertyBase().SetterName = FString::Printf(TEXT("Set%s"), *NewPropDef.GetName());
		}
		if (NewPropDef.GetPropertyBase().bGetterTagFound && NewPropDef.GetPropertyBase().GetterName.IsEmpty())
		{
			NewPropDef.GetPropertyBase().GetterName = FString::Printf(TEXT("Get%s"), *NewPropDef.GetName());
		}

		NewProperties.Add(&NewPropDef);

		// we'll need any metadata tags we parsed later on when we call ConvertEOLCommentToTooltip() so the tags aren't clobbered
		OriginalPropertyBase.MetaData = PropertyBase.MetaData;

		if (LayoutMacroType != ELayoutMacroType::None || !MatchSymbol(TEXT(',')))
		{
			break;
		}
	}

	// Optional member initializer.
	if (LayoutMacroType == ELayoutMacroType::FieldInitialized)
	{
		// Skip past the specified member initializer; we make no attempt to parse it
		FToken SkipToken;
		int Nesting = 1;
		while (GetToken(SkipToken))
		{
			if (SkipToken.IsSymbol(TEXT('(')))
			{
				++Nesting;
			}
			else if (SkipToken.IsSymbol(TEXT(')')))
			{
				--Nesting;
				if (Nesting == 0)
				{
					UngetToken(SkipToken);
					break;
				}
			}
		}
	}
	else if (MatchSymbol(TEXT('=')))
	{
		// Skip past the specified member initializer; we make no attempt to parse it
		FToken SkipToken;
		while (GetToken(SkipToken))
		{
			if (SkipToken.IsSymbol(TEXT(';')))
			{
				// went too far
				UngetToken(SkipToken);
				break;
			}
		}
	}
	// Using Brace Initialization
	else if (MatchSymbol(TEXT('{')))
	{
		FToken SkipToken;
		int BraceLevel = 1;
		while (GetToken(SkipToken))
		{
			if (SkipToken.IsSymbol(TEXT('{')))
			{
				++BraceLevel;
			}
			else if (SkipToken.IsSymbol(TEXT('}')))
			{
				--BraceLevel;
				if (BraceLevel == 0)
				{
					break;
				}
			}
		}
	}

	if (LayoutMacroType == ELayoutMacroType::None)
	{
		// Expect a semicolon.
		RequireSymbol(TEXT(';'), TEXT("'variable declaration'"));
	}
	else
	{
		// Expect a close bracket.
		RequireSymbol(TEXT(')'), GLayoutMacroNames[(int32)LayoutMacroType]);
	}

	// Skip redundant semi-colons
	for (;;)
	{
		int32 CurrInputPos  = InputPos;
		int32 CurrInputLine = InputLine;

		FToken Token;
		if (!GetToken(Token, /*bNoConsts=*/ true))
		{
			break;
		}

		if (!Token.IsSymbol(TEXT(';')))
		{
			InputPos  = CurrInputPos;
			InputLine = CurrInputLine;
			break;
		}
	}
}

//
// Compile a statement: Either a declaration or a command.
// Returns 1 if success, 0 if end of file.
//
bool FHeaderParser::CompileStatement(TArray<FUnrealFunctionDefinitionInfo*>& DelegatesToFixup)
{
	// Get a token and compile it.
	FToken Token;
	if( !GetToken(Token, true) )
	{
		// End of file.
		return false;
	}
	else if (!CompileDeclaration(DelegatesToFixup, Token))
	{
		Throwf(TEXT("'%s': Bad command or expression"), *Token.GetTokenValue() );
	}
	return true;
}

/*-----------------------------------------------------------------------------
	Code skipping.
-----------------------------------------------------------------------------*/

/**
 * Skip over code, honoring { and } pairs.
 *
 * @param	NestCount	number of nest levels to consume. if 0, consumes a single statement
 * @param	ErrorTag	text to use in error message if EOF is encountered before we've done
 */
void FHeaderParser::SkipStatements( int32 NestCount, const TCHAR* ErrorTag  )
{
	FToken Token;

	int32 OriginalNestCount = NestCount;

	while( GetToken( Token, true ) )
	{
		if ( Token.IsSymbol(TEXT('{')) )
		{
			NestCount++;
		}
		else if	( Token.IsSymbol(TEXT('}')) )
		{
			NestCount--;
		}
		else if ( Token.IsSymbol(TEXT(';')) && OriginalNestCount == 0 )
		{
			break;
		}

		if ( NestCount < OriginalNestCount || NestCount < 0 )
			break;
	}

	if( NestCount > 0 )
	{
		Throwf(TEXT("Unexpected end of file at end of %s"), ErrorTag );
	}
	else if ( NestCount < 0 )
	{
		Throwf(TEXT("Extraneous closing brace found in %s"), ErrorTag);
	}
}

/*-----------------------------------------------------------------------------
	Main script compiling routine.
-----------------------------------------------------------------------------*/

// Parse Class's annotated headers and optionally its child classes.
static const FString ObjectHeader(TEXT("NoExportTypes.h"));

//
// Parses the header associated with the specified class.
// Returns result enumeration.
//
void FHeaderParser::ParseHeader()
{
	// Reset the parser to begin a new class
	bEncounteredNewStyleClass_UnmatchedBrackets = false;
	bSpottedAutogeneratedHeaderInclude          = false;
	bHaveSeenUClass                             = false;
	bClassHasGeneratedBody                      = false;
	bClassHasGeneratedUInterfaceBody            = false;
	bClassHasGeneratedIInterfaceBody            = false;

	// Message.
	UE_LOG(LogCompile, Verbose, TEXT("Parsing %s"), *SourceFile.GetFilename());

	// Make sure that all of the requried include files are added to the outer scope
	{
		TArray<FUnrealSourceFile*> SourceFilesRequired;
		for (FHeaderProvider& Include : SourceFile.GetIncludes())
		{
			if (Include.GetId() == ObjectHeader)
			{
				continue;
			}

			if (FUnrealSourceFile* DepFile = Include.Resolve(SourceFile))
			{
				SourceFilesRequired.Add(DepFile);
			}
		}

		for (const TSharedRef<FUnrealTypeDefinitionInfo>& ClassDataPair : SourceFile.GetDefinedClasses())
		{
			FUnrealClassDefinitionInfo& ClassDef = ClassDataPair->AsClassChecked();
			for (FUnrealClassDefinitionInfo* ParentClassDef = ClassDef.GetSuperClass(); ParentClassDef; ParentClassDef = ParentClassDef->GetSuperClass())
			{
				if (ParentClassDef->IsParsed() || ParentClassDef->HasAnyClassFlags(CLASS_Intrinsic))
				{
					break;
				}
				SourceFilesRequired.Add(&ParentClassDef->GetUnrealSourceFile());
			}
		}

		for (FUnrealSourceFile* RequiredFile : SourceFilesRequired)
		{
			SourceFile.GetScope()->IncludeScope(&RequiredFile->GetScope().Get());
		}
	}

	// Init compiler variables.
	ResetParser(*SourceFile.GetContent());

	// Init nesting.
	NestLevel = 0;
	TopNest = NULL;
	PushNest(ENestType::GlobalScope, nullptr, &SourceFile);

	// C++ classes default to private access level
	CurrentAccessSpecifier = ACCESS_Private; 

	// Try to compile it, and catch any errors.
	bool bEmptyFile = true;

	// Tells if this header defines no-export classes only.
	bool bNoExportClassesOnly = true;

	// Parse entire program.
	TArray<FUnrealFunctionDefinitionInfo*> DelegatesToFixup;
	while (CompileStatement(DelegatesToFixup))
	{
		bEmptyFile = false;

		// Clear out the previous comment in anticipation of the next statement.
		ClearComment();
		StatementsParsed++;
	}

	PopNest(ENestType::GlobalScope, TEXT("Global scope"));

	// now validate all delegate variables declared in the class
	{
		auto ScopeTypeIterator = SourceFile.GetScope()->GetTypeIterator();
		while (ScopeTypeIterator.MoveNext())
		{
			FUnrealFieldDefinitionInfo* TypeDef = *ScopeTypeIterator;
			if (TypeDef->AsScriptStruct() != nullptr || TypeDef->AsClass() != nullptr)
			{
				TMap<FName, FUnrealFunctionDefinitionInfo*> DelegateCache;
				FixupDelegateProperties(TypeDef->AsStructChecked(), *TypeDef->GetScope(), DelegateCache);
			}
		}
	}

	// Fix up any delegates themselves, if they refer to other delegates
	{
		TMap<FName, FUnrealFunctionDefinitionInfo*> DelegateCache;
		for (FUnrealFunctionDefinitionInfo* DelegateDef : DelegatesToFixup)
		{
			FixupDelegateProperties(*DelegateDef, SourceFile.GetScope().Get(), DelegateCache);
		}
	}

	// Precompute info for runtime optimization.
	LinesParsed += InputLine;

	if (RPCsNeedingHookup.Num() > 0)
	{
		FString ErrorMsg(TEXT("Request functions missing response pairs:\r\n"));
		for (TMap<int32, FString>::TConstIterator It(RPCsNeedingHookup); It; ++It)
		{
			ErrorMsg += FString::Printf(TEXT("%s missing id %d\r\n"), *It.Value(), It.Key());
		}

		RPCsNeedingHookup.Empty();
		Throwf(TEXT("%s"), *ErrorMsg);
	}

	// Make sure the compilation ended with valid nesting.
	if (bEncounteredNewStyleClass_UnmatchedBrackets)
	{
		Throwf(TEXT("Missing } at end of class") );
	}

	if (NestLevel == 1)
	{
		Throwf(TEXT("Internal nest inconsistency") );
	}
	else if (NestLevel > 2)
	{
		Throwf(TEXT("Unexpected end of script in '%s' block"), NestTypeName(TopNest->NestType) );
	}

	for (TSharedRef<FUnrealTypeDefinitionInfo>& TypeDef : SourceFile.GetDefinedClasses())
	{
		FUnrealClassDefinitionInfo& ClassDef = UHTCastChecked<FUnrealClassDefinitionInfo>(TypeDef);
		PostParsingClassSetup(ClassDef);

		bNoExportClassesOnly = bNoExportClassesOnly && ClassDef.IsNoExport();
	}

	if (!bSpottedAutogeneratedHeaderInclude && !bEmptyFile && !bNoExportClassesOnly)
	{
		const FString& ExpectedHeaderName = SourceFile.GetGeneratedHeaderFilename();
		Throwf(TEXT("Expected an include at the top of the header: '#include \"%s\"'"), *ExpectedHeaderName);
	}

	for (const TSharedRef<FUnrealTypeDefinitionInfo>& ClassDef : SourceFile.GetDefinedClasses())
	{
		ClassDef->AsClassChecked().MarkParsed();
	}

	// Perform any final concurrent finalization
	for (const TSharedRef<FUnrealTypeDefinitionInfo>& TypeDef : SourceFile.GetDefinedTypes())
	{
		TypeDef->ConcurrentPostParseFinalize();
	}

	// Remember stats about this file
	SourceFile.SetLinesParsed(LinesParsed);
	SourceFile.SetStatementsParsed(StatementsParsed);
}

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

FHeaderParser::FHeaderParser(FUnrealPackageDefinitionInfo& InPackageDef, FUnrealSourceFile& InSourceFile)
	: FBaseParser(InSourceFile)
	, Filename(IFileManager::Get().ConvertToAbsolutePathForExternalAppForRead(*InSourceFile.GetFilename()))
	, PackageDef(InPackageDef)
{

	const FManifestModule& Module = PackageDef.GetModule();

	// Determine if the current module is part of the engine or a game (we are more strict about things for Engine modules)
	switch (Module.ModuleType)
	{
	case EBuildModuleType::Program:
		{
			const FString AbsoluteEngineDir = FPaths::ConvertRelativePathToFull(FPaths::EngineDir());
			const FString ModuleDir = FPaths::ConvertRelativePathToFull(Module.BaseDirectory);
			bIsCurrentModulePartOfEngine = ModuleDir.StartsWith(AbsoluteEngineDir);
		}
		break;
	case EBuildModuleType::EngineRuntime:
	case EBuildModuleType::EngineUncooked:
	case EBuildModuleType::EngineDeveloper:
	case EBuildModuleType::EngineEditor:
	case EBuildModuleType::EngineThirdParty:
		bIsCurrentModulePartOfEngine = true;
		break;
	case EBuildModuleType::GameRuntime:
	case EBuildModuleType::GameUncooked:
	case EBuildModuleType::GameDeveloper:
	case EBuildModuleType::GameEditor:
	case EBuildModuleType::GameThirdParty:
		bIsCurrentModulePartOfEngine = false;
		break;
	default:
		bIsCurrentModulePartOfEngine = true;
		check(false);
	}
}

// Throws if a specifier value wasn't provided
void FHeaderParser::RequireSpecifierValue(const FUHTMessageProvider& Context, const FPropertySpecifier& Specifier, bool bRequireExactlyOne)
{
	if (Specifier.Values.Num() == 0)
	{
		Context.Throwf(TEXT("The specifier '%s' must be given a value"), *Specifier.Key);
	}
	else if ((Specifier.Values.Num() != 1) && bRequireExactlyOne)
	{
		Context.Throwf(TEXT("The specifier '%s' must be given exactly one value"), *Specifier.Key);
	}
}

// Throws if a specifier value wasn't provided
FString FHeaderParser::RequireExactlyOneSpecifierValue(const FUHTMessageProvider& Context, const FPropertySpecifier& Specifier)
{
	RequireSpecifierValue(Context, Specifier, /*bRequireExactlyOne*/ true);
	return Specifier.Values[0];
}

void FHeaderParser::Parse(
	FUnrealPackageDefinitionInfo& PackageDef,
	FUnrealSourceFile& SourceFile)
{
	SCOPE_SECONDS_COUNTER_UHT(Parse);
	FHeaderParser(PackageDef, SourceFile).ParseHeader();
}

/** 
 * Returns True if the given class name includes a valid Unreal prefix and matches up with the given original class.
 *
 * @param InNameToCheck - Name w/ potential prefix to check
 * @param OriginalClassName - Name of class w/ no prefix to check against
 */
bool FHeaderParser::ClassNameHasValidPrefix(const FString& InNameToCheck, const FString& OriginalClassName)
{
	bool bIsLabledDeprecated;
	const FString ClassPrefix = GetClassPrefix( InNameToCheck, bIsLabledDeprecated );

	// If the class is labeled deprecated, don't try to resolve it during header generation, valid results can't be guaranteed.
	if (bIsLabledDeprecated)
	{
		return true;
	}

	if (ClassPrefix.IsEmpty())
	{
		return false;
	}

	FString TestString = FString::Printf(TEXT("%s%s"), *ClassPrefix, *OriginalClassName);

	const bool bNamesMatch = ( InNameToCheck == *TestString );

	return bNamesMatch;
}

void FHeaderParser::ParseClassName(const TCHAR* Temp, FString& ClassName)
{
	// Skip leading whitespace
	while (FChar::IsWhitespace(*Temp))
	{
		++Temp;
	}

	// Run thru characters (note: relying on later code to reject the name for a leading number, etc...)
	const TCHAR* StringStart = Temp;
	while (FChar::IsAlnum(*Temp) || FChar::IsUnderscore(*Temp))
	{
		++Temp;
	}

	ClassName = FString(UE_PTRDIFF_TO_INT32(Temp - StringStart), StringStart);
	if (ClassName.EndsWith(TEXT("_API"), ESearchCase::CaseSensitive))
	{
		// RequiresAPI token for a given module

		//@TODO: UCREMOVAL: Validate the module name
		FString RequiresAPISymbol = ClassName;

		// Now get the real class name
		ClassName.Empty();
		ParseClassName(Temp, ClassName);
	}
}

enum class EBlockDirectiveType
{
	// We're in a CPP block
	CPPBlock,

	// We're in a !CPP block
	NotCPPBlock,

	// We're in a 0 block
	ZeroBlock,

	// We're in a 1 block
	OneBlock,

	// We're in a WITH_HOT_RELOAD block
	WithHotReload,

	// We're in a WITH_EDITOR block
	WithEditor,

	// We're in a WITH_EDITORONLY_DATA block
	WithEditorOnlyData,

	// We're in a block with an unrecognized directive
	UnrecognizedBlock
};

bool ShouldKeepBlockContents(EBlockDirectiveType DirectiveType)
{
	switch (DirectiveType)
	{
		case EBlockDirectiveType::NotCPPBlock:
		case EBlockDirectiveType::OneBlock:
		case EBlockDirectiveType::WithHotReload:
		case EBlockDirectiveType::WithEditor:
		case EBlockDirectiveType::WithEditorOnlyData:
			return true;

		case EBlockDirectiveType::CPPBlock:
		case EBlockDirectiveType::ZeroBlock:
		case EBlockDirectiveType::UnrecognizedBlock:
			return false;
	}

	check(false);
	UE_ASSUME(false);
}

bool ShouldKeepDirective(EBlockDirectiveType DirectiveType)
{
	switch (DirectiveType)
	{
		case EBlockDirectiveType::WithHotReload:
		case EBlockDirectiveType::WithEditor:
		case EBlockDirectiveType::WithEditorOnlyData:
			return true;

		case EBlockDirectiveType::CPPBlock:
		case EBlockDirectiveType::NotCPPBlock:
		case EBlockDirectiveType::ZeroBlock:
		case EBlockDirectiveType::OneBlock:
		case EBlockDirectiveType::UnrecognizedBlock:
			return false;
	}

	check(false);
	UE_ASSUME(false);
}

EBlockDirectiveType ParseCommandToBlockDirectiveType(const TCHAR** Str)
{
	if (FParse::Command(Str, TEXT("0")))
	{
		return EBlockDirectiveType::ZeroBlock;
	}

	if (FParse::Command(Str, TEXT("1")))
	{
		return EBlockDirectiveType::OneBlock;
	}

	if (FParse::Command(Str, TEXT("CPP")))
	{
		return EBlockDirectiveType::CPPBlock;
	}

	if (FParse::Command(Str, TEXT("!CPP")))
	{
		return EBlockDirectiveType::NotCPPBlock;
	}

	if (FParse::Command(Str, TEXT("WITH_HOT_RELOAD")))
	{
		return EBlockDirectiveType::WithHotReload;
	}

	if (FParse::Command(Str, TEXT("WITH_EDITOR")))
	{
		return EBlockDirectiveType::WithEditor;
	}

	if (FParse::Command(Str, TEXT("WITH_EDITORONLY_DATA")))
	{
		return EBlockDirectiveType::WithEditorOnlyData;
	}

	return EBlockDirectiveType::UnrecognizedBlock;
}

const TCHAR* GetBlockDirectiveTypeString(EBlockDirectiveType DirectiveType)
{
	switch (DirectiveType)
	{
		case EBlockDirectiveType::CPPBlock:           return TEXT("CPP");
		case EBlockDirectiveType::NotCPPBlock:        return TEXT("!CPP");
		case EBlockDirectiveType::ZeroBlock:          return TEXT("0");
		case EBlockDirectiveType::OneBlock:           return TEXT("1");
		case EBlockDirectiveType::WithHotReload:      return TEXT("WITH_HOT_RELOAD");
		case EBlockDirectiveType::WithEditor:         return TEXT("WITH_EDITOR");
		case EBlockDirectiveType::WithEditorOnlyData: return TEXT("WITH_EDITORONLY_DATA");
		case EBlockDirectiveType::UnrecognizedBlock:  return TEXT("<unrecognized>");
	}

	check(false);
	UE_ASSUME(false);
}

// Performs a preliminary parse of the text in the specified buffer, pulling out useful information for the header generation process
void FHeaderParser::SimplifiedClassParse(FUnrealSourceFile& SourceFile, const TCHAR* InBuffer, FStringOutputDevice& ClassHeaderTextStrippedOfCppText)
{
	FHeaderPreParser Parser(SourceFile);
	FString StrLine;
	FString ClassName;
	FString BaseClassName;

	// Two passes, preprocessor, then looking for the class stuff

	// The layer of multi-line comment we are in.
	int32 CurrentLine = 0;
	const TCHAR* Buffer = InBuffer;

	// Preprocessor pass
	while (FParse::Line(&Buffer, StrLine, true))
	{
		CurrentLine++;
		const TCHAR* Str = *StrLine;
		int32 BraceCount = 0;

		bool bIf = FParse::Command(&Str,TEXT("#if"));
		if( bIf || FParse::Command(&Str,TEXT("#ifdef")) || FParse::Command(&Str,TEXT("#ifndef")) )
		{
			EBlockDirectiveType RootDirective;
			if (bIf)
			{
				RootDirective = ParseCommandToBlockDirectiveType(&Str);
			}
			else
			{
				// #ifdef or #ifndef are always treated as CPP
				RootDirective = EBlockDirectiveType::UnrecognizedBlock;
			}

			TArray<EBlockDirectiveType, TInlineAllocator<8>> DirectiveStack;
			DirectiveStack.Push(RootDirective);

			bool bShouldKeepBlockContents = ShouldKeepBlockContents(RootDirective);
			bool bIsZeroBlock = RootDirective == EBlockDirectiveType::ZeroBlock;

			ClassHeaderTextStrippedOfCppText.Logf(TEXT("%s\r\n"), ShouldKeepDirective(RootDirective) ? *StrLine : TEXT(""));

			while ((DirectiveStack.Num() > 0) && FParse::Line(&Buffer, StrLine, 1))
			{
				CurrentLine++;
				Str = *StrLine;

				bool bShouldKeepLine = bShouldKeepBlockContents;

				bool bIsDirective = false;
				if( FParse::Command(&Str,TEXT("#endif")) )
				{
					EBlockDirectiveType OldDirective = DirectiveStack.Pop();

					bShouldKeepLine &= ShouldKeepDirective(OldDirective);
					bIsDirective     = true;
				}
				else if( FParse::Command(&Str,TEXT("#if")) || FParse::Command(&Str,TEXT("#ifdef")) || FParse::Command(&Str,TEXT("#ifndef")) )
				{
					EBlockDirectiveType Directive = ParseCommandToBlockDirectiveType(&Str);
					DirectiveStack.Push(Directive);

					bShouldKeepLine &= ShouldKeepDirective(Directive);
					bIsDirective     = true;
				}
				else if (FParse::Command(&Str,TEXT("#elif")))
				{
					EBlockDirectiveType NewDirective = ParseCommandToBlockDirectiveType(&Str);
					EBlockDirectiveType OldDirective = DirectiveStack.Top();

					// Check to see if we're mixing ignorable directive types - we don't support this
					bool bKeepNewDirective = ShouldKeepDirective(NewDirective);
					bool bKeepOldDirective = ShouldKeepDirective(OldDirective);
					if (bKeepNewDirective != bKeepOldDirective)
					{
						FUHTMessage(SourceFile, CurrentLine).Throwf(
							TEXT("Mixing %s with %s in an #elif preprocessor block is not supported"),
							GetBlockDirectiveTypeString(OldDirective),
							GetBlockDirectiveTypeString(NewDirective)
						);
					}

					DirectiveStack.Top() = NewDirective;

					bShouldKeepLine &= bKeepNewDirective;
					bIsDirective     = true;
				}
				else if (FParse::Command(&Str, TEXT("#else")))
				{
					switch (DirectiveStack.Top())
					{
						case EBlockDirectiveType::ZeroBlock:
							DirectiveStack.Top() = EBlockDirectiveType::OneBlock;
							break;

						case EBlockDirectiveType::OneBlock:
							DirectiveStack.Top() = EBlockDirectiveType::ZeroBlock;
							break;

						case EBlockDirectiveType::CPPBlock:
							DirectiveStack.Top() = EBlockDirectiveType::NotCPPBlock;
							break;

						case EBlockDirectiveType::NotCPPBlock:
							DirectiveStack.Top() = EBlockDirectiveType::CPPBlock;
							break;

						case EBlockDirectiveType::WithHotReload:
							FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("Bad preprocessor directive in metadata declaration: %s; Only 'CPP', '1' and '0' can have #else directives"), *ClassName);

						case EBlockDirectiveType::UnrecognizedBlock:
						case EBlockDirectiveType::WithEditor:
						case EBlockDirectiveType::WithEditorOnlyData:
							// We allow unrecognized directives, WITH_EDITOR and WITH_EDITORONLY_DATA to have #else blocks.
							// However, we don't actually change how UHT processes these #else blocks.
							break;
					}

					bShouldKeepLine &= ShouldKeepDirective(DirectiveStack.Top());
					bIsDirective     = true;
				}
				else
				{
					// Check for UHT identifiers inside skipped blocks, unless it's a zero block, because the compiler is going to skip those anyway.
					if (!bShouldKeepBlockContents && !bIsZeroBlock)
					{
						auto FindInitialStr = [](const TCHAR*& FoundSubstr, const FString& StrToSearch, const TCHAR* ConstructName) -> bool
						{
							if (StrToSearch.StartsWith(ConstructName, ESearchCase::CaseSensitive))
							{
								FoundSubstr = ConstructName;
								return true;
							}

							return false;
						};

						FString TrimmedStrLine = StrLine;
						TrimmedStrLine.TrimStartInline();

						const TCHAR* FoundSubstr = nullptr;
						if (FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("UPROPERTY"))
							|| FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("UCLASS"))
							|| FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("USTRUCT"))
							|| FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("UENUM"))
							|| FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("UINTERFACE"))
							|| FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("UDELEGATE"))
							|| FindInitialStr(FoundSubstr, TrimmedStrLine, TEXT("UFUNCTION")))
						{
							FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("%s must not be inside preprocessor blocks, except for WITH_EDITORONLY_DATA"), FoundSubstr);
						}

						// Try and determine if this line contains something like a serialize function
						if (TrimmedStrLine.Len() > 0)
						{
							static const FString Str_Void = TEXT("void");
							static const FString Str_Serialize = TEXT("Serialize(");
							static const FString Str_FArchive = TEXT("FArchive");
							static const FString Str_FStructuredArchive = TEXT("FStructuredArchive::FSlot");

							int32 Pos = 0;
							if ((Pos = TrimmedStrLine.Find(Str_Void, ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos)) != -1)
							{
								Pos += Str_Void.Len();
								if ((Pos = TrimmedStrLine.Find(Str_Serialize, ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos)) != -1)
								{
									Pos += Str_Serialize.Len();

									if (((TrimmedStrLine.Find(Str_FArchive, ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos)) != -1) ||
										((TrimmedStrLine.Find(Str_FStructuredArchive, ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos)) != -1))
									{
										FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("'%s' must not be inside preprocessor blocks, except for WITH_EDITORONLY_DATA"), *TrimmedStrLine);
									}
								}
							}
						}
					}
				}

				ClassHeaderTextStrippedOfCppText.Logf(TEXT("%s\r\n"), bShouldKeepLine ? *StrLine : TEXT(""));

				if (bIsDirective)
				{
					bShouldKeepBlockContents = Algo::AllOf(DirectiveStack, &ShouldKeepBlockContents);
					bIsZeroBlock = DirectiveStack.Contains(EBlockDirectiveType::ZeroBlock);
				}
			}
		}
		else if ( FParse::Command(&Str,TEXT("#include")) )
		{
			ClassHeaderTextStrippedOfCppText.Logf( TEXT("%s\r\n"), *StrLine );
		}
		else
		{
			ClassHeaderTextStrippedOfCppText.Logf( TEXT("%s\r\n"), *StrLine );
		}
	}

	// now start over go look for the class

	bool bInComment = false;
	CurrentLine = 0;
	Buffer      = *ClassHeaderTextStrippedOfCppText;

	bool bFoundGeneratedInclude = false;
	bool bGeneratedIncludeRequired = false;

	for (const TCHAR* StartOfLine = Buffer; FParse::Line(&Buffer, StrLine, true); StartOfLine = Buffer)
	{
		CurrentLine++;

		const TCHAR* Str = *StrLine;
		bool bProcess = !bInComment;	// for skipping nested multi-line comments

		int32 BraceCount = 0;
		if( bProcess && FParse::Command(&Str,TEXT("#if")) )
		{
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#include")) )
		{
			// Handle #include directives as if they were 'dependson' keywords.
			const FString& DependsOnHeaderName = Str;

			if (bFoundGeneratedInclude)
			{
				FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("#include found after .generated.h file - the .generated.h file should always be the last #include in a header"));
			}

			bFoundGeneratedInclude = DependsOnHeaderName.Contains(TEXT(".generated.h"));
			if (!bFoundGeneratedInclude && DependsOnHeaderName.Len())
			{
				bool  bIsQuotedInclude = DependsOnHeaderName[0] == '\"';
				int32 HeaderFilenameEnd = DependsOnHeaderName.Find(bIsQuotedInclude ? TEXT("\"") : TEXT(">"), ESearchCase::CaseSensitive, ESearchDir::FromStart, 1);

				if (HeaderFilenameEnd != INDEX_NONE)
				{
					// Include the extension in the name so that we later know where this entry came from.
					SourceFile.GetIncludes().AddUnique(FHeaderProvider(EHeaderProviderSourceType::FileName, FPaths::GetCleanFilename(DependsOnHeaderName.Mid(1, HeaderFilenameEnd - 1))));
				}
			}
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#else")) )
		{
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#elif")) )
		{
		}
		else if ( bProcess && FParse::Command(&Str,TEXT("#endif")) )
		{
		}
		else
		{
			int32 Pos = INDEX_NONE;
			int32 EndPos = INDEX_NONE;
			int32 StrBegin = INDEX_NONE;
			int32 StrEnd = INDEX_NONE;
				
			bool bEscaped = false;
			for ( int32 CharPos = 0; CharPos < StrLine.Len(); CharPos++ )
			{
				if ( bEscaped )
				{
					bEscaped = false;
				}
				else if ( StrLine[CharPos] == TEXT('\\') )
				{
					bEscaped = true;
				}
				else if ( StrLine[CharPos] == TEXT('\"') )
				{
					if ( StrBegin == INDEX_NONE )
					{
						StrBegin = CharPos;
					}
					else
					{
						StrEnd = CharPos;
						break;
					}
				}
			}

			// Find the first '/' and check for '//' or '/*' or '*/'
			if (StrLine.FindChar(TEXT('/'), Pos))
			{
				if (Pos >= 0)
				{
					// Stub out the comments, ignoring anything inside literal strings.
					Pos = StrLine.Find(TEXT("//"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);

					// Check if first slash is end of multiline comment and adjust position if necessary.
					if (Pos > 0 && StrLine[Pos - 1] == TEXT('*'))
					{
						++Pos;
					}

					if (Pos >= 0)
					{
						if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
						{
							StrLine.LeftInline(Pos, false);
						}

						if (StrLine.IsEmpty())
						{
							continue;
						}
					}

					// look for a / * ... * / block, ignoring anything inside literal strings
					Pos = StrLine.Find(TEXT("/*"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
					EndPos = StrLine.Find(TEXT("*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FMath::Max(0, Pos - 1));
					if (Pos >= 0)
					{
						if (StrBegin == INDEX_NONE || Pos < StrBegin || Pos > StrEnd)
						{
							if (EndPos != INDEX_NONE && (EndPos < StrBegin || EndPos > StrEnd))
							{
								StrLine = StrLine.Left(Pos) + StrLine.Mid(EndPos + 2);
								EndPos = INDEX_NONE;
								bInComment = false;
							}
							else
							{
								StrLine.LeftInline(Pos, false);
								bInComment = true;
							}
						}
						bProcess = !bInComment;
					}

					if (EndPos >= 0)
					{
						if (StrBegin == INDEX_NONE || EndPos < StrBegin || EndPos > StrEnd)
						{
							StrLine.MidInline(EndPos + 2, MAX_int32, false);
							bInComment = false;
						}
						bProcess = !bInComment;
					}
				}
			}

			StrLine.TrimStartInline();
			if (!bProcess || StrLine.IsEmpty())
			{
				continue;
			}

			Str = *StrLine;

			// Get class or interface name
			if (const TCHAR* UInterfaceMacroDecl = FCString::Strfind(Str, TEXT("UINTERFACE")))
			{
				if (UInterfaceMacroDecl == FCString::Strspn(Str, TEXT("\t ")) + Str)
				{
					if (UInterfaceMacroDecl[10] != TEXT('('))
					{
						FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("Missing open parenthesis after UINTERFACE"));
					}

					TSharedRef<FUnrealTypeDefinitionInfo> ClassDecl = Parser.ParseClassDeclaration(StartOfLine + (UInterfaceMacroDecl - Str), CurrentLine, true, TEXT("UINTERFACE"));
					bGeneratedIncludeRequired |= !ClassDecl->AsClassChecked().IsNoExport();
					SourceFile.AddDefinedClass(MoveTemp(ClassDecl));
				}
			}

			else if (const TCHAR* UClassMacroDecl = FCString::Strfind(Str, TEXT("UCLASS")))
			{
				if (UClassMacroDecl == FCString::Strspn(Str, TEXT("\t ")) + Str)
				{
					if (UClassMacroDecl[6] != TEXT('('))
					{
						FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("Missing open parenthesis after UCLASS"));
					}

					TSharedRef<FUnrealTypeDefinitionInfo> ClassDecl = Parser.ParseClassDeclaration(StartOfLine + (UClassMacroDecl - Str), CurrentLine, false, TEXT("UCLASS"));
					bGeneratedIncludeRequired |= !ClassDecl->AsClassChecked().IsNoExport();
					SourceFile.AddDefinedClass(MoveTemp(ClassDecl));
				}
			}

			else if (const TCHAR* UEnumMacroDecl = FCString::Strfind(Str, TEXT("UENUM")))
			{
				if (UEnumMacroDecl == FCString::Strspn(Str, TEXT("\t ")) + Str)
				{
					if (UEnumMacroDecl[5] != TEXT('('))
					{
						FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("Missing open parenthesis after UENUM"));
					}

					TSharedRef<FUnrealTypeDefinitionInfo> EnumDecl = Parser.ParseEnumDeclaration(StartOfLine + (UEnumMacroDecl - Str), CurrentLine);
					SourceFile.AddDefinedEnum(MoveTemp(EnumDecl));
				}
			}

			else if (const TCHAR* UDelegateMacroDecl = FCString::Strfind(Str, TEXT("UDELEGATE")))
			{
				if (UDelegateMacroDecl == FCString::Strspn(Str, TEXT("\t ")) + Str)
				{
					if (UDelegateMacroDecl[9] != TEXT('('))
					{
						FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("Missing open parenthesis after UDELEGATE"));
					}
					// We don't preparse the delegates, but we need to make sure the include is there
					bGeneratedIncludeRequired = true;
				}
			}

			else if (const TCHAR* UStructMacroDecl = FCString::Strfind(Str, TEXT("USTRUCT")))
			{
				if (UStructMacroDecl == FCString::Strspn(Str, TEXT("\t ")) + Str)
				{
					if (UStructMacroDecl[7] != TEXT('('))
					{
						FUHTMessage(SourceFile, CurrentLine).Throwf(TEXT("Missing open parenthesis after USTRUCT"));
					}

					TSharedRef<FUnrealTypeDefinitionInfo> StructDecl = Parser.ParseStructDeclaration(StartOfLine + (UStructMacroDecl - Str), CurrentLine);
					bGeneratedIncludeRequired |= !StructDecl->AsScriptStructChecked().HasAnyStructFlags(STRUCT_NoExport);
					SourceFile.AddDefinedStruct(MoveTemp(StructDecl));
				}
			}
		}
	}

	if (bGeneratedIncludeRequired && !bFoundGeneratedInclude)
	{
		Parser.Throwf(TEXT("No #include found for the .generated.h file - the .generated.h file should always be the last #include in a header"));
	}
}

/////////////////////////////////////////////////////
// FHeaderPreParser

TSharedRef<FUnrealTypeDefinitionInfo> FHeaderPreParser::ParseClassDeclaration(const TCHAR* InputText, int32 InLineNumber, bool bClassIsAnInterface, const TCHAR* StartingMatchID)
{
	const TCHAR* ErrorMsg = TEXT("Class declaration");

	ResetParser(InputText, InLineNumber);

	// Require 'UCLASS' or 'UINTERFACE'
	RequireIdentifier(StartingMatchID, ESearchCase::CaseSensitive, ErrorMsg);

	// New-style UCLASS() syntax
	TMap<FName, FString> MetaData;
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, ErrorMsg, MetaData);

	// Require 'class'
	RequireIdentifier(TEXT("class"), ESearchCase::CaseSensitive, ErrorMsg);

	SkipAlignasAndDeprecatedMacroIfNecessary(*this);

	// Read the class name
	FString RequiredAPIMacroIfPresent;
	FString ClassName;
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ ClassName, /*out*/ RequiredAPIMacroIfPresent, StartingMatchID);

	FString ClassNameWithoutPrefixStr = GetClassNameWithPrefixRemoved(ClassName);

	if (ClassNameWithoutPrefixStr.IsEmpty())
	{
		Throwf(TEXT("When compiling class definition for '%s', attempting to strip prefix results in an empty name. Did you leave off a prefix?"), *ClassName);
	}

	// Skip optional final keyword
	MatchIdentifier(TEXT("final"), ESearchCase::CaseSensitive);


	// Create the definition
	TSharedRef<FUnrealClassDefinitionInfo> ClassDef = MakeShareable(new FUnrealClassDefinitionInfo(SourceFile, InLineNumber, MoveTemp(ClassName), FName(ClassNameWithoutPrefixStr, FNAME_Add), bClassIsAnInterface));

	// Parse the inheritance list
	ParseInheritance(TEXT("class"), [this, &ClassNameWithoutPrefixStr, &ClassDef = *ClassDef](const TCHAR* ClassName, bool bIsSuperClass)
	{
		FString ClassNameStr(ClassName);
		SourceFile.AddClassIncludeIfNeeded(*this, ClassNameWithoutPrefixStr, ClassNameStr);
		if (bIsSuperClass)
		{
			ClassDef.GetSuperStructInfo().Name = MoveTemp(ClassNameStr);
		}
		else
		{
			ClassDef.GetBaseStructInfos().Emplace(FUnrealStructDefinitionInfo::FBaseStructInfo{ MoveTemp(ClassNameStr) });
		}
	}
	);

	ClassDef->GetParsedMetaData() = MoveTemp(MetaData);
	ClassDef->ParseClassProperties(MoveTemp(SpecifiersFound), RequiredAPIMacroIfPresent);
	return ClassDef;
}

//
// Compile an enumeration definition.
//
TSharedRef<FUnrealTypeDefinitionInfo> FHeaderPreParser::ParseEnumDeclaration(const TCHAR* InputText, int32 InLineNumber)
{
	const TCHAR* ErrorMsg = TEXT("Enum declaration");

	ResetParser(InputText, InLineNumber);

	// Require 'UCLASS' or 'UINTERFACE'
	RequireIdentifier(TEXT("UENUM"), ESearchCase::CaseSensitive, ErrorMsg);

	// Get the enum specifier list
	FToken EnumToken;
	FMetaData EnumMetaData;
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, ErrorMsg, EnumMetaData);

	// Check enum type. This can be global 'enum', 'namespace' or 'enum class' enums.
	bool bReadEnumName = false;
	UEnum::ECppForm CppForm = UEnum::ECppForm::Regular;
	if (!GetIdentifier(EnumToken))
	{
		Throwf(TEXT("Missing identifier after UENUM()"));
	}

	if (EnumToken.IsValue(TEXT("namespace"), ESearchCase::CaseSensitive))
	{
		CppForm = UEnum::ECppForm::Namespaced;

		SkipDeprecatedMacroIfNecessary(*this);

		bReadEnumName = GetIdentifier(EnumToken);
	}
	else if (EnumToken.IsValue(TEXT("enum"), ESearchCase::CaseSensitive))
	{
		if (!GetIdentifier(EnumToken))
		{
			Throwf(TEXT("Missing identifier after enum"));
		}

		if (EnumToken.IsValue(TEXT("class"), ESearchCase::CaseSensitive) || EnumToken.IsValue(TEXT("struct"), ESearchCase::CaseSensitive))
		{
			CppForm = UEnum::ECppForm::EnumClass;
		}
		else
		{
			// Put whatever token we found back so that we can correctly skip below
			UngetToken(EnumToken);

			CppForm = UEnum::ECppForm::Regular;
		}

		SkipAlignasAndDeprecatedMacroIfNecessary(*this);

		bReadEnumName = GetIdentifier(EnumToken);
	}
	else
	{
		Throwf(TEXT("UENUM() should be followed by \'enum\' or \'namespace\' keywords."));
	}

	// Get enumeration name.
	if (!bReadEnumName)
	{
		Throwf(TEXT("Missing enumeration name"));
	}

	// Read base for enum class
	EUnderlyingEnumType UnderlyingType = EUnderlyingEnumType::uint8;
	if (CppForm == UEnum::ECppForm::EnumClass)
	{
		UnderlyingType = ParseUnderlyingEnumType();
	}

	TSharedRef<FUnrealEnumDefinitionInfo> EnumDef = MakeShareable(new FUnrealEnumDefinitionInfo(SourceFile, InLineNumber, FString(EnumToken.Value), FName(EnumToken.Value, FNAME_Add), CppForm, UnderlyingType));
	return EnumDef;
}

//
// Compile an structure definition.
//
TSharedRef<FUnrealTypeDefinitionInfo> FHeaderPreParser::ParseStructDeclaration(const TCHAR* InputText, int32 InLineNumber)
{
	const TCHAR* ErrorMsg = TEXT("Struct declaration");

	ResetParser(InputText, InLineNumber);

	// Require 'UCLASS' or 'UINTERFACE'
	RequireIdentifier(TEXT("USTRUCT"), ESearchCase::CaseSensitive, ErrorMsg);

	// Get the structure specifier list
	FToken StructToken;
	FMetaData StructMetaData;
	TArray<FPropertySpecifier> SpecifiersFound;
	ReadSpecifierSetInsideMacro(SpecifiersFound, ErrorMsg, StructMetaData);

	// Consume the struct keyword
	RequireIdentifier(TEXT("struct"), ESearchCase::CaseSensitive, TEXT("Struct declaration specifier"));

	SkipAlignasAndDeprecatedMacroIfNecessary(*this);

	// The struct name as parsed in script and stripped of it's prefix
	FString StructNameInScript;

	// The required API module for this struct, if any
	FString RequiredAPIMacroIfPresent;

	// Read the struct name
	ParseNameWithPotentialAPIMacroPrefix(/*out*/ StructNameInScript, /*out*/ RequiredAPIMacroIfPresent, TEXT("struct"));

	// The struct name stripped of it's prefix
	FString StructNameStripped = GetClassNameWithPrefixRemoved(StructNameInScript);

	if (StructNameStripped.IsEmpty())
	{
		Throwf(TEXT("When compiling struct definition for '%s', attempting to strip prefix results in an empty name. Did you leave off a prefix?"), *StructNameInScript);
	}

	// Skip optional final keyword
	MatchIdentifier(TEXT("final"), ESearchCase::CaseSensitive);

	// Create the structure definition
	TSharedRef<FUnrealScriptStructDefinitionInfo> StructDef = MakeShareable(new FUnrealScriptStructDefinitionInfo(SourceFile, InLineNumber, *StructNameInScript, FName(StructNameStripped, FNAME_Add)));

	// Parse the inheritance list
	ParseInheritance(TEXT("struct"), [this, &StructNameStripped, &StructDef = *StructDef](const TCHAR* StructName, bool bIsSuperClass)
	{
		FString StructNameStr(StructName);
		SourceFile.AddScriptStructIncludeIfNeeded(*this, StructNameStripped, StructNameStr);
		if (bIsSuperClass)
		{
			StructDef.GetSuperStructInfo().Name = MoveTemp(StructNameStr);
		}
		else
		{
			StructDef.GetBaseStructInfos().Emplace(FUnrealStructDefinitionInfo::FBaseStructInfo{ MoveTemp(StructNameStr) });
		}
	}
	);

	// Initialize the structure flags
	StructDef->SetStructFlags(STRUCT_Native);

	// Record that this struct is RequiredAPI if the CORE_API style macro was present
	if (!RequiredAPIMacroIfPresent.IsEmpty())
	{
		StructDef->SetStructFlags(STRUCT_RequiredAPI);
	}

	// Process the list of specifiers
	for (const FPropertySpecifier& Specifier : SpecifiersFound)
	{
		switch ((EStructSpecifier)Algo::FindSortedStringCaseInsensitive(*Specifier.Key, GStructSpecifierStrings))
		{
		default:
		{
			Throwf(TEXT("Unknown struct specifier '%s'"), *Specifier.Key);
		}
		break;

		case EStructSpecifier::NoExport:
		{
			StructDef->SetStructFlags(STRUCT_NoExport);
			StructDef->ClearStructFlags(STRUCT_Native);
		}
		break;

		case EStructSpecifier::Atomic:
		{
			StructDef->SetStructFlags(STRUCT_Atomic);
		}
		break;

		case EStructSpecifier::Immutable:
		{
			StructDef->SetStructFlags(STRUCT_Immutable);
			StructDef->SetStructFlags(STRUCT_Atomic);
		}
		break;

		case EStructSpecifier::HasDefaults:
			if (!SourceFile.IsNoExportTypes())
			{
				LogError(TEXT("The 'HasDefaults' struct specifier is only valid in the NoExportTypes.h file"));
			}
			break;

		case EStructSpecifier::HasNoOpConstructor:
			if (!SourceFile.IsNoExportTypes())
			{
				LogError(TEXT("The 'HasNoOpConstructor' struct specifier is only valid in the NoExportTypes.h file"));
			}
			break;

		case EStructSpecifier::IsAlwaysAccessible:
			if (!SourceFile.IsNoExportTypes())
			{
				LogError(TEXT("The 'IsAlwaysAccessible' struct specifier is only valid in the NoExportTypes.h file"));
			}
			break;

		case EStructSpecifier::IsCoreType:
			if (!SourceFile.IsNoExportTypes())
			{
				LogError(TEXT("The 'IsCoreType' struct specifier is only valid in the NoExportTypes.h file"));
			}
			break;
		}
	}

	// Check to make sure the syntactic native prefix was set-up correctly.
	// If this check results in a false positive, it will be flagged as an identifier failure.
	FString DeclaredPrefix = GetClassPrefix(StructNameInScript);
	if (DeclaredPrefix == StructDef->GetPrefixCPP() || DeclaredPrefix == TEXT("T"))
	{
		// Found a prefix, do a basic check to see if it's valid
		const TCHAR* ExpectedPrefixCPP = UHTConfig.StructsWithTPrefix.Contains(StructNameStripped) ? TEXT("T") : StructDef->GetPrefixCPP();
		FString ExpectedStructName = FString::Printf(TEXT("%s%s"), ExpectedPrefixCPP, *StructNameStripped);
		if (StructNameInScript != ExpectedStructName)
		{
			StructDef->Throwf(TEXT("Struct '%s' has an invalid Unreal prefix, expecting '%s'"), *StructNameInScript, *ExpectedStructName);
		}
	}
	else
	{
		const TCHAR* ExpectedPrefixCPP = UHTConfig.StructsWithTPrefix.Contains(StructNameInScript) ? TEXT("T") : StructDef->GetPrefixCPP();
		FString ExpectedStructName = FString::Printf(TEXT("%s%s"), ExpectedPrefixCPP, *StructNameInScript);
		StructDef->Throwf(TEXT("Struct '%s' is missing a valid Unreal prefix, expecting '%s'"), *StructNameInScript, *ExpectedStructName);
	}

	return StructDef;
}

bool FHeaderParser::TryToMatchConstructorParameterList(FToken Token)
{
	FToken PotentialParenthesisToken;
	if (!GetToken(PotentialParenthesisToken))
	{
		return false;
	}

	if (!PotentialParenthesisToken.IsSymbol(TEXT('(')))
	{
		UngetToken(PotentialParenthesisToken);
		return false;
	}

	FUnrealClassDefinitionInfo& ClassDef = GetCurrentClassDef();

	bool bOICtor = false;
	bool bVTCtor = false;

	if (!ClassDef.IsDefaultConstructorDeclared() && MatchSymbol(TEXT(')')))
	{
		ClassDef.MarkDefaultConstructorDeclared();
	}
	else if (!ClassDef.IsObjectInitializerConstructorDeclared()
		|| !ClassDef.IsCustomVTableHelperConstructorDeclared())
	{
		FToken ObjectInitializerParamParsingToken;

		bool bIsConst = false;
		bool bIsRef = false;
		int32 ParenthesesNestingLevel = 1;

		while (ParenthesesNestingLevel && GetToken(ObjectInitializerParamParsingToken))
		{
			// Template instantiation or additional parameter excludes ObjectInitializer constructor.
			if (ObjectInitializerParamParsingToken.IsSymbol(TEXT(',')) || ObjectInitializerParamParsingToken.IsSymbol(TEXT('<')))
			{
				bOICtor = false;
				bVTCtor = false;
				break;
			}

			if (ObjectInitializerParamParsingToken.IsSymbol(TEXT('(')))
			{
				ParenthesesNestingLevel++;
				continue;
			}

			if (ObjectInitializerParamParsingToken.IsSymbol(TEXT(')')))
			{
				ParenthesesNestingLevel--;
				continue;
			}

			if (ObjectInitializerParamParsingToken.IsIdentifier(TEXT("const"), ESearchCase::CaseSensitive))
			{
				bIsConst = true;
				continue;
			}

			if (ObjectInitializerParamParsingToken.IsSymbol(TEXT('&')))
			{
				bIsRef = true;
				continue;
			}

			if (ObjectInitializerParamParsingToken.IsIdentifier(TEXT("FObjectInitializer"), ESearchCase::CaseSensitive)
				|| ObjectInitializerParamParsingToken.IsIdentifier(TEXT("FPostConstructInitializeProperties"), ESearchCase::CaseSensitive) // Deprecated, but left here, so it won't break legacy code.
				)
			{
				bOICtor = true;
			}

			if (ObjectInitializerParamParsingToken.IsIdentifier(TEXT("FVTableHelper"), ESearchCase::CaseSensitive))
			{
				bVTCtor = true;
			}
		}

		// Parse until finish.
		while (ParenthesesNestingLevel && GetToken(ObjectInitializerParamParsingToken))
		{
			if (ObjectInitializerParamParsingToken.IsSymbol(TEXT('(')))
			{
				ParenthesesNestingLevel++;
				continue;
			}

			if (ObjectInitializerParamParsingToken.IsSymbol(TEXT(')')))
			{
				ParenthesesNestingLevel--;
				continue;
			}
		}

		if (bOICtor && bIsRef && bIsConst)
		{
			ClassDef.MarkObjectInitializerConstructorDeclared();
		}
		if (bVTCtor && bIsRef)
		{
			ClassDef.MarkCustomVTableHelperConstructorDeclared();
		}
	}

	if (!bVTCtor)
	{
		ClassDef.MarkConstructorDeclared();
	}

	// Optionally match semicolon.
	if (!MatchSymbol(TEXT(';')))
	{
		// If not matched a semicolon, this is inline constructor definition. We have to skip it.
		UngetToken(Token); // Resets input stream to the initial token.
		GetToken(Token); // Re-gets the initial token to start constructor definition skip.
		return SkipDeclaration(Token);
	}

	return true;
}

void FHeaderParser::CompileVersionDeclaration(FUnrealStructDefinitionInfo& StructDef)
{
	// Do nothing if we're at the end of file.
	FToken Token;
	if (!GetToken(Token, true, ESymbolParseOption::Normal))
	{
		return;
	}

	// Default version based on config file.
	EGeneratedCodeVersion Version = UHTConfig.DefaultGeneratedCodeVersion;

	// Overwrite with module-specific value if one was specified.
	if (PackageDef.GetModule().GeneratedCodeVersion != EGeneratedCodeVersion::None)
	{
		Version = PackageDef.GetModule().GeneratedCodeVersion;
	}

	if (Token.IsSymbol(TEXT(')')))
	{
		StructDef.SetGeneratedCodeVersion(Version);
		UngetToken(Token);
		return;
	}

	// Overwrite with version specified by macro.
	Version = ToGeneratedCodeVersion(Token.Value);
	StructDef.SetGeneratedCodeVersion(Version);
}

void FHeaderParser::ResetClassData()
{
	FUnrealClassDefinitionInfo& CurrentClassDef = GetCurrentClassDef();

	check(CurrentClassDef.GetClassWithin() == nullptr);

	// Set class flags and within.
	CurrentClassDef.ClearClassFlags(CLASS_RecompilerClear);

	if (FUnrealClassDefinitionInfo* SuperClassDef = CurrentClassDef.GetSuperClass())
	{
		CurrentClassDef.SetClassFlags(SuperClassDef->GetClassFlags() & CurrentClassDef.GetInheritClassFlags());
		CurrentClassDef.SetClassConfigName(SuperClassDef->GetClassConfigName());

		check(SuperClassDef->GetClassWithin());
		CurrentClassDef.SetClassWithin(SuperClassDef->GetClassWithin());

		// Copy special categories from parent
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_HideCategories))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_HideCategories, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_HideCategories));
		}
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_ShowCategories))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_ShowCategories, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_ShowCategories));
		}
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_SparseClassDataTypes))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_SparseClassDataTypes, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_SparseClassDataTypes));
		}
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_HideFunctions))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_HideFunctions, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_HideFunctions));
		}
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_AutoExpandCategories))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_AutoExpandCategories, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_AutoExpandCategories));
		}
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_AutoCollapseCategories))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_AutoCollapseCategories, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_AutoCollapseCategories));
		}
		if (SuperClassDef->HasMetaData(FHeaderParserNames::NAME_PrioritizeCategories))
		{
			CurrentClassDef.SetMetaData(FHeaderParserNames::NAME_PrioritizeCategories, *SuperClassDef->GetMetaData(FHeaderParserNames::NAME_PrioritizeCategories));
		}
	}
	else
	{
		CurrentClassDef.SetClassWithin(GUObjectDef);
	}

	check(CurrentClassDef.GetClassWithin());
}

void FHeaderParser::PostPopNestClass(FUnrealClassDefinitionInfo& CurrentClassDef)
{
	// Validate all the rep notify events here, to make sure they're implemented
	VerifyPropertyMarkups(CurrentClassDef);

	VerifyFunctionsMarkups(CurrentClassDef);

	// Iterate over all the interfaces we claim to implement
	for (const FUnrealStructDefinitionInfo::FBaseStructInfo& BaseClassInfo : CurrentClassDef.GetBaseStructInfos())
	{

		// Skip unknown base classes or things that aren't interfaces
		if (BaseClassInfo.Struct == nullptr)
		{
			continue;
		}
		FUnrealClassDefinitionInfo* InterfaceDef = UHTCast<FUnrealClassDefinitionInfo>(BaseClassInfo.Struct);
		if (InterfaceDef == nullptr || !InterfaceDef->IsInterface())
		{
			continue;
		}

		// And their super-classes
		for (; InterfaceDef; InterfaceDef = InterfaceDef->GetSuperClass())
		{
			// If this interface is a common ancestor, skip it
			if (CurrentClassDef.IsChildOf(*InterfaceDef))
			{
				continue;
			}

			const bool bCanImplementInBlueprints = InterfaceDef->GetBoolMetaData(NAME_IsBlueprintBase);
			const bool bCannotImplementInBlueprints = (!bCanImplementInBlueprints && InterfaceDef->HasMetaData(NAME_IsBlueprintBase))
				|| InterfaceDef->HasMetaData(NAME_CannotImplementInterfaceInBlueprint);

			// So iterate over all functions this interface declares
			for (FUnrealFunctionDefinitionInfo* InterfaceFunctionDef : TUHTFieldRange<FUnrealFunctionDefinitionInfo>(*InterfaceDef, EFieldIteratorFlags::ExcludeSuper))
			{
				bool bImplemented = false;

				// And try to find one that matches
				for (FUnrealFunctionDefinitionInfo* ClassFunctionDef : TUHTFieldRange<FUnrealFunctionDefinitionInfo>(CurrentClassDef))
				{
					if (ClassFunctionDef->GetFName() != InterfaceFunctionDef->GetFName())
					{
						continue;
					}

					if (InterfaceFunctionDef->HasAnyFunctionFlags(FUNC_Event) && !ClassFunctionDef->HasAnyFunctionFlags(FUNC_Event))
					{
						Throwf(TEXT("Implementation of function '%s::%s' must be declared as 'event' to match declaration in interface '%s'"), *ClassFunctionDef->GetOuter()->GetName(), *ClassFunctionDef->GetName(), *InterfaceDef->GetName());
					}

					if (InterfaceFunctionDef->HasAnyFunctionFlags(FUNC_Delegate) && !ClassFunctionDef->HasAnyFunctionFlags(FUNC_Delegate))
					{
						Throwf(TEXT("Implementation of function '%s::%s' must be declared as 'delegate' to match declaration in interface '%s'"), *ClassFunctionDef->GetOuter()->GetName(), *ClassFunctionDef->GetName(), *InterfaceDef->GetName());
					}

					// Making sure all the parameters match up correctly
					bImplemented = true;

					if (ClassFunctionDef->GetProperties().Num() != InterfaceFunctionDef->GetProperties().Num())
					{
						Throwf(TEXT("Implementation of function '%s' conflicts with interface '%s' - different number of parameters (%i/%i)"), *InterfaceFunctionDef->GetName(), *InterfaceDef->GetName(), ClassFunctionDef->GetProperties().Num(), InterfaceFunctionDef->GetProperties().Num());
					}

					for (int32 Index = 0, End = ClassFunctionDef->GetProperties().Num(); Index != End; ++Index)
					{
						FUnrealPropertyDefinitionInfo& InterfaceParamDef = *InterfaceFunctionDef->GetProperties()[Index];
						FUnrealPropertyDefinitionInfo& ClassParamDef = *ClassFunctionDef->GetProperties()[Index];
						if (!InterfaceParamDef.MatchesType(ClassParamDef, true))
						{
							if (InterfaceParamDef.HasAnyPropertyFlags(CPF_ReturnParm))
							{
								Throwf(TEXT("Implementation of function '%s' conflicts only by return type with interface '%s'"), *InterfaceFunctionDef->GetName(), *InterfaceDef->GetName());
							}
							else
							{
								Throwf(TEXT("Implementation of function '%s' conflicts with interface '%s' - parameter %i '%s'"), *InterfaceFunctionDef->GetName(), *InterfaceDef->GetName(), End, *InterfaceParamDef.GetName());
							}
						}
					}
				}

				// Delegate signature functions are simple stubs and aren't required to be implemented (they are not callable)
				if (InterfaceFunctionDef->HasAnyFunctionFlags(FUNC_Delegate))
				{
					bImplemented = true;
				}

				// Verify that if this has blueprint-callable functions that are not implementable events, we've implemented them as a UFunction in the target class
				// This is only relevant for bp-implementable interfaces, for native interfaces the interface-defined function is sufficient
				if (!bImplemented
					&& InterfaceFunctionDef->HasAnyFunctionFlags(FUNC_BlueprintCallable)
					&& !InterfaceFunctionDef->HasAnyFunctionFlags(FUNC_BlueprintEvent)
					&& !bCannotImplementInBlueprints)
				{
					Throwf(TEXT("Missing UFunction implementation of function '%s' from interface '%s'.  This function needs a UFUNCTION() declaration."), *InterfaceFunctionDef->GetName(), *InterfaceDef->GetName());
				}
			}
		}
	}
}

void FHeaderParser::PostPopFunctionDeclaration(FUnrealFunctionDefinitionInfo& PoppedFunctionDef)
{
	//@TODO: UCREMOVAL: Move this code to occur at delegate var declaration, and force delegates to be declared before variables that use them
	if (!GetCurrentScope()->IsFileScope() && GetCurrentClassDef().ContainsDelegates())
	{
		// now validate all delegate variables declared in the class
		TMap<FName, FUnrealFunctionDefinitionInfo*> DelegateCache;
		FixupDelegateProperties(PoppedFunctionDef, *GetCurrentScope(), DelegateCache);
	}
}

void FHeaderParser::PostPopNestInterface(FUnrealClassDefinitionInfo& CurrentInterfaceDef)
{
	if (CurrentInterfaceDef.ContainsDelegates())
	{
		TMap<FName, FUnrealFunctionDefinitionInfo*> DelegateCache;
		FixupDelegateProperties(CurrentInterfaceDef, *CurrentInterfaceDef.GetScope(), DelegateCache);
	}
}

FDocumentationPolicy FHeaderParser::GetDocumentationPolicyFromName(const FUHTMessageProvider& Context, const FString& PolicyName)
{
	FDocumentationPolicy DocumentationPolicy;
	if (FCString::Strcmp(*PolicyName, TEXT("Strict")) == 0)
	{
		DocumentationPolicy.bClassOrStructCommentRequired = true;
		DocumentationPolicy.bFunctionToolTipsRequired = true;
		DocumentationPolicy.bMemberToolTipsRequired = true;
		DocumentationPolicy.bParameterToolTipsRequired = true;
		DocumentationPolicy.bFloatRangesRequired = true;
	}
	else
	{
		Context.Throwf(TEXT("Documentation Policy '%s' not yet supported"), *PolicyName);
	}
	return DocumentationPolicy;
}


FDocumentationPolicy FHeaderParser::GetDocumentationPolicyForStruct(FUnrealStructDefinitionInfo& StructDef)
{
	SCOPE_SECONDS_COUNTER_UHT(DocumentationPolicy);

	FDocumentationPolicy DocumentationPolicy;
	FString DocumentationPolicyName;
	if (StructDef.GetStringMetaDataHierarchical(NAME_DocumentationPolicy, &DocumentationPolicyName))
	{
		DocumentationPolicy = GetDocumentationPolicyFromName(StructDef, DocumentationPolicyName);
	}
	return DocumentationPolicy;
}

void FHeaderParser::CheckDocumentationPolicyForEnum(FUnrealEnumDefinitionInfo& EnumDef, const TMap<FName, FString>& MetaData, const TArray<TMap<FName, FString>>& Entries)
{
	SCOPE_SECONDS_COUNTER_UHT(DocumentationPolicy);

	const FString* DocumentationPolicyName = MetaData.Find(NAME_DocumentationPolicy);
	if (DocumentationPolicyName == nullptr)
	{
		return;
	}

	check(!DocumentationPolicyName->IsEmpty());

	FDocumentationPolicy DocumentationPolicy = GetDocumentationPolicyFromName(EnumDef, *DocumentationPolicyName);
	if (DocumentationPolicy.bClassOrStructCommentRequired)
	{
		const FString* EnumToolTip = MetaData.Find(NAME_ToolTip);
		if (EnumToolTip == nullptr)
		{
			LogError(TEXT("Enum '%s' does not provide a tooltip / comment (DocumentationPolicy)."), *EnumDef.GetName());
		}
	}

	TMap<FString, FString> ToolTipToEntry;
	for (const TMap<FName, FString>& Entry : Entries)
	{
		const FString* EntryName = Entry.Find(NAME_Name);
		if (EntryName == nullptr)
		{
			continue;
		}

		const FString* ToolTip = Entry.Find(NAME_ToolTip);
		if (ToolTip == nullptr)
		{
			LogError(TEXT("Enum entry '%s::%s' does not provide a tooltip / comment (DocumentationPolicy)."), *EnumDef.GetName(), *(*EntryName));
			continue;
		}

		const FString* ExistingEntry = ToolTipToEntry.Find(*ToolTip);
		if (ExistingEntry != nullptr)
		{
			LogError(TEXT("Enum entries '%s::%s' and '%s::%s' have identical tooltips / comments (DocumentationPolicy)."), *EnumDef.GetName(), *(*ExistingEntry), *EnumDef.GetName(), *(*EntryName));
		}
		ToolTipToEntry.Add(*ToolTip, *EntryName);
	}
}

void FHeaderParser::CheckDocumentationPolicyForStruct(FUnrealStructDefinitionInfo& StructDef)
{
	SCOPE_SECONDS_COUNTER_UHT(DocumentationPolicy);

	FDocumentationPolicy DocumentationPolicy = GetDocumentationPolicyForStruct(StructDef);
	if (DocumentationPolicy.bClassOrStructCommentRequired)
	{
		FString ClassTooltip;
		if (const FString* ClassTooltipPtr = StructDef.FindMetaData(NAME_ToolTip))
		{
			ClassTooltip = *ClassTooltipPtr;
		}

		if (ClassTooltip.IsEmpty() || ClassTooltip.Equals(StructDef.GetName()))
		{
			LogError(TEXT("Struct '%s' does not provide a tooltip / comment (DocumentationPolicy)."), *StructDef.GetName());
		}
	}

	if (DocumentationPolicy.bMemberToolTipsRequired)
	{
		TMap<FString, FName> ToolTipToPropertyName;
		for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : StructDef.GetProperties())
		{
			FString ToolTip = PropertyDef->GetToolTipText().ToString();
			if (ToolTip.IsEmpty() || ToolTip.Equals(PropertyDef->GetDisplayNameText().ToString()))
			{
				LogError(TEXT("Property '%s::%s' does not provide a tooltip / comment (DocumentationPolicy)."), *StructDef.GetName(), *PropertyDef->GetName());
				continue;
			}
			const FName* ExistingPropertyName = ToolTipToPropertyName.Find(ToolTip);
			if (ExistingPropertyName != nullptr)
			{
				LogError(TEXT("Property '%s::%s' and '%s::%s' are using identical tooltips (DocumentationPolicy)."), *StructDef.GetName(), *ExistingPropertyName->ToString(), *StructDef.GetName(), *PropertyDef->GetName());
			}
			ToolTipToPropertyName.Add(MoveTemp(ToolTip), PropertyDef->GetFName());
		}
	}

	if (DocumentationPolicy.bFloatRangesRequired)
	{
		for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : StructDef.GetProperties())
		{
			if(DoesCPPTypeRequireDocumentation(PropertyDef->GetCPPType()))
			{
				const FString& UIMin = PropertyDef->GetMetaData(NAME_UIMin);
				const FString& UIMax = PropertyDef->GetMetaData(NAME_UIMax);

				if(!CheckUIMinMaxRangeFromMetaData(UIMin, UIMax))
				{
					LogError(TEXT("Property '%s::%s' does not provide a valid UIMin / UIMax (DocumentationPolicy)."), *StructDef.GetName(), *PropertyDef->GetName());
				}
			}
		}
	}

	// also compare all tooltips to see if they are unique
	if (DocumentationPolicy.bFunctionToolTipsRequired)
	{
		if (FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(StructDef))
		{
			TMap<FString, FName> ToolTipToFunc;
			for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : ClassDef->GetFunctions())
			{
				FString ToolTip = FunctionDef->GetToolTipText().ToString();
				if (ToolTip.IsEmpty())
				{
					LogError(TEXT("Function '%s::%s' does not provide a tooltip / comment (DocumentationPolicy)."), *ClassDef->GetName(), *FunctionDef->GetName());
					continue;
				}
				const FName* ExistingFuncName = ToolTipToFunc.Find(ToolTip);
				if (ExistingFuncName != nullptr)
				{
					LogError(TEXT("Functions '%s::%s' and '%s::%s' uses identical tooltips / comments (DocumentationPolicy)."), *ClassDef->GetName(), *(*ExistingFuncName).ToString(), *ClassDef->GetName(), *FunctionDef->GetName());
				}
				ToolTipToFunc.Add(MoveTemp(ToolTip), FunctionDef->GetFName());
			}
		}
	}
}

bool FHeaderParser::DoesCPPTypeRequireDocumentation(const FString& CPPType)
{
	return PropertyCPPTypesRequiringUIRanges.Contains(CPPType);
}

// Validates the documentation for a given method
void FHeaderParser::CheckDocumentationPolicyForFunc(FUnrealClassDefinitionInfo& ClassDef, FUnrealFunctionDefinitionInfo& FunctionDef)
{
	SCOPE_SECONDS_COUNTER_UHT(DocumentationPolicy);

	FDocumentationPolicy DocumentationPolicy = GetDocumentationPolicyForStruct(ClassDef);
	if (DocumentationPolicy.bFunctionToolTipsRequired)
	{
		const FString* FunctionTooltip = FunctionDef.FindMetaData(NAME_ToolTip);
		if (FunctionTooltip == nullptr)
		{
			LogError(TEXT("Function '%s::%s' does not provide a tooltip / comment (DocumentationPolicy)."), *ClassDef.GetName(), *FunctionDef.GetName());
		}
	}

	if (DocumentationPolicy.bParameterToolTipsRequired)
	{
		const FString* FunctionComment = FunctionDef.FindMetaData(NAME_Comment);
		if (FunctionComment == nullptr)
		{
			LogError(TEXT("Function '%s::%s' does not provide a comment (DocumentationPolicy)."), *ClassDef.GetName(), *FunctionDef.GetName());
			return;
		}
		
		TMap<FName, FString> ParamToolTips = GetParameterToolTipsFromFunctionComment(*FunctionComment);
		bool HasAnyParamToolTips = ParamToolTips.Num() > 0;
		if (ParamToolTips.Num() == 0)
		{
			const FString* ReturnValueToolTip = ParamToolTips.Find(NAME_ReturnValue);
			if (ReturnValueToolTip != nullptr)
			{
				HasAnyParamToolTips = false;
			}
		}

		// only apply the validation for parameter tooltips if a function has any @param statements at all.
		if (HasAnyParamToolTips)
		{
			// ensure each parameter has a tooltip
			TSet<FName> ExistingFields;
			for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : FunctionDef.GetProperties())
			{
				FName ParamName = PropertyDef->GetFName();
				if (ParamName == NAME_ReturnValue)
				{
					continue;
				}
				const FString* ParamToolTip = ParamToolTips.Find(ParamName);
				if (ParamToolTip == nullptr)
				{
					LogError(TEXT("Function '%s::%s' doesn't provide a tooltip for parameter '%s' (DocumentationPolicy)."), *ClassDef.GetName(), *FunctionDef.GetName(), *ParamName.ToString());
				}
				ExistingFields.Add(ParamName);
			}

			// ensure we don't have parameter tooltips for parameters that don't exist
			for (TPair<FName, FString>& Pair : ParamToolTips)
			{
				const FName& ParamName = Pair.Key;
				if (ParamName == NAME_ReturnValue)
				{
					continue;
				}
				if (!ExistingFields.Contains(ParamName))
				{
					LogError(TEXT("Function '%s::%s' provides a tooltip for an unknown parameter '%s' (DocumentationPolicy)."), *ClassDef.GetName(), *FunctionDef.GetName(), *Pair.Key.ToString());
				}
			}

			// check for duplicate tooltips
			TMap<FString, FName> ToolTipToParam;
			for (TPair<FName, FString>& Pair : ParamToolTips)
			{
				const FName& ParamName = Pair.Key;
				if (ParamName == NAME_ReturnValue)
				{
					continue;
				}
				const FName* ExistingParam = ToolTipToParam.Find(Pair.Value);
				if (ExistingParam != nullptr)
				{
					LogError(TEXT("Function '%s::%s' uses identical tooltips for parameters '%s' and '%s' (DocumentationPolicy)."), *ClassDef.GetName(), *FunctionDef.GetName(), *ExistingParam->ToString(), *Pair.Key.ToString());
				}
				ToolTipToParam.Add(MoveTemp(Pair.Value), Pair.Key);
			}
		}
	}
}

bool FHeaderParser::IsReservedTypeName(const FString& TypeName)
{
	for(const FString& ReservedName : ReservedTypeNames)
	{
		if(TypeName == ReservedName)
		{
			return true;
		}
	}
	return false;
}

bool FHeaderParser::CheckUIMinMaxRangeFromMetaData(const FString& UIMin, const FString& UIMax)
{
	if (UIMin.IsEmpty() || UIMax.IsEmpty())
	{
		return false;
	}

	double UIMinValue = FCString::Atod(*UIMin);
	double UIMaxValue = FCString::Atod(*UIMax);
	if (UIMin > UIMax) // note that we actually allow UIMin == UIMax to disable the range manually.
	{
		return false;
	}

	return true;
}

void FHeaderParser::ConditionalLogPointerUsage(EPointerMemberBehavior PointerMemberBehavior, const TCHAR* PointerTypeDesc, FString&& PointerTypeDecl, const TCHAR* AlternativeTypeDesc)
{
	switch (PointerMemberBehavior)
	{
		case EPointerMemberBehavior::Disallow:
		{
			if (AlternativeTypeDesc)
			{
				LogError(TEXT("%s usage in member declaration detected [[[%s]]].  This is disallowed for the target/module, consider %s as an alternative."), PointerTypeDesc, *PointerTypeDecl, AlternativeTypeDesc);
			}
			else
			{
				LogError(TEXT("%s usage in member declaration detected [[[%s]]]."), PointerTypeDesc, *PointerTypeDecl);
			}
		}
		break;
		case EPointerMemberBehavior::AllowAndLog:
		{
			if (AlternativeTypeDesc)
			{
				UE_LOG(LogCompile, Log, TEXT("%s(%d): %s usage in member declaration detected [[[%s]]].  Consider %s as an alternative."), *Filename, InputLine, PointerTypeDesc, *PointerTypeDecl, AlternativeTypeDesc);
			}
			else
			{
				UE_LOG(LogCompile, Log, TEXT("%s(%d): usage in member declaration detected [[[%s]]]."), *Filename, InputLine, PointerTypeDesc, *PointerTypeDecl);
			}
		}
		break;
		case EPointerMemberBehavior::AllowSilently:
		{
			// Do nothing
		}
		break;
		default:
		{
			checkf(false, TEXT("Unhandled case"));
		}
		break;
	}
}

FUnrealFunctionDefinitionInfo& FHeaderParser::CreateFunction(const TCHAR* FuncName, FFuncInfo&& FuncInfo, EFunctionType InFunctionType) const
{
	FScope* CurrentScope = GetCurrentScope();
	FUnrealObjectDefinitionInfo& OuterDef = IsInAClass() ? static_cast<FUnrealObjectDefinitionInfo&>(GetCurrentClassDef()) : SourceFile.GetPackageDef();


	// Allocate local property frame, push nesting level and verify
	// uniqueness at this scope level.
	{
		auto TypeIterator = CurrentScope->GetTypeIterator();
		while (TypeIterator.MoveNext())
		{
			FUnrealFieldDefinitionInfo* Type = *TypeIterator;
			if (Type->GetFName() == FuncName)
			{
				Throwf(TEXT("'%s' conflicts with '%s'"), FuncName, *Type->GetFullName());
			}
		}
	}

	TSharedRef<FUnrealFunctionDefinitionInfo> FuncDefRef = MakeShared<FUnrealFunctionDefinitionInfo>(SourceFile, InputLine, FString(FuncName), FName(FuncName, FNAME_Add), OuterDef, MoveTemp(FuncInfo), InFunctionType);
	FUnrealFunctionDefinitionInfo& FuncDef = *FuncDefRef;
	FuncDef.GetFunctionData().SetFunctionNames(FuncDef);

	CurrentScope->AddType(FuncDef);

	if (!CurrentScope->IsFileScope())
	{
		FUnrealStructDefinitionInfo& StructDef = ((FStructScope*)CurrentScope)->GetStructDef();
		StructDef.AddFunction(FuncDefRef);
	}
	else
	{
		SourceFile.AddDefinedFunction(FuncDefRef);
		FScopeLock Lock(&GlobalDelegatesLock);
		GlobalDelegates.Add(FuncDef.GetName(), &FuncDef);
	}

	return FuncDef;
}

FRecordTokens::FRecordTokens(FHeaderParser& InParser, FUnrealStructDefinitionInfo* InStructDef, FToken* InToken)
	: Parser(InParser)
	, StructDef(InStructDef)
	, CurrentCompilerDirective(Parser.GetCurrentCompilerDirective())
{
	if (StructDef != nullptr)
	{
		Parser.bRecordTokens = true;
		if (InToken != nullptr)
		{
			Parser.RecordedTokens.Push(*InToken);
		}
	}
}

FRecordTokens::~FRecordTokens()
{
	Stop();
}

bool FRecordTokens::Stop()
{
	if (StructDef != nullptr)
	{
		Parser.bRecordTokens = false;
		StructDef->AddDeclaration(CurrentCompilerDirective, MoveTemp(Parser.RecordedTokens));
		StructDef = nullptr;
		return true;
	}
	return false;
}
