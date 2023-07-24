// Copyright Epic Games, Inc. All Rights Reserved.

#include "InterchangeTranslatorBase.h"

#include "Algo/Find.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(InterchangeTranslatorBase)

bool UInterchangeTranslatorBase::CanImportSourceData(const UInterchangeSourceData* InSourceData) const
{
	const bool bIncludeDot = false;
	const FString Extension = FPaths::GetExtension(InSourceData->GetFilename(), bIncludeDot);

	const bool bExtensionMatches =
		Algo::FindByPredicate( GetSupportedFormats(),
		[ &Extension ]( const FString& Format )
		{
			return Format.StartsWith( Extension );
		}) != nullptr;

	return bExtensionMatches;
}
