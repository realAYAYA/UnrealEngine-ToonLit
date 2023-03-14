// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DataInterfaceContext.h"
#include "DataInterfaceTypes.h"
#include "IDataInterface.h"
#include "DataInterface_Object.generated.h"

UCLASS()
class CHOOSER_API UDataInterface_Object_Asset : public UObject, public IDataInterface
{
	GENERATED_BODY()
	DATA_INTERFACE_RETURN_TYPE(Object);
	
	// IDataInterface interface
	virtual bool GetDataImpl(const UE::DataInterface::FContext& Context) const final override;
	
public: 
	UPROPERTY(EditAnywhere, Category = "Parameters")
	TObjectPtr<UObject> Asset;
};
