// Copyright Epic Games, Inc. All Rights Reserved.

#include "Commandlets/DumpMaterialExpressionsCommandlet.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Containers/Array.h"
#include "Containers/Map.h"
#include "Containers/StringConv.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformCrt.h"
#include "Internationalization/Text.h"
#include "Logging/LogCategory.h"
#include "Logging/LogMacros.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionComposite.h"
#include "Materials/MaterialExpressionExecBegin.h"
#include "Materials/MaterialExpressionExecEnd.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialLayerOutput.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionPinBase.h"
#include "Materials/MaterialFunction.h"
#include "Math/UnrealMathSSE.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/Archive.h"
#include "Templates/Casts.h"
#include "Trace/Detail/Channel.h"
#include "UObject/Class.h"
#include "UObject/NameTypes.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogDumpMaterialExpressionsCommandlet, Log, All);

FString GetFormattedText(const FString& InText)
{
	FString OutText = InText;
	OutText = InText.Replace(TEXT("\n"), TEXT(" "));
	if (OutText.IsEmpty())
	{
		OutText = TEXT("N/A");
	}
	return OutText;
};

FString GenerateSpacePadding(int32 MaxLen, int32 TextLen)
{
	FString Padding;
	for (int i = 0; i < MaxLen - TextLen; ++i)
	{
		Padding += TEXT(" ");
	}
	return Padding;
};

void WriteLine(FArchive* FileWriter, const TArray<FString>& FieldNames, const TArray<uint32>& MaxFieldLengths)
{
	check(FieldNames.Num() >= MaxFieldLengths.Num());

	FString OutputLine;
	for (int32 i = 0; i < FieldNames.Num(); ++i)
	{
		// The last field doesn't need space padding, it changes to a new line.
		FString Padding = (i + 1 < FieldNames.Num()) ? GenerateSpacePadding(MaxFieldLengths[i], FieldNames[i].Len()) : TEXT("\n");
		OutputLine += (FieldNames[i] + Padding);
	}
	FileWriter->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());
};

void WriteOutMaterialExpressions(FArchive* FileWriter, const FString& OutputFilePath)
{
	FString OutputLine = TEXT("[MATERIAL EXPRESSIONS]\n");
	FileWriter->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());

	struct FMaterialExpressionInfo
	{
		UMaterialExpression* MaterialExpression;
		FString Name;
		FString Keywords;
		FString CreationName;
		FString CreationDescription;
		FString Caption;
		FString Description;
		FString Tooltip;
		FString	Type;
		FString ShowInCreateMenu;
	};
	TArray<FMaterialExpressionInfo> MaterialExpressionInfos;

	const FString NameField = TEXT("NAME");
	const FString TypeField = TEXT("TYPE");
	const FString ShowInCreateMenuField = TEXT("SHOW_IN_CREATE_MENU");
	const FString KeywordsField = TEXT("KEYWORDS");
	const FString CreationNameField = TEXT("CREATION_NAME");
	const FString CreationDescriptionField = TEXT("CREATION_DESCRIPTION");
	const FString CaptionField = TEXT("CAPTION");
	const FString DescriptionField = TEXT("DESCRIPTION");
	const FString TooltipField = TEXT("TOOLTIP");

	int32 MaxNameLength = NameField.Len();
	int32 MaxTypeLength = TypeField.Len();
	int32 MaxShowInCreateMenuLength = ShowInCreateMenuField.Len();
	int32 MaxKeywordsLength = KeywordsField.Len();
	int32 MaxCreationNameLength = CreationNameField.Len();
	int32 MaxCreationDescriptionLength = CreationDescriptionField.Len();
	int32 MaxCaptionLength = CaptionField.Len();
	int32 MaxDescriptionLength = DescriptionField.Len();
	int32 MaxTooltipLength = TooltipField.Len();

	// Collect all default material expression objects
	for (TObjectIterator<UClass> It; It; ++It)
	{
		UClass* Class = *It;
		// Skip the base UMaterialExpression class
		if (!Class->HasAnyClassFlags(CLASS_Abstract))
		{
			if (UMaterialExpression* DefaultExpression = Cast<UMaterialExpression>(Class->GetDefaultObject()))
			{
				const bool bClassDeprecated = Class->HasAnyClassFlags(CLASS_Deprecated);
				const bool bControlFlow = Class->HasMetaData("MaterialControlFlow");
				const bool bNewHLSLGenerator = Class->HasMetaData("MaterialNewHLSLGenerator");

				// If the expression is listed in the material node creation dropdown menu
				// See class exclusions in:
				//    MaterialExpressionClasses::InitMaterialExpressionClasses()
				//    FMaterialEditorUtilities::AddMaterialExpressionCategory() 
				//    MaterialExpressions.cpp IsAllowedExpressionType()
				bool bShowInCreateMenu = !(bClassDeprecated
					|| Class == UMaterialExpressionComment::StaticClass()
					|| Class == UMaterialExpressionMaterialLayerOutput::StaticClass()
					|| Class == UMaterialExpressionParameter::StaticClass()
					|| Class == UMaterialExpressionNamedRerouteUsage::StaticClass()
					|| Class == UMaterialExpressionExecBegin::StaticClass()
					|| Class == UMaterialExpressionExecEnd::StaticClass()
					|| Class == UMaterialExpressionPinBase::StaticClass()
					|| Class == UMaterialExpressionFunctionInput::StaticClass()
					|| Class == UMaterialExpressionFunctionOutput::StaticClass()
					|| Class == UMaterialExpressionComposite::StaticClass());

				FString ExpressionType;
				if (bControlFlow) { ExpressionType = TEXT("ControlFlow"); }
				if (!ExpressionType.IsEmpty() && bNewHLSLGenerator) { ExpressionType += TEXT("|"); }
				if (bNewHLSLGenerator) { ExpressionType += TEXT("HLSLGenerator"); }
				if (!ExpressionType.IsEmpty() && bClassDeprecated) { ExpressionType += TEXT("|"); }
				if (bClassDeprecated) { ExpressionType += TEXT("CLASS_Deprecated"); }

				TArray<FString> MultilineCaption;
				DefaultExpression->GetCaption(MultilineCaption);
				FString Caption;
				for (const FString& Line : MultilineCaption)
				{
					Caption += Line;
				}

				TArray<FString> MultilineToolTip;
				DefaultExpression->GetExpressionToolTip(MultilineToolTip);
				FString Tooltip;
				for (const FString& Line : MultilineToolTip)
				{
					Tooltip += Line;
				}

				FString DisplayName = Class->GetMetaData(TEXT("DisplayName"));
				FString CreationName = DefaultExpression->GetCreationName().ToString();

				FMaterialExpressionInfo ExpressionInfo;
				ExpressionInfo.MaterialExpression = DefaultExpression;
				ExpressionInfo.Name = Class->GetName().Mid(FCString::Strlen(TEXT("MaterialExpression")));
				ExpressionInfo.Keywords = DefaultExpression->GetKeywords().ToString();
				ExpressionInfo.CreationName = (CreationName.IsEmpty() ? (DisplayName.IsEmpty() ? ExpressionInfo.Name : DisplayName) : CreationName);
				ExpressionInfo.CreationDescription = DefaultExpression->GetCreationDescription().ToString();
				ExpressionInfo.Caption = Caption;
				ExpressionInfo.Description = DefaultExpression->GetDescription();
				ExpressionInfo.Tooltip = Tooltip;
				ExpressionInfo.Type = ExpressionType;
				ExpressionInfo.ShowInCreateMenu = bShowInCreateMenu ? TEXT("Yes") : TEXT("No");
				MaterialExpressionInfos.Add(ExpressionInfo);

				MaxNameLength = FMath::Max(MaxNameLength, ExpressionInfo.Name.Len());
				MaxTypeLength = FMath::Max(MaxTypeLength, ExpressionInfo.Type.Len());
				MaxShowInCreateMenuLength = FMath::Max(MaxShowInCreateMenuLength, ExpressionInfo.ShowInCreateMenu.Len());
				MaxKeywordsLength = FMath::Max(MaxKeywordsLength, ExpressionInfo.Keywords.Len());
				MaxCreationNameLength = FMath::Max(MaxCreationNameLength, ExpressionInfo.CreationName.Len());
				MaxCreationDescriptionLength = FMath::Max(MaxCreationDescriptionLength, ExpressionInfo.CreationDescription.Len());
				MaxCaptionLength = FMath::Max(MaxCaptionLength, ExpressionInfo.Caption.Len());
				MaxDescriptionLength = FMath::Max(MaxDescriptionLength, ExpressionInfo.Description.Len());
				MaxTooltipLength = FMath::Max(MaxTooltipLength, ExpressionInfo.Tooltip.Len());
			}
		}
	}

	// Additional padding for spacing
	const int32 AdditionalPadding = 3;
	MaxNameLength += AdditionalPadding;
	MaxTypeLength += AdditionalPadding;
	MaxShowInCreateMenuLength += AdditionalPadding;
	MaxKeywordsLength += AdditionalPadding;
	MaxCreationNameLength += AdditionalPadding;
	MaxCreationDescriptionLength += AdditionalPadding;
	MaxCaptionLength += AdditionalPadding;
	MaxDescriptionLength += AdditionalPadding;
	MaxTooltipLength += AdditionalPadding;

	TArray<uint32> MaxFieldLength;
	MaxFieldLength.Add(MaxNameLength);
	MaxFieldLength.Add(MaxTypeLength);
	MaxFieldLength.Add(MaxShowInCreateMenuLength);
	MaxFieldLength.Add(MaxKeywordsLength);
	MaxFieldLength.Add(MaxCreationNameLength);
	MaxFieldLength.Add(MaxCreationDescriptionLength);
	MaxFieldLength.Add(MaxCaptionLength);
	MaxFieldLength.Add(MaxDescriptionLength);
	MaxFieldLength.Add(MaxTooltipLength);

	// Write the material expression list to a text file
	TArray<FString> FieldNames;
	FieldNames.Add(NameField);
	FieldNames.Add(TypeField);
	FieldNames.Add(ShowInCreateMenuField);
	FieldNames.Add(KeywordsField);
	FieldNames.Add(CreationNameField);
	FieldNames.Add(CreationDescriptionField);
	FieldNames.Add(CaptionField);
	FieldNames.Add(DescriptionField);
	FieldNames.Add(TooltipField);
	WriteLine(FileWriter, FieldNames, MaxFieldLength);

	for (FMaterialExpressionInfo& ExpressionInfo : MaterialExpressionInfos)
	{
		FieldNames.Reset();
		FieldNames.Add(GetFormattedText(ExpressionInfo.Name));
		FieldNames.Add(GetFormattedText(ExpressionInfo.Type));
		FieldNames.Add(GetFormattedText(ExpressionInfo.ShowInCreateMenu));
		FieldNames.Add(GetFormattedText(ExpressionInfo.Keywords));
		FieldNames.Add(GetFormattedText(ExpressionInfo.CreationName));
		FieldNames.Add(GetFormattedText(ExpressionInfo.CreationDescription));
		FieldNames.Add(GetFormattedText(ExpressionInfo.Caption));
		FieldNames.Add(GetFormattedText(ExpressionInfo.Description));
		FieldNames.Add(GetFormattedText(ExpressionInfo.Tooltip));
		WriteLine(FileWriter, FieldNames, MaxFieldLength);
	}

	OutputLine = FString::Printf(TEXT("\nTotal %d material expressions found."), MaterialExpressionInfos.Num());
	FileWriter->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());

	UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("Total %d material expressions are written to %s"), MaterialExpressionInfos.Num(), *OutputFilePath);
}

void WriteOutMaterialFunctions(FArchive* FileWriter, const FString& OutputFilePath)
{
	FString OutputLine = TEXT("[MATERIAL FUNCTIONS]\n");
	FileWriter->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());

	struct FMaterialFunctionInfo
	{
		FString Name;
		FString Description;
		FString Path;
	};
	TArray<FMaterialFunctionInfo> MaterialFunctionInfos;

	const FString NameField = TEXT("NAME");
	const FString DescriptionField = TEXT("DESCRIPTION");
	const FString PathField = TEXT("PATH");

	int32 MaxNameLength = NameField.Len();
	int32 MaxDescriptionLength = DescriptionField.Len();
	int32 MaxPathLength = PathField.Len();

	// See UMaterialGraphSchema::GetMaterialFunctionActions for reference
	TArray<FAssetData> AssetDataList;
	FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	AssetRegistryModule.Get().GetAssetsByClass(UMaterialFunction::StaticClass()->GetClassPathName(), AssetDataList);
	for (const FAssetData& AssetData : AssetDataList)
	{
		// If this is a function that is selected to be exposed to the library
		if (AssetData.GetTagValueRef<bool>("bExposeToLibrary"))
		{
			const FString FunctionPathName = AssetData.GetObjectPathString();
			const FString Description = AssetData.GetTagValueRef<FText>("Description").ToString();

			FString FunctionName = FunctionPathName;
			int32 PeriodIndex = FunctionPathName.Find(TEXT("."), ESearchCase::CaseSensitive, ESearchDir::FromEnd);
			if (PeriodIndex != INDEX_NONE)
			{
				FunctionName = FunctionPathName.Right(FunctionPathName.Len() - PeriodIndex - 1);
			}

			FMaterialFunctionInfo FunctionInfo;
			FunctionInfo.Name = FunctionName;
			FunctionInfo.Description = Description;
			FunctionInfo.Path = FunctionPathName;
			MaterialFunctionInfos.Add(FunctionInfo);

			MaxNameLength = FMath::Max(MaxNameLength, FunctionInfo.Name.Len());
			MaxDescriptionLength = FMath::Max(MaxDescriptionLength, FunctionInfo.Description.Len());
			MaxPathLength = FMath::Max(MaxPathLength, FunctionInfo.Path.Len());
		}
	}

	// Additional padding for spacing
	const int32 AdditionalPadding = 3;
	MaxNameLength += AdditionalPadding;
	MaxDescriptionLength += AdditionalPadding;
	MaxPathLength += AdditionalPadding;

	TArray<uint32> MaxFieldLength;
	MaxFieldLength.Add(MaxNameLength);
	MaxFieldLength.Add(MaxDescriptionLength);
	MaxFieldLength.Add(MaxPathLength);

	// Write the material expression list to a text file
	TArray<FString> FieldNames;
	FieldNames.Add(NameField);
	FieldNames.Add(DescriptionField);
	FieldNames.Add(PathField);
	WriteLine(FileWriter, FieldNames, MaxFieldLength);

	for (FMaterialFunctionInfo& FunctionInfo : MaterialFunctionInfos)
	{
		FieldNames.Reset();
		FieldNames.Add(GetFormattedText(FunctionInfo.Name));
		FieldNames.Add(GetFormattedText(FunctionInfo.Description));
		FieldNames.Add(GetFormattedText(FunctionInfo.Path));
		WriteLine(FileWriter, FieldNames, MaxFieldLength);
	}
	
	OutputLine = FString::Printf(TEXT("\nTotal %d (bExposeToLibrary=true) material functions found."), MaterialFunctionInfos.Num());
	FileWriter->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());

	UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("Total %d material functions are written to %s"), MaterialFunctionInfos.Num(), *OutputFilePath);
}

UDumpMaterialExpressionsCommandlet::UDumpMaterialExpressionsCommandlet(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UDumpMaterialExpressionsCommandlet::Main(const FString& Params)
{
	TArray<FString> Tokens;
	TArray<FString> Switches;
	TMap<FString, FString> ParamVals;
	UCommandlet::ParseCommandLine(*Params, Tokens, Switches, ParamVals);

	if (Switches.Contains("help"))
	{
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("DumpMaterialExpressions"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("This commandlet will dump to a plain text file an info table of all material expressions in the engine and the plugins enabled on the project."));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("The output fields include:"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("Name - The class name of the material expression"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("Type - ControlFlow | HLSLGenerator | CLASS_Deprecated"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("ShowInCreateMenu - If the expression appears in the create node dropdown menu"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("CreationName - The name displayed in the create node dropdown menu to add an expression"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("CreationDescription - The tooltip displayed on the CreationName in the create node dropdown menu"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("Caption - The caption displayed on the material expression node"));
		UE_LOG(LogDumpMaterialExpressionsCommandlet, Log, TEXT("Tooltip - The tooltip displayed on the material expression node"));
		return 0;
	}

	const FString OutputFilePath = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("MaterialEditor"), TEXT("MaterialExpressions.txt"));
	FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*OutputFilePath);

	WriteOutMaterialExpressions(FileWriter, OutputFilePath);

	FString OutputLine = FString::Printf(TEXT("\n\n============================================================================\n\n"));
	FileWriter->Serialize(const_cast<ANSICHAR*>(StringCast<ANSICHAR>(*OutputLine).Get()), OutputLine.Len());

	WriteOutMaterialFunctions(FileWriter, OutputFilePath);

	FileWriter->Close();
	delete FileWriter;

	return 0;
}
