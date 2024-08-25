// Copyright Epic Games, Inc. All Rights Reserved.

#include "Stateless/NiagaraDistributionPropertyCustomization.h"

#include "DetailWidgetRow.h"
#include "Editor.h"
#include "EditorUndoClient.h"
#include "PropertyHandle.h"
#include "ScopedTransaction.h"
#include "Stateless/NiagaraStatelessDistribution.h"
#include "Styling/StyleColors.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/NiagaraDistributionEditorUtilities.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SNiagaraDistributionEditor.h"

#include "NiagaraNodeParameterMapBase.h"
#include "NiagaraParameterMapHistory.h"
#include "NiagaraScriptSource.h"
#include "NiagaraSystem.h"

#define LOCTEXT_NAMESPACE "NiagaraDistributionPropertyCustomization"

class FNiagaraDistributionAdapter : public INiagaraDistributionAdapter
{
public:
	FNiagaraDistributionAdapter(TSharedPtr<IPropertyHandle> InPropertyHandle, UObject* InOwningObject, FNiagaraDistributionBase* InDistribution, int32 InNumChannels)
		: PropertyHandle(InPropertyHandle)
		, OwnerObjectWeak(TWeakObjectPtr<UObject>(InOwningObject))
		, SourceDistribution(InDistribution)
		, SourceNumChannels(InNumChannels)
	{
		if (IsValid())
		{
			const FName DisableBindingDistributionName("DisableBindingDistribution");
			const FName DisableCurveDistributionName("DisableCurveDistribution");
			const FName DisableUniformDistributionName("DisableUniformDistribution");
			const FName DisableNonUniformDistributionName("DisableNonUniformDistribution");
			const FName DisableRangeDistributionName("DisableRangeDistribution");
			const FName DisplayAsColorDistributionName("DisplayAsColorDistribution");
			bAllowBinding = InDistribution->AllowBinding() && InDistribution->GetBindingTypeDef().IsValid() && (PropertyHandle ? PropertyHandle->HasMetaData(DisableBindingDistributionName) == false : false);
			bAllowUniform = PropertyHandle ? PropertyHandle->HasMetaData(DisableUniformDistributionName) == false : false;
			bAllowNonUniform = PropertyHandle ? PropertyHandle->HasMetaData(DisableNonUniformDistributionName) == false : false;
			bAllowRange = PropertyHandle ? PropertyHandle->HasMetaData(DisableRangeDistributionName) == false : false;
			bAllowCurves = InDistribution->AllowCurves() && (PropertyHandle ? PropertyHandle->HasMetaData(DisableCurveDistributionName) == false : false);
			bDisplayAsColor = InDistribution->DisplayAsColor() || (InNumChannels >= 3 && (PropertyHandle ? PropertyHandle->HasMetaData(DisplayAsColorDistributionName) : false));

			EditorMode = GetDistributionEditorModeFromSourceMode(SourceNumChannels, bDisplayAsColor, SourceDistribution->Mode);
			EditorNumChannels = FNiagaraDistributionEditorUtilities::IsUniform(EditorMode) ? 1 : SourceNumChannels;
		}
	}

	virtual bool IsValid() const override { return SourceDistribution != nullptr; }

	virtual int32 GetNumChannels() const override { return EditorNumChannels; }

	virtual FText GetChannelDisplayName(int32 ChannelIndex) const override 
	{
		if (bDisplayAsColor)
		{
			switch (ChannelIndex)
			{
			case 0:
				return LOCTEXT("RChannelName", "R");
			case 1:
				return LOCTEXT("GChannelName", "G");
			case 2:
				return LOCTEXT("BChannelName", "B");
			case 3:
				return LOCTEXT("AChannelName", "A");
			}
		}
		else
		{
			switch(ChannelIndex)
			{
			case 0:
				return LOCTEXT("XChannelName", "X");
			case 1:
				return LOCTEXT("YChannelName", "Y");
			case 2:
				return LOCTEXT("ZChannelName", "Z");
			case 3:
				return LOCTEXT("WChannelName", "W");
			}
		}
		return FText();
	}

	virtual FSlateColor GetChannelColor(int32 ChannelIndex) const override
	{
		if (EditorNumChannels != 1)
		{
			switch (ChannelIndex)
			{
			case 0:
				return FSlateColor(EStyleColor::AccentRed);
			case 1:
				return FSlateColor(EStyleColor::AccentGreen);
			case 2:
				return FSlateColor(EStyleColor::AccentBlue);
			case 3:
				return FSlateColor(EStyleColor::AccentWhite);
			}
		}
		return FSlateColor(EStyleColor::AccentWhite);
	}

	virtual void GetSupportedDistributionModes(TArray<ENiagaraDistributionEditorMode>& OutSupportedModes) const override
	{
		if (bAllowBinding)
		{
			OutSupportedModes.Add(ENiagaraDistributionEditorMode::Binding);
		}
		if (bDisplayAsColor)
		{
			OutSupportedModes.Add(ENiagaraDistributionEditorMode::ColorConstant);
			OutSupportedModes.Add(ENiagaraDistributionEditorMode::ColorRange);
			if (bAllowCurves)
			{
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::NonUniformCurve);
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::ColorGradient);
			}
		}
		else if (SourceNumChannels == 1)
		{
			if (bAllowUniform)
			{
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::Constant);
				if (bAllowRange)
				{
					OutSupportedModes.Add(ENiagaraDistributionEditorMode::Range);
				}
			}
			if (bAllowCurves)
			{
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::Curve);
			}
		}
		else if (SourceNumChannels > 1)
		{
			if (bAllowUniform)
			{
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::UniformConstant);
				if (bAllowRange)
				{
					OutSupportedModes.Add(ENiagaraDistributionEditorMode::UniformRange);
				}
			}
			if (bAllowNonUniform)
			{
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::NonUniformConstant);
				if (bAllowRange)
				{
					OutSupportedModes.Add(ENiagaraDistributionEditorMode::NonUniformRange);
				}
			}
			if (bAllowCurves)
			{
				OutSupportedModes.Add(ENiagaraDistributionEditorMode::UniformCurve);
				if (bAllowNonUniform)
				{
					OutSupportedModes.Add(ENiagaraDistributionEditorMode::NonUniformCurve);
				}
			}
		}
	}

	static ENiagaraDistributionEditorMode GetDistributionEditorModeFromSourceMode(int32 InSourceNumChannels, bool bInDisplayAsColor, ENiagaraDistributionMode InSourceMode)
	{
		if (bInDisplayAsColor)
		{
			switch (InSourceMode)
			{
			case ENiagaraDistributionMode::NonUniformConstant:
				return ENiagaraDistributionEditorMode::ColorConstant;
			case ENiagaraDistributionMode::NonUniformRange:
				return ENiagaraDistributionEditorMode::ColorRange;
			case ENiagaraDistributionMode::NonUniformCurve:
				return ENiagaraDistributionEditorMode::ColorGradient;
			default:
				return ENiagaraDistributionEditorMode::ColorConstant;
			}
		}
		else if (InSourceNumChannels == 1)
		{
			switch (InSourceMode)
			{
			case ENiagaraDistributionMode::Binding:
				return ENiagaraDistributionEditorMode::Binding;
			case ENiagaraDistributionMode::UniformConstant:
				return ENiagaraDistributionEditorMode::Constant;
			case ENiagaraDistributionMode::UniformRange:
				return ENiagaraDistributionEditorMode::Range;
			case ENiagaraDistributionMode::UniformCurve:
				return ENiagaraDistributionEditorMode::Curve;
			default:
				return ENiagaraDistributionEditorMode::Constant;
			}
		}
		else
		{
			switch (InSourceMode)
			{
			case ENiagaraDistributionMode::Binding:
				return ENiagaraDistributionEditorMode::Binding;
			case ENiagaraDistributionMode::UniformConstant:
				return ENiagaraDistributionEditorMode::UniformConstant;
			case ENiagaraDistributionMode::NonUniformConstant:
				return ENiagaraDistributionEditorMode::NonUniformConstant;
			case ENiagaraDistributionMode::UniformRange:
				return ENiagaraDistributionEditorMode::UniformRange;
			case ENiagaraDistributionMode::NonUniformRange:
				return ENiagaraDistributionEditorMode::NonUniformRange;
			case ENiagaraDistributionMode::UniformCurve:
				return ENiagaraDistributionEditorMode::UniformCurve;
			case ENiagaraDistributionMode::NonUniformCurve:
				return ENiagaraDistributionEditorMode::NonUniformCurve;
			default:
				return ENiagaraDistributionEditorMode::UniformConstant;
			}
		}
	}

	static ENiagaraDistributionMode GetDistributionSourceModeFromEditorMode(ENiagaraDistributionEditorMode InEditorMode)
	{
		switch (InEditorMode)
		{
		case ENiagaraDistributionEditorMode::Binding:
			return ENiagaraDistributionMode::Binding;
		case ENiagaraDistributionEditorMode::Constant:
		case ENiagaraDistributionEditorMode::UniformConstant:
			return ENiagaraDistributionMode::UniformConstant;
		case ENiagaraDistributionEditorMode::NonUniformConstant:
		case ENiagaraDistributionEditorMode::ColorConstant:
			return ENiagaraDistributionMode::NonUniformConstant;
		case ENiagaraDistributionEditorMode::Range:
		case ENiagaraDistributionEditorMode::UniformRange:
			return ENiagaraDistributionMode::UniformRange;
		case ENiagaraDistributionEditorMode::NonUniformRange:
		case ENiagaraDistributionEditorMode::ColorRange:
			return ENiagaraDistributionMode::NonUniformRange;
		case ENiagaraDistributionEditorMode::Curve:
		case ENiagaraDistributionEditorMode::UniformCurve:
			return ENiagaraDistributionMode::UniformCurve;
		case ENiagaraDistributionEditorMode::NonUniformCurve:
		case ENiagaraDistributionEditorMode::ColorGradient:
			return ENiagaraDistributionMode::NonUniformCurve;
		default:
			return ENiagaraDistributionMode::UniformConstant;
		}
	}

	virtual ENiagaraDistributionEditorMode GetDistributionMode() const override { return EditorMode; }

	virtual void SetDistributionMode(ENiagaraDistributionEditorMode InEditorMode) override
	{
		if (InEditorMode == EditorMode)
		{
			return;
		}

		UObject* OwnerObject = OwnerObjectWeak.Get();
		if (OwnerObject == nullptr)
		{
			return;
		}

		int32 NewEditorNumChannels = FNiagaraDistributionEditorUtilities::IsUniform(InEditorMode) ? 1 : SourceNumChannels;

		ENiagaraDistributionMode NewSourceMode = GetDistributionSourceModeFromEditorMode(InEditorMode);
		if (SourceDistribution->Mode != NewSourceMode)
		{
			const FScopedTransaction Transaction(LOCTEXT("SetDistributionModeTransaction", "Set distribution mode"));
			OwnerObject->Modify();
			PropertyHandle->NotifyPreChange();

			MigrateDataFromModeChange(NewSourceMode, NewEditorNumChannels);
			SourceDistribution->Mode = NewSourceMode;

			PropertyHandle->NotifyPostChange(EPropertyChangeType::ValueSet);
		}

		EditorNumChannels = NewEditorNumChannels;
		EditorMode = InEditorMode;
	}

	float GetConstantOrRangeValue(int32 ChannelIndex, int32 ValueIndex) const override
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		int32 DataIndex = ValueIndex * EditorNumChannels + ChannelIndex;
		if (OwnerObject != nullptr &&
			SourceDistribution != nullptr &&
			ChannelIndex < EditorNumChannels &&
			DataIndex < SourceDistribution->ChannelConstantsAndRanges.Num())
		{
			return SourceDistribution->ChannelConstantsAndRanges[DataIndex];
		}
		return 0.0f;
	}

	void SetConstantOrRangeValue(int32 ChannelIndex, int32 ValueIndex, float InValue) override
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		if (OwnerObject == nullptr ||
			ChannelIndex >= EditorNumChannels ||
			GetConstantOrRangeValue(ChannelIndex, ValueIndex) == InValue)
		{
			return;
		}

		FText TransactionText;
		int32 RequiredValueCount = 0;
		if (FNiagaraDistributionEditorUtilities::IsConstant(GetDistributionMode()))
		{
			TransactionText = LOCTEXT("SetConstantValueTransaction", "Set constant value");
			RequiredValueCount = EditorNumChannels;
		}
		else if (FNiagaraDistributionEditorUtilities::IsRange(GetDistributionMode()))
		{
			TransactionText = ValueIndex == 0
				? LOCTEXT("SetRangeMinValueTransaction", "Set range min value")
				: LOCTEXT("SetRangeMaxValueTransaction", "Set range max value");
			RequiredValueCount = EditorNumChannels * 2;
		}

		if (bContinuousTransactionPending)
		{
			bContinuousTransactionPending = false;
			ContinuousTransactionIndex = GEditor->BeginTransaction(TransactionText);
		}

		const FScopedTransaction Transaction(TransactionText, ContinuousTransactionIndex.IsSet() == false);
		OwnerObject->Modify();
		PropertyHandle->NotifyPreChange();

		if (SourceDistribution->ChannelConstantsAndRanges.Num() < RequiredValueCount)
		{
			SourceDistribution->ChannelConstantsAndRanges.AddZeroed(RequiredValueCount - SourceDistribution->ChannelConstantsAndRanges.Num());
		}
		else if (SourceDistribution->ChannelConstantsAndRanges.Num() > RequiredValueCount)
		{
			SourceDistribution->ChannelConstantsAndRanges.SetNum(RequiredValueCount);
		}

		SourceDistribution->ChannelConstantsAndRanges[ValueIndex * EditorNumChannels + ChannelIndex] = InValue;
		PropertyHandle->NotifyPostChange(bContinuousChangeActive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
	}

	virtual void SetConstantOrRangeValues(int32 ValueIndex, const TArray<float>& InValues) override
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		if (OwnerObject == nullptr || InValues.Num() != EditorNumChannels)
		{
			return;
		}

		bool bAllChannelsTheSame = true;
		for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ++ChannelIndex)
		{
			if (GetConstantOrRangeValue(ChannelIndex, ValueIndex) != InValues[ChannelIndex])
			{
				bAllChannelsTheSame = false;
				break;
			}
		}

		if (bAllChannelsTheSame)
		{
			return;
		}

		FText TransactionText;
		int32 RequiredValueCount = 0;
		if (FNiagaraDistributionEditorUtilities::IsConstant(GetDistributionMode()))
		{
			TransactionText = LOCTEXT("SetConstantValueTransaction", "Set constant value");
			RequiredValueCount = EditorNumChannels;
		}
		else if (FNiagaraDistributionEditorUtilities::IsRange(GetDistributionMode()))
		{
			TransactionText = ValueIndex == 0
				? LOCTEXT("SetRangeMinValueTransaction", "Set range min value")
				: LOCTEXT("SetRangeMaxValueTransaction", "Set range max value");
			RequiredValueCount = EditorNumChannels * 2;
		}

		if (bContinuousTransactionPending)
		{
			bContinuousTransactionPending = false;
			ContinuousTransactionIndex = GEditor->BeginTransaction(TransactionText);
		}

		const FScopedTransaction Transaction(TransactionText, ContinuousTransactionIndex.IsSet() == false);
		OwnerObject->Modify();
		PropertyHandle->NotifyPreChange();

		if (SourceDistribution->ChannelConstantsAndRanges.Num() < RequiredValueCount)
		{
			SourceDistribution->ChannelConstantsAndRanges.AddZeroed(RequiredValueCount - SourceDistribution->ChannelConstantsAndRanges.Num());
		}
		else if (SourceDistribution->ChannelConstantsAndRanges.Num() > RequiredValueCount)
		{
			SourceDistribution->ChannelConstantsAndRanges.SetNum(RequiredValueCount);
		}

		for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ++ChannelIndex)
		{
			SourceDistribution->ChannelConstantsAndRanges[ValueIndex * EditorNumChannels + ChannelIndex] = InValues[ChannelIndex];
		}
		PropertyHandle->NotifyPostChange(bContinuousChangeActive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
	}

	virtual FNiagaraVariableBase GetBindingValue() const
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		if (OwnerObject == nullptr)
		{
			return FNiagaraVariableBase();
		}
		return SourceDistribution->ParameterBinding;
	}

	virtual void SetBindingValue(FNiagaraVariableBase Binding)
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		if (OwnerObject == nullptr)
		{
			return;
		}

		FText TransactionText = LOCTEXT("SetBinding", "Set binding value");

		if (bContinuousTransactionPending)
		{
			bContinuousTransactionPending = false;
			ContinuousTransactionIndex = GEditor->BeginTransaction(TransactionText);
		}

		const FScopedTransaction Transaction(TransactionText, ContinuousTransactionIndex.IsSet() == false);
		OwnerObject->Modify();
		PropertyHandle->NotifyPreChange();

		SourceDistribution->ParameterBinding = Binding;

		PropertyHandle->NotifyPostChange(bContinuousChangeActive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
	}

	virtual TArray<FNiagaraVariableBase> GetAvailableBindings() const
	{
		TArray<FNiagaraVariableBase> AvailableBindings;
		UObject* OwnerObject = OwnerObjectWeak.Get();
		UNiagaraSystem* OwnerSystem = OwnerObject ? OwnerObject->GetTypedOuter<UNiagaraSystem>() : nullptr;
		const FNiagaraTypeDefinition AllowedTypeDef = SourceDistribution->GetBindingTypeDef();
		if ( OwnerSystem && AllowedTypeDef.IsValid() )
		{
			if (UNiagaraScriptSource* Source = Cast<UNiagaraScriptSource>(OwnerSystem->GetSystemUpdateScript()->GetLatestSource()))
			{
				TArray<FNiagaraParameterMapHistory> Histories = UNiagaraNodeParameterMapBase::GetParameterMaps(Source->NodeGraph);
				for (const FNiagaraParameterMapHistory& History : Histories)
				{
					for (const FNiagaraVariable& Variable : History.Variables)
					{
						if (Variable.GetType() == AllowedTypeDef && Variable.IsInNameSpace(FNiagaraConstants::SystemNamespaceString))
						{
							AvailableBindings.Add(Variable);
						}
					}
				}
			}

			for (const FNiagaraVariableBase& Variable : OwnerSystem->GetExposedParameters().ReadParameterVariables())
			{
				if (Variable.GetType() == AllowedTypeDef)
				{
					AvailableBindings.Add(Variable);
				}
			}
		}
		return AvailableBindings;
	}

	virtual const FRichCurve* GetCurveValue(int32 ChannelIndex) const override
	{
		if (ChannelIndex < EditorNumChannels &&
			ChannelIndex < SourceDistribution->ChannelCurves.Num())
		{
			return &SourceDistribution->ChannelCurves[ChannelIndex];
		}
		return nullptr;
	}

	virtual void SetCurveValue(int32 ChannelIndex, const FRichCurve& InValue) override
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		const FRichCurve* CurrentValue = GetCurveValue(ChannelIndex);
		if (OwnerObject != nullptr &&
			(CurrentValue == nullptr || *CurrentValue != InValue) &&
			ChannelIndex < EditorNumChannels)
		{
			FText TransactionText = LOCTEXT("SetCurveValueTransaction", "Set curve value");

			if (bContinuousTransactionPending)
			{
				bContinuousTransactionPending = false;
				ContinuousTransactionIndex = GEditor->BeginTransaction(TransactionText);
			}

			const FScopedTransaction Transaction(TransactionText, ContinuousTransactionIndex.IsSet() == false);
			OwnerObject->Modify();
			PropertyHandle->NotifyPreChange();

			const int32 RequiredCurveCount = EditorNumChannels;
			if (SourceDistribution->ChannelCurves.Num() < RequiredCurveCount)
			{
				SourceDistribution->ChannelCurves.AddDefaulted(RequiredCurveCount - SourceDistribution->ChannelCurves.Num());
			}
			else if (SourceDistribution->ChannelCurves.Num() > RequiredCurveCount)
			{
				SourceDistribution->ChannelCurves.SetNum(RequiredCurveCount);
			}

			SourceDistribution->ChannelCurves[ChannelIndex] = InValue;
			PropertyHandle->NotifyPostChange(bContinuousChangeActive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
		}
	}

	virtual void BeginContinuousChange() override
	{
		bContinuousTransactionPending = true;
		bContinuousChangeActive = true;
	}

	virtual void EndContinuousChange() override
	{
		if (ContinuousTransactionIndex.IsSet())
		{
			GEditor->EndTransaction();
		}
		bContinuousTransactionPending = false;
		ContinuousTransactionIndex.Reset();
		bContinuousChangeActive = false;
		PropertyHandle->NotifyFinishedChangingProperties();
	}

	virtual void CancelContinuousChange() override
	{
		if (ContinuousTransactionIndex.IsSet())
		{
			GEditor->CancelTransaction(ContinuousTransactionIndex.GetValue());
		}
		bContinuousTransactionPending = false;
		ContinuousTransactionIndex.Reset();
		bContinuousChangeActive = false;
		PropertyHandle->NotifyFinishedChangingProperties();
	}

	virtual void ModifyOwners() override
	{
		UObject* OwnerObject = OwnerObjectWeak.Get();
		if (OwnerObject != nullptr)
		{
			OwnerObject->Modify();
		}
	}

private:
	void MigrateDataFromModeChange(ENiagaraDistributionMode NewMode, int32 NewEditorNumChannels)
	{
		if (NewMode == ENiagaraDistributionMode::UniformConstant || NewMode == ENiagaraDistributionMode::NonUniformConstant)
		{
			MigrateDataToConstants(NewEditorNumChannels);
		}
		else if(NewMode == ENiagaraDistributionMode::UniformRange || NewMode == ENiagaraDistributionMode::NonUniformRange)
		{
			MigrateDataToRanges(NewEditorNumChannels);
		}
		else if (NewMode == ENiagaraDistributionMode::UniformCurve || NewMode == ENiagaraDistributionMode::NonUniformCurve)
		{
			MigrateDataToCurves(NewEditorNumChannels);
		}
	}

	void MigrateDataToConstants(int32 NewEditorNumChannels)
	{
		TArray<float> ConstantValues;
		switch (SourceDistribution->Mode)
		{
		case ENiagaraDistributionMode::UniformConstant:
		case ENiagaraDistributionMode::UniformRange:
		case ENiagaraDistributionMode::NonUniformConstant:
		case ENiagaraDistributionMode::NonUniformRange:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				ConstantValues.Add(GetConstantOrRangeValue(ChannelIndex, 0));
			}
			break;
		case ENiagaraDistributionMode::UniformCurve:
		case ENiagaraDistributionMode::NonUniformCurve:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				const FRichCurve* Curve = GetCurveValue(ChannelIndex);
				float ConstantValue = Curve != nullptr && Curve->GetNumKeys() > 0 ? Curve->GetFirstKey().Value : 0;
				ConstantValues.Add(ConstantValue);
			}
			break;
		default:
			ConstantValues.Add(0);
		}

		SourceDistribution->ChannelConstantsAndRanges.SetNum(NewEditorNumChannels);
		SourceDistribution->ChannelCurves.Empty();
		for (int32 ChannelIndex = 0; ChannelIndex < NewEditorNumChannels; ChannelIndex++)
		{
			int32 ValueIndex = ChannelIndex < ConstantValues.Num() ? ChannelIndex : 0;
			SourceDistribution->ChannelConstantsAndRanges[ChannelIndex] = ConstantValues[ValueIndex];
		}
	}

	void MigrateDataToRanges(int32 NewEditorNumChannels)
	{
		TArray<float> MinValues;
		TArray<float> MaxValues;
		switch (SourceDistribution->Mode)
		{
		case ENiagaraDistributionMode::UniformConstant:
		case ENiagaraDistributionMode::NonUniformConstant:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				float ConstantValue = GetConstantOrRangeValue(ChannelIndex, 0);
				MinValues.Add(ConstantValue);
				MaxValues.Add(ConstantValue);
			}
			break;
		case ENiagaraDistributionMode::UniformRange:
		case ENiagaraDistributionMode::NonUniformRange:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				MinValues.Add(GetConstantOrRangeValue(ChannelIndex, 0));
				MaxValues.Add(GetConstantOrRangeValue(ChannelIndex, 1));
			}
			break;
		case ENiagaraDistributionMode::UniformCurve:
		case ENiagaraDistributionMode::NonUniformCurve:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				const FRichCurve* Curve = GetCurveValue(ChannelIndex);
				if (Curve != nullptr && Curve->GetNumKeys() > 0)
				{
					MinValues.Add(Curve->GetFirstKey().Value);
					MaxValues.Add(Curve->GetLastKey().Value);
				}
				else
				{
					MinValues.Add(0);
					MaxValues.Add(0);
				}
			}
			break;
		default:
			MinValues.Add(0);
			MaxValues.Add(0);
			break;
		}

		SourceDistribution->ChannelConstantsAndRanges.SetNum(NewEditorNumChannels * 2);
		SourceDistribution->ChannelCurves.Empty();
		for (int32 ChannelIndex = 0; ChannelIndex < NewEditorNumChannels; ChannelIndex++)
		{
			int32 MinValueIndex = ChannelIndex < MinValues.Num() ? ChannelIndex : 0;
			SourceDistribution->ChannelConstantsAndRanges[ChannelIndex] = MinValues[MinValueIndex];

			int32 MaxValueIndex = ChannelIndex < MaxValues.Num() ? ChannelIndex : 0;
			SourceDistribution->ChannelConstantsAndRanges[NewEditorNumChannels + ChannelIndex] = MaxValues[MaxValueIndex];
		}
	}

	void MigrateDataToCurves(int32 NewEditorNumChannels)
	{
		TArray<FRichCurve> CurveValues;
		switch (SourceDistribution->Mode)
		{
		case ENiagaraDistributionMode::UniformConstant:
		case ENiagaraDistributionMode::NonUniformConstant:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				FRichCurve& CurveValue = CurveValues.AddDefaulted_GetRef();
				CurveValue.AddKey(0, GetConstantOrRangeValue(ChannelIndex, 0));
			}
			break;
		case ENiagaraDistributionMode::UniformRange:
		case ENiagaraDistributionMode::NonUniformRange:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				FRichCurve& CurveValue = CurveValues.AddDefaulted_GetRef();
				CurveValue.AddKey(0, GetConstantOrRangeValue(ChannelIndex, 0));
				CurveValue.AddKey(1, GetConstantOrRangeValue(ChannelIndex, 1));
			}
			break;
		case ENiagaraDistributionMode::UniformCurve:
		case ENiagaraDistributionMode::NonUniformCurve:
			for (int32 ChannelIndex = 0; ChannelIndex < EditorNumChannels; ChannelIndex++)
			{
				const FRichCurve* Curve = GetCurveValue(ChannelIndex);
				if (Curve != nullptr)
				{
					CurveValues.Add(*Curve);
				}
				else
				{
					FRichCurve& CurveValue = CurveValues.AddDefaulted_GetRef();
					CurveValue.AddKey(0, 0);
				}
			}
			break;
		default:
			FRichCurve& CurveValue = CurveValues.AddDefaulted_GetRef();
			CurveValue.AddKey(0, 0);
			break;
		}

		SourceDistribution->ChannelCurves.SetNum(NewEditorNumChannels);
		SourceDistribution->ChannelConstantsAndRanges.Empty();
		for (int32 ChannelIndex = 0; ChannelIndex < NewEditorNumChannels; ChannelIndex++)
		{
			int32 ValueIndex = ChannelIndex < CurveValues.Num() ? ChannelIndex : 0;
			SourceDistribution->ChannelCurves[ChannelIndex] = CurveValues[ValueIndex];
		}
	}

private:
	TSharedPtr<IPropertyHandle> PropertyHandle;
	TWeakObjectPtr<UObject> OwnerObjectWeak;
	FNiagaraDistributionBase* SourceDistribution = nullptr;
	int32 SourceNumChannels = 0;

	ENiagaraDistributionEditorMode EditorMode = ENiagaraDistributionEditorMode::Constant;
	int32 EditorNumChannels = 0;

	bool bContinuousTransactionPending = false;
	TOptional<int32> ContinuousTransactionIndex;
	bool bContinuousChangeActive = false;

	bool bAllowBinding = true;
	bool bAllowUniform = true;
	bool bAllowNonUniform = true;
	bool bAllowRange = true;
	bool bAllowCurves = true;
	bool bDisplayAsColor = false;
};

class SNiagaraDistributionPropertyWidget : public SCompoundWidget, public FEditorUndoClient
{
	SLATE_BEGIN_ARGS(SNiagaraDistributionPropertyWidget) { }
	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<IPropertyHandle> InDistributionPropertyHandle, TSharedRef<INiagaraDistributionAdapter> InDistributionAdapter)
	{
		GEditor->RegisterForUndo(this);
		DistributionPropertyHandleWeak = InDistributionPropertyHandle;
		DistributionAdapter = InDistributionAdapter;
		InDistributionPropertyHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &SNiagaraDistributionPropertyWidget::HandleValueChanged));

		ChildSlot
		[
			SNew(SNiagaraDistributionEditor, DistributionAdapter.ToSharedRef())
		];
	}

	virtual ~SNiagaraDistributionPropertyWidget()
	{
		if (GEditor != nullptr)
		{
			GEditor->UnregisterForUndo(this);
		}
	}

	// Begin FEditorUndoClient
	virtual void PostUndo(bool bSuccess) override
	{
	}

	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }
	// End of FEditorUndoClient

private:
	void HandleValueChanged()
	{
		if (bUpdatingHandle == false)
		{
		}
	}

private:
	TWeakPtr<IPropertyHandle> DistributionPropertyHandleWeak;
	TSharedPtr<INiagaraDistributionAdapter> DistributionAdapter;
	bool bUpdatingHandle = false;
};

TSharedRef<IPropertyTypeCustomization> FNiagaraDistributionPropertyCustomization::MakeFloatInstance(UObject* OptionalOuter)
{
	FPropertyHandleToDistributionAdapter FloatDistributionPropertyHandleToDistributionAdapter = FPropertyHandleToDistributionAdapter::CreateLambda([OptionalOuter](TSharedRef<IPropertyHandle> FloatDistributionPropertyHandle)
	{
		void* ValueData = nullptr;
		TArray<UObject*> OuterObjects;
		if (OptionalOuter)
		{
			TArray<TSharedPtr<FStructOnScope>> OutStructOnScopes;
			FloatDistributionPropertyHandle->GetOuterStructs(OutStructOnScopes);
			if (OutStructOnScopes.Num() == 1)
			{
				OuterObjects.Add(OptionalOuter);
			}
		}
		else
		{
			FloatDistributionPropertyHandle->GetOuterObjects(OuterObjects);
		}
		if (OuterObjects.Num() == 1 && FloatDistributionPropertyHandle->GetValueData(ValueData) == FPropertyAccess::Success)
		{
			FNiagaraDistributionBase* FloatDistribution = static_cast<FNiagaraDistributionBase*>(ValueData);
			return MakeShared<FNiagaraDistributionAdapter>(FloatDistributionPropertyHandle, OuterObjects[0], FloatDistribution, 1);
		}
		return MakeShared<FNiagaraDistributionAdapter>(nullptr, nullptr, nullptr, INDEX_NONE);
	});

	return MakeShareable<FNiagaraDistributionPropertyCustomization>(new FNiagaraDistributionPropertyCustomization(FloatDistributionPropertyHandleToDistributionAdapter));
}

TSharedRef<IPropertyTypeCustomization> FNiagaraDistributionPropertyCustomization::MakeFloatInstance()
{
	return MakeFloatInstance(nullptr);
}

TSharedRef<IPropertyTypeCustomization> FNiagaraDistributionPropertyCustomization::MakeVector2Instance()
{
	FPropertyHandleToDistributionAdapter Vector2DistributionPropertyHandleToDistributionAdapter = FPropertyHandleToDistributionAdapter::CreateLambda([](TSharedRef<IPropertyHandle> Vector2DistributionPropertyHandle)
	{
		void* ValueData = nullptr;
		TArray<UObject*> OuterObjects;
		Vector2DistributionPropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() == 1 && Vector2DistributionPropertyHandle->GetValueData(ValueData) == FPropertyAccess::Success)
		{
			FNiagaraDistributionBase* Vector2Distribution = static_cast<FNiagaraDistributionBase*>(ValueData);
			return MakeShared<FNiagaraDistributionAdapter>(Vector2DistributionPropertyHandle, OuterObjects[0], Vector2Distribution, 2);
		}
		return MakeShared<FNiagaraDistributionAdapter>(nullptr, nullptr, nullptr, INDEX_NONE);
	});

	return MakeShareable<FNiagaraDistributionPropertyCustomization>(new FNiagaraDistributionPropertyCustomization(Vector2DistributionPropertyHandleToDistributionAdapter));
}

TSharedRef<IPropertyTypeCustomization> FNiagaraDistributionPropertyCustomization::MakeVector3Instance()
{
	FPropertyHandleToDistributionAdapter Vector3DistributionPropertyHandleToDistributionAdapter = FPropertyHandleToDistributionAdapter::CreateLambda([](TSharedRef<IPropertyHandle> Vector3DistributionPropertyHandle)
	{
		void* ValueData = nullptr;
		TArray<UObject*> OuterObjects;
		Vector3DistributionPropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() == 1 && Vector3DistributionPropertyHandle->GetValueData(ValueData) == FPropertyAccess::Success)
		{
			FNiagaraDistributionBase* Vector3Distribution = static_cast<FNiagaraDistributionBase*>(ValueData);
			return MakeShared<FNiagaraDistributionAdapter>(Vector3DistributionPropertyHandle, OuterObjects[0], Vector3Distribution, 3);
		}
		return MakeShared<FNiagaraDistributionAdapter>(nullptr, nullptr, nullptr, INDEX_NONE);
	});

	return MakeShareable<FNiagaraDistributionPropertyCustomization>(new FNiagaraDistributionPropertyCustomization(Vector3DistributionPropertyHandleToDistributionAdapter));
}

TSharedRef<IPropertyTypeCustomization> FNiagaraDistributionPropertyCustomization::MakeColorInstance()
{
	FPropertyHandleToDistributionAdapter ColorDistributionPropertyHandleToDistributionAdapter = FPropertyHandleToDistributionAdapter::CreateLambda([](TSharedRef<IPropertyHandle> ColorDistributionPropertyHandle)
	{
		void* ValueData = nullptr;
		TArray<UObject*> OuterObjects;
		ColorDistributionPropertyHandle->GetOuterObjects(OuterObjects);
		if (OuterObjects.Num() == 1 && ColorDistributionPropertyHandle->GetValueData(ValueData) == FPropertyAccess::Success)
		{
			FNiagaraDistributionBase* ColorDistribution = static_cast<FNiagaraDistributionBase*>(ValueData);
			return MakeShared<FNiagaraDistributionAdapter>(ColorDistributionPropertyHandle, OuterObjects[0], ColorDistribution, 4);
		}
		return MakeShared<FNiagaraDistributionAdapter>(nullptr, nullptr, nullptr, INDEX_NONE);
	});

	return MakeShareable<FNiagaraDistributionPropertyCustomization>(new FNiagaraDistributionPropertyCustomization(ColorDistributionPropertyHandleToDistributionAdapter));
}

void FNiagaraDistributionPropertyCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
	[
		PropertyHandle->CreatePropertyNameWidget()
	];
	HeaderRow.ValueContent()
	[
		SNew(SNiagaraDistributionPropertyWidget, PropertyHandle, PropertyHandleToDistributionAdapter.Execute(PropertyHandle))
	];
}

void FNiagaraDistributionPropertyCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
}

#undef LOCTEXT_NAMESPACE