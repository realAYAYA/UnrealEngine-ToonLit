// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessSpawnInfo.h"
#include "NiagaraDataSet.h"
#include "NiagaraRendererProperties.h"
#include "NiagaraSystemEmitterState.h"
#include "Stateless/NiagaraStatelessEmitterTemplate.h"

#include "NiagaraStatelessEmitter.generated.h"

//-TODO:Stateless: Merge this into UNiagaraEmitterBase perhaps?

struct FNiagaraStatelessEmitterData;
class UNiagaraStatelessModule;
class UNiagaraRendererProperties;
namespace NiagaraStateless
{
	class FCommonShaderParameters;
}

using FNiagaraStatelessEmitterDataPtr = TSharedPtr<const FNiagaraStatelessEmitterData, ESPMode::ThreadSafe>;

/**
* Editor data for stateless emitters
* Generates runtime data to be consumed by the game
*/
UCLASS(MinimalAPI, EditInlineNew)
class UNiagaraStatelessEmitter : public UObject//, public INiagaraParameterDefinitionsSubscriber, public FNiagaraVersionedObject
{
	GENERATED_BODY()

public:
	//~Begin UObject Interface
	virtual void PostLoad() override;
	virtual bool NeedsLoadForTargetPlatform(const ITargetPlatform* TargetPlatform) const override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~End UObject Interface

#if WITH_EDITOR
	void OnEmitterTemplateChanged();
#endif
	NIAGARA_API const UNiagaraStatelessEmitterTemplate* GetEmitterTemplate() const;

	const TArray<UNiagaraRendererProperties*>& GetRenderers() { return RendererProperties; }
	const TArray<UNiagaraRendererProperties*>& GetRenderers() const { return RendererProperties; }

	void CacheFromCompiledData();
protected:
	void BuildCompiledDataSet();

public:
	template<typename TAction>
	void ForEachEnabledRenderer(TAction Func) const
	{
		for (UNiagaraRendererProperties* Renderer : RendererProperties)
		{
			if (Renderer && Renderer->GetIsEnabled() && Renderer->IsSimTargetSupported(ENiagaraSimTarget::GPUComputeSim))
			{
				Func(Renderer);
			}
		}
	}

	const FString& GetUniqueEmitterName() const { return UniqueEmitterName; }
	NIAGARA_API bool SetUniqueEmitterName(const FString& InName);

	FNiagaraStatelessEmitterDataPtr GetEmitterData() const { return StatelessEmitterData; }
	NiagaraStateless::FCommonShaderParameters* AllocateShaderParameters(const FNiagaraParameterStore& RendererBindings) const;

#if WITH_EDITOR
	NIAGARA_API void SetEmitterTemplateClass(UClass* TemplateClass);

	NIAGARA_API void AddRenderer(UNiagaraRendererProperties* Renderer, FGuid EmitterVersion);
	NIAGARA_API void RemoveRenderer(UNiagaraRendererProperties* Renderer, FGuid EmitterVersion);
	NIAGARA_API void MoveRenderer(UNiagaraRendererProperties* Renderer, int32 NewIndex, FGuid EmitterVersion);

	NIAGARA_API FSimpleMulticastDelegate& OnRenderersChanged() { return OnRenderersChangedDelegate; }

	NIAGARA_API FNiagaraStatelessSpawnInfo& AddSpawnInfo();

	NIAGARA_API void RemoveSpawnInfoBySourceId(FGuid& InSourceIdToRemove);

	NIAGARA_API int32 IndexOfSpawnInfoBySourceId(const FGuid& InSourceId);

	NIAGARA_API FNiagaraStatelessSpawnInfo* FindSpawnInfoBySourceId(const FGuid& InSourceId);

	NIAGARA_API int32 GetNumSpawnInfos() const { return SpawnInfos.Num(); }

	NIAGARA_API FNiagaraStatelessSpawnInfo* GetSpawnInfoByIndex(int32 Index);

	NIAGARA_API const TArray<TObjectPtr<UNiagaraStatelessModule>>& GetModules() const { return Modules; }

	UNiagaraStatelessEmitter* CreateAsDuplicate(FName InDuplicateName, UNiagaraSystem& InDuplicateOwnerSystem) const;

	NIAGARA_API void DrawModuleDebug(UWorld* World, const FTransform& LocalToWorld) const;
#endif //WITH_NIAGARA_DEBUGGER

protected:
	TSharedPtr<FNiagaraStatelessEmitterData, ESPMode::ThreadSafe> StatelessEmitterData;

	UPROPERTY()
	FString UniqueEmitterName;

	UPROPERTY(EditAnywhere, Category = "General", meta = (AllowedClasses = "/Script/Niagara.NiagaraStatelessEmitterTemplate", HideInStack))
	TObjectPtr<UClass> EmitterTemplateClass;

	UPROPERTY(EditAnywhere, Category = "General")
	bool bDeterministic = false;

	UPROPERTY(EditAnywhere, Category = "General")
	int32 RandomSeed = 0;

	UPROPERTY(EditAnywhere, Category = "General")
	FBox FixedBounds = FBox(FVector(-100), FVector(100));

	UPROPERTY(EditAnywhere, Category = "Emitter State")
	FNiagaraEmitterStateData EmitterState;

	UPROPERTY(EditAnywhere, Category = "Spawn Info", meta = (HideInStack))
	TArray<FNiagaraStatelessSpawnInfo> SpawnInfos;

	UPROPERTY(EditAnywhere, Category = "Modules", Instanced, meta = (HideInStack))
	TArray<TObjectPtr<UNiagaraStatelessModule>> Modules;

	UPROPERTY(EditAnywhere, Category = "Renderer", Instanced, meta = (HideInStack))
	TArray<TObjectPtr<UNiagaraRendererProperties>> RendererProperties;

	UPROPERTY(EditAnywhere, Category = "Scalability", meta = (DisplayInScalabilityContext))
	FNiagaraPlatformSet Platforms;

	UPROPERTY()
	FNiagaraDataSetCompiledData ParticleDataSetCompiledData;

	UPROPERTY()
	TArray<int32> ComponentOffsets;

#if WITH_EDITORONLY_DATA
	FSimpleMulticastDelegate OnRenderersChangedDelegate;
#endif
};
