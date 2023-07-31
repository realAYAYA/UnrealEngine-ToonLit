// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GeometryProcessingInterfaces/UVEditorAssetEditor.h"

namespace UE
{
namespace Geometry
{

/** * Implementation of IGeometryProcessing_UVEditorAssetEditor
 */
class UVEDITOR_API FUVEditorAssetEditorImpl : public IGeometryProcessing_UVEditorAssetEditor
{
public:
	virtual void LaunchUVEditor(const TArray<TObjectPtr<UObject>>& Objects) override;
	virtual bool CanLaunchUVEditor(const TArray<TObjectPtr<UObject>>& Objects) override;

protected:
	void ConvertInputArgsToValidTargets(const TArray<TObjectPtr<UObject>>& ObjectsIn, TArray<TObjectPtr<UObject>>& ObjectsOut) const;

};


}
}
