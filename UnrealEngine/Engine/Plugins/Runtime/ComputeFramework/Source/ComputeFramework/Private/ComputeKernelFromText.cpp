// Copyright Epic Games, Inc. All Rights Reserved.

#include "ComputeFramework/ComputeKernelFromText.h"
#include "ComputeFramework/ComputeFramework.h"
#include "Internationalization/Regex.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_EDITOR

void UComputeKernelFromText::PostLoad()
{
	Super::PostLoad();
	ReparseKernelSourceText();
}

void UComputeKernelFromText::PreEditChange(FEditPropertyChain& PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	PrevSourceFile = SourceFile;
}

void UComputeKernelFromText::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	FProperty* ModifiedProperty = PropertyChangedEvent.PropertyChain.GetActiveMemberNode()->GetValue();
	if (!ModifiedProperty)
	{
		return;
	}

	FName ModifiedPropName = ModifiedProperty->GetFName();

	if (ModifiedPropName == GET_MEMBER_NAME_CHECKED(UComputeKernelFromText, SourceFile))
	{
		ReparseKernelSourceText();
	}
}

void UComputeKernelFromText::ReparseKernelSourceText()
{
	if (SourceFile.FilePath.IsEmpty())
	{
		EntryPoint = FString();
		GroupSize = FIntVector(1, 1, 1);
		KernelSourceText = FString();
		PermutationSet = FComputeKernelPermutationSet();
		DefinitionsSet = FComputeKernelDefinitionSet();

		return;
	}

	FString FullKernelPath = FPaths::ConvertRelativePathToFull(SourceFile.FilePath);

	IPlatformFile& PlatformFileSystem = IPlatformFile::GetPlatformPhysical();
	if (!PlatformFileSystem.FileExists(*FullKernelPath))
	{
		UE_LOG(LogComputeFramework, Error, TEXT("Unable to find kernel file \"%s\""), *FullKernelPath);

		SourceFile = PrevSourceFile;
		return;
	}

	if (!FFileHelper::LoadFileToString(KernelSourceText, &PlatformFileSystem, *FullKernelPath))
	{
		UE_LOG(LogComputeFramework, Error, TEXT("Unable to read kernel file \"%s\""), *FullKernelPath);

		SourceFile = PrevSourceFile;
		return;
	}

	{
		static const FRegexPattern KernelGroupSizePattern(TEXT(R"(KERNEL_GROUP_SIZE\(\s*([\d]+)\s*,\s*([\d]+)\s*,\s*([\d]+)\s*\))"));
		FRegexMatcher Matcher(KernelGroupSizePattern, KernelSourceText);
		if (Matcher.FindNext())
		{
			GroupSize.X = FCString::Atoi(*Matcher.GetCaptureGroup(1));
			GroupSize.Y = FCString::Atoi(*Matcher.GetCaptureGroup(2));
			GroupSize.Z = FCString::Atoi(*Matcher.GetCaptureGroup(3));
		}
	}

	{
		static const FRegexPattern KernelEntryPointPattern(TEXT(R"(KERNEL_ENTRY_POINT\(\s*([a-zA-Z_]\w*)\s*\))"));
		FRegexMatcher Matcher(KernelEntryPointPattern, KernelSourceText);
		if (Matcher.FindNext())
		{
			EntryPoint = Matcher.GetCaptureGroup(1);
		}
	}

	{
		static const FRegexPattern KernelPermutationBoolPattern(TEXT(R"(KERNEL_PERMUTATION_BOOL\(\s*([a-zA-Z_]\w*)\s*\))"));
		FComputeKernelPermutationSet NewPermutationSet;

		FRegexMatcher Matcher(KernelPermutationBoolPattern, KernelSourceText);
		while (Matcher.FindNext())
		{
			NewPermutationSet.BooleanOptions.Emplace(Matcher.GetCaptureGroup(1), true);
		}

		PermutationSet = MoveTemp(NewPermutationSet);
	}

	{
		static const FRegexPattern KernelDefinePattern(TEXT(R"(KERNEL_DEFINE\(\s*([a-zA-Z_][\w]*)\s*\))"));
		FComputeKernelDefinitionSet NewDefinitionsSet;

		FRegexMatcher Matcher(KernelDefinePattern, KernelSourceText);
		while (Matcher.FindNext())
		{
			NewDefinitionsSet.Defines.Emplace(Matcher.GetCaptureGroup(1));
		}

		DefinitionsSet = MoveTemp(NewDefinitionsSet);
	}

	{
		static const FRegexPattern KernelReadExternPattern(TEXT(R"(KERNEL_EXTERN_READ\(\s*([a-zA-Z_]\w*)((\s*,\s*[a-z0-9]+)+)\s*\))"));
		TArray<FShaderFunctionDefinition> ExternalReadFunctions;

		FRegexMatcher Matcher(KernelReadExternPattern, KernelSourceText);
		while (Matcher.FindNext())
		{
			FShaderFunctionDefinition FunctionDesc;
			FunctionDesc.Name = Matcher.GetCaptureGroup(1);
			FunctionDesc.bHasReturnType = true;

			FString AllParameters = Matcher.GetCaptureGroup(2);
			TArray<FString> ParamArray;
			AllParameters.ParseIntoArray(ParamArray, TEXT(","));
			
			for(const FString& ParamDecl: ParamArray)
			{
				FShaderParamTypeDefinition Param = {};

				Param.ValueType = FShaderValueType::FromString(ParamDecl);
				if (Param.ValueType.IsValid())
				{
					Param.ResetTypeDeclaration();
					FunctionDesc.ParamTypes.Emplace(MoveTemp(Param));
				}
			}

			if (ParamArray.Num() == FunctionDesc.ParamTypes.Num())
			{
				ExternalReadFunctions.Emplace(MoveTemp(FunctionDesc));
			}
		}

		ExternalInputs = MoveTemp(ExternalReadFunctions);
	}

	{
		static const FRegexPattern KernelWriteExternPattern(TEXT(R"(KERNEL_EXTERN_WRITE\(\s*([a-zA-Z_]\w*)((\s*,\s*[a-z0-9]+)+)\s*\))"));
		TArray<FShaderFunctionDefinition> ExternalWriteFunctions;

		FRegexMatcher Matcher(KernelWriteExternPattern, KernelSourceText);
		while (Matcher.FindNext())
		{
			FShaderFunctionDefinition FunctionDesc;
			FunctionDesc.Name = Matcher.GetCaptureGroup(1);
			FunctionDesc.bHasReturnType = false;

			FString AllParameters = Matcher.GetCaptureGroup(2);
			TArray<FString> ParamArray;

			for(const FString& ParamDecl: ParamArray)
			{
				FShaderParamTypeDefinition Param = {};

				Param.ValueType = FShaderValueType::FromString(ParamDecl);
				if (Param.ValueType.IsValid())
				{
					Param.ResetTypeDeclaration();
					FunctionDesc.ParamTypes.Emplace(MoveTemp(Param));
				}


				Param.ResetTypeDeclaration();
				FunctionDesc.ParamTypes.Emplace(MoveTemp(Param));
			}

			if (ParamArray.Num() == FunctionDesc.ParamTypes.Num())
			{
				ExternalWriteFunctions.Emplace(MoveTemp(FunctionDesc));
			}
		}

		ExternalOutputs = MoveTemp(ExternalWriteFunctions);
	}

	// todo[CF]: Notify graphs for recompilation
}

#endif
