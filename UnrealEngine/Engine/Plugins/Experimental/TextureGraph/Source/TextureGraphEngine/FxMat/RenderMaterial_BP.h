// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "RenderMaterial.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"

class FxMaterial;
typedef std::shared_ptr< FxMaterial> FxMaterialPtr;

class TEXTUREGRAPHENGINE_API RenderMaterial_BP : public RenderMaterial
{
protected:
	UMaterial*						Material = nullptr;			/// The base material that is part 
	TStrongObjectPtr<UMaterialInstanceDynamic> 
									MaterialInstance;			/// An instance of the material
	CHashPtr						HashValue;					/// The hash for this material
	bool							RequestMaterialValidation = true;		/// 
	bool							MaterialInstanceValidated = false;		/// 

	FxMaterialPtr					FXMaterialObj;

	UCanvas*						Canvas = nullptr;

	void							DrawMaterial(UMaterialInterface* RenderMaterial, FVector2D ScreenPosition, FVector2D ScreenSize,
									FVector2D CoordinatePosition, FVector2D CoordinateSize=FVector2D::UnitVector, float Rotation=0.f,
									FVector2D PivotPoint=FVector2D(0.5f,0.5f)) const;
public:
									RenderMaterial_BP(FString Name, UMaterial* InMaterial, UMaterialInstanceDynamic* InMaterialInstance = nullptr);

	virtual							~RenderMaterial_BP() override;

	static bool						ValidateMaterialCompatible(UMaterialInterface* InMaterial);


	virtual AsyncPrepareResult		PrepareResources(const TransformArgs& Args) override;

	//////////////////////////////////////////////////////////////////////////
	/// BlobTransform Implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void					Bind(int32 Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(float Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(const FLinearColor& Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(const FIntVector4& Value, const ResourceBindInfo& BindInfo) override;
	virtual void					Bind(const FMatrix& Value, const ResourceBindInfo& BindInfo) override;
	virtual void					BindStruct(const char* ValueAddress, size_t StructSize, const ResourceBindInfo& BindInfo) override;
	virtual CHashPtr				Hash() const override;
	virtual std::shared_ptr<BlobTransform> DuplicateInstance(FString InName) override;

	//////////////////////////////////////////////////////////////////////////
	/// RenderMaterial Implementation
	//////////////////////////////////////////////////////////////////////////
	virtual void					BlitTo(FRHICommandListImmediate& RHI, UTextureRenderTarget2D* DstRT, const RenderMesh* MeshObj, int32 TargetId) const override;
	virtual void					SetTexture(FName InName, const UTexture* Texture) const override;
	virtual void					SetArrayTexture(FName InName, const std::vector<const UTexture*>& Textures) const override;
	virtual void					SetInt(FName InName, int32 Value) const override;
	virtual void					SetFloat(FName InName, float Value) const override;
	virtual void					SetColor(FName InName, const FLinearColor& Value) const override;
	virtual void					SetIntVector4(FName InName, const FIntVector4& Value) const override;
	virtual void					SetMatrix(FName InName, const FMatrix& Value) const override;
	
	//////////////////////////////////////////////////////////////////////////
	/// Inline functions
	////////////////////////////////////////////////////////////////////////// 
	FORCEINLINE UMaterial*			GetMaterial() { return Material; }

	FORCEINLINE UMaterialInstanceDynamic* Instance() { return MaterialInstance.Get(); }
	FORCEINLINE const UMaterialInstanceDynamic* Instance() const { return MaterialInstance.Get(); }
};

typedef std::shared_ptr<RenderMaterial_BP> RenderMaterial_BPPtr;