// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraMergeable.h"
#include "NiagaraStatelessCommon.h"
#include "NiagaraStatelessDistribution.h"
#include "ShaderParameterStruct.h"

#include "NiagaraStatelessModule.generated.h"

class FNiagaraStatelessEmitterDataBuildContext;
#if WITH_EDITOR
struct FNiagaraStatelessDrawDebugContext;
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

class FNiagaraStatelessSetShaderParameterContext
{
public:
	UE_NONCOPYABLE(FNiagaraStatelessSetShaderParameterContext);

	explicit FNiagaraStatelessSetShaderParameterContext(TConstArrayView<uint8> InRendererParameterData, TConstArrayView<uint8> InBuiltData, const FShaderParametersMetadata* InShaderParametersMetadata, uint8* InShaderParameters)
		: RendererParameterData(InRendererParameterData)
		, BuiltData(InBuiltData)
		, ShaderParametersBase(InShaderParameters)
		, ParameterOffset(0)
		, ShaderParametersMetadata(InShaderParametersMetadata)
	{
	}

	template<typename T>
	T* GetParameterNestedStruct() const
	{
		const uint32 StructOffset = Align(ParameterOffset, TShaderParameterStructTypeInfo<T>::Alignment);
	#if DO_CHECK
		ValidateIncludeStructType(StructOffset, TShaderParameterStructTypeInfo<T>::GetStructMetadata());
	#endif
		ParameterOffset = StructOffset + TShaderParameterStructTypeInfo<T>::GetStructMetadata()->GetSize();
		return reinterpret_cast<T*>(ShaderParametersBase + StructOffset);
	}

	template<typename T>
	const T* ReadBuiltData() const
	{
		const int32 Align = BuiltDataOffset % alignof(T);
		const int32 Offset = BuiltDataOffset + Align;
		BuiltDataOffset = Offset + sizeof(T);
		check(BuiltDataOffset <= BuiltData.Num());
		return reinterpret_cast<const T*>(BuiltData.GetData() + Offset);
	}

	template<typename T>
	void GetRendererParameterValue(T& OutValue, int32 Offset, const T& DefaultValue) const
	{
		if (Offset != INDEX_NONE)
		{
			Offset *= sizeof(uint32);
			check(Offset >= 0 && Offset + sizeof(T) <= RendererParameterData.Num());
			FMemory::Memcpy(&OutValue, RendererParameterData.GetData() + Offset, sizeof(T));
		}
		else
		{
			OutValue = DefaultValue;
		}
	}

	void ConvertRangeToScaleBias(const FNiagaraStatelessRangeFloat& Range, float& OutScale, float& OutBias) const { OutScale = Range.GetScale(); GetRendererParameterValue(OutBias, Range.ParameterOffset, Range.Min); }
	void ConvertRangeToScaleBias(const FNiagaraStatelessRangeVector2& Range, FVector2f& OutScale, FVector2f& OutBias) const { OutScale = Range.GetScale(); GetRendererParameterValue(OutBias, Range.ParameterOffset, Range.Min); }
	void ConvertRangeToScaleBias(const FNiagaraStatelessRangeVector3& Range, FVector3f& OutScale, FVector3f& OutBias) const { OutScale = Range.GetScale(); GetRendererParameterValue(OutBias, Range.ParameterOffset, Range.Min); }
	void ConvertRangeToScaleBias(const FNiagaraStatelessRangeColor& Range, FLinearColor& OutScale, FLinearColor& OutBias) const { OutScale = Range.GetScale(); GetRendererParameterValue(OutBias, Range.ParameterOffset, Range.Min); }

protected:
#if DO_CHECK
	void ValidateIncludeStructType(uint32 StructOffset, const FShaderParametersMetadata* StructMetaData) const;
#endif

private:
	TConstArrayView<uint8>				RendererParameterData;
	TConstArrayView<uint8>				BuiltData;
	mutable int32						BuiltDataOffset = 0;
	uint8*								ShaderParametersBase = nullptr;
	mutable uint32						ParameterOffset = 0;
	const FShaderParametersMetadata*	ShaderParametersMetadata = nullptr;
};

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

UCLASS(MinimalAPI, abstract, EditInlineNew)
class UNiagaraStatelessModule : public UNiagaraMergeable
{
	GENERATED_BODY()

protected:
	UPROPERTY(EditAnywhere, Category = "Parameters", meta = (DisplayPriority = 0, HideInStack))
	uint32 bModuleEnabled : 1 = true;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Transient, Category = "Parameters", meta = (DisplayPriority = 0, HideInStack, StackItemHeaderIcon = "Icons.Visibility"))
	uint32 bDebugDrawEnabled : 1 = false;
#endif

public:
	bool IsModuleEnabled() const { return bModuleEnabled; }
#if WITH_EDITOR
	bool IsDebugDrawEnabled() const { return bDebugDrawEnabled; }

	struct PrivateMemberNames
	{
		static NIAGARA_API const FName bModuleEnabled;
		static NIAGARA_API const FName bDebugDrawEnabled;
	};

#endif

	virtual void BuildEmitterData(FNiagaraStatelessEmitterDataBuildContext& BuildContext) const {}
	virtual void SetShaderParameters(const FNiagaraStatelessSetShaderParameterContext& SetShaderParameterContext) const { }

#if WITH_EDITOR
	virtual bool CanDisableModule() const { return false; }
	void SetIsModuleEnabled(bool bInIsEnabled) { bModuleEnabled = bInIsEnabled; }

	virtual bool CanDebugDraw() const { return false; }
	virtual void DrawDebug(const FNiagaraStatelessDrawDebugContext& DrawDebugContext) const {}
#endif

#if WITH_EDITORONLY_DATA
	virtual void GetOutputVariables(TArray<FNiagaraVariableBase>& OutVariables) const {}
#endif

	//~UObject interface Begin
#if WITH_EDITOR
	virtual bool CanEditChange(const FProperty* InProperty) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
	//~UObject interface End
};
