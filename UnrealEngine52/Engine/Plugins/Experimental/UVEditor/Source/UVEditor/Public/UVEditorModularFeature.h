// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessingInterfaces/IUVEditorModularFeature.h"

namespace UE
{
namespace Geometry
{

/** 
 * Connector class that allows other plugins to look for the UV Editor to see if the plugin is present,
 * (via IModularFeatures::Get().GetModularFeature and related methods), and then launch it if so.
 */
class UVEDITOR_API FUVEditorModularFeature : public IUVEditorModularFeature
{
public:
	virtual void LaunchUVEditor(const TArray<TObjectPtr<UObject>>& Objects) override;
	virtual bool CanLaunchUVEditor(const TArray<TObjectPtr<UObject>>& Objects) override;

protected:
	void ConvertInputArgsToValidTargets(const TArray<TObjectPtr<UObject>>& ObjectsIn, TArray<TObjectPtr<UObject>>& ObjectsOut) const;

};


}
}
