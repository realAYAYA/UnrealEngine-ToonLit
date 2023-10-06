// Copyright Epic Games, Inc. All Rights Reserved.

#include "DetailsCustomizations/ChaosVDParticleDataWrapperCustomization.h"

#include "DataWrappers/ChaosVDParticleDataWrapper.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyHandle.h"

TSharedRef<IPropertyTypeCustomization> FChaosVDParticleDataWrapperCustomization::MakeInstance()
{
	return MakeShareable(new FChaosVDParticleDataWrapperCustomization());
}

void FChaosVDParticleDataWrapperCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	if (NumChildren == 0)
	{
		return;
	}

	TSet<FName> ParticleDataViewersNames;
	ParticleDataViewersNames.Reserve(5);

	ParticleDataViewersNames.Add(GET_MEMBER_NAME_CHECKED(FChaosVDParticleDataWrapper, ParticlePositionRotation));
	ParticleDataViewersNames.Add(GET_MEMBER_NAME_CHECKED(FChaosVDParticleDataWrapper, ParticleVelocities));
	ParticleDataViewersNames.Add(GET_MEMBER_NAME_CHECKED(FChaosVDParticleDataWrapper, ParticleDynamics));
	ParticleDataViewersNames.Add(GET_MEMBER_NAME_CHECKED(FChaosVDParticleDataWrapper, ParticleDynamicsMisc));
	ParticleDataViewersNames.Add(GET_MEMBER_NAME_CHECKED(FChaosVDParticleDataWrapper, ParticleMassProps));

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();

		const FName PropertyName = ChildHandle->GetProperty()->GetFName();
		if (ParticleDataViewersNames.Contains(PropertyName))
		{
			void* Data = nullptr;
			ChildHandle->GetValueData(Data);
			if (Data)
			{
				FChaosVDParticleDataBase* DataViewer = static_cast<FChaosVDParticleDataBase*>(Data);

				// The Particle Data viewer struct has several fields that will have default values if there was no recorded data for them in the trace file
				// As these do not represent anything real value, we only add to the details panel the ones with recorded data
				if (DataViewer->HasValidData())
				{
					StructBuilder.AddProperty(ChildHandle);
				}
			}
		}
		else
		{
			StructBuilder.AddProperty(ChildHandle);
		}
	}
}
