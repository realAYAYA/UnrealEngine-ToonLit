// Copyright Epic Games, Inc. All Rights Reserved.

#include "ControlsProxyDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "EditMode/ControlRigControlsProxy.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SSegmentedControl.h"
#include "Widgets/Layout/SBox.h"
#include "Algo/Transform.h"
#include "SAdvancedTransformInputBox.h"
#include "PropertyCustomizationHelpers.h"
#include "PropertyEditorModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"

#include "MVVM/CurveEditorExtension.h"
#include "MVVM/ViewModelPtr.h"
#include "MVVM/Selection/Selection.h"
#include "MVVM/ViewModels/SequencerEditorViewModel.h"
#include "MVVM/ViewModels/OutlinerViewModel.h"
#include "MVVM/ViewModels/ViewModelIterators.h"
#include "MVVM/Extensions/IOutlinerExtension.h"
#include "MVVM/ViewModels/TrackModel.h"
#include "MVVM/ViewModels/ChannelModel.h"
#include "Rigs/RigControlHierarchy.h"
#include "Tree/SCurveEditorTree.h"

#include "CurveEditor.h"
#include "CurveModel.h"
#include "Sequencer/MovieSceneControlRigParameterSection.h"
#include "Framework/Application/SlateApplication.h"

#define LOCTEXT_NAMESPACE "FControlsProxyDetailCustomization"

void FPropertySelectionCache::OnCurveModelDisplayChanged(FCurveModel* InCurveModel, bool bDisplayed, const FCurveEditor* InCurveEditor)
{
	bCacheValid = false;
}

EAnimDetailSelectionState FPropertySelectionCache::IsPropertySelected(TSharedPtr<FCurveEditor>& InCurveEditor, UControlRigControlsProxy* Proxy, const FName& PropertyName)
{
	if (CurveEditor.IsValid() == false || InCurveEditor.Get() != CurveEditor.Pin().Get())
	{
		ClearDelegates();
		CurveEditor = InCurveEditor;
		if (InCurveEditor->OnCurveArrayChanged.IsBoundToObject(this) == false)
		{
			InCurveEditor->OnCurveArrayChanged.AddRaw(this, &FPropertySelectionCache::OnCurveModelDisplayChanged);
		}
		bCacheValid = false;
	}
	if (bCacheValid == false)
	{
		CachePropertySelection(Proxy);
	}
	EAnimDetailSelectionState SelectionState = EAnimDetailSelectionState::None;

	if ( PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyLocation, LX)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector2D, X)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyFloat, Float)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyBool, Bool)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyInteger, Integer)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FControlRigEnumControlProxyValue, EnumIndex)

		)
	{
		SelectionState = LocationSelectionCache.XSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyLocation, LY)
		|| PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector2D, Y))
	{
		SelectionState = LocationSelectionCache.YSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyLocation, LZ))
	{
		SelectionState = LocationSelectionCache.ZSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyVector2D, Vector2D))
	{
		if (LocationSelectionCache.XSelected == EAnimDetailSelectionState::All && LocationSelectionCache.YSelected == EAnimDetailSelectionState::All)
		{
			SelectionState = EAnimDetailSelectionState::All;
		}
		else if (LocationSelectionCache.XSelected == EAnimDetailSelectionState::None && LocationSelectionCache.YSelected == EAnimDetailSelectionState::None)
		{
			SelectionState = EAnimDetailSelectionState::None;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyTransform, Location))
	{
		if (LocationSelectionCache.XSelected == EAnimDetailSelectionState::All && LocationSelectionCache.YSelected == EAnimDetailSelectionState::All
			&& LocationSelectionCache.ZSelected == EAnimDetailSelectionState::All)
		{
			SelectionState = EAnimDetailSelectionState::All;
		}
		else if (LocationSelectionCache.XSelected == EAnimDetailSelectionState::None && LocationSelectionCache.YSelected == EAnimDetailSelectionState::None
			&& LocationSelectionCache.ZSelected == EAnimDetailSelectionState::None)
		{
			SelectionState = EAnimDetailSelectionState::None;
		}
		else
		{
			SelectionState = EAnimDetailSelectionState::Partial;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyLocation, Location))
	{
		if (LocationSelectionCache.XSelected == EAnimDetailSelectionState::All && LocationSelectionCache.YSelected == EAnimDetailSelectionState::All
			&& LocationSelectionCache.ZSelected == EAnimDetailSelectionState::All)
		{
			SelectionState = EAnimDetailSelectionState::All;
		}
		else if (LocationSelectionCache.XSelected == EAnimDetailSelectionState::None && LocationSelectionCache.YSelected == EAnimDetailSelectionState::None
			&& LocationSelectionCache.ZSelected == EAnimDetailSelectionState::None)
		{
			SelectionState = EAnimDetailSelectionState::None;
		}
		else
		{
			SelectionState = EAnimDetailSelectionState::Partial;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyRotation, RX))
	{
		SelectionState = RotationSelectionCache.XSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyRotation, RY))
	{
		SelectionState = RotationSelectionCache.YSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyRotation, RZ))
	{
		SelectionState = RotationSelectionCache.ZSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyTransform, Rotation))
	{
		if (RotationSelectionCache.XSelected == EAnimDetailSelectionState::All && RotationSelectionCache.YSelected == EAnimDetailSelectionState::All
			&& RotationSelectionCache.ZSelected == EAnimDetailSelectionState::All)
		{
			SelectionState = EAnimDetailSelectionState::All;
		}
		else if (RotationSelectionCache.XSelected == EAnimDetailSelectionState::None && RotationSelectionCache.YSelected == EAnimDetailSelectionState::None
			&& RotationSelectionCache.ZSelected == EAnimDetailSelectionState::None)
		{
			SelectionState = EAnimDetailSelectionState::None;
		}
		else
		{
			SelectionState = EAnimDetailSelectionState::Partial;
		}
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyScale, SX))
	{
		SelectionState = ScaleSelectionCache.XSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyScale, SY))
	{
		SelectionState = ScaleSelectionCache.YSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyScale, SZ))
	{
		SelectionState = ScaleSelectionCache.ZSelected;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(UAnimDetailControlsProxyTransform, Scale))
	{
		if (ScaleSelectionCache.XSelected == EAnimDetailSelectionState::All && ScaleSelectionCache.YSelected == EAnimDetailSelectionState::All
			&& ScaleSelectionCache.ZSelected == EAnimDetailSelectionState::All)
		{
			SelectionState = EAnimDetailSelectionState::All;
		}
		else if (ScaleSelectionCache.XSelected == EAnimDetailSelectionState::None && ScaleSelectionCache.YSelected == EAnimDetailSelectionState::None
			&& ScaleSelectionCache.ZSelected == EAnimDetailSelectionState::None)
		{
			SelectionState = EAnimDetailSelectionState::None;
		}
		else
		{
			SelectionState = EAnimDetailSelectionState::Partial;
		}
	}

	return SelectionState;
}

void FPropertySelectionCache::ClearDelegates()
{
	TSharedPtr<FCurveEditor> SharedCurveEditor = CurveEditor.Pin();

	if (SharedCurveEditor)
	{
		SharedCurveEditor->OnCurveArrayChanged.RemoveAll(this);
	}
}

void FPropertySelectionCache::CachePropertySelection(UControlRigControlsProxy* Proxy)
{
	if (CurveEditor.IsValid() == false)
	{
		return;
	}
	bCacheValid = true;
	Proxy->GetChannelSelectionState(CurveEditor, LocationSelectionCache, RotationSelectionCache, ScaleSelectionCache);
}

UControlRigControlsProxy* FAnimDetailValueCustomization::GetProxy(TSharedRef< IPropertyHandle>& PropertyHandle) const
{
	TArray<UObject*> OuterObjects;
	PropertyHandle->GetOuterObjects(OuterObjects);
	for (UObject* OuterObject : OuterObjects)
	{
		UControlRigControlsProxy* Proxy = Cast<UControlRigControlsProxy>(OuterObject);
		if (Proxy)
		{
			return Proxy;
		}
	}
	return nullptr;
}

UControlRigDetailPanelControlProxies* FAnimDetailValueCustomization::GetProxyOwner(UControlRigControlsProxy* Proxy) const
{
	if (Proxy)
	{
		UControlRigDetailPanelControlProxies* ProxyOwner = Proxy->GetTypedOuter<UControlRigDetailPanelControlProxies>();
		if (ProxyOwner == nullptr || ProxyOwner->GetSequencer() == nullptr)
		{
			return nullptr;
		}
		return ProxyOwner;
	}
	return nullptr;
}

EVisibility FAnimDetailValueCustomization::IsVisible() const 
{
	if (DetailPropertyRow == nullptr) //need to wait for it to be constructed
	{
		DetailPropertyRow = DetailBuilder->EditPropertyFromRoot(StructPropertyHandlePtr);
	}
	return (DetailPropertyRow && DetailPropertyRow->IsExpanded()) ? EVisibility::Collapsed : EVisibility::Visible;
}

void FAnimDetailValueCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FOnBooleanValueChanged OnExpansionChanged = FOnBooleanValueChanged::CreateLambda([this](bool bNewValue)
		{
			int a = 3;
		});
	IDetailCategoryBuilder& CatBuilder = StructBuilder.GetParentCategory();
	CatBuilder.OnExpansionChanged(OnExpansionChanged);
	DetailBuilder = &CatBuilder.GetParentLayout();
	StructPropertyHandlePtr = StructPropertyHandle;
	FProperty* Property = StructPropertyHandle->GetProperty(); // Is a FProperty*
	UControlRigControlsProxy* Proxy = GetProxy(StructPropertyHandle);

	for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

		const FName PropertyName = ChildHandle->GetProperty()->GetFName();
		const FText PropertyDisplayText = ChildHandle->GetPropertyDisplayName();

		TSharedRef<SWidget> ValueWidget = MakeChildWidget(StructPropertyHandle, ChildHandle);

		// Add the individual properties as children as well so the vector can be expanded for more room
		
		StructBuilder.AddProperty(ChildHandle).CustomWidget()
		.NameContent()
		.HAlign(HAlign_Fill)
		[
			 SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
			.BorderBackgroundColor_Lambda([Proxy, PropertyName, this]()
			{
				EAnimDetailSelectionState SelectionState = IsPropertySelected(Proxy, PropertyName);
				if (SelectionState == EAnimDetailSelectionState::All)
				{
					return  FStyleColors::Select;
				}
				else if (SelectionState == EAnimDetailSelectionState::Partial)
				{
					return  FStyleColors::SelectInactive;
				}
				return FStyleColors::Transparent;
			})
			.OnMouseButtonDown_Lambda([Proxy, PropertyName, this](const FGeometry&, const FPointerEvent& PointerEvent)
			{
				TogglePropertySelection(Proxy, PropertyName);
				return FReply::Handled();
			})
			.Content()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.Padding(0,0,0,0)
				[
					SNew(STextBlock)
					//.Font(InCustomizationUtils.GetRegularFont())
					.Text_Lambda([PropertyDisplayText]()
					{
						return PropertyDisplayText;
					})
					
				]
			]
		]
		.ValueContent()
		.HAlign(HAlign_Fill)
		[
			 SNew(SBorder)
			.BorderImage(FAppStyle::Get().GetBrush("DetailsView.CategoryMiddle"))
			.BorderBackgroundColor_Lambda([Proxy, PropertyName, this]()
			{
				EAnimDetailSelectionState SelectionState = IsPropertySelected(Proxy, PropertyName);
				if (SelectionState == EAnimDetailSelectionState::All)
				{
					return  FStyleColors::Select;
				}
				else if (SelectionState == EAnimDetailSelectionState::Partial)
				{
					return  FStyleColors::SelectInactive;
				}
				return FStyleColors::Transparent;
			})
			.OnMouseButtonDown_Lambda([Proxy, PropertyName, this](const FGeometry&, const FPointerEvent& PointerEvent)
			{
				TogglePropertySelection(Proxy, PropertyName);
				return FReply::Handled();
			})
			.Content()
			[
				ValueWidget
			]
		];
		/*
		.ValueContent()
		[
			ValueWidget
		];
		*/
		
	}
}


TSharedRef<IPropertyTypeCustomization> FAnimDetailValueCustomization::MakeInstance()
{
	return MakeShareable(new FAnimDetailValueCustomization);
}

FAnimDetailValueCustomization::FAnimDetailValueCustomization() = default;

FAnimDetailValueCustomization::~FAnimDetailValueCustomization() = default;

void FAnimDetailValueCustomization::MakeHeaderRow(TSharedRef<class IPropertyHandle>& StructPropertyHandle, FDetailWidgetRow& Row)
{
	if (SortedChildHandles.Num() == 1)
	{
		return;
	}

	FProperty* Property = StructPropertyHandle->GetProperty(); // Is a FProperty*
	TWeakPtr<IPropertyHandle> StructWeakHandlePtr = StructPropertyHandle;
	TSharedPtr<SHorizontalBox> HorizontalBox;
	Row.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
	.ValueContent()
		// Make enough space for each child handle
		.MinDesiredWidth(125.f * SortedChildHandles.Num())
		.MaxDesiredWidth(125.f * SortedChildHandles.Num())
		[
				SAssignNew(HorizontalBox, SHorizontalBox)
				.Visibility(this, &FAnimDetailValueCustomization::IsVisible)
				.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, StructWeakHandlePtr)
		];

	for (int32 ChildIndex = 0; ChildIndex < SortedChildHandles.Num(); ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = SortedChildHandles[ChildIndex];

		// Propagate metadata to child properties so that it's reflected in the nested, individual spin boxes
		ChildHandle->SetInstanceMetaData(TEXT("UIMin"), StructPropertyHandle->GetMetaData(TEXT("UIMin")));
		ChildHandle->SetInstanceMetaData(TEXT("UIMax"), StructPropertyHandle->GetMetaData(TEXT("UIMax")));
		ChildHandle->SetInstanceMetaData(TEXT("SliderExponent"), StructPropertyHandle->GetMetaData(TEXT("SliderExponent")));
		ChildHandle->SetInstanceMetaData(TEXT("Delta"), StructPropertyHandle->GetMetaData(TEXT("Delta")));
		ChildHandle->SetInstanceMetaData(TEXT("LinearDeltaSensitivity"), StructPropertyHandle->GetMetaData(TEXT("LinearDeltaSensitivity")));
		ChildHandle->SetInstanceMetaData(TEXT("ShiftMultiplier"), StructPropertyHandle->GetMetaData(TEXT("ShiftMultiplier")));
		ChildHandle->SetInstanceMetaData(TEXT("CtrlMultiplier"), StructPropertyHandle->GetMetaData(TEXT("CtrlMultiplier")));
		ChildHandle->SetInstanceMetaData(TEXT("SupportDynamicSliderMaxValue"), StructPropertyHandle->GetMetaData(TEXT("SupportDynamicSliderMaxValue")));
		ChildHandle->SetInstanceMetaData(TEXT("SupportDynamicSliderMinValue"), StructPropertyHandle->GetMetaData(TEXT("SupportDynamicSliderMinValue")));
		ChildHandle->SetInstanceMetaData(TEXT("ClampMin"), StructPropertyHandle->GetMetaData(TEXT("ClampMin")));
		ChildHandle->SetInstanceMetaData(TEXT("ClampMax"), StructPropertyHandle->GetMetaData(TEXT("ClampMax")));

		const bool bLastChild = SortedChildHandles.Num() - 1 == ChildIndex;

		TSharedRef<SWidget> ChildWidget = MakeChildWidget(StructPropertyHandle, ChildHandle);
		if (ChildHandle->GetPropertyClass() == FBoolProperty::StaticClass())
		{
			HorizontalBox->AddSlot()
				.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
				.AutoWidth()  // keep the check box slots small
				[
					ChildWidget
				];
		}
		else
		{
			if (ChildHandle->GetPropertyClass() == FDoubleProperty::StaticClass())
			{
				NumericEntryBoxWidgetList.Add(ChildWidget);
			}

			HorizontalBox->AddSlot()
				.Padding(FMargin(0.f, 2.f, bLastChild ? 0.f : 3.f, 2.f))
				[
					ChildWidget
				];
		}
	}
}

FLinearColor FAnimDetailValueCustomization::GetColorFromProperty(const FName& PropertyName) const
{
	FLinearColor Color = FLinearColor::White;
	if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector3, X) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector2D, X) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyLocation, LX) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyRotation, RX) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyScale, SX))
	{
		Color = SNumericEntryBox<double>::RedLabelBackgroundColor;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector3, Y) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector2D, Y) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyLocation, LY) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyRotation, RY) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyScale, SY))
	{
		Color = SNumericEntryBox<double>::GreenLabelBackgroundColor;
	}
	else if (PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyVector3, Z) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyLocation, LZ) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyRotation, RZ) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(FAnimDetailProxyScale, SZ))
	{
		Color = SNumericEntryBox<double>::BlueLabelBackgroundColor;
	}
	return Color;
}

EAnimDetailSelectionState FAnimDetailValueCustomization::IsPropertySelected(UControlRigControlsProxy* Proxy,const FName& PropertyName)
{
	using namespace UE::Sequencer;

	UControlRigDetailPanelControlProxies* ProxyOwner = GetProxyOwner(Proxy);
	if (ProxyOwner == nullptr)
	{
		return EAnimDetailSelectionState::None;
	}
	const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = ProxyOwner->GetSequencer()->GetViewModel();
	const FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>();
	check(CurveEditorExtension);
	TSharedPtr<FCurveEditor> CurveEditor = CurveEditorExtension->GetCurveEditor();
	return SelectionCache.IsPropertySelected(CurveEditor,Proxy, PropertyName);
	
}

//may want this to be under each proxy? need to think about this.
static bool GetChannelNameForCurve(const TArray<FString>& CurveString, const FRigControlElement* ControlElement, FString& OutChannelName)
{
	//if single channel expect one item and the name will match
	if (ControlElement->Settings.ControlType == ERigControlType::ScaleFloat ||
		ControlElement->Settings.ControlType == ERigControlType::Float ||
		ControlElement->Settings.ControlType == ERigControlType::Bool ||
		ControlElement->Settings.ControlType == ERigControlType::Integer)
	{
		if (CurveString[0] == ControlElement->GetKey().Name)
		{
			if (ControlElement->Settings.ControlType == ERigControlType::ScaleFloat ||
				ControlElement->Settings.ControlType == ERigControlType::Float)
			{
				OutChannelName = FString("Float");
				return true;
			}
			if (ControlElement->Settings.ControlType == ERigControlType::Bool)
			{
				OutChannelName = FString("Bool");
				return true;
			}
			if (ControlElement->Settings.ControlType == ERigControlType::Integer)
			{
				OutChannelName = FString("Integer");
				return true;
			}
		}
	}
	else if (CurveString.Num() > 1)
	{
		if (CurveString[0] == ControlElement->GetKey().Name)
		{
			if (CurveString.Num() == 3)
			{
				OutChannelName = CurveString[1] + "." + CurveString[2];
				return true;
			}
			else if (CurveString.Num() == 2)
			{
				OutChannelName = CurveString[1];
				return true;
			}
		}
	}
	return false;
}

void FAnimDetailValueCustomization::TogglePropertySelection(UControlRigControlsProxy* Proxy, const FName& PropertyName) const
{
	using namespace UE::Sequencer;
	UControlRigDetailPanelControlProxies* ProxyOwner = GetProxyOwner(Proxy);
	if (ProxyOwner == nullptr)
	{
		return;
	}

	const TSharedPtr<FSequencerEditorViewModel> SequencerViewModel = ProxyOwner->GetSequencer()->GetViewModel();
	const FCurveEditorExtension* CurveEditorExtension = SequencerViewModel->CastDynamic<FCurveEditorExtension>();
	check(CurveEditorExtension);
	TSharedPtr<FCurveEditor> CurveEditor = CurveEditorExtension->GetCurveEditor();
	TSharedPtr<SCurveEditorTree>  CurveEditorTreeView = CurveEditorExtension->GetCurveEditorTreeView();
	TSharedPtr<FOutlinerViewModel> OutlinerViewModel = SequencerViewModel->GetOutliner();

	const bool bIsShiftDown = FSlateApplication::Get().GetModifierKeys().IsShiftDown();
	const bool bIsCtrlDown = FSlateApplication::Get().GetModifierKeys().IsControlDown();
	if (bIsShiftDown == false && bIsCtrlDown == false)
	{
		CurveEditorTreeView->ClearSelection();
	}

	if (Proxy)
	{
		for (const TPair<TWeakObjectPtr<UControlRig>, FControlRigProxyItem>& Items : Proxy->ControlRigItems)
		{
			if (UControlRig* ControlRig = Items.Value.ControlRig.Get())
			{
				for (const FName& CName : Items.Value.ControlElements)
				{
					if (FRigControlElement* ControlElement = Items.Value.GetControlElement(CName))
					{
						EControlRigContextChannelToKey ChannelToKey = Proxy->GetChannelToKeyFromPropertyName(PropertyName);
						TParentFirstChildIterator<IOutlinerExtension> OutlinerExtenstionIt = OutlinerViewModel->GetRootItem()->GetDescendantsOfType<IOutlinerExtension>();
						for (; OutlinerExtenstionIt; ++OutlinerExtenstionIt)
						{
							if (TSharedPtr<FTrackModel> TrackModel = OutlinerExtenstionIt.GetCurrentItem()->FindAncestorOfType<FTrackModel>())
							{
								if(UMovieSceneControlRigParameterTrack* Track = Cast<UMovieSceneControlRigParameterTrack>(TrackModel->GetTrack()))
								{
									if (Track->GetControlRig() != ControlRig)
									{
										continue;
									}
									if (TViewModelPtr<FChannelGroupOutlinerModel> ChannelModel = CastViewModel<FChannelGroupOutlinerModel>(OutlinerExtenstionIt.GetCurrentItem()))
									{
										if (ChannelModel->GetChannel(Track->GetSectionToKey()) == nullptr) //if not section to key we also don't select it.
										{
											continue;
										}
									}
									else
									{
										continue;
									}
									
									FName ID = OutlinerExtenstionIt->GetIdentifier();
									FString Name = ID.ToString();
									TArray<FString> StringArray;
									Name.ParseIntoArray(StringArray, TEXT("."));

									FString ChannelName;
									if (GetChannelNameForCurve(StringArray, ControlElement, ChannelName))
									{
										EControlRigContextChannelToKey ChannelToKeyFromCurve = Proxy->GetChannelToKeyFromChannelName(ChannelName);
										if (ChannelToKey == ChannelToKeyFromCurve)
										{
											if (TViewModelPtr<ICurveEditorTreeItemExtension> CurveEditorItem = OutlinerExtenstionIt.GetCurrentItem().ImplicitCast())
											{
												FCurveEditorTreeItemID CurveEditorTreeItem = CurveEditorItem->GetCurveEditorItemID();
												if (CurveEditorTreeItem != FCurveEditorTreeItemID::Invalid())
												{
													const bool bSelected = bIsShiftDown ? true : !CurveEditorTreeView->IsItemSelected(CurveEditorTreeItem);
													if (bIsCtrlDown || bSelected)
													{
														CurveEditorTreeView->SetItemSelection(CurveEditorTreeItem, bSelected);
													}
												}
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
		for (const TPair<TWeakObjectPtr<UObject>, FSequencerProxyItem>& SItems : Proxy->SequencerItems)
		{
			if (UObject* Object = SItems.Key.Get())
			{
				for (const FBindingAndTrack& Element : SItems.Value.Bindings)
				{
					EControlRigContextChannelToKey ChannelToKey = Proxy->GetChannelToKeyFromPropertyName(PropertyName);
					TParentFirstChildIterator<IOutlinerExtension> OutlinerExtenstionIt = OutlinerViewModel->GetRootItem()->GetDescendantsOfType<IOutlinerExtension>();
					for (; OutlinerExtenstionIt; ++OutlinerExtenstionIt)
					{
						if (TSharedPtr<FTrackModel> TrackModel = OutlinerExtenstionIt.GetCurrentItem()->FindAncestorOfType<FTrackModel>())
						{
							if (TrackModel->GetTrack() == Element.WeakTrack.Get())
							{
								FName ID = OutlinerExtenstionIt->GetIdentifier();
								FString Name = ID.ToString();
								TArray<FString> StringArray;
								Name.ParseIntoArray(StringArray, TEXT("."));

								FString ChannelName;
								if (StringArray.Num() == 2)
								{
									ChannelName = StringArray[0] + "." + StringArray[1];
								}
								else if (StringArray.Num() == 0)
								{
									ChannelName = StringArray[0];
								}

								EControlRigContextChannelToKey ChannelToKeyFromCurve = Proxy->GetChannelToKeyFromChannelName(ChannelName);
								if (ChannelToKey == ChannelToKeyFromCurve)
								{
									if (TViewModelPtr<ICurveEditorTreeItemExtension> CurveEditorItem = OutlinerExtenstionIt.GetCurrentItem().ImplicitCast())
									{
										FCurveEditorTreeItemID CurveEditorTreeItem = CurveEditorItem->GetCurveEditorItemID();
										if (CurveEditorTreeItem != FCurveEditorTreeItemID::Invalid())
										{
											const bool bSelected = bIsShiftDown ? true : !CurveEditorTreeView->IsItemSelected(CurveEditorTreeItem);
											if (bIsCtrlDown || bSelected)
											{
												CurveEditorTreeView->SetItemSelection(CurveEditorTreeItem, bSelected);
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

bool FAnimDetailValueCustomization::IsMultiple(UControlRigControlsProxy* Proxy,const FName& InPropertyName) const
{
	if (Proxy)
	{
		if (Proxy->IsMultiple(InPropertyName))
		{
			return true;
		}
	}
	return false;
}

TSharedRef<SWidget> FAnimDetailValueCustomization::MakeChildWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle)
{
	const FFieldClass* PropertyClass = PropertyHandle->GetPropertyClass();
	FName PropertyName = PropertyHandle->GetProperty()->GetFName();
	FLinearColor LinearColor = GetColorFromProperty(PropertyName);
	
	if (PropertyClass == FDoubleProperty::StaticClass())
	{
		return MakeDoubleWidget(StructurePropertyHandle, PropertyHandle, LinearColor);
	}
	else if (PropertyClass == FBoolProperty::StaticClass())
	{
		return MakeBoolWidget(StructurePropertyHandle, PropertyHandle, LinearColor);
	}
	else if (PropertyClass == FInt64Property::StaticClass())
	{
		return MakeIntegerWidget(StructurePropertyHandle, PropertyHandle, LinearColor);

	}

	checkf(false, TEXT("Unsupported property class for the Anim Detail Values customization."));
	return SNullWidget::NullWidget;
}


TSharedRef<SWidget> FAnimDetailValueCustomization::MakeIntegerWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle,
	const FLinearColor& LabelColor)
{

	UControlRigControlsProxy* Proxy = GetProxy(StructurePropertyHandle);

	TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;

	return SNew(SNumericEntryBox<int64>)
		.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, WeakHandlePtr)
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Value_Lambda([WeakHandlePtr, this, Proxy]()
			{
				int64 Value = 0;
				bool bIsMultiple = IsMultiple(Proxy, WeakHandlePtr.Pin()->GetProperty()->GetFName());
				return (bIsMultiple == false && WeakHandlePtr.Pin()->GetValue(Value) == FPropertyAccess::Success) ?
					TOptional<int64>(Value) :
					TOptional<int64>();  // Value couldn't be accessed, return an unset value
			})
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
		.OnValueCommitted_Lambda([WeakHandlePtr](int64 Value, ETextCommit::Type)
			{
				WeakHandlePtr.Pin()->SetValue(Value, EPropertyValueSetFlags::DefaultFlags);
			})
		.OnValueChanged_Lambda([this, WeakHandlePtr](int64 Value)
			{
				if (bIsUsingSlider)
				{
					WeakHandlePtr.Pin()->SetValue(Value, EPropertyValueSetFlags::InteractiveChange);
				}
			})
		.OnBeginSliderMovement_Lambda([this]()
			{
				bIsUsingSlider = true;
				GEditor->BeginTransaction(LOCTEXT("SetVectorProperty", "Set Property"));
			})
		.OnEndSliderMovement_Lambda([this](int64 Value)
			{
				bIsUsingSlider = false;
				GEditor->EndTransaction();
			})
			// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
			.AllowSpin(PropertyHandle->GetNumOuterObjects() < 2)
			.LabelPadding(FMargin(3))
			.LabelLocation(SNumericEntryBox<int64>::ELabelLocation::Inside)
			.Label()
			[
				SNumericEntryBox<int64>::BuildNarrowColorLabel(LabelColor)
			];
}

TSharedRef<SWidget> FAnimDetailValueCustomization::MakeBoolWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle,
	const FLinearColor& LabelColor)
{
	UControlRigControlsProxy* Proxy = GetProxy(StructurePropertyHandle);
	TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;
	return
		SNew(SCheckBox)
		.Type(ESlateCheckBoxType::CheckBox)
		.IsChecked_Lambda([WeakHandlePtr, this, Proxy]()->ECheckBoxState
			{
				bool bIsMultiple = IsMultiple(Proxy, WeakHandlePtr.Pin()->GetProperty()->GetFName());
				if (bIsMultiple)
				{
					return ECheckBoxState::Undetermined;
				}

				bool bValue = false;
				WeakHandlePtr.Pin()->GetValue(bValue);
				return bValue ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;

			})
		.OnCheckStateChanged_Lambda([WeakHandlePtr](ECheckBoxState CheckBoxState)
			{
				WeakHandlePtr.Pin()->SetValue(CheckBoxState == ECheckBoxState::Checked, EPropertyValueSetFlags::DefaultFlags);
			});
}

TSharedRef<SWidget> FAnimDetailValueCustomization::MakeDoubleWidget(
	TSharedRef<IPropertyHandle>& StructurePropertyHandle,
	TSharedRef<IPropertyHandle>& PropertyHandle,
	const FLinearColor &LabelColor)
{
	TOptional<double> MinValue, MaxValue, SliderMinValue, SliderMaxValue;
	double SliderExponent, Delta;
	float ShiftMultiplier = 10.f;
	float CtrlMultiplier = 0.1f;
	bool SupportDynamicSliderMaxValue = false;
	bool SupportDynamicSliderMinValue = false;

	UControlRigControlsProxy* Proxy = GetProxy(StructurePropertyHandle);

	ExtractDoubleMetadata(PropertyHandle, MinValue, MaxValue, SliderMinValue, SliderMaxValue, SliderExponent, Delta, ShiftMultiplier, CtrlMultiplier, SupportDynamicSliderMaxValue, SupportDynamicSliderMinValue);

	TWeakPtr<IPropertyHandle> WeakHandlePtr = PropertyHandle;

	return SNew(SNumericEntryBox<double>)
		.IsEnabled(this, &FMathStructCustomization::IsValueEnabled, WeakHandlePtr)
		.EditableTextBoxStyle(&FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
		.Value_Lambda([WeakHandlePtr,this,Proxy ]()
			{
				bool bIsMultiple = IsMultiple(Proxy, WeakHandlePtr.Pin()->GetProperty()->GetFName());
				
				/* wip attempt to try to get sliders working with multiple values,
				one issue is that we need a bIsUsingSliderOnThisProxy, not a global one for al
				if (bIsMultiple && bIsUsingSlider)
				{
					return TOptional<double>(MultipleValue);
				}*/
				double Value = 0.;
				return (bIsMultiple == false && WeakHandlePtr.Pin()->GetValue(Value) == FPropertyAccess::Success) ?
					TOptional<double>(Value) :
					TOptional<double>();  // Value couldn't be accessed, return an unset value
			})
		.Font(IDetailLayoutBuilder::GetDetailFont())
				.UndeterminedString(NSLOCTEXT("PropertyEditor", "MultipleValues", "Multiple Values"))
				.OnValueCommitted_Lambda([WeakHandlePtr](double Value, ETextCommit::Type)
					{
						WeakHandlePtr.Pin()->SetValue(Value, EPropertyValueSetFlags::DefaultFlags);
					})
				.OnValueChanged_Lambda([this, WeakHandlePtr](double Value)
					{
						if (bIsUsingSlider)
						{
							MultipleValue = Value;
							WeakHandlePtr.Pin()->SetValue(Value, EPropertyValueSetFlags::InteractiveChange);
						}
					})
		.OnBeginSliderMovement_Lambda([WeakHandlePtr, this, Proxy]()
			{
				bIsUsingSlider = true;
				GEditor->BeginTransaction(LOCTEXT("SetVectorProperty", "Set Property"));
				bool bIsMultiple = IsMultiple(Proxy, WeakHandlePtr.Pin()->GetProperty()->GetFName());
				if (bIsMultiple)
				{
					MultipleValue = 0.0;
					WeakHandlePtr.Pin()->SetValue(MultipleValue, EPropertyValueSetFlags::DefaultFlags);
				}

			})
		.OnEndSliderMovement_Lambda([this](double Value)
			{
				bIsUsingSlider = false;
				GEditor->EndTransaction();
			})
				// Only allow spin on handles with one object.  Otherwise it is not clear what value to spin
				.AllowSpin(PropertyHandle->GetNumOuterObjects() < 2)
				.ShiftMultiplier(ShiftMultiplier)
				.CtrlMultiplier(CtrlMultiplier)
				.SupportDynamicSliderMaxValue(SupportDynamicSliderMaxValue)
				.SupportDynamicSliderMinValue(SupportDynamicSliderMinValue)
				.OnDynamicSliderMaxValueChanged(this, &FAnimDetailValueCustomization::OnDynamicSliderMaxValueChanged)
				.OnDynamicSliderMinValueChanged(this, &FAnimDetailValueCustomization::OnDynamicSliderMinValueChanged)
				.MinValue(MinValue)
				.MaxValue(MaxValue)
				.MinSliderValue(SliderMinValue)
				.MaxSliderValue(SliderMaxValue)
				.SliderExponent(SliderExponent)
				.LinearDeltaSensitivity(1.0)
				.Delta(Delta)
				.LabelPadding(FMargin(3))
				.LabelLocation(SNumericEntryBox<double>::ELabelLocation::Inside)
				.Label()
				[
					SNumericEntryBox<double>::BuildNarrowColorLabel(LabelColor)
				];
}

// The following code is just a plain copy of FMathStructCustomization which
// would need changes to be able to serve as a base class for this customization.
void FAnimDetailValueCustomization::ExtractDoubleMetadata(TSharedRef<IPropertyHandle>& PropertyHandle, TOptional<double>& MinValue, TOptional<double>& MaxValue, TOptional<double>& SliderMinValue, TOptional<double>& SliderMaxValue, double& SliderExponent, double& Delta, float& ShiftMultiplier, float& CtrlMultiplier, bool& SupportDynamicSliderMaxValue, bool& SupportDynamicSliderMinValue)
{
	FProperty* Property = PropertyHandle->GetProperty();

	const FString& MetaUIMinString = Property->GetMetaData(TEXT("UIMin"));
	const FString& MetaUIMaxString = Property->GetMetaData(TEXT("UIMax"));
	const FString& SliderExponentString = Property->GetMetaData(TEXT("SliderExponent"));
	const FString& DeltaString = Property->GetMetaData(TEXT("Delta"));
	const FString& ShiftMultiplierString = Property->GetMetaData(TEXT("ShiftMultiplier"));
	const FString& CtrlMultiplierString = Property->GetMetaData(TEXT("CtrlMultiplier"));
	const FString& SupportDynamicSliderMaxValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMaxValue"));
	const FString& SupportDynamicSliderMinValueString = Property->GetMetaData(TEXT("SupportDynamicSliderMinValue"));
	const FString& ClampMinString = Property->GetMetaData(TEXT("ClampMin"));
	const FString& ClampMaxString = Property->GetMetaData(TEXT("ClampMax"));

	// If no UIMin/Max was specified then use the clamp string
	const FString& UIMinString = MetaUIMinString.Len() ? MetaUIMinString : ClampMinString;
	const FString& UIMaxString = MetaUIMaxString.Len() ? MetaUIMaxString : ClampMaxString;

	double ClampMin = TNumericLimits<double>::Lowest();
	double ClampMax = TNumericLimits<double>::Max();

	if (!ClampMinString.IsEmpty())
	{
		TTypeFromString<double>::FromString(ClampMin, *ClampMinString);
	}

	if (!ClampMaxString.IsEmpty())
	{
		TTypeFromString<double>::FromString(ClampMax, *ClampMaxString);
	}

	double UIMin = TNumericLimits<double>::Lowest();
	double UIMax = TNumericLimits<double>::Max();
	TTypeFromString<double>::FromString(UIMin, *UIMinString);
	TTypeFromString<double>::FromString(UIMax, *UIMaxString);

	SliderExponent = double(1);

	if (SliderExponentString.Len())
	{
		TTypeFromString<double>::FromString(SliderExponent, *SliderExponentString);
	}

	Delta = double(0);

	if (DeltaString.Len())
	{
		TTypeFromString<double>::FromString(Delta, *DeltaString);
	}

	ShiftMultiplier = 10.f;
	if (ShiftMultiplierString.Len())
	{
		TTypeFromString<float>::FromString(ShiftMultiplier, *ShiftMultiplierString);
	}

	CtrlMultiplier = 0.1f;
	if (CtrlMultiplierString.Len())
	{
		TTypeFromString<float>::FromString(CtrlMultiplier, *CtrlMultiplierString);
	}

	const double ActualUIMin = FMath::Max(UIMin, ClampMin);
	const double ActualUIMax = FMath::Min(UIMax, ClampMax);

	MinValue = ClampMinString.Len() ? ClampMin : TOptional<double>();
	MaxValue = ClampMaxString.Len() ? ClampMax : TOptional<double>();
	SliderMinValue = (UIMinString.Len()) ? ActualUIMin : TOptional<double>();
	SliderMaxValue = (UIMaxString.Len()) ? ActualUIMax : TOptional<double>();

	SupportDynamicSliderMaxValue = SupportDynamicSliderMaxValueString.Len() > 0 && SupportDynamicSliderMaxValueString.ToBool();
	SupportDynamicSliderMinValue = SupportDynamicSliderMinValueString.Len() > 0 && SupportDynamicSliderMinValueString.ToBool();
}

void FAnimDetailValueCustomization::OnDynamicSliderMaxValueChanged(double NewMaxSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfHigher)
{
	for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
	{
		TSharedPtr<SNumericEntryBox<double>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<double>>(Widget.Pin());

		if (NumericBox.IsValid())
		{
			TSharedPtr<SSpinBox<double>> SpinBox = StaticCastSharedPtr<SSpinBox<double>>(NumericBox->GetSpinBox());

			if (SpinBox.IsValid())
			{
				if (SpinBox != InValueChangedSourceWidget)
				{
					if ((NewMaxSliderValue > SpinBox->GetMaxSliderValue() && UpdateOnlyIfHigher) || !UpdateOnlyIfHigher)
					{
						// Make sure the max slider value is not a getter otherwise we will break the link!
						verifySlow(!SpinBox->IsMaxSliderValueBound());
						SpinBox->SetMaxSliderValue(NewMaxSliderValue);
					}
				}
			}
		}
	}

	if (IsOriginator)
	{
		OnNumericEntryBoxDynamicSliderMaxValueChanged.Broadcast((double)NewMaxSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfHigher);
	}
}

void FAnimDetailValueCustomization::OnDynamicSliderMinValueChanged(double NewMinSliderValue, TWeakPtr<SWidget> InValueChangedSourceWidget, bool IsOriginator, bool UpdateOnlyIfLower)
{
	for (TWeakPtr<SWidget>& Widget : NumericEntryBoxWidgetList)
	{
		TSharedPtr<SNumericEntryBox<double>> NumericBox = StaticCastSharedPtr<SNumericEntryBox<double>>(Widget.Pin());

		if (NumericBox.IsValid())
		{
			TSharedPtr<SSpinBox<double>> SpinBox = StaticCastSharedPtr<SSpinBox<double>>(NumericBox->GetSpinBox());

			if (SpinBox.IsValid())
			{
				if (SpinBox != InValueChangedSourceWidget)
				{
					if ((NewMinSliderValue < SpinBox->GetMinSliderValue() && UpdateOnlyIfLower) || !UpdateOnlyIfLower)
					{
						// Make sure the min slider value is not a getter otherwise we will break the link!
						verifySlow(!SpinBox->IsMinSliderValueBound());
						SpinBox->SetMinSliderValue(NewMinSliderValue);
					}
				}
			}
		}
	}

	if (IsOriginator)
	{
		OnNumericEntryBoxDynamicSliderMinValueChanged.Broadcast((double)NewMinSliderValue, InValueChangedSourceWidget, false, UpdateOnlyIfLower);
	}
}

FAnimDetailProxyDetails::FAnimDetailProxyDetails(const FName& InCategoryName)
{
	CategoryName = InCategoryName;
}

TSharedRef<IDetailCustomization> FAnimDetailProxyDetails::MakeInstance(const FName& InCategoryName)
{
	return MakeShareable(new FAnimDetailProxyDetails(InCategoryName));
}

void FAnimDetailProxyDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<UControlRigControlsProxy*> ControlProxies;

	TArray<TWeakObjectPtr<UObject> > EditedObjects;
	DetailBuilder.GetObjectsBeingCustomized(EditedObjects);
	for (int32 i = 0; i < EditedObjects.Num(); i++)
	{
		UControlRigControlsProxy* Proxy = Cast<UControlRigControlsProxy>(EditedObjects[i].Get());
		if (Proxy)
		{
			ControlProxies.Add(Proxy);
		}
	}
	FText Name;
	if (ControlProxies.Num() == 1)
	{
		Name = FText::FromName(ControlProxies[0]->GetName());
		ControlProxies[0]->UpdatePropertyNames(DetailBuilder);
	}
	else
	{
		FString DisplayString = TEXT("Multiple");
		Name = FText::FromString(*DisplayString);
		
	}
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, Name, ECategoryPriority::Important);
		
	//add custom attributes
	TArray<UControlRigControlsProxy*> NestedProxies;
	for (UControlRigControlsProxy* Proxy : ControlProxies)
	{
		if (Proxy)
		{
			for (UControlRigControlsProxy* ChildProxy : Proxy->ChildProxies)
			{
				if (ChildProxy)
				{
					NestedProxies.Add(ChildProxy);
				}
			}
		}
	}
	IDetailCategoryBuilder& AttributesCategory = DetailBuilder.EditCategory("Attributes");

	if (NestedProxies.Num() > 0)
	{
		AttributesCategory.SetCategoryVisibility(true);
		for (UControlRigControlsProxy* Proxy: NestedProxies)
		{
			if (Proxy)
			{
				TArray<UObject*> ExternalObjects;
				ExternalObjects.Add(Proxy);

				FAddPropertyParams Params;
				Params.CreateCategoryNodes(false);
				Params.HideRootObjectNode(true);
				IDetailPropertyRow* NestedRow = AttributesCategory.AddExternalObjects(
					ExternalObjects,
					EPropertyLocation::Default,
					Params);
			}
		}
	}
	else
	{
		AttributesCategory.SetCategoryVisibility(true);
	}
}

#undef LOCTEXT_NAMESPACE
