// Copyright Epic Games, Inc. All Rights Reserved.

#include "WidgetSnapshotUtils.h"

#include "LogVCamCore.h"
#include "Blueprint/WidgetTree.h"
#include "Serialization/ArchiveSerializedPropertyChain.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "Serialization/ObjectReader.h"
#include "Serialization/ObjectWriter.h"
#include "UI/WidgetSnapshots.h"
#include "Util/WidgetTreeUtils.h"

namespace UE::VCamCore::WidgetSnapshotUtils::Private
{
	bool FWidgetSnapshotSettings::IsClassAllowed(TSubclassOf<UWidget> Widget) const
	{
		for (UClass* Current = Widget; Current && Current != UWidget::StaticClass(); Current = Current->GetSuperClass())
		{
			if (AllowedWidgetClasses.Contains(Current))
			{
				return true;
			}
		}
		return false;
	}

	FWidgetTreeSnapshot TakeTreeHierarchySnapshot(UUserWidget& Widget, const FWidgetSnapshotSettings& SnapshotSettings)
	{
		if (!Widget.WidgetTree)
		{
			return {};
		}

		FWidgetTreeSnapshot Result;
		UE::VCamCore::ForEachWidgetToConsiderForVCam(Widget, [&Result, &SnapshotSettings](UWidget* Widget)
		{
			if (Widget && SnapshotSettings.IsClassAllowed(Widget->GetClass()))
			{
				Result.WidgetSnapshots.Add(Widget->GetFName(), TakeWidgetSnapshot(*Widget, SnapshotSettings.AllowedProperties));
			}
		});

		// Root widget may get re-instanced in the future which would change its name
		const FName RootWidgetName = Widget.GetFName();
		if (Result.WidgetSnapshots.Contains(RootWidgetName))
		{
			Result.RootWidget = RootWidgetName;
		}
			
		return Result;
	}
	
	bool ApplyTreeHierarchySnapshot(const FWidgetTreeSnapshot& Snapshot, UUserWidget& Widget)
	{
		if (!Widget.WidgetTree)
		{
			return false;
		}

		UE::VCamCore::ForEachWidgetToConsiderForVCam(Widget,[&Snapshot](UWidget* Widget)
		{
			if (const FWidgetSnapshot* WidgetSnapshot = Snapshot.WidgetSnapshots.Find(Widget->GetFName()))
			{
				ApplyWidgetSnapshot(*WidgetSnapshot, *Widget);
			}
		});

		// Root widget may have been re-instanced thus replacing its name
		const FName RootWidgetName = Widget.GetFName();
		if (!Snapshot.WidgetSnapshots.Contains(RootWidgetName))
		{
			const FWidgetSnapshot* RootWidgetSnapshot = Snapshot.WidgetSnapshots.Find(Snapshot.RootWidget);
			if (RootWidgetSnapshot && RootWidgetSnapshot->WidgetClass == Widget.GetClass())
			{
				ApplyWidgetSnapshot(*RootWidgetSnapshot, Widget);
			}
			else
			{
				UE_LOG(LogVCamCore, Warning, TEXT("Failed to apply snapshot to root widget"));
			}
		}
		
		return true;
	}

	void RetakeSnapshotForWidgetInHierarchy(FWidgetTreeSnapshot& WidgetTreeSnapshot, UWidget& Widget, const FWidgetSnapshotSettings& SnapshotSettings)
	{
		if (FWidgetSnapshot* Snapshot = WidgetTreeSnapshot.WidgetSnapshots.Find(Widget.GetFName()))
		{
			*Snapshot = TakeWidgetSnapshot(Widget, SnapshotSettings.AllowedProperties);
			return;
		}
		
		FWidgetSnapshot* RootSnapshot = WidgetTreeSnapshot.WidgetSnapshots.Find(WidgetTreeSnapshot.RootWidget);
		if (RootSnapshot && RootSnapshot->WidgetClass == Widget.GetClass())
		{
			*RootSnapshot = TakeWidgetSnapshot(Widget, SnapshotSettings.AllowedProperties);
			return;
		}
		
		UE_LOG(LogVCamCore, Warning, TEXT("Failed to retake snapshot of widget %s"), *Widget.GetPathName());
	}

	class FTakeWidgetSnapshotArchive : public FObjectWriter
	{
		const TSet<const FProperty*>& AllowedProperties;

		static bool CheckIsDangerousProperty(const FProperty* InProperty, bool bLogWarnings)
		{
			// No delegates
			if (InProperty->HasAnyPropertyFlags(CPF_BlueprintAssignable))
			{
				return true;
			}
			
			// These properties have the potential to blow up our archive... if this is really needed in the future, adjust this code.
			// By default, let's be careful because I have not particular case in mind where we'd want to save these properties.
			if (InProperty->HasAnyPropertyFlags(CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_PersistentInstance))
			{
				UE_CLOG(bLogWarnings, LogVCamCore, Warning, TEXT("Property %s was skipped because it could possibly bloat the snapshot!"), *InProperty->GetName());
				return true;
			}
			
			return false;
		}
	public:

		FTakeWidgetSnapshotArchive(const TSet<const FProperty*>& AllowedProperties, TArray<uint8>& InBytes)
			: FObjectWriter(InBytes)
			, AllowedProperties(AllowedProperties)
		{
			SetIsPersistent(true);
			SetIsSaving(true);
		}

		virtual bool ShouldSkipProperty(const FProperty* InProperty) const override
		{
			if (AllowedProperties.Contains(InProperty))
			{
				// Warn the caller that an explicitly specified property will be skipped...
				return CheckIsDangerousProperty(InProperty, true);
			}
			// ... but do not warn if the property was not explicitly specified
			if (CheckIsDangerousProperty(InProperty, false))
			{
				return true;
			}
			
			const FArchiveSerializedPropertyChain* PropertyChain = GetSerializedPropertyChain();
			if (!PropertyChain)
			{
				return true;
			}

			for (int32 i = 0; PropertyChain->GetNumProperties(); ++i)
			{
				// We expect allowed properties to be closer to the beginning than the end of the chain so start from beginning rather than from end
				if (AllowedProperties.Contains(PropertyChain->GetPropertyFromRoot(i)))
				{
					// Child properties of an allowed property must not be skipped or they won't be saved.
					return false;
				}
			}

			return true;
		}
	};
	
	FWidgetSnapshot TakeWidgetSnapshot(UWidget& Widget, const TSet<const FProperty*>& AllowedProperties)
	{
		FWidgetSnapshot Result;
		Result.WidgetClass = Widget.GetClass();
		FTakeWidgetSnapshotArchive Archive(AllowedProperties, Result.SavedBinaryData);
		FObjectAndNameAsStringProxyArchive Proxy(Archive, false);
		// Not calling SetCustomVersions() because I do not expect these widgets to require any migration
		Widget.Serialize(Proxy);
		return Result;
	}
	
	void ApplyWidgetSnapshot(const FWidgetSnapshot& Snapshot, UWidget& Widget)
	{
		FMemoryReader Reader(Snapshot.SavedBinaryData, true);
		FObjectAndNameAsStringProxyArchive Proxy(Reader, false);
		// Not calling SetCustomVersions() because I do not expect these widgets to require any migration
		Widget.Serialize(Proxy);
	}
}
