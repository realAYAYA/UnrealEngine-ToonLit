// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Elements/Interfaces/TypedElementWorldInterface.h"

#include "CoreMinimal.h"
#include "Exporters/Exporter.h"
#include "Components/ActorComponent.h"

#include "ComponentElementEditorCopyAndPaste.generated.h"

UCLASS(BlueprintType)
class UNREALED_API UComponentElementsCopy : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadWrite, Category = "TypedElement|Component|Copy")
	TArray<TObjectPtr<UActorComponent>> ComponentsToCopy;
};

UCLASS()
class UNREALED_API UComponentElementsExporterT3D : public UExporter
{
public:
	GENERATED_BODY()

public:
	explicit UComponentElementsExporterT3D(const FObjectInitializer& ObjectInitializer = FObjectInitializer());

	//~ Begin UExporter Interface
	virtual bool ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Out, FFeedbackContext* Warn, uint32 PortFlags = 0) override;
	//~ End UExporter Interface
};

struct UNREALED_API FComponentElementEditorPasteImporter : public FWorldElementPasteImporter
{
public:
	virtual void Import(FContext& Context) override;
	
	virtual TArray<FTypedElementHandle> GetImportedElements() override;
private:
	TArray<TObjectPtr<UActorComponent>> ImportedComponents;
};