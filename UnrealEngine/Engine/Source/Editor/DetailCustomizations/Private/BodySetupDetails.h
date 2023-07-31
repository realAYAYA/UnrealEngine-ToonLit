// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "IDetailCustomization.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/WeakObjectPtrTemplates.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
class UObject;

class FBodySetupDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	void AddPhysicalAnimation(IDetailLayoutBuilder& DetailBuilder);

private:
	TSharedPtr<class FBodyInstanceCustomizationHelper> BodyInstanceCustomizationHelper;
	TArray< TWeakObjectPtr<UObject> > ObjectsCustomized;
	TSharedPtr<IPropertyHandle> CollisionReponseHandle;
};



class FSkeletalBodySetupDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

private:
	TArray< TWeakObjectPtr<UObject> > ObjectsCustomized;
};

