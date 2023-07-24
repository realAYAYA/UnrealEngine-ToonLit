// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "UObject/Interface.h"

#if WITH_EDITOR
#include "DetailLayoutBuilder.h"
#endif

#include "DeformableInterface.generated.h"


UINTERFACE()
class ENGINE_API UDeformableInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class ENGINE_API IDeformableInterface
{
	GENERATED_IINTERFACE_BODY()

public:


#if WITH_EDITOR
	// Take damage
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) = 0;
#endif

};
