// Copyright Epic Games, Inc. All Rights Reserved.

#include "Replication/Editor/Model/PropertySource/SelectPropertyFromUClassModel.h"

#include "ConcertLogGlobal.h"
#include "Replication/Editor/Model/PropertySource/ConcertSyncCoreReplicatedPropertySource.h"

#define LOCTEXT_NAMESPACE "FSelectPropertyFromUClassModel"

namespace UE::ConcertClientSharedSlate
{
	FSelectPropertyFromUClassModel::FSelectPropertyFromUClassModel()
		: UClassIteratorSource(MakeShared<FConcertSyncCoreReplicatedPropertySource>())
	{}

	TSharedRef<ConcertSharedSlate::IPropertySourceModel> FSelectPropertyFromUClassModel::GetPropertySource(const FSoftClassPath& Class) const
	{
		UClass* LoadedClass = Class.TryLoadClass<UObject>();
		if (LoadedClass)
		{
			UClassIteratorSource->SetClass(LoadedClass);
		}
		else
		{
			UClassIteratorSource->SetClass(nullptr);
			UE_LOG(LogConcert, Warning, TEXT("Could not resolve class %s. Properties will not be available."), *Class.ToString());
		}
		return UClassIteratorSource;
	}
}

#undef LOCTEXT_NAMESPACE