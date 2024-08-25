// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stack/SNiagaraSimTargetToggle.h"

#include "NiagaraEditorStyle.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Images/SImage.h"
#include "ViewModels/NiagaraEmitterViewModel.h"

#define LOCTEXT_NAMESPACE "SNiagaraSimTargetToggle"

void SNiagaraSimTargetToggle::Construct(const FArguments& InArgs, TSharedPtr<FNiagaraEmitterHandleViewModel> InEmitterHandleViewModel)
{
	EmitterHandleViewModel = InEmitterHandleViewModel;

	if(EmitterHandleViewModel.IsValid() == false)
	{
		return;
	}
	
	ChildSlot
	[
		SNew(SButton)
		.ButtonStyle(&FAppStyle::GetWidgetStyle<FButtonStyle>("HoverHintOnly"))
		.OnClicked(this, &SNiagaraSimTargetToggle::ToggleSimTargetInUI)
		.ToolTipText(this, &SNiagaraSimTargetToggle::GetSimTargetToggleTooltip)
		[
			SNew(SImage)
			.Image(this, &SNiagaraSimTargetToggle::GetSimTargetImage)
		]
	];
}

void SNiagaraSimTargetToggle::ToggleSimTarget() const
{
	if(EmitterHandleViewModel.IsValid() == false)
	{
		return;
	}
	
	FProperty* MatchingProperty = nullptr;
	for(TFieldIterator<FProperty> It(FVersionedNiagaraEmitterData::StaticStruct()); It; ++It)
	{
		if(*It->GetName() == GET_MEMBER_NAME_CHECKED(FVersionedNiagaraEmitterData, SimTarget))
		{
			MatchingProperty = *It;
		}
	}
	ensure(MatchingProperty != nullptr);

	FScopedTransaction Transaction(LOCTEXT("ToggleSimTargetTransaction", "Simulation Target Changed"));
	EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEmitter().Emitter->PreEditChange(MatchingProperty);
	
	ENiagaraSimTarget CurrentSimTarget = EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEmitter().GetEmitterData()->SimTarget;
	EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEmitter().GetEmitterData()->SimTarget = CurrentSimTarget == ENiagaraSimTarget::CPUSim ?  ENiagaraSimTarget::GPUComputeSim : ENiagaraSimTarget::CPUSim;

	FPropertyChangedEvent PropertyChangedEvent(MatchingProperty);
	UNiagaraSystem::RequestCompileForEmitter(EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEmitter());
}

FReply SNiagaraSimTargetToggle::ToggleSimTargetInUI() const
{
	ToggleSimTarget();
	return FReply::Handled();
}

const FSlateBrush* SNiagaraSimTargetToggle::GetSimTargetImage() const
{
	if(EmitterHandleViewModel.IsValid() == false)
	{
		return FAppStyle::GetNoBrush();
	}
	
	return EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEmitter().GetEmitterData()->SimTarget == ENiagaraSimTarget::CPUSim
		? FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.CPUIcon")
		: FNiagaraEditorStyle::Get().GetBrush("NiagaraEditor.Stack.GPUIcon");
}

FText SNiagaraSimTargetToggle::GetSimTargetToggleTooltip() const
{
	if(EmitterHandleViewModel.IsValid() == false)
	{
		return FText::GetEmpty();
	}

	return EmitterHandleViewModel.Pin()->GetEmitterViewModel()->GetEmitter().GetEmitterData()->SimTarget == ENiagaraSimTarget::CPUSim
		? LOCTEXT("ToggleSimTargetTooltip_CPU", "CPU emitter. Clicking will turn this emitter into a GPU emitter and recompile.")
		: LOCTEXT("ToggleSimTargetTooltip_GPU", "GPU emitter. Clicking will turn this emitter into a CPU emitter and recompile.");		
}

#undef LOCTEXT_NAMESPACE
