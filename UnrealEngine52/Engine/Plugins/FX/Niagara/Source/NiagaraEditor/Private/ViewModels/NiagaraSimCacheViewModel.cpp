// Copyright Epic Games, Inc. All Rights Reserved.

#include "ViewModels/NiagaraSimCacheViewModel.h"
#include "NiagaraComponent.h"
#include "NiagaraSimCache.h"
#include "NiagaraEditorCommon.h"
#include "NiagaraDebuggerCommon.h"
#include "Serialization/MemoryReader.h"
#include "SNiagaraSimCacheTreeView.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "UObject/Package.h"

#define LOCTEXT_NAMESPACE "NiagaraSimCacheViewModel"

FNiagaraSimCacheViewModel::FNiagaraSimCacheViewModel()
{
}

FNiagaraSimCacheViewModel::~FNiagaraSimCacheViewModel()
{
	UNiagaraSimCache::OnCacheEndWrite.RemoveAll(this);
	bDelegatesAdded = false;
}

void FNiagaraSimCacheViewModel::Initialize(TWeakObjectPtr<UNiagaraSimCache> SimCache) 
{
	if (bDelegatesAdded == false)
	{
		bDelegatesAdded = true;
		UNiagaraSimCache::OnCacheEndWrite.AddSP(this, &FNiagaraSimCacheViewModel::OnCacheModified);
	}

	WeakSimCache = SimCache;
	UpdateComponentInfos();
	UpdateCachedFrame();
	SetupPreviewComponentAndInstance();
	
	OnSimCacheChangedDelegate.Broadcast();
	OnViewDataChangedDelegate.Broadcast(true);
}

void FNiagaraSimCacheViewModel::UpdateSimCache(const FNiagaraSystemSimCacheCaptureReply& Reply)
{
	UNiagaraSimCache* SimCache = nullptr;
	
	if (Reply.SimCacheData.Num() > 0)
	{
		SimCache = NewObject<UNiagaraSimCache>();

		FMemoryReader ArReader(Reply.SimCacheData);
		FObjectAndNameAsStringProxyArchive ProxyArReader(ArReader, false);
		SimCache->Serialize(ProxyArReader);
		bComponentFilterActive = false;
		UpdateComponentInfos();
		
	}
	else
	{
		UE_LOG(LogNiagaraEditor, Warning, TEXT("Debug Spreadsheet received empty sim cache data."));
	}
	Initialize(SimCache);
}

void FNiagaraSimCacheViewModel::SetupPreviewComponentAndInstance()
{
	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	UNiagaraSystem* System = SimCache ? SimCache->GetSystem(true) : nullptr;

	if(SimCache && System)
	{
		PreviewComponent = NewObject<UNiagaraComponent>(GetTransientPackage(), NAME_None, RF_Transient);
		PreviewComponent->CastShadow = 1;
		PreviewComponent->bCastDynamicShadow = 1;
		PreviewComponent->SetAllowScalability(false);
		PreviewComponent->SetAsset(System);
		PreviewComponent->SetForceSolo(true);
		PreviewComponent->SetAgeUpdateMode(ENiagaraAgeUpdateMode::DesiredAge);
		PreviewComponent->SetCanRenderWhileSeeking(false);
		PreviewComponent->Activate(true);
		PreviewComponent->SetSimCache(SimCache);
		PreviewComponent->SetRelativeLocation(FVector::ZeroVector);
		PreviewComponent->SetDesiredAge(SimCache->GetStartSeconds());
	}

	
}

FText FNiagaraSimCacheViewModel::GetComponentText(const FName ComponentName, const int32 InstanceIndex) const
{
	const FComponentInfo* ComponentInfo = ComponentInfos->FindByPredicate([ComponentName](const FComponentInfo& FoundInfo) { return FoundInfo.Name == ComponentName; });

	if (ComponentInfo)
	{
		if (InstanceIndex >= 0 && InstanceIndex < NumInstances)
		{
			if (ComponentInfo->bIsFloat)
			{
				const float Value = FloatComponents[(ComponentInfo->ComponentOffset * NumInstances) + InstanceIndex];
				return FText::AsNumber(Value);
			}
			else if (ComponentInfo->bIsHalf)
			{
				const FFloat16 Value = HalfComponents[(ComponentInfo->ComponentOffset * NumInstances) + InstanceIndex];
				return FText::AsNumber(Value.GetFloat());
			}
			else if (ComponentInfo->bIsInt32)
			{
				const int32 Value = Int32Components[(ComponentInfo->ComponentOffset * NumInstances) + InstanceIndex];
				if (ComponentInfo->bShowAsBool)
				{
					return Value == 0 ? LOCTEXT("False", "False") : LOCTEXT("True", "True");
				}
				else if (ComponentInfo->Enum != nullptr)
				{
					return ComponentInfo->Enum->GetDisplayNameTextByValue(Value);
				}
				else
				{
					return FText::AsNumber(Value);
				}
			}
		}
	}
	return LOCTEXT("Error", "Error");
}

void FNiagaraSimCacheViewModel::SetComponentFilterActive(bool bNewActive)
{
	if(bNewActive != bComponentFilterActive)
	{
		bComponentFilterActive = bNewActive;
		OnViewDataChangedDelegate.Broadcast(true);
	}
}

int32 FNiagaraSimCacheViewModel::GetNumFrames() const
{
	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	return SimCache ? SimCache->GetNumFrames() : 0;
}

void FNiagaraSimCacheViewModel::SetFrameIndex(const int32 InFrameIndex)
{
	FrameIndex = InFrameIndex;
	UpdateCachedFrame();
	const UNiagaraSimCache* SimCache = WeakSimCache.Get();
	if(PreviewComponent && SimCache)
	{
		const float Duration = SimCache->GetDurationSeconds();
		const int NumFrames = SimCache->GetNumFrames();
		const float StartSeconds = SimCache->GetStartSeconds();

		const float NormalizedFrame = FMath::Clamp( NumFrames == 0 ? 0.0f : float(InFrameIndex) / float(NumFrames - 1), 0.0f, 1.0f );
		const float DesiredAge = FMath::Clamp(StartSeconds + (Duration * NormalizedFrame), StartSeconds, StartSeconds + Duration);
		
		PreviewComponent->SetDesiredAge(DesiredAge);
	}
	OnViewDataChangedDelegate.Broadcast(false);
}

void FNiagaraSimCacheViewModel::SetEmitterIndex(const int32 InEmitterIndex)
{
	EmitterIndex = InEmitterIndex;
	UpdateCachedFrame();
	UpdateCurrentEntries();
	bComponentFilterActive = false;
	OnBufferChangedDelegate.Broadcast();
	OnViewDataChangedDelegate.Broadcast(true);
}

bool FNiagaraSimCacheViewModel::IsCacheValid()
{
	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	return SimCache ? SimCache->IsCacheValid() : false;
}

int32 FNiagaraSimCacheViewModel::GetNumEmitterLayouts()
{
	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	return SimCache ? SimCache->GetNumEmitters() : 0;
}

FName FNiagaraSimCacheViewModel::GetEmitterLayoutName(const int32 Index)
{
	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	return SimCache ? SimCache->GetEmitterName(Index) : NAME_None;
}

FNiagaraSimCacheViewModel::FOnViewDataChanged& FNiagaraSimCacheViewModel::OnViewDataChanged()
{
	return OnViewDataChangedDelegate;
}

FNiagaraSimCacheViewModel::FOnSimCacheChanged& FNiagaraSimCacheViewModel::OnSimCacheChanged()
{
	return OnSimCacheChangedDelegate;
}

FNiagaraSimCacheViewModel::FOnBufferChanged& FNiagaraSimCacheViewModel::OnBufferChanged()
{
	return OnBufferChangedDelegate;
}

void FNiagaraSimCacheViewModel::OnCacheModified(UNiagaraSimCache* SimCache)
{
	if ( UNiagaraSimCache* ThisSimCache = WeakSimCache.Get() )
	{
		if ( ThisSimCache == SimCache )
		{
			UpdateComponentInfos();
			UpdateCachedFrame();
			OnSimCacheChangedDelegate.Broadcast();
			OnViewDataChangedDelegate.Broadcast(true);
		}
	}
}

void FNiagaraSimCacheViewModel::UpdateCachedFrame()
{
	NumInstances = 0;
	FloatComponents.Empty();
	HalfComponents.Empty();
	Int32Components.Empty();
	

	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	if (SimCache == nullptr)
	{
		return;
	}

	if ( FrameIndex < 0 || FrameIndex >= SimCache->GetNumFrames() )
	{
		return;
	}

	if ( EmitterIndex != INDEX_NONE && EmitterIndex >= SimCache->GetNumEmitters() )
	{
		return;
	}

	NumInstances = EmitterIndex == INDEX_NONE ? 1 : SimCache->GetEmitterNumInstances(EmitterIndex, FrameIndex);
	const FName EmitterName = EmitterIndex == INDEX_NONE ? NAME_None : SimCache->GetEmitterName(EmitterIndex);
	// Cached frame data needs to match the component info being viewed. Update here so they match.
	ComponentInfos = EmitterIndex == INDEX_NONE ? &SystemComponentInfos : &EmitterComponentInfos[EmitterIndex];

	SimCache->ForEachEmitterAttribute(EmitterIndex,
		[&](const FNiagaraSimCacheVariable& Variable)
		{
			// Pull in data
			SimCache->ReadAttribute(FloatComponents, HalfComponents, Int32Components, Variable.Variable.GetName(), EmitterName, FrameIndex);

			return true;
		}
	);
}

void FNiagaraSimCacheViewModel::UpdateComponentInfos()
{
	SystemComponentInfos.Empty();
	EmitterComponentInfos.Empty();
	FoundFloatComponents = 0;
	FoundHalfComponents = 0;
	FoundInt32Components = 0;
	
	UNiagaraSimCache* SimCache = WeakSimCache.Get();
	if (SimCache == nullptr)
	{
		return;
	}
	
	
	SimCache->ForEachEmitterAttribute(INDEX_NONE,
		[&](const FNiagaraSimCacheVariable& Variable)
		{
			// Build component info
			const FNiagaraTypeDefinition& TypeDef = Variable.Variable.GetType();
			if (TypeDef.IsEnum())
			{
				FComponentInfo& ComponentInfo = SystemComponentInfos.AddDefaulted_GetRef();
				ComponentInfo.Name = Variable.Variable.GetName();
				ComponentInfo.ComponentOffset = FoundInt32Components++;
				ComponentInfo.bIsInt32 = true;
				ComponentInfo.Enum = TypeDef.GetEnum();
			}
			else
			{
				BuildComponentInfos(Variable.Variable.GetName(), TypeDef.GetScriptStruct(), SystemComponentInfos);
			}

			return true;
		}
	);

	for (int32 i = 0; i < SimCache->GetNumEmitters(); ++i)
	{
		TArray<FComponentInfo>& CurrentComponentInfos = EmitterComponentInfos.AddDefaulted_GetRef();

		FoundFloatComponents = 0;
		FoundHalfComponents = 0;
		FoundInt32Components = 0;
		
		SimCache->ForEachEmitterAttribute(i,
		[&](const FNiagaraSimCacheVariable& Variable)
			{
				// Build component info
				const FNiagaraTypeDefinition& TypeDef = Variable.Variable.GetType();
				if (TypeDef.IsEnum())
				{
					FComponentInfo& ComponentInfo = CurrentComponentInfos.AddDefaulted_GetRef();
					ComponentInfo.Name = Variable.Variable.GetName();
					ComponentInfo.ComponentOffset = FoundInt32Components++;
					ComponentInfo.bIsInt32 = true;
					ComponentInfo.Enum = TypeDef.GetEnum();
				}
				else
				{
					BuildComponentInfos(Variable.Variable.GetName(), TypeDef.GetScriptStruct(), CurrentComponentInfos);
				}

				return true;
			}
		);
	}
}

void FNiagaraSimCacheViewModel::BuildTreeItemChildren(TSharedPtr<FNiagaraSimCacheTreeItem> InTreeItem, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView)
{
	FNiagaraSimCacheTreeItem* TreeItem = InTreeItem.Get();
	UNiagaraSimCache* SimCache = WeakSimCache.Get();

	if(TreeItem && SimCache)
	{
		int32 BufferIndex = TreeItem->GetBufferIndex();
		
		SimCache->ForEachEmitterAttribute(BufferIndex,
	[&](const FNiagaraSimCacheVariable& Variable)
			{
				FNiagaraTypeDefinition TypeDef = Variable.Variable.GetType();
			

				TSharedRef<FNiagaraSimCacheComponentTreeItem> CurrentItem = MakeShared<FNiagaraSimCacheComponentTreeItem>(OwningTreeView);
				
				CurrentItem->SetDisplayName(FText::FromName(Variable.Variable.GetName()));
				CurrentItem->SetFilterName(Variable.Variable.GetName().ToString());
				CurrentItem->TypeDef = TypeDef;
				CurrentItem->BufferIndex = TreeItem->GetBufferIndex();
				//CurrentItem->RootItem = TreeItem;

				TreeItem->AddChild(CurrentItem);
				
				if(!TypeDef.IsEnum() && !FNiagaraTypeDefinition::IsScalarDefinition(TypeDef))
				{
					RecursiveBuildTreeItemChildren(TreeItem, CurrentItem, TypeDef, OwningTreeView);
				}
				return true;
			}
		);
		
	}
}

void FNiagaraSimCacheViewModel::RecursiveBuildTreeItemChildren(FNiagaraSimCacheTreeItem* Root,
	TSharedRef<FNiagaraSimCacheComponentTreeItem> Parent, FNiagaraTypeDefinition TypeDefinition, TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView)
{
	UScriptStruct* Struct = TypeDefinition.GetScriptStruct();

	for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		TSharedRef<FNiagaraSimCacheComponentTreeItem> CurrentItem = MakeShared<FNiagaraSimCacheComponentTreeItem> (OwningTreeView);
		
		FString PropertyName = Property->GetName();
		
		CurrentItem->SetDisplayName(FText::FromString(PropertyName));
		CurrentItem->SetFilterName(Parent->GetFilterName().Append(".").Append(PropertyName));
		CurrentItem->SetBufferIndex(Root->GetBufferIndex());
		//CurrentItem->RootItem = Root;

		Parent->AddChild(CurrentItem);

		if(Property->IsA(FStructProperty::StaticClass()))
		{
			const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
			UScriptStruct* FriendlyStruct = FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(StructProperty->Struct, ENiagaraStructConversion::Simulation);
			FNiagaraTypeDefinition StructTypeDef = FNiagaraTypeDefinition(FriendlyStruct);
			CurrentItem->TypeDef = StructTypeDef;
			RecursiveBuildTreeItemChildren(Root, CurrentItem, StructTypeDef, OwningTreeView);
		}
		else if (Property->IsA(FNumericProperty::StaticClass()))
		{
			if(Property->IsA(FIntProperty::StaticClass()))
			{
				CurrentItem->TypeDef = FNiagaraTypeDefinition::GetIntDef();
			}
			else if (Property->IsA(FFloatProperty::StaticClass()))
			{
				CurrentItem->TypeDef = FNiagaraTypeDefinition::GetFloatDef();
			}
		}
		else if (Property->IsA(FBoolProperty::StaticClass()))
		{
			CurrentItem->TypeDef = FNiagaraTypeDefinition::GetBoolDef();
		}
	}
}

void FNiagaraSimCacheViewModel::BuildEntries(TWeakPtr<SNiagaraSimCacheTreeView> OwningTreeView)
{
	RootEntries.Empty();
	BufferEntries.Empty();
	
	const TSharedRef<FNiagaraSimCacheTreeItem> SharedSystemTreeItem = MakeShared<FNiagaraSimCacheTreeItem>(OwningTreeView);
	const TSharedRef<FNiagaraSimCacheOverviewSystemItem> SharedSystemBufferItem = MakeShared<FNiagaraSimCacheOverviewSystemItem>();

	SharedSystemTreeItem->SetDisplayName(LOCTEXT("SystemInstance", "System Instance"));
	SharedSystemBufferItem->SetDisplayName(LOCTEXT("SystemInstance", "System Instance"));

	RootEntries.Add(SharedSystemTreeItem);
	BufferEntries.Add(SharedSystemBufferItem);
		
	BuildTreeItemChildren(SharedSystemTreeItem, OwningTreeView);
		
	for(int32 i = 0; i < GetNumEmitterLayouts(); i++)
	{
		const TSharedRef<FNiagaraSimCacheEmitterTreeItem> CurrentEmitterItem = MakeShared<FNiagaraSimCacheEmitterTreeItem>(OwningTreeView);
		const TSharedRef<FNiagaraSimCacheOverviewEmitterItem> CurrentEmitterBufferItem = MakeShared<FNiagaraSimCacheOverviewEmitterItem>();
		
		CurrentEmitterItem->SetDisplayName(FText::FromName(GetEmitterLayoutName(i)));
		CurrentEmitterBufferItem->SetDisplayName(FText::FromName(GetEmitterLayoutName(i)));
		
		CurrentEmitterItem->SetBufferIndex(i);
		CurrentEmitterBufferItem->SetBufferIndex(i);

		RootEntries.Add(CurrentEmitterItem);
		BufferEntries.Add(CurrentEmitterBufferItem);

		BuildTreeItemChildren(CurrentEmitterItem, OwningTreeView);
	}

	UpdateCurrentEntries();
}

void FNiagaraSimCacheViewModel::UpdateCurrentEntries()
{
	// The overview panel is in charge of building these entries.
	// Early out if they haven't been built yet.
	if(!RootEntries.IsValidIndex(EmitterIndex +1))
	{
		return;
	}
	CurrentRootEntries.Empty();
	CurrentRootEntries.Add(RootEntries[EmitterIndex + 1]);
}

TArray<TSharedRef<FNiagaraSimCacheTreeItem>>* FNiagaraSimCacheViewModel::GetCurrentRootEntries()
{
	return &CurrentRootEntries;
}

TArray<TSharedRef<FNiagaraSimCacheOverviewItem>>* FNiagaraSimCacheViewModel::GetBufferEntries()
{
	return &BufferEntries;
}

void FNiagaraSimCacheViewModel::BuildComponentInfos(const FName Name, const UScriptStruct* Struct, TArray<FComponentInfo>& InComponentInfos)
{
	int32 NumProperties = 0;
	for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		++NumProperties;
	}

	for (TFieldIterator<FProperty> PropertyIt(Struct, EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
	{
		FProperty* Property = *PropertyIt;
		const FName PropertyName = NumProperties > 1 ? FName(*FString::Printf(TEXT("%s.%s"), *Name.ToString(), *Property->GetName())) : Name;
		if (Property->IsA(FFloatProperty::StaticClass()))
		{
			FComponentInfo& ComponentInfo = InComponentInfos.AddDefaulted_GetRef();
			ComponentInfo.Name = PropertyName;
			ComponentInfo.ComponentOffset = FoundFloatComponents++;
			ComponentInfo.bIsFloat = true;
		}
		else if (Property->IsA(FUInt16Property::StaticClass()))
		{
			FComponentInfo& ComponentInfo = InComponentInfos.AddDefaulted_GetRef();
			ComponentInfo.Name = PropertyName;
			ComponentInfo.ComponentOffset = FoundHalfComponents++;
			ComponentInfo.bIsHalf = true;
		}
		else if (Property->IsA(FIntProperty::StaticClass()))
		{
			FComponentInfo& ComponentInfo = InComponentInfos.AddDefaulted_GetRef();
			ComponentInfo.Name = PropertyName;
			ComponentInfo.ComponentOffset = FoundInt32Components++;
			ComponentInfo.bIsInt32 = true;
			ComponentInfo.bShowAsBool = (NumProperties == 1) && (Struct == FNiagaraTypeDefinition::GetBoolStruct());
		}
		else if (Property->IsA(FBoolProperty::StaticClass()))
		{
			FComponentInfo& ComponentInfo = InComponentInfos.AddDefaulted_GetRef();
			ComponentInfo.Name = PropertyName;
			ComponentInfo.ComponentOffset = FoundInt32Components++;
			ComponentInfo.bIsInt32 = true;
			ComponentInfo.bShowAsBool = true;
		}
		else if (Property->IsA(FEnumProperty::StaticClass()))
		{
			FComponentInfo& ComponentInfo = InComponentInfos.AddDefaulted_GetRef();
			ComponentInfo.Name = PropertyName;
			ComponentInfo.ComponentOffset = FoundInt32Components++;
			ComponentInfo.bIsInt32 = true;
			ComponentInfo.Enum = CastFieldChecked<FEnumProperty>(Property)->GetEnum();
		}
		else if (Property->IsA(FStructProperty::StaticClass()))
		{
			const FStructProperty* StructProperty = CastFieldChecked<FStructProperty>(Property);
			BuildComponentInfos(PropertyName, FNiagaraTypeHelper::FindNiagaraFriendlyTopLevelStruct(StructProperty->Struct, ENiagaraStructConversion::Simulation), InComponentInfos);
		}
		else
		{
			// Fail
		}
	}
}

#undef LOCTEXT_NAMESPACE
