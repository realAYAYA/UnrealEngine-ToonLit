// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Components/SceneComponent.h"
#include "CoreMinimal.h"
#include "BevelType.h"
#include "Mesh.h"
#include "UObject/ObjectMacros.h"

#include "Text3DComponent.generated.h"

class FTextLayout;
class ITextLayoutMarshaller;
class UFont;
class UMaterialInterface;
struct FTypefaceEntry;

UENUM()
enum class EText3DVerticalTextAlignment : uint8
{
	FirstLine		UMETA(DisplayName = "First Line"),
	Top				UMETA(DisplayName = "Top"),
	Center			UMETA(DisplayName = "Center"),
	Bottom			UMETA(DisplayName = "Bottom"),
};

UENUM()
enum class EText3DHorizontalTextAlignment : uint8
{
	Left			UMETA(DisplayName = "Left"),
	Center			UMETA(DisplayName = "Center"),
	Right			UMETA(DisplayName = "Right"),
};

UENUM()
enum class EText3DModifyFlags : uint8
{
	None = 0,
	Layout = 1 << 0,
	Geometry = 1 << 1,
	Unfreeze = 1 << 2,

	All = Layout | Geometry | Unfreeze
};
ENUM_CLASS_FLAGS(EText3DModifyFlags)

UCLASS(ClassGroup = (Text3D), PrioritizeCategories = "Text Layout Geometry Materials", meta = (BlueprintSpawnableComponent))
class TEXT3D_API UText3DComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UText3DComponent();

	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void OnRegister() override;
	virtual void OnUnregister() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostTransacted(const FTransactionObjectEvent& TransactionEvent) override;
#endif

	/**
	 * Delegate called after text is rebuilt
	 */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE(FTextGenerated);

	/**
	 * Delegate called after text is rebuilt
	 */
	DECLARE_MULTICAST_DELEGATE(FTextGeneratedNative);
	FTextGeneratedNative& OnTextGenerated() { return TextGeneratedNativeDelegate; }

	/** Get whether to allow automatic refresh/mesh generation */
	bool RefreshesOnChange() const;

	/** Set whether to allow automatic refresh/mesh generation */
	void SetRefreshOnChange(const bool Value);

	/** Get the text value and signal the primitives to be rebuilt */
	const FText& GetText() const;

	/** Set the text value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetText(const FText& Value);

	/** Get the text font and signal the primitives to be rebuilt */
	const UFont* GetFont() const;

	/** Set the text font and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetFont(UFont* InFont);

	/** Get whether an outline is applied. */
	bool HasOutline() const;

	/** Set whether an outline is applied. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetHasOutline(const bool bValue);

	/** Get the outline width. */
	float GetOutlineExpand() const;

	/** Set the outline width. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetOutlineExpand(const float Value);

	/** Get the text extrusion size and signal the primitives to be rebuilt */
	float GetExtrude() const;

	/** Set the text extrusion size and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetExtrude(const float Value);

	/** Get the 3d bevel value */
	float GetBevel() const;

	/** Set the 3d bevel value */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetBevel(const float Value);

	/** Get the 3d bevel type */
	EText3DBevelType GetBevelType() const;

	/** Set the 3d bevel type */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetBevelType(const EText3DBevelType Value);

	/** Get the amount of segments that will be used to tessellate the Bevel */
	int32 GetBevelSegments() const;

	/** Set the amount of segments that will be used to tessellate the Bevel */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetBevelSegments(const int32 Value);

	/** Get the text front material */
	UMaterialInterface* GetFrontMaterial() const;

	/** Set the text front material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetFrontMaterial(UMaterialInterface* Value);

	/** Get the text bevel material */
	UMaterialInterface* GetBevelMaterial() const;

	/** Set the text bevel material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetBevelMaterial(UMaterialInterface* Value);

	/** Get the text extrude material */
	UMaterialInterface* GetExtrudeMaterial() const;

	/** Set the text extrude material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetExtrudeMaterial(UMaterialInterface* Value);

	/** Get the text back material */
	UMaterialInterface* GetBackMaterial() const;

	/** Set the text back material */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetBackMaterial(UMaterialInterface* Value);

	/** Get the kerning value and signal the primitives to be rebuilt */
	float GetKerning() const;

	/** Set the kerning value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetKerning(const float Value);

	/** Get the line spacing value and signal the primitives to be rebuilt */
	float GetLineSpacing() const;

	/** Set the line spacing value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetLineSpacing(const float Value);

	/** Get the word spacing value and signal the primitives to be rebuilt */
	float GetWordSpacing() const;

	/** Set the word spacing value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetWordSpacing(const float Value);

	/** Get the horizontal alignment value and signal the primitives to be rebuilt */
	EText3DHorizontalTextAlignment GetHorizontalAlignment() const;

	/** Set the horizontal alignment value and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetHorizontalAlignment(const EText3DHorizontalTextAlignment value);

	/** Get the vertical alignment and signal the primitives to be rebuilt */
	EText3DVerticalTextAlignment GetVerticalAlignment() const;

	/** Set the vertical alignment and signal the primitives to be rebuilt */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetVerticalAlignment(const EText3DVerticalTextAlignment value);

	/** Whether a maximum width is specified */
	bool HasMaxWidth() const;

	/** Enable / Disable a Maximum Width */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetHasMaxWidth(const bool Value);

	/** Get the Maximum Width - If width is larger, mesh will scale down to fit MaxWidth value */
	float GetMaxWidth() const;

	/** Set the Maximum Width - If width is larger, mesh will scale down to fit MaxWidth value */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetMaxWidth(const float Value);

	/** Whether a maximum height is specified */
	bool HasMaxHeight() const;

	/** Enable / Disable a Maximum Height */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetHasMaxHeight(const bool Value);

	/** Get the Maximum Height - If height is larger, mesh will scale down to fit MaxHeight value */
	float GetMaxHeight() const;

	/** Set the Maximum Height - If height is larger, mesh will scale down to fit MaxHeight value */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetMaxHeight(const float Value);

	/** Get if the mesh should scale proportionally when Max Width/Height is set */
	bool ScalesProportionally() const;

	/** Set if the mesh should scale proportionally when Max Width/Height is set */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetScaleProportionally(const bool Value);

	/** Freeze mesh rebuild, to avoid unnecessary mesh rebuilds when setting a few properties together */
	bool IsFrozen() const;

	/** Freeze mesh rebuild, to avoid unnecessary mesh rebuilds when setting a few properties together */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetFreeze(const bool bFreeze);

	/** Get the value of CastShadow. */
	bool CastsShadow() const;

	/** Set the value of CastShadow. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D", meta = (DeprecatedFunction, DeprecationMessage = "Set the property directly"))
	void SetCastShadow(bool NewCastShadow);

	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	void GetBounds(FVector& Origin, FVector& BoxExtent);

	/** Gets the number of font glyphs that are currently used */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	int32 GetGlyphCount();

	/** Gets the USceneComponent that a glyph is attached to */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	USceneComponent* GetGlyphKerningComponent(int32 Index);

	/** Gets all the glyph kerning components */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	const TArray<USceneComponent*>& GetGlyphKerningComponents();

	/** Gets the StaticMeshComponent of a glyph */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	UStaticMeshComponent* GetGlyphMeshComponent(int32 Index);

	/** Gets all the glyph meshes */
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	const TArray<UStaticMeshComponent*>& GetGlyphMeshComponents();

	/** Gets the scale of actual text geometry, taking into account MaxWidth and MaxHeight constraints. This function will NOT return the component scale*/
	UFUNCTION(BlueprintCallable, Category = "Rendering|Components|Text3D")
	FVector GetTextScale();


	/** Get the typeface */
	FName GetTypeface() const { return  Typeface; }

	/** Set the typeface */
	void SetTypeface(const FName InTypeface);

	/** Manually update the geometry, ignoring RefreshOnChange (but still accounting for the Freeze flag) */
	void Rebuild();

protected:
	/** Whether to allow automatic refresh/mesh generation */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "RefreshesOnChange", Setter = "SetRefreshOnChange", Category = "Text3D", AdvancedDisplay, meta = (AllowPrivateAccess = "true"))
	bool bRefreshOnChange = true;

	/** The text to generate a 3d mesh */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Text", meta = (MultiLine = true, AllowPrivateAccess = "true"))
	FText Text;

	/** Size of the extrude */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Geometry", meta = (ClampMin = 0, AllowPrivateAccess = "true"))
	float Extrude;

	/** Size of bevel */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Geometry", meta = (EditCondition = "!bOutline", ClampMin = 0, AllowPrivateAccess = "true"))
	float Bevel;

	/** Bevel Type */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Geometry", meta = (EditCondition = "!bOutline", AllowPrivateAccess = "true"))
	EText3DBevelType BevelType;

	/** Bevel Segments (Defines the amount of tesselation for the bevel part) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Geometry", meta = (EditCondition = "!bOutline", ClampMin = 1, ClampMax = 15, AllowPrivateAccess = "true"))
	int32 BevelSegments;

	/** Generate Outline */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "HasOutline", Setter = "SetHasOutline", Category = "Geometry", meta = (AllowPrivateAccess = "true"))
	bool bOutline;

	/** Outline expand/offset amount */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Geometry", meta = (EditCondition = "bOutline", AllowPrivateAccess = "true"))
	float OutlineExpand;

	/** Material for the front part */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Materials", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> FrontMaterial;

	/** Material for the bevel part */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Materials", meta = (EditCondition = "!bOutline", AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> BevelMaterial;

	/** Material for the extruded part */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Materials", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> ExtrudeMaterial;

	/** Material for the back part */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Materials", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UMaterialInterface> BackMaterial;

	/** Text font */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Text", meta = (AllowPrivateAccess = "true"))
	TObjectPtr<UFont> Font;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Text", meta = (AllowPrivateAccess = "true", GetOptions="GetTypefaceNames"))
	FName Typeface;

	/** Horizontal text alignment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (AllowPrivateAccess = "true"))
	EText3DHorizontalTextAlignment HorizontalAlignment;

	/** Vertical text alignment */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (AllowPrivateAccess = "true"))
	EText3DVerticalTextAlignment VerticalAlignment;

	/** Text kerning */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (AllowPrivateAccess = "true"))
	float Kerning;

	/** Extra line spacing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (AllowPrivateAccess = "true"))
	float LineSpacing;

	/** Extra word spacing */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Setter, Category = "Layout", meta = (AllowPrivateAccess = "true"))
	float WordSpacing;

	/** Enables a maximum width to the 3D Text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "HasMaxWidth", Setter = "SetHasMaxWidth", Category = "Layout", meta = (InlineEditConditionToggle, AllowPrivateAccess = "true"))
	bool bHasMaxWidth;

	/** Sets a maximum width to the 3D Text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (EditCondition = "bHasMaxWidth", ClampMin = 1, AllowPrivateAccess = "true"))
	float MaxWidth;

	/** Enables a maximum height to the 3D Text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "HasMaxHeight", Setter = "SetHasMaxHeight", Category = "Layout", meta = (InlineEditConditionToggle, AllowPrivateAccess = "true"))
	bool bHasMaxHeight;

	/** Sets a maximum height to the 3D Text */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter, Setter, Category = "Layout", meta = (EditCondition = "bHasMaxHeight", ClampMin = 1, AllowPrivateAccess = "true"))
	float MaxHeight;

	/** Should the mesh scale proportionally when Max Width/Height is set */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "ScalesProportionally", Setter = "SetScaleProportionally", Category = "Layout", meta = (AllowPrivateAccess = "true"))
	bool bScaleProportionally;

	// Lighting flags

	/** Controls whether the text glyphs should cast a shadow or not. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Getter = "CastsShadow", Setter = "SetCastShadow", Category = "Lighting", meta = (AllowPrivateAccess = "true"))
	bool bCastShadow = true;

	/**
	 * Returns the Text property, after being formatted by the FormatText virtual function.
	 * If FormatText is not overriden, the return FText will be the same as the Text property.
	 */
	UFUNCTION(BlueprintCallable, Category = "Text3D")
	FText GetFormattedText() const;

protected:
	UFUNCTION()
	TArray<FName> GetTypefaceNames() const;

	/** Intercept and propagate a change on this component to all children. */
	virtual void OnVisibilityChanged() override;

	/** Intercept and propagate a change on this component to all children. */
	virtual void OnHiddenInGameChanged() override;

	/**
	 * Will be called when text geometry is generated.
	 * Override it to customize text formatting in the final geometry, without affecting the Text property.
	 * Use GetFormattedText() to retrieve a FText with the result of this formatting.
	 */
	virtual void FormatText(FText& InOutText) const {}

	/** Clears all generated components and meshes from this component. */
	void ClearTextMesh();

	/** Can be used to force an internal geometry and/or layout rebuild */
	void TriggerInternalRebuild(const EText3DModifyFlags InModifyFlags);

private:
	UPROPERTY()
	TObjectPtr<USceneComponent> TextRoot;

	UPROPERTY(BlueprintAssignable, Category = Events, meta = (AllowPrivateAccess = "true", DisplayName = "On Text Generated"))
	FTextGenerated TextGeneratedDelegate;

	FTextGeneratedNative TextGeneratedNativeDelegate;

	/** Flagged as true when the text mesh is being built. */
	std::atomic<bool> bIsBuilding;

	/** No mesh or layout rebuilds occur while this flag is true, allowing the user to set numerous properties before rebuilding. */
	bool bFreezeBuild;

	/** Used to determine and selectively perform the the type of rebuild requested. */
	EText3DModifyFlags ModifyFlags;

	/** Additional scale to apply to the text. */
	FVector TextScale;

	/** To uniquely identify and discard duplicate update requests. */
	TArray<TSharedPtr<int32>> CachedCounterReferences;

	/** Caches the last result of ShapedText, to allow faster updates of layout changes. */
	TSharedPtr<struct FText3DShapedText> ShapedText;

	/** Stores the text layout calculated by the TextLayoutMarshaller. */
	TSharedPtr<FTextLayout> TextLayout;

	/** Determines how text is laid out, ie. parsing line breaks. */
	TSharedPtr<ITextLayoutMarshaller> TextLayoutMarshaller;

	/** Each character mesh is parented to a Kerning component. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<USceneComponent>> CharacterKernings;

	/** Each character mesh is held in a component. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UStaticMeshComponent>> CharacterMeshes;

	/** Allocates, or shrinks existing components to match the input number. Returns false if nothing modified. */
	bool AllocateGlyphs(int32 Num);

	/** Slot based Material accessors. */
	UMaterialInterface* GetMaterial(const EText3DGroupType Type) const;
	void SetMaterial(const EText3DGroupType Type, UMaterialInterface* Material);
	void UpdateMaterial(const EText3DGroupType Type, UMaterialInterface* Material);

	/** Validation and context sensitive limits applied to the current bevel value. */
	void CheckBevel();
	float MaxBevel() const;

	/** Selective build functionality (geometry, only layout, etc.) */
	void UpdateTransforms();
	void RebuildInternal(const bool& bIsAutoUpdate = true, const bool& bCleanCache = false);
	void BuildTextMesh(const bool& bCleanCache = false);
	void BuildTextMeshInternal(const bool& bCleanCache);

	/** Layout functionality. */
	void CalculateTextWidth();
	float GetTextHeight() const;
	void CalculateTextScale();
	FVector GetLineLocation(int32 LineIndex);

	/** Convenience functions to set and query ModifyFlags. */
	bool NeedsMeshRebuild() const;
	bool NeedsLayoutUpdate() const;
	void MarkForGeometryUpdate();
	void MarkForLayoutUpdate();
	void ClearUpdateFlags();

	uint32 GetTypeFaceIndex() const;
	bool IsTypefaceAvailable(FName InTypeface) const;
	TArray<FTypefaceEntry> GetAvailableTypefaces() const;
	void RefreshTypeface();
};
