// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"
#include "NNETypes.h"
#include "UObject/Class.h"

#include "NNERuntimeIREEMetaData.generated.h"

namespace UE::NNERuntimeIREE
{
	struct NNERUNTIMEIREE_API FFunctionMetaData
	{
		FString Name;
		TArray<UE::NNE::FTensorDesc> InputDescs;
		TArray<UE::NNE::FTensorDesc> OutputDescs;
	};
} // UE::NNERuntimeIREE


UCLASS()
class NNERUNTIMEIREE_API UNNERuntimeIREEModuleMetaData : public UObject
{
	GENERATED_BODY()

public:
	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	// End of UObject interface

	bool ParseFromString(const FString& ModuleString);

	TArray<UE::NNERuntimeIREE::FFunctionMetaData> FunctionMetaData;
};
