// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "InstancedStruct.h"

#include "IHasContext.generated.h"

CHOOSER_API DECLARE_LOG_CATEGORY_EXTERN(LogChooser, Log, All);

UINTERFACE(MinimalAPI)
class UHasContextClass : public UInterface
{
	GENERATED_BODY()
};

DECLARE_MULTICAST_DELEGATE(FContextClassChanged)

class IHasContextClass
{
	GENERATED_BODY()
public:
	FContextClassChanged OnContextClassChanged;
	virtual TConstArrayView<FInstancedStruct> GetContextData() const { return TConstArrayView<FInstancedStruct>(); }

	virtual void Compile(bool bForce = false) {}
#if WITH_EDITOR
	virtual void AddCompileDependency(const UStruct* Struct) { }
#endif

};

DECLARE_MULTICAST_DELEGATE_OneParam(FChooserOutputObjectTypeChanged, const UClass* OutputObjectType);

UENUM()
enum class EObjectChooserResultType
{
	ObjectResult,
	ClassResult,
};
