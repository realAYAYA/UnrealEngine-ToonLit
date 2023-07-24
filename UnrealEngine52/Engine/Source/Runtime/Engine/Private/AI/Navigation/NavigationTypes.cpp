// Copyright Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavigationTypes.h"
#include "AI/NavigationSystemBase.h"
#include "AI/Navigation/NavQueryFilter.h"
#include "Engine/Level.h"
#include "EngineStats.h"
#include "Components/ShapeComponent.h"
#include "AI/Navigation/NavAreaBase.h"
#include "GameFramework/WorldSettings.h"
#include "WorldPartition/DataLayer/DataLayerAsset.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NavigationTypes)

DEFINE_STAT(STAT_Navigation_MetaAreaTranslation);

static const uint32 MAX_NAV_SEARCH_NODES = 2048;

namespace FNavigationSystem
{
	// these are totally arbitrary values, and it should haven happen these are ever used.
	// in any reasonable case UNavigationSystemV1::SupportedAgents should be filled in ini file
	// and only those values will be used
	const float FallbackAgentRadius = 35.f;
	const float FallbackAgentHeight = 144.f;

	bool IsLevelVisibilityChanging(const UObject* Object)
	{
		const UActorComponent* ObjectAsComponent = Cast<UActorComponent>(Object);
		if (ObjectAsComponent)
		{
			if (const ULevel* Level = ObjectAsComponent->GetComponentLevel())
			{
				return Level->HasVisibilityChangeRequestPending();
			}
		}
		else if (const AActor* Actor = Cast<AActor>(Object))
		{
			if (const ULevel* Level = Actor->GetLevel())
			{
				return Level->HasVisibilityChangeRequestPending();
			}
		}

		return false;
	}
	
	bool IsInBaseNavmesh(const UObject* Object)
	{
		const UActorComponent* ObjectAsComponent = Cast<UActorComponent>(Object);
		if (const AActor* Actor = ObjectAsComponent ? ObjectAsComponent->GetOwner() : Cast<AActor>(Object))
		{
			if (!Actor->HasDataLayers())
			{
				return true;
			}
		
			if (const UWorld* World = Object->GetWorld())
			{
				if (const AWorldSettings* WorldSettings = World->GetWorldSettings())
				{
					const TArray<TObjectPtr<UDataLayerAsset>>& BaseNavmeshLayers = WorldSettings->BaseNavmeshDataLayers;
					for (const TObjectPtr<UDataLayerAsset>& DataLayer : BaseNavmeshLayers)
					{
						if (Actor->ContainsDataLayer(DataLayer))
						{
							return true;
						}
					}
				}
			}
		}

		return false;
	}	
}

//----------------------------------------------------------------------//
// FNavigationQueryFilter
//----------------------------------------------------------------------//
const uint32 FNavigationQueryFilter::DefaultMaxSearchNodes = MAX_NAV_SEARCH_NODES;

//----------------------------------------------------------------------//
// FNavPathType
//----------------------------------------------------------------------//
uint32 FNavPathType::NextUniqueId = 0;

//----------------------------------------------------------------------//
// FNavDataConfig
//----------------------------------------------------------------------//
PRAGMA_DISABLE_DEPRECATION_WARNINGS
FNavDataConfig::FNavDataConfig(float Radius, float Height)
	: FNavAgentProperties(Radius, Height)
	, Name(TEXT("Default"))
	, Color(38, 75, 0, 164) // do not change this default value or the universe will explode!
	, DefaultQueryExtent(DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_HORIZONTAL, DEFAULT_NAV_QUERY_EXTENT_VERTICAL)
	, NavDataClass(FNavigationSystem::GetDefaultNavDataClass())
{
}

FNavDataConfig::FNavDataConfig(const FNavDataConfig& Other)
	: FNavAgentProperties(Other)
	, Name(Other.Name)
	, Color(Other.Color)
	, DefaultQueryExtent(Other.DefaultQueryExtent)
	, NavDataClass(Other.NavDataClass)
{
}
PRAGMA_ENABLE_DEPRECATION_WARNINGS

void FNavDataConfig::SetNavDataClass(UClass* InNavDataClass)
{
	NavDataClass = InNavDataClass;
}

void FNavDataConfig::SetNavDataClass(TSoftClassPtr<AActor> InNavDataClass)
{
	NavDataClass = InNavDataClass;
}

void FNavDataConfig::Invalidate()
{
	new(this) FNavAgentProperties();
	SetNavDataClass(nullptr);
}

FString FNavDataConfig::GetDescription() const
{
	return FString::Printf(TEXT("Name %s class %s agent radius %.1f")
		, *Name.ToString(), *NavDataClass.ToString(), AgentRadius);
}
//----------------------------------------------------------------------//
// FNavigationRelevantData
//----------------------------------------------------------------------//
bool FNavigationRelevantData::FCollisionDataHeader::IsValid(const uint8* RawData, int32 RawDataSize)
{
	const int32 HeaderSize = sizeof(FCollisionDataHeader);
	return (RawDataSize == 0) || ((RawDataSize >= HeaderSize) && (((const FCollisionDataHeader*)RawData)->DataSize == RawDataSize));
}

bool FNavigationRelevantData::HasPerInstanceTransforms() const
{
	return NavDataPerInstanceTransformDelegate.IsBound();
}

bool FNavigationRelevantData::IsMatchingFilter(const FNavigationRelevantDataFilter& Filter) const
{
	if (Filter.bExcludeLoadedData && bLoadedData)
	{
		return false;
	}
	return (Filter.bIncludeGeometry && HasGeometry()) ||
		(Filter.bIncludeOffmeshLinks && (Modifiers.HasPotentialLinks() || Modifiers.HasLinks())) ||
		(Filter.bIncludeAreas && Modifiers.HasAreas()) ||
		(Filter.bIncludeMetaAreas && Modifiers.HasMetaAreas());
}

void FNavigationRelevantData::Shrink()
{
	CollisionData.Shrink();
	VoxelData.Shrink();
	Modifiers.Shrink();
}

bool FNavigationRelevantData::IsCollisionDataValid() const
{
	const bool bIsValid = FCollisionDataHeader::IsValid(CollisionData.GetData(), CollisionData.Num());
	if (!ensure(bIsValid))
	{
		UE_LOG(LogNavigation, Error, TEXT("NavOctree element has corrupted collision data! Owner:%s Bounds:%s"), *GetNameSafe(GetOwner()), *Bounds.ToString());
		return false;
	}

	return true;
}

//----------------------------------------------------------------------//
// FNavigationQueryFilter
//----------------------------------------------------------------------//
FNavigationQueryFilter::FNavigationQueryFilter(const FNavigationQueryFilter& Source)
{
	Assign(Source);
}

FNavigationQueryFilter::FNavigationQueryFilter(const FNavigationQueryFilter* Source)
	: MaxSearchNodes(DefaultMaxSearchNodes)
{
	if (Source != NULL)
	{
		Assign(*Source);
	}
}

FNavigationQueryFilter::FNavigationQueryFilter(const FSharedNavQueryFilter Source)
	: MaxSearchNodes(DefaultMaxSearchNodes)
{
	if (Source.IsValid())
	{
		SetFilterImplementation(Source->GetImplementation());
	}
}

FNavigationQueryFilter& FNavigationQueryFilter::operator=(const FNavigationQueryFilter& Source)
{
	Assign(Source);
	return *this;
}

void FNavigationQueryFilter::Assign(const FNavigationQueryFilter& Source)
{
	if (Source.GetImplementation() != NULL)
	{
		QueryFilterImpl = Source.QueryFilterImpl;
	}
	MaxSearchNodes = Source.GetMaxSearchNodes();
}

FSharedNavQueryFilter FNavigationQueryFilter::GetCopy() const
{
	FSharedNavQueryFilter Copy = MakeShareable(new FNavigationQueryFilter());
	Copy->QueryFilterImpl = MakeShareable(QueryFilterImpl->CreateCopy());
	Copy->MaxSearchNodes = MaxSearchNodes;

	return Copy;
}

void FNavigationQueryFilter::SetAreaCost(uint8 AreaType, float Cost)
{
	check(QueryFilterImpl.IsValid());
	QueryFilterImpl->SetAreaCost(AreaType, Cost);
}

void FNavigationQueryFilter::SetFixedAreaEnteringCost(uint8 AreaType, float Cost)
{
	check(QueryFilterImpl.IsValid());
	QueryFilterImpl->SetFixedAreaEnteringCost(AreaType, Cost);
}

void FNavigationQueryFilter::SetExcludedArea(uint8 AreaType)
{
	QueryFilterImpl->SetExcludedArea(AreaType);
}

void FNavigationQueryFilter::SetAllAreaCosts(const TArray<float>& CostArray)
{
	SetAllAreaCosts(CostArray.GetData(), CostArray.Num());
}

void FNavigationQueryFilter::SetAllAreaCosts(const float* CostArray, const int32 Count)
{
	QueryFilterImpl->SetAllAreaCosts(CostArray, Count);
}

void FNavigationQueryFilter::GetAllAreaCosts(float* CostArray, float* FixedCostArray, const int32 Count) const
{
	QueryFilterImpl->GetAllAreaCosts(CostArray, FixedCostArray, Count);
}

void FNavigationQueryFilter::SetIncludeFlags(uint16 Flags)
{
	QueryFilterImpl->SetIncludeFlags(Flags);
}

uint16 FNavigationQueryFilter::GetIncludeFlags() const
{
	return QueryFilterImpl->GetIncludeFlags();
}

void FNavigationQueryFilter::SetExcludeFlags(uint16 Flags)
{
	QueryFilterImpl->SetExcludeFlags(Flags);
}

uint16 FNavigationQueryFilter::GetExcludeFlags() const
{
	return QueryFilterImpl->GetExcludeFlags();
}

//----------------------------------------------------------------------//
// FNavAgentSelector
//----------------------------------------------------------------------//
FNavAgentSelector::FNavAgentSelector(const uint32 InBits)
	: PackedBits(InBits)
{
}

bool FNavAgentSelector::Serialize(FArchive& Ar)
{
	Ar << PackedBits;
	return true;
}

//----------------------------------------------------------------------//
// FNavHeightfieldSample
//----------------------------------------------------------------------//
FNavHeightfieldSamples::FNavHeightfieldSamples()
{
}

void FNavHeightfieldSamples::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	CumulativeResourceSize.AddDedicatedSystemMemoryBytes(sizeof(*this) + Heights.GetAllocatedSize() + Holes.GetAllocatedSize());
}

void FNavHeightfieldSamples::Empty()
{
	Heights.Empty();
	Holes.Empty();
}

//----------------------------------------------------------------------//
// FNavAgentProperties
//----------------------------------------------------------------------//
const FNavAgentProperties FNavAgentProperties::DefaultProperties;

FNavAgentProperties::FNavAgentProperties(const FNavAgentProperties& Other)
	: Super(Other)
	, AgentRadius(Other.AgentRadius)
	, AgentHeight(Other.AgentHeight)
	, AgentStepHeight(Other.AgentStepHeight)
	, NavWalkingSearchHeightScale(Other.NavWalkingSearchHeightScale)
	, PreferredNavData(Other.PreferredNavData)
{

}

void FNavAgentProperties::UpdateWithCollisionComponent(UShapeComponent* CollisionComponent)
{
	check(CollisionComponent != NULL);
	AgentRadius = FloatCastChecked<float>(CollisionComponent->Bounds.SphereRadius, UE::LWC::DefaultFloatPrecision);
}

bool FNavAgentProperties::IsNavDataMatching(const FNavAgentProperties& Other) const
{
	return (PreferredNavData == Other.PreferredNavData || PreferredNavData.IsNull() || Other.PreferredNavData.IsNull());
}

void FNavAgentProperties::SetPreferredNavData(TSubclassOf<AActor> NavDataClass)
{
	PreferredNavData = FSoftClassPath(NavDataClass.Get());
}

//----------------------------------------------------------------------//
// UNavAreaBase
//----------------------------------------------------------------------//
UNavAreaBase::UNavAreaBase(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsMetaArea = false;
}

TSubclassOf<UNavAreaBase> UNavAreaBase::PickAreaClassForAgent(const AActor& Actor, const FNavAgentProperties& NavAgent) const
{
	UE_CLOG(IsMetaArea(), LogNavigation, Warning, TEXT("UNavAreaBase::PickAreaClassForAgent called for meta class %s. Please override PickAreaClass.")
		, *(GetClass()->GetName()));

	return GetClass();
}

