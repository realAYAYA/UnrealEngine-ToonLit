// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "BaseWidgetBlueprint.h"
#include "Binding/DynamicPropertyPath.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Animation/WidgetAnimationBinding.h"
#include "Templates/ValueOrError.h"

#include "WidgetBlueprint.generated.h"

class FCompilerResultsLog;
class UEdGraph;
class UMovieScene;
class UTexture2D;
class UUserWidget;
class UWidget;
class UWidgetAnimation;
class FKismetCompilerContext;
class UWidgetBlueprint;
enum class EWidgetTickFrequency : uint8;
enum class EWidgetCompileTimeTickPrediction : uint8;
class UWidgetEditingProjectSettings;


/** Widget Delegates */
class UMGEDITOR_API FWidgetBlueprintDelegates
{
public:
	// delegate for generating widget asset registry tags.
	DECLARE_MULTICAST_DELEGATE_TwoParams(FGetAssetTagsWithContext, const UWidgetBlueprint*, FAssetRegistryTagsContext);
	DECLARE_MULTICAST_DELEGATE_TwoParams(FGetAssetTags, const UWidgetBlueprint*, TArray<UObject::FAssetRegistryTag>&);

	// called by UWdgetBlueprint::GetAssetRegistryTags()
	static FGetAssetTagsWithContext GetAssetTagsWithContext;
	UE_DEPRECATED(5.4, "Subscribe to GetAssetTagsWithContext instead.")
	static FGetAssetTags GetAssetTags;
};


/** */
USTRUCT()
struct UMGEDITOR_API FEditorPropertyPathSegment
{
	GENERATED_USTRUCT_BODY()

public:
	FEditorPropertyPathSegment();
	FEditorPropertyPathSegment(const FProperty* InProperty);
	FEditorPropertyPathSegment(const UFunction* InFunction);
	FEditorPropertyPathSegment(const UEdGraph* InFunctionGraph);

	UStruct* GetStruct() const { return Struct; }
	FFieldVariant GetMember() const;

	void Rebase(UBlueprint* SegmentBase);
	bool ValidateMember(FDelegateProperty* DelegateProperty, FText& OutError) const;

	FName GetMemberName() const;
	FText GetMemberDisplayText() const;
	FGuid GetMemberGuid() const;

private:

	/** The owner of the path segment (ie. What class or structure was this property from) */
	UPROPERTY()
	TObjectPtr<UStruct> Struct;

	/** The member name in the structure this segment represents. */
	UPROPERTY()
	FName MemberName;

	/**
	 * The member guid in this structure this segment represents.  If this is valid it should 
	 * be used instead of Name to get the true name.
	 */
	UPROPERTY()
	FGuid MemberGuid;

	/** true if property, false if function */
	UPROPERTY()
	bool IsProperty;
};


/**  */
USTRUCT()
struct UMGEDITOR_API FEditorPropertyPath
{
	GENERATED_USTRUCT_BODY()

public:

	/**  */
	FEditorPropertyPath();

	/**  */
	FEditorPropertyPath(const TArray<FFieldVariant>& BindingChain);

	/**  */
	bool Rebase(UBlueprint* SegmentBase);

	/**  */
	bool IsEmpty() const { return Segments.Num() == 0; }

	/**  */
	bool Validate(FDelegateProperty* Destination, FText& OutError) const;

	/**  */
	FText GetDisplayText() const;

	/**  */
	FDynamicPropertyPath ToPropertyPath() const;

public:

	/** The path of properties. */
	UPROPERTY()
	TArray<FEditorPropertyPathSegment> Segments;
};

/** */
USTRUCT()
struct UMGEDITOR_API FDelegateEditorBinding
{
	GENERATED_USTRUCT_BODY()

	/** The member widget the binding is on, must be a direct variable of the UUserWidget. */
	UPROPERTY()
	FString ObjectName;

	/** The property on the ObjectName that we are binding to. */
	UPROPERTY()
	FName PropertyName;

	/** The function that was generated to return the SourceProperty */
	UPROPERTY()
	FName FunctionName;

	/** The property we are bindings to directly on the source object. */
	UPROPERTY()
	FName SourceProperty;

	/**  */
	UPROPERTY()
	FEditorPropertyPath SourcePath;

	/** If it's an actual Function Graph in the blueprint that we're bound to, there's a GUID we can use to lookup that function, to deal with renames better.  This is that GUID. */
	UPROPERTY()
	FGuid MemberGuid;

	UPROPERTY()
	EBindingKind Kind = EBindingKind::Property;

	bool operator==( const FDelegateEditorBinding& Other ) const
	{
		// NOTE: We intentionally only compare object name and property name, the function is irrelevant since
		// you're only allowed to bind a property on an object to a single function.
		return ObjectName == Other.ObjectName && PropertyName == Other.PropertyName;
	}

	bool IsAttributePropertyBinding(class UWidgetBlueprint* Blueprint) const;

	bool DoesBindingTargetExist(UWidgetBlueprint* Blueprint) const;

	bool IsBindingValid(UClass* Class, class UWidgetBlueprint* Blueprint, FCompilerResultsLog& MessageLog) const;

	FDelegateRuntimeBinding ToRuntimeBinding(class UWidgetBlueprint* Blueprint) const;
};


/** Struct used only for loading old animations */
USTRUCT()
struct FWidgetAnimation_DEPRECATED
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	TObjectPtr<UMovieScene> MovieScene = nullptr;

	UPROPERTY()
	TArray<FWidgetAnimationBinding> AnimationBindings;

	bool SerializeFromMismatchedTag(struct FPropertyTag const& Tag, FStructuredArchive::FSlot Slot);

};

template<>
struct TStructOpsTypeTraits<FWidgetAnimation_DEPRECATED> : public TStructOpsTypeTraitsBase2<FWidgetAnimation_DEPRECATED>
{
	enum
	{
		WithStructuredSerializeFromMismatchedTag = true,
	};
};

UENUM()
enum class EWidgetSupportsDynamicCreation : uint8
{
	Default,
	Yes,
	No,
};


UENUM()
enum class EThumbnailPreviewSizeMode : uint8
{
	MatchDesignerMode,
	FillScreen,
	Custom,
	Desired
};



/**
 * This represents the tickability of a widget computed at compile time
 * It is designed as a hint so the runtime can determine if ticking needs to be enabled
 * A lot of widgets set to WillTick means you might have a performance problem
 */
UENUM()
enum class EWidgetCompileTimeTickPrediction : uint8
{
	/** The widget is manually set to never tick or we dont detect any animations, latent actions, and/or script or possible native tick methods */
	WontTick,

	/** This widget is set to auto tick and we detect animations, latent actions but not script or native tick methods*/
	OnDemand,

	/** This widget has an implemented script tick or native tick */
	WillTick,
};

/**
 * The widget blueprint enables extending UUserWidget the user extensible UWidget.
 */
UCLASS(BlueprintType)
class UMGEDITOR_API UWidgetBlueprint : public UBaseWidgetBlueprint
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITORONLY_DATA
	
	UPROPERTY()
	TArray< FDelegateEditorBinding > Bindings;

	UPROPERTY()
	TArray<FWidgetAnimation_DEPRECATED> AnimationData_DEPRECATED;

	UPROPERTY()
	TArray<TObjectPtr<UWidgetAnimation>> Animations;

	/**
	 * Don't directly modify this property to change the palette category.  The actual value is stored 
	 * in the CDO of the UUserWidget, but a copy is stored here so that it's available in the serialized 
	 * Tag data in the asset header for access in the FAssetData.
	 */
	UPROPERTY(AssetRegistrySearchable)
	FString PaletteCategory;

	/** Run the initialize event on widget that doesn't have a player context. */
	UPROPERTY(EditAnywhere, Category="Widget")
	bool bCanCallInitializedWithoutPlayerContext;
#endif

public:

	/** UObject interface */
	virtual void PostLoad() override;
	virtual void PostDuplicate(bool bDuplicateForPIE) override;

#if WITH_EDITORONLY_DATA
	PRAGMA_DISABLE_DEPRECATION_WARNINGS // Suppress compiler warning on override of deprecated function
	UE_DEPRECATED(5.0, "Use version that takes FObjectPreSaveContext instead.")
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	PRAGMA_ENABLE_DEPRECATION_WARNINGS
	virtual void PreSave(FObjectPreSaveContext ObjectSaveContext) override;
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	virtual void GetAssetRegistryTags(FAssetRegistryTagsContext Context) const override;
	UE_DEPRECATED(5.4, "Implement the version that takes FAssetRegistryTagsContext instead.")
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual void NotifyGraphRenamed(class UEdGraph* Graph, FName OldName, FName NewName) override;
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
	bool DetectSlateWidgetLeaks(class FDataValidationContext& Context) const;
	virtual bool FindDiffs(const UBlueprint* OtherBlueprint, FDiffResults& Results) const override;
#endif

	virtual void Serialize(FArchive& Ar) override;

	UPackage* GetWidgetTemplatePackage() const;

	virtual void ReplaceDeprecatedNodes() override;
	
	//~ Begin UBlueprint Interface
	virtual UClass* GetBlueprintClass() const override;

	virtual bool AllowsDynamicBinding() const override;

	virtual bool SupportsInputEvents() const override;

	virtual bool SupportedByDefaultBlueprintFactory() const override
	{
		return false;
	}

	virtual void GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const override;

	/** UWidget blueprints are never data only, should always compile on load (data only blueprints cannot declare new variables) */
	virtual bool AlwaysCompileOnLoad() const override { return true; }
	//~ End UBlueprint Interface

	virtual void GatherDependencies(TSet<TWeakObjectPtr<UBlueprint>>& InDependencies) const override;

	/** Returns true if the supplied user widget will not create a circular reference when added to this blueprint */
	bool IsWidgetFreeFromCircularReferences(UUserWidget* UserWidget) const;
	
	/**  */
	TValueOrError<void, UWidget*> HasCircularReferences() const;

	static bool ValidateGeneratedClass(const UClass* InClass);
	
	static TSharedPtr<FKismetCompilerContext> GetCompilerForWidgetBP(UBlueprint* BP, FCompilerResultsLog& InMessageLog, const FKismetCompilerOptions& InCompileOptions);

	void UpdateTickabilityStats(bool& OutHasLatentActions, bool& OutHasAnimations, bool& OutClassRequiresNativeTick);

	bool ArePropertyBindingsAllowed() const;

	/** Gets any named slots exposed by the parent generated class that can be slotted into by the subclass. */
	TArray<FName> GetInheritedAvailableNamedSlots() const;

	virtual UWidgetEditingProjectSettings* GetRelevantSettings();
	virtual const UWidgetEditingProjectSettings* GetRelevantSettings() const;

protected:
	virtual void LoadModulesRequiredForCompilation() override;

private:
	/**
	 * The desired tick frequency set by the user on the UserWidget's CDO.
	 */
	UPROPERTY(AssetRegistrySearchable)
	EWidgetTickFrequency TickFrequency;

	/**
	 * The computed frequency that the widget will need to be ticked at.  You can find the reasons for
	 * this decision by looking at TickPredictionReason.
	 */
	UPROPERTY(AssetRegistrySearchable)
	EWidgetCompileTimeTickPrediction TickPrediction;

	/**
	 * The reasons we may need to tick this widget.
	 */
	UPROPERTY(AssetRegistrySearchable)
	FString TickPredictionReason;

public:

	/**
	 * The total number of property bindings.  Consider this as a performance warning.
	 */
	UPROPERTY(AssetRegistrySearchable)
	int32 PropertyBindings;

	UPROPERTY(EditDefaultsOnly, Category = ThumbnailSettings)
	EThumbnailPreviewSizeMode ThumbnailSizeMode;

	UPROPERTY(EditDefaultsOnly, Category = ThumbnailSettings)
	FVector2D ThumbnailCustomSize;

	UPROPERTY(EditDefaultsOnly, Category = ThumbnailSettings)
	TObjectPtr<UTexture2D> ThumbnailImage;

};
