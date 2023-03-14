// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CommonUITypes.h"
#include "ICommonUIModule.h"
#include "Engine/StreamableManager.h"
#include "Components/ContentWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Animation/CurveSequence.h"
#include "Engine/AssetManager.h"

#include "CommonLoadGuard.generated.h"

class UCommonTextStyle;
class STextBlock;
class SBorder;

//////////////////////////////////////////////////////////////////////////
// SLoadGuard
//////////////////////////////////////////////////////////////////////////

DECLARE_DELEGATE_OneParam(FOnLoadGuardStateChanged, bool);
DECLARE_DELEGATE_OneParam(FOnLoadGuardAssetLoaded, UObject*);

class COMMONUI_API SLoadGuard : public SCompoundWidget
{
	SLATE_BEGIN_ARGS(SLoadGuard)
		: _ThrobberHAlign(HAlign_Center)
		, _GuardTextStyle(nullptr)
		, _GuardBackgroundBrush(nullptr)
	{}
		SLATE_DEFAULT_SLOT(FArguments, Content)
	
		SLATE_ARGUMENT(EHorizontalAlignment, ThrobberHAlign)
		SLATE_ARGUMENT(FText, GuardText)
		SLATE_ARGUMENT(TSubclassOf<UCommonTextStyle>, GuardTextStyle)
		SLATE_ARGUMENT(const FSlateBrush*, GuardBackgroundBrush)

		SLATE_EVENT(FOnLoadGuardStateChanged, OnLoadingStateChanged)
	SLATE_END_ARGS()

public:
	SLoadGuard();
	void Construct(const FArguments& InArgs);
	virtual FVector2D ComputeDesiredSize(float) const override;

	void SetForceShowSpinner(bool bInForceShowSpinner);
	bool IsLoading() const { return bIsShowingSpinner; }
	
	void SetContent(const TSharedRef<SWidget>& InContent);
	void SetThrobberHAlign(EHorizontalAlignment InHAlign);
	void SetGuardText(const FText& InText);
	void SetGuardTextStyle(const FTextBlockStyle& InGuardTextStyle);
	void SetGuardBackgroundBrush(const FSlateBrush* InGuardBackground);

	/**
	 * Displays the loading spinner until the asset is loaded
	 * Will pass a casted pointer to the given asset in the lambda callback - could be nullptr if you provide an incompatible type or invalid asset.
	 */
	void GuardAndLoadAsset(const TSoftObjectPtr<UObject>& InLazyAsset, FOnLoadGuardAssetLoaded OnAssetLoaded);

	template <typename ObjectType>
	void GuardAndLoadAsset(const TSoftObjectPtr<UObject>& InLazyAsset, TFunction<void(ObjectType*)> OnAssetLoaded)
	{
		GuardAndLoadAsset(InLazyAsset, FOnLoadGuardAssetLoaded::CreateLambda([OnAssetLoaded](UObject* LoadedObject) {
			OnAssetLoaded(Cast<ObjectType>(LoadedObject));
		}));
	}

	TSharedRef<SBorder> GetContentBorder() const { return ContentBorder.ToSharedRef(); };

private:
	void UpdateLoadingAppearance();

	TSoftObjectPtr<UObject> LazyAsset;

	TSharedPtr<SBorder> ContentBorder;
	TSharedPtr<SBorder> GuardBorder;
	TSharedPtr<STextBlock> GuardTextBlock;

	FOnLoadGuardStateChanged OnLoadingStateChanged;

	TSharedPtr<FStreamableHandle> StreamingHandle;
	bool bForceShowSpinner = false;
	bool bIsShowingSpinner = false;
};

//////////////////////////////////////////////////////////////////////////
// ULoadGuardSlot
//////////////////////////////////////////////////////////////////////////

/** Virtually identical to a UBorderSlot, but unfortunately that assumes (fairly) that its parent widget is a UBorder. */
UCLASS()
class COMMONUI_API ULoadGuardSlot : public UPanelSlot
{
	GENERATED_BODY()

public:
	virtual void SynchronizeProperties() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	
	void BuildSlot(TSharedRef<SLoadGuard> InLoadGuard);

	UFUNCTION(BlueprintCallable, Category = "Layout|LoadGuard Slot")
	void SetPadding(FMargin InPadding);

	UFUNCTION(BlueprintCallable, Category = "Layout|LoadGuard Slot")
	void SetHorizontalAlignment(EHorizontalAlignment InHorizontalAlignment);

	UFUNCTION(BlueprintCallable, Category = "Layout|LoadGuard Slot")
	void SetVerticalAlignment(EVerticalAlignment InVerticalAlignment);

private:
	UPROPERTY(EditAnywhere, Category = "Layout|LoadGuard Slot")
	FMargin Padding;

	UPROPERTY(EditAnywhere, Category = "Layout|LoadGuard Slot")
	TEnumAsByte<EHorizontalAlignment> HorizontalAlignment = HAlign_Fill;

	UPROPERTY(EditAnywhere, Category = "Layout|LoadGuard Slot")
	TEnumAsByte<EVerticalAlignment> VerticalAlignment = VAlign_Fill;

	TWeakPtr<SLoadGuard> LoadGuard;
	friend class UCommonLoadGuard;
};


//////////////////////////////////////////////////////////////////////////
// ULoadGuard
//////////////////////////////////////////////////////////////////////////

DECLARE_MULTICAST_DELEGATE_OneParam(FOnLoadGuardStateChangedEvent, bool);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadGuardStateChangedDynamic, bool, bIsLoading);


/** 
 * The Load Guard behaves similarly to a Border, but with the ability to hide its primary content and display a loading spinner
 * and optional message while needed content is loaded or otherwise prepared.
 * 
 * Use GuardAndLoadAsset to automatically display the loading state until the asset is loaded (then the content widget will be displayed).
 * For other applications (ex: waiting for an async backend call to complete), you can manually set the loading state of the guard.
 */
UCLASS(Config = Game, DefaultConfig)
class COMMONUI_API UCommonLoadGuard : public UContentWidget
{
	GENERATED_BODY()

public:
	UCommonLoadGuard(const FObjectInitializer& Initializer);

	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;

	virtual void ReleaseSlateResources(bool bReleaseChildren) override;
	virtual void SynchronizeProperties() override;

	virtual UClass* GetSlotClass() const override { return ULoadGuardSlot::StaticClass(); }
	virtual void OnSlotAdded(UPanelSlot* NewSlot) override;
	virtual void OnSlotRemoved(UPanelSlot* OldSlot) override;

	UFUNCTION(BlueprintCallable, Category = LoadGuard)
	void SetLoadingText(const FText& InLoadingText);
	
	UFUNCTION(BlueprintCallable, Category = LoadGuard)
	void SetIsLoading(bool bInIsLoading);

	UFUNCTION(BlueprintCallable, Category = LoadGuard)
	bool IsLoading() const;

	/**
	 * Displays the loading spinner until the asset is loaded
	 * Will pass a casted pointer to the given asset in the lambda callback - could be nullptr if you provide an incompatible type or invalid asset.
	 */
	template <typename ObjectType = UObject>
	void GuardAndLoadAsset(const TSoftObjectPtr<ObjectType>& InLazyAsset, TFunction<void(ObjectType*)> OnAssetLoaded)
	{
		if (MyLoadGuard.IsValid())
		{
			MyLoadGuard->GuardAndLoadAsset<ObjectType>(InLazyAsset, OnAssetLoaded);
		}
	}

	void GuardAndLoadAsset(const TSoftObjectPtr<UObject>& InLazyAsset, FOnLoadGuardAssetLoaded OnAssetLoaded)
	{
		if (MyLoadGuard.IsValid())
		{
			MyLoadGuard->GuardAndLoadAsset(InLazyAsset, OnAssetLoaded);
		}
	}

	FOnLoadGuardStateChangedEvent& OnLoadingStateChanged() { return OnLoadingStateChangedEvent; }

#if WITH_EDITOR
	virtual void OnCreationFromPalette() override;
	virtual const FText GetPaletteCategory() override;
#endif

	DECLARE_DYNAMIC_DELEGATE_OneParam(FOnAssetLoaded, UObject*, Object);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;	

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = LoadGuard)
	bool bShowLoading = false;
#endif

private:
	UFUNCTION(BlueprintCallable, Category = LoadGuard, meta = (DisplayName = "Guard and Load Asset", ScriptName="GuardAndLoadAsset", AllowPrivateAccess = true))
	void BP_GuardAndLoadAsset(const TSoftObjectPtr<UObject>& InLazyAsset, const FOnAssetLoaded& OnAssetLoaded);

	void HandleLoadingStateChanged(bool bIsLoading);

	/** The background brush to display while loading the content */
	UPROPERTY(EditAnywhere, Category = LoadGuardThrobber)
	FSlateBrush LoadingBackgroundBrush;

	/** The horizontal alignment of the loading throbber & message */
	UPROPERTY(EditAnywhere, Category = LoadGuardThrobber)
	TEnumAsByte<EHorizontalAlignment> ThrobberAlignment;

	/** The horizontal alignment of the loading throbber & message */
	UPROPERTY(EditAnywhere, Category = LoadGuardThrobber)
	FMargin ThrobberPadding;

	/** Loading message to display alongside the throbber */
	UPROPERTY(EditAnywhere, Category = LoadGuardText)
	FText LoadingText;

	/** Style to apply to the loading message */
	UPROPERTY(EditAnywhere, Category = LoadGuardText)
	TSubclassOf<UCommonTextStyle> TextStyle;

	UPROPERTY(BlueprintAssignable, Category = LoadGuard, meta = (DisplayName = "On Loading State Changed"))
	FOnLoadGuardStateChangedDynamic BP_OnLoadingStateChanged;

	UPROPERTY(Config)
	FSoftObjectPath SpinnerMaterialPath;

#if WITH_EDITORONLY_DATA
	/** Used to track widgets that were created before changing the default style pointer to null */
	UPROPERTY()
	bool bStyleNoLongerNeedsConversion;
#endif

	TSharedPtr<SLoadGuard> MyLoadGuard;

	FOnLoadGuardStateChangedEvent OnLoadingStateChangedEvent;
};
