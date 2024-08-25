// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Stateless/NiagaraStatelessModule.h"
#include "Stateless/NiagaraStatelessEmitterDataBuildContext.h"
#include "Stateless/NiagaraStatelessModuleShaderParameters.h"
#include "Stateless/Modules/NiagaraStatelessModuleCommon.h"

#include "NiagaraStatelessModule_Drag.generated.h"

UCLASS(MinimalAPI, EditInlineNew, meta = (DisplayName = "Drag"))
class UNiagaraStatelessModule_Drag : public UNiagaraStatelessModule
{
	GENERATED_BODY()

	static constexpr float DefaultDrag = 1.0f;

public:
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayName = "Drag", DisableBindingDistribution))
	FNiagaraDistributionRangeFloat DragDistribution = FNiagaraDistributionRangeFloat(DefaultDrag);

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const override
	{
		if (!IsModuleEnabled())
		{
			return;
		}

		NiagaraStateless::FPhysicsBuildData& PhysicsBuildData = BuildContext.GetTransientBuildData<NiagaraStateless::FPhysicsBuildData>();
		PhysicsBuildData.DragRange = DragDistribution.CalculateRange(DefaultDrag);
	}

#if WITH_EDITOR
	virtual bool CanDisableModule() const override { return true; }
#endif
};
