// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/DMMaterialLinkedComponent.h"
#include "DMDefs.h"
#include "Misc/Guid.h"
#include "Templates/SharedPointer.h"

#if WITH_EDITOR
#include "Delegates/Delegate.h"
#include "Delegates/DelegateCombinations.h"
#endif

#include "DMTextureUV.generated.h"

class UDMMaterialParameter;
class UDMTextureUV;
class UDynamicMaterialModel;
class UMaterialInstanceDynamic;

#if WITH_EDITOR
class IDetailTreeNode;
class IPropertyHandle;
class IPropertyRowGenerator;
#endif

namespace UE::DynamicMaterial::ParamID
{
	// Individual parameters
	constexpr int32 PivotX = 0;
	constexpr int32 PivotY = 1;
	constexpr int32 ScaleX = 2;
	constexpr int32 ScaleY = 3;
	constexpr int32 Rotation = 4;
	constexpr int32 OffsetX = 5;
	constexpr int32 OffsetY = 6;

	// Parameter groups
	constexpr int32 Pivot = PivotX;
	constexpr int32 Scale = ScaleX;
	//constexpr int32 Rotation = 4; // No additional group value needed
	constexpr int32 Offset = OffsetX;
}

UCLASS(BlueprintType, ClassGroup = "Material Designer", meta = (DisplayName = "Material Designer Texture UV"))
class DYNAMICMATERIAL_API UDMTextureUV : public UDMMaterialLinkedComponent
{
	GENERATED_BODY()

	friend class SDMStageEdit;
	friend class UDMMaterialStageInputTextureUV;

public:
#if WITH_EDITOR
	static const FName NAME_UVSource;
	static const FName NAME_Offset;
	static const FName NAME_Pivot;
	static const FName NAME_Rotation;
	static const FName NAME_Scale;
	static const FName NAME_bMirrorOnX;
	static const FName NAME_bMirrorOnY;
#endif

	static const FString OffsetXPathToken;
	static const FString OffsetYPathToken;
	static const FString PivotXPathToken;
	static const FString PivotYPathToken;
	static const FString RotationPathToken;
	static const FString ScaleXPathToken;
	static const FString ScaleYPathToken;
	
	static const FGuid GUID;

#if WITH_EDITOR
	// The bool is whether or not the property should be exposed to Sequencer as keyable.
	static const TMap<FName, bool> TextureProperties;

	static UDMTextureUV* CreateTextureUV(UObject* InOuter);
#endif

	UDMTextureUV();

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Material Designer")
	bool bLinkScale = true;
#endif

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	UDynamicMaterialModel* GetMaterialModel() const;

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	EDMUVSource GetUVSource() const { return UVSource; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetUVSource(EDMUVSource InUVSource);
#endif

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetOffset() const { return Offset; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetOffset(const FVector2D& InOffset);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetPivot() const { return Pivot; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetPivot(const FVector2D& InPivot);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	float GetRotation() const { return Rotation; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetRotation(float InRotation);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	const FVector2D& GetScale() const { return Scale; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetScale(const FVector2D& InScale);

#if WITH_EDITOR
	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetMirrorOnX() const { return bMirrorOnX; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMirrorOnX(bool bInMirrorOnX);

	UFUNCTION(BlueprintPure, Category = "Material Designer")
	bool GetMirrorOnY() const { return bMirrorOnY; }

	UFUNCTION(BlueprintCallable, Category = "Material Designer")
	void SetMirrorOnY(bool bInMirrorOnY);

	TSharedPtr<IDetailTreeNode> GetDetailTreeNode(FName InProperty);
	TSharedPtr<IPropertyHandle> GetPropertyHandle(FName InProperty);
#endif

	TArray<UDMMaterialParameter*> GetParameters() const;

	void SetMIDParameters(UMaterialInstanceDynamic* InMID);

	//~ Begin UObject
#if WITH_EDITOR
	virtual bool Modify(bool bInAlwaysMarkDirty = true) override;
	virtual void PostLoad() override;
	virtual void PostEditImport() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PreEditUndo() override;
	virtual void PostEditUndo() override;
#endif
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject

#if WITH_EDITOR
	//~ Begin UDMMaterialComponent
	virtual void PostEditorDuplicate(UDynamicMaterialModel* InMaterialModel, UDMMaterialComponent* InParent) override;
	//~ End UDMMaterialComponent
#endif

protected:
#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetUVSource, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", ToolTip = "Forces a material rebuild.", NotKeyframeable))
	EDMUVSource UVSource = EDMUVSource::Texture;

	EDMUVSource UVSource_PreUndo = UVSource;
#endif

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetOffset, Setter = SetOffset, BlueprintSetter = SetOffset, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", Delta = 0.001))
	FVector2D Offset = FVector2D(0.f, 0.f);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetPivot, Setter = SetPivot, BlueprintSetter = SetPivot, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", ToolTip="Pivot for rotation and scale.", Delta = 0.001))
	FVector2D Pivot = FVector2D(0.5, 0.5);

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetRotation, Setter = SetRotation, BlueprintSetter = SetRotation, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", Delta = 1.0))
	float Rotation = 0.f;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Getter = GetScale, Setter = SetScale, BlueprintSetter = SetScale, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", AllowPreserveRatio, Delta = 0.001))
	FVector2D Scale = FVector2D(1.f, 1.f);

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetMirrorOnX, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", ToolTip = "Forces a material rebuild.", NotKeyframeable))
	bool bMirrorOnX = false;

	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, BlueprintSetter = SetMirrorOnY, Category = "Material Designer",
		meta = (AllowPrivateAccess = "true", ToolTip = "Forces a material rebuild.", NotKeyframeable))
	bool bMirrorOnY = false;

	bool bMirrorOnX_PreUndo = bMirrorOnX;
	bool bMirrorOnY_PreUndo = bMirrorOnY;
#endif

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, DuplicateTransient, TextExportTransient, Category = "Material Designer")
	TMap<int32, TObjectPtr<UDMMaterialParameter>> MaterialParameters;

#if WITH_EDITOR
	mutable TMap<int32, bool> MaterialNodesCreated;

	TSharedPtr<IPropertyRowGenerator> PropertyRowGenerator;
	TMap<FName, TSharedPtr<IDetailTreeNode>> DetailTreeNodes;
	TMap<FName, TSharedPtr<IPropertyHandle>> PropertyHandles;

	bool bNeedsPostLoadValueUpdate = false;
	bool bNeedsPostLoadStructureUpdate = false;

	void EnsureDetailObjects();

	void CreateParameterNames();
	void RemoveParameterNames();
#endif

	//~ Begin UDMMaterialComponent
	virtual void Update(EDMUpdateType InUpdateType) override;
	virtual UDMMaterialComponent* GetSubComponentByPath(FDMComponentPath& InPath, const FDMComponentPathSegment& InPathSegment) const override;
#if WITH_EDITOR
	virtual void GetComponentPathInternal(TArray<FString>& OutChildComponentPathComponents) const override;
	virtual void OnComponentAdded() override;
	virtual void OnComponentRemoved() override;
#endif
	//~ End UDMMaterialComponent
};
