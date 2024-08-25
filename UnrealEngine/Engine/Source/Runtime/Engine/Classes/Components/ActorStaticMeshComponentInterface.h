// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "Components/ComponentInterfaces.h"

class FActorStaticMeshComponentInterface : public IStaticMeshComponent
{
public:
#if WITH_EDITOR
	virtual void OnMeshRebuild(bool bRenderDataChanged) override;
	virtual void PostStaticMeshCompilation() override;
#endif
	virtual UStaticMesh* GetStaticMesh() const override;
	virtual IPrimitiveComponent* GetPrimitiveComponentInterface() override;
};
