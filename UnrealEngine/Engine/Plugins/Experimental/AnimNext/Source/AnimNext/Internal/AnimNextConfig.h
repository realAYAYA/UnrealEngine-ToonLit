// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Param/AnimNextObjectAccessorConfig.h"
#include "AnimNextConfig.generated.h"

namespace UE::AnimNext
{
	class FObjectProxyFactory;
}

UCLASS(Config=AnimNext)
class ANIMNEXT_API UAnimNextConfig : public UObject
{
	GENERATED_BODY()

private:
	// UObject interface
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	friend class UE::AnimNext::FObjectProxyFactory;

	/** The classes that are exposed to AnimNext systems */
	UPROPERTY(Config, EditAnywhere, Category = "Exposed Classes")
	TArray<FAnimNextObjectAccessorConfig> ExposedClasses;
};