// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithC4DImportOptions.h"

#include "DatasmithC4DTranslatorModule.h"

#include "UObject/UnrealType.h"

#define LOCTEXT_NAMESPACE "DatasmithC4DImportPlugin"

UDatasmithC4DImportOptions::UDatasmithC4DImportOptions(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bImportEmptyMesh = false;
	bOptimizeEmptySingleChildActors = false;
	bAlwaysGenerateNormals = false;
	ScaleVertices = 1.0;
#if WITH_EDITOR
	bExportToUDatasmith = false;

	// In debug show all properties including the ones for debug
	if (IDatasmithC4DTranslatorModule::Get().InDebugMode())
	{
		for (TFieldIterator<FProperty> It(GetClass()); It; ++It)
		{
			FProperty* Property = *It;
			if (Property && Property->HasMetaData(TEXT("Category")) && Property->GetMetaData(TEXT("Category")) == TEXT("DebugProperty"))
			{
				Property->SetMetaData(TEXT("Category"), TEXT("PrivateSettings"));
			}
		}
	}
#endif //WITH_EDITOR
}

#undef LOCTEXT_NAMESPACE
