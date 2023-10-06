// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Templates/SharedPointer.h"

namespace mu { class Model; }



class FCustomizableObjectPrivateData
{
private:

	TSharedPtr<mu::Model, ESPMode::ThreadSafe> MutableModel;

public:

	void SetModel(const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& Model, const FGuid Identifier);
	const TSharedPtr<mu::Model, ESPMode::ThreadSafe>& GetModel();
	TSharedPtr<const mu::Model, ESPMode::ThreadSafe> GetModel() const;

	// See UCustomizableObjectSystem::LockObject. Must only be modified from the game thread
	bool bLocked = false;

#if WITH_EDITOR
	FGuid Identifier;

	bool bModelCompiledForCook = false;
	TArray<FString> CachedPlatformNames;
#endif

};

