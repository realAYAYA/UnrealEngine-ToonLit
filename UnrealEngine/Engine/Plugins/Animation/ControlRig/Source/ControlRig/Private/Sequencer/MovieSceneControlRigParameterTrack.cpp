// Copyright Epic Games, Inc. All Rights Reserved.

#include "Sequencer/MovieSceneControlRigParameterTrack.h"
#include "Compilation/MovieSceneCompilerRules.h"
#include "Evaluation/MovieSceneEvaluationTrack.h"
#include "MovieSceneCommonHelpers.h"
#include "Sequencer/MovieSceneControlRigParameterTemplate.h"
#include "MovieScene.h"
#include "MovieSceneTimeHelpers.h"
#include "Channels/MovieSceneChannelProxy.h"
#include "Rigs/FKControlRig.h"
#include "Rigs/RigHierarchyController.h"
#include "UObject/Package.h"
#include "Async/Async.h"

#if WITH_EDITOR
#include "ScopedTransaction.h"
#endif//WITH_EDITOR

#include UE_INLINE_GENERATED_CPP_BY_NAME(MovieSceneControlRigParameterTrack)

#define LOCTEXT_NAMESPACE "MovieSceneParameterControlRigTrack"

FColor UMovieSceneControlRigParameterTrack::AbsoluteRigTrackColor = FColor(65, 89, 194, 65);
FColor UMovieSceneControlRigParameterTrack::LayeredRigTrackColor = FColor(173, 151, 114);

FControlRotationOrder::FControlRotationOrder()
	:RotationOrder(EEulerRotationOrder::YZX),
	bOverrideSetting(false)

{

}

UMovieSceneControlRigParameterTrack::UMovieSceneControlRigParameterTrack(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ControlRig(nullptr)
	, PriorityOrder(INDEX_NONE)
{
#if WITH_EDITORONLY_DATA
	TrackTint = AbsoluteRigTrackColor;
#endif

	SupportedBlendTypes = FMovieSceneBlendTypeField::None();
	SupportedBlendTypes.Add(EMovieSceneBlendType::Additive);
	SupportedBlendTypes.Add(EMovieSceneBlendType::Absolute);

}

void UMovieSceneControlRigParameterTrack::BeginDestroy()
{
	Super::BeginDestroy();
	if (IsValid(ControlRig))
	{
		ControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
	}
}

FMovieSceneEvalTemplatePtr UMovieSceneControlRigParameterTrack::CreateTemplateForSection(const UMovieSceneSection& InSection) const
{
	return FMovieSceneControlRigParameterTemplate(*CastChecked<UMovieSceneControlRigParameterSection>(&InSection), *this);
}

bool UMovieSceneControlRigParameterTrack::SupportsType(TSubclassOf<UMovieSceneSection> SectionClass) const
{
	return SectionClass == UMovieSceneControlRigParameterSection::StaticClass();
}

UMovieSceneSection* UMovieSceneControlRigParameterTrack::CreateNewSection()
{
	UMovieSceneControlRigParameterSection* NewSection = NewObject<UMovieSceneControlRigParameterSection>(this, NAME_None, RF_Transactional);
	NewSection->SetControlRig(ControlRig);
	bool bSetDefault = false;
	if (Sections.Num() == 0)
	{
		NewSection->SetBlendType(EMovieSceneBlendType::Absolute);
		bSetDefault = true;
	}
	else
	{
		NewSection->SetBlendType(EMovieSceneBlendType::Additive);
	}

	NewSection->SpaceChannelAdded().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnSpaceAdded);
	if (!NewSection->ConstraintChannelAdded().IsBoundToObject(this))
	{
		NewSection->ConstraintChannelAdded().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnConstraintAdded);
	}

	if (ControlRig)
	{
		NewSection->RecreateWithThisControlRig(ControlRig,bSetDefault);
	}
	return  NewSection;
}

void UMovieSceneControlRigParameterTrack::HandleOnSpaceAdded(UMovieSceneControlRigParameterSection* Section, const FName& InControlName, FMovieSceneControlRigSpaceChannel* Channel)
{
	OnSpaceChannelAdded.Broadcast(Section, InControlName, Channel);

	Channel->OnSpaceNoLongerUsed().RemoveAll(this);
	Channel->OnSpaceNoLongerUsed().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnSpaceNoLongerUsed, InControlName);
}

void UMovieSceneControlRigParameterTrack::HandleOnSpaceNoLongerUsed(FMovieSceneControlRigSpaceChannel* InChannel, const TArray<FRigElementKey>& InSpaces, FName InControlName)
{
	if (InChannel && GetControlRig())
	{
		if(URigHierarchy* Hierarchy = GetControlRig()->GetHierarchy())
		{
			if(URigHierarchyController* Controller = Hierarchy->GetController())
			{
				const FRigElementKey ControlKey(InControlName, ERigElementType::Control);
				if(Hierarchy->Find<FRigControlElement>(ControlKey))
				{
					FRigElementKey DefaultParent = Hierarchy->GetFirstParent(ControlKey);
					for(const FRigElementKey& ParentToRemove : InSpaces)
					{
						if(DefaultParent != ParentToRemove)
						{
							Controller->RemoveParent(ControlKey, ParentToRemove, false, false, false);
						}
					}
				}
			}
		}
	}
}

void UMovieSceneControlRigParameterTrack::HandleOnConstraintAdded(
	IMovieSceneConstrainedSection* InSection,
	FMovieSceneConstraintChannel* InChannel) const
{
	OnConstraintChannelAdded.Broadcast(InSection, InChannel);
}

void UMovieSceneControlRigParameterTrack::RemoveAllAnimationData()
{
	Sections.Empty();
	SectionToKey = nullptr;
}

bool UMovieSceneControlRigParameterTrack::HasSection(const UMovieSceneSection& Section) const
{
	return Sections.Contains(&Section);
}

void UMovieSceneControlRigParameterTrack::AddSection(UMovieSceneSection& Section)
{
	Sections.Add(&Section);
	if (UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(&Section))
	{
		if (CRSection->GetControlRig() != ControlRig)
		{
			CRSection->SetControlRig(ControlRig);
		}
		CRSection->ReconstructChannelProxy();
	}

	if (Sections.Num() > 1)
	{
		SetSectionToKey(&Section);
	}
}

void UMovieSceneControlRigParameterTrack::RemoveSection(UMovieSceneSection& Section)
{
	Sections.Remove(&Section);
	if (SectionToKey == &Section)
	{
		if (Sections.Num() > 0)
		{
			SectionToKey = Sections[0];
		}
		else
		{
			SectionToKey = nullptr;
		}
	}
}

void UMovieSceneControlRigParameterTrack::RemoveSectionAt(int32 SectionIndex)
{
	bool bResetSectionToKey = (SectionToKey == Sections[SectionIndex]);

	Sections.RemoveAt(SectionIndex);

	if (bResetSectionToKey)
	{
		SectionToKey = Sections.Num() > 0 ? Sections[0] : nullptr;
	}
}

bool UMovieSceneControlRigParameterTrack::IsEmpty() const
{
	return Sections.Num() == 0;
}

const TArray<UMovieSceneSection*>& UMovieSceneControlRigParameterTrack::GetAllSections() const
{
	return Sections;
}


#if WITH_EDITORONLY_DATA
FText UMovieSceneControlRigParameterTrack::GetDefaultDisplayName() const
{
	return LOCTEXT("DisplayName", "Control Rig Parameter");
}
#endif


UMovieSceneSection* UMovieSceneControlRigParameterTrack::CreateControlRigSection(FFrameNumber StartTime, UControlRig* InControlRig, bool bInOwnsControlRig)
{
	if (InControlRig == nullptr)
	{
		return nullptr;
	}
	if (!bInOwnsControlRig)
	{
		InControlRig->Rename(nullptr, this);
	}

	if (IsValid(ControlRig))
	{
		ControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
	}

	ControlRig = InControlRig;

	ControlRig->OnPostConstruction_AnyThread().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnPostConstructed);

	UMovieSceneControlRigParameterSection* NewSection = Cast<UMovieSceneControlRigParameterSection>(CreateNewSection());

	UMovieScene* OuterMovieScene = GetTypedOuter<UMovieScene>();
	NewSection->SetRange(TRange<FFrameNumber>::All());

	//mz todo tbd maybe just set it to animated range? TRange<FFrameNumber> Range = OuterMovieScene->GetPlaybackRange();
	//Range.SetLowerBoundValue(StartTime);
	//NewSection->SetRange(Range);

	AddSection(*NewSection);

	return NewSection;
}

TArray<UMovieSceneSection*, TInlineAllocator<4>> UMovieSceneControlRigParameterTrack::FindAllSections(FFrameNumber Time)
{
	TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections;

	for (UMovieSceneSection* Section : Sections)
	{
		if (MovieSceneHelpers::IsSectionKeyable(Section) && Section->GetRange().Contains(Time))
		{
			OverlappingSections.Add(Section);
		}
	}

	Algo::Sort(OverlappingSections, MovieSceneHelpers::SortOverlappingSections);

	return OverlappingSections;
}


UMovieSceneSection* UMovieSceneControlRigParameterTrack::FindSection(FFrameNumber Time)
{
	TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections = FindAllSections(Time);

	if (OverlappingSections.Num())
	{
		if (SectionToKey && OverlappingSections.Contains(SectionToKey))
		{
			return SectionToKey;
		}
		else
		{
			return OverlappingSections[0];
		}
	}

	return nullptr;
}


UMovieSceneSection* UMovieSceneControlRigParameterTrack::FindOrExtendSection(FFrameNumber Time, float& Weight)
{
	Weight = 1.0f;
	TArray<UMovieSceneSection*, TInlineAllocator<4>> OverlappingSections = FindAllSections(Time);
	if (SectionToKey && MovieSceneHelpers::IsSectionKeyable(SectionToKey))
	{
		bool bCalculateWeight = false;
		if (!OverlappingSections.Contains(SectionToKey))
		{
			if (SectionToKey->HasEndFrame() && SectionToKey->GetExclusiveEndFrame() <= Time)
			{
				if (SectionToKey->GetExclusiveEndFrame() != Time)
				{
					SectionToKey->SetEndFrame(Time);
				}
			}
			else
			{
				SectionToKey->SetStartFrame(Time);
			}
			if (OverlappingSections.Num() > 0)
			{
				bCalculateWeight = true;
			}
		}
		else
		{
			if (OverlappingSections.Num() > 1)
			{
				bCalculateWeight = true;
			}
		}
		//we need to calculate weight also possibly
		FOptionalMovieSceneBlendType BlendType = SectionToKey->GetBlendType();
		if (bCalculateWeight)
		{
			Weight = MovieSceneHelpers::CalculateWeightForBlending(SectionToKey, Time);
		}
		return SectionToKey;
	}
	else
	{
		if (OverlappingSections.Num() > 0)
		{
			return OverlappingSections[0];
		}
	}

	// Find a spot for the section so that they are sorted by start time
	for (int32 SectionIndex = 0; SectionIndex < Sections.Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = Sections[SectionIndex];

		// Check if there are no more sections that would overlap the time 
		if (!Sections.IsValidIndex(SectionIndex + 1) || (Sections[SectionIndex + 1]->HasEndFrame() && Sections[SectionIndex + 1]->GetExclusiveEndFrame() > Time))
		{
			// No sections overlap the time

			if (SectionIndex > 0)
			{
			// Append and grow the previous section
			UMovieSceneSection* PreviousSection = Sections[SectionIndex ? SectionIndex - 1 : 0];

			PreviousSection->SetEndFrame(Time);
			return PreviousSection;
			}
			else if (Sections.IsValidIndex(SectionIndex + 1))
			{
			// Prepend and grow the next section because there are no sections before this one
			UMovieSceneSection* NextSection = Sections[SectionIndex + 1];
			NextSection->SetStartFrame(Time);
			return NextSection;
			}
			else
			{
			// SectionIndex == 0 
			UMovieSceneSection* PreviousSection = Sections[0];
			if (PreviousSection->HasEndFrame() && PreviousSection->GetExclusiveEndFrame() <= Time)
			{
				// Append and grow the section
				if (PreviousSection->GetExclusiveEndFrame() != Time)
				{
					PreviousSection->SetEndFrame(Time);
				}
			}
			else
			{
				// Prepend and grow the section
				PreviousSection->SetStartFrame(Time);
			}
			return PreviousSection;
			}
		}
	}

	return nullptr;
}

UMovieSceneSection* UMovieSceneControlRigParameterTrack::FindOrAddSection(FFrameNumber Time, bool& bSectionAdded)
{
	bSectionAdded = false;

	UMovieSceneSection* FoundSection = FindSection(Time);
	if (FoundSection)
	{
		return FoundSection;
	}

	// Add a new section that starts and ends at the same time
	UMovieSceneSection* NewSection = CreateNewSection();
	ensureAlwaysMsgf(NewSection->HasAnyFlags(RF_Transactional), TEXT("CreateNewSection must return an instance with RF_Transactional set! (pass RF_Transactional to NewObject)"));
	NewSection->SetFlags(RF_Transactional);
	NewSection->SetRange(TRange<FFrameNumber>::Inclusive(Time, Time));

	Sections.Add(NewSection);

	bSectionAdded = true;

	return NewSection;
}

void UMovieSceneControlRigParameterTrack::SetSectionToKey(UMovieSceneSection* InSection)
{
	SectionToKey = InSection;
}

UMovieSceneSection* UMovieSceneControlRigParameterTrack::GetSectionToKey() const
{
	if (SectionToKey)
	{
		return SectionToKey;
	}
	else if(Sections.Num() >0)
	{
		return Sections[0];
	}
	return nullptr;
}

void UMovieSceneControlRigParameterTrack::ReconstructControlRig()
{
	if (ControlRig && !ControlRig->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedInitialization))
	{
		ControlRig->ConditionalPostLoad();
		ControlRig->Initialize();
		for (UMovieSceneSection* Section : Sections)
		{
			if (Section)
			{
				UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section);
				if (CRSection)
				{
					if (CRSection->SpaceChannelAdded().IsBoundToObject(this) == false)
					{
						CRSection->SpaceChannelAdded().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnSpaceAdded);
					}

					if (!CRSection->ConstraintChannelAdded().IsBoundToObject(this))
					{
						CRSection->ConstraintChannelAdded().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnConstraintAdded);
					}
					
					CRSection->RecreateWithThisControlRig(ControlRig, CRSection->GetBlendType() == EMovieSceneBlendType::Absolute);
				}
			}
		}
	}
}

void UMovieSceneControlRigParameterTrack::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	FCoreUObjectDelegates::OnEndLoadPackage.AddUObject(this, &UMovieSceneControlRigParameterTrack::HandlePackageDone);
	// If we have a control Rig and it's not a native one, register OnEndLoadPackage callback on instance directly
	if (ControlRig && !ControlRig->GetClass()->IsNative())
	{
		ControlRig->OnEndLoadPackage().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleControlRigPackageDone);
	}
	// Otherwise try and reconstruct the control rig directly (which is fine for native classes)
	else
#endif
	{		
		ReconstructControlRig();
	}

	if (IsValid(ControlRig))
	{
		ControlRig->OnPostConstruction_AnyThread().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnPostConstructed);
	}
}

//mz todo this is coming from BuildPatchServices in Runtime/Online/BuildPatchServices/Online/Core/AsyncHelper.h
//looking at moving this over
//this is very useful since it properly handles passing in a this pointer to the async task.
namespace MovieSceneControlRigTrack
{
	/**
	 * Helper functions for wrapping async functionality.
	 */
	namespace AsyncHelpers
	{
		template<typename ResultType, typename... Args>
		static TFunction<void()> MakePromiseKeeper(const TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe>& Promise, const TFunction<ResultType(Args...)>& Function, Args... FuncArgs)
		{
			return [Promise, Function, FuncArgs...]()
			{
				Promise->SetValue(Function(FuncArgs...));
			};
		}

		template<typename... Args>
		static TFunction<void()> MakePromiseKeeper(const TSharedRef<TPromise<void>, ESPMode::ThreadSafe>& Promise, const TFunction<void(Args...)>& Function, Args... FuncArgs)
		{
			return [Promise, Function, FuncArgs...]()
			{
				Function(FuncArgs...);
				Promise->SetValue();
			};
		}

		template<typename ResultType, typename... Args>
		static TFuture<ResultType> ExecuteOnGameThread(const TFunction<ResultType(Args...)>& Function, Args... FuncArgs)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function, FuncArgs...);
			if (!IsInGameThread())
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}

		template<typename ResultType>
		static TFuture<ResultType> ExecuteOnGameThread(const TFunction<ResultType()>& Function)
		{
			TSharedRef<TPromise<ResultType>, ESPMode::ThreadSafe> Promise = MakeShareable(new TPromise<ResultType>());
			TFunction<void()> PromiseKeeper = MakePromiseKeeper(Promise, Function);
			if (!IsInGameThread())
			{
				AsyncTask(ENamedThreads::GameThread, MoveTemp(PromiseKeeper));
			}
			else
			{
				PromiseKeeper();
			}
			return Promise->GetFuture();
		}
	}
}

void UMovieSceneControlRigParameterTrack::HandleOnPostConstructed_GameThread()
{
	if (IsValid(ControlRig))
	{
		TArray<FRigControlElement*> SortedControls;
		ControlRig->GetControlsInOrder(SortedControls);
#if WITH_EDITOR
		const FScopedTransaction PostConstructTransation(NSLOCTEXT("ControlRig", "PostConstructTransation", "Post Construct"), !GIsTransacting);
#endif		
		bool bSectionWasDifferent = false;
		for (UMovieSceneSection* BaseSection : GetAllSections())
		{
			if (UMovieSceneControlRigParameterSection* Section = Cast<UMovieSceneControlRigParameterSection>(BaseSection))
			{
				if (Section->IsDifferentThanLastControlsUsedToReconstruct(SortedControls))
				{
					Section->RecreateWithThisControlRig(ControlRig, Section->GetBlendType() == EMovieSceneBlendType::Absolute);
					bSectionWasDifferent = true;
				}
			}
		}
		if (bSectionWasDifferent)
		{
			BroadcastChanged();
		}
		if (SortedControls.Num() > 0) //really set up
		{
			TArray<FName> Names = GetControlsWithDifferentRotationOrders();
			ResetControlsToSettingsRotationOrder(Names);
		}
	}
}

void UMovieSceneControlRigParameterTrack::HandleOnPostConstructed(UControlRig* Subject, const FName& InEventName)
{
	if(IsInGameThread())
	{
		HandleOnPostConstructed_GameThread();

	}
}

#if WITH_EDITORONLY_DATA
void UMovieSceneControlRigParameterTrack::DeclareConstructClasses(TArray<FTopLevelAssetPath>& OutConstructClasses, const UClass* SpecificSubclass)
{
	Super::DeclareConstructClasses(OutConstructClasses, SpecificSubclass);
	OutConstructClasses.Add(FTopLevelAssetPath(UMovieSceneSection::StaticClass()));
	OutConstructClasses.Add(FTopLevelAssetPath(UMovieSceneControlRigParameterSection::StaticClass()));
}
#endif

#if WITH_EDITOR
void UMovieSceneControlRigParameterTrack::HandlePackageDone(const FEndLoadPackageContext& Context)
{
	if (!GetPackage()->GetHasBeenEndLoaded())
	{
		return;
	}

	// ensure both packages are fully end-loaded
	if (ControlRig && !ControlRig->GetClass()->IsNative())
	{
		if (const UPackage* ControlRigPackage = Cast<UPackage>(ControlRig->GetClass()->GetOutermost()))
		{
			if (!ControlRigPackage->GetHasBeenEndLoaded())
			{
				return;
			}
		}

		// Only reconstruct in case it is not a native ControlRig class
		ReconstructControlRig();
	}

	FCoreUObjectDelegates::OnEndLoadPackage.RemoveAll(this);
}

void UMovieSceneControlRigParameterTrack::HandleControlRigPackageDone(URigVMHost* InControlRig)
{
	if (ensure(ControlRig == InControlRig))
	{
		ControlRig->OnEndLoadPackage().RemoveAll(this);
		ReconstructControlRig();
	}
}
#endif


void UMovieSceneControlRigParameterTrack::PostEditImport()
{
	Super::PostEditImport();
	if (ControlRig)
	{
		if (ControlRig->OnInitialized_AnyThread().IsBoundToObject(this) == false)
		{
			ControlRig->OnPostConstruction_AnyThread().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnPostConstructed);
		}
		ControlRig->ClearFlags(RF_Transient); //when copied make sure it's no longer transient, sequencer does this for tracks/sections 
											  //but not for all objects in them since the control rig itself has transient objects.
	}
	ReconstructControlRig();
}

void UMovieSceneControlRigParameterTrack::RenameParameterName(const FName& OldParameterName, const FName& NewParameterName)
{
	if (OldParameterName != NewParameterName)
	{
		for (UMovieSceneSection* Section : Sections)
		{
			if (UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section))
			{
				CRSection->RenameParameterName(OldParameterName, NewParameterName);
			}
		}
		if (FControlRotationOrder* RotationOrder = ControlsRotationOrder.Find(OldParameterName))
		{
			FControlRotationOrder NewRotationOrder = *RotationOrder;
			ControlsRotationOrder.Remove(OldParameterName);
			ControlsRotationOrder.Add(NewParameterName, NewRotationOrder);
		}
	}
}

void UMovieSceneControlRigParameterTrack::ReplaceControlRig(UControlRig* NewControlRig, bool RecreateChannels)
{
	if (IsValid(ControlRig))
	{
		ControlRig->OnPostConstruction_AnyThread().RemoveAll(this);
	}

	ControlRig = NewControlRig;

	if (IsValid(ControlRig))
	{
		ControlRig->OnPostConstruction_AnyThread().AddUObject(this, &UMovieSceneControlRigParameterTrack::HandleOnPostConstructed);

		if (ControlRig->GetOuter() != this)
		{
			ControlRig->Rename(nullptr, this);
		}
	}
	for (UMovieSceneSection* Section : Sections)
	{
		if (UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section))
		{
			if (RecreateChannels)
			{
				CRSection->RecreateWithThisControlRig(NewControlRig, CRSection->GetBlendType() == EMovieSceneBlendType::Absolute);
			}
			else
			{
				CRSection->SetControlRig(NewControlRig);
			}
		}
	}
}
TOptional<EEulerRotationOrder> UMovieSceneControlRigParameterTrack::GetControlRotationOrder(const FRigControlElement* ControlElement,
	bool bCurrent) const
{
	TOptional<EEulerRotationOrder> Order;
	if (bCurrent)
	{
		const FControlRotationOrder* RotationOrder = ControlsRotationOrder.Find(ControlElement->GetFName());
		if (RotationOrder)
		{
			Order = RotationOrder->RotationOrder;
		}
	}
	else //use setting
	{
		if (ControlRig->GetHierarchy()->GetUsePreferredRotationOrder(ControlElement))
		{
			Order = ControlRig->GetHierarchy()->GetControlPreferredEulerRotationOrder(ControlElement);
		}
	}
	return Order;
}

TArray<FName> UMovieSceneControlRigParameterTrack::GetControlsWithDifferentRotationOrders() const
{
	TArray<FName> Names;
	if (ControlRig && ControlRig->GetHierarchy())
	{
		URigHierarchy* Hierarchy = ControlRig->GetHierarchy();
		TArray<FRigControlElement*> SortedControls;
		ControlRig->GetControlsInOrder(SortedControls);
		for (const FRigControlElement* ControlElement : SortedControls)
		{
			if (!Hierarchy->IsAnimatable(ControlElement))
			{
				continue;
			}
			TOptional<EEulerRotationOrder> Current = GetControlRotationOrder(ControlElement, true);
			TOptional<EEulerRotationOrder> Setting = GetControlRotationOrder(ControlElement, false);
			if (Current != Setting)
			{
				Names.Add(ControlElement->GetFName());
			}

		}
	}
	return Names;
}

void UMovieSceneControlRigParameterTrack::ResetControlsToSettingsRotationOrder(const TArray<FName>& Names, EMovieSceneKeyInterpolation Interpolation)
{
	if (ControlRig && ControlRig->GetHierarchy())
	{
		for (const FName& Name : Names)
		{
			if (FRigControlElement* ControlElement = GetControlRig()->FindControl(Name))
			{

				TOptional<EEulerRotationOrder> Current = GetControlRotationOrder(ControlElement, true);
				TOptional<EEulerRotationOrder> Setting = GetControlRotationOrder(ControlElement, false);
				if (Current != Setting)
				{
					ChangeControlRotationOrder(Name, Setting, Interpolation);
				}
			}
		}
	}
}
void UMovieSceneControlRigParameterTrack::ChangeControlRotationOrder(const FName& InControlName, const TOptional<EEulerRotationOrder>& NewOrder, EMovieSceneKeyInterpolation Interpolation)
{
	if (ControlRig && ControlRig->GetHierarchy())
	{
		if (FRigControlElement* ControlElement = GetControlRig()->FindControl(InControlName))
		{
			TOptional<EEulerRotationOrder> Current = GetControlRotationOrder(ControlElement, true);
			if (Current != NewOrder)
			{
				if (NewOrder.IsSet())
				{
					FControlRotationOrder& RotationOrder = ControlsRotationOrder.FindOrAdd(InControlName);
					RotationOrder.RotationOrder = NewOrder.GetValue();
					TOptional<EEulerRotationOrder> Setting = GetControlRotationOrder(ControlElement, false);
					if (Setting != NewOrder)
					{
						RotationOrder.bOverrideSetting = true;
					}
				}
				else //no longer set so just remove
				{
					ControlsRotationOrder.Remove(InControlName);
				}
				for (UMovieSceneSection* Section : Sections)
				{
					if (UMovieSceneControlRigParameterSection* CRSection = Cast<UMovieSceneControlRigParameterSection>(Section))
					{
						CRSection->ChangeControlRotationOrder(InControlName, Current, NewOrder, Interpolation);
					}
				}
			}
		}
	}	
}

void UMovieSceneControlRigParameterTrack::GetSelectedNodes(TArray<FName>& SelectedControlNames)
{
	if (GetControlRig())
	{
		SelectedControlNames = GetControlRig()->CurrentControlSelection();
	}
}

int32 UMovieSceneControlRigParameterTrack::GetPriorityOrder() const
{
	return PriorityOrder;
}

void UMovieSceneControlRigParameterTrack::SetPriorityOrder(int32 InPriorityIndex)
{
	if (InPriorityIndex >= 0)
	{
		PriorityOrder = InPriorityIndex;
	}
	else
	{
		PriorityOrder = 0;
	}
}

#if WITH_EDITOR
bool UMovieSceneControlRigParameterTrack::GetFbxCurveDataFromChannelMetadata(const FMovieSceneChannelMetaData& MetaData, FControlRigFbxCurveData& OutCurveData)
{
	const FString ChannelName = MetaData.Name.ToString();
	TArray<FString> ChannelParts;

	// The channel has an attribute
	if (ChannelName.ParseIntoArray(ChannelParts, TEXT(".")) > 1)
	{
		// Retrieve the attribute
		OutCurveData.AttributeName = ChannelParts.Last();
		
		// The control name (left part) will be used as the node name
		OutCurveData.NodeName = ChannelParts[0];
		OutCurveData.ControlName = *OutCurveData.NodeName;

		// The channel has 3 parts, the middle one (i.e. Location) will be treated as the property name
		if (ChannelParts.Num() > 2)
		{
			OutCurveData.AttributePropertyName = ChannelParts[1];
		}
	}
	// The channel does not have an attribute
	else
	{
		// Thus no property above the attribute
		OutCurveData.AttributePropertyName.Empty();
		
		// The channel group will be used as the node name (name of the control this channel is grouped under - i.e. for animation channels)
		OutCurveData.NodeName = MetaData.Group.ToString();

		// The channel name will be used as the control name and attribute name (i.e. Weight)
		OutCurveData.ControlName = *ChannelName;
		OutCurveData.AttributeName = OutCurveData.ControlName.ToString();
	}

	if (OutCurveData.NodeName.IsEmpty() || OutCurveData.AttributeName.IsEmpty())
	{
		return false;
	}
	
	// Retrieve the control type
	if (GetControlRig())
	{
		if (FRigControlElement* Control = GetControlRig()->FindControl(OutCurveData.ControlName))
		{
			OutCurveData.ControlType = (FFBXControlRigTypeProxyEnum)(uint8)Control->Settings.ControlType;
			return true;
		}
	}
	return false;
}
#endif

UControlRig* UMovieSceneControlRigParameterTrack::GetGameWorldControlRig(UWorld* InWorld) 
{
	if (GameWorldControlRigs.Find(InWorld) == nullptr && ControlRig)
	{
		UControlRig* NewGameWorldControlRig = NewObject<UControlRig>(this, ControlRig->GetClass(), NAME_None, RF_Transient);
		NewGameWorldControlRig->Initialize();
		if (UFKControlRig* FKControlRig = Cast<UFKControlRig>(Cast<UControlRig>(ControlRig)))
		{
			if (UFKControlRig* NewFKControlRig = Cast<UFKControlRig>(Cast<UControlRig>(NewGameWorldControlRig)))
			{
				NewFKControlRig->SetApplyMode(FKControlRig->GetApplyMode());
			}
		}
		else
		{
			NewGameWorldControlRig->SetIsAdditive(ControlRig->IsAdditive());
		}
		GameWorldControlRigs.Add(InWorld, NewGameWorldControlRig);
	}
	TObjectPtr<UControlRig> * GameWorldControlRig = GameWorldControlRigs.Find(InWorld);
	if (GameWorldControlRig != nullptr)
	{
		return GameWorldControlRig->Get();
	}
	return nullptr;
}

TArray<FRigControlFBXNodeAndChannels>* UMovieSceneControlRigParameterTrack::GetNodeAndChannelMappings(UMovieSceneSection* InSection )
{
#if WITH_EDITOR
	if (GetControlRig() == nullptr)
	{
		return nullptr;
	}
	bool bSectionAdded;
	//use passed in section if available, else section to key if available, else first section or create one.
	UMovieSceneControlRigParameterSection* CurrentSectionToKey = InSection ? Cast<UMovieSceneControlRigParameterSection>(InSection) : Cast<UMovieSceneControlRigParameterSection>(GetSectionToKey());
	if (CurrentSectionToKey == nullptr)
	{
		CurrentSectionToKey = Cast<UMovieSceneControlRigParameterSection>(FindOrAddSection(0, bSectionAdded));
	} 
	if (!CurrentSectionToKey)
	{
		return nullptr;
	}

	const FName DoubleChannelTypeName = FMovieSceneDoubleChannel::StaticStruct()->GetFName();
	const FName FloatChannelTypeName = FMovieSceneFloatChannel::StaticStruct()->GetFName();
	const FName BoolChannelTypeName = FMovieSceneBoolChannel::StaticStruct()->GetFName();
	const FName EnumChannelTypeName = FMovieSceneByteChannel::StaticStruct()->GetFName();
	const FName IntegerChannelTypeName = FMovieSceneIntegerChannel::StaticStruct()->GetFName();

	// Our resulting mapping containing FBX node & UE channels data for each control
	TArray<FRigControlFBXNodeAndChannels>* NodeAndChannels = new TArray<FRigControlFBXNodeAndChannels>();
	
	FMovieSceneChannelProxy& ChannelProxy = CurrentSectionToKey->GetChannelProxy();
	for (const FMovieSceneChannelEntry& Entry : CurrentSectionToKey->GetChannelProxy().GetAllEntries())
	{
		const FName ChannelTypeName = Entry.GetChannelTypeName();
		if (ChannelTypeName != DoubleChannelTypeName && ChannelTypeName != FloatChannelTypeName && ChannelTypeName != BoolChannelTypeName
			&& ChannelTypeName != EnumChannelTypeName && ChannelTypeName != IntegerChannelTypeName)
		{
			continue;
		}

		TArrayView<FMovieSceneChannel* const> Channels = Entry.GetChannels();
		TArrayView<const FMovieSceneChannelMetaData> AllMetaData = Entry.GetMetaData();

		for (int32 Index = 0; Index < Channels.Num(); ++Index)
		{
			FMovieSceneChannelHandle Channel = ChannelProxy.MakeHandle(ChannelTypeName, Index);
			const FMovieSceneChannelMetaData& MetaData = AllMetaData[Index];

			FControlRigFbxCurveData FbxCurveData;
			if (!GetFbxCurveDataFromChannelMetadata(MetaData, FbxCurveData))
			{
				continue;
			}

			// Retrieve the current control node, usually the last one but not given
			FRigControlFBXNodeAndChannels* CurrentNodeAndChannel;

			const int i = NodeAndChannels->FindLastByPredicate([FbxCurveData](const FRigControlFBXNodeAndChannels& A)
				{ return A.NodeName == FbxCurveData.NodeName && A.ControlName == FbxCurveData.ControlName; }
			);
			if (i != INDEX_NONE)
			{
				CurrentNodeAndChannel = &(*NodeAndChannels)[i];
			}
			// Create the node if not created yet
			else
			{
				NodeAndChannels->Add(FRigControlFBXNodeAndChannels());
				
				CurrentNodeAndChannel = &NodeAndChannels->Last();

				CurrentNodeAndChannel->MovieSceneTrack = this;
				CurrentNodeAndChannel->ControlType = FbxCurveData.ControlType;
				CurrentNodeAndChannel->NodeName = FbxCurveData.NodeName;
				CurrentNodeAndChannel->ControlName = FbxCurveData.ControlName;
			}

			if (ChannelTypeName == DoubleChannelTypeName)
			{
				FMovieSceneDoubleChannel* DoubleChannel = Channel.Cast<FMovieSceneDoubleChannel>().Get();
				CurrentNodeAndChannel->DoubleChannels.Add(DoubleChannel);
			}
			else if (ChannelTypeName == FloatChannelTypeName)
			{
				FMovieSceneFloatChannel* FloatChannel = Channel.Cast<FMovieSceneFloatChannel>().Get();
				CurrentNodeAndChannel->FloatChannels.Add(FloatChannel);
			}
			else if (ChannelTypeName == BoolChannelTypeName)
			{
				FMovieSceneBoolChannel* BoolChannel = Channel.Cast<FMovieSceneBoolChannel>().Get();
				CurrentNodeAndChannel->BoolChannels.Add(BoolChannel);
			}
			else if (ChannelTypeName == EnumChannelTypeName)
			{
				FMovieSceneByteChannel* EnumChannel = Channel.Cast<FMovieSceneByteChannel>().Get();
				CurrentNodeAndChannel->EnumChannels.Add(EnumChannel);
			}
			else if (ChannelTypeName == IntegerChannelTypeName)
			{
				FMovieSceneIntegerChannel* IntegerChannel = Channel.Cast<FMovieSceneIntegerChannel>().Get();
				CurrentNodeAndChannel->IntegerChannels.Add(IntegerChannel);
			}
		}
	}

	return NodeAndChannels;
#else
	return nullptr;
#endif
}


#undef LOCTEXT_NAMESPACE

