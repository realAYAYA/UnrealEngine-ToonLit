// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "EditorReimportHandler.h"
#include "SVGFactory.h"
#include "SVGReimportFactory.generated.h"

UCLASS()
class USVGReimportFactory : public USVGFactory, public FReimportHandler
{
    GENERATED_BODY()

    // Begin FReimportHandler interface
    virtual bool CanReimport(UObject* Obj, TArray<FString>& OutFilenames) override;
    virtual void SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths) override;
    virtual EReimportResult::Type Reimport(UObject* Obj) override;
    // End FReimportHandler interface
};