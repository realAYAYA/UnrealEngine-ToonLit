// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Render/Projection/IDisplayClusterProjectionPolicy.h"
#include "Misc/DisplayClusterObjectRef.h"

#include "DisplayClusterProjectionStrings.h"

class USceneComponent;


/**
 * Base projection policy
 */
class FDisplayClusterProjectionPolicyBase
	: public IDisplayClusterProjectionPolicy
{
public:
	FDisplayClusterProjectionPolicyBase(const FString& ProjectionPolicyId, const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy);

public:
	virtual const FString& GetId() const override
	{
		return PolicyInstanceId;
	}

	const USceneComponent* const GetOriginComp() const
	{
		return PolicyOriginComponentRef.GetOrFindSceneComponent();
	}

	USceneComponent* GetOriginComp()
	{
		return PolicyOriginComponentRef.GetOrFindSceneComponent();
	}

	void SetOriginComp(USceneComponent* OriginComp)
	{
		PolicyOriginComponentRef.SetSceneComponent(OriginComp);
	}

	virtual const TMap<FString, FString>& GetParameters() const override
	{
		return Parameters;
	}

	virtual bool IsConfigurationChanged(const FDisplayClusterConfigurationProjection* InConfigurationProjectionPolicy) const override;

	static bool IsEditorOperationMode(IDisplayClusterViewport* InViewport);
	static bool IsEditorOperationMode_RenderThread(const IDisplayClusterViewportProxy* InViewportProxy);

protected:
	void InitializeOriginComponent(IDisplayClusterViewport* InViewport, const FString& OriginCopmId);
	void ReleaseOriginComponent();

private:
	const FString PolicyInstanceId;

	FString PolicyOriginCompId;
	TMap<FString, FString> Parameters;
	FDisplayClusterSceneComponentRef PolicyOriginComponentRef;
};
