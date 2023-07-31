// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "IDetailCustomNodeBuilder.h"
#include "UObject/WeakObjectPtr.h"
#include "Graph/ControlRigGraph.h"
#include "ControlRigBlueprint.h"
#include "Editor/ControlRigEditor.h"
#include "SGraphPin.h"
#include "Graph/SControlRigGraphNode.h"
#include "Widgets/Colors/SColorBlock.h"
#include "Editor/DetailsViewWrapperObject.h"
#include "IDetailPropertyExtensionHandler.h"
#include "Graph/ControlRigGraphSchema.h"
#include "SAdvancedTransformInputBox.h"
#include "PropertyNode.h"

class IDetailLayoutBuilder;

class FControlRigArgumentGroupLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FControlRigArgumentGroupLayout>
{
public:
	FControlRigArgumentGroupLayout(
		URigVMGraph* InGraph, 
		UControlRigBlueprint* InBlueprint,
		TWeakPtr<IControlRigEditor> InEditor,
		bool bInputs);
	virtual ~FControlRigArgumentGroupLayout();

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override { OnRebuildChildren = InOnRegenerateChildren; }
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override {}
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { return NAME_None; }
	virtual bool InitiallyCollapsed() const override { return false; }

private:

	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	TWeakObjectPtr<URigVMGraph> GraphPtr;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprintPtr;
	TWeakPtr<IControlRigEditor> ControlRigEditorPtr;
	bool bIsInputGroup;
	FSimpleDelegate OnRebuildChildren;
};

class FControlRigArgumentLayout : public IDetailCustomNodeBuilder, public TSharedFromThis<FControlRigArgumentLayout>
{
public:

	FControlRigArgumentLayout(
		URigVMPin* InPin, 
		URigVMGraph* InGraph, 
		UControlRigBlueprint* InBlueprint,
		TWeakPtr<IControlRigEditor> InEditor)
		: PinPtr(InPin)
		, GraphPtr(InGraph)
		, ControlRigBlueprintPtr(InBlueprint)
		, ControlRigEditorPtr(InEditor)
		, NameValidator(InBlueprint, InGraph, InPin->GetFName())
	{}

private:

	/** IDetailCustomNodeBuilder Interface*/
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override;
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { return PinPtr.Get()->GetFName(); }
	virtual bool InitiallyCollapsed() const override { return true; }

private:

	/** Determines if this pin should not be editable */
	bool ShouldPinBeReadOnly(bool bIsEditingPinType = false) const;

	/** Determines if editing the pins on the node should be read only */
	bool IsPinEditingReadOnly(bool bIsEditingPinType = false) const;

	/** Determines if an argument can be moved up or down */
	bool CanArgumentBeMoved(bool bMoveUp) const;

	/** Callbacks for all the functionality for modifying arguments */
	void OnRemoveClicked();
	FReply OnArgMoveUp();
	FReply OnArgMoveDown();

	FText OnGetArgNameText() const;
	FText OnGetArgToolTipText() const;
	void OnArgNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit);

	FEdGraphPinType OnGetPinInfo() const;
	void PinInfoChanged(const FEdGraphPinType& PinType);
	void OnPrePinInfoChange(const FEdGraphPinType& PinType);

private:

	/** The argument pin that this layout reflects */
	TWeakObjectPtr<URigVMPin> PinPtr;
	
	/** The target graph that this argument is on */
	TWeakObjectPtr<URigVMGraph> GraphPtr;

	/** The blueprint we are editing */
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprintPtr;

	/** The editor we are editing */
	TWeakPtr<IControlRigEditor> ControlRigEditorPtr;

	/** Holds a weak pointer to the argument name widget, used for error notifications */
	TWeakPtr<SEditableTextBox> ArgumentNameWidget;

	/** The validator to check if a name for an argument is valid */
	FControlRigLocalVariableNameValidator NameValidator;
};

class FControlRigArgumentDefaultNode : public IDetailCustomNodeBuilder, public TSharedFromThis<FControlRigArgumentDefaultNode>
{
public:
	FControlRigArgumentDefaultNode(
		URigVMGraph* InGraph,
		UControlRigBlueprint* InBlueprint
	);
	virtual ~FControlRigArgumentDefaultNode();

private:
	/** IDetailCustomNodeBuilder Interface*/
	virtual void SetOnRebuildChildren(FSimpleDelegate InOnRegenerateChildren) override { OnRebuildChildren = InOnRegenerateChildren; }
	virtual void GenerateHeaderRowContent(FDetailWidgetRow& NodeRow) override {}
	virtual void GenerateChildContent(IDetailChildrenBuilder& ChildrenBuilder) override;
	virtual void Tick(float DeltaTime) override {}
	virtual bool RequiresTick() const override { return false; }
	virtual FName GetName() const override { return NAME_None; }
	virtual bool InitiallyCollapsed() const override { return false; }

private:

	void OnGraphChanged(const FEdGraphEditAction& InAction);
	void HandleModifiedEvent(ERigVMGraphNotifType InNotifType, URigVMGraph* InGraph, UObject* InSubject);

	TWeakObjectPtr<URigVMGraph> GraphPtr;
	TWeakObjectPtr<UControlRigGraph> EdGraphOuterPtr;
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprintPtr;
	FSimpleDelegate OnRebuildChildren;
	TSharedPtr<SControlRigGraphNode> OwnedNodeWidget;
	FDelegateHandle GraphChangedDelegateHandle;
};


/** Customization for editing Control Rig graphs */
class FControlRigGraphDetails : public IDetailCustomization
{
public:

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedPtr<IDetailCustomization> MakeInstance(TSharedPtr<IBlueprintEditor> InBlueprintEditor);

	FControlRigGraphDetails(TSharedPtr<IControlRigEditor> InControlRigEditor, UControlRigBlueprint* ControlRigBlueprint)
		: ControlRigEditorPtr(InControlRigEditor)
		, ControlRigBlueprintPtr(ControlRigBlueprint)
	{}

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	bool IsAddNewInputOutputEnabled() const;
	EVisibility GetAddNewInputOutputVisibility() const;
	FReply OnAddNewInputClicked();
	FReply OnAddNewOutputClicked();
	FText GetNodeCategory() const;
	void SetNodeCategory(const FText& InNewText, ETextCommit::Type InCommitType);
	FText GetNodeKeywords() const;
	void SetNodeKeywords(const FText& InNewText, ETextCommit::Type InCommitType);
	FText GetNodeDescription() const;
	void SetNodeDescription(const FText& InNewText, ETextCommit::Type InCommitType);
	FLinearColor GetNodeColor() const;
	void SetNodeColor(FLinearColor InColor, bool bSetupUndoRedo);
	void OnNodeColorBegin();
	void OnNodeColorEnd();
	void OnNodeColorCancelled(FLinearColor OriginalColor);
	FReply OnNodeColorClicked();
	FText GetCurrentAccessSpecifierName() const;
	void OnAccessSpecifierSelected( TSharedPtr<FString> SpecifierName, ESelectInfo::Type SelectInfo );
	TSharedRef<ITableRow> HandleGenerateRowAccessSpecifier( TSharedPtr<FString> SpecifierName, const TSharedRef<STableViewBase>& OwnerTable );

private:

	/** The Blueprint editor we are embedded in */
	TWeakPtr<IControlRigEditor> ControlRigEditorPtr;

	/** The graph we are editing */
	TWeakObjectPtr<UControlRigGraph> GraphPtr;

	/** The blueprint we are editing */
	TWeakObjectPtr<UControlRigBlueprint> ControlRigBlueprintPtr;

	/** The color block widget */
	TSharedPtr<SColorBlock> ColorBlock;

	/** The color to change */
	FLinearColor TargetColor;

	/** The color array to change */
	TArray<FLinearColor*> TargetColors;

	/** Set to true if the UI is currently picking a color */
	bool bIsPickingColor;

	static TArray<TSharedPtr<FString>> AccessSpecifierStrings;
};

/** Customization for editing a Control Rig node */
class FControlRigWrappedNodeDetails : public IDetailCustomization
{
public:
	
	FControlRigWrappedNodeDetails();

	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	// IDetailCustomization interface
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	TSharedRef<SWidget> MakeNameListItemWidget(TSharedPtr<FString> InItem);
	FText GetNameListText(FNameProperty* InProperty) const;
	TSharedPtr<FString> GetCurrentlySelectedItem(FNameProperty* InProperty, const TArray<TSharedPtr<FString>>* InNameList) const;
	void SetNameListText(const FText& NewTypeInValue, ETextCommit::Type, FNameProperty* InProperty, TSharedRef<IPropertyUtilities> PropertyUtilities);
	void OnNameListChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo, FNameProperty* InProperty, TSharedRef<IPropertyUtilities> PropertyUtilities);
	void OnNameListComboBox(FNameProperty* InProperty, const TArray<TSharedPtr<FString>>* InNameList);
	void CustomizeLiveValues(IDetailLayoutBuilder& DetailLayout);

	UControlRigBlueprint* BlueprintBeingCustomized;
	TArray<TWeakObjectPtr<UDetailsViewWrapperObject>> ObjectsBeingCustomized;
	TArray<TWeakObjectPtr<URigVMNode>> NodesBeingCustomized;
	TMap<FName, TSharedPtr<SControlRigGraphPinNameListValueWidget>> NameListWidgets;
};

/** Customization for editing a Control Rig node */
class FControlRigGraphMathTypeDetails : public IPropertyTypeCustomization, public FGCObject
{
public:

	FControlRigGraphMathTypeDetails();
	
	static TSharedRef<IPropertyTypeCustomization> MakeInstance()
	{
		return MakeShareable(new FControlRigGraphMathTypeDetails);
	}

	/** IPropertyTypeCustomization interface */
	virtual void CustomizeHeader(TSharedRef<class IPropertyHandle> InPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual FString GetReferencerName() const override;

protected:

	FORCEINLINE_DEBUGGABLE bool GetPropertyChain(TSharedRef<class IPropertyHandle> InPropertyHandle, FEditPropertyChain& OutPropertyChain, TArray<int32> &OutPropertyArrayIndices, bool& bOutEnabled)
	{
		OutPropertyChain.Empty();
		OutPropertyArrayIndices.Reset();
		bOutEnabled = false;
		if (!ObjectsBeingCustomized.IsEmpty())
		{
			if (ObjectsBeingCustomized[0])
			{
				TSharedPtr<class IPropertyHandle> ChainHandle = InPropertyHandle;
				while (ChainHandle.IsValid() && ChainHandle->GetProperty() != nullptr)
				{
					OutPropertyChain.AddHead(ChainHandle->GetProperty());
					OutPropertyArrayIndices.Insert(ChainHandle->GetIndexInArray(), 0);
					ChainHandle = ChainHandle->GetParentHandle();					
				}

				if (OutPropertyChain.GetHead() != nullptr)
				{
					OutPropertyChain.SetActiveMemberPropertyNode(OutPropertyChain.GetTail()->GetValue());
					bOutEnabled = !OutPropertyChain.GetHead()->GetValue()->HasAnyPropertyFlags(CPF_EditConst);
					return true;
				}
			}
		}
		return false;
	}

	// extracts the value for a nested property (for Example Settings.WorldTransform) from an outer owner
	template<typename ValueType>
	FORCEINLINE_DEBUGGABLE ValueType& ContainerUObjectToValueRef(UObject* InOwner, ValueType& InDefault, FEditPropertyChain& InPropertyChain, TArray<int32> &InPropertyArrayIndices) const
	{
		if (InPropertyChain.GetHead() == nullptr)
		{
			return InDefault;
		}
		
		FEditPropertyChain::TDoubleLinkedListNode* PropertyNode = InPropertyChain.GetHead();
		uint8* MemoryPtr = (uint8*)InOwner;
		int32 ChainIndex = 0;
		do
		{
			const FProperty* Property = PropertyNode->GetValue();
			MemoryPtr = Property->ContainerPtrToValuePtr<uint8>(MemoryPtr);

			PropertyNode = PropertyNode->GetNextNode();
			ChainIndex++;
			
			if(InPropertyArrayIndices.IsValidIndex(ChainIndex))
			{
				const int32 ArrayIndex = InPropertyArrayIndices[ChainIndex];
				if(ArrayIndex != INDEX_NONE)
				{
					const FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property->GetOwnerProperty());
					check(ArrayProperty);
					
					FScriptArrayHelper ArrayHelper(ArrayProperty, MemoryPtr);
					if(!ArrayHelper.IsValidIndex(ArrayIndex))
					{
						return InDefault;
					}
					MemoryPtr = ArrayHelper.GetRawPtr(ArrayIndex);

					// skip to the next property node already
					PropertyNode = PropertyNode->GetNextNode();
					ChainIndex++;
				}
			}

		}
		while (PropertyNode);

		return *(ValueType*)MemoryPtr;
	}

	// specializations for FEulerTransform and FRotator at the end of this file
	template<typename ValueType>
	static bool IsQuaternionBasedRotation() { return true; }

	// returns the numeric value of a vector component (or empty optional for varying values)
	template<typename VectorType, typename NumericType>
	TOptional<NumericType> GetVectorComponent(TSharedRef<class IPropertyHandle> InPropertyHandle, int32 InComponent) 
	{
		TOptional<NumericType> Result;
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return Result;
		}
	
		for(const TObjectPtr<UObject>& Object : ObjectsBeingCustomized)
		{
			if(Object && InPropertyHandle->IsValidHandle())
			{
				static VectorType ZeroVector = VectorType();
				const VectorType& Vector = ContainerUObjectToValueRef<VectorType>(Object.Get(), ZeroVector, PropertyChain, PropertyArrayIndices);
				NumericType Component = Vector[InComponent];
				if(Result.IsSet())
				{
					if(!FMath::IsNearlyEqual(Result.GetValue(), Component))
					{
						return TOptional<NumericType>();
					}
				}
				else
				{
					Result = Component;
				}
			}
		}
		return Result;
	};

	// called when a numeric value of a vector component is changed
	template<typename VectorType, typename NumericType>
	void OnVectorComponentChanged(TSharedRef<class IPropertyHandle> InPropertyHandle, int32 InComponent, NumericType InValue, bool bIsCommit, ETextCommit::Type InCommitType = ETextCommit::Default)
	{
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}
	
		TArray<UObject*> ObjectsView;
		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if (Object)
			{
				ObjectsView.Add(Object.Get());
			}
		}
		FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), bIsCommit ? EPropertyChangeType::ValueSet : EPropertyChangeType::Interactive, ObjectsView);
		FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);

		URigVMController* Controller = nullptr;
		if(BlueprintBeingCustomized && GraphBeingCustomized)
		{
			Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
			if(bIsCommit)
			{
				Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InPropertyHandle->GetProperty()->GetName()));
			}
		}

		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if(Object && InPropertyHandle->IsValidHandle())
			{
				static VectorType ZeroVector = VectorType();
				VectorType& Vector = ContainerUObjectToValueRef<VectorType>(Object.Get(), ZeroVector, PropertyChain, PropertyArrayIndices);
				VectorType PreviousVector = Vector;
				Vector[InComponent] = InValue;
					
				if(!PreviousVector.Equals(Vector))
				{
					Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
					InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}
			}
		}

		if(Controller && bIsCommit)
		{
			Controller->CloseUndoBracket();
		}
	};

	// specializations for FVector and FVector4 at the end of this file
	template<typename VectorType>
	FORCEINLINE void ExtendVectorArgs(TSharedRef<class IPropertyHandle> InPropertyHandle, void* ArgumentsPtr) {}

	template<typename VectorType, int32 NumberOfComponents>
	FORCEINLINE void CustomizeVector(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		typedef typename VectorType::FReal NumericType;
		typedef SNumericVectorInputBox<NumericType, VectorType, NumberOfComponents> SLocalVectorInputBox;

		FEditPropertyChain PropertyChain;
        TArray<int32> PropertyArrayIndices;
        bool bEnabled;
        if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
        {
        	return;
        }

		typename SLocalVectorInputBox::FArguments Args;
		Args.Font(IDetailLayoutBuilder::GetDetailFont());
		Args.IsEnabled(bEnabled);
		Args.AllowSpin(true);
		Args.SpinDelta(0.01f);
		Args.bColorAxisLabels(true);
		Args.X_Lambda([this, InPropertyHandle]()
		{
			return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 0);
		});
		Args.OnXChanged_Lambda([this, InPropertyHandle](NumericType Value)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 0, Value, false);
		});
		Args.OnXCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 0, Value, true, CommitType);
		});
		Args.Y_Lambda([this, InPropertyHandle]()
		{
			return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 1);
		});
		Args.OnYChanged_Lambda([this, InPropertyHandle](NumericType Value)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 1, Value, false);
		});
		Args.OnYCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
		{
			OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 1, Value, true, CommitType);
		});

		ExtendVectorArgs<VectorType>(InPropertyHandle, &Args);

		StructBuilder.AddProperty(InPropertyHandle).CustomWidget()
		.IsEnabled(bEnabled)
		.NameContent()
		[
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(375.f)
		.MaxDesiredWidth(375.f)
		.HAlign(HAlign_Left)
		[
			SArgumentNew(Args, SLocalVectorInputBox)
		];
	}

	// returns the rotation for rotator or quaternions (or empty optional for varying values)
	template<typename RotationType>
	TOptional<RotationType> GetRotation(TSharedRef<class IPropertyHandle> InPropertyHandle)
	{
		TOptional<RotationType> Result;
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return Result;
		}
		
		for(const TObjectPtr<UObject>& Object : ObjectsBeingCustomized)
		{
			if(Object && InPropertyHandle->IsValidHandle())
			{
				static RotationType ZeroRotation = RotationType();
				const RotationType& Rotation = ContainerUObjectToValueRef<RotationType>(Object.Get(), ZeroRotation, PropertyChain, PropertyArrayIndices);
				if(Result.IsSet())
				{
					if(!Rotation.Equals(Result.GetValue()))
					{
						return TOptional<RotationType>();
					}
				}
				else
				{
					Result = Rotation;
				}
			}
		}
		return Result;
	};

	// called when a rotation value is changed / committed
	template<typename RotationType>
	void OnRotationChanged(TSharedRef<class IPropertyHandle> InPropertyHandle, RotationType InValue, bool bIsCommit, ETextCommit::Type InCommitType = ETextCommit::Default)
	{
		FEditPropertyChain PropertyChain;
        TArray<int32> PropertyArrayIndices;
        bool bEnabled;
        if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
        {
        	return;
        }
	
		TArray<UObject*> ObjectsView;
		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if (Object)
			{
				ObjectsView.Add(Object.Get());
			}
		}
		FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), bIsCommit ? EPropertyChangeType::ValueSet : EPropertyChangeType::Interactive, ObjectsView);
		FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);

		URigVMController* Controller = nullptr;
		if(BlueprintBeingCustomized && GraphBeingCustomized)
		{
			Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
			if(bIsCommit)
			{
				Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InPropertyHandle->GetProperty()->GetName()));
			}
		}

		for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
		{
			const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
			if(Object && InPropertyHandle->IsValidHandle())
			{
				static RotationType ZeroRotation = RotationType();
				RotationType& Rotation = ContainerUObjectToValueRef<RotationType>(Object.Get(), ZeroRotation, PropertyChain, PropertyArrayIndices);
				RotationType PreviousRotation = Rotation;
				Rotation = InValue;
					
				if(!PreviousRotation.Equals(Rotation))
				{
					Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
					InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
				}
			}
		}

		if(Controller && bIsCommit)
		{
			Controller->CloseUndoBracket();
		}
	};

	// specializations for FRotator and FQuat at the end of this file
	template<typename RotationType>
	FORCEINLINE void ExtendRotationArgs(TSharedRef<class IPropertyHandle> InPropertyHandle, void* ArgumentsPtr) {}

	// add the widget for a rotation (rotator or quat)
	template<typename RotationType>
	FORCEINLINE void CustomizeRotation(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}
		
		typedef typename RotationType::FReal NumericType;
		typedef SAdvancedRotationInputBox<NumericType> SLocalRotationInputBox;
		typename SLocalRotationInputBox::FArguments Args;
		Args.Font(IDetailLayoutBuilder::GetDetailFont());
		Args.IsEnabled(bEnabled);
		Args.AllowSpin(true);
		Args.bColorAxisLabels(true);

		ExtendRotationArgs<RotationType>(InPropertyHandle, &Args);
		
		StructBuilder.AddProperty(InPropertyHandle).CustomWidget()
		.IsEnabled(bEnabled)
		.NameContent()
		[
			
			InPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MinDesiredWidth(375.f)
		.MaxDesiredWidth(375.f)
		.HAlign(HAlign_Left)
		[
			SArgumentNew(Args, SLocalRotationInputBox)
		];
	}

	// add the widget for a transform / euler transform
	template<typename TransformType>
	FORCEINLINE void CustomizeTransform(TSharedRef<class IPropertyHandle> InPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
	{
		FEditPropertyChain PropertyChain;
		TArray<int32> PropertyArrayIndices;
		bool bEnabled;
		if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
		{
			return;
		}
		
		typedef typename TransformType::FReal FReal;
		typename SAdvancedTransformInputBox<TransformType>::FArguments WidgetArgs;
		WidgetArgs.IsEnabled(bEnabled);
		WidgetArgs.AllowEditRotationRepresentation(true);
		WidgetArgs.UseQuaternionForRotation(IsQuaternionBasedRotation<TransformType>());

		static TransformType Identity = TransformType::Identity;
		TransformType DefaultValue = ContainerUObjectToValueRef<TransformType>(ObjectsBeingCustomized[0]->GetClass()->GetDefaultObject(), Identity, PropertyChain, PropertyArrayIndices);

		WidgetArgs.DiffersFromDefault_Lambda([this, InPropertyHandle, DefaultValue](ESlateTransformComponent::Type InTransformComponent) -> bool
		{
			FEditPropertyChain PropertyChain;
			TArray<int32> PropertyArrayIndices;
			bool bEnabled;
			if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
			{
				return false;
			}
			
			for(const TObjectPtr<UObject>& Object : ObjectsBeingCustomized)
			{
				if(Object && InPropertyHandle->IsValidHandle())
				{
					const TransformType& Transform = ContainerUObjectToValueRef<TransformType>(Object.Get(), Identity, PropertyChain, PropertyArrayIndices);

					switch(InTransformComponent)
					{
						case ESlateTransformComponent::Location:
						{
							if(!Transform.GetLocation().Equals(DefaultValue.GetLocation()))
							{
								return true;
							}
							break;
						}
						case ESlateTransformComponent::Rotation:
						{
							if(!Transform.Rotator().Equals(DefaultValue.Rotator()))
							{
								return true;
							}
							break;
						}
						case ESlateTransformComponent::Scale:
						{
							if(!Transform.GetScale3D().Equals(DefaultValue.GetScale3D()))
							{
								return true;
							}
							break;
						}
						default:
						{
							break;
						}
					}
				}
			}
			return false;
		});

		WidgetArgs.OnGetNumericValue_Lambda([this, InPropertyHandle](
			ESlateTransformComponent::Type InTransformComponent,
			ESlateRotationRepresentation::Type InRotationRepresentation,
			ESlateTransformSubComponent::Type InTransformSubComponent) -> TOptional<FReal>
		{
			TOptional<FReal> Result;
			FEditPropertyChain PropertyChain;
			TArray<int32> PropertyArrayIndices;
			bool bEnabled;
			if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
			{
				return Result;
			}
			
			for(const TObjectPtr<UObject>& Object : ObjectsBeingCustomized)
			{
				if(Object && InPropertyHandle->IsValidHandle())
				{
					const TransformType& Transform = ContainerUObjectToValueRef<TransformType>(Object.Get(), Identity, PropertyChain, PropertyArrayIndices);
					
					TOptional<FReal> Value = SAdvancedTransformInputBox<TransformType>::GetNumericValueFromTransform(
						Transform,
						InTransformComponent,
						InRotationRepresentation,
						InTransformSubComponent
						);

					if(Value.IsSet())
					{
						if(Result.IsSet())
						{
							if(!FMath::IsNearlyEqual(Result.GetValue(), Value.GetValue()))
							{
								return TOptional<FReal>();
							}
						}
						else
						{
							Result = Value;
						}
					}
				}
			}
			return Result;
		});
		

		auto OnNumericValueChanged = [this, InPropertyHandle](
			ESlateTransformComponent::Type InTransformComponent, 
			ESlateRotationRepresentation::Type InRotationRepresentation, 
			ESlateTransformSubComponent::Type InSubComponent,
			FReal InValue,
			bool bIsCommit,
			ETextCommit::Type InCommitType = ETextCommit::Default)
		{
			FEditPropertyChain PropertyChain;
			TArray<int32> PropertyArrayIndices;
			bool bEnabled;
			if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
			{
				return;
			}
			
			TArray<UObject*> ObjectsView;
			for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
			{
				const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
				if (Object)
				{
					ObjectsView.Add(Object.Get());
				}
			}
			FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), bIsCommit ? EPropertyChangeType::ValueSet : EPropertyChangeType::Interactive, ObjectsView);
			FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);

			URigVMController* Controller = nullptr;
			if(BlueprintBeingCustomized && GraphBeingCustomized)
			{
				Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
				if(bIsCommit)
				{
					Controller->OpenUndoBracket(FString::Printf(TEXT("Set %s"), *InPropertyHandle->GetProperty()->GetName()));
				}
			}

			for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
			{
				const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
				if(Object && InPropertyHandle->IsValidHandle())
				{
					TransformType& Transform = ContainerUObjectToValueRef<TransformType>(Object.Get(), Identity, PropertyChain, PropertyArrayIndices);
					TransformType PreviousTransform = Transform;
					
					SAdvancedTransformInputBox<TransformType>::ApplyNumericValueChange(
						Transform,
						InValue,
						InTransformComponent,
						InRotationRepresentation,
						InSubComponent);

					if(!PreviousTransform.Equals(Transform))
					{
						Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
						InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
					}
				}
			}

			if(Controller && bIsCommit)
			{
				Controller->CloseUndoBracket();
			}
		};

		WidgetArgs.OnNumericValueChanged_Lambda([OnNumericValueChanged](
			ESlateTransformComponent::Type InTransformComponent, 
			ESlateRotationRepresentation::Type InRotationRepresentation, 
			ESlateTransformSubComponent::Type InSubComponent,
			FReal InValue)
		{
			OnNumericValueChanged(InTransformComponent, InRotationRepresentation, InSubComponent, InValue, false);
		});

		WidgetArgs.OnNumericValueCommitted_Lambda([OnNumericValueChanged](
			ESlateTransformComponent::Type InTransformComponent, 
			ESlateRotationRepresentation::Type InRotationRepresentation, 
			ESlateTransformSubComponent::Type InSubComponent,
			FReal InValue, 
			ETextCommit::Type InCommitType)
		{
			OnNumericValueChanged(InTransformComponent, InRotationRepresentation, InSubComponent, InValue, true, InCommitType);
		});

		WidgetArgs.OnResetToDefault_Lambda([this, DefaultValue, InPropertyHandle](ESlateTransformComponent::Type InTransformComponent)
		{
			FEditPropertyChain PropertyChain;
			TArray<int32> PropertyArrayIndices;
			bool bEnabled;
			if (!GetPropertyChain(InPropertyHandle, PropertyChain, PropertyArrayIndices, bEnabled))
			{
				return;
			}
			
			URigVMController* Controller = nullptr;
			if(BlueprintBeingCustomized && GraphBeingCustomized)
			{
				Controller = BlueprintBeingCustomized->GetController(GraphBeingCustomized);
				if(Controller)
				{
					Controller->OpenUndoBracket(FString::Printf(TEXT("Reset %s to Default"), *InPropertyHandle->GetProperty()->GetName()));
				}
			}

			TArray<UObject*> ObjectsView;
			for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
			{
				const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
				if (Object)
				{
					ObjectsView.Add(Object.Get());
				}
			}
			FPropertyChangedEvent PropertyChangedEvent(InPropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, ObjectsView);
			FPropertyChangedChainEvent PropertyChangedChainEvent(PropertyChain, PropertyChangedEvent);
			
			for(int32 Index = 0; Index < ObjectsBeingCustomized.Num(); Index++)
			{
				const TObjectPtr<UObject>& Object = ObjectsBeingCustomized[Index];
				if(Object && InPropertyHandle->IsValidHandle())
				{
					TransformType& Transform = ContainerUObjectToValueRef<TransformType>(Object.Get(), Identity, PropertyChain, PropertyArrayIndices);
					TransformType PreviousTransform = Transform;

					switch(InTransformComponent)
					{
						case ESlateTransformComponent::Location:
						{
							Transform.SetLocation(DefaultValue.GetLocation());
							break;
						}
						case ESlateTransformComponent::Rotation:
						{
							Transform.SetRotation(DefaultValue.GetRotation());
							break;
						}
						case ESlateTransformComponent::Scale:
						{
							Transform.SetScale3D(DefaultValue.GetScale3D());
							break;
						}
						case ESlateTransformComponent::Max:
						default:
						{
							Transform.SetLocation(DefaultValue.GetLocation());
							break;
						}
					}

					
					if(!PreviousTransform.Equals(Transform))
					{
						Object->PostEditChangeChainProperty(PropertyChangedChainEvent);
						InPropertyHandle->NotifyPostChange(PropertyChangedEvent.ChangeType);
					}
				}
			}

			if(Controller)
			{
				Controller->CloseUndoBracket();
			}
		});

		SAdvancedTransformInputBox<TransformType>::ConstructGroupedTransformRows(
			StructBuilder,
			InPropertyHandle->GetPropertyDisplayName(),
			InPropertyHandle->GetToolTipText(),
			WidgetArgs);
	}
		
	UScriptStruct* ScriptStruct;
	UControlRigBlueprint* BlueprintBeingCustomized;
	URigVMGraph* GraphBeingCustomized;
	TArray<TObjectPtr<UObject>> ObjectsBeingCustomized; 
};

template<>
FORCEINLINE bool FControlRigGraphMathTypeDetails::IsQuaternionBasedRotation<FEulerTransform>() { return false; }

template<>
FORCEINLINE bool FControlRigGraphMathTypeDetails::IsQuaternionBasedRotation<FRotator>() { return false; }

template<>
FORCEINLINE void FControlRigGraphMathTypeDetails::ExtendVectorArgs<FVector>(TSharedRef<class IPropertyHandle> InPropertyHandle, void* ArgumentsPtr)
{
	using VectorType = FVector;
	typedef typename VectorType::FReal NumericType;
	typedef SNumericVectorInputBox<NumericType, VectorType, 3> SLocalVectorInputBox;

	typename SLocalVectorInputBox::FArguments& Args = *(typename SLocalVectorInputBox::FArguments*)ArgumentsPtr; 
	Args
	.Z_Lambda([this, InPropertyHandle]()
	{
		return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 2);
	})
	.OnZChanged_Lambda([this, InPropertyHandle](NumericType Value)
	{
		OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 2, Value, false);
	})
	.OnZCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
	{
		OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 2, Value, true, CommitType);
	});
}

template<>
FORCEINLINE void FControlRigGraphMathTypeDetails::ExtendVectorArgs<FVector4>(TSharedRef<class IPropertyHandle> InPropertyHandle, void* ArgumentsPtr)
{
	using VectorType = FVector4;
	typedef typename VectorType::FReal NumericType;
	typedef SNumericVectorInputBox<NumericType, VectorType, 4> SLocalVectorInputBox;

	typename SLocalVectorInputBox::FArguments& Args = *(typename SLocalVectorInputBox::FArguments*)ArgumentsPtr; 
	Args
	.Z_Lambda([this, InPropertyHandle]()
	{
		return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 2);
	})
	.OnZChanged_Lambda([this, InPropertyHandle](NumericType Value)
	{
		OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 2, Value, false);
	})
	.OnZCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
	{
		OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 2, Value, true, CommitType);
	})
	.W_Lambda([this, InPropertyHandle]()
	{
		return GetVectorComponent<VectorType, NumericType>(InPropertyHandle, 3);
	})
	.OnWChanged_Lambda([this, InPropertyHandle](NumericType Value)
	{
		OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 3, Value, false);
	})
	.OnWCommitted_Lambda([this, InPropertyHandle](NumericType Value, ETextCommit::Type CommitType)
	{
		OnVectorComponentChanged<VectorType, NumericType>(InPropertyHandle, 3, Value, true, CommitType);
	});
}

template<>
FORCEINLINE void FControlRigGraphMathTypeDetails::ExtendRotationArgs<FQuat>(TSharedRef<class IPropertyHandle> InPropertyHandle, void* ArgumentsPtr)
{
	using RotationType = FQuat;
	typedef typename RotationType::FReal NumericType;
	typedef SAdvancedRotationInputBox<NumericType> SLocalRotationInputBox;
	typename SLocalRotationInputBox::FArguments& Args = *(typename SLocalRotationInputBox::FArguments*)ArgumentsPtr; 

	Args.Quaternion_Lambda([this, InPropertyHandle]() -> TOptional<RotationType>
	{
		return GetRotation<RotationType>(InPropertyHandle);
	});

	Args.OnQuaternionChanged_Lambda([this, InPropertyHandle](RotationType InValue)
	{
		OnRotationChanged<RotationType>(InPropertyHandle, InValue, false);
	});

	Args.OnQuaternionCommitted_Lambda([this, InPropertyHandle](RotationType InValue, ETextCommit::Type InCommitType)
	{
		OnRotationChanged<RotationType>(InPropertyHandle, InValue, true, InCommitType);
	});
}

template<>
FORCEINLINE void FControlRigGraphMathTypeDetails::ExtendRotationArgs<FRotator>(TSharedRef<class IPropertyHandle> InPropertyHandle, void* ArgumentsPtr)
{
	using RotationType = FRotator;
	typedef typename RotationType::FReal NumericType;
	typedef SAdvancedRotationInputBox<NumericType> SLocalRotationInputBox;
	typename SLocalRotationInputBox::FArguments& Args = *(typename SLocalRotationInputBox::FArguments*)ArgumentsPtr; 

	Args.Rotator_Lambda([this, InPropertyHandle]() -> TOptional<RotationType>
	{
		return GetRotation<RotationType>(InPropertyHandle);
	});

	Args.OnRotatorChanged_Lambda([this, InPropertyHandle](RotationType InValue)
	{
		OnRotationChanged<RotationType>(InPropertyHandle, InValue, false);
	});

	Args.OnRotatorCommitted_Lambda([this, InPropertyHandle](RotationType InValue, ETextCommit::Type InCommitType)
	{
		OnRotationChanged<RotationType>(InPropertyHandle, InValue, true, InCommitType);
	});
}
