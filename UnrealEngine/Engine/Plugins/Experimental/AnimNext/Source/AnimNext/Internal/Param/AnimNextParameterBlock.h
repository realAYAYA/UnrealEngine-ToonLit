// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AnimNextRigVMAsset.h"
#include "PropertyBag.h"
#include "Param/IParameterSource.h"
#include "AnimNextParameterBlock.generated.h"

class UEdGraph;
struct FAnimNextScheduleGraphTask;
struct FAnimNextScheduleParamScopeTask;
class UAnimNextParameterBlockParameter;

namespace UE::AnimNext
{
	struct FContext;
	struct FParameterBlockProxy;
}

namespace UE::AnimNext::UncookedOnly
{
	struct FUtils;
	struct FUtilsPrivate;
}

namespace UE::AnimNext::Editor
{
	class FParametersEditor;
	struct FUtils;
	class SRigVMAssetViewRow;
	class FParameterBlockParameterCustomization;
}

/** An asset used to define AnimNext parameters and their bindings */
UCLASS(MinimalAPI, BlueprintType)
class UAnimNextParameterBlock : public UAnimNextRigVMAsset
{
	GENERATED_BODY()

	UAnimNextParameterBlock(const FObjectInitializer& ObjectInitializer);

	friend class UAnimNextParameterBlockFactory;
	friend class UAnimNextParameterBlock_EditorData;
	friend struct UE::AnimNext::UncookedOnly::FUtils;
	friend struct UE::AnimNext::UncookedOnly::FUtilsPrivate;
	friend class UE::AnimNext::Editor::FParametersEditor;
	friend struct UE::AnimNext::Editor::FUtils;
	friend struct FAnimNode_AnimNextParameters;
	friend struct FAnimNextScheduleGraphTask;
	friend struct FAnimNextScheduleParamScopeEntryTask;
	friend class UE::AnimNext::Editor::SRigVMAssetViewRow;
	friend class UE::AnimNext::Editor::FParameterBlockParameterCustomization;
	friend struct UE::AnimNext::FParameterBlockProxy;
	friend class UAnimNextParameterBlockParameter;

	void UpdateLayer(UE::AnimNext::FParamStackLayerHandle& InHandle, float InDeltaTime) const;

	UPROPERTY()
	FInstancedPropertyBag PropertyBag;
};
