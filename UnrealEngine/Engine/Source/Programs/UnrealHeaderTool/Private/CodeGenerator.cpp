// Copyright Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UnrealHeaderTool.h"
#include "Misc/AssertionMacros.h"
#include "HAL/PlatformProcess.h"
#include "Templates/UnrealTemplate.h"
#include "Math/UnrealMathUtility.h"
#include "Containers/UnrealString.h"
#include "UObject/NameTypes.h"
#include "Logging/LogMacros.h"
#include "CoreGlobals.h"
#include "HAL/FileManager.h"
#include "Misc/Parse.h"
#include "Misc/CoreMisc.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Delegates/Delegate.h"
#include "Misc/Guid.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FeedbackContext.h"
#include "Misc/OutputDeviceNull.h"
#include "UObject/ClassTree.h"
#include "UObject/Script.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "UObject/Interface.h"
#include "UObject/UnrealType.h"
#include "UObject/TextProperty.h"
#include "UObject/FieldPathProperty.h"
#include "Misc/PackageName.h"
#include "UnrealHeaderToolGlobals.h"

#include "Exceptions.h"
#include "Scope.h"
#include "HeaderProvider.h"
#include "GeneratedCodeVersion.h"
#include "UnrealSourceFile.h"
#include "ParserHelper.h"
#include "EngineAPI.h"
#include "ClassMaps.h"
#include "NativeClassExporter.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "HeaderParser.h"
#include "IScriptGeneratorPluginInterface.h"
#include "Manifest.h"
#include "StringUtils.h"
#include "Features/IModularFeatures.h"
#include "Algo/Copy.h"
#include "Algo/Sort.h"
#include "Algo/Reverse.h"
#include "Async/Async.h"
#include "Async/ParallelFor.h"
#include "Misc/ScopeExit.h"
#include "UnrealTypeDefinitionInfo.h"
#include "Misc/WildcardString.h"

#include "UObject/FieldIterator.h"
#include "UObject/FieldPath.h"

#include "UObject/WeakFieldPtr.h"
#include "Templates/SubclassOf.h"
#include <atomic>

/////////////////////////////////////////////////////
// Globals

FManifest GManifest;

double GMacroizeTime = 0.0;

static TArray<FString> ChangeMessages;
static bool bWriteContents = false;
static bool bVerifyContents = false;
static bool bIncludeDebugOutput = false;
bool bGoWide = true;

void ProcessParsedClass(FUnrealClassDefinitionInfo& ClassDef);
void ProcessParsedEnum(FUnrealEnumDefinitionInfo& EnumDef);
void ProcessParsedStruct(FUnrealScriptStructDefinitionInfo& ScriptStructDef);

// Array of all the temporary header async file tasks so we can ensure they have completed before issuing our timings
static FGraphEventArray GAsyncFileTasks;

// Globals for common class definitions
FUnrealClassDefinitionInfo* GUObjectDef = nullptr;
FUnrealClassDefinitionInfo* GUClassDef = nullptr;
FUnrealClassDefinitionInfo* GUInterfaceDef = nullptr;

// Busy wait support for holes in the include graph issues
std::atomic<int> GSourcesToParse = 0;
std::atomic<int> GSourcesParsing = 0;
std::atomic<int> GSourcesCompleted = 0;
std::atomic<int> GSourcesStalled = 0;

struct FFindDelcarationResults
{
	bool bVirtualFound = false;
	const FDeclaration* Declaration = nullptr;
	int FunctionNameTokenIndex = -1;

	bool WasFound() const
	{
		return Declaration != nullptr;
	}

	bool IsVirtual() const
	{
		return bVirtualFound;
	}
};

FFindDelcarationResults FindDeclaration(const FUnrealStructDefinitionInfo& StructDef, const FString& Identifier);

namespace
{
	static const FName NAME_SerializeToFArchive("SerializeToFArchive");
	static const FName NAME_SerializeToFStructuredArchive("SerializeToFStructuredArchive");
	static const FName NAME_ObjectInitializerConstructorDeclared("ObjectInitializerConstructorDeclared");
	static const FName NAME_NoGetter("NoGetter");
	static const FName NAME_GetByRef("GetByRef");

	static const FString STRING_StructPackage(TEXT("StructPackage"));

	static const int32 HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH = FString(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX).Len(); 

	static FString AsTEXT(const FString& InStr)
	{
		return FString::Printf(TEXT("TEXT(\"%s\")"), *InStr);
	}

	const TCHAR* HeaderCopyright =
		TEXT("// Copyright Epic Games, Inc. All Rights Reserved.\r\n"
		     "/*===========================================================================\r\n"
		     "\tGenerated code exported from UnrealHeaderTool.\r\n"
		     "\tDO NOT modify this manually! Edit the corresponding .h files instead!\r\n"
		     "===========================================================================*/\r\n" LINE_TERMINATOR_ANSI);

	const TCHAR* RequiredCPPIncludes = TEXT("#include \"UObject/GeneratedCppIncludes.h\"" LINE_TERMINATOR_ANSI);

	const TCHAR* EnableDeprecationWarnings = TEXT("PRAGMA_ENABLE_DEPRECATION_WARNINGS" LINE_TERMINATOR_ANSI);
	const TCHAR* DisableDeprecationWarnings = TEXT("PRAGMA_DISABLE_DEPRECATION_WARNINGS" LINE_TERMINATOR_ANSI);

	// A struct which emits #if and #endif blocks as appropriate when invoked.
	struct FMacroBlockEmitter
	{
		explicit FMacroBlockEmitter(FOutputDevice& InOutput, const TCHAR* InMacro)
			: Output(InOutput)
			, bEmittedIf(false)
			, Macro(InMacro)
		{
		}

		~FMacroBlockEmitter()
		{
			if (bEmittedIf)
			{
				Output.Logf(TEXT("#endif // %s\r\n"), Macro);
			}
		}

		void operator()(bool bInBlock)
		{
			if (!bEmittedIf && bInBlock)
			{
				Output.Logf(TEXT("#if %s\r\n"), Macro);
				bEmittedIf = true;
			}
			else if (bEmittedIf && !bInBlock)
			{
				Output.Logf(TEXT("#endif // %s\r\n"), Macro);
				bEmittedIf = false;
			}
		}

		FMacroBlockEmitter(const FMacroBlockEmitter&) = delete;
		FMacroBlockEmitter& operator=(const FMacroBlockEmitter&) = delete;

	private:
		FOutputDevice& Output;
		bool bEmittedIf;
		const TCHAR* Macro;
	};

	/** Guard that should be put at the start editor only generated code */
	const auto& BeginEditorOnlyGuard = TEXT("#if WITH_EDITOR" LINE_TERMINATOR_ANSI);

	/** Guard that should be put at the end of editor only generated code */
	const auto& EndEditorOnlyGuard = TEXT("#endif //WITH_EDITOR" LINE_TERMINATOR_ANSI);

	/** Whether or not the given class has any replicated properties. */
	static bool ClassHasReplicatedProperties(const FUnrealClassDefinitionInfo& ClassDef)
	{
		if (!ClassDef.HasAnyClassFlags(CLASS_ReplicationDataIsSetUp))
		{
			for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : ClassDef.GetProperties())
			{
				if (PropertyDef->HasAnyPropertyFlags(CPF_Net))
				{
					return true;
				}
			}
		}

		return ClassDef.HasOwnedClassReps();
	}

	static void ExportNetData(FOutputDevice& Out, const FUnrealClassDefinitionInfo& ClassDef, const TCHAR* API)
	{
		const TArray<FUnrealPropertyDefinitionInfo*>& ClassReps = ClassDef.GetClassReps();

		FUHTStringBuilder NetFieldBuilder;
		NetFieldBuilder.Logf(TEXT(""
		"\tenum class ENetFields_Private : uint16\r\n"
		"\t{\r\n"
		"\t\tNETFIELD_REP_START=(uint16)((int32)Super::ENetFields_Private::NETFIELD_REP_END + (int32)1),\r\n"));

		FUHTStringBuilder ArrayDimBuilder;

		bool bAnyStaticArrays = false;
		bool bIsFirst = true;
		for (int32 ClassRepIndex = ClassDef.GetFirstOwnedClassRep(); ClassRepIndex < ClassReps.Num(); ++ClassRepIndex)
		{
			const FUnrealPropertyDefinitionInfo* PropertyDef = ClassReps[ClassRepIndex];
			const FString PropertyName = PropertyDef->GetName();

			if (!PropertyDef->IsStaticArray())
			{
				if (UNLIKELY(bIsFirst))
				{
					NetFieldBuilder.Logf(TEXT("\t\t%s=NETFIELD_REP_START,\r\n"), *PropertyName);
					bIsFirst = false;
				}
				else
				{
					NetFieldBuilder.Logf(TEXT("\t\t%s,\r\n"), *PropertyName);
				}
			}
			else
			{
				bAnyStaticArrays = true;
				ArrayDimBuilder.Logf(TEXT("\t\t%s=%s,\r\n"), *PropertyName, PropertyDef->GetArrayDimensions());

				if (UNLIKELY(bIsFirst))
				{
					NetFieldBuilder.Logf(TEXT("\t\t%s_STATIC_ARRAY=NETFIELD_REP_START,\r\n"), *PropertyName);
					bIsFirst = false;
				}
				else
				{
					NetFieldBuilder.Logf(TEXT("\t\t%s_STATIC_ARRAY,\r\n"), *PropertyName);
				}

				NetFieldBuilder.Logf(TEXT("\t\t%s_STATIC_ARRAY_END=((uint16)%s_STATIC_ARRAY + (uint16)EArrayDims_Private::%s - (uint16)1),\r\n"), *PropertyName, *PropertyName, *PropertyName);
			}
		}

		const FUnrealPropertyDefinitionInfo* LastPropertyDef = ClassReps.Last();
		NetFieldBuilder.Logf(TEXT("\t\tNETFIELD_REP_END=%s%s"), *LastPropertyDef->GetName(), LastPropertyDef->IsStaticArray() ? TEXT("_STATIC_ARRAY_END") : TEXT(""));

		NetFieldBuilder.Log(TEXT("\t};"));

		if (bAnyStaticArrays)
		{
			Out.Logf(TEXT(""
				"\tenum class EArrayDims_Private : uint16\r\n"
				"\t{\r\n"
				"%s"
				"\t};\r\n"), *ArrayDimBuilder);
		}

		Out.Logf(TEXT(""
			"%s\r\n" // NetFields
			"\t%s_API virtual void ValidateGeneratedRepEnums(const TArray<struct FRepRecord>& ClassReps) const override;\r\n"),
			*NetFieldBuilder,
			API);
	}

	static const FString STRING_GetLifetimeReplicatedPropsStr(TEXT("GetLifetimeReplicatedProps"));

	static void WriteReplicatedMacroData(
		const TCHAR* ClassCPPName,
		const TCHAR* API,
		FUnrealClassDefinitionInfo& ClassDef,
		FOutputDevice& Writer,
		const FUnrealSourceFile& SourceFile,
		EExportClassOutFlags& OutFlags)
	{
		const bool bHasGetLifetimeReplicatedProps = FindDeclaration(ClassDef, STRING_GetLifetimeReplicatedPropsStr).WasFound();

		if (!bHasGetLifetimeReplicatedProps)
		{
			// Default version autogenerates declarations.
			if (ClassDef.GetGeneratedCodeVersion() == EGeneratedCodeVersion::V1)
			{
				Writer.Logf(TEXT("\t%s_API void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;\r\n"), API);
			}
			else
			{
				ClassDef.Throwf(TEXT("Class %s has Net flagged properties and should declare member function: void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override"), ClassCPPName);
			}
		}


		ExportNetData(Writer, ClassDef, API);
		
		// If this class has replicated properties and it owns the first one, that means
		// it's the base most replicated class. In that case, go ahead and add our interface macro.
		if (ClassDef.HasClassReps() && ClassDef.GetFirstOwnedClassRep() == 0)
		{
			OutFlags |= EExportClassOutFlags::NeedsPushModelHeaders;
			Writer.Logf(TEXT(
				"private:\r\n"
				"\tREPLICATED_BASE_CLASS(%s%s)\r\n"
				"public:\r\n"
			), ClassDef.GetPrefixCPP(), *ClassDef.GetName());
		}
	}
}

void FGeneratedFileInfo::StartLoad(FString&& InFilename)
{
	ensureMsgf(Filename.IsEmpty(), TEXT("FPreloadHeaderFileInfo::StartLoad called twice with different paths."));
	Filename = MoveTemp(InFilename);

	if (bAllowSaveExportedHeaders)
	{
		auto LoadFileContentsTask = [this]()
		{
			SCOPE_SECONDS_COUNTER_UHT(LoadHeaderContentFromFile);
			FFileHelper::LoadFileToString(OriginalContents, *Filename);
		};

		LoadTaskRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(LoadFileContentsTask), TStatId());
	}
}

void FGeneratedFileInfo::Load(FString&& InFilename)
{
	ensureMsgf(Filename.IsEmpty(), TEXT("FPreloadHeaderFileInfo::StartLoad called twice with different paths."));
	Filename = MoveTemp(InFilename);

	if (bAllowSaveExportedHeaders)
	{
		SCOPE_SECONDS_COUNTER_UHT(LoadHeaderContentFromFile);
		FFileHelper::LoadFileToString(OriginalContents, *Filename);
	}
}

void FGeneratedFileInfo::GenerateBodyHash()
{
	GeneratedBodyHash = GenerateTextHash(*GeneratedBody);
}

FGeneratedCPP::FGeneratedCPP(FUnrealPackageDefinitionInfo& InPackageDef, FUnrealSourceFile& InSourceFile)
	: PackageDef(InPackageDef)
	, SourceFile(InSourceFile)
	, Header(InPackageDef.GetModule().SaveExportedHeaders)
	, Source(InPackageDef.GetModule().SaveExportedHeaders)
{
}

void FGeneratedCPP::AddGenerateTaskRef(FGraphEventArray& Events) const
{
	check(GenerateTaskRef.IsValid() || !SourceFile.ShouldExport());
	if (GenerateTaskRef.IsValid())
	{
		Events.Add(GenerateTaskRef);
	}
}

void FGeneratedCPP::AddExportTaskRef(FGraphEventArray& Events) const
{
	check(ExportTaskRef.IsValid() || !SourceFile.ShouldExport());
	if (ExportTaskRef.IsValid())
	{
		Events.Add(ExportTaskRef);
	}
}

#define BEGIN_WRAP_EDITOR_ONLY(DoWrap) DoWrap ? BeginEditorOnlyGuard : TEXT("")
#define END_WRAP_EDITOR_ONLY(DoWrap) DoWrap ? EndEditorOnlyGuard : TEXT("")

FFindDelcarationResults FindDeclaration(const FUnrealStructDefinitionInfo& StructDef, const FString& Identifier)
{
	FFindDelcarationResults Results;

	if (Identifier.IsEmpty())
	{
		return Results;
	}

	for (const FDeclaration& Declaration : StructDef.GetDeclarations())
	{
		for (int32 Index = 0, EIndex = Declaration.Tokens.Num(); Index != EIndex; ++Index)
		{
			const FToken& Token = Declaration.Tokens[Index];
			if (Token.IsIdentifier())
			{
				if (Token.IsValue(TEXT("virtual"), ESearchCase::CaseSensitive))
				{
					Results.bVirtualFound = true;
				}
				else if (Token.IsValue(*Identifier, ESearchCase::CaseSensitive))
				{
					Results.Declaration = &Declaration;
					Results.FunctionNameTokenIndex = Index;
					return Results;
				}
			}
		}
	}
	return Results;
}

void ConvertToBuildIncludePath(const FManifestModule& Module, FString& LocalPath)
{
	FPaths::MakePathRelativeTo(LocalPath, *Module.IncludeBase);
}

FString Macroize(const TCHAR* MacroName, FString&& StringToMacroize)
{
	FScopedDurationTimer Tracker(GMacroizeTime);

	FString Result(MoveTemp(StringToMacroize));
	if (Result.Len())
	{
		Result.ReplaceInline(TEXT("\r\n"), TEXT("\n"), ESearchCase::CaseSensitive);
		Result.ReplaceInline(TEXT("\n"), TEXT(" \\\n"), ESearchCase::CaseSensitive);
		checkSlow(Result.EndsWith(TEXT(" \\\n"), ESearchCase::CaseSensitive));

		if (Result.Len() >= 3)
		{
			for (int32 Index = Result.Len() - 3; Index < Result.Len(); ++Index)
			{
				Result[Index] = TEXT('\n');
			}
		}
		else
		{
			Result = TEXT("\n\n\n");
		}
		Result.ReplaceInline(TEXT("\n"), TEXT("\r\n"), ESearchCase::CaseSensitive);
	}
	return FString::Printf(TEXT("#define %s%s\r\n%s"), MacroName, Result.Len() ? TEXT(" \\") : TEXT(""), *Result);
}

struct FParmsAndReturnProperties
{
	bool HasParms() const
	{
		return Parms.Num() || Return;
	}

	TArray<FUnrealPropertyDefinitionInfo*> Parms;
	FUnrealPropertyDefinitionInfo* Return = nullptr;
};

/**
 * Get parameters and return type for a given function.
 *
 * @param  Function The function to get the parameters for.
 * @return An aggregate containing the parameters and return type of that function.
 */
FParmsAndReturnProperties GetFunctionParmsAndReturn(FUnrealFunctionDefinitionInfo& FunctionDef)
{
	FParmsAndReturnProperties Result;
	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : FunctionDef.GetProperties())
	{
		if (PropertyDef->HasSpecificPropertyFlags(CPF_Parm | CPF_ReturnParm, CPF_Parm))
		{
			Result.Parms.Add(&*PropertyDef);
		}
		else if (PropertyDef->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			Result.Return = &*PropertyDef;
		}
	}
	return Result;
}

/**
 * Determines whether the glue version of the specified native function
 * should be exported
 *
 * @param	Function	the function to check
 * @return	true if the glue version of the function should be exported.
 */
bool ShouldExportFunction(FUnrealFunctionDefinitionInfo& FunctionDef)
{
	// export any script stubs for native functions declared in interface classes
	bool bIsBlueprintNativeEvent = FunctionDef.HasAllFunctionFlags(FUNC_BlueprintEvent | FUNC_Native);
	if (FunctionDef.GetOwnerClass()->HasAnyClassFlags(CLASS_Interface) && !bIsBlueprintNativeEvent)
	{
		return true;
	}

	// always export if the function is static
	if (FunctionDef.HasAnyFunctionFlags(FUNC_Static))
	{
		return true;
	}

	// don't export the function if this is not the original declaration and there is
	// at least one parent version of the function that is declared native
	for (FUnrealFunctionDefinitionInfo* ParentFunctionDef = FunctionDef.GetSuperFunction(); ParentFunctionDef; ParentFunctionDef = ParentFunctionDef->GetSuperFunction())
	{
		if (ParentFunctionDef->HasAnyFunctionFlags(FUNC_Native))
		{
			return false;
		}
	}

	return true;
}

FString CreateLiteralString(const FString& Str)
{
	FString Result;

	// Have a reasonable guess at reserving the right size
	Result.Reserve(Str.Len() + 8);
	Result += TEXT("TEXT(\"");

	bool bPreviousCharacterWasHex = false;

	const TCHAR* Ptr = *Str;
	while (TCHAR Ch = *Ptr++)
	{
		switch (Ch)
		{
			case TEXT('\r'): continue;
			case TEXT('\n'): Result += TEXT("\\n");  bPreviousCharacterWasHex = false; break;
			case TEXT('\\'): Result += TEXT("\\\\"); bPreviousCharacterWasHex = false; break;
			case TEXT('\"'): Result += TEXT("\\\""); bPreviousCharacterWasHex = false; break;
			default:
				if (Ch < 31 || Ch >= 128)
				{
					Result += FString::Printf(TEXT("\\x%04x"), Ch);
					bPreviousCharacterWasHex = true;
				}
				else
				{
					// We close and open the literal (with TEXT) here in order to ensure that successive hex characters aren't appended to the hex sequence, causing a different number
					if (bPreviousCharacterWasHex && FCharWide::IsHexDigit(Ch))
					{
						Result += "\")TEXT(\"";
					}
					bPreviousCharacterWasHex = false;
					Result += Ch;
				}
				break;
		}
	}

	Result += TEXT("\")");
	return Result;
}

FString CreateUTF8LiteralString(const FString& Str)
{
	FString Result;

	// Have a reasonable guess at reserving the right size
	Result.Reserve(Str.Len() + 2);
	Result += TEXT("\"");

	bool bPreviousCharacterWasHex = false;

	FTCHARToUTF8 StrUTF8(*Str);

	const char* Ptr = StrUTF8.Get();
	while (char Ch = *Ptr++)
	{
		switch (Ch)
		{
			case '\r': continue;
			case '\n': Result += TEXT("\\n");  bPreviousCharacterWasHex = false; break;
			case '\\': Result += TEXT("\\\\"); bPreviousCharacterWasHex = false; break;
			case '\"': Result += TEXT("\\\""); bPreviousCharacterWasHex = false; break;
			default:
				if (Ch < 31)
				{
					Result += FString::Printf(TEXT("\\x%02x"), (uint8)Ch);
					bPreviousCharacterWasHex = true;
				}
				else
				{
					// We close and open the literal here in order to ensure that successive hex characters aren't appended to the hex sequence, causing a different number
					if (bPreviousCharacterWasHex && FCharWide::IsHexDigit(Ch))
					{
						Result += "\"\"";
					}
					bPreviousCharacterWasHex = false;
					Result += Ch;
				}
				break;
		}
	}

	Result += TEXT("\"");
	return Result;
}

// Returns the METADATA_PARAMS for this output
static FString OutputMetaDataCodeForObject(FOutputDevice& OutDeclaration, FOutputDevice& Out, FUnrealTypeDefinitionInfo& TypeDef, const TCHAR* MetaDataBlockName, const TCHAR* DeclSpaces, const TCHAR* Spaces)
{
	TMap<FName, FString> MetaData = TypeDef.GenerateMetadataMap();

	FString Result;
	if (MetaData.Num())
	{
		typedef TKeyValuePair<FName, FString*> KVPType;
		TArray<KVPType> KVPs;
		KVPs.Reserve(MetaData.Num());
		for (TPair<FName, FString>& KVP : MetaData)
		{
			KVPs.Add(KVPType(KVP.Key, &KVP.Value));
		}

		// We sort the metadata here so that we can get consistent output across multiple runs
		// even when metadata is added in a different order
		Algo::SortBy(KVPs, &KVPType::Key, FNameLexicalLess());

		FString MetaDataBlockNameWithoutScope = MetaDataBlockName;
		int32 ScopeIndex = MetaDataBlockNameWithoutScope.Find(TEXT("::"), ESearchCase::CaseSensitive);
		if (ScopeIndex != INDEX_NONE)
		{
			MetaDataBlockNameWithoutScope.RightChopInline(ScopeIndex + 2, false);
		}

		OutDeclaration.Log (TEXT("#if WITH_METADATA\r\n"));
		OutDeclaration.Logf(TEXT("%sstatic const UECodeGen_Private::FMetaDataPairParam %s[];\r\n"), DeclSpaces, *MetaDataBlockNameWithoutScope);
		OutDeclaration.Log (TEXT("#endif\r\n"));

		Out.Log (TEXT("#if WITH_METADATA\r\n"));
		Out.Logf(TEXT("%sconst UECodeGen_Private::FMetaDataPairParam %s[] = {\r\n"), Spaces, MetaDataBlockName);

		for (const KVPType& KVP : KVPs)
		{
			Out.Logf(TEXT("%s\t{ %s, %s },\r\n"), Spaces, *CreateUTF8LiteralString(KVP.Key.ToString()), *CreateUTF8LiteralString(*KVP.Value));
		}

		Out.Logf(TEXT("%s};\r\n"), Spaces);
		Out.Log (TEXT("#endif\r\n"));

		Result = FString::Printf(TEXT("METADATA_PARAMS(%s, UE_ARRAY_COUNT(%s))"), MetaDataBlockName, MetaDataBlockName);
	}
	else
	{
		Result = TEXT("METADATA_PARAMS(nullptr, 0)");
	}

	return Result;
}

void FNativeClassHeaderGenerator::ExportProperties(FOutputDevice& Out, FUnrealStructDefinitionInfo& StructDef, int32 TextIndent)
{
	FMacroBlockEmitter WithEditorOnlyData(Out, TEXT("WITH_EDITORONLY_DATA"));

	// Iterate over all properties in this struct.
	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : StructDef.GetProperties())
	{
		WithEditorOnlyData(PropertyDef->IsEditorOnlyProperty());

		// Export property specifiers
		// Indent code and export CPP text.
		FUHTStringBuilder JustPropertyDecl;
		PropertyDef->ExportCppDeclaration( JustPropertyDecl, EExportedDeclaration::Member, PropertyDef->GetArrayDimensions());
		ApplyAlternatePropertyExportText(*PropertyDef, JustPropertyDecl, EExportingState::TypeEraseDelegates);

		// Finish up line.
		Out.Logf(TEXT("%s%s;\r\n"), FCString::Tab(TextIndent + 1), *JustPropertyDecl);
	}
}

static FString GNullPtr(TEXT("nullptr"));

const FString& FNativeClassHeaderGenerator::GetPackageSingletonName(FUnrealPackageDefinitionInfo& PackageDef, TSet<FString>* UniqueCrossModuleReferences)
{
	PackageDef.AddCrossModuleReference(UniqueCrossModuleReferences);
	return PackageDef.GetSingletonName();
}

const FString& FNativeClassHeaderGenerator::GetPackageSingletonNameFuncAddr(FUnrealPackageDefinitionInfo& PackageDef, TSet<FString>* UniqueCrossModuleReferences)
{
	PackageDef.AddCrossModuleReference(UniqueCrossModuleReferences);
	return PackageDef.GetSingletonNameChopped();
}

const FString& FNativeClassHeaderGenerator::GetSingletonNameFuncAddr(FUnrealFieldDefinitionInfo* FieldDef, TSet<FString>* UniqueCrossModuleReferences, bool bRequiresValidObject)
{
	if (!FieldDef)
	{
		return GNullPtr;
	}
	else
	{
		FieldDef->AddCrossModuleReference(UniqueCrossModuleReferences, bRequiresValidObject);
		return FieldDef->GetSingletonNameChopped(bRequiresValidObject);
	}
}

void FNativeClassHeaderGenerator::GetPropertyTag(FUHTStringBuilder& Out, FUnrealPropertyDefinitionInfo& PropDef)
{
	const FPropertyBase& PropertyBase = PropDef.GetPropertyBase();

	// Do the value types
	switch (PropertyBase.GetUHTPropertyType())
	{
#if UHT_ENABLE_VALUE_PROPERTY_TAG
	case EUHTPropertyType::Enum:
		PropertyBase.EnumDef->GetHashTag(PropDef, Out);
		break;

	case EUHTPropertyType::Struct:
		PropertyBase.ScriptStructDef->GetHashTag(PropDef, Out);
		break;
#endif

#if UHT_ENABLE_PTR_PROPERTY_TAG
	case EUHTPropertyType::ObjectReference:
	case EUHTPropertyType::WeakObjectReference:
	case EUHTPropertyType::LazyObjectReference:
	case EUHTPropertyType::SoftObjectReference:
	case EUHTPropertyType::ObjectPtrReference:
	case EUHTPropertyType::InterfaceReference:
	case EUHTPropertyType::InterfaceReference:
		PropertyBase.ClassDef->GetHashTag(Out);
		break;
#endif

#if UHT_ENABLE_DELEGATE_PROPERTY_TAG
	case EUHTPropertyType::Delegate:
	case EUHTPropertyType::MulticastDelegate:
		PropertyBase.FunctionDef->GetHashTag(PropDef, Out);
		break;
#endif

	default:
		break;
	}

	// Add the key value
	if (PropDef.HasKeyPropDef())
	{
		GetPropertyTag(Out, PropDef.GetKeyPropDef());
	}
	if (PropDef.HasValuePropDef())
	{
		GetPropertyTag(Out, PropDef.GetValuePropDef());
	}
}

/** Helper function that generates property getter wrapper function name */
inline FString GetPropertyGetterWrapperName(FUnrealPropertyDefinitionInfo& Prop)
{
	return FString::Printf(TEXT("Get%s_WrapperImpl"), *Prop.GetName());
}

/** Helper function that generates property setter wrapper function name */
inline FString GetPropertySetterWrapperName(FUnrealPropertyDefinitionInfo& Prop)
{
	return FString::Printf(TEXT("Set%s_WrapperImpl"), *Prop.GetName());
}

/** Helper function that generates property getter wrapper function name for passing as a function pointer argument */
inline FString GetPropertyGetterWrapperPtr(FUnrealPropertyDefinitionInfo& Prop, const TCHAR* SourceStruct)
{
	return !Prop.GetPropertyBase().bGetterFunctionFound ? TEXT("nullptr") : FString::Printf(TEXT("&%s::%s"), SourceStruct, *GetPropertyGetterWrapperName(Prop));
}

/** Helper function that generates property setter wrapper function name for passing as a function pointer argument */
inline FString GetPropertySetterWrapperPtr(FUnrealPropertyDefinitionInfo& Prop, const TCHAR* SourceStruct)
{
	return !Prop.GetPropertyBase().bSetterFunctionFound ? TEXT("nullptr") : FString::Printf(TEXT("&%s::%s"), SourceStruct, *GetPropertySetterWrapperName(Prop));
}

void FNativeClassHeaderGenerator::OutputProperty(FOutputDevice& DeclOut, FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const TCHAR* Scope, TArray<FPropertyNamePointerPair>& PropertyNamesAndPointers, FUnrealPropertyDefinitionInfo& PropertyDef, const TCHAR* OffsetStr, FString&& Name, const TCHAR* DeclSpaces, const TCHAR* Spaces, const TCHAR* SourceStruct) const
{
	const FPropertyBase& PropertyBase   = PropertyDef.GetPropertyBase();
	FString        PropName             = CreateUTF8LiteralString(PropertyDef.GetName());
	FString        PropNameDep          = PropertyDef.HasAllPropertyFlags(CPF_Deprecated) ? PropertyDef.GetName() + TEXT("_DEPRECATED") : PropertyDef.GetName();
	const TCHAR*   FPropertyObjectFlags = TEXT("RF_Public|RF_Transient|RF_MarkAsNative");
	EPropertyFlags PropFlags            = PropertyDef.GetPropertyFlags() & ~CPF_ComputedFlags;

	FUHTStringBuilder PropTag;
	GetPropertyTag(PropTag, PropertyDef);

	FString PropNotifyFunc = PropertyDef.GetRepNotifyFunc() != NAME_None ? CreateUTF8LiteralString(*PropertyDef.GetRepNotifyFunc().ToString()) : TEXT("nullptr");

	FString ArrayDim = PropertyDef.IsStaticArray() ? FString::Printf(TEXT("CPP_ARRAY_DIM(%s, %s)"), *PropNameDep, SourceStruct) : TEXT("1");

	FString NameWithoutScope = *Name;
	{
		//FString Scope;
		int32 ScopeIndex = NameWithoutScope.Find(TEXT("::"), ESearchCase::CaseSensitive);
		if (ScopeIndex != INDEX_NONE)
		{
			//Scope = NameWithoutScope.Left(ScopeIndex) + TEXT("_");
			NameWithoutScope.RightChopInline(ScopeIndex + 2, false);
		}
	}

	FString SetterFunc = GetPropertySetterWrapperPtr(PropertyDef, SourceStruct);
	FString GetterFunc = GetPropertyGetterWrapperPtr(PropertyDef, SourceStruct);

	auto OutputByteProperty = [&]()
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FBytePropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FBytePropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Byte, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.EnumDef, OutReferenceGatherers.UniqueCrossModuleReferences),
			*MetaDataParams,
			*PropTag
		);
	};

	switch (PropertyBase.GetUHTPropertyType())
	{
	case EUHTPropertyType::Byte:
	{
		OutputByteProperty();
		break;
	}

	case EUHTPropertyType::Int8:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FInt8PropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FInt8PropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Int8, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Int16:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FInt16PropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FInt16PropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Int16, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Int:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		const TCHAR* PropTypeName = PropertyDef.IsUnsized() ? TEXT("FUnsizedIntPropertyParams") : TEXT("FIntPropertyParams");

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::%s %s;\r\n"), DeclSpaces, PropTypeName, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::%s %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Int, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			PropTypeName,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Int64:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FInt64PropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FInt64PropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Int64, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::UInt16:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FFInt16PropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FFInt16PropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::UInt16, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::UInt32:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		const TCHAR* PropTypeName = PropertyDef.IsUnsized() ? TEXT("FUnsizedFIntPropertyParams") : TEXT("FUInt32PropertyParams");

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::%s %s;\r\n"), DeclSpaces, PropTypeName, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::%s %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::UInt32, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			PropTypeName,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::UInt64:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FFInt64PropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FFInt64PropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::UInt64, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Float:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FFloatPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FFloatPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Float, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Double:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FDoublePropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FDoublePropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Double, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::LargeWorldCoordinatesReal:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FLargeWorldCoordinatesRealPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FLargeWorldCoordinatesRealPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::LargeWorldCoordinatesReal, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Bool:
	case EUHTPropertyType::Bool8:
	case EUHTPropertyType::Bool16:
	case EUHTPropertyType::Bool32:
	case EUHTPropertyType::Bool64:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		bool bIsNativeBool = PropertyBase.Type == CPT_Bool || EnumHasAnyFlags(PropertyBase.PropertyFlags, CPF_ReturnParm);
		FString OuterSize;
		FString Setter;
		if (UHTCast<FUnrealObjectDefinitionInfo>(PropertyDef.GetOuter()) == nullptr)
		{
			OuterSize = TEXT("0");
			Setter = TEXT("nullptr");
		}
		else
		{
			OuterSize = FString::Printf(TEXT("sizeof(%s)"), SourceStruct);

			DeclOut.Logf(TEXT("%sstatic void %s_SetBit(void* Obj);\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(TEXT("%svoid %s_SetBit(void* Obj)\r\n"), Spaces, *Name);
			Out.Logf(TEXT("%s{\r\n"), Spaces);
			Out.Logf(TEXT("%s\t((%s*)Obj)->%s%s = 1;\r\n"), Spaces, SourceStruct, *PropertyDef.GetName(), PropertyDef.HasAllPropertyFlags(CPF_Deprecated) ? TEXT("_DEPRECATED") : TEXT(""));
			Out.Logf(TEXT("%s}\r\n"), Spaces);

			Setter = FString::Printf(TEXT("&%s_SetBit"), *Name);
		}
		
		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FBoolPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FBoolPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Bool %s, %s, %s, %s, %s, sizeof(%s), %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			bIsNativeBool ? TEXT("| UECodeGen_Private::EPropertyGenFlags::NativeBool") : TEXT(""),
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			*PropertyDef.GetCPPType(nullptr, 0),
			*OuterSize,
			*Setter,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::ObjectReference:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		if (PropertyBase.ClassDef->IsChildOf(*GUClassDef))
		{
			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FClassPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FClassPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Class, %s, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*GetSingletonNameFuncAddr(PropertyBase.MetaClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),				
				*MetaDataParams,
				*PropTag
			);
		}
		else
		{
			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FObjectPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FObjectPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Object, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*MetaDataParams,
				*PropTag
			);
		}
		break;
	}

	case EUHTPropertyType::ObjectPtrReference:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		if (PropertyBase.ClassDef->IsChildOf(*GUClassDef))
		{
			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FClassPtrPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FClassPtrPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Class | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, %s, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*GetSingletonNameFuncAddr(PropertyBase.MetaClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*MetaDataParams,
				*PropTag
			);
		}
		else
		{
			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FObjectPtrPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FObjectPtrPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Object | UECodeGen_Private::EPropertyGenFlags::ObjectPtr, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*MetaDataParams,
				*PropTag
			);
		}
		break;
	}

	case EUHTPropertyType::SoftObjectReference:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		if (PropertyBase.ClassDef->IsChildOf(*GUClassDef))
		{
			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FSoftClassPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FSoftClassPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::SoftClass, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.MetaClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*MetaDataParams,
				*PropTag
			);
		}
		else
		{
			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FSoftObjectPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FSoftObjectPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::SoftObject, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
				*MetaDataParams,
				*PropTag
			);
		}
		break;
	}

	case EUHTPropertyType::WeakObjectReference:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FWeakObjectPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FWeakObjectPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::WeakObject, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::LazyObjectReference:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FLazyObjectPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FLazyObjectPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::LazyObject, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Interface:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FInterfacePropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FInterfacePropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Interface, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.ClassDef, OutReferenceGatherers.UniqueCrossModuleReferences, false),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Name:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FNamePropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FNamePropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Name, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::String:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FStrPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FStrPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Str, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::DynamicArray:
	{
		FUnrealPropertyDefinitionInfo& ValueDef = PropertyDef.GetValuePropDef();

		FString ValueVariableName = FString::Printf(TEXT("%sNewProp_%s_Inner"), Scope, *ValueDef.GetName());

		OutputProperty(DeclOut, Out, OutReferenceGatherers, Scope, PropertyNamesAndPointers, ValueDef, TEXT("0"), MoveTemp(ValueVariableName), DeclSpaces, Spaces, nullptr);

		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FArrayPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FArrayPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Array, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			PropertyDef.GetAllocatorType() == EAllocatorType::MemoryImage ? TEXT("EArrayPropertyFlags::UsesMemoryImageAllocator") : TEXT("EArrayPropertyFlags::None"),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Map:
	{
		FUnrealPropertyDefinitionInfo& KeyDef = PropertyDef.GetKeyPropDef();
		FUnrealPropertyDefinitionInfo& ValueDef = PropertyDef.GetValuePropDef();

		FString KeyVariableName = FString::Printf(TEXT("%sNewProp_%s_KeyProp"), Scope, *KeyDef.GetName());
		FString ValueVariableName = FString::Printf(TEXT("%sNewProp_%s_ValueProp"), Scope, *ValueDef.GetName());

		OutputProperty(DeclOut, Out, OutReferenceGatherers, Scope, PropertyNamesAndPointers, ValueDef, TEXT("1"), MoveTemp(ValueVariableName), DeclSpaces, Spaces, nullptr);
		OutputProperty(DeclOut, Out, OutReferenceGatherers, Scope, PropertyNamesAndPointers, KeyDef, TEXT("0"), MoveTemp(KeyVariableName), DeclSpaces, Spaces, nullptr);

		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FMapPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FMapPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Map, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			PropertyDef.GetAllocatorType() == EAllocatorType::MemoryImage ? TEXT("EMapPropertyFlags::UsesMemoryImageAllocator") : TEXT("EMapPropertyFlags::None"),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Set:
	{
		FUnrealPropertyDefinitionInfo& ValueDef = PropertyDef.GetValuePropDef();

		FString ValueVariableName = FString::Printf(TEXT("%sNewProp_%s_ElementProp"), Scope, *ValueDef.GetName());

		OutputProperty(DeclOut, Out, OutReferenceGatherers, Scope, PropertyNamesAndPointers, ValueDef, TEXT("0"), MoveTemp(ValueVariableName), DeclSpaces, Spaces, nullptr);

		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FSetPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		if (ValueDef.IsStructOrStructStaticArray())
		{
			const FString& StructName = ValueDef.GetPropertyBase().ScriptStructDef->GetNameCPP();
			Out.Logf(TEXT("%sstatic_assert(TModels<CGetTypeHashable, %s>::Value, \"The structure '%s' is used in a TSet but does not have a GetValueTypeHash defined\");\r\n"), Spaces, *StructName, *StructName);
		}

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FSetPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Set, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Struct:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FStructPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FStructPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Struct, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.ScriptStructDef, OutReferenceGatherers.UniqueCrossModuleReferences),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Delegate:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FDelegatePropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FDelegatePropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Delegate, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.FunctionDef, OutReferenceGatherers.UniqueCrossModuleReferences),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::MulticastDelegate:
	{
		bool bIsSparse = PropertyBase.FunctionDef->GetFunctionType() == EFunctionType::SparseDelegate;

		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FMulticastDelegatePropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FMulticastDelegatePropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::%sMulticastDelegate, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			!bIsSparse ? TEXT("Inline") : TEXT("Sparse"),
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*GetSingletonNameFuncAddr(PropertyBase.FunctionDef, OutReferenceGatherers.UniqueCrossModuleReferences),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Text:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FTextPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FTextPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Text, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	case EUHTPropertyType::Enum:
	{
		if (PropertyBase.EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			OutputByteProperty();
		}
		else
		{
			// Output the underlying property
			FString PropVarName = FString::Printf(TEXT("%s_Underlying"), *Name);
			OutputProperty(DeclOut, Out, OutReferenceGatherers, Scope, PropertyNamesAndPointers, PropertyDef.GetValuePropDef(), TEXT("0"), *PropVarName, DeclSpaces, Spaces, nullptr);

			FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

			DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FEnumPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

			Out.Logf(
				TEXT("%sconst UECodeGen_Private::FEnumPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::Enum, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
				Spaces,
				*Name,
				*PropName,
				*PropNotifyFunc,
				PropFlags,
				FPropertyObjectFlags,
				*ArrayDim,
				*SetterFunc,
				*GetterFunc,
				OffsetStr,
				*GetSingletonNameFuncAddr(PropertyBase.EnumDef, OutReferenceGatherers.UniqueCrossModuleReferences),
				*MetaDataParams,
				*PropTag
			);
		}
		break;
	}

	case EUHTPropertyType::FieldPath:
	{
		FString MetaDataParams = OutputMetaDataCodeForObject(DeclOut, Out, PropertyDef, *FString::Printf(TEXT("%s_MetaData"), *Name), DeclSpaces, Spaces);

		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FFieldPathPropertyParams %s;\r\n"), DeclSpaces, *NameWithoutScope);

		Out.Logf(
			TEXT("%sconst UECodeGen_Private::FFieldPathPropertyParams %s = { %s, %s, (EPropertyFlags)0x%016llx, UECodeGen_Private::EPropertyGenFlags::FieldPath, %s, %s, %s, %s, %s, %s, %s };%s\r\n"),
			Spaces,
			*Name,
			*PropName,
			*PropNotifyFunc,
			PropFlags,
			FPropertyObjectFlags,
			*ArrayDim,
			*SetterFunc,
			*GetterFunc,
			OffsetStr,
			*FString::Printf(TEXT("&F%s::StaticClass"), *PropertyBase.FieldClassName.ToString()),
			*MetaDataParams,
			*PropTag
		);
		break;
	}

	default:
		check(false);
	}

	PropertyNamesAndPointers.Emplace(MoveTemp(Name), PropertyDef);
}

bool IsEditorOnlyDataProperty(FUnrealPropertyDefinitionInfo& PropDef)
{
	for (FUnrealPropertyDefinitionInfo* TestPropDef = &PropDef; TestPropDef; TestPropDef = UHTCast<FUnrealPropertyDefinitionInfo>(TestPropDef->GetOuter()))
	{
		if (TestPropDef->IsEditorOnlyProperty())
		{
			return true;
		}
	}
	return false;
}

FString GetEventStructParamsName(FUnrealObjectDefinitionInfo& OuterDef, const TCHAR* FunctionName)
{
	FString OuterName;
	if (FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(OuterDef))
	{
		OuterName = ClassDef->GetName();
	}
	else if (FUnrealPackageDefinitionInfo* PackageDef = UHTCast<FUnrealPackageDefinitionInfo>(OuterDef))
	{
		OuterName = PackageDef->GetPackage()->GetName();
		OuterName.ReplaceInline(TEXT("/"), TEXT("_"), ESearchCase::CaseSensitive);
	}
	else
	{
		OuterDef.Throwf(TEXT("Unrecognized outer type"));
	}

	FString Result = FString::Printf(TEXT("%s_event%s_Parms"), *OuterName, FunctionName);
	if (Result.Len() && FChar::IsDigit(Result[0]))
	{
		Result.InsertAt(0, TCHAR('_'));
	}
	return Result;
}

TTuple<FString, FString> FNativeClassHeaderGenerator::OutputProperties(FOutputDevice& DeclOut, FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const TCHAR* Scope, FUnrealStructDefinitionInfo& StructDef, const TCHAR* DeclSpaces, const TCHAR* Spaces) const
{
	if (StructDef.GetProperties().Num() == 0)
	{
		return TTuple<FString, FString>(TEXT("nullptr"), TEXT("0"));
	}

	TArray<FPropertyNamePointerPair> PropertyNamesAndPointers;
	bool bHasAllEditorOnlyDataProperties = true;

	{
		FString SourceStruct;
		if (FUnrealFunctionDefinitionInfo* FunctionDef = UHTCast<FUnrealFunctionDefinitionInfo>(StructDef))
		{
			while (FunctionDef->GetSuperFunction())
			{
				FunctionDef = FunctionDef->GetSuperFunction();
			}
			FString FunctionName = FunctionDef->GetName();
			if (FunctionDef->HasAnyFunctionFlags(FUNC_Delegate))
			{
				FunctionName.LeftChopInline(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH, false);
			}

			SourceStruct = GetEventStructParamsName(*FunctionDef->GetOuter(), *FunctionName);
		}
		else 
		{
			SourceStruct = StructDef.GetAlternateNameCPP();
		}

		FMacroBlockEmitter WithEditorOnlyMacroEmitter(Out, TEXT("WITH_EDITORONLY_DATA"));
		FMacroBlockEmitter WithEditorOnlyMacroEmitterDecl(DeclOut, TEXT("WITH_EDITORONLY_DATA"));

		for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : StructDef.GetProperties())
		{
			bool bRequiresHasEditorOnlyMacro = IsEditorOnlyDataProperty(*PropertyDef);
			if (!bRequiresHasEditorOnlyMacro)
			{
				bHasAllEditorOnlyDataProperties = false;
			}

			WithEditorOnlyMacroEmitter(bRequiresHasEditorOnlyMacro);
			WithEditorOnlyMacroEmitterDecl(bRequiresHasEditorOnlyMacro);

			FString PropName = PropertyDef->GetName();
			FString PropVariableName = FString::Printf(TEXT("%sNewProp_%s"), Scope, *PropName);

			if (PropertyDef->HasAllPropertyFlags(CPF_Deprecated))
			{
				PropName += TEXT("_DEPRECATED");
			}

			FString PropMacroOuterClass = FString::Printf(TEXT("STRUCT_OFFSET(%s, %s)"), *SourceStruct, *PropName);

			OutputProperty(DeclOut, Out, OutReferenceGatherers, Scope, PropertyNamesAndPointers, *PropertyDef, *PropMacroOuterClass, MoveTemp(PropVariableName), DeclSpaces, Spaces, *SourceStruct);
		}

		WithEditorOnlyMacroEmitter(bHasAllEditorOnlyDataProperties);
		WithEditorOnlyMacroEmitterDecl(bHasAllEditorOnlyDataProperties);
		DeclOut.Logf(TEXT("%sstatic const UECodeGen_Private::FPropertyParamsBase* const PropPointers[];\r\n"), DeclSpaces);
		Out.Logf(TEXT("%sconst UECodeGen_Private::FPropertyParamsBase* const %sPropPointers[] = {\r\n"), Spaces, Scope);

		for (const FPropertyNamePointerPair& PropNameAndPtr : PropertyNamesAndPointers)
		{
			bool bRequiresHasEditorOnlyMacro = IsEditorOnlyDataProperty(*PropNameAndPtr.PropDef);

			WithEditorOnlyMacroEmitter(bRequiresHasEditorOnlyMacro);
			WithEditorOnlyMacroEmitterDecl(bRequiresHasEditorOnlyMacro);
			Out.Logf(TEXT("%s\t(const UECodeGen_Private::FPropertyParamsBase*)&%s,\r\n"), Spaces, *PropNameAndPtr.Name);
		}

		WithEditorOnlyMacroEmitter(bHasAllEditorOnlyDataProperties);
		WithEditorOnlyMacroEmitterDecl(bHasAllEditorOnlyDataProperties);
		Out.Logf(TEXT("%s};\r\n"), Spaces);
	}

	if (bHasAllEditorOnlyDataProperties)
	{
		return TTuple<FString, FString>(
			FString::Printf(TEXT("IF_WITH_EDITORONLY_DATA(%sPropPointers, nullptr)"), Scope),
			FString::Printf(TEXT("IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(%sPropPointers), 0)"), Scope)
		);
	}
	else
	{
		return TTuple<FString, FString>(
			FString::Printf(TEXT("%sPropPointers"), Scope),
			FString::Printf(TEXT("UE_ARRAY_COUNT(%sPropPointers)"), Scope)
		);
	}
}

static bool IsAlwaysAccessible(FUnrealScriptStructDefinitionInfo& ScriptDef)
{
	FName ToTest = ScriptDef.GetFName();
	if (ToTest == NAME_Matrix || ToTest == NAME_Matrix44f || ToTest == NAME_Matrix44d)
	{
		return false; // special case, the C++ FMatrix does not have the same members.
	}
	bool Result = ScriptDef.HasDefaults(); // if we have cpp struct ops in it for UHT, then we can assume it is always accessible

	// LWC_TODO: 
	//UScriptStruct::ICppStructOps* StructOps = UScriptStruct::FindDeferredCppStructOps(ToTest);
	//check(StructOps || !ToTest_falls_within_UnrealNames.inl_SpecialTypes);
	//return StructOps != nullptr;

	if( ToTest == NAME_Plane || ToTest == NAME_Plane4f || ToTest == NAME_Plane4d
		||	ToTest == NAME_Vector || ToTest == NAME_Vector3f || ToTest == NAME_Vector3d
		|| ToTest == NAME_Vector4 || ToTest == NAME_Vector4f || ToTest == NAME_Vector4d
		|| ToTest == NAME_Box || ToTest == NAME_Box3f || ToTest == NAME_Box3d
		||	ToTest == NAME_Quat || ToTest == NAME_Quat4f || ToTest == NAME_Quat4d
		|| ToTest == NAME_Rotator || ToTest == NAME_Rotator3f || ToTest == NAME_Rotator3d
		||	ToTest == NAME_Color
		)
	{
		check(Result);
	}
	return Result;
}

static void FindNoExportStructsRecursive(TArray<FUnrealScriptStructDefinitionInfo*>& StructDefs, FUnrealStructDefinitionInfo* StartDef)
{
	for (; StartDef; StartDef = StartDef->GetSuperStructInfo().Struct)
	{
		if (FUnrealScriptStructDefinitionInfo* StartScriptDef = UHTCast<FUnrealScriptStructDefinitionInfo>(StartDef))
		{
			if (StartScriptDef->HasAnyStructFlags(STRUCT_Native))
			{
				break;
			}

			if (!IsAlwaysAccessible(*StartScriptDef)) // these are a special cases that already exists and if wrong if exported naively
			{
				// this will topologically sort them in reverse order
				StructDefs.Remove(&StartDef->AsScriptStructChecked());
				StructDefs.Add(&StartDef->AsScriptStructChecked());
			}
		}

		for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : StartDef->GetProperties())
		{
			const FPropertyBase& PropertyBase = PropertyDef->GetPropertyBase();
			if (UHTCast<FUnrealScriptStructDefinitionInfo>(PropertyBase.TypeDef) != nullptr)
			{
				FindNoExportStructsRecursive(StructDefs, PropertyBase.ScriptStructDef);
			}
			if (PropertyBase.MapKeyProp != nullptr && UHTCast<FUnrealScriptStructDefinitionInfo>(PropertyBase.MapKeyProp->TypeDef) != nullptr)
			{
				FindNoExportStructsRecursive(StructDefs, PropertyBase.MapKeyProp->ScriptStructDef);
			}
		}
	}
}

static TArray<FUnrealScriptStructDefinitionInfo*> FindNoExportStructs(FUnrealStructDefinitionInfo* StartDef)
{
	TArray<FUnrealScriptStructDefinitionInfo*> Result;
	FindNoExportStructsRecursive(Result, StartDef);

	// These come out in reverse order of topology so reverse them
	Algo::Reverse(Result);

	return Result;
}

bool IsDelegateFunction(FUnrealFieldDefinitionInfo& FieldDef)
{
	FUnrealFunctionDefinitionInfo* FunctionDef = UHTCast<FUnrealFunctionDefinitionInfo>(FieldDef);
	return FunctionDef != nullptr && FunctionDef->IsDelegateFunction();
}

void FNativeClassHeaderGenerator::ExportGeneratedPackageInitCode(FOutputDevice& Out, const TCHAR* InDeclarations, const TArray<FGeneratedCPP*>& ExportedSorted, uint32 Hash)
{
	UPackage* Package = PackageDef.GetPackage();
	const FString& SingletonName = GetPackageSingletonNameFuncAddr(PackageDef, nullptr);

	FString PackageName = Package->GetName();
	PackageName.ReplaceInline(TEXT("/"), TEXT("_"), ESearchCase::CaseSensitive);
	FString BodyHashFn = FString::Printf(TEXT("Z_UPackage_%s_BodyHash"), *PackageName);
	FString DeclarationsHashFn = FString::Printf(TEXT("Z_UPackage_%s_DeclarationsHash"), *PackageName);

	uint32 DeclarationsHash = GenerateTextHash(InDeclarations);

	TArray<FUnrealFieldDefinitionInfo*> Singletons;
	for (TSharedRef<FUnrealSourceFile>& SourceFile : PackageDef.GetAllSourceFiles())
	{
		Singletons.Append(SourceFile->GetSingletons());
	}

	Algo::Sort(Singletons, [](FUnrealFieldDefinitionInfo* A, FUnrealFieldDefinitionInfo* B)
	{
		bool bADel = IsDelegateFunction(*A);
		bool bBDel = IsDelegateFunction(*B);
		if (bADel != bBDel)
		{
			return !bADel;
		}
		const FString& AName = A->GetSingletonName(true);
		const FString& BName = B->GetSingletonName(true);
		return AName < BName;
	});

	if (bIncludeDebugOutput)
	{
		Out.Log(TEXT("#if 0\r\n"));
		for (FGeneratedCPP* gen : ExportedSorted)
		{
			Out.Logf(TEXT("\t%s\r\n"), *gen->SourceFile.GetFilename());
		}
		Out.Log(InDeclarations);
		Out.Log(TEXT("#endif\r\n"));
	}

	for (FUnrealFieldDefinitionInfo* FieldDef : Singletons)
	{
		Out.Log(FieldDef->GetExternDecl(true));
	}

	FOutputDeviceNull OutputDeviceNull;
	FString MetaDataParams = OutputMetaDataCodeForObject(OutputDeviceNull, Out, PackageDef, TEXT("Package_MetaDataParams"), TEXT(""), TEXT("\t\t\t"));

	Out.Logf(TEXT("\tstatic FPackageRegistrationInfo Z_Registration_Info_UPackage_%s;\r\n"), *PackageName);

	Out.Logf(TEXT("\tFORCENOINLINE UPackage* %s()\r\n"), *SingletonName);
	Out.Logf(TEXT("\t{\r\n"));
	Out.Logf(TEXT("\t\tif (!Z_Registration_Info_UPackage_%s.OuterSingleton)\r\n"), *PackageName);
	Out.Logf(TEXT("\t\t{\r\n"));

	const TCHAR* SingletonArray;
	const TCHAR* SingletonCount;
	if (!Singletons.IsEmpty())
	{
		Out.Logf(TEXT("\t\t\tstatic UObject* (*const SingletonFuncArray[])() = {\r\n"));
		for (FUnrealFieldDefinitionInfo* FieldDef : Singletons)
		{
			const FString& Name = FieldDef->GetSingletonNameChopped(true);
			Out.Logf(TEXT("\t\t\t\t(UObject* (*)())%s,\r\n"), *Name);
		}
		Out.Logf(TEXT("\t\t\t};\r\n"));

		SingletonArray = TEXT("SingletonFuncArray");
		SingletonCount = TEXT("UE_ARRAY_COUNT(SingletonFuncArray)");
	}
	else
	{
		SingletonArray = TEXT("nullptr");
		SingletonCount = TEXT("0");
	}

	Out.Logf(TEXT("\t\t\tstatic const UECodeGen_Private::FPackageParams PackageParams = {\r\n"));
	Out.Logf(TEXT("\t\t\t\t%s,\r\n"), *CreateUTF8LiteralString(Package->GetName()));
	Out.Logf(TEXT("\t\t\t\t%s,\r\n"), SingletonArray);
	Out.Logf(TEXT("\t\t\t\t%s,\r\n"), SingletonCount);
	Out.Logf(TEXT("\t\t\t\tPKG_CompiledIn | 0x%08X,\r\n"), Package->GetPackageFlags() & (PKG_ClientOptional | PKG_ServerSideOnly | PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly));
	Out.Logf(TEXT("\t\t\t\t0x%08X,\r\n"), Hash);
	Out.Logf(TEXT("\t\t\t\t0x%08X,\r\n"), DeclarationsHash);
	Out.Logf(TEXT("\t\t\t\t%s\r\n"), *MetaDataParams);
	Out.Logf(TEXT("\t\t\t};\r\n"));
	Out.Logf(TEXT("\t\t\tUECodeGen_Private::ConstructUPackage(Z_Registration_Info_UPackage_%s.OuterSingleton, PackageParams);\r\n"), *PackageName);
	Out.Logf(TEXT("\t\t}\r\n"));
	Out.Logf(TEXT("\t\treturn Z_Registration_Info_UPackage_%s.OuterSingleton;\r\n"), *PackageName);
	Out.Logf(TEXT("\t}\r\n"));

	// Do not change the Z_CompiledInDeferPackage_UPackage_ without changing LC_SymbolPatterns
	Out.Logf(TEXT("\tstatic FRegisterCompiledInInfo Z_CompiledInDeferPackage_UPackage_%s(%s, TEXT(\"%s\"), Z_Registration_Info_UPackage_%s, CONSTRUCT_RELOAD_VERSION_INFO(FPackageReloadVersionInfo, 0x%08X, 0x%08X));\r\n"),
		*PackageName,
		*SingletonName,
		*Package->GetName(),
		*PackageName,
		Hash,
		DeclarationsHash
	);
}

void FNativeClassHeaderGenerator::ExportNativeGeneratedInitCode(FOutputDevice& Out, FOutputDevice& OutDeclarations, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealClassDefinitionInfo& ClassDef, FUHTStringBuilder& OutFriendText) const
{
	check(!OutFriendText.Len());

	const bool   bIsNoExport  = ClassDef.IsNoExport();
	const FString ClassNameCPP = ClassDef.GetAlternateNameCPP();

	const FString& ApiString = GetAPIString();

	TSet<FName> AlreadyIncludedNames;
	TArray<FUnrealFunctionDefinitionInfo*> FunctionsToExport;
	bool bAllEditorOnlyFunctions = true;
	for (TSharedRef<FUnrealFunctionDefinitionInfo> LocalFuncDef : ClassDef.GetFunctions())
	{
		FName TrueName = LocalFuncDef->GetFName();
		bool bAlreadyIncluded = false;
		AlreadyIncludedNames.Add(TrueName, &bAlreadyIncluded);
		if (bAlreadyIncluded)
		{
			if (!LocalFuncDef->IsDelegateFunction())
			{
				LocalFuncDef->Throwf(TEXT("The same function linked twice. Function: %s Class: %s"), *LocalFuncDef->GetName(), *ClassDef.GetName());
			}
			continue;
		}
		if (!LocalFuncDef->IsDelegateFunction())
		{
			bAllEditorOnlyFunctions &= LocalFuncDef->HasAnyFunctionFlags(FUNC_EditorOnly);
		}
		FunctionsToExport.Add(&*LocalFuncDef);
	}

	// Sort the list of functions
	Algo::SortBy(FunctionsToExport, [](FUnrealFunctionDefinitionInfo* Obj) { return Obj->GetName(); });

	FUHTStringBuilder GeneratedClassRegisterFunctionText;

	// The class itself.
	{
		// simple ::StaticClass wrapper to avoid header, link and DLL hell
		{
			ClassDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, false);
			const FString& SingletonNameNoRegister = ClassDef.GetSingletonName(false);

			OutDeclarations.Log(ClassDef.GetExternDecl(false));

			GeneratedClassRegisterFunctionText.Logf(TEXT("\tUClass* %s\r\n"), *SingletonNameNoRegister);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t{\r\n"));
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\treturn %s::StaticClass();\r\n"), *ClassNameCPP);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t}\r\n"));
		}

		// NOTE: We are adding the cross module reference here to avoid output differences.
		ClassDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, true);
		const FString& SingletonName = ClassDef.GetSingletonName(true);

		FString StaticsStructName = ClassDef.GetSingletonNameChopped(true) + TEXT("_Statics");

		OutFriendText.Logf(TEXT("\tfriend struct %s;\r\n"), *StaticsStructName);
		OutDeclarations.Log(ClassDef.GetExternDecl(true));

		GeneratedClassRegisterFunctionText.Logf(TEXT("\tstruct %s\r\n"), *StaticsStructName);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t{\r\n"));

		FUHTStringBuilder StaticDefinitions;

		FUHTStringBuilder Singletons;
		FUnrealClassDefinitionInfo* SuperClassDef = ClassDef.GetSuperClass();
		if (SuperClassDef && SuperClassDef != &ClassDef)
		{
			SuperClassDef->AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, true);
			OutDeclarations.Log(SuperClassDef->GetExternDecl(true));
			Singletons.Logf(TEXT("\t\t(UObject* (*)())%s,\r\n"), *SuperClassDef->GetSingletonNameChopped(true));
		}

			check(ClassDef.HasSource());
			FUnrealPackageDefinitionInfo& ClassPackageDef = ClassDef.GetPackageDef();
			PackageDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences);
			OutDeclarations.Logf(TEXT("\t%s_API UPackage* %s;\r\n"), *ApiString, *ClassPackageDef.GetSingletonName());
			Singletons.Logf(TEXT("\t\t(UObject* (*)())%s,\r\n"), *ClassPackageDef.GetSingletonNameChopped());

		const TCHAR* SingletonsArray;
		const TCHAR* SingletonsCount;
		if (Singletons.Len() != 0)
		{
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tstatic UObject* (*const DependentSingletons[])();\r\n"));

			StaticDefinitions.Logf(TEXT("\tUObject* (*const %s::DependentSingletons[])() = {\r\n"), *StaticsStructName);
			StaticDefinitions.Log (*Singletons);
			StaticDefinitions.Logf(TEXT("\t};\r\n"));

			SingletonsArray = TEXT("DependentSingletons");
			SingletonsCount = TEXT("UE_ARRAY_COUNT(DependentSingletons)");
		}
		else
		{
			SingletonsArray = TEXT("nullptr");
			SingletonsCount = TEXT("0");
		}

		const TCHAR* FunctionsArray;
		const TCHAR* FunctionsCount;
		if (FunctionsToExport.Num() != 0)
		{
			GeneratedClassRegisterFunctionText.Log(BEGIN_WRAP_EDITOR_ONLY(bAllEditorOnlyFunctions));
			GeneratedClassRegisterFunctionText.Log(TEXT("\t\tstatic const FClassFunctionLinkInfo FuncInfo[];\r\n"));
			GeneratedClassRegisterFunctionText.Log(END_WRAP_EDITOR_ONLY(bAllEditorOnlyFunctions));

			StaticDefinitions.Log(BEGIN_WRAP_EDITOR_ONLY(bAllEditorOnlyFunctions));
			StaticDefinitions.Logf(TEXT("\tconst FClassFunctionLinkInfo %s::FuncInfo[] = {\r\n"), *StaticsStructName);

			for (FUnrealFunctionDefinitionInfo* FunctionDef : FunctionsToExport)
			{
				const bool bIsEditorOnlyFunction = FunctionDef->HasAnyFunctionFlags(FUNC_EditorOnly);

				if (!FunctionDef->IsDelegateFunction())
				{
					ExportFunction(Out, OutReferenceGatherers, SourceFile, *FunctionDef, bIsNoExport);
				}

				FUHTStringBuilder FuncHashTag;
				FunctionDef->GetHashTag(ClassDef, FuncHashTag);

				StaticDefinitions.Logf(
					TEXT("%s\t\t{ &%s, %s },%s\r\n%s"),
					BEGIN_WRAP_EDITOR_ONLY(bIsEditorOnlyFunction),
					*GetSingletonNameFuncAddr(FunctionDef, OutReferenceGatherers.UniqueCrossModuleReferences),
					*CreateUTF8LiteralString(FunctionDef->GetName()),
					*FuncHashTag,
					END_WRAP_EDITOR_ONLY(bIsEditorOnlyFunction)
				);
			}

			StaticDefinitions.Log(TEXT("\t};\r\n"));
			StaticDefinitions.Log(END_WRAP_EDITOR_ONLY(bAllEditorOnlyFunctions));

			if (bAllEditorOnlyFunctions)
			{
				FunctionsArray = TEXT("IF_WITH_EDITOR(FuncInfo, nullptr)");
				FunctionsCount = TEXT("IF_WITH_EDITOR(UE_ARRAY_COUNT(FuncInfo), 0)");
			}
			else
			{
				FunctionsArray = TEXT("FuncInfo");
				FunctionsCount = TEXT("UE_ARRAY_COUNT(FuncInfo)");
			}
		}
		else
		{
			FunctionsArray = TEXT("nullptr");
			FunctionsCount = TEXT("0");
		}

		if (ClassDef.IsObjectInitializerConstructorDeclared())
		{
			ClassDef.SetMetaData(NAME_ObjectInitializerConstructorDeclared, TEXT(""));
		}

		FString MetaDataParams = OutputMetaDataCodeForObject(GeneratedClassRegisterFunctionText, StaticDefinitions, ClassDef, *FString::Printf(TEXT("%s::Class_MetaDataParams"), *StaticsStructName), TEXT("\t\t"), TEXT("\t"));

		TTuple<FString, FString> PropertyRange = OutputProperties(GeneratedClassRegisterFunctionText, StaticDefinitions, OutReferenceGatherers, *FString::Printf(TEXT("%s::"), *StaticsStructName), ClassDef, TEXT("\t\t"), TEXT("\t"));

		const TCHAR* InterfaceArray;
		const TCHAR* InterfaceCount;

		// Check to see if we have any interfaces
		bool bHasInterfaces = false;
		for (FUnrealStructDefinitionInfo::FBaseStructInfo& BaseStruct : ClassDef.GetBaseStructInfos())
		{
			if (FUnrealClassDefinitionInfo* BaseClass = UHTCast<FUnrealClassDefinitionInfo>(BaseStruct.Struct); BaseClass != nullptr && BaseClass->IsInterface())
			{
				bHasInterfaces = true;
				break;
			}
		}

		if (bHasInterfaces)
		{
			GeneratedClassRegisterFunctionText.Log(TEXT("\t\tstatic const UECodeGen_Private::FImplementedInterfaceParams InterfaceParams[];\r\n"));

			StaticDefinitions.Logf(TEXT("\t\tconst UECodeGen_Private::FImplementedInterfaceParams %s::InterfaceParams[] = {\r\n"), *StaticsStructName);
			for (FUnrealStructDefinitionInfo::FBaseStructInfo& BaseStruct : ClassDef.GetBaseStructInfos())
			{
				if (FUnrealClassDefinitionInfo* BaseClass = UHTCast<FUnrealClassDefinitionInfo>(BaseStruct.Struct); BaseClass == nullptr || !BaseClass->IsInterface())
				{
					continue;
				}

				FUnrealClassDefinitionInfo& InterClassDef = UHTCastChecked<FUnrealClassDefinitionInfo>(BaseStruct.Struct);
				InterClassDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, false);
				FString OffsetString = FString::Printf(TEXT("(int32)VTABLE_OFFSET(%s, %s)"), *ClassNameCPP, *InterClassDef.GetAlternateNameCPP(true));

				FUHTStringBuilder IntHash;
				InterClassDef.GetHashTag(ClassDef, IntHash);
				StaticDefinitions.Logf(
					TEXT("\t\t\t{ %s, %s, %s }, %s\r\n"),
					*InterClassDef.GetSingletonNameChopped(false),
					*OffsetString,
					TEXT("false"),
					*IntHash
				);
			}
			StaticDefinitions.Log(TEXT("\t\t};\r\n"));

			InterfaceArray = TEXT("InterfaceParams");
			InterfaceCount = TEXT("UE_ARRAY_COUNT(InterfaceParams)");
		}
		else
		{
			InterfaceArray = TEXT("nullptr");
			InterfaceCount = TEXT("0");
		}

		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tstatic const FCppClassTypeInfoStatic StaticCppClassTypeInfo;\r\n"));

		StaticDefinitions.Logf(TEXT("\tconst FCppClassTypeInfoStatic %s::StaticCppClassTypeInfo = {\r\n"), *StaticsStructName);
		StaticDefinitions.Logf(TEXT("\t\tTCppClassTypeTraits<%s>::IsAbstract,\r\n"), *ClassDef.GetAlternateNameCPP(ClassDef.HasAllClassFlags(CLASS_Interface)));
		StaticDefinitions.Logf(TEXT("\t};\r\n"));

		GeneratedClassRegisterFunctionText.Log (TEXT("\t\tstatic const UECodeGen_Private::FClassParams ClassParams;\r\n"));

		uint32 ClassFlags = (uint32)ClassDef.GetClassFlags();
		if (!bIsNoExport)
		{
			ClassFlags = ClassFlags | CLASS_MatchedSerializers;
		}
		ClassFlags = ClassFlags & CLASS_SaveInCompiledInClasses;

		StaticDefinitions.Logf(TEXT("\tconst UECodeGen_Private::FClassParams %s::ClassParams = {\r\n"), *StaticsStructName);
		StaticDefinitions.Logf(TEXT("\t\t&%s::StaticClass,\r\n"), *ClassNameCPP);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), (ClassDef.GetClassConfigName() != NAME_None) ? *CreateUTF8LiteralString(ClassDef.GetClassConfigName().ToString()) : TEXT("nullptr"));
		StaticDefinitions.Log (TEXT("\t\t&StaticCppClassTypeInfo,\r\n"));
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), SingletonsArray);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), FunctionsArray);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *PropertyRange.Get<0>());
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), InterfaceArray);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), SingletonsCount);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), FunctionsCount);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *PropertyRange.Get<1>());
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), InterfaceCount);
		StaticDefinitions.Logf(TEXT("\t\t0x%08Xu,\r\n"), ClassFlags);
		StaticDefinitions.Logf(TEXT("\t\t%s\r\n"), *MetaDataParams);
		StaticDefinitions.Log (TEXT("\t};\r\n"));

		GeneratedClassRegisterFunctionText.Logf(TEXT("\t};\r\n"));
		GeneratedClassRegisterFunctionText.Log(*StaticDefinitions);

			Out.Logf(TEXT("\tIMPLEMENT_CLASS_NO_AUTO_REGISTRATION(%s);\r\n"), *ClassNameCPP);

		GeneratedClassRegisterFunctionText.Logf(TEXT("\tUClass* %s\r\n"), *SingletonName);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t{\r\n"));
		FString OuterSingletonName = FString::Printf(TEXT("Z_Registration_Info_UClass_%s.OuterSingleton"), *ClassNameCPP);
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\tif (!%s)\r\n"), *OuterSingletonName);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t{\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\tUECodeGen_Private::ConstructUClass(%s, %s::ClassParams);\r\n"), *OuterSingletonName, *StaticsStructName);

		TArray<FString> SparseClassDataTypes;
		ClassDef.GetSparseClassDataTypes(SparseClassDataTypes);
		
		for (const FString& SparseClassDataString : SparseClassDataTypes)
		{
			GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t\t%s->SetSparseClassDataStruct(%s::StaticGet%sScriptStruct());\r\n"), *OuterSingletonName, *ClassNameCPP, *SparseClassDataString);
		}

		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\t}\r\n"));
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t\treturn %s;\r\n"), *OuterSingletonName);
		GeneratedClassRegisterFunctionText.Logf(TEXT("\t}\r\n"));

		Out.Log(*GeneratedClassRegisterFunctionText);
	}

	if (OutFriendText.Len() && bIsNoExport)
	{
		Out.Logf(TEXT("\t/* friend declarations for pasting into noexport class %s\r\n"), *ClassNameCPP);
		Out.Log(OutFriendText);
		Out.Logf(TEXT("\t*/\r\n"));
		OutFriendText.Reset();
	}

	FString SingletonName = ClassDef.GetSingletonName(true);
	SingletonName.ReplaceInline(TEXT("()"), TEXT(""), ESearchCase::CaseSensitive); // function address

	// Append base class' hash at the end of the generated code, this will force update derived classes
	// when base class changes during hot-reload.
	uint32 BaseClassHash = 0;
	FUnrealClassDefinitionInfo* SuperClassDef = ClassDef.GetSuperClass();
	if (SuperClassDef && !SuperClassDef->HasAnyClassFlags(CLASS_Intrinsic))
	{
		BaseClassHash = SuperClassDef->GetHash(ClassDef);
	}

	FUHTStringBuilder HashBuilder;
	HashBuilder.Logf(TEXT("\r\n// %u\r\n"), BaseClassHash);

	// Append info for the sparse class data struct onto the text to be hashed
	TArray<FString> SparseClassDataTypes;
	ClassDef.GetSparseClassDataTypes(SparseClassDataTypes);

	for (const FString& SparseClassDataString : SparseClassDataTypes)
	{
		if (FUnrealScriptStructDefinitionInfo* SparseScriptStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*SparseClassDataString))
		{
			HashBuilder.Logf(TEXT("%s\r\n"), *SparseScriptStructDef->GetName());
			for (FUnrealPropertyDefinitionInfo* ChildDef : TUHTFieldRange<FUnrealPropertyDefinitionInfo>(*SparseScriptStructDef))
			{
				HashBuilder.Logf(TEXT("%s %s\r\n"), *ChildDef->GetCPPType(), *ChildDef->GetNameWithDeprecated());
			}
		}
	}

	if (bIncludeDebugOutput)
	{
		Out.Log(TEXT("#if 0\r\n"));
		Out.Log(HashBuilder);
		Out.Log(TEXT("#endif\r\n"));
	}

	GeneratedClassRegisterFunctionText.Log(HashBuilder);

	// Calculate generated class initialization code hash so that we know when it changes after hot-reload
	uint32 ClassHash = GenerateTextHash(*GeneratedClassRegisterFunctionText);
	ClassDef.SetHash(ClassHash);

	Out.Logf(TEXT("\ttemplate<> %sUClass* StaticClass<%s>()\r\n"), *GetAPIString(), *ClassNameCPP);
	Out.Logf(TEXT("\t{\r\n"));
	Out.Logf(TEXT("\t\treturn %s::StaticClass();\r\n"), *ClassNameCPP);
	Out.Logf(TEXT("\t}\r\n"));

	if (ClassHasReplicatedProperties(ClassDef))
	{
		Out.Logf(TEXT(
			"\r\n"
			"\tvoid %s::ValidateGeneratedRepEnums(const TArray<struct FRepRecord>& ClassReps) const\r\n"
			"\t{\r\n"
		), *ClassNameCPP);

		FUHTStringBuilder NameBuilder;

		FUHTStringBuilder ValidationBuilder;
		ValidationBuilder.Log(TEXT("\t\tconst bool bIsValid = true"));

		for (int32 i = ClassDef.GetFirstOwnedClassRep(); i < ClassDef.GetClassReps().Num(); ++i)
		{
			const FUnrealPropertyDefinitionInfo* PropertyDef = ClassDef.GetClassReps()[i];
			const FString PropertyName = PropertyDef->GetName();

			NameBuilder.Logf(TEXT("\t\tstatic const FName Name_%s(TEXT(\"%s\"));\r\n"), *PropertyName, *PropertyName);

			if (!PropertyDef->IsStaticArray())
			{
				ValidationBuilder.Logf(TEXT("\r\n\t\t\t&& Name_%s == ClassReps[(int32)ENetFields_Private::%s].Property->GetFName()"), *PropertyName, *PropertyName);
			}
			else
			{
				ValidationBuilder.Logf(TEXT("\r\n\t\t\t&& Name_%s == ClassReps[(int32)ENetFields_Private::%s_STATIC_ARRAY].Property->GetFName()"), *PropertyName, *PropertyName);
			}
		}

		ValidationBuilder.Log(TEXT(";\r\n"));

		Out.Logf(TEXT(
			"%s\r\n" // NameBuilder
			"%s\r\n" // ValidationBuilder
			"\t\tcheckf(bIsValid, TEXT(\"UHT Generated Rep Indices do not match runtime populated Rep Indices for properties in %s\"));\r\n"
			"\t}\r\n"
		), *NameBuilder, *ValidationBuilder, *ClassNameCPP);
	}
}

void FNativeClassHeaderGenerator::ExportFunction(FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealFunctionDefinitionInfo& FunctionDef, bool bIsNoExport) const
{
	FunctionDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, true);

	FUnrealFunctionDefinitionInfo* SuperFunctionDef = FunctionDef.GetSuperFunction();

	const bool bIsEditorOnlyFunction = FunctionDef.HasAnyFunctionFlags(FUNC_EditorOnly);

	bool bIsDelegate = FunctionDef.HasAnyFunctionFlags(FUNC_Delegate);

	const FString& SingletonName = FunctionDef.GetSingletonName(true);
	FString StaticsStructName = FunctionDef.GetSingletonNameChopped(true) + TEXT("_Statics");

	FUHTStringBuilder CurrentFunctionText;
	FUHTStringBuilder StaticDefinitions;

	// Begin wrapping editor only functions.  Note: This should always be the first step!
	if (bIsEditorOnlyFunction)
	{
		CurrentFunctionText.Logf(BeginEditorOnlyGuard);
	}

	CurrentFunctionText.Logf(TEXT("\tstruct %s\r\n"), *StaticsStructName);
	CurrentFunctionText.Log (TEXT("\t{\r\n"));

	bool bParamsInStatic = bIsNoExport || !FunctionDef.HasAnyFunctionFlags(FUNC_Event); // non-events do not export a params struct, so lets do that locally for offset determination
	if (bParamsInStatic)
	{
		TArray<FUnrealScriptStructDefinitionInfo*> StructDefs = FindNoExportStructs(&FunctionDef);
		for (FUnrealScriptStructDefinitionInfo* StructDef : StructDefs)
		{
			ExportMirrorsForNoexportStruct(CurrentFunctionText, *StructDef, /*Indent=*/ 2);
		}

		ExportEventParm(CurrentFunctionText, OutReferenceGatherers.ForwardDeclarations, FunctionDef, /*Indent=*/ 2, /*bOutputConstructor=*/ false, EExportingState::TypeEraseDelegates);
	}

	FUnrealFieldDefinitionInfo* FieldOuterDef = UHTCast<FUnrealFieldDefinitionInfo>(FunctionDef.GetOuter());

	FString OuterFunc;
	if (FUnrealObjectDefinitionInfo* OuterDef = FunctionDef.GetOuter())
	{
		FUnrealPackageDefinitionInfo* OuterPackageDef = UHTCast<FUnrealPackageDefinitionInfo>(OuterDef);
		OuterFunc = OuterPackageDef ? GetPackageSingletonNameFuncAddr(*OuterPackageDef, OutReferenceGatherers.UniqueCrossModuleReferences) : GetSingletonNameFuncAddr(FunctionDef.GetOwnerClass(), OutReferenceGatherers.UniqueCrossModuleReferences);
	}
	else
	{
		OuterFunc = TEXT("nullptr");
	}
	
	FString StructureSize;
	if (FunctionDef.GetProperties().Num())
	{
		FUnrealFunctionDefinitionInfo* TempFunctionDef = &FunctionDef;
		while (TempFunctionDef->GetSuperFunction())
		{
			TempFunctionDef = TempFunctionDef->GetSuperFunction();
		}
		FString FunctionName = TempFunctionDef->GetName();
		if (TempFunctionDef->HasAnyFunctionFlags(FUNC_Delegate))
		{
			FunctionName.LeftChopInline(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH, false);
		}

		if (bParamsInStatic)
		{
			StructureSize = FString::Printf(TEXT("sizeof(%s::%s)"), *StaticsStructName, *GetEventStructParamsName(*TempFunctionDef->GetOuter(), *FunctionName));
		}
		else
		{
			StructureSize = FString::Printf(TEXT("sizeof(%s)"), *GetEventStructParamsName(*TempFunctionDef->GetOuter(), *FunctionName));
		}
	}
	else
	{
		StructureSize = TEXT("0");
	}

	bool bIsSparse = FunctionDef.GetFunctionType() == EFunctionType::SparseDelegate;
	const TCHAR* UFunctionObjectFlags = TEXT("RF_Public|RF_Transient|RF_MarkAsNative");

	TTuple<FString, FString> PropertyRange = OutputProperties(CurrentFunctionText, StaticDefinitions, OutReferenceGatherers, *FString::Printf(TEXT("%s::"), *StaticsStructName), FunctionDef, TEXT("\t\t"), TEXT("\t"));

	const FFuncInfo&     FunctionData = FunctionDef.GetFunctionData();
	const bool           bIsNet       = FunctionDef.HasAnyFunctionFlags(FUNC_NetRequest | FUNC_NetResponse);

	FString MetaDataParams = OutputMetaDataCodeForObject(CurrentFunctionText, StaticDefinitions, FunctionDef, *FString::Printf(TEXT("%s::Function_MetaDataParams"), *StaticsStructName), TEXT("\t\t"), TEXT("\t"));

	CurrentFunctionText.Log(TEXT("\t\tstatic const UECodeGen_Private::FFunctionParams FuncParams;\r\n"));

	StaticDefinitions.Logf(
		TEXT("\tconst UECodeGen_Private::FFunctionParams %s::FuncParams = { (UObject*(*)())%s, %s, %s, %s, %s, %s, %s, %s, %s, (EFunctionFlags)0x%08X, %d, %d, %s };\r\n"),
		*StaticsStructName,
		*OuterFunc,
		*GetSingletonNameFuncAddr(SuperFunctionDef, OutReferenceGatherers.UniqueCrossModuleReferences),
		*CreateUTF8LiteralString(FunctionDef.GetName()),
		(bIsSparse ? *CreateUTF8LiteralString(FunctionDef.GetSparseOwningClassName().ToString()) : TEXT("nullptr")),
		(bIsSparse ? *CreateUTF8LiteralString(FunctionDef.GetSparseDelegateName().ToString()) : TEXT("nullptr")),
		*StructureSize,
		*PropertyRange.Get<0>(),
		*PropertyRange.Get<1>(),
		UFunctionObjectFlags,
		(uint32)FunctionDef.GetFunctionFlags(),
		bIsNet ? FunctionData.RPCId : 0,
		bIsNet ? FunctionData.RPCResponseId : 0,
		*MetaDataParams
	);

	CurrentFunctionText.Log(TEXT("\t};\r\n"));
	CurrentFunctionText.Log(*StaticDefinitions);

	CurrentFunctionText.Logf(TEXT("\tUFunction* %s\r\n"), *SingletonName);
	CurrentFunctionText.Log (TEXT("\t{\r\n"));
		CurrentFunctionText.Logf(TEXT("\t\tstatic UFunction* ReturnFunction = nullptr;\r\n"));
	CurrentFunctionText.Logf(TEXT("\t\tif (!ReturnFunction)\r\n"));
	CurrentFunctionText.Logf(TEXT("\t\t{\r\n"));
	CurrentFunctionText.Logf(TEXT("\t\t\tUECodeGen_Private::ConstructUFunction(&ReturnFunction, %s::FuncParams);\r\n"), *StaticsStructName);
	CurrentFunctionText.Log (TEXT("\t\t}\r\n"));
	CurrentFunctionText.Log (TEXT("\t\treturn ReturnFunction;\r\n"));
	CurrentFunctionText.Log (TEXT("\t}\r\n"));

	// End wrapping editor only functions.  Note: This should always be the last step!
	if (bIsEditorOnlyFunction)
	{
		CurrentFunctionText.Logf(EndEditorOnlyGuard);
	}

	uint32 FunctionHash = GenerateTextHash(*CurrentFunctionText);
	FunctionDef.SetHash(FunctionHash);
	Out.Log(CurrentFunctionText);
}

void FNativeClassHeaderGenerator::ExportNatives(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef)
{
	const FString ClassCPPName = ClassDef.GetAlternateNameCPP();
	const FString TypeName = ClassDef.GetAlternateNameCPP(ClassDef.HasAnyClassFlags(CLASS_Interface));

	Out.Logf(TEXT("\tvoid %s::StaticRegisterNatives%s()\r\n"), *ClassCPPName, *ClassCPPName);
	Out.Log(TEXT("\t{\r\n"));

	{
		bool bAllEditorOnly = true;

		TArray<TTuple<FUnrealFunctionDefinitionInfo*, FString>> NamedFunctionsToExport;
		for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : ClassDef.GetFunctions())
		{
			if (FunctionDef->HasSpecificFunctionFlags(FUNC_Native | FUNC_NetRequest, FUNC_Native))
			{
				FString FunctionName = CreateUTF8LiteralString(FunctionDef->GetName());
				NamedFunctionsToExport.Emplace(&*FunctionDef, MoveTemp(FunctionName));

				if (!FunctionDef->HasAnyFunctionFlags(FUNC_EditorOnly))
				{
					bAllEditorOnly = false;
				}
			}
		}

		Algo::SortBy(NamedFunctionsToExport, [](const TTuple<FUnrealFunctionDefinitionInfo*, FString>& Pair){ return Pair.Get<0>()->GetFName(); }, FNameLexicalLess());

		if (NamedFunctionsToExport.Num() > 0)
		{
			FMacroBlockEmitter EditorOnly(Out, TEXT("WITH_EDITOR"));
			EditorOnly(bAllEditorOnly);

			Out.Logf(TEXT("\t\tUClass* Class = %s::StaticClass();\r\n"), *ClassCPPName);
			Out.Log(TEXT("\t\tstatic const FNameNativePtrPair Funcs[] = {\r\n"));

			for (const TTuple<FUnrealFunctionDefinitionInfo*, FString>& Func : NamedFunctionsToExport)
			{
				FUnrealFunctionDefinitionInfo* FunctionDef = Func.Get<0>();

				EditorOnly(FunctionDef->HasAnyFunctionFlags(FUNC_EditorOnly));

				Out.Logf(
					TEXT("\t\t\t{ %s, &%s::exec%s },\r\n"),
					*Func.Get<1>(),
					*TypeName,
					*FunctionDef->GetName()
				);
			}

			EditorOnly(bAllEditorOnly);

			Out.Log(TEXT("\t\t};\r\n"));
			Out.Logf(TEXT("\t\tFNativeFunctionRegistrar::RegisterFunctions(Class, Funcs, UE_ARRAY_COUNT(Funcs));\r\n"));
		}
	}

	Out.Logf(TEXT("\t}\r\n"));
}

void FNativeClassHeaderGenerator::ExportInterfaceCallFunctions(FOutputDevice& OutCpp, FUHTStringBuilder& Out, FReferenceGatherers& OutReferenceGatherers, const TArray<FUnrealFunctionDefinitionInfo*>& CallbackFunctions, const TCHAR* ClassName) const
{
	const FString& APIString = GetAPIString();

	for (FUnrealFunctionDefinitionInfo* FunctionDef : CallbackFunctions)
	{
		FString FunctionName = FunctionDef->GetName();

		const FFuncInfo& FunctionData = FunctionDef->GetFunctionData();
		const TCHAR* ConstQualifier = FunctionDef->HasAllFunctionFlags(FUNC_Const) ? TEXT("const ") : TEXT("");
		FString ExtraParam = FString::Printf(TEXT("%sUObject* O"), ConstQualifier);

		ExportNativeFunctionHeader(Out, OutReferenceGatherers.ForwardDeclarations, *FunctionDef, FunctionData, EExportFunctionType::Interface, EExportFunctionHeaderStyle::Declaration, *ExtraParam, *APIString);
		Out.Logf( TEXT(";" LINE_TERMINATOR_ANSI) );

		FString FunctionNameName = FString::Printf(TEXT("NAME_%s_%s"), *UHTCastChecked<FUnrealStructDefinitionInfo>(FunctionDef->GetOuter()).GetAlternateNameCPP(), *FunctionName);
		OutCpp.Logf(TEXT("\tstatic FName %s = FName(TEXT(\"%s\"));" LINE_TERMINATOR_ANSI), *FunctionNameName, *FunctionDef->GetFName().ToString());

		ExportNativeFunctionHeader(OutCpp, OutReferenceGatherers.ForwardDeclarations, *FunctionDef, FunctionData, EExportFunctionType::Interface, EExportFunctionHeaderStyle::Definition, *ExtraParam, *APIString);
		OutCpp.Logf( TEXT(LINE_TERMINATOR_ANSI "\t{" LINE_TERMINATOR_ANSI) );

		OutCpp.Logf(TEXT("\t\tcheck(O != NULL);" LINE_TERMINATOR_ANSI));
		OutCpp.Logf(TEXT("\t\tcheck(O->GetClass()->ImplementsInterface(U%s::StaticClass()));" LINE_TERMINATOR_ANSI), ClassName);

		FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(*FunctionDef);

		// See if we need to create Parms struct
		const bool bHasParms = Parameters.HasParms();
		if (bHasParms)
		{
			FString EventParmStructName = GetEventStructParamsName(*FunctionDef->GetOuter(), *FunctionName);
			OutCpp.Logf(TEXT("\t\t%s Parms;" LINE_TERMINATOR_ANSI), *EventParmStructName);
		}

		OutCpp.Logf(TEXT("\t\tUFunction* const Func = O->FindFunction(%s);" LINE_TERMINATOR_ANSI), *FunctionNameName);
		OutCpp.Log(TEXT("\t\tif (Func)" LINE_TERMINATOR_ANSI));
		OutCpp.Log(TEXT("\t\t{" LINE_TERMINATOR_ANSI));

		// code to populate Parms struct
		for (FUnrealPropertyDefinitionInfo* ParamDef : Parameters.Parms)
		{
			const FString ParamName = ParamDef->GetNameWithDeprecated();
			OutCpp.Logf(TEXT("\t\t\tParms.%s=%s;" LINE_TERMINATOR_ANSI), *ParamName, *ParamName);
		}

		const FString ObjectRef = FunctionDef->HasAllFunctionFlags(FUNC_Const) ? FString::Printf(TEXT("const_cast<UObject*>(O)")) : TEXT("O");
		OutCpp.Logf(TEXT("\t\t\t%s->ProcessEvent(Func, %s);" LINE_TERMINATOR_ANSI), *ObjectRef, bHasParms ? TEXT("&Parms") : TEXT("NULL"));

		for (FUnrealPropertyDefinitionInfo* ParamDef : Parameters.Parms)
		{
			if(ParamDef->HasAllPropertyFlags(CPF_OutParm) && !ParamDef->HasAnyPropertyFlags(CPF_ConstParm|CPF_ReturnParm))
			{
				const FString ParamName = ParamDef->GetNameWithDeprecated();
				OutCpp.Logf(TEXT("\t\t\t%s=Parms.%s;" LINE_TERMINATOR_ANSI), *ParamName, *ParamName);
			}
		}

		OutCpp.Log(TEXT("\t\t}" LINE_TERMINATOR_ANSI));


		// else clause to call back into native if it's a BlueprintNativeEvent
		if (FunctionDef->HasAnyFunctionFlags(FUNC_Native))
		{
			OutCpp.Logf(TEXT("\t\telse if (auto I = (%sI%s*)(O->GetNativeInterfaceAddress(U%s::StaticClass())))" LINE_TERMINATOR_ANSI), ConstQualifier, ClassName, ClassName);
			OutCpp.Log(TEXT("\t\t{" LINE_TERMINATOR_ANSI));

			OutCpp.Log(TEXT("\t\t\t"));
			if (Parameters.Return)
			{
				OutCpp.Log(TEXT("Parms.ReturnValue = "));
			}

			OutCpp.Logf(TEXT("I->%s_Implementation("), *FunctionName);

			bool bFirst = true;
			for (FUnrealPropertyDefinitionInfo* ParamDef : Parameters.Parms)
			{
				if (!bFirst)
				{
					OutCpp.Logf(TEXT(","));
				}
				bFirst = false;

				OutCpp.Log(*ParamDef->GetName());
			}

			OutCpp.Logf(TEXT(");" LINE_TERMINATOR_ANSI));

			OutCpp.Logf(TEXT("\t\t}" LINE_TERMINATOR_ANSI));
		}

		if (Parameters.Return)
		{
			OutCpp.Logf(TEXT("\t\treturn Parms.ReturnValue;" LINE_TERMINATOR_ANSI));
		}

		OutCpp.Logf(TEXT("\t}" LINE_TERMINATOR_ANSI));
	}
}

/**
 * Gets preprocessor string to emit GENERATED_U*_BODY() macro is deprecated.
 *
 * @param MacroName Name of the macro to be deprecated.
 *
 * @returns Preprocessor string to emit the message.
 */
FString GetGeneratedMacroDeprecationWarning(const TCHAR* MacroName)
{
	// Deprecation warning is disabled right now. After people get familiar with the new macro it should be re-enabled.
	//return FString() + TEXT("EMIT_DEPRECATED_WARNING_MESSAGE(\"") + MacroName + TEXT("() macro is deprecated. Please use GENERATED_BODY() macro instead.\")") LINE_TERMINATOR;
	return TEXT("");
}

/**
 * Returns a string with access specifier that was met before parsing GENERATED_BODY() macro to preserve it.
 *
 * @param Class Class for which to return the access specifier.
 *
 * @returns Access specifier string.
 */
FString GetPreservedAccessSpecifierString(FUnrealClassDefinitionInfo& ClassDef)
{
	FString PreservedAccessSpecifier;
	switch (ClassDef.GetGeneratedBodyMacroAccessSpecifier())
	{
	case EAccessSpecifier::ACCESS_Private:
		PreservedAccessSpecifier = "private:";
		break;
	case EAccessSpecifier::ACCESS_Protected:
		PreservedAccessSpecifier = "protected:";
		break;
	case EAccessSpecifier::ACCESS_Public:
		PreservedAccessSpecifier = "public:";
		break;
	case EAccessSpecifier::ACCESS_NotAnAccessSpecifier :
		PreservedAccessSpecifier = FString::Printf(TEXT("static_assert(false, \"Unknown access specifier for GENERATED_BODY() macro in class %s.\");"), *ClassDef.GetName());
		break;
	}

	return PreservedAccessSpecifier + LINE_TERMINATOR;
}

void WriteMacro(FOutputDevice& Output, const FString& MacroName, FString MacroContent)
{
	Output.Log(Macroize(*MacroName, MoveTemp(MacroContent)));
}

void FNativeClassHeaderGenerator::ExportClassFromSourceFileInner(
	FOutputDevice&           OutGeneratedHeaderText,
	FOutputDevice&           OutCpp,
	FOutputDevice&           OutDeclarations,
	FReferenceGatherers&     OutReferenceGatherers,
	FUnrealClassDefinitionInfo&			 ClassDef,
	const FUnrealSourceFile& SourceFile,
	EExportClassOutFlags&    OutFlags
) const
{
	FUHTStringBuilder StandardUObjectConstructorsMacroCall;
	FUHTStringBuilder EnhancedUObjectConstructorsMacroCall;


	FUnrealClassDefinitionInfo* SuperClassDef = ClassDef.GetSuperClass();

	// C++ -> VM stubs (native function execs)
	FUHTStringBuilder ClassMacroCalls;
	FUHTStringBuilder ClassNoPureDeclsMacroCalls;
	ExportNativeFunctions(OutGeneratedHeaderText, OutCpp, ClassMacroCalls, ClassNoPureDeclsMacroCalls, OutReferenceGatherers, SourceFile, ClassDef);

	// Get Callback functions
	TArray<FUnrealFunctionDefinitionInfo*> CallbackFunctions;
	for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : ClassDef.GetFunctions())
	{
		if (FunctionDef->HasAnyFunctionFlags(FUNC_Event) && FunctionDef->GetSuperFunction() == nullptr)
		{
			CallbackFunctions.Add(&*FunctionDef);
		}
	}

	FUHTStringBuilder PrologMacroCalls;
	if (CallbackFunctions.Num() != 0)
	{
		Algo::SortBy(CallbackFunctions, [](FUnrealFunctionDefinitionInfo* Obj) { return Obj->GetName(); });

		FUHTStringBuilder UClassMacroContent;

		// export parameters structs for all events and delegates
		for (FUnrealFunctionDefinitionInfo* FunctionDef : CallbackFunctions)
		{
			//ExportEventParm(UClassMacroContent, OutReferenceGatherers.ForwardDeclarations, *FunctionDef, /*Indent=*/ 1, /*bOutputConstructor=*/ true, EExportingState::Normal);
			FUHTStringBuilder Output;
			ExportEventParm(Output, OutReferenceGatherers.ForwardDeclarations, *FunctionDef, /*Indent=*/ 1, /*bOutputConstructor=*/ true, EExportingState::Normal);
			OutCpp.Log(Output);
		}

		//FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_EVENT_PARMS"));
		//WriteMacro(OutGeneratedHeaderText, MacroName, UClassMacroContent);
		//PrologMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		// VM -> C++ proxies (events and delegates).
		FOutputDeviceNull NullOutput;
		FOutputDevice& CallbackOut = ClassDef.IsNoExport() ? NullOutput : OutCpp;
		FString CallbackWrappersMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_CALLBACK_WRAPPERS"));
		ExportCallbackFunctions(
			OutGeneratedHeaderText,
			CallbackOut,
			OutReferenceGatherers.ForwardDeclarations,
			CallbackFunctions,
			*CallbackWrappersMacroName,
			ClassDef.HasAnyClassFlags(CLASS_Interface) ? EExportCallbackType::Interface : EExportCallbackType::Class,
			*GetAPIString()
		);

		ClassMacroCalls.Logf(TEXT("\t%s\r\n"), *CallbackWrappersMacroName);
		ClassNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *CallbackWrappersMacroName);
	}

	// Class definition.
	if (!ClassDef.IsNoExport())
	{
		ExportNatives(OutCpp, ClassDef);
	}

	FUHTStringBuilder FriendText;
	ExportNativeGeneratedInitCode(OutCpp, OutDeclarations, OutReferenceGatherers, SourceFile, ClassDef, FriendText);

	// the name for the C++ version of the UClass
	const FString ClassCPPName = ClassDef.GetAlternateNameCPP();
	const FString SuperClassCPPName = (SuperClassDef ? SuperClassDef->GetAlternateNameCPP() : TEXT("None"));

	FString APIArg = PackageDef.GetShortUpperName();
	if (!ClassDef.HasAnyClassFlags(CLASS_MinimalAPI))
	{
		APIArg = TEXT("NO");
	}

	FString GeneratedSerializeFunctionCPP;
	FString GeneratedSerializeFunctionHeaderMacroName;

	// Only write out adapters if the user has provided one or the other of the Serialize overloads
	if (FMath::CountBits((uint32)ClassDef.GetArchiveType()) == 1)
	{
		FUHTStringBuilder Boilerplate, BoilerPlateCPP;
		const TCHAR* MacroNameHeader;
		const TCHAR* MacroNameCPP;
		GeneratedSerializeFunctionHeaderMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_ARCHIVESERIALIZER"));

		if (ClassDef.GetArchiveType() == ESerializerArchiveType::StructuredArchiveRecord)
		{
			MacroNameHeader = TEXT("DECLARE_FARCHIVE_SERIALIZER");
			MacroNameCPP = TEXT("IMPLEMENT_FARCHIVE_SERIALIZER");
		}
		else
		{
			MacroNameHeader = TEXT("DECLARE_FSTRUCTUREDARCHIVE_SERIALIZER");
			MacroNameCPP = TEXT("IMPLEMENT_FSTRUCTUREDARCHIVE_SERIALIZER");
		}

		// if the existing Serialize function was wrapped in a compiler define directive, we need to replicate that on the generated function
		if (ClassDef.GetEnclosingDefine().Len())
		{
			OutGeneratedHeaderText.Logf(TEXT("#if %s\r\n"), *ClassDef.GetEnclosingDefine());
			BoilerPlateCPP.Logf(TEXT("#if %s\r\n"), *ClassDef.GetEnclosingDefine());
		}

		Boilerplate.Logf(TEXT("\t%s(%s, %s_API)\r\n"), MacroNameHeader, *ClassCPPName, *APIArg);
		OutGeneratedHeaderText.Log(Macroize(*GeneratedSerializeFunctionHeaderMacroName, *Boilerplate));
		BoilerPlateCPP.Logf(TEXT("\t%s(%s)\r\n"), MacroNameCPP, *ClassCPPName);

		if (ClassDef.GetEnclosingDefine().Len())
		{
			OutGeneratedHeaderText.Logf(TEXT("#else\r\n"));
			OutGeneratedHeaderText.Log(Macroize(*GeneratedSerializeFunctionHeaderMacroName, TEXT("")));
			OutGeneratedHeaderText.Logf(TEXT("#endif\r\n"));
			BoilerPlateCPP.Logf(TEXT("#endif\r\n"));
		}

		GeneratedSerializeFunctionCPP = BoilerPlateCPP;
	}

	{
		FUHTStringBuilder Boilerplate;

		// Export the class's native function registration.
		Boilerplate.Logf(TEXT("private:\r\n"));
		Boilerplate.Logf(TEXT("\tstatic void StaticRegisterNatives%s();\r\n"), *ClassCPPName);
		Boilerplate.Log(*FriendText);
		Boilerplate.Logf(TEXT("public:\r\n"));

		const bool bCastedClass = ClassDef.HasAnyCastFlags(CASTCLASS_AllFlags) && SuperClassDef && ClassDef.GetClassCastFlags() != SuperClassDef->GetClassCastFlags();

		Boilerplate.Logf(TEXT("\tDECLARE_CLASS(%s, %s, COMPILED_IN_FLAGS(%s%s), %s, TEXT(\"%s\"), %s_API)\r\n"),
			*ClassCPPName,
			*SuperClassCPPName,
			ClassDef.HasAnyClassFlags(CLASS_Abstract) ? TEXT("CLASS_Abstract") : TEXT("0"),
			*GetClassFlagExportText(ClassDef),
			bCastedClass ? *FString::Printf(TEXT("CASTCLASS_%s"), *ClassCPPName) : TEXT("CASTCLASS_None"),
			*ClassDef.GetTypePackageName(),
			*APIArg);

		Boilerplate.Logf(TEXT("\tDECLARE_SERIALIZER(%s)\r\n"), *ClassCPPName);

		// Add the serialization function declaration if we generated one
		if (GeneratedSerializeFunctionHeaderMacroName.Len() > 0)
		{
			Boilerplate.Logf(TEXT("\t%s\r\n"), *GeneratedSerializeFunctionHeaderMacroName);
		}

		if (SuperClassDef && ClassDef.GetClassWithin() != SuperClassDef->GetClassWithin())
		{
			Boilerplate.Logf(TEXT("\tDECLARE_WITHIN(%s)\r\n"), *ClassDef.GetClassWithin()->GetAlternateNameCPP());
		}

		if (ClassDef.HasAnyClassFlags(CLASS_Interface))
		{
			ExportConstructorsMacros(OutGeneratedHeaderText, OutCpp, StandardUObjectConstructorsMacroCall, EnhancedUObjectConstructorsMacroCall, SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine()), ClassDef, *APIArg);

			FString InterfaceMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_GENERATED_UINTERFACE_BODY"));
			OutGeneratedHeaderText.Log(Macroize(*(InterfaceMacroName + TEXT("()")), *Boilerplate));

			int32 ClassGeneratedBodyLine = ClassDef.GetGeneratedBodyLine();

			FString DeprecationWarning = GetGeneratedMacroDeprecationWarning(TEXT("GENERATED_UINTERFACE_BODY"));

			const TCHAR* Offset = TEXT("\t");

			OutGeneratedHeaderText.Log(
				Macroize(
					*SourceFile.GetGeneratedBodyMacroName(ClassGeneratedBodyLine, true),
					FString::Printf(TEXT("\t%s\t%s\t%s()" LINE_TERMINATOR_ANSI "%s\t%s")
						, *DeprecationWarning
						, DisableDeprecationWarnings
						, *InterfaceMacroName
						, *StandardUObjectConstructorsMacroCall
						, EnableDeprecationWarnings
					)
				)
			);

			OutGeneratedHeaderText.Log(
				Macroize(
					*SourceFile.GetGeneratedBodyMacroName(ClassGeneratedBodyLine),
					FString::Printf(TEXT("\t%s\t%s()" LINE_TERMINATOR_ANSI "%s%s\t%s")
						, DisableDeprecationWarnings
						, *InterfaceMacroName
						, *EnhancedUObjectConstructorsMacroCall
						, *GetPreservedAccessSpecifierString(ClassDef)
						, EnableDeprecationWarnings
					)
				)
			);

			// =============================================
			// Export the pure interface version of the class

			// the name of the pure interface class
			FString InterfaceCPPName = ClassDef.GetAlternateNameCPP(true);
			FString SuperInterfaceCPPName = SuperClassDef->GetAlternateNameCPP(true);

			// Thunk functions
			FUHTStringBuilder InterfaceBoilerplate;

			InterfaceBoilerplate.Logf(TEXT("protected:\r\n\tvirtual ~%s() {}\r\n"), *InterfaceCPPName);
			InterfaceBoilerplate.Logf(TEXT("public:\r\n\ttypedef %s UClassType;\r\n"), *ClassCPPName);
			InterfaceBoilerplate.Logf(TEXT("\ttypedef %s ThisClass;\r\n"), *InterfaceCPPName);

			ExportInterfaceCallFunctions(OutCpp, InterfaceBoilerplate, OutReferenceGatherers, CallbackFunctions, *ClassDef.GetName());

			// we'll need a way to get to the UObject portion of a native interface, so that we can safely pass native interfaces
			// to script VM functions
			if (SuperClassDef->IsChildOf(*GUInterfaceDef))
			{
				// Note: This used to be declared as a pure virtual function, but it was changed here in order to allow the Blueprint nativization process
				// to detect C++ interface classes that explicitly declare pure virtual functions via type traits. This code will no longer trigger that check.
				InterfaceBoilerplate.Logf(TEXT("\tvirtual UObject* _getUObject() const { return nullptr; }\r\n"));
			}

			if (ClassHasReplicatedProperties(ClassDef))
			{
				WriteReplicatedMacroData(*ClassCPPName, *APIArg, ClassDef, InterfaceBoilerplate, SourceFile, OutFlags);
			}

			FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_INCLASS_IINTERFACE_NO_PURE_DECLS"));
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, InterfaceBoilerplate);
			ClassNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);

			FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_INCLASS_IINTERFACE"));
			WriteMacro(OutGeneratedHeaderText, MacroName, InterfaceBoilerplate);
			ClassMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);
		}
		else
		{
			// export the class's config name
			if (SuperClassDef && ClassDef.GetClassConfigName() != NAME_None && ClassDef.GetClassConfigName() != SuperClassDef->GetClassConfigName())
			{
				Boilerplate.Logf(TEXT("\tstatic const TCHAR* StaticConfigName() {return TEXT(\"%s\");}\r\n\r\n"), *ClassDef.GetClassConfigName().ToString());
			}

			// export implementation of _getUObject for classes that implement interfaces
			for (FUnrealStructDefinitionInfo::FBaseStructInfo& BaseStruct : ClassDef.GetBaseStructInfos())
			{
				if (FUnrealClassDefinitionInfo* BaseClass = UHTCast<FUnrealClassDefinitionInfo>(BaseStruct.Struct); BaseClass != nullptr && BaseClass->IsInterface())
				{
					Boilerplate.Logf(TEXT("\tvirtual UObject* _getUObject() const override { return const_cast<%s*>(this); }\r\n"), *ClassCPPName);
					break;
				}
			}

			if (ClassHasReplicatedProperties(ClassDef))
			{
				WriteReplicatedMacroData(*ClassCPPName, *APIArg, ClassDef, Boilerplate, SourceFile, OutFlags);
			}

			{
				FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_INCLASS_NO_PURE_DECLS"));
				WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, Boilerplate);
				ClassNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);

				FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_INCLASS"));
				WriteMacro(OutGeneratedHeaderText, MacroName, Boilerplate);
				ClassMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

				ExportConstructorsMacros(OutGeneratedHeaderText, OutCpp, StandardUObjectConstructorsMacroCall, EnhancedUObjectConstructorsMacroCall, SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine()), ClassDef, *APIArg);
				ExportFieldNotify(OutGeneratedHeaderText, OutCpp, StandardUObjectConstructorsMacroCall, EnhancedUObjectConstructorsMacroCall, SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine()), ClassDef);
			}
		}
	}

	{
		FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetPrologLine(), TEXT("_PROLOG"));
		WriteMacro(OutGeneratedHeaderText, MacroName, PrologMacroCalls);
	}

	{
		const TCHAR* Public = TEXT("public:" LINE_TERMINATOR_ANSI);

		const bool bIsIInterface = ClassDef.HasAnyClassFlags(CLASS_Interface);

		const TCHAR* MacroName;
		FString DeprecationWarning;
		FString LegacyGeneratedBody;
		FString GeneratedBody;
		int32 GeneratedBodyLine;


		if (bIsIInterface)
		{
			MacroName = TEXT("GENERATED_IINTERFACE_BODY()");
			GeneratedBodyLine = ClassDef.GetInterfaceGeneratedBodyLine();
			LegacyGeneratedBody = ClassMacroCalls;
			GeneratedBody = ClassNoPureDeclsMacroCalls;
		}
		else
		{
			MacroName = TEXT("GENERATED_UCLASS_BODY()");
			DeprecationWarning = GetGeneratedMacroDeprecationWarning(MacroName);
			GeneratedBodyLine = ClassDef.GetGeneratedBodyLine();
			LegacyGeneratedBody = FString::Printf(TEXT("%s%s"), *ClassMacroCalls, *StandardUObjectConstructorsMacroCall);
			GeneratedBody = FString::Printf(TEXT("%s%s"), *ClassNoPureDeclsMacroCalls, *EnhancedUObjectConstructorsMacroCall);
		}

		FString WrappedLegacyGeneratedBody = FString::Printf(TEXT("%s%s%s%s%s%s"), *DeprecationWarning, DisableDeprecationWarnings, Public, *LegacyGeneratedBody, Public, EnableDeprecationWarnings);
		FString WrappedGeneratedBody = FString::Printf(TEXT("%s%s%s%s%s"), DisableDeprecationWarnings, Public, *GeneratedBody, *GetPreservedAccessSpecifierString(ClassDef), EnableDeprecationWarnings);

		OutGeneratedHeaderText.Log(Macroize(*SourceFile.GetGeneratedBodyMacroName(GeneratedBodyLine, true), MoveTemp(WrappedLegacyGeneratedBody)));
		OutGeneratedHeaderText.Log(Macroize(*SourceFile.GetGeneratedBodyMacroName(GeneratedBodyLine, false), MoveTemp(WrappedGeneratedBody)));
	}

	// Forward declare the StaticClass specialisation in the header
	OutGeneratedHeaderText.Logf(TEXT("template<> %sUClass* StaticClass<class %s>();\r\n\r\n"), *GetAPIString(), *ClassCPPName);

	// If there is a serialization function implementation for the CPP file, add it now
	if (GeneratedSerializeFunctionCPP.Len())
	{
		OutCpp.Log(GeneratedSerializeFunctionCPP);
	}
}

/**
* Generates private copy-constructor declaration.
*
* @param Out Output device to generate to.
* @param Class Class to generate constructor for.
* @param API API string for this constructor.
*/
void ExportCopyConstructorDefinition(FOutputDevice& Out, const TCHAR* API, const TCHAR* ClassCPPName)
{
	Out.Logf(TEXT("private:\r\n"));
	Out.Logf(TEXT("\t/** Private move- and copy-constructors, should never be used */\r\n"));
	Out.Logf(TEXT("\t%s_API %s(%s&&);\r\n"), API, ClassCPPName, ClassCPPName);
	Out.Logf(TEXT("\t%s_API %s(const %s&);\r\n"), API, ClassCPPName, ClassCPPName);
	Out.Logf(TEXT("public:\r\n"));
}

void ExportDestructorDefinition(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!ClassDef.IsDestructorDeclared())
	{
		Out.Logf(TEXT("\t%s_API virtual ~%s();\r\n"), API, ClassCPPName);
	}
}

/**
 * Generates vtable helper caller and eventual constructor body.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate for.
 * @param API API string.
 */
void ExportVTableHelperCtorAndCaller(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!ClassDef.IsCustomVTableHelperConstructorDeclared())
	{
		Out.Logf(TEXT("\tDECLARE_VTABLE_PTR_HELPER_CTOR(%s_API, %s);" LINE_TERMINATOR_ANSI), API, ClassCPPName);
	}
	Out.Logf(TEXT("\tDEFINE_VTABLE_PTR_HELPER_CTOR_CALLER(%s);" LINE_TERMINATOR_ANSI), ClassCPPName);
}

/**
 * Generates standard constructor declaration.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor for.
 * @param API API string for this constructor.
 */
void ExportStandardConstructorsMacro(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!ClassDef.HasCustomConstructor())
	{
		Out.Logf(TEXT("\t/** Standard constructor, called after all reflected properties have been initialized */\r\n"));
		Out.Logf(TEXT("\t%s_API %s(const FObjectInitializer& ObjectInitializer%s);\r\n"), API, ClassCPPName,
			ClassDef.IsDefaultConstructorDeclared() ? TEXT("") : TEXT(" = FObjectInitializer::Get()"));
	}
	if (ClassDef.HasAnyClassFlags(CLASS_Abstract))
	{
		Out.Logf(TEXT("\tDEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
	}
	else
	{
		Out.Logf(TEXT("\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);

	}

	ExportVTableHelperCtorAndCaller(Out, ClassDef, API, ClassCPPName);
	ExportCopyConstructorDefinition(Out, API, ClassCPPName);
	ExportDestructorDefinition(Out, ClassDef, API, ClassCPPName);
}

/**
 * Generates constructor definition.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor for.
 * @param API API string for this constructor.
 */
void ExportConstructorDefinition(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* API, const TCHAR* ClassCPPName)
{
	if (!ClassDef.IsConstructorDeclared())
	{
		Out.Logf(TEXT("\t/** Standard constructor, called after all reflected properties have been initialized */\r\n"));

		// Assume super class has OI constructor, this may not always be true but we should always be able to check this.
		// In any case, it will default to old behaviour before we even checked this.
		bool bSuperClassObjectInitializerConstructorDeclared = true;
		FUnrealClassDefinitionInfo* SuperClassDef = ClassDef.GetSuperClass();
		if (SuperClassDef != nullptr)
		{
			if (SuperClassDef->HasSource() && !SuperClassDef->GetUnrealSourceFile().IsNoExportTypes()) // Don't consider the internal types generated from the engine
			{
				// Since we are dependent on our SuperClass having determined which constructors are defined, 
				// if it is not yet determined we will need to wait on it becoming available. 
				// Since the SourceFile array provided to the ParallelFor is in dependency order and does not allow cyclic dependencies, 
				// we can be certain that another thread has started processing the file containing our SuperClass before this
				// file would have been assigned out,  so we just have to wait
				while (!SuperClassDef->IsConstructorDeclared())
				{
					FPlatformProcess::Sleep(0.01f);
				}

				bSuperClassObjectInitializerConstructorDeclared = SuperClassDef->IsObjectInitializerConstructorDeclared();
			}
		}
		if (bSuperClassObjectInitializerConstructorDeclared)
		{
			Out.Logf(TEXT("\t%s_API %s(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get()) : Super(ObjectInitializer) { };\r\n"), API, ClassCPPName);
			ClassDef.MarkObjectInitializerConstructorDeclared();
		}
		else
		{
			Out.Logf(TEXT("\t%s_API %s() { };\r\n"), API, ClassCPPName);
			ClassDef.MarkDefaultConstructorDeclared();
		}

		ClassDef.MarkConstructorDeclared();
	}
	ExportCopyConstructorDefinition(Out, API, ClassCPPName);
}

/**
 * Generates constructor call definition.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor call definition for.
 */
void ExportDefaultConstructorCallDefinition(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* ClassCPPName)
{
	if (ClassDef.IsObjectInitializerConstructorDeclared())
	{
		if (ClassDef.HasAnyClassFlags(CLASS_Abstract))
		{
			Out.Logf(TEXT("\tDEFINE_ABSTRACT_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
		}
		else
		{
			Out.Logf(TEXT("\tDEFINE_DEFAULT_OBJECT_INITIALIZER_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
		}
	}
	else if (ClassDef.IsDefaultConstructorDeclared())
	{
		if (ClassDef.HasAnyClassFlags(CLASS_Abstract))
		{
			Out.Logf(TEXT("\tDEFINE_ABSTRACT_DEFAULT_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
		}
		else
		{
			Out.Logf(TEXT("\tDEFINE_DEFAULT_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
		}
	}
	else
	{
		Out.Logf(TEXT("\tDEFINE_FORBIDDEN_DEFAULT_CONSTRUCTOR_CALL(%s)\r\n"), ClassCPPName);
	}
}

/**
 * Generates enhanced constructor declaration.
 *
 * @param Out Output device to generate to.
 * @param Class Class to generate constructor for.
 * @param API API string for this constructor.
 */
void ExportEnhancedConstructorsMacro(FOutputDevice& Out, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* API, const TCHAR* ClassCPPName)
{
	ExportConstructorDefinition(Out, ClassDef, API, ClassCPPName);
	ExportVTableHelperCtorAndCaller(Out, ClassDef, API, ClassCPPName);
	ExportDefaultConstructorCallDefinition(Out, ClassDef, ClassCPPName);
	ExportDestructorDefinition(Out, ClassDef, API, ClassCPPName);
}

/**
 * Gets a package relative inclusion path of the given source file for build.
 *
 * @param SourceFile Given source file.
 *
 * @returns Inclusion path.
 */
FString GetBuildPath(const FUnrealSourceFile& SourceFile)
{
	FString Out = SourceFile.GetFilename();

	ConvertToBuildIncludePath(SourceFile.GetPackageDef().GetModule(), Out);

	return Out;
}

void FNativeClassHeaderGenerator::ExportConstructorsMacros(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& Out, FOutputDevice& StandardUObjectConstructorsMacroCall, FOutputDevice& EnhancedUObjectConstructorsMacroCall, const FString& ConstructorsMacroPrefix, FUnrealClassDefinitionInfo& ClassDef, const TCHAR* APIArg)
{
	const FString ClassCPPName = ClassDef.GetAlternateNameCPP();

	FUHTStringBuilder StdMacro;
	FUHTStringBuilder EnhMacro;
	FString StdMacroName = ConstructorsMacroPrefix + TEXT("_STANDARD_CONSTRUCTORS");
	FString EnhMacroName = ConstructorsMacroPrefix + TEXT("_ENHANCED_CONSTRUCTORS");

	ExportStandardConstructorsMacro(StdMacro, ClassDef, APIArg, *ClassCPPName);
	ExportEnhancedConstructorsMacro(EnhMacro, ClassDef, APIArg, *ClassCPPName);

	if (!ClassDef.IsCustomVTableHelperConstructorDeclared())
	{
		Out.Logf(TEXT("\tDEFINE_VTABLE_PTR_HELPER_CTOR(%s);" LINE_TERMINATOR_ANSI), *ClassCPPName);
	}

	if (!ClassDef.IsDestructorDeclared())
	{
		Out.Logf(TEXT("\t%s::~%s() {}" LINE_TERMINATOR_ANSI), *ClassCPPName, *ClassCPPName);
	}

	OutGeneratedHeaderText.Log(Macroize(*StdMacroName, *StdMacro));
	OutGeneratedHeaderText.Log(Macroize(*EnhMacroName, *EnhMacro));

	StandardUObjectConstructorsMacroCall.Logf(TEXT("\t%s\r\n"), *StdMacroName);
	EnhancedUObjectConstructorsMacroCall.Logf(TEXT("\t%s\r\n"), *EnhMacroName);
}

bool FNativeClassHeaderGenerator::WriteHeader(FGeneratedCPP& FileInfo, const FString& InBodyText, const TSet<FString>& InAdditionalHeaders, const TSet<FString>& ForwardDeclarations)
{
	FUHTStringBuilder GeneratedHeaderTextWithCopyright;
	GeneratedHeaderTextWithCopyright.Log(HeaderCopyright);
	GeneratedHeaderTextWithCopyright.Logf(TEXT("// IWYU pragma: private, include \"%s\"\r\n"), *FileInfo.SourceFile.GetIncludePath());
	GeneratedHeaderTextWithCopyright.Log(TEXT("#include \"UObject/ObjectMacros.h\"\r\n"));
	GeneratedHeaderTextWithCopyright.Log(TEXT("#include \"UObject/ScriptMacros.h\"\r\n"));

	for (const FString& AdditionalHeader : InAdditionalHeaders)
	{
		GeneratedHeaderTextWithCopyright.Logf(TEXT("#include \"%s\"\r\n"), *AdditionalHeader);
	}

	GeneratedHeaderTextWithCopyright.Log(LINE_TERMINATOR);
	GeneratedHeaderTextWithCopyright.Log(DisableDeprecationWarnings);

	if (ForwardDeclarations.Num() > 0)
	{
		TArray<const FString*> Sorted;
		Sorted.Reserve(ForwardDeclarations.Num());
		for (const FString& Ref : ForwardDeclarations)
		{
			if (Ref.Len() > 0)
			{
				Sorted.Add(&Ref);
			}
		}
		Sorted.Sort();
		for (const FString* Ref : Sorted)
		{
			GeneratedHeaderTextWithCopyright.Logf(TEXT("%s\r\n"), **Ref);
		}
	}

	GeneratedHeaderTextWithCopyright.Log(InBodyText);
	GeneratedHeaderTextWithCopyright.Log(EnableDeprecationWarnings);

	const bool bHasChanged = SaveHeaderIfChanged(FileInfo.Header, MoveTemp(GeneratedHeaderTextWithCopyright));
	return bHasChanged;
}

/**
 * Returns a string in the format CLASS_Something|CLASS_Something which represents all class flags that are set for the specified
 * class which need to be exported as part of the DECLARE_CLASS macro
 */
FString FNativeClassHeaderGenerator::GetClassFlagExportText(FUnrealClassDefinitionInfo& ClassDef)
{
	FString StaticClassFlagText;

	if (ClassDef.HasAnyClassFlags(CLASS_Transient) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Transient");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_Optional))
	{
		StaticClassFlagText += TEXT(" | CLASS_Optional");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_DefaultConfig) )
	{
		StaticClassFlagText += TEXT(" | CLASS_DefaultConfig");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_GlobalUserConfig) )
	{
		StaticClassFlagText += TEXT(" | CLASS_GlobalUserConfig");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_ProjectUserConfig))
	{
		StaticClassFlagText += TEXT(" | CLASS_ProjectUserConfig");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_Config) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Config");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_Interface) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Interface");
	}
	if (ClassDef.HasAnyClassFlags(CLASS_Deprecated) )
	{
		StaticClassFlagText += TEXT(" | CLASS_Deprecated");
	}

	return StaticClassFlagText;
}

/**
* Exports the header text for the list of enums specified
*
* @param	Enums	the enums to export
*/
void FNativeClassHeaderGenerator::ExportEnum(FOutputDevice& Out, FUnrealEnumDefinitionInfo& EnumDef) const
{
	// Export FOREACH macro
	Out.Logf( TEXT("#define FOREACH_ENUM_%s(op) "), *EnumDef.GetName().ToUpper() );
	bool bHasExistingMax = EnumDef.ContainsExistingMax();
	int64 MaxEnumVal = bHasExistingMax ? EnumDef.GetMaxEnumValue() : 0;
	for (int32 i = 0; i < EnumDef.NumEnums(); i++)
	{
		if (bHasExistingMax && EnumDef.GetValueByIndex(i) == MaxEnumVal)
		{
			continue;
		}

		const FString QualifiedEnumValue = EnumDef.GetNameByIndex(i).ToString();
		Out.Logf( TEXT("\\\r\n\top(%s) "), *QualifiedEnumValue );
	}
	Out.Logf( TEXT("\r\n") );

	if (EnumDef.GetCppForm() == UEnum::ECppForm::EnumClass)
	{
		FString UnderlyingTypeString;

		if (EnumDef.GetUnderlyingType() != EUnderlyingEnumType::Unspecified)
		{
			UnderlyingTypeString = TEXT(" : ");

			switch (EnumDef.GetUnderlyingType())
			{
			case EUnderlyingEnumType::int8:        UnderlyingTypeString += TNameOf<int8>::GetName();	break;
			case EUnderlyingEnumType::int16:       UnderlyingTypeString += TNameOf<int16>::GetName();	break;
			case EUnderlyingEnumType::int32:       UnderlyingTypeString += TNameOf<int32>::GetName();	break;
			case EUnderlyingEnumType::int64:       UnderlyingTypeString += TNameOf<int64>::GetName();	break;
			case EUnderlyingEnumType::uint8:       UnderlyingTypeString += TNameOf<uint8>::GetName();	break;
			case EUnderlyingEnumType::uint16:      UnderlyingTypeString += TNameOf<uint16>::GetName();	break;
			case EUnderlyingEnumType::uint32:      UnderlyingTypeString += TNameOf<uint32>::GetName();	break;
			case EUnderlyingEnumType::uint64:      UnderlyingTypeString += TNameOf<uint64>::GetName();	break;
			default:
				check(false);
			}
		}

		Out.Logf( TEXT("\r\n") );
		Out.Logf( TEXT("enum class %s%s;\r\n"), *EnumDef.GetCppType(), *UnderlyingTypeString );
		
		// Add TIsUEnumClass typetraits
		Out.Logf( TEXT("template<> struct TIsUEnumClass<%s> { enum { Value = true }; };\r\n"), *EnumDef.GetCppType());

		// Forward declare the StaticEnum<> specialisation for enum classes
		Out.Logf( TEXT("template<> %sUEnum* StaticEnum<%s>();\r\n"), *GetAPIString(), *EnumDef.GetCppType());
		Out.Logf( TEXT("\r\n") );
	}
}

// Exports the header text for the list of structs specified (GENERATED_BODY impls)
void FNativeClassHeaderGenerator::ExportGeneratedStructBodyMacros(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealScriptStructDefinitionInfo& ScriptStructDef, EExportClassOutFlags& OutFlags) const
{
	ScriptStructDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, true);

	const FString ActualStructName = ScriptStructDef.GetName();
	const FString& FriendApiString  = GetAPIString();

	FUnrealStructDefinitionInfo* BaseStructDef = ScriptStructDef.GetSuperStruct();

	const FString StructNameCPP = ScriptStructDef.GetAlternateNameCPP();

	const FString& SingletonName = ScriptStructDef.GetSingletonName(true);
	const FString& ChoppedSingletonName = ScriptStructDef.GetSingletonNameChopped(true);

	const FString RigVMExecuteContextDeclaration = TEXT("FRigVMExtendedExecuteContext& RigVMExecuteContext");
	const FString RigVMExecuteContextPublicDeclaration = TEXT("const FRigVMExecuteContext& RigVMExecuteContext");
	TArray<FString> RigVMVirtualFuncProlog, RigVMVirtualFuncEpilog, RigVMStubProlog;

	// for RigVM methods we need to generated a macro used for implementing the static method
	// and prepare two prologs: one for the virtual function implementation, and one for the stub
	// invoking the static method.
	const FRigVMStructInfo& StructRigVMInfo = ScriptStructDef.GetRigVMInfo();
	if(StructRigVMInfo.bHasRigVM)
	{
		//RigVMStubProlog.Add(FString::Printf(TEXT("ensure(RigVMOperandMemory.Num() == %d);"), StructRigVMInfo->Members.Num()));

		int32 OperandIndex = 0;

		for (int32 ParameterIndex = 0; ParameterIndex < StructRigVMInfo.Members.Num(); ParameterIndex++)
		{
			const FRigVMParameter& Parameter = StructRigVMInfo.Members[ParameterIndex];

			if(Parameter.RequiresCast())
			{
				RigVMVirtualFuncProlog.Add(FString::Printf(TEXT("%s %s(%s);"), *Parameter.CastType, *Parameter.CastName, *Parameter.Name));
			}

			const FString& ParamTypeOriginal = Parameter.TypeOriginal(true);
			const FString& ParamNameOriginal = Parameter.NameOriginal(false);

			FString AdditionalParameters;
			if(!Parameter.bInput && !Parameter.bOutput && !Parameter.bSingleton)
			{
				static const FString SliceContextParameter = TEXT(", RigVMExecuteContext.GetSlice().GetIndex()");
				AdditionalParameters = SliceContextParameter;
			}

			if (Parameter.IsArray())
			{
				FString ExtendedType = Parameter.ExtendedType();
				RigVMStubProlog.Add(FString::Printf(TEXT("TArray%s& %s = *(TArray%s*)RigVMMemoryHandles[%d].GetData(false%s);"),
					*ExtendedType,
					*ParamNameOriginal,
					*ExtendedType,
					OperandIndex,
					*AdditionalParameters));

				OperandIndex++;
			}
			else
			{
				FString VariableType = Parameter.TypeVariableRef(true);
				FString ExtractedType = Parameter.TypeOriginal();
				FString ParameterCast = FString::Printf(TEXT("*(%s*)"), *ExtractedType);

				RigVMStubProlog.Add(FString::Printf(TEXT("%s %s = %sRigVMMemoryHandles[%d].GetData(false%s);"),
				*VariableType,
				*ParamNameOriginal,
				*ParameterCast,
				OperandIndex,
				*AdditionalParameters));

				OperandIndex++;
			}
		}

		FString StructMembers = StructRigVMInfo.Members.Declarations(false, TEXT(", \\\r\n\t\t"), true, false);

		OutGeneratedHeaderText.Log(TEXT("\r\n"));
		for (const FRigVMMethodInfo& MethodInfo : StructRigVMInfo.Methods)
		{
			FString ParameterSuffix = MethodInfo.Parameters.Declarations(true, TEXT(", \\\r\n\t\t"));
			FString RigVMParameterPrefix2 = RigVMExecuteContextPublicDeclaration + FString((StructMembers.IsEmpty() && ParameterSuffix.IsEmpty()) ? TEXT("") : TEXT(", \\\r\n\t\t"));
			OutGeneratedHeaderText.Logf(TEXT("#define %s_%s() \\\r\n"), *StructNameCPP, *MethodInfo.Name);
			OutGeneratedHeaderText.Logf(TEXT("\t%s %s::Static%s( \\\r\n\t\t%s%s%s \\\r\n\t)\r\n"), *MethodInfo.ReturnType, *StructNameCPP, *MethodInfo.Name, *RigVMParameterPrefix2, *StructMembers, *ParameterSuffix);
		}
		OutGeneratedHeaderText.Log(TEXT("\r\n"));
	}

	// Export struct.
	if (ScriptStructDef.HasAnyStructFlags(STRUCT_Native))
	{
		check(ScriptStructDef.GetMacroDeclaredLineNumber() != INDEX_NONE);

		const bool bRequiredAPI = !ScriptStructDef.HasAnyStructFlags(STRUCT_RequiredAPI);

		const FString FriendLine = FString::Printf(TEXT("\tfriend struct %s_Statics;\r\n"), *ChoppedSingletonName);
		const FString StaticClassLine = FString::Printf(TEXT("\t%sstatic class UScriptStruct* StaticStruct();\r\n"), (bRequiredAPI ? *FriendApiString : TEXT("")));
		
		// if we have RigVM methods on this struct we need to 
		// declare the static method as well as the stub method
		FString RigVMMethodsDeclarations;
		if (StructRigVMInfo.bHasRigVM)
		{
			FString StructMembers = StructRigVMInfo.Members.Declarations(false, TEXT(",\r\n\t\t"), true, false);
			for (const FRigVMMethodInfo& MethodInfo : StructRigVMInfo.Methods)
			{
				FString StructMembersForStub = StructRigVMInfo.Members.Names(false, TEXT(",\r\n\t\t\t"), false);
				FString ParameterSuffix = MethodInfo.Parameters.Declarations(true, TEXT(",\r\n\t\t"));
				FString ParameterNamesSuffix = MethodInfo.Parameters.Names(true, TEXT(",\r\n\t\t\t"));
				FString RigVMParameterPrefix2 = RigVMExecuteContextPublicDeclaration + FString((StructMembers.IsEmpty() && ParameterSuffix.IsEmpty()) ? TEXT("") : TEXT(",\r\n\t\t"));
				FString RigVMParameterPrefix4 = FString(TEXT("RigVMExecuteContext.PublicData")) + FString((StructMembersForStub.IsEmpty() && ParameterSuffix.IsEmpty()) ? TEXT("") : TEXT(",\r\n\t\t\t"));

				RigVMMethodsDeclarations += FString::Printf(TEXT("\tstatic %s Static%s(\r\n\t\t%s%s%s\r\n\t);\r\n"), *MethodInfo.ReturnType, *MethodInfo.Name, *RigVMParameterPrefix2, *StructMembers, *ParameterSuffix);
				RigVMMethodsDeclarations += FString::Printf(TEXT("\tFORCEINLINE_DEBUGGABLE static %s RigVM%s(\r\n\t\t%s,\r\n\t\tFRigVMMemoryHandleArray RigVMMemoryHandles\r\n\t)\r\n"), *MethodInfo.ReturnType, *MethodInfo.Name, *RigVMExecuteContextDeclaration);
				RigVMMethodsDeclarations += FString::Printf(TEXT("\t{\r\n"));

				// implement inline stub method body
				if (MethodInfo.Parameters.Num() > 0)
				{
					//RigVMMethodsDeclarations += FString::Printf(TEXT("\t\tensure(RigVMUserData.Num() == %d);\r\n"), MethodInfo.Parameters.Num());
					for (int32 ParameterIndex = 0; ParameterIndex < MethodInfo.Parameters.Num(); ParameterIndex++)
					{
						const FRigVMParameter& Parameter = MethodInfo.Parameters[ParameterIndex];
						RigVMMethodsDeclarations += FString::Printf(TEXT("\t\t%s = *(%s*)RigVMExecuteContext.OpaqueArguments[%d];\r\n"), *Parameter.Declaration(), *Parameter.TypeNoRef(), ParameterIndex);
					}
					RigVMMethodsDeclarations += FString::Printf(TEXT("\t\t\r\n"));
				}

				if (RigVMStubProlog.Num() > 0)
				{
					for (const FString& RigVMStubPrologLine : RigVMStubProlog)
					{
						RigVMMethodsDeclarations += FString::Printf(TEXT("\t\t%s\r\n"), *RigVMStubPrologLine);
					}
					RigVMMethodsDeclarations += FString::Printf(TEXT("\t\t\r\n"));
				}

				RigVMMethodsDeclarations += FString::Printf(TEXT("\t\t%sStatic%s(\r\n\t\t\t%s%s%s\r\n\t\t);\r\n"), *MethodInfo.ReturnPrefix(), *MethodInfo.Name, *RigVMParameterPrefix4, *StructMembersForStub, *ParameterNamesSuffix);
				RigVMMethodsDeclarations += FString::Printf(TEXT("\t}\r\n"));
			}
		}

		// Check to see if we are a FastArraySerializer and should try to deduce the FastArraySerializerItemType
		// To fulfill that requirement the struct should be derived from FFastArraySerializer and have a single replicated TArrayProperty
		bool bGenerateFastArraySerializerTypeDefinition = false;
		FString FastArraySerializerTypeDefinition;
		{
			const FUnrealStructDefinitionInfo* FastArraySerializerStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealStructDefinitionInfo>(TEXT("FastArraySerializer"));
			if (FastArraySerializerStructDef && BaseStructDef && BaseStructDef->IsChildOf(*FastArraySerializerStructDef))
			{
				uint32 ReplicatedPropertyCount = 0U;
				const FUnrealPropertyDefinitionInfo* PotentialItemArrayPropertyDefinition = nullptr;

				// Only output bindings for FastArraySerializers that have a single replicated dynamic array
				for (const TSharedRef<FUnrealPropertyDefinitionInfo>& Property : ScriptStructDef.GetProperties())
				{
					if (!EnumHasAnyFlags(Property->GetPropertyFlags(), EPropertyFlags::CPF_RepSkip) && Property->IsDynamicArray())
					{
						PotentialItemArrayPropertyDefinition = &Property.Get();
						++ReplicatedPropertyCount;
					}
				}

				if (ReplicatedPropertyCount == 1)
				{
					FastArraySerializerTypeDefinition = FString::Printf(TEXT("\tUE_NET_DECLARE_FASTARRAY(%s, %s, %s);\r\n"), *StructNameCPP, *(PotentialItemArrayPropertyDefinition->GetName()), (bRequiredAPI ? *GetAPIString() : TEXT("")));
					bGenerateFastArraySerializerTypeDefinition = true;
				}
			}
		}

		const FString SuperTypedef = BaseStructDef ? FString::Printf(TEXT("\ttypedef %s Super;\r\n"), *BaseStructDef->GetAlternateNameCPP()) : FString();

		FString CombinedLine = FString::Printf(TEXT("%s%s%s%s%s"), *FriendLine, *StaticClassLine, *RigVMMethodsDeclarations, *SuperTypedef, *FastArraySerializerTypeDefinition);
		const FString MacroName = SourceFile.GetGeneratedBodyMacroName(ScriptStructDef.GetMacroDeclaredLineNumber());

		const FString Macroized = Macroize(*MacroName, MoveTemp(CombinedLine));
		OutGeneratedHeaderText.Log(Macroized);

		// Inject static assert to verify that we do not add vtable
		if (BaseStructDef)
		{
			FString BaseStructNameCPP = BaseStructDef->GetAlternateNameCPP();

			FString VerifyPolymorphicStructString = FString::Printf(TEXT("\r\nstatic_assert(std::is_polymorphic<%s>() == std::is_polymorphic<%s>(), \"USTRUCT %s cannot be polymorphic unless super %s is polymorphic\");\r\n\r\n"), *StructNameCPP, *BaseStructNameCPP, *StructNameCPP, *BaseStructNameCPP);			
			Out.Log(VerifyPolymorphicStructString);
		}

		FString GetHashName = FString::Printf(TEXT("Get_%s_Hash"), *ChoppedSingletonName);

			Out.Logf(TEXT("\tstatic FStructRegistrationInfo Z_Registration_Info_UScriptStruct_%s;\r\n"), *ScriptStructDef.GetName());
		Out.Logf(TEXT("class UScriptStruct* %s::StaticStruct()\r\n"), *StructNameCPP);
		Out.Logf(TEXT("{\r\n"));

		// UStructs can have UClass or UPackage outer (if declared in non-UClass headers).
		const FString& OuterName (GetPackageSingletonName(UHTCastChecked<FUnrealPackageDefinitionInfo>(ScriptStructDef.GetOuter()), OutReferenceGatherers.UniqueCrossModuleReferences));
		FString OuterSingletonName = FString::Printf(TEXT("Z_Registration_Info_UScriptStruct_%s.OuterSingleton"), *ScriptStructDef.GetName());

		Out.Logf(TEXT("\tif (!%s)\r\n"), *OuterSingletonName);
		Out.Logf(TEXT("\t{\r\n"));

		Out.Logf(TEXT("\t\t%s = GetStaticStruct(%s, %s, TEXT(\"%s\"));\r\n"),
			*OuterSingletonName, *ChoppedSingletonName, *OuterName, *ActualStructName);

		// if this struct has RigVM methods - we need to register the method to our central
		// registry on construction of the static struct
		if (StructRigVMInfo.bHasRigVM)
		{
			for (const FRigVMMethodInfo& MethodInfo : StructRigVMInfo.Methods)
			{
				const FString ArgumentsName = FString::Printf(TEXT("Arguments_%s_%s"), *StructNameCPP, *MethodInfo.Name);
				Out.Logf(TEXT("\t\tTArray<FRigVMFunctionArgument> %s;\r\n"), *ArgumentsName);
				for (int32 MemberIndex = 0; MemberIndex < StructRigVMInfo.Members.Num(); MemberIndex++)
				{
					const FRigVMParameter& Parameter = StructRigVMInfo.Members[MemberIndex];
					Out.Logf(TEXT("\t\t%s.Emplace(TEXT(\"%s\"), TEXT(\"%s\"));\r\n"), *ArgumentsName, *Parameter.NameOriginal(), *Parameter.TypeOriginal());
				}
				for (int32 ParameterIndex = 0; ParameterIndex < MethodInfo.Parameters.Num(); ParameterIndex++)
				{
					const FRigVMParameter& Parameter = MethodInfo.Parameters[ParameterIndex];
					Out.Logf(TEXT("\t\t%s.Emplace(TEXT(\"%s\"), TEXT(\"%s\"));\r\n"), *ArgumentsName, *Parameter.NameOriginal(), *Parameter.TypeOriginal());
				}
				Out.Logf(TEXT("\t\tFRigVMRegistry::Get().Register(TEXT(\"%s::%s\"), &%s::RigVM%s, %s, %s);\r\n"),
					*StructNameCPP, *MethodInfo.Name, *StructNameCPP, *MethodInfo.Name, *OuterSingletonName, *ArgumentsName);
			}
		}

		Out.Logf(TEXT("\t}\r\n"));
		Out.Logf(TEXT("\treturn %s;\r\n"), *OuterSingletonName);
		Out.Logf(TEXT("}\r\n"));

		// Forward declare the StaticStruct specialization in the header
		OutGeneratedHeaderText.Logf(TEXT("template<> %sUScriptStruct* StaticStruct<struct %s>();\r\n\r\n"), *GetAPIString(), *StructNameCPP);

		// Generate the StaticStruct specialization
		Out.Logf(TEXT("template<> %sUScriptStruct* StaticStruct<%s>()\r\n"), *GetAPIString(), *StructNameCPP);
		Out.Logf(TEXT("{\r\n"));
		Out.Logf(TEXT("\treturn %s::StaticStruct();\r\n"), *StructNameCPP);
		Out.Logf(TEXT("}\r\n"));

		// Inject implementation needed to support auto bindings of fast arrays
		if (bGenerateFastArraySerializerTypeDefinition)
		{
			OutFlags |= EExportClassOutFlags::NeedsFastArrayHeaders;
			Out.Logf(TEXT("UE_NET_IMPLEMENT_FASTARRAY(%s);\r\n"), *StructNameCPP);
		}
	}

	FString StaticsStructName = ChoppedSingletonName + TEXT("_Statics");

	FUHTStringBuilder GeneratedStructRegisterFunctionText;
	FUHTStringBuilder StaticDefinitions;

	GeneratedStructRegisterFunctionText.Logf(TEXT("\tstruct %s\r\n"), *StaticsStructName);
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t{\r\n"));

	// if this is a no export struct, we will put a local struct here for offset determination
	TArray<FUnrealScriptStructDefinitionInfo*> NoExportStructs = FindNoExportStructs(&ScriptStructDef);
	for (FUnrealScriptStructDefinitionInfo* NoExportStructDef : NoExportStructs)
	{
		ExportMirrorsForNoexportStruct(GeneratedStructRegisterFunctionText, *NoExportStructDef, /*Indent=*/ 2);
	}

	if (BaseStructDef)
	{
		UHTCastChecked<FUnrealScriptStructDefinitionInfo>(BaseStructDef); // this better actually be a script struct
		BaseStructDef->AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, true);
	}

	EStructFlags UncomputedFlags = (EStructFlags)(ScriptStructDef.GetStructFlags() & ~STRUCT_ComputedFlags);

	FString OuterFunc = GetPackageSingletonNameFuncAddr(UHTCastChecked<FUnrealPackageDefinitionInfo>(ScriptStructDef.GetOuter()), OutReferenceGatherers.UniqueCrossModuleReferences);
	FString MetaDataParams = OutputMetaDataCodeForObject(GeneratedStructRegisterFunctionText, StaticDefinitions, ScriptStructDef, *FString::Printf(TEXT("%s::Struct_MetaDataParams"), *StaticsStructName), TEXT("\t\t"), TEXT("\t"));

	FString NewStructOps;
	if (ScriptStructDef.HasAnyStructFlags(STRUCT_Native))
	{
		GeneratedStructRegisterFunctionText.Log(TEXT("\t\tstatic void* NewStructOps();\r\n"));

		StaticDefinitions.Logf(TEXT("\tvoid* %s::NewStructOps()\r\n"), *StaticsStructName);
		StaticDefinitions.Log (TEXT("\t{\r\n"));
		StaticDefinitions.Logf(TEXT("\t\treturn (UScriptStruct::ICppStructOps*)new UScriptStruct::TCppStructOps<%s>();\r\n"), *StructNameCPP);
		StaticDefinitions.Log (TEXT("\t}\r\n"));

		NewStructOps = TEXT("&NewStructOps");
	}
	else
	{
		NewStructOps = TEXT("nullptr");
	}

	TTuple<FString, FString> PropertyRange = OutputProperties(GeneratedStructRegisterFunctionText, StaticDefinitions, OutReferenceGatherers, *FString::Printf(TEXT("%s::"), *StaticsStructName), ScriptStructDef, TEXT("\t\t"), TEXT("\t"));

	GeneratedStructRegisterFunctionText.Log (TEXT("\t\tstatic const UECodeGen_Private::FStructParams ReturnStructParams;\r\n"));

	StaticDefinitions.Logf(TEXT("\tconst UECodeGen_Private::FStructParams %s::ReturnStructParams = {\r\n"), *StaticsStructName);
	StaticDefinitions.Logf(TEXT("\t\t(UObject* (*)())%s,\r\n"), *OuterFunc);
	StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *GetSingletonNameFuncAddr(BaseStructDef, OutReferenceGatherers.UniqueCrossModuleReferences));
	StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *NewStructOps);
	StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *CreateUTF8LiteralString(ActualStructName));
	StaticDefinitions.Logf(TEXT("\t\tsizeof(%s),\r\n"), *StructNameCPP);
	StaticDefinitions.Logf(TEXT("\t\talignof(%s),\r\n"), *StructNameCPP);
	StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *PropertyRange.Get<0>());
	StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *PropertyRange.Get<1>());
	StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), TEXT("RF_Public|RF_Transient|RF_MarkAsNative"));
	StaticDefinitions.Logf(TEXT("\t\tEStructFlags(0x%08X),\r\n"), (uint32)UncomputedFlags);
	StaticDefinitions.Logf(TEXT("\t\t%s\r\n"), *MetaDataParams);
	StaticDefinitions.Log (TEXT("\t};\r\n"));

	GeneratedStructRegisterFunctionText.Log (TEXT("\t};\r\n"));

	GeneratedStructRegisterFunctionText.Log(StaticDefinitions);

	GeneratedStructRegisterFunctionText.Logf(TEXT("\tUScriptStruct* %s\r\n"), *SingletonName);
	GeneratedStructRegisterFunctionText.Log (TEXT("\t{\r\n"));

	FString NoExportStructNameCPP;
	if (NoExportStructs.Contains(&ScriptStructDef))
	{
		NoExportStructNameCPP = FString::Printf(TEXT("%s::%s"), *StaticsStructName, *StructNameCPP);
	}
	else
	{
		NoExportStructNameCPP = StructNameCPP;
	}

	FString HashFuncName = FString::Printf(TEXT("Get_%s_Hash"), *SingletonName.Replace(TEXT("()"), TEXT(""), ESearchCase::CaseSensitive));
	// Structs can either have a UClass or UPackage as outer (if declared in non-UClass header).
	FString InnerSingletonName;
		if (ScriptStructDef.HasAnyStructFlags(STRUCT_Native))
		{
			InnerSingletonName = FString::Printf(TEXT("Z_Registration_Info_UScriptStruct_%s.InnerSingleton"), *ScriptStructDef.GetName());
		}
		else
		{
			GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tstatic UScriptStruct* ReturnStruct = nullptr;\r\n"));
			InnerSingletonName = TEXT("ReturnStruct");
		}
	
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\tif (!%s)\r\n"), *InnerSingletonName);
	GeneratedStructRegisterFunctionText.Log (TEXT("\t\t{\r\n"));

	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\t\tUECodeGen_Private::ConstructUScriptStruct(%s, %s::ReturnStructParams);\r\n"), *InnerSingletonName, *StaticsStructName);
	GeneratedStructRegisterFunctionText.Log (TEXT("\t\t}\r\n"));
	GeneratedStructRegisterFunctionText.Logf(TEXT("\t\treturn %s;\r\n"), *InnerSingletonName);
	GeneratedStructRegisterFunctionText.Log (TEXT("\t}\r\n"));

	uint32 StructHash = GenerateTextHash(*GeneratedStructRegisterFunctionText);
	ScriptStructDef.SetHash(StructHash);

	Out.Log(GeneratedStructRegisterFunctionText);

	// if this struct has RigVM methods we need to implement both the 
	// virtual function as well as the stub method here.
	// The static method is implemented by the user using a macro.
	if (StructRigVMInfo.bHasRigVM)
	{
		FString StructMembersForVirtualFunc = StructRigVMInfo.Members.Names(false, TEXT(",\r\n\t\t"), true);

		for (const FRigVMMethodInfo& MethodInfo : StructRigVMInfo.Methods)
		{
			Out.Log(TEXT("\r\n"));

			FString ParameterDeclaration = MethodInfo.Parameters.Declarations(false, TEXT(",\r\n\t\t"));
			FString ParameterSuffix = MethodInfo.Parameters.Names(true, TEXT(",\r\n\t\t"));
			FString RigVMParameterPrefix3 = FString(TEXT("RigVMExecuteContext")) + FString((StructMembersForVirtualFunc.IsEmpty() && ParameterSuffix.IsEmpty()) ? TEXT("") : TEXT(",\r\n\t\t"));

			// implement the virtual function body.
			Out.Logf(TEXT("%s %s::%s(%s)\r\n"), *MethodInfo.ReturnType, *StructNameCPP, *MethodInfo.Name, *ParameterDeclaration);
			Out.Log(TEXT("{\r\n"));
			Out.Log(TEXT("\tFRigVMExecuteContext RigVMExecuteContext;\r\n"));

			if(RigVMVirtualFuncProlog.Num() > 0)
			{
				for (const FString& RigVMVirtualFuncPrologLine : RigVMVirtualFuncProlog)
				{
					Out.Logf(TEXT("\t%s\r\n"), *RigVMVirtualFuncPrologLine);
				}
				Out.Log(TEXT("\t\r\n"));
			}

			Out.Logf(TEXT("\t%sStatic%s(\r\n\t\t%s%s%s\r\n\t);\r\n"), *MethodInfo.ReturnPrefix(), *MethodInfo.Name, *RigVMParameterPrefix3, *StructMembersForVirtualFunc, *ParameterSuffix);

			if (RigVMVirtualFuncEpilog.Num() > 0)
			{
				for (const FString& RigVMVirtualFuncEpilogLine : RigVMVirtualFuncEpilog)
				{
					Out.Logf(TEXT("\t%s\r\n"), *RigVMVirtualFuncEpilogLine);
				}
				Out.Log(TEXT("\t\r\n"));
			}

			Out.Log(TEXT("}\r\n"));
		}

			Out.Log(TEXT("\r\n"));
	}
}

void FNativeClassHeaderGenerator::ExportGeneratedEnumInitCode(FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealEnumDefinitionInfo& EnumDef) const
{
	const FString& SingletonName        = EnumDef.GetSingletonNameChopped(true);
	const FString EnumNameCpp           = EnumDef.GetName(); //UserDefinedEnum should already have a valid cpp name.
	const FString StaticsStructName     = SingletonName + TEXT("_Statics");
	const bool bIsEditorOnlyDataType    = EnumDef.IsEditorOnly();

	EnumDef.AddCrossModuleReference(OutReferenceGatherers.UniqueCrossModuleReferences, true);

	FMacroBlockEmitter EditorOnlyData(Out, TEXT("WITH_EDITORONLY_DATA"));
	EditorOnlyData(bIsEditorOnlyDataType);

	const FString& PackageSingletonName = GetPackageSingletonName(UHTCastChecked<FUnrealPackageDefinitionInfo>(EnumDef.GetOuter()), OutReferenceGatherers.UniqueCrossModuleReferences);

	// If we don't have a zero 0 then we emit a static assert to verify we have one
	if (!EnumDef.IsValidEnumValue(0) && EnumDef.HasMetaData(FHeaderParserNames::NAME_BlueprintType))
	{
		bool bHasUnparsedValue = false;
		for (const TPair<FName, int64>& Enum : EnumDef.GetEnums())
		{
			if (Enum.Value == -1)
			{
				bHasUnparsedValue = true;
				break;
			}
		}

		if (bHasUnparsedValue)
		{
			Out.Logf(TEXT("\tstatic_assert("));
			bool bDoneFirst = false;
			for (const TPair<FName, int64>& Enum : EnumDef.GetEnums())
			{
				if (Enum.Value == -1)
				{
					if (bDoneFirst)
					{
						Out.Logf(TEXT("||"));
					}
					bDoneFirst = true;
					Out.Logf(TEXT("!int64(%s)"), *Enum.Key.ToString());
				}
			}
			Out.Logf(TEXT(", \"'%s' does not have a 0 entry!(This is a problem when the enum is initalized by default)\");\r\n"), *EnumDef.GetName());
		}
	}

		Out.Logf(TEXT("\tstatic FEnumRegistrationInfo Z_Registration_Info_UEnum_%s;\r\n"), *EnumNameCpp);
	Out.Logf(TEXT("\tstatic UEnum* %s_StaticEnum()\r\n"), *EnumNameCpp);
	Out.Logf(TEXT("\t{\r\n"));

		Out.Logf(TEXT("\t\tif (!Z_Registration_Info_UEnum_%s.OuterSingleton)\r\n"), *EnumNameCpp);
		Out.Logf(TEXT("\t\t{\r\n"));
		Out.Logf(TEXT("\t\t\tZ_Registration_Info_UEnum_%s.OuterSingleton = GetStaticEnum(%s, %s, TEXT(\"%s\"));\r\n"), *EnumNameCpp, *SingletonName, *PackageSingletonName, *EnumNameCpp);
		Out.Logf(TEXT("\t\t}\r\n"));
		Out.Logf(TEXT("\t\treturn Z_Registration_Info_UEnum_%s.OuterSingleton;\r\n"), *EnumNameCpp);

	Out.Logf(TEXT("\t}\r\n"));

	const FString& EnumSingletonName = EnumDef.GetSingletonName(true);
	const FString HashFuncName = FString::Printf(TEXT("Get_%s_Hash"), *SingletonName);

	Out.Logf(TEXT("\ttemplate<> %sUEnum* StaticEnum<%s>()\r\n"), *GetAPIString(), *EnumDef.GetCppType());
	Out.Logf(TEXT("\t{\r\n"));
	Out.Logf(TEXT("\t\treturn %s_StaticEnum();\r\n"), *EnumNameCpp);
	Out.Logf(TEXT("\t}\r\n"));

	FUHTStringBuilder StaticDefinitions;
	FUHTStringBuilder StaticDeclarations;

	// Generate the static declarations and definitions
	{

		// Enums can either have a UClass or UPackage as outer (if declared in non-UClass header).
		FString OuterString = PackageSingletonName;
		const TCHAR* UEnumObjectFlags = TEXT("RF_Public|RF_Transient|RF_MarkAsNative");
		const TCHAR* EnumFlags = EnumDef.HasAnyEnumFlags(EEnumFlags::Flags) ? TEXT("EEnumFlags::Flags") : TEXT("EEnumFlags::None");

		const TCHAR* EnumFormStr = TEXT("");
		switch (EnumDef.GetCppForm())
		{
		case UEnum::ECppForm::Regular:    EnumFormStr = TEXT("UEnum::ECppForm::Regular");    break;
		case UEnum::ECppForm::Namespaced: EnumFormStr = TEXT("UEnum::ECppForm::Namespaced"); break;
		case UEnum::ECppForm::EnumClass:  EnumFormStr = TEXT("UEnum::ECppForm::EnumClass");  break;
		}

		const FString& EnumDisplayNameFn = EnumDef.GetMetaData(TEXT("EnumDisplayNameFn"));

		StaticDeclarations.Logf(TEXT("\tstruct %s\r\n"), *StaticsStructName);
		StaticDeclarations.Logf(TEXT("\t{\r\n"));

		StaticDeclarations.Logf(TEXT("\t\tstatic const UECodeGen_Private::FEnumeratorParam Enumerators[];\r\n"));
		StaticDefinitions.Logf(TEXT("\tconst UECodeGen_Private::FEnumeratorParam %s::Enumerators[] = {\r\n"), *StaticsStructName);
		for (int32 Index = 0; Index != EnumDef.NumEnums(); ++Index)
		{
			const TCHAR* OverridenNameMetaDatakey = TEXT("OverrideName");
			const FString KeyName = EnumDef.HasMetaData(OverridenNameMetaDatakey, Index) ? EnumDef.GetMetaData(OverridenNameMetaDatakey, Index) : EnumDef.GetNameByIndex(Index).ToString();
			StaticDefinitions.Logf(TEXT("\t\t{ %s, (int64)%s },\r\n"), *CreateUTF8LiteralString(KeyName), *EnumDef.GetNameByIndex(Index).ToString());
		}
		StaticDefinitions.Logf(TEXT("\t};\r\n"));

		FString MetaDataParamsName = StaticsStructName + TEXT("::Enum_MetaDataParams");
		FString MetaDataParams = OutputMetaDataCodeForObject(StaticDeclarations, StaticDefinitions, EnumDef, *MetaDataParamsName, TEXT("\t\t"), TEXT("\t"));

		StaticDeclarations.Logf(TEXT("\t\tstatic const UECodeGen_Private::FEnumParams EnumParams;\r\n"));
		StaticDefinitions.Logf(TEXT("\tconst UECodeGen_Private::FEnumParams %s::EnumParams = {\r\n"), *StaticsStructName);
		StaticDefinitions.Logf(TEXT("\t\t(UObject*(*)())%s,\r\n"), *OuterString.LeftChop(2));
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), EnumDisplayNameFn.IsEmpty() ? TEXT("nullptr") : *EnumDisplayNameFn);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *CreateUTF8LiteralString(EnumNameCpp));
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), *CreateUTF8LiteralString(EnumDef.GetCppType()));
		StaticDefinitions.Logf(TEXT("\t\t%s::Enumerators,\r\n"), *StaticsStructName);
		StaticDefinitions.Logf(TEXT("\t\tUE_ARRAY_COUNT(%s::Enumerators),\r\n"), *StaticsStructName);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), UEnumObjectFlags);
		StaticDefinitions.Logf(TEXT("\t\t%s,\r\n"), EnumFlags);
		StaticDefinitions.Logf(TEXT("\t\t(uint8)%s,\r\n"), EnumFormStr);
		StaticDefinitions.Logf(TEXT("\t\t%s\r\n"), *MetaDataParams);
		StaticDefinitions.Logf(TEXT("\t};\r\n"));

		StaticDeclarations.Logf(TEXT("\t};\r\n"));
	}

	//////////////////////////////////////

	FUHTStringBuilder GeneratedEnumRegisterFunctionText;

	GeneratedEnumRegisterFunctionText.Log(*StaticDeclarations);
	GeneratedEnumRegisterFunctionText.Log(*StaticDefinitions);

	GeneratedEnumRegisterFunctionText.Logf(TEXT("\tUEnum* %s\r\n"), *EnumSingletonName);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t{\r\n"));

	// Enums can either have a UClass or UPackage as outer (if declared in non-UClass header).
	FString InnerSingletonName = FString::Printf(TEXT("Z_Registration_Info_UEnum_%s.InnerSingleton"), *EnumNameCpp);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\tif (!%s)\r\n"), *InnerSingletonName);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t{\r\n"));

	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t\tUECodeGen_Private::ConstructUEnum(%s, %s::EnumParams);\r\n"), *InnerSingletonName, *StaticsStructName);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\t}\r\n"));
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t\treturn %s;\r\n"), *InnerSingletonName);
	GeneratedEnumRegisterFunctionText.Logf(TEXT("\t}\r\n"));

	uint32 EnumHash = GenerateTextHash(*GeneratedEnumRegisterFunctionText);
	EnumDef.SetHash(EnumHash);

	Out.Log(GeneratedEnumRegisterFunctionText);
}

void FNativeClassHeaderGenerator::ExportMirrorsForNoexportStruct(FOutputDevice& Out, FUnrealScriptStructDefinitionInfo& ScriptStructDef, int32 TextIndent)
{
	// Export struct.
	const FString StructName = ScriptStructDef.GetAlternateNameCPP();
	Out.Logf(TEXT("%sstruct %s"), FCString::Tab(TextIndent), *StructName);
	if (FUnrealStructDefinitionInfo* SuperStructDef = ScriptStructDef.GetSuperStruct())
	{
		Out.Logf(TEXT(" : public %s"), *SuperStructDef->GetAlternateNameCPP());
	}
	Out.Logf(TEXT("\r\n%s{\r\n"), FCString::Tab(TextIndent));

	// Export the struct's CPP properties.
	ExportProperties(Out, ScriptStructDef, TextIndent);

	Out.Logf(TEXT("%s};\r\n\r\n"), FCString::Tab(TextIndent));
}

bool FNativeClassHeaderGenerator::WillExportEventParms(FUnrealFunctionDefinitionInfo& FunctionDef)
{
	const TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = FunctionDef.GetProperties();
	return Properties.Num() > 0 && Properties[0]->HasAnyPropertyFlags(CPF_Parm);
}

void WriteEventFunctionPrologue(FOutputDevice& Output, int32 Indent, const FParmsAndReturnProperties& Parameters, FUnrealObjectDefinitionInfo& FunctionOuterDef, const TCHAR* FunctionName)
{
	// now the body - first we need to declare a struct which will hold the parameters for the event/delegate call
	Output.Logf(TEXT("\r\n%s{\r\n"), FCString::Tab(Indent));

	// declare and zero-initialize the parameters and return value, if applicable
	if (!Parameters.HasParms())
		return;

	FString EventStructName = GetEventStructParamsName(FunctionOuterDef, FunctionName);

	Output.Logf(TEXT("%s%s Parms;\r\n"), FCString::Tab(Indent + 1), *EventStructName );

	// Declare a parameter struct for this event/delegate and assign the struct members using the values passed into the event/delegate call.
	for (FUnrealPropertyDefinitionInfo* PropDef : Parameters.Parms)
	{
		const FString PropertyName = PropDef->GetNameWithDeprecated();
		if (PropDef->IsStaticArray())
		{
			Output.Logf(TEXT("%sFMemory::Memcpy(Parms.%s,%s,sizeof(Parms.%s));\r\n"), FCString::Tab(Indent + 1), *PropertyName, *PropertyName, *PropertyName);
		}
		else
		{
			FString ValueAssignmentText = PropertyName;
			if (PropDef->IsBooleanOrBooleanStaticArray())
			{
				ValueAssignmentText += TEXT(" ? true : false");
			}

			Output.Logf(TEXT("%sParms.%s=%s;\r\n"), FCString::Tab(Indent + 1), *PropertyName, *ValueAssignmentText);
		}
	}
}

void WriteEventFunctionEpilogue(FOutputDevice& Output, int32 Indent, const FParmsAndReturnProperties& Parameters)
{
	// Out parm copying.
	for (FUnrealPropertyDefinitionInfo* PropDef : Parameters.Parms)
	{
		if (PropDef->HasSpecificPropertyFlags(CPF_OutParm | CPF_ConstParm, CPF_OutParm))
		{
			const FString PropertyName = PropDef->GetNameWithDeprecated();
			if (PropDef->IsStaticArray())
			{
				Output.Logf(TEXT("%sFMemory::Memcpy(&%s,&Parms.%s,sizeof(%s));\r\n"), FCString::Tab(Indent + 1), *PropertyName, *PropertyName, *PropertyName);
			}
			else
			{
				Output.Logf(TEXT("%s%s=Parms.%s;\r\n"), FCString::Tab(Indent + 1), *PropertyName, *PropertyName);
			}
		}
	}

	// Return value.
	if (Parameters.Return)
	{
		// Make sure uint32 -> bool is supported
		bool bBoolProperty = Parameters.Return->IsBooleanOrBooleanStaticArray();
		Output.Logf(TEXT("%sreturn %sParms.%s;\r\n"), FCString::Tab(Indent + 1), bBoolProperty ? TEXT("!!") : TEXT(""), *Parameters.Return->GetName());
	}
	Output.Logf(TEXT("%s}\r\n"), FCString::Tab(Indent));
}

void FNativeClassHeaderGenerator::ExportDelegateDeclaration(FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealFunctionDefinitionInfo& FunctionDef) const
{
	static const auto& DelegateStr = TEXT("delegate");

	FFuncInfo FunctionData = FunctionDef.GetFunctionData(); // THIS IS A COPY

	check(FunctionDef.HasAnyFunctionFlags(FUNC_Delegate));

	const bool bIsMulticastDelegate = FunctionDef.HasAnyFunctionFlags( FUNC_MulticastDelegate );

	// Unmangle the function name
	const FString DelegateName = FunctionDef.GetName().LeftChop(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH);

	// Add class name to beginning of function, to avoid collisions with other classes with the same delegate name in this scope
	check(FunctionData.MarshallAndCallName.StartsWith(DelegateStr));
	FString ShortName = *FunctionData.MarshallAndCallName + UE_ARRAY_COUNT(DelegateStr) - 1;
	FunctionData.MarshallAndCallName = FString::Printf( TEXT( "F%s_DelegateWrapper" ), *ShortName );

	// Setup delegate parameter
	const FString ExtraParam = FString::Printf(
		TEXT( "const %s& %s" ),
		bIsMulticastDelegate ? TEXT( "FMulticastScriptDelegate" ) : TEXT( "FScriptDelegate" ),
		*DelegateName
	);

	FUHTStringBuilder DelegateOutput;
	DelegateOutput.Log(TEXT("static "));

	// export the line that looks like: int32 Main(const FString& Parms)
	ExportNativeFunctionHeader(DelegateOutput, OutReferenceGatherers.ForwardDeclarations, FunctionDef, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Declaration, *ExtraParam, *GetAPIString());

	// Only exporting function prototype
	DelegateOutput.Logf(TEXT(";\r\n"));

	ExportFunction(Out, OutReferenceGatherers, SourceFile, FunctionDef, false);
}

void FNativeClassHeaderGenerator::ExportDelegateDefinition(FOutputDevice& Out, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealFunctionDefinitionInfo& FunctionDef) const
{
	const auto& DelegateStr = TEXT("delegate");

	FFuncInfo FunctionData = FunctionDef.GetFunctionData(); // THIS IS A COPY

	check(FunctionDef.HasAnyFunctionFlags(FUNC_Delegate));

	// Export parameters structs for all delegates.  We'll need these to declare our delegate execution function.
	FUHTStringBuilder DelegateOutput;
	ExportEventParm(DelegateOutput, OutReferenceGatherers.ForwardDeclarations, FunctionDef, /*Indent=*/ 0, /*bOutputConstructor=*/ true, EExportingState::Normal);

	const bool bIsMulticastDelegate = FunctionDef.HasAnyFunctionFlags( FUNC_MulticastDelegate );

	// Unmangle the function name
	const FString DelegateName = FunctionDef.GetName().LeftChop(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH);

	// Always export delegate wrapper functions as inline
	FunctionData.FunctionExportFlags |= FUNCEXPORT_Inline;

	// Add class name to beginning of function, to avoid collisions with other classes with the same delegate name in this scope
	check(FunctionData.MarshallAndCallName.StartsWith(DelegateStr));
	FString ShortName = *FunctionData.MarshallAndCallName + UE_ARRAY_COUNT(DelegateStr) - 1;
	FunctionData.MarshallAndCallName = FString::Printf( TEXT( "F%s_DelegateWrapper" ), *ShortName );

	// Setup delegate parameter
	const FString ExtraParam = FString::Printf(
		TEXT( "const %s& %s" ),
		bIsMulticastDelegate ? TEXT( "FMulticastScriptDelegate" ) : TEXT( "FScriptDelegate" ),
		*DelegateName
	);

	DelegateOutput.Log(TEXT("static "));

	// export the line that looks like: int32 Main(const FString& Parms)
	ExportNativeFunctionHeader(DelegateOutput, OutReferenceGatherers.ForwardDeclarations, FunctionDef, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Declaration, *ExtraParam, *GetAPIString());

	FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(FunctionDef);

	WriteEventFunctionPrologue(DelegateOutput, 0, Parameters, *FunctionDef.GetOuter(), *DelegateName);
	{
		const TCHAR* DelegateType = bIsMulticastDelegate ? TEXT( "ProcessMulticastDelegate" ) : TEXT( "ProcessDelegate" );
		const TCHAR* DelegateArg  = Parameters.HasParms() ? TEXT("&Parms") : TEXT("NULL");
		DelegateOutput.Logf(TEXT("\t%s.%s<UObject>(%s);\r\n"), *DelegateName, DelegateType, DelegateArg);
	}
	WriteEventFunctionEpilogue(DelegateOutput, 0, Parameters);

	FString MacroName = SourceFile.GetGeneratedMacroName(FunctionData.MacroLine, TEXT("_DELEGATE"));
	WriteMacro(Out, MacroName, DelegateOutput);
}

void FNativeClassHeaderGenerator::ExportEventParm(FUHTStringBuilder& Out, TSet<FString>& ForwardDeclarations, FUnrealFunctionDefinitionInfo& FunctionDef, int32 Indent, bool bOutputConstructor, EExportingState ExportingState)
{
	if (!WillExportEventParms(FunctionDef))
	{
		return;
	}

	FString FunctionName = FunctionDef.GetName();
	if (FunctionDef.HasAnyFunctionFlags(FUNC_Delegate))
	{
		FunctionName.LeftChopInline(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH, false);
	}

	FString EventParmStructName = GetEventStructParamsName(*FunctionDef.GetOuter(), *FunctionName);
	Out.Logf(TEXT("%sstruct %s\r\n"), FCString::Tab(Indent), *EventParmStructName);
	Out.Logf(TEXT("%s{\r\n"), FCString::Tab(Indent));

	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropDef : FunctionDef.GetProperties())
	{
		FPropertyBase& PropertyBase = PropDef->GetPropertyBase();
		if (!PropDef->HasAnyPropertyFlags(CPF_Parm))
		{
			continue;
		}

		ForwardDeclarations.Add(PropDef->GetCPPTypeForwardDeclaration());

		FUHTStringBuilder PropertyText;
		PropertyText.Log(FCString::Tab(Indent + 1));

		bool bEmitConst = PropDef->HasAnyPropertyFlags(CPF_ConstParm) && PropertyBase.IsObjectRefOrObjectRefStaticArray();

		//@TODO: UCREMOVAL: This is awful code duplication to avoid a double-const
		{
			//@TODO: bEmitConst will only be true if we have an object, so checking interface here doesn't do anything.
			// export 'const' for parameters
			const bool bIsConstParam = PropertyBase.IsInterfaceOrInterfaceStaticArray() && !PropDef->HasAllPropertyFlags(CPF_OutParm); //@TODO: This should be const once that flag exists
			const bool bIsOnConstClass = PropertyBase.IsObjectRefOrObjectRefStaticArray() && PropertyBase.ClassDef->HasAnyClassFlags(CLASS_Const);

			if (bIsConstParam || bIsOnConstClass)
			{
				bEmitConst = false; // ExportCppDeclaration will do it for us
			}
		}

		if (bEmitConst)
		{
			PropertyText.Logf(TEXT("const "));
		}

		PropDef->ExportCppDeclaration(PropertyText, EExportedDeclaration::Local, PropDef->GetArrayDimensions());
		ApplyAlternatePropertyExportText(*PropDef, PropertyText, ExportingState);

		PropertyText.Log(TEXT(";\r\n"));
		Out += *PropertyText;

	}
	// constructor must initialize the return property if it needs it
	FUnrealPropertyDefinitionInfo* PropDef = FunctionDef.GetReturn();
	if (PropDef && bOutputConstructor)
	{
		const FPropertyBase& PropertyBase = PropDef->GetPropertyBase();
		FUHTStringBuilder InitializationAr;

		bool bNeedsOutput = true;
		switch (PropertyBase.GetUHTPropertyType())
		{
		case EUHTPropertyType::Struct:
			bNeedsOutput = PropDef->HasNoOpConstructor();
			break;
		case EUHTPropertyType::Name:
		case EUHTPropertyType::Delegate:
		case EUHTPropertyType::MulticastDelegate:
		case EUHTPropertyType::String:
		case EUHTPropertyType::Text:
		case EUHTPropertyType::DynamicArray:
		case EUHTPropertyType::Map:
		case EUHTPropertyType::Set:
		case EUHTPropertyType::Interface:
		case EUHTPropertyType::FieldPath:
			bNeedsOutput = false;
		}

		if (bNeedsOutput)
		{
			check(!PropDef->IsStaticArray()); // can't return arrays
			Out.Logf(TEXT("\r\n%s/** Constructor, initializes return property only **/\r\n"), FCString::Tab(Indent + 1));
			Out.Logf(TEXT("%s%s()\r\n"), FCString::Tab(Indent + 1), *EventParmStructName);
			Out.Logf(TEXT("%s%s %s(%s)\r\n"), FCString::Tab(Indent + 2), TEXT(":"), *PropDef->GetName(), *GetNullParameterValue(*PropDef, true));
			Out.Logf(TEXT("%s{\r\n"), FCString::Tab(Indent + 1));
			Out.Logf(TEXT("%s}\r\n"), FCString::Tab(Indent + 1));
		}
	}
	Out.Logf(TEXT("%s};\r\n"), FCString::Tab(Indent));
}

/**
 * Get the intrinsic null value for this property
 *
 * @param	Prop				the property to get the null value for
 * @param	bMacroContext		true when exporting the P_GET* macro, false when exporting the friendly C++ function header
 *
 * @return	the intrinsic null value for the property (0 for ints, TEXT("") for strings, etc.)
 */
FString FNativeClassHeaderGenerator::GetNullParameterValue(FUnrealPropertyDefinitionInfo& PropertyDef, bool bInitializer/*=false*/)
{
	const FPropertyBase& PropertyBase = PropertyDef.GetPropertyBase();
	switch (PropertyBase.GetUHTPropertyType())
	{
	case EUHTPropertyType::Byte:
	case EUHTPropertyType::Int16:
	case EUHTPropertyType::Int:
	case EUHTPropertyType::Int64:
	case EUHTPropertyType::UInt16:
	case EUHTPropertyType::UInt32:
	case EUHTPropertyType::UInt64:
	case EUHTPropertyType::Float:
	case EUHTPropertyType::Double:
	case EUHTPropertyType::LargeWorldCoordinatesReal:
		return TEXT("0");

	case EUHTPropertyType::Bool:
	case EUHTPropertyType::Bool8:
	case EUHTPropertyType::Bool16:
	case EUHTPropertyType::Bool32:
	case EUHTPropertyType::Bool64:
		return TEXT("false");

	case EUHTPropertyType::ObjectReference:
	case EUHTPropertyType::ObjectPtrReference:
	case EUHTPropertyType::SoftObjectReference:
	case EUHTPropertyType::WeakObjectReference:
	case EUHTPropertyType::LazyObjectReference:
		return TEXT("NULL");

	case EUHTPropertyType::Interface:
		return TEXT("NULL");

	case EUHTPropertyType::Name:
		return TEXT("NAME_None");

	case EUHTPropertyType::String:
		return TEXT("TEXT(\"\")");

	case EUHTPropertyType::DynamicArray:
	case EUHTPropertyType::Map:
	case EUHTPropertyType::Set:
	case EUHTPropertyType::Delegate:
	case EUHTPropertyType::MulticastDelegate:
	{
		FString Type, ExtendedType;
		Type = PropertyDef.GetCPPType(&ExtendedType, CPPF_OptionalValue);
		return Type + ExtendedType + TEXT("()");
	}

	case EUHTPropertyType::Struct:
	{
		bool bHasNoOpConstuctor = PropertyDef.HasNoOpConstructor();
		if (bInitializer && bHasNoOpConstuctor)
		{
			return TEXT("ForceInit");
		}

		FString Type, ExtendedType;
		Type = PropertyDef.GetCPPType(&ExtendedType, CPPF_OptionalValue);
		return Type + ExtendedType + (bHasNoOpConstuctor ? TEXT("(ForceInit)") : TEXT("()"));
	}

	case EUHTPropertyType::Text:
		return TEXT("FText::GetEmpty()");

	case EUHTPropertyType::Enum:
	{
		if (PropertyBase.EnumDef->GetCppForm() != UEnum::ECppForm::EnumClass)
		{
			return TEXT("0");
		}
		return FString::Printf(TEXT("(%s)0"), *PropertyDef.GetCPPType());
	}

	case EUHTPropertyType::FieldPath:
		return TEXT("nullptr");

	default:
		UE_LOG(LogCompile, Fatal, TEXT("GetNullParameterValue - Unhandled property type '%s': %s"), *PropertyDef.GetEngineClassName(), *PropertyDef.GetPathName());
		return TEXT("");
	}
}


FString FNativeClassHeaderGenerator::GetFunctionReturnString(FUnrealFunctionDefinitionInfo& FunctionDef, FReferenceGatherers& OutReferenceGatherers)
{
	FString Result;

	if (FUnrealPropertyDefinitionInfo* ReturnDef = FunctionDef.GetReturn())
	{
		FString ExtendedReturnType;
		OutReferenceGatherers.ForwardDeclarations.Add(ReturnDef->GetCPPTypeForwardDeclaration());
		FString ReturnType = ReturnDef->GetCPPType(&ExtendedReturnType, CPPF_ArgumentOrReturnValue);
		FUHTStringBuilder ReplacementText;
		ReplacementText += MoveTemp(ReturnType);
		ApplyAlternatePropertyExportText(*ReturnDef, ReplacementText, EExportingState::Normal);
		Result = MoveTemp(ReplacementText) + MoveTemp(ExtendedReturnType);
	}
	else
	{
		Result = TEXT("void");
	}

	return Result;
}

void FNativeClassHeaderGenerator::CheckRPCFunctions(FReferenceGatherers& OutReferenceGatherers, FUnrealFunctionDefinitionInfo& FunctionDef, const FString& ClassName, const FFindDelcarationResults& Implementation, const FFindDelcarationResults& Validation, const FUnrealSourceFile& SourceFile) const
{
	bool bHasImplementation = Implementation.WasFound();
	bool bHasValidate = Validation.WasFound();

	const FFuncInfo& FunctionData = FunctionDef.GetFunctionData();
	FString FunctionReturnType = GetFunctionReturnString(FunctionDef, OutReferenceGatherers);
	const TCHAR* ConstModifier = (FunctionDef.HasAllFunctionFlags(FUNC_Const) ? TEXT("const ") : TEXT(" "));

	const bool bIsNative = FunctionDef.HasAllFunctionFlags(FUNC_Native);
	const bool bIsNet = FunctionDef.HasAllFunctionFlags(FUNC_Net);
	const bool bIsNetValidate = FunctionDef.HasAllFunctionFlags(FUNC_NetValidate);
	const bool bIsNetResponse = FunctionDef.HasAllFunctionFlags(FUNC_NetResponse);
	const bool bIsBlueprintEvent = FunctionDef.HasAllFunctionFlags(FUNC_BlueprintEvent);

	bool bNeedsImplementation = (bIsNet && !bIsNetResponse) || bIsBlueprintEvent || bIsNative;
	bool bNeedsValidate = (bIsNative || bIsNet) && !bIsNetResponse && bIsNetValidate;

	check(bNeedsImplementation || bNeedsValidate);

	FString ParameterString = GetFunctionParameterString(FunctionDef, OutReferenceGatherers);
	const FString& Filename = SourceFile.GetFilename();
	const FString& FileContent = SourceFile.GetContent();

	//
	// Get string with function specifiers, listing why we need _Implementation or _Validate functions.
	//
	TArray<const TCHAR*, TInlineAllocator<4>> FunctionSpecifiers;
	if (bIsNative)			{ FunctionSpecifiers.Add(TEXT("Native"));			}
	if (bIsNet)				{ FunctionSpecifiers.Add(TEXT("Net"));				}
	if (bIsBlueprintEvent)	{ FunctionSpecifiers.Add(TEXT("BlueprintEvent"));	}
	if (bIsNetValidate)		{ FunctionSpecifiers.Add(TEXT("NetValidate"));		}

	check(FunctionSpecifiers.Num() > 0);

	//
	// Coin static_assert message
	//
	FUHTStringBuilder AssertMessage;
	AssertMessage.Logf(TEXT("Function %s was marked as %s"), *(FunctionDef.GetName()), FunctionSpecifiers[0]);
	for (int32 i = 1; i < FunctionSpecifiers.Num(); ++i)
	{
		AssertMessage.Logf(TEXT(", %s"), FunctionSpecifiers[i]);
	}

	AssertMessage.Logf(TEXT("."));

	//
	// Check if functions are missing.
	//
	if (bNeedsImplementation && !bHasImplementation)
	{
		FString FunctionDecl = FString::Printf(TEXT("virtual %s %s::%s(%s) %s"), *FunctionReturnType, *ClassName, *FunctionData.CppImplName, *ParameterString, *ConstModifier);
		FunctionDef.Throwf(TEXT("%s Declare function %s"), *AssertMessage, *FunctionDecl);
	}

	if (bNeedsValidate && !bHasValidate)
	{
		FString FunctionDecl = FString::Printf(TEXT("virtual bool %s::%s(%s) %s"), *ClassName, *FunctionData.CppValidationImplName, *ParameterString, *ConstModifier);
		FunctionDef.Throwf(TEXT("%s Declare function %s"), *AssertMessage, *FunctionDecl);
	}

	//
	// If all needed functions are declared, check if they have virtual specifiers.
	//
	if (bNeedsImplementation && bHasImplementation && !Implementation.IsVirtual())
	{
		FString FunctionDecl = FString::Printf(TEXT("%s %s::%s(%s) %s"), *FunctionReturnType, *ClassName, *FunctionData.CppImplName, *ParameterString, *ConstModifier);
		FunctionDef.Throwf(TEXT("Declared function %sis not marked as virtual."), *FunctionDecl);
	}

	if (bNeedsValidate && bHasValidate && !Validation.IsVirtual())
	{
		FString FunctionDecl = FString::Printf(TEXT("bool %s::%s(%s) %s"), *ClassName, *FunctionData.CppValidationImplName, *ParameterString, *ConstModifier);
		FunctionDef.Throwf(TEXT("Declared function %sis not marked as virtual."), *FunctionDecl);
	}
}

void FNativeClassHeaderGenerator::ExportNativeFunctionHeader(
	FOutputDevice&                   Out,
	TSet<FString>&                   ForwardDeclarations,
	FUnrealFunctionDefinitionInfo&	 FunctionDef,
	const FFuncInfo&				 FunctionData,
	EExportFunctionType::Type        FunctionType,
	EExportFunctionHeaderStyle::Type FunctionHeaderStyle,
	const TCHAR*                     ExtraParam,
	const TCHAR*                     APIString
)
{
	const bool bIsDelegate   = FunctionDef.HasAnyFunctionFlags( FUNC_Delegate );
	const bool bIsInterface  = !bIsDelegate && FunctionDef.GetOwnerClass()->HasAnyClassFlags(CLASS_Interface);
	const bool bIsK2Override = FunctionDef.HasAnyFunctionFlags( FUNC_BlueprintEvent );

	if (!bIsDelegate)
	{
		Out.Log(TEXT("\t"));
	}

	if (FunctionHeaderStyle == EExportFunctionHeaderStyle::Declaration)
	{
		// cpp implementation of functions never have these appendages

		// If the function was marked as 'RequiredAPI', then add the *_API macro prefix.  Note that if the class itself
		// was marked 'RequiredAPI', this is not needed as C++ will exports all methods automatically.
		if (FunctionType != EExportFunctionType::Event &&
			!FunctionDef.GetOwnerClass()->HasAnyClassFlags(CLASS_RequiredAPI) &&
			(FunctionData.FunctionExportFlags & FUNCEXPORT_RequiredAPI))
		{
			Out.Log(APIString);
		}

		if(FunctionType == EExportFunctionType::Interface)
		{
			Out.Log(TEXT("static "));
		}
		else if (bIsK2Override)
		{
			Out.Log(TEXT("virtual "));
		}
		// if the owning class is an interface class
		else if ( bIsInterface )
		{
			Out.Log(TEXT("virtual "));
		}
		// this is not an event, the function is not a static function and the function is not marked final
		else if ( FunctionType != EExportFunctionType::Event && !FunctionDef.HasAnyFunctionFlags(FUNC_Static) && !(FunctionData.FunctionExportFlags & FUNCEXPORT_Final) )
		{
			Out.Log(TEXT("virtual "));
		}
		else if( FunctionData.FunctionExportFlags & FUNCEXPORT_Inline )
		{
			Out.Log(TEXT("inline "));
		}
	}

	FUnrealPropertyDefinitionInfo* ReturnPropertyDef = FunctionDef.GetReturn();
	if (ReturnPropertyDef != nullptr)
	{
		if (ReturnPropertyDef->HasAnyPropertyFlags(EPropertyFlags::CPF_ConstParm))
		{
			Out.Log(TEXT("const "));
		}

		FString ExtendedReturnType;
		FString ReturnType = ReturnPropertyDef->GetCPPType(&ExtendedReturnType, (FunctionHeaderStyle == EExportFunctionHeaderStyle::Definition && (FunctionType != EExportFunctionType::Interface) ? CPPF_Implementation : 0) | CPPF_ArgumentOrReturnValue);
		ForwardDeclarations.Add(ReturnPropertyDef->GetCPPTypeForwardDeclaration());
		FUHTStringBuilder ReplacementText;
		ReplacementText += ReturnType;
		ApplyAlternatePropertyExportText(*ReturnPropertyDef, ReplacementText, EExportingState::Normal);
		Out.Logf(TEXT("%s%s"), *ReplacementText, *ExtendedReturnType);
	}
	else
	{
		Out.Log( TEXT("void") );
	}

	FString FunctionName;
	if (FunctionHeaderStyle == EExportFunctionHeaderStyle::Definition)
	{
		FunctionName = FString::Printf(TEXT("%s::"), *UHTCastChecked<FUnrealClassDefinitionInfo>(FunctionDef.GetOuter()).GetAlternateNameCPP(bIsInterface || FunctionType == EExportFunctionType::Interface));
	}

	if (FunctionType == EExportFunctionType::Interface)
	{
		FunctionName += FString::Printf(TEXT("Execute_%s"), *FunctionDef.GetName());
	}
	else if (FunctionType == EExportFunctionType::Event)
	{
		FunctionName += FunctionData.MarshallAndCallName;
	}
	else
	{
		FunctionName += FunctionData.CppImplName;
	}

	Out.Logf(TEXT(" %s("), *FunctionName);

	int32 ParmCount=0;

	// Emit extra parameter if we have one
	if( ExtraParam )
	{
		Out.Log(ExtraParam);
		++ParmCount;
	}

	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : FunctionDef.GetProperties())
	{
		if (!PropertyDef->HasSpecificPropertyFlags(CPF_Parm | CPF_ReturnParm, CPF_Parm))
		{
			continue;
		}

		ForwardDeclarations.Add(PropertyDef->GetCPPTypeForwardDeclaration());

		if( ParmCount++ )
		{
			Out.Log(TEXT(", "));
		}

		FUHTStringBuilder PropertyText;

		PropertyDef->ExportCppDeclaration( PropertyText, EExportedDeclaration::Parameter, PropertyDef->GetArrayDimensions() );
		ApplyAlternatePropertyExportText(*PropertyDef, PropertyText, EExportingState::Normal);

		Out.Log(*PropertyText);
	}

	Out.Log( TEXT(")") );
	if (FunctionType != EExportFunctionType::Interface)
	{
		if (!bIsDelegate && FunctionDef.HasAllFunctionFlags(FUNC_Const))
		{
			Out.Log( TEXT(" const") );
		}

		if (bIsInterface && FunctionHeaderStyle == EExportFunctionHeaderStyle::Declaration)
		{
			// all methods in interface classes are pure virtuals
			if (bIsK2Override)
			{
				// For BlueprintNativeEvent methods we emit a stub implementation. This allows Blueprints that implement the interface class to be nativized.
				FString ReturnValue;
				if (ReturnPropertyDef != nullptr)
				{
					if (ReturnPropertyDef->IsByteEnumOrByteEnumStaticArray())
					{ 
						ReturnValue = FString::Printf(TEXT(" return TEnumAsByte<%s>(%s); "), *ReturnPropertyDef->GetPropertyBase().AsEnum()->GetCppType(), *GetNullParameterValue(*ReturnPropertyDef, false));
					}
					else
					{
						ReturnValue = FString::Printf(TEXT(" return %s; "), *GetNullParameterValue(*ReturnPropertyDef, false));
					}
				}

				Out.Logf(TEXT(" {%s}"), *ReturnValue);
			}
			else
			{
				Out.Log(TEXT("=0"));
			}
		}
	}
}

/**
 * Export the actual internals to a standard thunk function
 *
 * @param RPCWrappers output device for writing
 * @param FunctionData function data for the current function
 * @param Parameters list of parameters in the function
 * @param Return return parameter for the function
 * @param DeprecationWarningOutputDevice Device to output deprecation warnings for _Validate and _Implementation functions.
 */
void FNativeClassHeaderGenerator::ExportFunctionThunk(FUHTStringBuilder& RPCWrappers, FReferenceGatherers& OutReferenceGatherers, FUnrealFunctionDefinitionInfo& FunctionDef, const TArray<FUnrealPropertyDefinitionInfo*>& ParameterDefs, FUnrealPropertyDefinitionInfo* ReturnDef) const
{
	const FFuncInfo& FunctionData = FunctionDef.GetFunctionData();

	// export the GET macro for this parameter
	FString ParameterList;
	for (int32 ParameterIndex = 0; ParameterIndex < ParameterDefs.Num(); ParameterIndex++)
	{
		FUnrealPropertyDefinitionInfo* ParamDef = ParameterDefs[ParameterIndex];
		const FPropertyBase& PropertyBase = ParamDef->GetPropertyBase();
		OutReferenceGatherers.ForwardDeclarations.Add(ParamDef->GetCPPTypeForwardDeclaration());

		FString EvalBaseText = TEXT("P_GET_");	// e.g. P_GET_STR
		FString EvalModifierText;				// e.g. _REF
		FString EvalParameterText;				// e.g. (UObject*,NULL)

		FString TypeText;

		if (ParamDef->IsStaticArray())
		{
			EvalBaseText += TEXT("ARRAY");
			TypeText = ParamDef->GetCPPType();
		}
		else
		{
			EvalBaseText += ParamDef->GetCPPMacroType(TypeText);

			if (PropertyBase.ArrayType == EArrayType::Dynamic && PropertyBase.Type == CPT_Interface)
			{
				FString InterfaceTypeText;
				ParamDef->GetValuePropDef().GetCPPMacroType(InterfaceTypeText);
				TypeText += FString::Printf(TEXT("<%s>"), *InterfaceTypeText);
			}
		}

		bool bPassAsNoPtr = ParamDef->HasAllPropertyFlags(CPF_UObjectWrapper | CPF_OutParm) && ParamDef->IsClassRefOrClassRefStaticArray();
		if (bPassAsNoPtr)
		{
			TypeText = ParamDef->GetCPPType();
		}

		FUHTStringBuilder ReplacementText;
		ReplacementText += TypeText;

		ApplyAlternatePropertyExportText(*ParamDef, ReplacementText, EExportingState::Normal);
		TypeText = ReplacementText;

		FString DefaultValueText;
		FString ParamPrefix = TEXT("Z_Param_");

		// if this property is an out parm, add the REF tag
		if (ParamDef->HasAnyPropertyFlags(CPF_OutParm))
		{
			if (!bPassAsNoPtr)
			{
				EvalModifierText += TEXT("_REF");
			}
			else
			{
				// Parameters passed as TSubclassOf<Class>& shouldn't have asterisk added.
				EvalModifierText += TEXT("_REF_NO_PTR");
			}

			ParamPrefix += TEXT("Out_");
		}

		// if this property requires a specialization, add a comma to the type name so we can print it out easily
		if (TypeText != TEXT(""))
		{
			TypeText += TCHAR(',');
		}

		FString ParamName = ParamPrefix + ParamDef->GetName();

		EvalParameterText = FString::Printf(TEXT("(%s%s%s)"), *TypeText, *ParamName, *DefaultValueText);

		RPCWrappers.Logf(TEXT("\t\t%s%s%s;" LINE_TERMINATOR_ANSI), *EvalBaseText, *EvalModifierText, *EvalParameterText);

		// add this property to the parameter list string
		if (ParameterList.Len())
		{
			ParameterList += TCHAR(',');
		}

		if (PropertyBase.Type == CPT_Delegate && PropertyBase.IsPrimitiveOrPrimitiveStaticArray())
		{
			// For delegates, add an explicit conversion to the specific type of delegate before passing it along
			const FString FunctionName = PropertyBase.FunctionDef->GetName().LeftChop(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH);
			ParamName = FString::Printf(TEXT("F%s(%s)"), *FunctionName, *ParamName);
		}

		if (PropertyBase.Type == CPT_MulticastDelegate && PropertyBase.IsPrimitiveOrPrimitiveStaticArray())
		{
			// For delegates, add an explicit conversion to the specific type of delegate before passing it along
			const FString FunctionName = PropertyBase.FunctionDef->GetName().LeftChop(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX_LENGTH);
			ParamName = FString::Printf(TEXT("F%s(%s)"), *FunctionName, *ParamName);
		}

		if (FUnrealEnumDefinitionInfo* EnumDef = PropertyBase.IsPrimitiveOrPrimitiveStaticArray() ? PropertyBase.AsEnum() : nullptr)
		{
			// For enums, add an explicit conversion
			if (!ParamDef->HasAnyPropertyFlags(CPF_OutParm))
			{
				ParamName = FString::Printf(TEXT("%s(%s)"), *EnumDef->GetCppType(), *ParamName);
			}
			else
			{
				if (EnumDef->GetCppForm() == UEnum::ECppForm::EnumClass)
				{
					// If we're an enum class don't require the wrapper
					ParamName = FString::Printf(TEXT("(%s&)(%s)"), *EnumDef->GetCppType(), *ParamName);
				}
				else
				{
					ParamName = FString::Printf(TEXT("(TEnumAsByte<%s>&)(%s)"), *EnumDef->GetCppType(), *ParamName);
				}
			}
		}

		ParameterList += ParamName;
	}

	RPCWrappers += TEXT("\t\tP_FINISH;" LINE_TERMINATOR_ANSI);
	RPCWrappers += TEXT("\t\tP_NATIVE_BEGIN;" LINE_TERMINATOR_ANSI);

	FUnrealClassDefinitionInfo* OwnerClassDef = FunctionDef.GetOwnerClass();
	check(OwnerClassDef);

	FString ClassName = OwnerClassDef->GetName();

	bool bHasImplementation = FindDeclaration(*OwnerClassDef, FunctionData.CppImplName).WasFound();
	bool bHasValidate = FindDeclaration(*OwnerClassDef, FunctionData.CppValidationImplName).WasFound();

	bool bShouldEnableImplementationDeprecation =
		// Enable deprecation warnings only if GENERATED_BODY is used inside class or interface (not GENERATED_UCLASS_BODY etc.)
		OwnerClassDef->HasGeneratedBody()
		// and implementation function is called, but not the one declared by user
		&& (FunctionData.CppImplName != FunctionDef.GetName() && !bHasImplementation);

	bool bShouldEnableValidateDeprecation =
		// Enable deprecation warnings only if GENERATED_BODY is used inside class or interface (not GENERATED_UCLASS_BODY etc.)
		OwnerClassDef->HasGeneratedBody()
		// and validation function is called
		&& FunctionDef.HasAnyFunctionFlags(FUNC_NetValidate) && !bHasValidate;

	//Emit warning here if necessary
	FUHTStringBuilder FunctionDeclaration;
	ExportNativeFunctionHeader(FunctionDeclaration, OutReferenceGatherers.ForwardDeclarations, FunctionDef, FunctionData, EExportFunctionType::Function, EExportFunctionHeaderStyle::Declaration, nullptr, *GetAPIString());

	// Call the validate function if there is one
	if (!(FunctionData.FunctionExportFlags & FUNCEXPORT_CppStatic) && FunctionDef.HasAnyFunctionFlags(FUNC_NetValidate))
	{
		RPCWrappers.Logf(TEXT("\t\tif (!P_THIS->%s(%s))" LINE_TERMINATOR_ANSI), *FunctionData.CppValidationImplName, *ParameterList);
		RPCWrappers.Logf(TEXT("\t\t{" LINE_TERMINATOR_ANSI));
		RPCWrappers.Logf(TEXT("\t\t\tRPC_ValidateFailed(TEXT(\"%s\"));" LINE_TERMINATOR_ANSI), *FunctionData.CppValidationImplName);
		RPCWrappers.Logf(TEXT("\t\t\treturn;" LINE_TERMINATOR_ANSI));	// If we got here, the validation function check failed
		RPCWrappers.Logf(TEXT("\t\t}" LINE_TERMINATOR_ANSI));
	}

	// write out the return value
	RPCWrappers.Log(TEXT("\t\t"));
	if (ReturnDef)
	{
		OutReferenceGatherers.ForwardDeclarations.Add(ReturnDef->GetCPPTypeForwardDeclaration());

		FUHTStringBuilder ReplacementText;
		FString ReturnExtendedType;
		ReplacementText += ReturnDef->GetCPPType(&ReturnExtendedType);
		ApplyAlternatePropertyExportText(*ReturnDef, ReplacementText, EExportingState::Normal);

		FString ReturnType = ReplacementText;
		if (ReturnDef->HasAnyPropertyFlags(CPF_ConstParm) && ReturnDef->IsObjectRefOrObjectRefStaticArray())
		{
			ReturnType = TEXT("const ") + ReturnType;
		}
		RPCWrappers.Logf(TEXT("*(%s%s*)" PREPROCESSOR_TO_STRING(RESULT_PARAM) "="), *ReturnType, *ReturnExtendedType);
	}

	// export the call to the C++ version
	if (FunctionData.FunctionExportFlags & FUNCEXPORT_CppStatic)
	{
		RPCWrappers.Logf(TEXT("%s::%s(%s);" LINE_TERMINATOR_ANSI), *FunctionDef.GetOwnerClass()->GetAlternateNameCPP(), *FunctionData.CppImplName, *ParameterList);
	}
	else
	{
		RPCWrappers.Logf(TEXT("P_THIS->%s(%s);" LINE_TERMINATOR_ANSI), *FunctionData.CppImplName, *ParameterList);
	}
	RPCWrappers += TEXT("\t\tP_NATIVE_END;" LINE_TERMINATOR_ANSI);
}

FString FNativeClassHeaderGenerator::GetFunctionParameterString(FUnrealFunctionDefinitionInfo& FunctionDef, FReferenceGatherers& OutReferenceGatherers)
{
	FString ParameterList;
	FUHTStringBuilder PropertyText;

	for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : FunctionDef.GetProperties())
	{
		OutReferenceGatherers.ForwardDeclarations.Add(PropertyDef->GetCPPTypeForwardDeclaration());

		if (!PropertyDef->HasSpecificPropertyFlags(CPF_Parm | CPF_ReturnParm, CPF_Parm))
		{
			break;
		}

		if (ParameterList.Len())
		{
			ParameterList += TEXT(", ");
		}

		PropertyDef->ExportCppDeclaration(PropertyText, EExportedDeclaration::Parameter, PropertyDef->GetArrayDimensions(), 0, true);
		ApplyAlternatePropertyExportText(*PropertyDef, PropertyText, EExportingState::Normal);

		ParameterList += PropertyText;
		PropertyText.Reset();
	}

	return ParameterList;
}

struct FNativeFunctionStringBuilder
{
	FUHTStringBuilder RPCWrappers;
	FUHTStringBuilder RPCImplementations;
	FUHTStringBuilder AutogeneratedBlueprintFunctionDeclarations;
	FUHTStringBuilder AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared;
	FUHTStringBuilder AutogeneratedStaticData;
	FUHTStringBuilder AutogeneratedStaticDataFuncs;
	FUHTStringBuilder AccessorWrappers;
	FUHTStringBuilder AccessorWrapperImplementations;
};

void FNativeClassHeaderGenerator::ExportNativeFunctions(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& OutGeneratedCPPText, FOutputDevice& OutMacroCalls, FOutputDevice& OutNoPureDeclsMacroCalls, FReferenceGatherers& OutReferenceGatherers, const FUnrealSourceFile& SourceFile, FUnrealClassDefinitionInfo& ClassDef) const
{
	FNativeFunctionStringBuilder RuntimeStringBuilders;
	FNativeFunctionStringBuilder EditorStringBuilders;

	const FString ClassCPPName = ClassDef.GetAlternateNameCPP(ClassDef.HasAnyClassFlags(CLASS_Interface));

	// gather static class data
	TArray<FString> SparseClassDataTypes;
	ClassDef.GetSparseClassDataTypes(SparseClassDataTypes);
	FString FullClassName = ClassDef.GetNameWithPrefix();
	for (const FString& SparseClassDataString : SparseClassDataTypes)
	{
		TArray<FString> SuperSparseClassDataTypes;
		if (FUnrealClassDefinitionInfo* SuperClass = ClassDef.GetSuperClass())
		{
			SuperClass->GetSparseClassDataTypes(SuperSparseClassDataTypes);
		}

		if (SparseClassDataTypes != SuperSparseClassDataTypes)
		{
			FString Empty;
			const FString& API = ClassDef.HasAnyClassFlags(CLASS_MinimalAPI) ? GetAPIString() : Empty;

			RuntimeStringBuilders.AutogeneratedStaticData.Logf(TEXT("%sF%s* Get%s();\r\n"), *API, *SparseClassDataString, *SparseClassDataString);
			RuntimeStringBuilders.AutogeneratedStaticData.Logf(TEXT("%sF%s* Get%s() const;\r\n"), *API, *SparseClassDataString, *SparseClassDataString);
			RuntimeStringBuilders.AutogeneratedStaticData.Logf(TEXT("%sconst F%s* Get%s(EGetSparseClassDataMethod GetMethod) const;\r\n"), *API, *SparseClassDataString, *SparseClassDataString);
			RuntimeStringBuilders.AutogeneratedStaticData.Logf(TEXT("%sstatic UScriptStruct* StaticGet%sScriptStruct();\r\n"), *API, *SparseClassDataString);
		}

		FUnrealScriptStructDefinitionInfo* SparseClassDataStructDef = GTypeDefinitionInfoMap.FindByName<FUnrealScriptStructDefinitionInfo>(*SparseClassDataString);
		while (SparseClassDataStructDef != nullptr)
		{
			for (TSharedRef<FUnrealPropertyDefinitionInfo> PropertyDef : SparseClassDataStructDef->GetProperties())
			{
				FString ReturnExtendedType;
				FString VarType = PropertyDef->GetCPPType(&ReturnExtendedType, EPropertyExportCPPFlags::CPPF_ArgumentOrReturnValue | EPropertyExportCPPFlags::CPPF_Implementation);
				if (!ReturnExtendedType.IsEmpty())
				{
					VarType.Append(ReturnExtendedType);
				}
				FString VarName = PropertyDef->GetName();
				FString CleanVarName = VarName;
				if (PropertyDef->IsBooleanOrBooleanStaticArray() && VarName.StartsWith(TEXT("b"), ESearchCase::CaseSensitive))
				{
					CleanVarName = VarName.RightChop(1);
				}

				if (!PropertyDef->HasMetaData(NAME_NoGetter))
				{
					if (PropertyDef->HasMetaData(NAME_GetByRef))
					{
						RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("const %s& Get%s()\r\n"), *VarType, *CleanVarName);
					}
					else
					{
						RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("%s Get%s()\r\n"), *VarType, *CleanVarName);
					}
					RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("{\r\n"));
					RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("\treturn Get%s()->%s;\r\n"), *SparseClassDataString, *VarName);
					RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("}\r\n"));

					if (PropertyDef->HasMetaData(NAME_GetByRef))
					{
						RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("const %s& Get%s() const\r\n"), *VarType, *CleanVarName);
					}
					else
					{
						RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("%s Get%s() const\r\n"), *VarType, *CleanVarName);
					}
					RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("{\r\n"));
					RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("\treturn Get%s()->%s;\r\n"), *SparseClassDataString, *VarName);
					RuntimeStringBuilders.AutogeneratedStaticDataFuncs.Logf(TEXT("}\r\n"));
				}
			}

			SparseClassDataStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(SparseClassDataStructDef->GetSuperStructInfo().Struct);
		}
	}

	TArray<TSharedRef<FUnrealFunctionDefinitionInfo>> Functions;
	Algo::Copy(ClassDef.GetFunctions(), Functions);
	Algo::Reverse(Functions);

	// export the C++ stubs
	for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : Functions)
	{
		const FFuncInfo& FunctionData = FunctionDef->GetFunctionData();
		if (!FunctionDef->HasAnyFunctionFlags(FUNC_Native))
		{
			continue;
		}

		const bool bEditorOnlyFunc = FunctionDef->HasAnyFunctionFlags(FUNC_EditorOnly);
		FNativeFunctionStringBuilder& FuncStringBuilders = bEditorOnlyFunc ? EditorStringBuilders : RuntimeStringBuilders;

		// Custom thunks don't get any C++ stub function generated
		if (FunctionData.FunctionExportFlags & FUNCEXPORT_CustomThunk)
		{
			continue;
		}

		// Should we emit these to RPC wrappers or just ignore them?
		const bool bWillBeProgrammerTyped = FunctionData.CppImplName == FunctionDef->GetName();

		if (!bWillBeProgrammerTyped)
		{
			FString FunctionName = FunctionDef->GetName();

			FFindDelcarationResults Implementation = FindDeclaration(ClassDef, FunctionData.CppImplName);
			FFindDelcarationResults Validation = FindDeclaration(ClassDef, FunctionData.CppValidationImplName);
			const FString& API = GetAPIString();

			//Emit warning here if necessary
			FUHTStringBuilder FunctionDeclaration;
			ExportNativeFunctionHeader(FunctionDeclaration, OutReferenceGatherers.ForwardDeclarations, *FunctionDef, FunctionData, EExportFunctionType::Function, EExportFunctionHeaderStyle::Declaration, nullptr, *API);
			FunctionDeclaration.Log(TEXT(";\r\n"));

			// Declare validation function if needed
			if (FunctionDef->HasAnyFunctionFlags(FUNC_NetValidate))
			{
				FString ParameterList = GetFunctionParameterString(*FunctionDef, OutReferenceGatherers);

				FStringOutputDevice ValidDecl;
				ValidDecl.Logf(TEXT("\t"));
				if (!FunctionDef->GetOwnerClass()->HasAnyClassFlags(CLASS_RequiredAPI) && (FunctionData.FunctionExportFlags & FUNCEXPORT_RequiredAPI))
				{
					ValidDecl.Logf(TEXT("%s"), *API);
				}
				const TCHAR* Virtual = (!FunctionDef->HasAnyFunctionFlags(FUNC_Static) && !(FunctionData.FunctionExportFlags & FUNCEXPORT_Final)) ? TEXT("virtual") : TEXT("");
				ValidDecl.Logf(TEXT("%s bool %s(%s);\r\n"), Virtual, * FunctionData.CppValidationImplName, * ParameterList);

				FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarations.Log(*ValidDecl);
				if (!Validation.WasFound())
				{
					FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared.Log(*ValidDecl);
				}
			}

			FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarations.Log(*FunctionDeclaration);
			if (!Implementation.WasFound() && FunctionData.CppImplName != FunctionName)
			{
				FuncStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared.Log(*FunctionDeclaration);
			}

			// Versions that skip function autodeclaration throw an error when a function is missing.
			if (ClassDef.HasGeneratedBody() && (ClassDef.GetGeneratedCodeVersion() > EGeneratedCodeVersion::V1))
			{
				CheckRPCFunctions(OutReferenceGatherers, *FunctionDef, ClassCPPName, Implementation, Validation, SourceFile);
			}
		}

		FuncStringBuilders.RPCWrappers.Log(TEXT("\r\n"));

		// if this function was originally declared in a base class, and it isn't a static function,
		// only the C++ function header will be exported
		if (!ShouldExportFunction(*FunctionDef))
		{
			continue;
		}

		// export the script wrappers
		FuncStringBuilders.RPCWrappers.Logf(TEXT("\tDECLARE_FUNCTION(%s);"), *FunctionData.UnMarshallAndCallName);
		FuncStringBuilders.RPCImplementations.Logf(TEXT("\tDEFINE_FUNCTION(%s::%s)"), *ClassCPPName, *FunctionData.UnMarshallAndCallName);
		FuncStringBuilders.RPCImplementations += TEXT(LINE_TERMINATOR_ANSI "\t{" LINE_TERMINATOR_ANSI);

		FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(*FunctionDef);
		ExportFunctionThunk(FuncStringBuilders.RPCImplementations, OutReferenceGatherers, *FunctionDef, Parameters.Parms, Parameters.Return);

		FuncStringBuilders.RPCImplementations += TEXT("\t}" LINE_TERMINATOR_ANSI);
	}

	// static class data
	{
		FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_SPARSE_DATA"));

		WriteMacro(OutGeneratedHeaderText, MacroName, RuntimeStringBuilders.AutogeneratedStaticData + RuntimeStringBuilders.AutogeneratedStaticDataFuncs);
		OutMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);
		OutNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);
	}

	// Write runtime wrappers
	{
		FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_RPC_WRAPPERS"));

		// WriteMacro has an assumption about what will be at the end of this block that is no longer true due to splitting the
		// definition and implementation, so add on a line terminator to satisfy it
		if (RuntimeStringBuilders.RPCWrappers.Len() > 0)
		{
			RuntimeStringBuilders.RPCWrappers += LINE_TERMINATOR;
		}

		WriteMacro(OutGeneratedHeaderText, MacroName, RuntimeStringBuilders.AutogeneratedBlueprintFunctionDeclarations + RuntimeStringBuilders.RPCWrappers);
		OutMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		// Put static checks before RPCWrappers to get proper messages from static asserts before compiler errors.
		FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_RPC_WRAPPERS_NO_PURE_DECLS"));
		if (ClassDef.GetGeneratedCodeVersion() > EGeneratedCodeVersion::V1)
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, RuntimeStringBuilders.RPCWrappers);
		}
		else
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, RuntimeStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared + RuntimeStringBuilders.RPCWrappers);
		}

		OutNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);

		OutGeneratedCPPText.Log(RuntimeStringBuilders.RPCImplementations);
	}

	// Write editor only RPC wrappers if they exist
	if (EditorStringBuilders.RPCWrappers.Len() > 0)
	{
		OutGeneratedHeaderText.Log( BeginEditorOnlyGuard );

		FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_EDITOR_ONLY_RPC_WRAPPERS"));

		// WriteMacro has an assumption about what will be at the end of this block that is no longer true due to splitting the
		// definition and implementation, so add on a line terminator to satisfy it
		if (EditorStringBuilders.RPCWrappers.Len() > 0)
		{
			EditorStringBuilders.RPCWrappers += LINE_TERMINATOR;
		}

		WriteMacro(OutGeneratedHeaderText, MacroName, EditorStringBuilders.AutogeneratedBlueprintFunctionDeclarations + EditorStringBuilders.RPCWrappers);
		OutMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		// Put static checks before RPCWrappers to get proper messages from static asserts before compiler errors.
		FString NoPureDeclsMacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_EDITOR_ONLY_RPC_WRAPPERS_NO_PURE_DECLS"));
		if (ClassDef.GetGeneratedCodeVersion() > EGeneratedCodeVersion::V1)
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, EditorStringBuilders.RPCWrappers);
		}
		else
		{
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, EditorStringBuilders.AutogeneratedBlueprintFunctionDeclarationsOnlyNotDeclared + EditorStringBuilders.RPCWrappers);
		}

		// write out an else preprocessor block for when not compiling for the editor.  The generated macros should be empty then since the functions are compiled out
		{
			OutGeneratedHeaderText.Log(TEXT("#else\r\n"));

			WriteMacro(OutGeneratedHeaderText, MacroName, TEXT(""));
			WriteMacro(OutGeneratedHeaderText, NoPureDeclsMacroName, TEXT(""));

			OutGeneratedHeaderText.Log(EndEditorOnlyGuard);
		}

		OutNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *NoPureDeclsMacroName);

		OutGeneratedCPPText.Log(BeginEditorOnlyGuard);
		OutGeneratedCPPText.Log(EditorStringBuilders.RPCImplementations);
		OutGeneratedCPPText.Log(EndEditorOnlyGuard);
	}

	// Property setters and getters
	{
		TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = ClassDef.GetProperties();
		
		for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : Properties)
		{
			const bool bEditorOnlyProperty = Prop->IsEditorOnlyProperty();

			if (!Prop->GetPropertyBase().GetterName.IsEmpty() && Prop->GetPropertyBase().bGetterFunctionFound)
			{
				const FString GetterWrapperFunctionName = GetPropertyGetterWrapperName(*Prop);
				RuntimeStringBuilders.AccessorWrappers.Logf(TEXT("static void %s(const void* Object, void* OutValue);\r\n"), *GetterWrapperFunctionName);
				RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\tvoid %s::%s(const void* Object, void* OutValue)\r\n"), *ClassCPPName, *GetterWrapperFunctionName);
				RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t{\r\n");
				if (bEditorOnlyProperty)
				{
					RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t#if WITH_EDITORONLY_DATA\r\n");
				}
				RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\tconst %s* Obj = (const %s*)Object;\r\n"), *ClassCPPName, *ClassCPPName);
				FString ExtendedType;
				FString PropertyType = Prop->GetCPPType(&ExtendedType, CPPF_Implementation | CPPF_ArgumentOrReturnValue);
				PropertyType += ExtendedType;
				if (Prop->GetArrayDimensions())
				{
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s* Source = (%s*)Obj->%s();\r\n"), *PropertyType, *PropertyType, *Prop->GetPropertyBase().GetterName);
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s* Result = (%s*)OutValue;\r\n"), *PropertyType, *PropertyType);
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\tCopyAssignItems(Result, Source, %s);\r\n"), Prop->GetArrayDimensions());
				}
				else
				{
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s& Result = *(%s*)OutValue;\r\n"), *PropertyType, *PropertyType);
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\tResult = (%s)Obj->%s();\r\n"), *PropertyType, *Prop->GetPropertyBase().GetterName);
				}
				if (bEditorOnlyProperty)
				{
					RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t#endif // WITH_EDITORONLY_DATA\r\n");
				}
				RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t}\r\n");
			}
			if (!Prop->GetPropertyBase().SetterName.IsEmpty() && Prop->GetPropertyBase().bSetterFunctionFound)
			{
				const FString SetterWrapperFunctionName = GetPropertySetterWrapperName(*Prop);
				RuntimeStringBuilders.AccessorWrappers.Logf(TEXT("static void %s(void* Object, const void* InValue);\r\n"), *SetterWrapperFunctionName);
				RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\tvoid %s::%s(void* Object, const void* InValue)\r\n"), *ClassCPPName, *SetterWrapperFunctionName);
				RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t{\r\n");
				if (bEditorOnlyProperty)
				{
					RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t#if WITH_EDITORONLY_DATA\r\n");
				}
				RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s* Obj = (%s*)Object;\r\n"), *ClassCPPName, *ClassCPPName);
				FString ExtendedType;
				FString PropertyType = Prop->GetCPPType(&ExtendedType, CPPF_Implementation | CPPF_ArgumentOrReturnValue);
				PropertyType += ExtendedType;
				if (Prop->GetArrayDimensions())
				{
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s* Value = (%s*)InValue;\r\n"), *PropertyType, *PropertyType);
				}
				else if (Prop->IsByteEnumOrByteEnumStaticArray() && Prop->GetPropertyBase().ArrayType == EArrayType::None)
				{
					// If someone passed in a TEnumAsByte instead of the actual enum value, the cast in the else clause would cause an issue.
					// Since this is known to be a TEnumAsByte, we just fetch the first byte.  *HOWEVER* on MSB machines where 
					// the actual enum value is passed in this will fail and return zero if the native size of the enum > 1 byte.
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s Value = (%s)*(uint8*)InValue;\r\n"), *PropertyType, *PropertyType);
				}
				else
				{
					RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\t%s& Value = *(%s*)InValue;\r\n"), *PropertyType, *PropertyType);
				}
				RuntimeStringBuilders.AccessorWrapperImplementations.Logf(TEXT("\t\tObj->%s(Value);\r\n"), *Prop->GetPropertyBase().SetterName);
				if (bEditorOnlyProperty)
				{
					RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t#endif // WITH_EDITORONLY_DATA\r\n");
				}
				RuntimeStringBuilders.AccessorWrapperImplementations += TEXT("\t}\r\n");
			}
		}

		FString MacroName = SourceFile.GetGeneratedMacroName(ClassDef.GetGeneratedBodyLine(), TEXT("_ACCESSORS"));
		WriteMacro(OutGeneratedHeaderText, MacroName, RuntimeStringBuilders.AccessorWrappers);

		OutMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);
		OutNoPureDeclsMacroCalls.Logf(TEXT("\t%s\r\n"), *MacroName);

		OutGeneratedCPPText.Log(RuntimeStringBuilders.AccessorWrapperImplementations);
	}

	if (!SparseClassDataTypes.IsEmpty())
	{
		TArray<FString> SuperSparseClassDataTypes;
		if (FUnrealClassDefinitionInfo* SuperClass = ClassDef.GetSuperClass())
		{
			SuperClass->GetSparseClassDataTypes(SuperSparseClassDataTypes);
		}

		if (SuperSparseClassDataTypes != SparseClassDataTypes)
		{
			for (const FString& SparseClassDataString : SparseClassDataTypes)
			{
				OutGeneratedCPPText.Logf(TEXT("F%s* %s::Get%s() \r\n"), *SparseClassDataString, *ClassCPPName, *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("{ \r\n"));
				OutGeneratedCPPText.Logf(TEXT("\treturn static_cast<F%s*>(GetClass()->GetOrCreateSparseClassData()); \r\n"), *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("} \r\n"));

				OutGeneratedCPPText.Logf(TEXT("F%s* %s::Get%s() const \r\n"), *SparseClassDataString, *ClassCPPName, *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("{ \r\n"));
				OutGeneratedCPPText.Logf(TEXT("\treturn static_cast<F%s*>(GetClass()->GetOrCreateSparseClassData()); \r\n"), *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("} \r\n"));

				OutGeneratedCPPText.Logf(TEXT("const F%s* %s::Get%s(EGetSparseClassDataMethod GetMethod) const \r\n"), *SparseClassDataString, *ClassCPPName, *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("{ \r\n"));
				OutGeneratedCPPText.Logf(TEXT("\treturn static_cast<const F%s*>(GetClass()->GetSparseClassData(GetMethod)); \r\n"), *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("} \r\n"));

				OutGeneratedCPPText.Logf(TEXT("UScriptStruct* %s::StaticGet%sScriptStruct()\r\n"), *ClassCPPName, *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("{ \r\n"));
				OutGeneratedCPPText.Logf(TEXT("\treturn F%s::StaticStruct(); \r\n"), *SparseClassDataString);
				OutGeneratedCPPText.Log(TEXT("} \r\n"));
			}
		}
	}
}


void FNativeClassHeaderGenerator::ExportFieldNotify(FOutputDevice& OutGeneratedHeaderText, FOutputDevice& OutGeneratedCPPText, FOutputDevice& StandardUObjectConstructorsMacroCall, FOutputDevice& EnhancedUObjectConstructorsMacroCall, const FString& ConstructorsMacroPrefix, FUnrealClassDefinitionInfo& ClassDef) const
{
	TArray<TSharedRef<FUnrealPropertyDefinitionInfo>> EditorProperties;
	TArray<TSharedRef<FUnrealPropertyDefinitionInfo>> RuntimeProperties;
	TArray<TSharedRef<FUnrealFunctionDefinitionInfo>> EditorFunctions;
	TArray<TSharedRef<FUnrealFunctionDefinitionInfo>> RuntimeFunctions;
	const FString ClassCPPName = ClassDef.GetAlternateNameCPP();
	const FString APIString = GetAPIString();

	if (ClassDef.HasCustomFieldNotify())
	{
		return;
	}

	{
		TArray<TSharedRef<FUnrealPropertyDefinitionInfo>>& Properties = ClassDef.GetProperties();
		for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : Properties)
		{
			if (Prop->GetPropertyBase().bFieldNotify)
			{
				const bool bEditorOnlyProperty = Prop->IsEditorOnlyProperty();
				if (bEditorOnlyProperty)
				{
					EditorProperties.Add(Prop);
				}
				else
				{
					RuntimeProperties.Add(Prop);
				}
			}
		}
		for (TSharedRef<FUnrealFunctionDefinitionInfo> FunctionDef : ClassDef.GetFunctions())
		{
			if (FunctionDef->GetFunctionData().bFieldNotify)
			{
				const bool bEditorOnlyFunc = FunctionDef->HasAnyFunctionFlags(FUNC_EditorOnly);
				if (bEditorOnlyFunc)
				{
					EditorFunctions.Add(FunctionDef);
				}
				else
				{
					RuntimeFunctions.Add(FunctionDef);
				}
			}
		}
	}

	if (RuntimeProperties.Num() > 0 || EditorProperties.Num() > 0 || RuntimeFunctions.Num() > 0 || EditorFunctions.Num() > 0)
	{
		FUHTStringBuilder RuntimeHeaderStringBuilders;
		FUHTStringBuilder EditorHeaderStringBuilders;

		const bool bOnlyHasEditorFields = RuntimeProperties.Num() == 0 && RuntimeFunctions.Num() == 0;
		const bool bOnlyHasRuntimeFields = EditorProperties.Num() == 0 && EditorFunctions.Num() == 0;
		const bool bHasEditorFields = EditorProperties.Num() > 0 || EditorFunctions.Num() > 0;

		if (bOnlyHasEditorFields)
		{
			OutGeneratedCPPText.Logf(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
		}

		RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_CLASS_DESCRIPTOR_BEGIN(%s)\r\n"), *APIString);
		EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_CLASS_DESCRIPTOR_BEGIN(%s)\r\n"), *APIString);

		//UE_FIELD_NOTIFICATION_DECLARE_FIELD
		{
			for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : RuntimeProperties)
			{
				RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(%s)\r\n"), *Prop->GetName());
				EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(%s)\r\n"), *Prop->GetName());
				OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENT_FIELD(%s, %s)\r\n"), *ClassCPPName ,*Prop->GetName());
			}
			for (TSharedRef<FUnrealFunctionDefinitionInfo>& FunctionDef : RuntimeFunctions)
			{
				RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(%s)\r\n"), *FunctionDef->GetFunctionData().CppImplName);
				EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(%s)\r\n"), *FunctionDef->GetFunctionData().CppImplName);
				OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENT_FIELD(%s, %s)\r\n"), *ClassCPPName, *FunctionDef->GetFunctionData().CppImplName);
			}

			if (bHasEditorFields)
			{
				if (!bOnlyHasEditorFields)
				{
					OutGeneratedCPPText.Logf(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
				}

				for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : EditorProperties)
				{
					EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(%s)\r\n"), *Prop->GetName());
					OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENT_FIELD(%s, %s)\r\n"), *ClassCPPName, *Prop->GetName());
				}
				for (TSharedRef<FUnrealFunctionDefinitionInfo>& FunctionDef : EditorFunctions)
				{
					EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_FIELD(%s)\r\n"), *FunctionDef->GetFunctionData().CppImplName);
					OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENT_FIELD(%s, %s)\r\n"), *ClassCPPName, *FunctionDef->GetFunctionData().CppImplName);
				}

				if (!bOnlyHasEditorFields)
				{
					OutGeneratedCPPText.Logf(TEXT("#endif // WITH_EDITORONLY_DATA\r\n"));
				}
			}
		}

		OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENTATION_BEGIN(%s)\r\n"), *ClassCPPName);

		//UE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD
		{
			bool bIsRuntimeFirst = true;
			bool bIsEditorFirst = true;
			auto AddFieldNotificationEnumField = [&bIsRuntimeFirst, &bIsEditorFirst, &RuntimeHeaderStringBuilders, &EditorHeaderStringBuilders, &OutGeneratedCPPText, &ClassCPPName](bool bRuntime, const FString& FieldName)
			{
				if (bRuntime)
				{
					if (bIsRuntimeFirst)
					{
						bIsRuntimeFirst = false;
						bIsEditorFirst = false;
						RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_BEGIN(%s)\r\n"), *FieldName);
						EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_BEGIN(%s)\r\n"), *FieldName);
					}
					else
					{
						RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD(%s)\r\n"), *FieldName);
						EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD(%s)\r\n"), *FieldName);
					}
				}
				else
				{
					if (bIsEditorFirst)
					{
						bIsEditorFirst = false;
						EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_BEGIN(%s)\r\n"), *FieldName);
					}
					EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD(%s)\r\n"), *FieldName);
				}
				OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENT_ENUM_FIELD(%s, %s)\r\n"), *ClassCPPName, *FieldName);
			};

			bool bIsFirstRuntime = true;
			for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : RuntimeProperties)
			{
				AddFieldNotificationEnumField(true, *Prop->GetName());
			}
			for (TSharedRef<FUnrealFunctionDefinitionInfo>& FunctionDef : RuntimeFunctions)
			{
				AddFieldNotificationEnumField(true, *FunctionDef->GetFunctionData().CppImplName);
			}

			if (bHasEditorFields)
			{
				if (!bOnlyHasEditorFields)
				{
					OutGeneratedCPPText.Logf(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
				}

				for (TSharedRef<FUnrealPropertyDefinitionInfo>& Prop : EditorProperties)
				{
					AddFieldNotificationEnumField(false, *Prop->GetName());
				}
				for (TSharedRef<FUnrealFunctionDefinitionInfo>& FunctionDef : EditorFunctions)
				{
					AddFieldNotificationEnumField(false, *FunctionDef->GetFunctionData().CppImplName);
				}

				if (!bOnlyHasEditorFields)
				{
					OutGeneratedCPPText.Logf(TEXT("#endif // WITH_EDITORONLY_DATA\r\n"));
				}
			}
		}

		RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_END()\r\n"));
		EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_ENUM_FIELD_END()\r\n"));
		RuntimeHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_CLASS_DESCRIPTOR_END();\r\n"));
		EditorHeaderStringBuilders.Logf(TEXT("\tUE_FIELD_NOTIFICATION_DECLARE_CLASS_DESCRIPTOR_END();\r\n"));
		OutGeneratedCPPText.Logf(TEXT("\tUE_FIELD_NOTIFICATION_IMPLEMENTATION_END(%s);\r\n"), *ClassCPPName);

		if (bOnlyHasEditorFields)
		{
			OutGeneratedCPPText.Logf(TEXT("#endif // WITH_EDITORONLY_DATA\r\n"));
		}

		FString HeaderMacroName = ConstructorsMacroPrefix + TEXT("_FIELDNOTIFY");

		if (bHasEditorFields)
		{
			OutGeneratedHeaderText.Logf(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
			WriteMacro(OutGeneratedHeaderText, HeaderMacroName, EditorHeaderStringBuilders);
			OutGeneratedHeaderText.Logf(TEXT("#else //WITH_EDITORONLY_DATA\r\n"));
			WriteMacro(OutGeneratedHeaderText, HeaderMacroName, RuntimeHeaderStringBuilders);
			OutGeneratedHeaderText.Logf(TEXT("#endif // WITH_EDITORONLY_DATA\r\n"));
		}
		else 
		{
			WriteMacro(OutGeneratedHeaderText, HeaderMacroName, RuntimeHeaderStringBuilders);
		}

		StandardUObjectConstructorsMacroCall.Logf(TEXT("\t%s\r\n"), *HeaderMacroName);
		EnhancedUObjectConstructorsMacroCall.Logf(TEXT("\t%s\r\n"), *HeaderMacroName);
	}
}

/**
 * Exports the methods which trigger UnrealScript events and delegates.
 *
 * @param	CallbackFunctions	the functions to export
 */
void FNativeClassHeaderGenerator::ExportCallbackFunctions(
	FOutputDevice&            OutGeneratedHeaderText,
	FOutputDevice&            OutCpp,
	TSet<FString>&            OutFwdDecls,
	const TArray<FUnrealFunctionDefinitionInfo*>& CallbackFunctions,
	const TCHAR*              CallbackWrappersMacroName,
	EExportCallbackType       ExportCallbackType,
	const TCHAR*              APIString
)
{
	FUHTStringBuilder RPCWrappers;

	FMacroBlockEmitter OutCppEditorOnly(OutCpp, TEXT("WITH_EDITOR"));
	for (FUnrealFunctionDefinitionInfo* FunctionDef : CallbackFunctions)
	{
		// Never expecting to export delegate functions this way
		check(!FunctionDef->HasAnyFunctionFlags(FUNC_Delegate));

		const FFuncInfo& FunctionData = FunctionDef->GetFunctionData();
		FString          FunctionName = FunctionDef->GetName();
		FUnrealClassDefinitionInfo&    ClassDef = UHTCastChecked<FUnrealClassDefinitionInfo>(FunctionDef->GetOuter());
		const FString    ClassName = ClassDef.GetAlternateNameCPP();

		if (FunctionDef->HasAnyFunctionFlags(FUNC_NetResponse))
		{
			// Net response functions don't go into the VM
			continue;
		}

		const bool bIsEditorOnly = FunctionDef->HasAnyFunctionFlags(FUNC_EditorOnly);

		OutCppEditorOnly(bIsEditorOnly);

		const bool bWillBeProgrammerTyped = FunctionName == FunctionData.MarshallAndCallName;

		// Emit the declaration if the programmer isn't responsible for declaring this wrapper
		if (!bWillBeProgrammerTyped)
		{
			// export the line that looks like: int32 Main(const FString& Parms)
			ExportNativeFunctionHeader(RPCWrappers, OutFwdDecls, *FunctionDef, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Declaration, nullptr, APIString);

			RPCWrappers.Log(TEXT(";\r\n"));
			RPCWrappers.Log(TEXT("\r\n"));
		}

		FString FunctionNameName;
		if (ExportCallbackType != EExportCallbackType::Interface)
		{
			FunctionNameName = FString::Printf(TEXT("NAME_%s_%s"), *ClassName, *FunctionName);
			OutCpp.Logf(TEXT("\tstatic FName %s = FName(TEXT(\"%s\"));" LINE_TERMINATOR_ANSI), *FunctionNameName, *FunctionName);
		}

		// Emit the thunk implementation
		ExportNativeFunctionHeader(OutCpp, OutFwdDecls, *FunctionDef, FunctionData, EExportFunctionType::Event, EExportFunctionHeaderStyle::Definition, nullptr, APIString);

		FParmsAndReturnProperties Parameters = GetFunctionParmsAndReturn(*FunctionDef);

		if (ExportCallbackType != EExportCallbackType::Interface)
		{
			WriteEventFunctionPrologue(OutCpp, /*Indent=*/ 1, Parameters, ClassDef, *FunctionName);
			{
				// Cast away const just in case, because ProcessEvent isn't const
				OutCpp.Logf(
					TEXT("\t\t%sProcessEvent(FindFunctionChecked(%s),%s);\r\n"),
					(FunctionDef->HasAllFunctionFlags(FUNC_Const)) ? *FString::Printf(TEXT("const_cast<%s*>(this)->"), *ClassName) : TEXT(""),
					*FunctionNameName,
					Parameters.HasParms() ? TEXT("&Parms") : TEXT("NULL")
				);
			}
			WriteEventFunctionEpilogue(OutCpp, /*Indent=*/ 1, Parameters);
		}
		else
		{
			OutCpp.Log(LINE_TERMINATOR);
			OutCpp.Log(TEXT("\t{" LINE_TERMINATOR_ANSI));

			// assert if this is ever called directly
			OutCpp.Logf(TEXT("\t\tcheck(0 && \"Do not directly call Event functions in Interfaces. Call Execute_%s instead.\");" LINE_TERMINATOR_ANSI), *FunctionName);

			// satisfy compiler if it's expecting a return value
			if (Parameters.Return)
			{
				FString EventParmStructName = GetEventStructParamsName(ClassDef, *FunctionName);
				OutCpp.Logf(TEXT("\t\t%s Parms;" LINE_TERMINATOR_ANSI), *EventParmStructName);
				OutCpp.Log(TEXT("\t\treturn Parms.ReturnValue;" LINE_TERMINATOR_ANSI));
			}
			OutCpp.Log(TEXT("\t}" LINE_TERMINATOR_ANSI));
		}
	}

	WriteMacro(OutGeneratedHeaderText, CallbackWrappersMacroName, RPCWrappers);
}


/**
 * Determines if the property has alternate export text associated with it and if so replaces the text in PropertyText with the
 * alternate version. (for example, structs or properties that specify a native type using export-text).  Should be called immediately
 * after ExportCppDeclaration()
 *
 * @param	Prop			the property that is being exported
 * @param	PropertyText	the string containing the text exported from ExportCppDeclaration
 */
void FNativeClassHeaderGenerator::ApplyAlternatePropertyExportText(FUnrealPropertyDefinitionInfo& PropertyDef, FUHTStringBuilder& PropertyText, EExportingState ExportingState)
{
	const FPropertyBase& PropertyBase = PropertyDef.GetPropertyBase();
	if (FUnrealEnumDefinitionInfo* EnumDef = PropertyBase.AsEnum())
	{
		return;
	}

	if (ExportingState == EExportingState::TypeEraseDelegates)
	{
		if (PropertyBase.Type == CPT_Delegate || PropertyBase.Type == CPT_MulticastDelegate)
		{
			FString Original = PropertyDef.GetCPPType();
			const TCHAR* PlaceholderOfSameSizeAndAlignemnt;
			if (PropertyBase.Type == CPT_Delegate)
			{
				PlaceholderOfSameSizeAndAlignemnt = TEXT("FScriptDelegate");
			}
			else
			{
				PlaceholderOfSameSizeAndAlignemnt = TEXT("FMulticastScriptDelegate");
			}
			PropertyText.ReplaceInline(*Original, PlaceholderOfSameSizeAndAlignemnt, ESearchCase::CaseSensitive);
		}
	}
}

static void AddIncludeForType(const FUnrealTypeDefinitionInfo* PropertyTypeDef, TArray<FString>& RelativeIncludes)
{
	if (UHTCast<FUnrealScriptStructDefinitionInfo>(PropertyTypeDef) != nullptr)
	{
		if (PropertyTypeDef->HasSource() && !PropertyTypeDef->GetUnrealSourceFile().IsNoExportTypes())
		{
			FString Header = GetBuildPath(PropertyTypeDef->GetUnrealSourceFile());
			RelativeIncludes.AddUnique(MoveTemp(Header));
		}
	}
}

static void AddIncludeForProperty(const TSharedRef<FUnrealPropertyDefinitionInfo>& PropertyDef, TArray<FString>& RelativeIncludes)
{
	const FPropertyBase& PropertyBase = PropertyDef->GetPropertyBase();
	AddIncludeForType(PropertyBase.TypeDef, RelativeIncludes);
	if (PropertyBase.MapKeyProp != nullptr)
	{
		AddIncludeForType(PropertyBase.MapKeyProp->TypeDef, RelativeIncludes);
	}
}

bool FNativeClassHeaderGenerator::WriteSource(const FManifestModule& Module, FGeneratedFileInfo& FileInfo, const FString& InBodyText, FUnrealSourceFile* InSourceFile, const TSet<FString>& InCrossModuleReferences, const EExportClassOutFlags& ExportFlags)
{
	// Collect the includes if this is from a source file
	TArray<FString> RelativeIncludes;
	if (InSourceFile)
	{
		if (EnumHasAnyFlags(ExportFlags, EExportClassOutFlags::NeedsFastArrayHeaders))
		{
			RelativeIncludes.Add(FString(TEXT("Net/Serialization/FastArraySerializerImplementation.h")));
		}

		bool bAddedStructuredArchiveFromArchiveHeader = false;
		bool bAddedArchiveUObjectFromStructuredArchiveHeader = false;
		bool bAddedCoreNetHeader = false;

		// We need to include the headers that declare any types that aren't pointers.
		// We can't rely on the original header that generates this cpp to include the 
		// headers because it could be forward declaring the types.
		for (const TSharedRef<FUnrealTypeDefinitionInfo>& TypeDef : InSourceFile->GetDefinedTypes())
		{
			if (FUnrealStructDefinitionInfo* Struct = TypeDef->AsStruct())
			{
				// Functions
				for (const TSharedRef<FUnrealFunctionDefinitionInfo>& FunctionDef : Struct->GetFunctions())
				{
					if (!(FunctionDef->GetFunctionData().FunctionExportFlags & FUNCEXPORT_CppStatic) && FunctionDef->HasAnyFunctionFlags(FUNC_NetValidate))
					{
						if (!bAddedCoreNetHeader)
						{
							RelativeIncludes.AddUnique(TEXT("UObject/CoreNet.h"));
							bAddedCoreNetHeader = true;
						}
					}
					for (const TSharedRef<FUnrealPropertyDefinitionInfo>& PropertyDef : FunctionDef->GetProperties())
					{
						AddIncludeForProperty(PropertyDef, RelativeIncludes);
					}
				}

				// Properties
				for (const TSharedRef<FUnrealPropertyDefinitionInfo>& PropertyDef : Struct->GetProperties())
				{
					AddIncludeForProperty(PropertyDef, RelativeIncludes);
				}
			}

			if (FUnrealClassDefinitionInfo* ClassDef = TypeDef->AsClass())
			{
				FUnrealClassDefinitionInfo* ClassWithin = ClassDef->GetClassWithin();
				if (ClassWithin && ClassWithin->HasSource() && !ClassWithin->GetUnrealSourceFile().IsNoExportTypes())
				{
					FString Header = GetBuildPath(ClassWithin->GetUnrealSourceFile());
					RelativeIncludes.AddUnique(MoveTemp(Header));
				}

				if (!bAddedStructuredArchiveFromArchiveHeader && ClassDef->GetArchiveType() == ESerializerArchiveType::StructuredArchiveRecord)
				{
					RelativeIncludes.AddUnique(TEXT("Serialization/StructuredArchive.h"));
					bAddedStructuredArchiveFromArchiveHeader = true;
				}

				if (!bAddedArchiveUObjectFromStructuredArchiveHeader && ClassDef->GetArchiveType() == ESerializerArchiveType::Archive)
				{
					RelativeIncludes.AddUnique(TEXT("Serialization/ArchiveUObjectFromStructuredArchive.h"));
					bAddedArchiveUObjectFromStructuredArchiveHeader = true;
				}
			}
		}

		RelativeIncludes.Sort();

		// Add the source header file to the top of the list
		FString ModuleRelativeFilename = InSourceFile->GetFilename();
		ConvertToBuildIncludePath(Module, ModuleRelativeFilename);
		RelativeIncludes.Remove(ModuleRelativeFilename);
		RelativeIncludes.EmplaceAt(0, MoveTemp(ModuleRelativeFilename));
	}

	FUHTStringBuilder FileText;
	FileText.Log(HeaderCopyright);
	FileText.Log(RequiredCPPIncludes);

	for (const FString& RelativeInclude : RelativeIncludes)
	{
		FileText.Logf(TEXT("#include \"%s\"\r\n"), *RelativeInclude);
	}
	FileText.Log(DisableDeprecationWarnings);

	FString CleanFilename = FPaths::GetCleanFilename(FileInfo.GetFilename());
	CleanFilename.ReplaceInline(TEXT(".gen.cpp"), TEXT(""), ESearchCase::CaseSensitive);
	CleanFilename.ReplaceInline(TEXT("."), TEXT("_"), ESearchCase::CaseSensitive);
	FileText.Logf(TEXT("void EmptyLinkFunctionForGeneratedCode%s() {}" LINE_TERMINATOR_ANSI), *CleanFilename);

	if (InCrossModuleReferences.Num() > 0)
	{
		TArray<const FString*> Sorted;
		Sorted.Reserve(InCrossModuleReferences.Num());
		for (const FString& Ref : InCrossModuleReferences)
		{
			Sorted.Add(&Ref);
		}
		Sorted.Sort();
		FileText.Logf(TEXT("// Cross Module References\r\n"));
		for (const FString* Ref : Sorted)
		{
			FileText.Log(**Ref);
		}
		FileText.Logf(TEXT("// End Cross Module References\r\n"));
	}
	FileText.Log(*InBodyText);
	FileText.Log(EnableDeprecationWarnings);

	return SaveHeaderIfChanged(FileInfo, MoveTemp(FileText));
}

// Constructor.
FNativeClassHeaderGenerator::FNativeClassHeaderGenerator(
	FUnrealPackageDefinitionInfo& InPackageDef)
	: PackageDef(InPackageDef)
{}

const FString& FNativeClassHeaderGenerator::GetAPIString() const
{
	return PackageDef.GetAPI();
}

bool FNativeClassHeaderGenerator::LoadSourceFile(FGeneratedCPP& GeneratedCPP)
{
	const FManifestModule& Module = GeneratedCPP.PackageDef.GetModule();
	FUnrealSourceFile& SourceFile = GeneratedCPP.SourceFile;
	if (!SourceFile.ShouldExport())
	{
		return false;
	}

	FString ModuleRelativeFilename = SourceFile.GetFilename();
	ConvertToBuildIncludePath(Module, ModuleRelativeFilename);

	FString StrippedName = FPaths::GetBaseFilename(MoveTemp(ModuleRelativeFilename));
	FString HeaderPath = (Module.GeneratedIncludeDirectory / StrippedName) + TEXT(".generated.h");
	FString GeneratedSourceFilename = (Module.GeneratedIncludeDirectory / StrippedName) + TEXT(".gen.cpp");

	if (bGoWide)
	{
		GeneratedCPP.Header.StartLoad(MoveTemp(HeaderPath));
		GeneratedCPP.Source.StartLoad(MoveTemp(GeneratedSourceFilename));
	}
	else
	{
		GeneratedCPP.Header.Load(MoveTemp(HeaderPath));
		GeneratedCPP.Source.Load(MoveTemp(GeneratedSourceFilename));
	}
	return true;
}

void FNativeClassHeaderGenerator::GenerateSourceFile(FGeneratedCPP& GeneratedCPP)
{
	FResults::Try([&GeneratedCPP]()
		{
			FUnrealPackageDefinitionInfo& PackageDefLcl = GeneratedCPP.PackageDef;
			const FManifestModule& Module = PackageDefLcl.GetModule();
			const FNativeClassHeaderGenerator Generator(PackageDefLcl);
			FUnrealSourceFile& SourceFile = GeneratedCPP.SourceFile;
			FUHTStringBuilder& GeneratedFunctionDeclarations = GeneratedCPP.GeneratedFunctionDeclarations;
			FUHTStringBuilder& GeneratedHeaderText = GeneratedCPP.Header.GetGeneratedBody();
			FUHTStringBuilder& GeneratedCPPText = GeneratedCPP.Source.GetGeneratedBody();
			FScopedDurationTimer SourceTimer(SourceFile.GetTime(ESourceFileTime::Generate));

			FReferenceGatherers ReferenceGatherers(&GeneratedCPP.CrossModuleReferences, GeneratedCPP.ForwardDeclarations);

			TArray<FUnrealFieldDefinitionInfo*> Types;
			SourceFile.GetScope()->GatherTypes(Types);

			// Make sure that all the types have a definition range.
			for (FUnrealFieldDefinitionInfo* TypeDef : Types)
			{
				TypeDef->ValidateDefinitionRange();
			}

			// Sort by the end of the definition range.  Since classes can contain delegates, we can't use the definition line
			// since that will place the class ahead of the delegate.
			Types.StableSort([](const FUnrealFieldDefinitionInfo& Lhs, const FUnrealFieldDefinitionInfo& Rhs)
				{
					return Lhs.GetDefinitionRange().End < Rhs.GetDefinitionRange().End;
				}
			);

			TArray<FUnrealFieldDefinitionInfo*> Singletons;
			Singletons.Reserve(Types.Num());
			TArray<FUnrealEnumDefinitionInfo*> EnumRegs;
			EnumRegs.Reserve(Types.Num());
			TArray<FUnrealScriptStructDefinitionInfo*> ScriptStructRegs;
			ScriptStructRegs.Reserve(Types.Num());
			TArray<FUnrealClassDefinitionInfo*> ClassRegs;
			ClassRegs.Reserve(Types.Num());

			const FString FileDefineName = SourceFile.GetFileDefineName();
			const FString& StrippedFilename = SourceFile.GetStrippedFilename();

			GeneratedHeaderText.Logf(
				TEXT("#ifdef %s"																	LINE_TERMINATOR_ANSI
				     "#error \"%s.generated.h already included, missing '#pragma once' in %s.h\""	LINE_TERMINATOR_ANSI
				     "#endif"																		LINE_TERMINATOR_ANSI
				     "#define %s"																	LINE_TERMINATOR_ANSI
				LINE_TERMINATOR_ANSI),
				*FileDefineName, *StrippedFilename, *StrippedFilename, *FileDefineName);

			for (FUnrealFieldDefinitionInfo* FieldDef : Types)
			{
				if (FUnrealEnumDefinitionInfo* EnumDef = UHTCast<FUnrealEnumDefinitionInfo>(FieldDef))
				{
					// Is this ever not the case?
					if (EnumDef->GetOuter()->GetObject()->IsA(UPackage::StaticClass()))
					{
						GeneratedFunctionDeclarations.Log(EnumDef->GetExternDecl(true));
						Generator.ExportGeneratedEnumInitCode(GeneratedCPPText, ReferenceGatherers, SourceFile, *EnumDef);
							EnumRegs.Add(EnumDef);
						}
					}
				else if (FUnrealScriptStructDefinitionInfo* ScriptStructDef = UHTCast<FUnrealScriptStructDefinitionInfo>(FieldDef))
				{
					if (ScriptStructDef->HasAnyStructFlags(STRUCT_NoExport))
					{
						Singletons.Add(ScriptStructDef);
					}
					GeneratedFunctionDeclarations.Log(ScriptStructDef->GetExternDecl(true));
					Generator.ExportGeneratedStructBodyMacros(GeneratedHeaderText, GeneratedCPPText, ReferenceGatherers, SourceFile, *ScriptStructDef, GeneratedCPP.ExportFlags);
					if (ScriptStructDef->HasAnyStructFlags(STRUCT_Native))
					{
						ScriptStructRegs.Add(ScriptStructDef);
					}
				}
				else if (FUnrealFunctionDefinitionInfo* FunctionDef = UHTCast<FUnrealFunctionDefinitionInfo>(FieldDef))
				{
						Singletons.Add(FunctionDef);
					GeneratedFunctionDeclarations.Log(FunctionDef->GetExternDecl(true));
					Generator.ExportDelegateDeclaration(GeneratedCPPText, ReferenceGatherers, SourceFile, *FunctionDef);
					Generator.ExportDelegateDefinition(GeneratedHeaderText, ReferenceGatherers, SourceFile, *FunctionDef);
				}
				else if (FUnrealClassDefinitionInfo* ClassDef = UHTCast<FUnrealClassDefinitionInfo>(FieldDef))
				{
					if (!ClassDef->HasAnyClassFlags(CLASS_Intrinsic))
					{
						Generator.ExportClassFromSourceFileInner(GeneratedHeaderText, GeneratedCPPText, GeneratedFunctionDeclarations, ReferenceGatherers, *ClassDef, SourceFile, GeneratedCPP.ExportFlags);
							ClassRegs.Add(ClassDef);
						}
					}
				}

			GeneratedHeaderText.Log(TEXT("#undef CURRENT_FILE_ID\r\n"));
			GeneratedHeaderText.Logf(TEXT("#define CURRENT_FILE_ID %s\r\n\r\n\r\n"), *SourceFile.GetFileId());

			for (FUnrealFieldDefinitionInfo* FieldDef : Types)
			{
				if (FUnrealEnumDefinitionInfo* EnumDef = UHTCast<FUnrealEnumDefinitionInfo>(FieldDef))
				{
					Generator.ExportEnum(GeneratedHeaderText, *EnumDef);
				}
			}

			// Generate the single registration method
			if (!EnumRegs.IsEmpty() || !ScriptStructRegs.IsEmpty() || !ClassRegs.IsEmpty())
			{
				static const TCHAR* Prefix = TEXT("Z_CompiledInDeferFile_");
				FString StaticsName = FString::Printf(TEXT("%s%s_Statics"), Prefix, *SourceFile.GetFileId());

				GeneratedCPPText.Logf(TEXT("\tstruct %s\r\n"), *StaticsName);
				GeneratedCPPText.Log(TEXT("\t{\r\n"));

				size_t EditorOnlyEnumCount = 0;
				for (FUnrealEnumDefinitionInfo* EnumDef : EnumRegs)
				{
					if (EnumDef->IsEditorOnly())
					{
						++EditorOnlyEnumCount;
					}
				}
				bool bAllEnumsEditorOnly = !EnumRegs.IsEmpty() && EditorOnlyEnumCount == EnumRegs.Num();

				if (!EnumRegs.IsEmpty())
				{
					if (bAllEnumsEditorOnly)
					{
						GeneratedCPPText.Log(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
					}
					GeneratedCPPText.Log(TEXT("\t\tstatic const FEnumRegisterCompiledInInfo EnumInfo[];\r\n"));
					if (bAllEnumsEditorOnly)
					{
						GeneratedCPPText.Log(TEXT("#endif\r\n"));
					}
				}
				if (!ScriptStructRegs.IsEmpty())
				{
					GeneratedCPPText.Log(TEXT("\t\tstatic const FStructRegisterCompiledInInfo ScriptStructInfo[];\r\n"));
				}
				if (!ClassRegs.IsEmpty())
				{
					GeneratedCPPText.Log(TEXT("\t\tstatic const FClassRegisterCompiledInInfo ClassInfo[];\r\n"));
				}

				GeneratedCPPText.Log(TEXT("\t};\r\n"));

				int32 CombinedHash = -1;
				if (!EnumRegs.IsEmpty())
				{
					if (bAllEnumsEditorOnly)
					{
						GeneratedCPPText.Log(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
					}
					GeneratedCPPText.Logf(TEXT("\tconst FEnumRegisterCompiledInInfo %s::EnumInfo[] = {\r\n"), *StaticsName);
					for (FUnrealEnumDefinitionInfo* EnumDef : EnumRegs)
					{
						if (!bAllEnumsEditorOnly && EnumDef->IsEditorOnly())
						{
							GeneratedCPPText.Log(TEXT("#if WITH_EDITORONLY_DATA\r\n"));
						}
						const FString EnumNameCpp = EnumDef->GetName();
						GeneratedCPPText.Logf(TEXT("\t\t{ %s_StaticEnum, TEXT(\"%s\"), &Z_Registration_Info_UEnum_%s, CONSTRUCT_RELOAD_VERSION_INFO(FEnumReloadVersionInfo, %uU) },\r\n"),
							*EnumNameCpp,
							*EnumNameCpp,
							*EnumNameCpp,
							EnumDef->GetHash(*EnumDef));
						if (!bAllEnumsEditorOnly && EnumDef->IsEditorOnly())
						{
							GeneratedCPPText.Log(TEXT("#endif\r\n"));
						}
						CombinedHash = HashCombine(CombinedHash, EnumDef->GetHash(*EnumDef));
					}
					GeneratedCPPText.Logf(TEXT("\t};\r\n"));
					if (bAllEnumsEditorOnly)
					{
						GeneratedCPPText.Log(TEXT("#endif\r\n"));
					}
				}

				if (!ScriptStructRegs.IsEmpty())
				{
					GeneratedCPPText.Logf(TEXT("\tconst FStructRegisterCompiledInInfo %s::ScriptStructInfo[] = {\r\n"), *StaticsName);
					for (FUnrealScriptStructDefinitionInfo* ScriptStructDef : ScriptStructRegs)
					{
						const FString StructNameCPP = ScriptStructDef->GetAlternateNameCPP();
						GeneratedCPPText.Logf(TEXT("\t\t{ %s::StaticStruct, Z_Construct_UScriptStruct_%s_Statics::NewStructOps, TEXT(\"%s\"), &Z_Registration_Info_UScriptStruct_%s, CONSTRUCT_RELOAD_VERSION_INFO(FStructReloadVersionInfo, sizeof(%s), %uU) },\r\n"),
							*StructNameCPP,
							*StructNameCPP,
							*ScriptStructDef->GetName(),
							*ScriptStructDef->GetName(),
							*StructNameCPP,
							ScriptStructDef->GetHash(*ScriptStructDef));
						CombinedHash = HashCombine(CombinedHash, ScriptStructDef->GetHash(*ScriptStructDef));
					}
					GeneratedCPPText.Logf(TEXT("\t};\r\n"));
				}

				if (!ClassRegs.IsEmpty())
				{
					GeneratedCPPText.Logf(TEXT("\tconst FClassRegisterCompiledInInfo %s::ClassInfo[] = {\r\n"), *StaticsName);
					for (FUnrealClassDefinitionInfo* ClassDef : ClassRegs)
					{
						const FString ClassNameCPP = ClassDef->GetAlternateNameCPP();
						GeneratedCPPText.Logf(TEXT("\t\t{ Z_Construct_UClass_%s, %s::StaticClass, TEXT(\"%s\"), &Z_Registration_Info_UClass_%s, CONSTRUCT_RELOAD_VERSION_INFO(FClassReloadVersionInfo, sizeof(%s), %uU) },\r\n"),
							*ClassNameCPP,
							*ClassNameCPP,
							*ClassNameCPP,
							*ClassNameCPP,
							*ClassNameCPP,
							ClassDef->GetHash(*ClassDef));
						CombinedHash = HashCombine(CombinedHash, ClassDef->GetHash(*ClassDef));
					}
					GeneratedCPPText.Logf(TEXT("\t};\r\n"));
				}

				auto FormatArray = [&StaticsName](bool bIsEmpty, bool bAllEditorOnlyData, const FString& Text)
				{
					if (bAllEditorOnlyData)
					{
						return FString::Printf(TEXT("IF_WITH_EDITORONLY_DATA(%s::%s, nullptr), IF_WITH_EDITORONLY_DATA(UE_ARRAY_COUNT(%s::%s), 0)"), *StaticsName, *Text, *StaticsName, *Text);
					}
					else if (!bIsEmpty)
					{
						return FString::Printf(TEXT("%s::%s, UE_ARRAY_COUNT(%s::%s)"), *StaticsName, *Text, *StaticsName, *Text);
					}
					else
					{
						return FString(TEXT("nullptr, 0"));
					}
				};

				// We add the hash to the name to generate a unique version of the name so that LiveCoding's default "only invoke static constructor once" 
				// is avoided when elements change.
				GeneratedCPPText.Logf(TEXT("\tstatic FRegisterCompiledInInfo %s%s_%u(TEXT(\"%s\"),\r\n\t\t%s,\r\n\t\t%s,\r\n\t\t%s);\r\n"),
					Prefix,
					*SourceFile.GetFileId(),
					CombinedHash,
					*PackageDefLcl.GetName(),
					*FormatArray(ClassRegs.IsEmpty(), false, TEXT("ClassInfo")),
					*FormatArray(ScriptStructRegs.IsEmpty(), false, TEXT("ScriptStructInfo")),
					*FormatArray(EnumRegs.IsEmpty(), bAllEnumsEditorOnly, TEXT("EnumInfo"))
				);
			}

			if (Singletons.Num())
			{
				SourceFile.GetSingletons().Append(MoveTemp(Singletons));
			}

			// Sort the forward declarations to make them more stable
			TArray<FString> Lines;
			GeneratedFunctionDeclarations.ParseIntoArrayLines(Lines);
			TSet<FString> Unique;
			for (const FString& Line : Lines)
			{
				Unique.Add(Line);
			}
			Lines.Empty();
			for (const FString& Line : Unique)
			{
				Lines.Add(Line);
			}
			Lines.Sort();
			GeneratedFunctionDeclarations.Empty();
			for (const FString& Line : Lines)
			{
				GeneratedFunctionDeclarations.Logf(TEXT("%s\r\n"), *Line);
			}
			if (bIncludeDebugOutput)
			{
				GeneratedCPPText.Logf(TEXT("#if 0\r\n"));
				GeneratedCPPText.Log(GeneratedFunctionDeclarations);
				GeneratedCPPText.Logf(TEXT("#endif\r\n"));
			}
		}
	);
}

void FNativeClassHeaderGenerator::WriteSourceFile(FGeneratedCPP& GeneratedCPP)
{
	FResults::Try([&GeneratedCPP]()
		{
			FUnrealPackageDefinitionInfo& PackageDefLcl = GeneratedCPP.PackageDef;
			const FManifestModule& Module = PackageDefLcl.GetModule();
			FUnrealSourceFile& SourceFile = GeneratedCPP.SourceFile;
			FUHTStringBuilder& GeneratedHeaderText = GeneratedCPP.Header.GetGeneratedBody();
			FUHTStringBuilder& GeneratedCPPText = GeneratedCPP.Source.GetGeneratedBody();
			FScopedDurationTimer SourceTimer(SourceFile.GetTime(ESourceFileTime::Generate));

			GeneratedCPP.Source.GenerateBodyHash();

			TSet<FString> AdditionalHeaders;
			if (EnumHasAnyFlags(GeneratedCPP.ExportFlags, EExportClassOutFlags::NeedsPushModelHeaders))
			{
				AdditionalHeaders.Add(FString(TEXT("Net/Core/PushModel/PushModelMacros.h")));
			}

			bool bHasChanged = WriteHeader(GeneratedCPP, GeneratedHeaderText, AdditionalHeaders, GeneratedCPP.ForwardDeclarations);
			WriteSource(Module, GeneratedCPP.Source, GeneratedCPPText, &SourceFile, GeneratedCPP.CrossModuleReferences, GeneratedCPP.ExportFlags);

			SourceFile.SetGeneratedFilename(MoveTemp(GeneratedCPP.Header.GetFilename()));
			SourceFile.SetHasChanged(bHasChanged);
		}
	);
}

void FNativeClassHeaderGenerator::GenerateSourceFiles(
	TArray<FGeneratedCPP>& GeneratedCPPs
)
{
	if (bGoWide)
	{
		TSet<FUnrealSourceFile*> Includes;
		Includes.Reserve(GeneratedCPPs.Num());
		FGraphEventArray TempTasks;
		TempTasks.Reserve(3);
		for (FGeneratedCPP& GeneratedCPP : GeneratedCPPs)
		{
			if (!LoadSourceFile(GeneratedCPP))
			{
				continue;
			}

			TempTasks.Reset();
			GeneratedCPP.Header.AddLoadTaskRef(TempTasks);
			GeneratedCPP.Source.AddLoadTaskRef(TempTasks);

			auto GenerateSource = [&GeneratedCPP]()
			{
				GenerateSourceFile(GeneratedCPP);
			};

			auto WriteGenerated = [&GeneratedCPP]()
			{
				WriteSourceFile(GeneratedCPP);
			};

			Includes.Reset();
			for (FHeaderProvider& Header : GeneratedCPP.SourceFile.GetIncludes())
			{
				if (FUnrealSourceFile* Include = Header.Resolve(GeneratedCPP.SourceFile))
				{
					Includes.Add(Include);
				}
			}

			// Our generation must wait on all of our includes generation to complete
			TempTasks.Reset();
			for (FUnrealSourceFile* Include : Includes)
			{
				FGeneratedCPP& IncludeCPP = GeneratedCPPs[Include->GetOrderedIndex()];
				IncludeCPP.AddGenerateTaskRef(TempTasks);
			}
			GeneratedCPP.GenerateTaskRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(GenerateSource), TStatId(), &TempTasks);

			// Our compare and save must wait on generation and loading of the current header and source
			TempTasks.Reset();
			TempTasks.Add(GeneratedCPP.GenerateTaskRef);
			GeneratedCPP.Header.AddLoadTaskRef(TempTasks);
			GeneratedCPP.Source.AddLoadTaskRef(TempTasks);
			GeneratedCPP.ExportTaskRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(WriteGenerated), TStatId(), &TempTasks);
		}

		// When this task fires, all of the exports for this package have completed
		FGraphEventArray ExportSourceTasks;
		ExportSourceTasks.Reserve(GeneratedCPPs.Num());
		for (FGeneratedCPP& GeneratedCPP : GeneratedCPPs)
		{
			GeneratedCPP.AddExportTaskRef(ExportSourceTasks);
		}

		// This is strange, but flushing the log can take a long time.  Without an explicit flush, we were getting a long stall in the UE_LOG message
		// during the detection of script plugins.  By doing it here, the flush should easily complete during code generation.
		GLog->FlushThreadedLogs(EOutputDeviceRedirectorFlushOptions::Async);

		FTaskGraphInterface::Get().WaitUntilTasksComplete(ExportSourceTasks);
	}
	else
	{
		for (FGeneratedCPP& GeneratedCPP : GeneratedCPPs)
		{
			if (!LoadSourceFile(GeneratedCPP))
			{
				continue;
			}
			GenerateSourceFile(GeneratedCPP);
			WriteSourceFile(GeneratedCPP);
		}
	}
	FResults::WaitForErrorTasks();
}

void FNativeClassHeaderGenerator::Generate(
	FUnrealPackageDefinitionInfo& PackageDef,
	TArray<FGeneratedCPP>& GeneratedCPPs
)
{
	UPackage* Package = PackageDef.GetPackage();
	FString PackageName = FPackageName::GetShortName(Package);
	const FManifestModule& Module = PackageDef.GetModule();
	const bool bWriteClassesH = PackageDef.GetWriteClassesH();
	const bool bAllowSaveExportedHeaders = Module.SaveExportedHeaders;

	FGraphEventArray TempTasks;
	TempTasks.Reserve(3);

	// Create a list of all exported files 
	TArray<FGeneratedCPP*> Exported;
	Exported.Reserve(GeneratedCPPs.Num());
	for (FGeneratedCPP& GeneratedCPP : GeneratedCPPs)
	{
		if (&GeneratedCPP.PackageDef == &PackageDef && GeneratedCPP.SourceFile.ShouldExport())
		{
			Exported.Emplace(&GeneratedCPP);
		}
	}

	// Create a sorted list of the exported source files so that the generated code is consistent
	TArray<FGeneratedCPP*> ExportedSorted(Exported);
	ExportedSorted.Sort([](const FGeneratedCPP& Lhs, const FGeneratedCPP& Rhs) { return Lhs.SourceFile.GetFilename() < Rhs.SourceFile.GetFilename(); });

	// Generate the package tasks
	FGraphEventArray PackageTasks;
	PackageTasks.Reserve(2);

	// If we are generated the classes H file, start the preload process for the H file
	TArray<FGeneratedFileInfo> GeneratedPackageFileInfo;
	GeneratedPackageFileInfo.Reserve(2);

	if (bWriteClassesH)
	{
		// Start loading the original header file for comparison
		FGeneratedFileInfo& GeneratedFileInfo = GeneratedPackageFileInfo[GeneratedPackageFileInfo.Add(FGeneratedFileInfo(bAllowSaveExportedHeaders))];
		FString ClassesHeaderPath = Module.GeneratedIncludeDirectory / (PackageName + TEXT("Classes.h"));
		GeneratedFileInfo.StartLoad(MoveTemp(ClassesHeaderPath));

		auto ClasssesH = [&PackageDef, &GeneratedFileInfo, &ExportedSorted]()
		{
			FResults::Try([&PackageDef, &GeneratedFileInfo, &ExportedSorted]()
			{
				const FManifestModule& Module = PackageDef.GetModule();

				// Write the classes and enums header prefixes.
				FUHTStringBuilder ClassesHText;
				ClassesHText.Log(HeaderCopyright);
				ClassesHText.Log(TEXT("#pragma once\r\n"));
				ClassesHText.Log(TEXT("\r\n"));
				ClassesHText.Log(TEXT("\r\n"));

				// Fill with the rest source files from this package.
				TSet<FUnrealSourceFile*> PublicHeaderGroupIncludes;
				for (FGeneratedCPP* GeneratedCPP : ExportedSorted)
				{
					if (GeneratedCPP->SourceFile.IsPublic())
					{
						PublicHeaderGroupIncludes.Add(&GeneratedCPP->SourceFile);
					}
				}
				for (TSharedRef<FUnrealSourceFile>& SourceFile : PackageDef.GetAllSourceFiles())
				{
					if (SourceFile->IsPublic())
					{
						PublicHeaderGroupIncludes.Add(&*SourceFile);
					}
				}

				// Make the public header list stable regardless of the order the files are parsed.
				TArray<FString> BuildPaths;
				BuildPaths.Reserve(PublicHeaderGroupIncludes.Num());
				for (FUnrealSourceFile* SourceFile : PublicHeaderGroupIncludes)
				{
					BuildPaths.Emplace(GetBuildPath(*SourceFile));
				}
				BuildPaths.Sort();

				for (const FString& BuildPath : BuildPaths)
				{
					ClassesHText.Logf(TEXT("#include \"%s\"" LINE_TERMINATOR_ANSI), *BuildPath);
				}

				ClassesHText.Log(LINE_TERMINATOR);

				// Save the classes header if it has changed.
				SaveHeaderIfChanged(GeneratedFileInfo, MoveTemp(ClassesHText));
			});
		};

		TempTasks.Reset();
		GeneratedFileInfo.AddLoadTaskRef(TempTasks);
		FGraphEventRef GenerateTask = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(ClasssesH), TStatId(), &TempTasks);
		PackageTasks.Add(GenerateTask);
	}

	// now export the names for the functions in this package
	// notice we always export this file (as opposed to only exporting if we have any marked names)
	// because there would be no way to know when the file was created otherwise

	int32 InitGenIndex = -1;
	{
		FGeneratedFileInfo& GeneratedFileInfo = GeneratedPackageFileInfo[InitGenIndex = GeneratedPackageFileInfo.Add(FGeneratedFileInfo(bAllowSaveExportedHeaders))];
		FString GeneratedSourceFilename = Module.GeneratedIncludeDirectory / FString::Printf(TEXT("%s.init.gen.cpp"), *PackageName);
		GeneratedFileInfo.StartLoad(MoveTemp(GeneratedSourceFilename));

		auto Functions = [&PackageDef, &GeneratedFileInfo, &ExportedSorted]()
		{
			FResults::Try([&PackageDef, &GeneratedFileInfo, &ExportedSorted]()
			{
				const FManifestModule& Module = PackageDef.GetModule();

				// Export an include line for each header
				FUHTStringBuilder GeneratedFunctionDeclarations;
				for (FGeneratedCPP* GeneratedCPP : ExportedSorted)
				{
					GeneratedFunctionDeclarations.Log(GeneratedCPP->GeneratedFunctionDeclarations);
				}

				if (GeneratedFunctionDeclarations.Len())
				{
					FNativeClassHeaderGenerator Generator(PackageDef);

					uint32 CombinedHash = 0;
					for (FGeneratedCPP* GeneratedCPP : ExportedSorted)
					{
						uint32 SourceHash = GeneratedCPP->Source.GetGeneratedBodyHash();
						if (CombinedHash == 0)
						{
							// Don't combine in the first case because it keeps GUID backwards compatibility
							CombinedHash = SourceHash;
						}
						else
						{
							CombinedHash = HashCombine(SourceHash, CombinedHash);
						}
					}

					Generator.ExportGeneratedPackageInitCode(GeneratedFileInfo.GetGeneratedBody(), *GeneratedFunctionDeclarations, ExportedSorted, CombinedHash);
					WriteSource(Module, GeneratedFileInfo, GeneratedFileInfo.GetGeneratedBody(), nullptr, TSet<FString>(), EExportClassOutFlags::None);
				}
			});
		};


		TempTasks.Reset();
		GeneratedFileInfo.AddLoadTaskRef(TempTasks);
		FGraphEventRef GenerateTask = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(Functions), TStatId(), &TempTasks);
		PackageTasks.Add(GenerateTask);
	}

	FTaskGraphInterface::Get().WaitUntilTasksComplete(PackageTasks);

	// Collect all of the paths and save tasks
	int32 MaxCount = (Exported.Num() * 2) + GeneratedPackageFileInfo.Num();
	TSet<FString> PackageHeaderPaths;
	TArray<FString> TempHeaderPaths;
	FGraphEventArray SaveTasks;
	PackageHeaderPaths.Reserve(MaxCount);
	TempHeaderPaths.Reserve(MaxCount);
	SaveTasks.Reserve(MaxCount);
	for (FGeneratedCPP* GeneratedCPP : Exported)
	{
		if (bAllowSaveExportedHeaders)
		{
			GeneratedCPP->Header.AddPackageFilename(PackageHeaderPaths);
			GeneratedCPP->Source.AddPackageFilename(PackageHeaderPaths);
		}
		GeneratedCPP->Header.AddTempFilename(TempHeaderPaths);
		GeneratedCPP->Source.AddTempFilename(TempHeaderPaths);
		GeneratedCPP->Header.AddSaveTaskRef(SaveTasks);
		GeneratedCPP->Source.AddSaveTaskRef(SaveTasks);
	}
	for (FGeneratedFileInfo& GeneratedFileInfo : GeneratedPackageFileInfo)
	{
		if (bAllowSaveExportedHeaders)
		{
			GeneratedFileInfo.AddPackageFilename(PackageHeaderPaths);
		}
		GeneratedFileInfo.AddTempFilename(TempHeaderPaths);
		GeneratedFileInfo.AddSaveTaskRef(SaveTasks);
	}

	// Export all changed headers from their temp files to the .h files
	ExportUpdatedHeaders(MoveTemp(PackageName), MoveTemp(TempHeaderPaths), SaveTasks);

	// Delete stale *.generated.h files
	if (bAllowSaveExportedHeaders)
	{
		DeleteUnusedGeneratedHeaders(MoveTemp(PackageHeaderPaths));
	}
}

TArray<FWildcardString> GSourceWildcards = { TEXT("*.generated.cpp"), TEXT("*.generated.*.cpp"), TEXT("*.gen.cpp"), TEXT("*.gen.*.cpp") };
TArray<FWildcardString> GHeaderWildcards = { TEXT("*.generated.h") };

bool MatchesWildcards(const TArray<FWildcardString>& Wildcards, const TCHAR* Filename)
{
	for (const FWildcardString& Wildcard : Wildcards)
	{
		if (Wildcard.IsMatch(Filename))
		{
			return true;
		}
	}
	return false;
}

void FNativeClassHeaderGenerator::DeleteUnusedGeneratedHeaders(TSet<FString>&& PackageHeaderPathSet)
{
	auto DeleteUnusedGeneratedHeadersTask = [PackageHeaderPathSet = MoveTemp(PackageHeaderPathSet)]()
	{
		TSet<FString> AllIntermediateFolders;

		for (const FString& PackageHeader : PackageHeaderPathSet)
		{
			FString IntermediatePath = FPaths::GetPath(PackageHeader);

			if (AllIntermediateFolders.Contains(IntermediatePath))
			{
				continue;
			}

			class FFileVisitor : public IPlatformFile::FDirectoryVisitor
			{
			public:
				const FString& Directory;
				const TSet<FString>& PackageHeaderPathSet;

				FFileVisitor(const FString& InDirectory, const TSet<FString>& InPackageHeaderPathSet)
					: Directory(InDirectory)
					, PackageHeaderPathSet(InPackageHeaderPathSet)
				{
				}

				virtual bool Visit(const TCHAR* FileNameOrDirectory, bool bIsDirectory)
				{
					if (!bIsDirectory)
					{
						FString Fullpath(FileNameOrDirectory);
						if (!PackageHeaderPathSet.Contains(Fullpath))
						{
							FString Filename = FPaths::GetCleanFilename(Fullpath);
							if (MatchesWildcards(GSourceWildcards, *Filename))
							{
								IFileManager::Get().Delete(*Fullpath);
							}
							else if (MatchesWildcards(GHeaderWildcards, *Filename))
							{
								// Is this intrinsic test valid anymore?
								FString BaseFilename = FPaths::GetBaseFilename(Filename);
								const int32   GeneratedIndex = BaseFilename.Find(TEXT(".generated"), ESearchCase::IgnoreCase, ESearchDir::FromEnd);
								const FString ClassName = MoveTemp(BaseFilename).Mid(0, GeneratedIndex);
								UClass* IntrinsicClass = FEngineAPI::FindFirstObject<UClass>(*ClassName, EFindFirstObjectOptions::None, ELogVerbosity::Fatal, TEXT("looking for intrinsic class"));
								if (!IntrinsicClass || !IntrinsicClass->HasAnyClassFlags(CLASS_Intrinsic))
								{
									IFileManager::Get().Delete(*Fullpath);
								}
							}
						}
					}
					return true;
				}
			};

			FFileVisitor Visitor(IntermediatePath, PackageHeaderPathSet);
			IFileManager::Get().IterateDirectory(*IntermediatePath, Visitor);

			AllIntermediateFolders.Add(MoveTemp(IntermediatePath));
		}
	};

	GAsyncFileTasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(DeleteUnusedGeneratedHeadersTask), TStatId()));
}

FCriticalSection TestCommandLineCS;

bool FNativeClassHeaderGenerator::SaveHeaderIfChanged(FGeneratedFileInfo& FileInfo, FString&& InNewHeaderContents)
{
	if (!FileInfo.AllowSaveExportedHeaders())
	{
		// Return false indicating that the header did not need updating
		return false;
	}

	static bool bTestedCmdLine = false;
	if (!bTestedCmdLine)
	{
		FScopeLock Lock(&TestCommandLineCS);
		if (!bTestedCmdLine)
		{
			const FString& ProjectSavedDir = FPaths::ProjectSavedDir();

			if (FParse::Param(FCommandLine::Get(), TEXT("WRITEREF")))
			{
				const FString ReferenceGeneratedCodePath = ProjectSavedDir / TEXT("ReferenceGeneratedCode/");

				bWriteContents = true;
				UE_LOG(LogCompile, Log, TEXT("********************************* Writing reference generated code to %s."), *ReferenceGeneratedCodePath);
				UE_LOG(LogCompile, Log, TEXT("********************************* Deleting all files in ReferenceGeneratedCode."));
				IFileManager::Get().DeleteDirectory(*ReferenceGeneratedCodePath, false, true);
				IFileManager::Get().MakeDirectory(*ReferenceGeneratedCodePath);
			}
			else if (FParse::Param(FCommandLine::Get(), TEXT("VERIFYREF")))
			{
				const FString ReferenceGeneratedCodePath = ProjectSavedDir / TEXT("ReferenceGeneratedCode/");
				const FString VerifyGeneratedCodePath = ProjectSavedDir / TEXT("VerifyGeneratedCode/");

				bVerifyContents = true;
				UE_LOG(LogCompile, Log, TEXT("********************************* Writing generated code to %s and comparing to %s"), *VerifyGeneratedCodePath, *ReferenceGeneratedCodePath);
				UE_LOG(LogCompile, Log, TEXT("********************************* Deleting all files in VerifyGeneratedCode."));
				IFileManager::Get().DeleteDirectory(*VerifyGeneratedCodePath, false, true);
				IFileManager::Get().MakeDirectory(*VerifyGeneratedCodePath);
			}
			bTestedCmdLine = true;
		}
	}

	if (bWriteContents || bVerifyContents)
	{
		// UHT is getting fast enough that we can create an I/O storm in these large directories.
		// The lock limits us to writing one file at a time.  It doesn't impact performance 
		// significantly.
		static FCriticalSection WritePacer;

		const FString& ProjectSavedDir = FPaths::ProjectSavedDir();
		const FString CleanFilename = FPaths::GetCleanFilename(FileInfo.GetFilename());

		const FString Ref = ProjectSavedDir / TEXT("ReferenceGeneratedCode") / CleanFilename;

		if (bWriteContents)
		{
			FScopeLock Lock(&WritePacer);
			bool Written = FFileHelper::SaveStringToFile(InNewHeaderContents, *Ref);
			check(Written);
		}
		else
		{
			{
				FScopeLock Lock(&WritePacer);
				const FString Verify = ProjectSavedDir / TEXT("VerifyGeneratedCode") / CleanFilename;
				bool Written = FFileHelper::SaveStringToFile(InNewHeaderContents, *Verify);
				check(Written);
			}

			FString RefHeader;
			FString Message;
			{
				SCOPE_SECONDS_COUNTER_UHT(LoadHeaderContentFromFile);
				if (!FFileHelper::LoadFileToString(RefHeader, *Ref))
				{
					Message = FString::Printf(TEXT("********************************* %s appears to be a new generated file."), *CleanFilename);
				}
				else
				{
					if (FCString::Strcmp(*InNewHeaderContents, *RefHeader) != 0)
					{
						Message = FString::Printf(TEXT("********************************* %s has changed."), *CleanFilename);
					}
				}
			}
			if (Message.Len())
			{
				UE_LOG(LogCompile, Log, TEXT("%s"), *Message);
				ChangeMessages.AddUnique(MoveTemp(Message));
			}
		}
	}

	FString HeaderPathStr = FileInfo.GetFilename();
	const FString& OriginalContents = FileInfo.GetOriginalContents();

	const bool bHasChanged = OriginalContents.Len() != InNewHeaderContents.Len() || FCString::Strcmp(*OriginalContents, *InNewHeaderContents);
	if (bHasChanged)
	{
		static const bool bFailIfGeneratedCodeChanges = FParse::Param(FCommandLine::Get(), TEXT("FailIfGeneratedCodeChanges"));
		if (bFailIfGeneratedCodeChanges)
		{
			FString ConflictPath = HeaderPathStr + TEXT(".conflict");
			FFileHelper::SaveStringToFile(InNewHeaderContents, *ConflictPath);

			FResults::SetResult(ECompilationResult::FailedDueToHeaderChange);
			FUHTMessage(FileInfo.GetFilename()).Throwf(TEXT("ERROR: '%s': Changes to generated code are not allowed - conflicts written to '%s'"), *HeaderPathStr, *ConflictPath);
		}

		// save the updated version to a tmp file so that the user can see what will be changing
		FString TmpHeaderFilename = GenerateTempHeaderName(HeaderPathStr, false);

		auto SaveTempTask = [&FileInfo, TmpHeaderFilename, InNewHeaderContents = MoveTemp(InNewHeaderContents)]()
		{
			// delete any existing temp file
			IFileManager::Get().Delete(*TmpHeaderFilename, false, true);
			if (!FFileHelper::SaveStringToFile(InNewHeaderContents, *TmpHeaderFilename))
			{
				FUHTMessage(FileInfo.GetFilename()).LogWarning(TEXT("Failed to save header export preview: '%s'"), *TmpHeaderFilename);
			}
		};

		FileInfo.SetSaveTaskRef(FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(SaveTempTask), TStatId()));
		FileInfo.SetTempFilename(MoveTemp(TmpHeaderFilename));
	}

	// Remember this header filename to be able to check for any old (unused) headers later.
	HeaderPathStr.ReplaceInline(TEXT("\\"), TEXT("/"), ESearchCase::CaseSensitive);
	FileInfo.SetPackageFilename(MoveTemp(HeaderPathStr));
	return bHasChanged;
}

FString FNativeClassHeaderGenerator::GenerateTempHeaderName( const FString& CurrentFilename, bool bReverseOperation )
{
	if (bReverseOperation)
	{
		FString Reversed = CurrentFilename;
		Reversed.RemoveFromEnd(TEXT(".tmp"), ESearchCase::CaseSensitive);
		return Reversed;
	}
	return CurrentFilename + TEXT(".tmp");
}

void FNativeClassHeaderGenerator::ExportUpdatedHeaders(FString&& PackageName, TArray<FString>&& TempHeaderPaths, FGraphEventArray& InTempSaveTasks)
{	
	// Asynchronously move the headers to the correct locations
	if (TempHeaderPaths.Num() > 0)
	{
		auto MoveHeadersTask = [PackageName = MoveTemp(PackageName), TempHeaderPaths = MoveTemp(TempHeaderPaths)]()
		{
			ParallelFor(TempHeaderPaths.Num(), [&](int32 Index)
			{
				const FString& TmpFilename = TempHeaderPaths[Index];
				FString Filename = GenerateTempHeaderName(TmpFilename, true);
				if (!IFileManager::Get().Move(*Filename, *TmpFilename, true, true))
				{
					UE_LOG(LogCompile, Error, TEXT("Error exporting %s: couldn't write file '%s'"), *PackageName, *Filename);
				}
				else
				{
					UE_LOG(LogCompile, Log, TEXT("Exported updated C++ header: %s"), *Filename);
				}
			});
		};

		FTaskGraphInterface::Get().WaitUntilTasksComplete(InTempSaveTasks);
		GAsyncFileTasks.Add(FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(MoveHeadersTask), TStatId()));
	}
}

/** Get all script plugins based on ini setting */
void GetScriptPlugins(TArray<IScriptGeneratorPluginInterface*>& ScriptPlugins)
{
	if (!GManifest.IsGameTarget)
	{
		UE_LOG(LogCompile, Log, TEXT("Script generator plugins only enabled in game targets."));
	}

	ScriptPlugins = IModularFeatures::Get().GetModularFeatureImplementations<IScriptGeneratorPluginInterface>(TEXT("ScriptGenerator"));
	UE_LOG(LogCompile, Log, TEXT("Found %d script generator plugins."), ScriptPlugins.Num());

	// Check if we can use these plugins and initialize them
	for (int32 PluginIndex = ScriptPlugins.Num() - 1; PluginIndex >= 0; --PluginIndex)
	{
		IScriptGeneratorPluginInterface* ScriptGenerator = ScriptPlugins[PluginIndex];
		bool bSupportedPlugin = ScriptGenerator->SupportsTarget(GManifest.TargetName);
		if (bSupportedPlugin)
		{
			// Find the right output directory for this plugin base on its target (Engine-side) plugin name.
			FString GeneratedCodeModuleName = ScriptGenerator->GetGeneratedCodeModuleName();
			const FManifestModule* GeneratedCodeModule = NULL;
			FString OutputDirectory;
			FString IncludeBase;
			for (const FManifestModule& Module : GManifest.Modules)
			{
				if (Module.Name == GeneratedCodeModuleName)
				{
					GeneratedCodeModule = &Module;
				}
			}
			if (GeneratedCodeModule)
			{
				UE_LOG(LogCompile, Log, TEXT("Initializing script generator \'%s\'"), *ScriptGenerator->GetGeneratorName());
				ScriptGenerator->Initialize(GManifest.RootLocalPath, GManifest.RootBuildPath, GeneratedCodeModule->GeneratedIncludeDirectory, GeneratedCodeModule->IncludeBase);
			}
			else
			{
				// Can't use this plugin
				UE_LOG(LogCompile, Log, TEXT("Unable to determine output directory for %s. Cannot export script glue with \'%s\'"), *GeneratedCodeModuleName, *ScriptGenerator->GetGeneratorName());
				bSupportedPlugin = false;
			}
		}
		if (!bSupportedPlugin)
		{
			UE_LOG(LogCompile, Log, TEXT("Script generator \'%s\' not supported for target: %s"), *ScriptGenerator->GetGeneratorName(), *GManifest.TargetName);
			ScriptPlugins.RemoveAt(PluginIndex);
		}
	}
}

/**
 * Tries to resolve super classes for classes defined in the given class
 */
void ResolveSuperClasses(const TCHAR* PackageName, FUnrealClassDefinitionInfo& ClassDef)
{

	// Resolve the base class
	{
		FUnrealStructDefinitionInfo::FBaseStructInfo& SuperClassInfo = ClassDef.GetSuperStructInfo();

		if (!SuperClassInfo.Name.IsEmpty())
		{
			const FString& BaseClassName = SuperClassInfo.Name;
			const FString& BaseClassNameStripped = GetClassNameWithPrefixRemoved(BaseClassName);

			FUnrealClassDefinitionInfo* FoundBaseClassDef = GTypeDefinitionInfoMap.FindByName<FUnrealClassDefinitionInfo>(*BaseClassNameStripped);
			if (FoundBaseClassDef == nullptr)
			{
				FoundBaseClassDef = GTypeDefinitionInfoMap.FindByName<FUnrealClassDefinitionInfo>(*BaseClassName);
			}

			if (FoundBaseClassDef == nullptr)
			{
				// Don't know its parent class. Raise error.
				ClassDef.Throwf(TEXT("Couldn't find parent type for '%s' named '%s' in current module (Package: %s) or any other module parsed so far."), *ClassDef.GetName(), *BaseClassName, PackageName);
			}

			SuperClassInfo.Struct = FoundBaseClassDef;
			ClassDef.SetClassCastFlags(FoundBaseClassDef->GetClassCastFlags());
		}
	}

	// Resolve the inherited classes
	{
		for (FUnrealStructDefinitionInfo::FBaseStructInfo& BaseClassInfo : ClassDef.GetBaseStructInfos())
		{
			BaseClassInfo.Struct = FUnrealClassDefinitionInfo::FindScriptClass(BaseClassInfo.Name);
		}
	}
}

/**
 * Tries to resolve super classes for classes defined in the given
 * module.
 *
 * @param Package Modules package.
 */
void ResolveSuperClasses(FUnrealPackageDefinitionInfo& PackageDef)
{
	FString PackageName = PackageDef.GetName();
	for (TSharedRef<FUnrealSourceFile> SourceFile : PackageDef.GetAllSourceFiles())
	{
		for (TSharedRef<FUnrealTypeDefinitionInfo> TypeDef : SourceFile->GetDefinedClasses())
		{
			ResolveSuperClasses(*PackageName, UHTCastChecked<FUnrealClassDefinitionInfo>(TypeDef));
		}
	}
}

UPackage* GetModulePackage(FManifestModule& Module)
{
	UPackage* Package = FEngineAPI::FindObjectFast<UPackage>(NULL, FName(*Module.LongPackageName), false);
	if (Package == NULL)
	{
		Package = CreatePackage(*Module.LongPackageName);
	}
	// Set some package flags for indicating that this package contains script
	// NOTE: We do this even if we didn't have to create the package, because CoreUObject is compiled into UnrealHeaderTool and we still
	//       want to make sure our flags get set
	Package->SetPackageFlags(PKG_ContainsScript | PKG_Compiling);
	Package->ClearPackageFlags(PKG_ClientOptional | PKG_ServerSideOnly);

	if (Module.OverrideModuleType == EPackageOverrideType::None)
	{
		switch (Module.ModuleType)
		{
		case EBuildModuleType::GameEditor:
		case EBuildModuleType::EngineEditor:
			Package->SetPackageFlags(PKG_EditorOnly);
			break;

		case EBuildModuleType::GameDeveloper:
		case EBuildModuleType::EngineDeveloper:
			Package->SetPackageFlags(PKG_Developer);
			break;

		case EBuildModuleType::GameUncooked:
		case EBuildModuleType::EngineUncooked:
			Package->SetPackageFlags(PKG_UncookedOnly);
			break;
		}
	}
	else
	{
		// If the user has specified this module to have another package flag, then OR it on
		switch (Module.OverrideModuleType)
		{
		case EPackageOverrideType::EditorOnly:
			Package->SetPackageFlags(PKG_EditorOnly);
			break;

		case EPackageOverrideType::EngineDeveloper:
		case EPackageOverrideType::GameDeveloper:
			Package->SetPackageFlags(PKG_Developer);
			break;

		case EPackageOverrideType::EngineUncookedOnly:
		case EPackageOverrideType::GameUncookedOnly:
			Package->SetPackageFlags(PKG_UncookedOnly);
			break;
		}
	}
	return Package;
}

void PrepareModules(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs, const FString& ModuleInfoPath)
{
	// Three passes.  1) Public 'Classes' headers (legacy)  2) Public headers   3) Private headers
	enum EHeaderFolderTypes
	{
		PublicClassesHeaders = 0,
		PublicHeaders,
		InternalHeaders,
		PrivateHeaders,

		FolderType_Count
	};

	for (int32 ModuleIndex = 0, NumModules = GManifest.Modules.Num(); ModuleIndex < NumModules; ++ModuleIndex)
	{
		FManifestModule& Module = GManifest.Modules[ModuleIndex];

		// Force regeneration of all subsequent modules, otherwise data will get corrupted.
		Module.ForceRegeneration();
		UPackage* Package = GetModulePackage(Module);

		// Create the package definition
		TSharedRef<FUnrealPackageDefinitionInfo> PackageDefRef = MakeShared<FUnrealPackageDefinitionInfo>(Module, Package);
		FUnrealPackageDefinitionInfo& PackageDef = *PackageDefRef;
		GTypeDefinitionInfoMap.AddNameLookup(PackageDef);
		PackageDefs.Add(&PackageDef);

		TArray<TSharedRef<FUnrealSourceFile>>& AllSourceFiles = PackageDef.GetAllSourceFiles();
		AllSourceFiles.Reserve(Module.PublicUObjectClassesHeaders.Num() + Module.PublicUObjectHeaders.Num() + Module.InternalUObjectHeaders.Num() + Module.PrivateUObjectHeaders.Num());

		// Initialize the other header data structures and create the unreal source files
		for (int32 PassIndex = 0; PassIndex < FolderType_Count && FResults::IsSucceeding(); ++PassIndex)
		{
			EHeaderFolderTypes CurrentlyProcessing = (EHeaderFolderTypes)PassIndex;

			const TArray<FString>& UObjectHeaders =
				(CurrentlyProcessing == PublicClassesHeaders) ? Module.PublicUObjectClassesHeaders :
				(CurrentlyProcessing == PublicHeaders) ? Module.PublicUObjectHeaders :
				(CurrentlyProcessing == InternalHeaders) ? Module.InternalUObjectHeaders :
				Module.PrivateUObjectHeaders;

			if (UObjectHeaders.Num() == 0)
			{
				continue;
			}

			// Create the unreal source file objects for each header
			for (int32 Index = 0, EIndex = UObjectHeaders.Num(); Index < EIndex; ++Index)
			{
				const FString& RawFilename = UObjectHeaders[Index];
				const FString FullFilename = FPaths::ConvertRelativePathToFull(ModuleInfoPath, RawFilename);

				FUnrealSourceFile* UnrealSourceFilePtr = new FUnrealSourceFile(PackageDef, RawFilename);
				TSharedRef<FUnrealSourceFile> UnrealSourceFile(UnrealSourceFilePtr);
				AllSourceFiles.Add(UnrealSourceFile);

				FString CleanFilename = FPaths::GetCleanFilename(RawFilename);
				uint32  CleanFilenameHash = GetTypeHash(CleanFilename);
				if (TSharedPtr<FUnrealSourceFile> ExistingSourceFile = GUnrealSourceFilesMap.AddByHash(CleanFilenameHash, MoveTemp(CleanFilename), UnrealSourceFile))
				{
					FString NormalizedFullFilename = FullFilename;
					FString NormalizedExistingFilename = ExistingSourceFile->GetFilename();

					FPaths::NormalizeFilename(NormalizedFullFilename);
					FPaths::NormalizeFilename(NormalizedExistingFilename);

					if (NormalizedFullFilename != NormalizedExistingFilename)
					{
						FUHTMessage(*UnrealSourceFile).LogError(TEXT("Duplicate leaf header name found: %s (original: %s)"), *NormalizedFullFilename, *NormalizedExistingFilename);
					}
				}

				if (CurrentlyProcessing == PublicClassesHeaders)
				{
					UnrealSourceFilePtr->MarkPublic();
				}

				// Save metadata for the class path, both for it's include path and relative to the module base directory
				if (FullFilename.StartsWith(Module.BaseDirectory))
				{
					// Get the path relative to the module directory
					const TCHAR* ModuleRelativePath = *FullFilename + Module.BaseDirectory.Len();

					UnrealSourceFilePtr->SetModuleRelativePath(ModuleRelativePath);

					// Calculate the include path
					const TCHAR* IncludePath = ModuleRelativePath;

					// Walk over the first potential slash
					if (*IncludePath == TEXT('/'))
					{
						IncludePath++;
					}

					// Does this module path start with a known include path location? If so, we can cut that part out of the include path
					static const auto& PublicFolderName = TEXT("Public/");
					static const auto& PrivateFolderName = TEXT("Private/");
					static const auto& ClassesFolderName = TEXT("Classes/");
					static const auto& InternalFolderName = TEXT("Internal/");
					if (FCString::Strnicmp(IncludePath, PublicFolderName, UE_ARRAY_COUNT(PublicFolderName) - 1) == 0)
					{
						IncludePath += (UE_ARRAY_COUNT(PublicFolderName) - 1);
					}
					else if (FCString::Strnicmp(IncludePath, PrivateFolderName, UE_ARRAY_COUNT(PrivateFolderName) - 1) == 0)
					{
						IncludePath += (UE_ARRAY_COUNT(PrivateFolderName) - 1);
					}
					else if (FCString::Strnicmp(IncludePath, ClassesFolderName, UE_ARRAY_COUNT(ClassesFolderName) - 1) == 0)
					{
						IncludePath += (UE_ARRAY_COUNT(ClassesFolderName) - 1);
					}
					else if (FCString::Strnicmp(IncludePath, InternalFolderName, UE_ARRAY_COUNT(InternalFolderName) - 1) == 0)
					{
						IncludePath += (UE_ARRAY_COUNT(InternalFolderName) - 1);
					}

					// Add the include path
					if (*IncludePath != 0)
					{
						UnrealSourceFilePtr->SetIncludePath(MoveTemp(IncludePath));
					}
				}
			}
		}
	}
	GUnrealSourceFilesMap.Freeze();
}

void LoadSource(FUnrealSourceFile& SourceFile, const FString& ModuleInfoPath)
{
	FResults::Try([&SourceFile, &ModuleInfoPath]()
		{
			FScopedDurationTimer SourceTimer(SourceFile.GetTime(ESourceFileTime::Load));

			const FString FullFilename = FPaths::ConvertRelativePathToFull(ModuleInfoPath, *SourceFile.GetFilename());

			FString Content;
			if (!FFileHelper::LoadFileToString(Content, *FullFilename))
			{
				FUHTMessage(SourceFile).Throwf(TEXT("UnrealHeaderTool was unable to load source file '%s'"), *FullFilename);
			}
			SourceFile.SetContent(MoveTemp(Content));
		}
	);
}

void PreparseSource(FUnrealSourceFile& SourceFile)
{
	FResults::Try([&SourceFile]()
		{
			FScopedDurationTimer SourceTimer(SourceFile.GetTime(ESourceFileTime::PreParse));

			// Parse the header to extract the information needed
			FUHTStringBuilder ClassHeaderTextStrippedOfCppText;
			FHeaderParser::SimplifiedClassParse(SourceFile, *SourceFile.GetContent(), /*out*/ ClassHeaderTextStrippedOfCppText);
			SourceFile.SetContent(MoveTemp(ClassHeaderTextStrippedOfCppText));
		}
	);
}

void PreparseSources(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs, const FString& ModuleInfoPath)
{
	if (bGoWide)
	{
		FGraphEventArray LoadTasks;
		LoadTasks.Reserve(1024); // Fairly arbitrary number
		for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
		{
			for (TSharedRef<FUnrealSourceFile>& SourceFile : PackageDef->GetAllSourceFiles())
			{

				// Phase #1: Load the file
				auto LoadLambda = [&SourceFile = *SourceFile, &ModuleInfoPath]()
				{
					LoadSource(SourceFile, ModuleInfoPath);
				};

				// Phase #2: Perform simplified class parse (can run concurrenrtly)
				auto PreProcessLambda = [&SourceFile = *SourceFile]()
				{
					PreparseSource(SourceFile);
				};

				FGraphEventRef LoadTask = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(LoadLambda), TStatId());
				FGraphEventRef PreProcessTask = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(PreProcessLambda), TStatId(), LoadTask);
				LoadTasks.Add(MoveTemp(PreProcessTask));
			}
		}

		// Wait for all the loading and preparsing to complete
		FTaskGraphInterface::Get().WaitUntilTasksComplete(LoadTasks);
	}
	else
	{
		for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
		{
			for (TSharedRef<FUnrealSourceFile>& SourceFile : PackageDef->GetAllSourceFiles())
			{
				LoadSource(*SourceFile, ModuleInfoPath);
				PreparseSource(*SourceFile);
			}
		}
	}

	FResults::WaitForErrorTasks();
}

void DefineTypes(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs)
{
	for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
	{
		const FManifestModule& Module = PackageDef->GetModule();

		for (TSharedRef<FUnrealSourceFile>& SourceFile : PackageDef->GetAllSourceFiles())
		{
			FResults::Try([PackageDef, &SourceFile = *SourceFile]()
			{
				TArray<TSharedRef<FUnrealTypeDefinitionInfo>>& AllClasses = PackageDef->GetAllClasses();
				UPackage* Package = PackageDef->GetPackage();

				for (TSharedRef<FUnrealTypeDefinitionInfo>& TypeDef : SourceFile.GetDefinedClasses())
				{
					FUnrealClassDefinitionInfo& ClassDef = TypeDef->AsClassChecked();
					ProcessParsedClass(ClassDef);
					GTypeDefinitionInfoMap.AddNameLookup(UHTCastChecked<FUnrealObjectDefinitionInfo>(TypeDef));
					AllClasses.Add(TypeDef);
				}

				for (TSharedRef<FUnrealTypeDefinitionInfo>& TypeDef : SourceFile.GetDefinedEnums())
				{
					FUnrealEnumDefinitionInfo& EnumDef = TypeDef->AsEnumChecked();
					ProcessParsedEnum(EnumDef);
					GTypeDefinitionInfoMap.AddNameLookup(UHTCastChecked<FUnrealObjectDefinitionInfo>(TypeDef));
				}

				for (TSharedRef<FUnrealTypeDefinitionInfo>& TypeDef : SourceFile.GetDefinedStructs())
				{
					FUnrealScriptStructDefinitionInfo& ScriptStructDef = TypeDef->AsScriptStructChecked();
					ProcessParsedStruct(ScriptStructDef);
					GTypeDefinitionInfoMap.AddNameLookup(UHTCastChecked<FUnrealObjectDefinitionInfo>(TypeDef));
				}

				static const bool bVerbose = FParse::Param(FCommandLine::Get(), TEXT("VERBOSE"));
				if (bVerbose)
				{
					for (FHeaderProvider& DependsOnElement : SourceFile.GetIncludes())
					{
						UE_LOG(LogCompile, Log, TEXT("\tAdding %s as a dependency"), *DependsOnElement.ToString());
					}
				}
			}
			);
		}
	}
	FResults::WaitForErrorTasks();
}

void ResolveParents(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs)
{
	GUObjectDef = &GTypeDefinitionInfoMap.FindByNameChecked<FUnrealClassDefinitionInfo>(*UObject::StaticClass()->GetFName().ToString());
	GUClassDef = &GTypeDefinitionInfoMap.FindByNameChecked<FUnrealClassDefinitionInfo>(*UClass::StaticClass()->GetFName().ToString());
	GUInterfaceDef = &GTypeDefinitionInfoMap.FindByNameChecked<FUnrealClassDefinitionInfo>(*UInterface::StaticClass()->GetFName().ToString());

	for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
	{
		FResults::Try([PackageDef]() { ResolveSuperClasses(*PackageDef); });
	}
	FResults::WaitForErrorTasks();
}

void PrepareTypesForParsing(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs)
{
	// Does nothing now
}

void TopologicalRecursion(FUnrealSourceFile& First, FUnrealSourceFile& Visit, TSet<FUnrealSourceFile*> SignaledFiles)
{
	check(Visit.GetTopologicalState() == ETopologicalState::Temporary);
	for (FHeaderProvider& Header : Visit.GetIncludes())
	{
		if (FUnrealSourceFile* Include = Header.Resolve(Visit))
		{
			if (Include->GetTopologicalState() == ETopologicalState::Temporary)
			{
				if (SignaledFiles.Contains(Include))
				{
					break;
				}
				SignaledFiles.Add(Include);
				UE_LOG(LogCompile, Error, TEXT("%s includes/requires %s"), *Visit.GetFilename(), *Include->GetFilename());
				if (&First != Include)
				{
					TopologicalRecursion(First, *Include, SignaledFiles);
				}
				break;
			}
		}
	}
}

FUnrealSourceFile* TopologicalVisit(TArray<FUnrealSourceFile*>& OrderedSourceFiles, FUnrealSourceFile& Visit)
{
	switch (Visit.GetTopologicalState())
	{
	case ETopologicalState::Unmarked:
		Visit.SetTopologicalState(ETopologicalState::Temporary);
		for (FHeaderProvider& Header : Visit.GetIncludes())
		{
			if (FUnrealSourceFile* Include = Header.Resolve(Visit))
			{
				if (FUnrealSourceFile* Recursion = TopologicalVisit(OrderedSourceFiles, *Include))
				{
					return Recursion;
				}
			}
		}
		Visit.SetTopologicalState(ETopologicalState::Permanent);
		OrderedSourceFiles.Add(&Visit);
		return nullptr;
	case ETopologicalState::Temporary:
		return &Visit;
	case ETopologicalState::Permanent:
		return nullptr;
	}
	return nullptr;
}

void TopologicalSort(TArray<FUnrealSourceFile*>& OrderedSourceFiles)
{
	const TArray<FUnrealSourceFile*>& UnorderedSourceFiles = GUnrealSourceFilesMap.GetAllSourceFiles();

	OrderedSourceFiles.Reset(UnorderedSourceFiles.Num());

	for (FUnrealSourceFile* SourceFile : UnorderedSourceFiles)
	{
		SourceFile->SetTopologicalState(ETopologicalState::Unmarked);
	}

	bool bCircularDependencyDetected = false;
	for (FUnrealSourceFile* SourceFile : UnorderedSourceFiles)
	{
		if (SourceFile->GetTopologicalState() == ETopologicalState::Unmarked)
		{
			if (FUnrealSourceFile* Recusion = TopologicalVisit(OrderedSourceFiles, *SourceFile))
			{
				UE_LOG(LogCompile, Error, TEXT("Circular dependency detected:"));
				TSet<FUnrealSourceFile*> SignaledFiles;
				TopologicalRecursion(*Recusion, *Recusion, SignaledFiles);
				FResults::SetResult(ECompilationResult::OtherCompilationError);
				bCircularDependencyDetected = true;
			}
		}
	}

	if (bCircularDependencyDetected)
	{
		return;
	}

	for (int32 Index = 0, EIndex = OrderedSourceFiles.Num(); Index != EIndex; ++Index)
	{
		OrderedSourceFiles[Index]->SetOrderedIndex(Index);
	}
	return;
}

void ParseSourceFiles(TArray<FUnrealSourceFile*>& OrderedSourceFiles)
{
	// Disable loading of objects outside of this package (or more exactly, objects which aren't UFields, CDO, or templates)
	TGuardValue<bool> AutoRestoreVerifyObjectRefsFlag(GVerifyObjectReferencesOnly, true);

	GSourcesToParse = (int)OrderedSourceFiles.Num();
	if (bGoWide)
	{
		/**
		 * For every FUnrealSourceFile being processed, an instance of this class represents the data associated with generating the new output.
		 */
		struct FParseCPP
		{
			FParseCPP(FUnrealPackageDefinitionInfo& InPackageDef, FUnrealSourceFile& InSourceFile)
				: PackageDef(InPackageDef)
				, SourceFile(InSourceFile)
			{}

			/**
			 * The package definition being exported
			 */
			FUnrealPackageDefinitionInfo& PackageDef;

			/**
			 * The source file being exported
			 */
			FUnrealSourceFile& SourceFile;

			/**
			 * This task represents the task that parses the source
			 */
			FGraphEventRef ParseTaskRef;
		};

		TArray<FParseCPP> ParsedCPPs;
		ParsedCPPs.Reserve(OrderedSourceFiles.Num());
		for (FUnrealSourceFile* SourceFile : OrderedSourceFiles)
		{
			ParsedCPPs.Emplace(SourceFile->GetPackageDef(), *SourceFile);
		}

		TSet<FUnrealSourceFile*> Includes;
		Includes.Reserve(ParsedCPPs.Num());
		FGraphEventArray TempTasks;
		TempTasks.Reserve(ParsedCPPs.Num());
		FGraphEventArray ParsedSourceTasks;
		ParsedSourceTasks.Reserve(ParsedCPPs.Num());
		for (FParseCPP& ParsedCPP : ParsedCPPs)
		{
			const FManifestModule& Module = ParsedCPP.PackageDef.GetModule();
			FUnrealSourceFile& SourceFile = ParsedCPP.SourceFile;

			FString ModuleRelativeFilename = SourceFile.GetFilename();
			ConvertToBuildIncludePath(Module, ModuleRelativeFilename);

			auto ParseSource = [&ParsedCPP]()
			{
				++GSourcesParsing;
				FResults::TryAlways([&ParsedCPP]()
					{
						FHeaderParser::Parse(ParsedCPP.PackageDef, ParsedCPP.SourceFile);
					});
				++GSourcesCompleted;
			};

			Includes.Reset();
			for (FHeaderProvider& Header : SourceFile.GetIncludes())
			{
				if (FUnrealSourceFile* Include = Header.Resolve(SourceFile))
				{
					Includes.Add(Include);
				}
			}

			// Our generation must wait on all of our includes generation to complete
			TempTasks.Reset();
			for (FUnrealSourceFile* Include : Includes)
			{
				FParseCPP& IncludeCPP = ParsedCPPs[Include->GetOrderedIndex()];
				TempTasks.Add(IncludeCPP.ParseTaskRef);

			}
			ParsedCPP.ParseTaskRef = FFunctionGraphTask::CreateAndDispatchWhenReady(MoveTemp(ParseSource), TStatId(), &TempTasks);
			ParsedSourceTasks.Add(ParsedCPP.ParseTaskRef);
		}

		// Wait for the results
		FTaskGraphInterface::Get().WaitUntilTasksComplete(ParsedSourceTasks);
		FResults::WaitForErrorTasks();
	}
	else
	{
		for (FUnrealSourceFile* SourceFile : OrderedSourceFiles)
		{
			FUnrealPackageDefinitionInfo& PackageDef = SourceFile->GetPackageDef();
			FScopedDurationTimer SourceTimer(SourceFile->GetTime(ESourceFileTime::Parse));
			++GSourcesParsing;
			FResults::TryAlways([&PackageDef, SourceFile]() { FHeaderParser::Parse(PackageDef, *SourceFile); });
			++GSourcesCompleted;
		}
	}
}

void PostParseFinalize(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs)
{
	auto PostParseFinalize = [&PackageDefs](EPostParseFinalizePhase Phase)
	{
		for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
		{
			FResults::Try([PackageDef, Phase]()
				{
					PackageDef->PostParseFinalize(Phase);
				});
		}
	};

	PostParseFinalize(EPostParseFinalizePhase::Phase1);
	PostParseFinalize(EPostParseFinalizePhase::Phase2);
	FResults::WaitForErrorTasks();
}

void CreateEngineTypes(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs)
{
	auto CreateEngineTypes = [&PackageDefs](ECreateEngineTypesPhase Phase)
	{
		for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
		{
			FResults::Try([PackageDef, Phase]()
				{
					PackageDef->CreateUObjectEngineTypes(Phase);
				});
		}

		for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
		{
			FResults::Try([PackageDef, Phase]()
				{
					PackageDef->CreateUObjectEngineTypes(Phase);
				});
		}
	};

	CreateEngineTypes(ECreateEngineTypesPhase::Phase1);
	CreateEngineTypes(ECreateEngineTypesPhase::Phase2);
	CreateEngineTypes(ECreateEngineTypesPhase::Phase3);
	FResults::WaitForErrorTasks();
}

void Export(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs, TArray<FUnrealSourceFile*>& OrderedSourceFiles)
{
	TArray<FGeneratedCPP> GeneratedCPPs;
	GeneratedCPPs.Reserve(OrderedSourceFiles.Num());
	for (FUnrealSourceFile* SourceFile : OrderedSourceFiles)
	{
		GeneratedCPPs.Emplace(SourceFile->GetPackageDef(), *SourceFile);
	}

	FResults::Try([&GeneratedCPPs]() { FNativeClassHeaderGenerator::GenerateSourceFiles(GeneratedCPPs); });

	for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
	{
		FResults::Try([&GeneratedCPPs, PackageDef]() { FNativeClassHeaderGenerator::Generate(*PackageDef, GeneratedCPPs); });
	}
	
	FResults::WaitForErrorTasks();
}

// Exports the class to all available plugins
void ExportClassToScriptPlugins(const TMap<UClass*, FUnrealSourceFile*>& SourceFileLookup, UClass* Class, const FManifestModule& Module, IScriptGeneratorPluginInterface& ScriptPlugin)
{
	check(SourceFileLookup.Find(Class) != nullptr);
	FUnrealSourceFile* const * SourceFile = SourceFileLookup.Find(Class);
	if (SourceFile == nullptr)
	{
		const FString Empty = TEXT("");
		ScriptPlugin.ExportClass(Class, Empty, Empty, false);
	}
	else
	{
		ScriptPlugin.ExportClass(Class, (*SourceFile)->GetFilename(), (*SourceFile)->GetGeneratedFilename(), (*SourceFile)->HasChanged());
	}
}

// Exports class tree to all available plugins
void ExportClassTreeToScriptPlugins(const TMap<UClass*, FUnrealSourceFile*>& SourceFileLookup, const FClassTree* Node, const FManifestModule& Module, IScriptGeneratorPluginInterface& ScriptPlugin)
{
	for (int32 ChildIndex = 0; ChildIndex < Node->NumChildren(); ++ChildIndex)
	{
		const FClassTree* ChildNode = Node->GetChild(ChildIndex);
		ExportClassToScriptPlugins(SourceFileLookup, ChildNode->GetClass(), Module, ScriptPlugin);
	}

	for (int32 ChildIndex = 0; ChildIndex < Node->NumChildren(); ++ChildIndex)
	{
		const FClassTree* ChildNode = Node->GetChild(ChildIndex);
		ExportClassTreeToScriptPlugins(SourceFileLookup, ChildNode, Module, ScriptPlugin);
	}
}

void ExportToScriptPlugins(TArray<IScriptGeneratorPluginInterface*>& ScriptPlugins, TArray<FUnrealPackageDefinitionInfo*>& PackageDefs, FString& ExternalDependencies)
{
	TMap<UClass*, FUnrealSourceFile*> SourceFileLookup;
	for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
	{
		for (TSharedRef<FUnrealTypeDefinitionInfo> TypeDef : PackageDef->GetAllClasses())
		{
			UClass* Class = UHTCastChecked<FUnrealClassDefinitionInfo>(TypeDef).GetClass();
			SourceFileLookup.Add(Class, &TypeDef->GetUnrealSourceFile());
		}
	}

	for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
	{
		const FManifestModule& Module = PackageDef->GetModule();

		FClassTree ClassTree(UObject::StaticClass());
		for (TSharedRef<FUnrealTypeDefinitionInfo> TypeDef : PackageDef->GetAllClasses())
		{
			ClassTree.AddClass(UHTCastChecked<FUnrealClassDefinitionInfo>(TypeDef).GetClass());
		}
		ClassTree.Validate();

		for (IScriptGeneratorPluginInterface* Plugin : ScriptPlugins)
		{
			if (Plugin->ShouldExportClassesForModule(Module.Name, Module.ModuleType, Module.GeneratedIncludeDirectory))
			{
				ExportClassToScriptPlugins(SourceFileLookup, ClassTree.GetClass(), Module, *Plugin);
				ExportClassTreeToScriptPlugins(SourceFileLookup, &ClassTree, Module, *Plugin);
			}
		}
	}

	for (IScriptGeneratorPluginInterface* ScriptGenerator : ScriptPlugins)
	{
		ScriptGenerator->FinishExport();
	}

	// Get a list of external dependencies from each enabled plugin
	for (IScriptGeneratorPluginInterface* ScriptPlugin : ScriptPlugins)
	{
		TArray<FString> PluginExternalDependencies;
		ScriptPlugin->GetExternalDependencies(PluginExternalDependencies);

		for (const FString& PluginExternalDependency : PluginExternalDependencies)
		{
			ExternalDependencies += PluginExternalDependency + LINE_TERMINATOR;
		}
	}
	return;
}

void WriteExternalDependencies(const FString& ExternalDependencies)
{
	FFileHelper::SaveStringToFile(ExternalDependencies, *GManifest.ExternalDependenciesFile);
}

void GenerateSummary(TArray<FUnrealPackageDefinitionInfo*>& PackageDefs)
{
	for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
	{
		const FManifestModule& Module = PackageDef->GetModule();

		double TotalTimes[int32(ESourceFileTime::Count)] = { 0.0 };
		int32 LinesParsed = 0;
		int32 StatementsParsed = 0;
		int32 SourceCount = 0;
		TArray<TSharedRef<FUnrealSourceFile>>& SourceFiles = PackageDef->GetAllSourceFiles();
		for (TSharedRef<FUnrealSourceFile>& SourceFile : SourceFiles)
		{
			for (int32 Index = 0; Index < int32(ESourceFileTime::Count); ++Index)
			{
				TotalTimes[int32(Index)] += SourceFile->GetTime(ESourceFileTime(Index));
			}
			LinesParsed += SourceFile->GetLinesParsed();
			StatementsParsed += SourceFile->GetStatementsParsed();
		}
		UE_LOG(LogCompile, Log, TEXT("Success: Module %s parsed %d sources(s), %d line(s), %d statement(s).  Times(secs) Load: %.3f, PreParse: %.3f, Parse: %.3f, Generate: %.3f."), *Module.Name,
			SourceFiles.Num(), LinesParsed, StatementsParsed, TotalTimes[int32(ESourceFileTime::Load)], TotalTimes[int32(ESourceFileTime::PreParse)], TotalTimes[int32(ESourceFileTime::Parse)], TotalTimes[int32(ESourceFileTime::Generate)]);
	}
}

ECompilationResult::Type UnrealHeaderTool_Main(const FString& ModuleInfoFilename)
{
	double MainTime = 0.0;
	FDurationTimer MainTimer(MainTime);
	MainTimer.Start();

	bIncludeDebugOutput = FParse::Param(FCommandLine::Get(), TEXT("IncludeDebugOutput"));
	bGoWide = !FParse::Param(FCommandLine::Get(), TEXT("NoGoWide"));

	check(GIsUCCMakeStandaloneHeaderGenerator);

	FString ModuleInfoPath = FPaths::GetPath(ModuleInfoFilename);

	// The meta data keywords must be initialized prior to going wide
	FBaseParser::InitMetadataKeywords();

	// Load the manifest file, giving a list of all modules to be processed, pre-sorted by dependency ordering
	FResults::Try([&ModuleInfoFilename]() { GManifest = FManifest::LoadFromFile(ModuleInfoFilename); });

	TArray<FUnrealSourceFile*> OrderedSourceFiles;
	TArray<FUnrealPackageDefinitionInfo*> PackageDefs;
	PackageDefs.Reserve(GManifest.Modules.Num());

	double TotalPrepareModuleTime = FResults::TimedTry([&PackageDefs, &ModuleInfoPath]() { PrepareModules(PackageDefs, ModuleInfoPath); });

	FString ExternalDependencies;
	TArray<IScriptGeneratorPluginInterface*> ScriptPlugins;

	double TotalPreparseTime = FResults::TimedTry([&PackageDefs, &ModuleInfoPath]() { PreparseSources(PackageDefs, ModuleInfoPath); });
	double TotalDefineTypesTime = FResults::TimedTry([&PackageDefs]() { DefineTypes(PackageDefs); });
	double TotalResolveParentsTime = FResults::TimedTry([&PackageDefs]() { ResolveParents(PackageDefs); });
	double TotalPrepareTypesForParsingTime = FResults::TimedTry([&PackageDefs]() { PrepareTypesForParsing(PackageDefs); });
	double TotalTopologicalSortTime = FResults::TimedTry([&OrderedSourceFiles]() { TopologicalSort(OrderedSourceFiles); });
	double TotalParseTime = FResults::TimedTry([&OrderedSourceFiles]() { ParseSourceFiles(OrderedSourceFiles); });
	double TotalPostParseFinalizeTime = FResults::TimedTry([&PackageDefs]() { PostParseFinalize(PackageDefs); });

	// Look for any core classes that don't have a definition
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		if (GTypeDefinitionInfoMap.FindByName(*ClassIt->GetFName().ToString()) == nullptr)
		{
			UE_LOG(LogCompile, Log, TEXT("The core class '%s' doesn't have a matching entry in NoExportTypes.h"), *ClassIt->GetFName().ToString());
		}
	}

	TotalTopologicalSortTime += FResults::TimedTry([&OrderedSourceFiles]() { TopologicalSort(OrderedSourceFiles); }); // Sort again to include new dependencies
	double TotalCodeGenTime = FResults::TimedTry([&PackageDefs, &OrderedSourceFiles]() { Export(PackageDefs, OrderedSourceFiles); });
	double TotalCheckForScriptPluginsTime = FResults::TimedTry([&ScriptPlugins]() { GetScriptPlugins(ScriptPlugins); });
	double TotalCreateEngineTypesTime = ScriptPlugins.IsEmpty() ? 0.0 : FResults::TimedTry([&PackageDefs]() { CreateEngineTypes(PackageDefs); });
	double TotalPluginTime = ScriptPlugins.IsEmpty() ? 0.0 : FResults::TimedTry([&ScriptPlugins, &PackageDefs, &ExternalDependencies]() { ExportToScriptPlugins(ScriptPlugins, PackageDefs, ExternalDependencies); });
	double TotalWriteExternalDependenciesTime = FResults::TimedTry([&ExternalDependencies]() { WriteExternalDependencies(ExternalDependencies); });
	double TotalSummaryTime = FResults::TimedTry([&PackageDefs]() { GenerateSummary(PackageDefs); });

	// Finish all async file tasks before stopping the clock
	FTaskGraphInterface::Get().WaitUntilTasksComplete(GAsyncFileTasks);
	GAsyncFileTasks.Reset();

	// TEMPORARY change to log all files when UHT fails
	if (FResults::GetOverallResults() != 0)
	{
		UE_LOG(LogCompile, Log, TEXT("Source file listing due to UHT detected errors:"));
		for (FUnrealPackageDefinitionInfo* PackageDef : PackageDefs)
		{
			const FManifestModule& Module = PackageDef->GetModule();

			UE_LOG(LogCompile, Log, TEXT("Package %s sources"), *Module.Name);
			TArray<TSharedRef<FUnrealSourceFile>>& SourceFiles = PackageDef->GetAllSourceFiles();
			for (TSharedRef<FUnrealSourceFile>& SourceFile : SourceFiles)
			{
				UE_LOG(LogCompile, Log, TEXT("---- %s"), *SourceFile->GetFilename());
			}
		}
	}

	double TotalShutdownTime = FResults::TimedTry([]() { GTypeDefinitionInfoMap.Reset(); GUnrealSourceFilesMap.Reset(); });

	MainTimer.Stop();

	// Count the number of sources
	int NumSources = 0;
	for (const FManifestModule& Module : GManifest.Modules)
	{
		NumSources += Module.PublicUObjectClassesHeaders.Num() + Module.PublicUObjectHeaders.Num() + Module.InternalUObjectHeaders.Num() + Module.PrivateUObjectHeaders.Num();
	}
	UE_LOG(LogCompile, Log, TEXT("Preparing %d modules took %.3f seconds"), GManifest.Modules.Num(), TotalPrepareModuleTime);
	UE_LOG(LogCompile, Log, TEXT("Preparsing %d sources took %.3f seconds"), NumSources, TotalPreparseTime);
	UE_LOG(LogCompile, Log, TEXT("Defining types took %.3f seconds"), TotalDefineTypesTime);
	UE_LOG(LogCompile, Log, TEXT("Resolving type parents took %.3f seconds"), TotalResolveParentsTime);
	UE_LOG(LogCompile, Log, TEXT("Preparing types for parsing took %.3f seconds"), TotalPrepareTypesForParsingTime);
	UE_LOG(LogCompile, Log, TEXT("Sorting files by dependencies took %.3f seconds"), TotalTopologicalSortTime);
	UE_LOG(LogCompile, Log, TEXT("Parsing took %.3f seconds"), TotalParseTime);
	UE_LOG(LogCompile, Log, TEXT("Post parse finalization took %.3f seconds"), TotalPostParseFinalizeTime);
	UE_LOG(LogCompile, Log, TEXT("Code generation took %.3f seconds"), TotalCodeGenTime);
	UE_LOG(LogCompile, Log, TEXT("Check for script plugins took %.3f seconds"), TotalCheckForScriptPluginsTime);
	UE_LOG(LogCompile, Log, TEXT("Create engine types took % .3f seconds"), TotalCreateEngineTypesTime);
	UE_LOG(LogCompile, Log, TEXT("ScriptPlugin overhead was %.3f seconds"), TotalPluginTime);
	UE_LOG(LogCompile, Log, TEXT("Write external dependencies overhead was %.3f seconds"), TotalWriteExternalDependenciesTime);
	UE_LOG(LogCompile, Log, TEXT("Summary generation took %.3f seconds"), TotalSummaryTime);
	UE_LOG(LogCompile, Log, TEXT("Macroize time was %.3f CPU seconds"), GMacroizeTime);
	UE_LOG(LogCompile, Log, TEXT("Freeing types was %.3f seconds"), TotalShutdownTime);

	FUnrealHeaderToolStats& Stats = FUnrealHeaderToolStats::Get();
	for (const TPair<FName, double>& Pair : Stats.Counters)
	{
		FString CounterName = Pair.Key.ToString();
		UE_LOG(LogCompile, Log, TEXT("%s timer was %.3f seconds"), *CounterName, Pair.Value);
	}

	UE_LOG(LogCompile, Log, TEXT("Total time was %.2f seconds"), MainTime);

	if (bWriteContents)
	{
		UE_LOG(LogCompile, Log, TEXT("********************************* Wrote reference generated code to ReferenceGeneratedCode."));
	}
	else if (bVerifyContents)
	{
		UE_LOG(LogCompile, Log, TEXT("********************************* Wrote generated code to VerifyGeneratedCode and compared to ReferenceGeneratedCode"));
		for (FString& Msg : ChangeMessages)
		{
			UE_LOG(LogCompile, Error, TEXT("%s"), *Msg);
		}
		TArray<FString> RefFileNames;
		IFileManager::Get().FindFiles( RefFileNames, *(FPaths::ProjectSavedDir() / TEXT("ReferenceGeneratedCode/*.*")), true, false );
		TArray<FString> VerFileNames;
		IFileManager::Get().FindFiles( VerFileNames, *(FPaths::ProjectSavedDir() / TEXT("VerifyGeneratedCode/*.*")), true, false );
		if (RefFileNames.Num() != VerFileNames.Num())
		{
			UE_LOG(LogCompile, Error, TEXT("Number of generated files mismatch ref=%d, ver=%d"), RefFileNames.Num(), VerFileNames.Num());
		}
		if (ChangeMessages.Num() > 0 || RefFileNames.Num() != VerFileNames.Num())
		{
			FResults::SetResult(ECompilationResult::OtherCompilationError);
		}
	}

	RequestEngineExit(TEXT("UnrealHeaderTool finished"));
	return FResults::GetOverallResults();
}

void ProcessParsedClass(FUnrealClassDefinitionInfo& ClassDef)
{
	UPackage* Package = ClassDef.GetPackageDef().GetPackage();
	const FString& ClassName = ClassDef.GetNameCPP();
	FString ClassNameStripped = GetClassNameWithPrefixRemoved(*ClassName);
	const FString& BaseClassName = ClassDef.GetSuperStructInfo().Name;

	// All classes must start with a valid unreal prefix
	if (!FHeaderParser::ClassNameHasValidPrefix(ClassName, ClassNameStripped))
	{
		ClassDef.Throwf(TEXT("Invalid class name '%s'. The class name must have an appropriate prefix added (A for Actors, U for other classes)."), *ClassName);
	}

	if(FHeaderParser::IsReservedTypeName(ClassNameStripped))
	{
		ClassDef.Throwf(TEXT("Invalid class name '%s'. Cannot use a reserved name ('%s')."), *ClassName, *ClassNameStripped);
	}

	// Ensure the base class has any valid prefix and exists as a valid class. Checking for the 'correct' prefix will occur during compilation
	FString BaseClassNameStripped;
	if (!BaseClassName.IsEmpty())
	{
		BaseClassNameStripped = GetClassNameWithPrefixRemoved(BaseClassName);
		if (!FHeaderParser::ClassNameHasValidPrefix(BaseClassName, BaseClassNameStripped))
		{
			ClassDef.Throwf(TEXT("No prefix or invalid identifier for base class %s.\nClass names must match Unreal prefix specifications (e.g., \"UObject\" or \"AActor\")"), *BaseClassName);
		}
	}

	//UE_LOG(LogCompile, Log, TEXT("Class: %s extends %s"),*ClassName,*BaseClassName);
	// Handle failure and non-class headers.
	if (BaseClassName.IsEmpty() && (ClassName != TEXT("UObject")))
	{
		ClassDef.Throwf(TEXT("Class '%s' must inherit UObject or a UObject-derived class"), *ClassName);
	}

	if (ClassName == BaseClassName)
	{
		ClassDef.Throwf(TEXT("Class '%s' cannot inherit from itself"), *ClassName);
	}

	// if we aren't generating headers, then we shouldn't set misaligned object, since it won't get cleared

	const static bool bVerboseOutput = FParse::Param(FCommandLine::Get(), TEXT("VERBOSE"));

	if (TSharedRef<FUnrealTypeDefinitionInfo>* Existing = GTypeDefinitionInfoMap.FindByName(*ClassNameStripped))
	{
		ClassDef.Throwf(TEXT("Duplicate class name: %s also exists in file %s"), *ClassName, *(*Existing)->GetFilename());
	}

	if (bVerboseOutput)
	{
		UE_LOG(LogCompile, Log, TEXT("Imported: %s"), *ClassDef.GetFullName());
	}
}

void ProcessParsedEnum(FUnrealEnumDefinitionInfo& EnumDef)
{
	UPackage* Package = EnumDef.GetPackageDef().GetPackage();
	const FString& EnumName = EnumDef.GetNameCPP();

	if (TSharedRef<FUnrealTypeDefinitionInfo>* Existing = GTypeDefinitionInfoMap.FindByName(*EnumName))
	{
		EnumDef.Throwf(TEXT("Duplicate enum name: %s also exists in file %s"), *EnumName, *(*Existing)->GetFilename());
	}

	// Check if the enum name is using a reserved keyword
	if (FHeaderParser::IsReservedTypeName(EnumName))
	{
		EnumDef.Throwf(TEXT("enum: '%s' uses a reserved type name."), *EnumName);
	}
}

void ProcessParsedStruct(FUnrealScriptStructDefinitionInfo& ScriptStructDef)
{
	const FString& StructName = ScriptStructDef.GetNameCPP();
	FString StructNameStripped = GetClassNameWithPrefixRemoved(*StructName);

	if (TSharedRef<FUnrealTypeDefinitionInfo>* Existing = GTypeDefinitionInfoMap.FindByName(*StructNameStripped))
	{
		ScriptStructDef.Throwf(TEXT("Duplicate struct name: %s also exists in file %s"), *StructNameStripped, *(*Existing)->GetFilename());
	}

	// Check if the enum name is using a reserved keyword
	if (FHeaderParser::IsReservedTypeName(StructNameStripped))
	{
		ScriptStructDef.Throwf(TEXT("struct: '%s' uses a reserved type name."), *StructNameStripped);
	}
}
