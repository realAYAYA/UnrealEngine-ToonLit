// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/LazyObjectPtr.h"
#include "TransformNoScale.h"
#include "EulerTransform.h"
#include "Math/Rotator.h"
#include "MovieSceneTrack.h"
#include "IDetailKeyframeHandler.h"
#include "IPropertyTypeCustomization.h"
#include "Rigs/RigHierarchyDefines.h"
#include "MovieSceneCommonHelpers.h"
#include "Rigs/RigHierarchyCache.h"

#include "ControlRigControlsProxy.generated.h"

struct FRigControlElement;
class UControlRig;
class IPropertyHandle;
class FControlRigInteractionScope;
struct FRigControlModifiedContext;
class ISequencer;
class FCurveEditor;
class IDetailLayoutBuilder;
class FProperty;

//channel selection states for selection matching with curves
enum class EAnimDetailSelectionState : uint8
{
	None = 0x0, Partial = 0x1, All = 0x2
};

struct FAnimDetailVectorSelection
{
	EAnimDetailSelectionState  XSelected = EAnimDetailSelectionState::None;
	EAnimDetailSelectionState  YSelected = EAnimDetailSelectionState::None;
	EAnimDetailSelectionState  ZSelected = EAnimDetailSelectionState::None;
};

//item to specify control rig, todo move to handle class
struct FControlRigProxyItem
{
	TWeakObjectPtr<UControlRig> ControlRig;
	TArray<FName> ControlElements;
	FRigControlElement* GetControlElement(const FName& InName) const;
};

struct FBindingAndTrack
{
	FBindingAndTrack() : Binding(nullptr), WeakTrack(nullptr) {};
	FBindingAndTrack(TSharedPtr<FTrackInstancePropertyBindings>& InBinding, UMovieSceneTrack* InWeakTrack) :
	Binding(InBinding), WeakTrack(InWeakTrack){}
	TSharedPtr<FTrackInstancePropertyBindings> Binding;
	TWeakObjectPtr<UMovieSceneTrack> WeakTrack;
};
//item to specify sequencer item, todo move to handle class
struct FSequencerProxyItem
{
	TWeakObjectPtr<UObject> OwnerObject;
	TArray<FBindingAndTrack> Bindings;
};

UCLASS(Abstract)
class UControlRigControlsProxy : public UObject
{
	GENERATED_BODY()

public:
	UControlRigControlsProxy() : bSelected(false) {}
	//will add controlrig or sequencer item
	virtual void AddItem(UControlRigControlsProxy* ControlProxy);
	virtual void AddChildProxy(UControlRigControlsProxy* ControlProxy);
	virtual FName GetName() const { return Name; }
	virtual void UpdatePropertyNames(IDetailLayoutBuilder& DetailBuilder) {};
	virtual void ValueChanged() {}
	virtual void SelectionChanged(bool bInSelected);
	virtual void SetKey(TSharedPtr<ISequencer>& Sequencer, const IPropertyHandle& KeyedPropertyHandle) {};
	virtual EPropertyKeyedStatus GetPropertyKeyedStatus(TSharedPtr<ISequencer>& Sequencer, const IPropertyHandle& PropertyHandle) const { return EPropertyKeyedStatus::NotKeyed; }
	virtual EControlRigContextChannelToKey GetChannelToKeyFromPropertyName(const FName& PropertyName) const { return EControlRigContextChannelToKey::AllTransform; }
	virtual EControlRigContextChannelToKey GetChannelToKeyFromChannelName(const FString& InChannelName) const { return EControlRigContextChannelToKey::AllTransform; }
	virtual bool IsMultiple(const FName& InPropertyName) const { return false; }
	virtual void SetControlRigElementValueFromCurrent(UControlRig* ControlRig, FRigControlElement* ControlElement, const FRigControlModifiedContext& Context) {};
	virtual void SetBindingValueFromCurrent(UObject* InObject, TSharedPtr<FTrackInstancePropertyBindings>& Binding, FRigControlModifiedContext& Context, bool bInteractive = false) {};
	virtual void GetChannelSelectionState(TWeakPtr<FCurveEditor>& CurveEditor, FAnimDetailVectorSelection& OutLocationSelection, FAnimDetailVectorSelection& OutRotationSelection,
		FAnimDetailVectorSelection& OutScaleSelection) {};
	virtual bool PropertyIsOnProxy(FProperty* Property, FProperty* MemberProperty){return false;}

	// UObject interface
	virtual void PostEditChangeChainProperty( struct FPropertyChangedChainEvent& PropertyChangedEvent ) override;
#if WITH_EDITOR
	virtual void PostEditUndo() override;
#endif

	TArray<FRigControlElement*> GetControlElements() const;

	TArray<FBindingAndTrack> GetSequencerItems() const;
	//Default type
	ERigControlType Type = ERigControlType::Transform;

	//reset items it owns
	void ResetItems();

	//add correct item
	void AddSequencerProxyItem(UObject* InObject, TWeakObjectPtr<UMovieSceneTrack>& InTrack,  TSharedPtr<FTrackInstancePropertyBindings>& Binding);
	void AddControlRigControl(UControlRig* InControlRig, const FName& InName);

private:
	void HandlePostEditChangedForControlRigs();
	void HandlePostEditChangedForSequencer();

	//reset the  rig controls this also owns
	void ResetControlRigItems();
	void ResetSequencerItems();

	FCachedRigElement& GetOwnerControlElement();

public:

	//if individual it will show up independently, this will happen for certain nested controls
	bool bIsIndividual = false;

	UPROPERTY()
	bool bSelected;
	UPROPERTY(VisibleAnywhere, Category = "Control")
	FName Name;

	//refactor
	//We can set/get values form multiple control rig elements but only one owns this.
	TWeakObjectPtr<UControlRig> OwnerControlRig;
	FCachedRigElement OwnerControlElement;
	TMap<TWeakObjectPtr<UControlRig>, FControlRigProxyItem> ControlRigItems;
	
	TWeakObjectPtr<UObject> OwnerObject;
	FBindingAndTrack OwnerBindingAndTrack;
	TMap <TWeakObjectPtr<UObject>, FSequencerProxyItem> SequencerItems;

	//list of child/animation channel proxies that we will customize
	TArray<UControlRigControlsProxy*> ChildProxies;

#if WITH_EDITOR
	TMap<FRigControlElement*, FControlRigInteractionScope*> InteractionScopes;
#endif
};

USTRUCT()
struct FNameToProxyMap
{
	GENERATED_BODY();
	UPROPERTY()
	TMap <FName, TObjectPtr<UControlRigControlsProxy>> NameToProxy;;
};

struct FSequencerProxyPerType
{
	TMap<UObject*, TArray<FBindingAndTrack>> Bindings;
};

/** Proxy in Details Panel */
UCLASS()
class UControlRigDetailPanelControlProxies :public UObject
{
	GENERATED_BODY()

	UControlRigDetailPanelControlProxies();
	~UControlRigDetailPanelControlProxies();

protected:

	UPROPERTY()
	TMap<TObjectPtr<UControlRig>, FNameToProxyMap> ControlRigOnlyProxies; //proxies themselves contain weakobjectptr to the controlrig

	UPROPERTY()
	TArray< TObjectPtr<UControlRigControlsProxy>> SelectedControlRigProxies;
	
	UPROPERTY()
	TMap<TObjectPtr<UObject>, FNameToProxyMap> SequencerOnlyProxies;

	UPROPERTY()
	TArray< TObjectPtr<UControlRigControlsProxy>> SelectedSequencerProxies;
	
	TWeakPtr<ISequencer> Sequencer;

public:

	void SelectProxy(UControlRig* InControlRig, FRigControlElement* RigElement, bool bSelected);
	UControlRigControlsProxy* AddProxy(UControlRig* InControlRig, FRigControlElement* RigElement);
	UControlRigControlsProxy* AddProxy(UObject* InObject, ERigControlType Type, TWeakObjectPtr<UMovieSceneTrack>& Track, TSharedPtr<FTrackInstancePropertyBindings>& Binding);
	void RemoveControlRigProxies(UControlRig* InControlRig);
	void RemoveSequencerProxies(UObject* InObject);
	void RemoveAllProxies();
	void ProxyChanged(UControlRig* InControlRig, FRigControlElement* RigElement, bool bModify = true);
	void RecreateAllProxies(UControlRig* InControlRig);
	const TArray<UControlRigControlsProxy*> GetAllSelectedProxies() const;
	bool IsSelected(UControlRig* InControlRig, FRigControlElement* RigElement) const;
	void SetSequencer(TWeakPtr<ISequencer> InSequencer) { Sequencer = InSequencer; }
	ISequencer* GetSequencer() const { return Sequencer.Pin().Get(); }
	void ValuesChanged();
	void ResetSequencerProxies(TMap<ERigControlType, FSequencerProxyPerType>& ProxyPerType);

private:
	UControlRigControlsProxy* FindProxy(UControlRig* InControlRig, FRigControlElement* RigElement) const;
	UControlRigControlsProxy* FindProxy(UObject* InObject, FName PropertyName) const;

	UControlRigControlsProxy* NewProxyFromType(ERigControlType Type, TObjectPtr<UEnum>& EnumPtr);

	//delegate for changed objects
	void OnPostPropertyChanged(UObject* InObject, FPropertyChangedEvent& InPropertyChangedEvent);
};
