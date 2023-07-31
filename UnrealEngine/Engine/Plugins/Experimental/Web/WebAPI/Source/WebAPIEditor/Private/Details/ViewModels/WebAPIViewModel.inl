// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "WebAPIEnumViewModel.h"
#include "WebAPIModelViewModel.h"
#include "WebAPIOperationViewModel.h"
#include "WebAPIParameterViewModel.h"
#include "WebAPIServiceViewModel.h"
#include "WebAPIViewModel.h"
#include "Dom/WebAPIService.h"

template <typename ModelType, class ParentViewModelType, class ViewModelType>
TSharedPtr<ViewModelType> UE::WebAPI::Details::CreateViewModel(const TSharedRef<ParentViewModelType>& InParentViewModel, ModelType* InModel)
{
 	if(!InModel)
	{
		return nullptr;
	}
	
	const UClass* ModelClass = InModel->GetClass();
	if(ModelClass == UWebAPIEnum::StaticClass())
	{
		return FWebAPIEnumViewModel::Create(InParentViewModel, Cast<UWebAPIEnum>(InModel));
	}
	else if(ModelClass == UWebAPIEnumValue::StaticClass())
	{
		return FWebAPIEnumValueViewModel::Create(InParentViewModel, Cast<UWebAPIEnumValue>(InModel));
	}
	else if(ModelClass == UWebAPIModel::StaticClass())
	{
		return FWebAPIModelViewModel::Create(InParentViewModel, Cast<UWebAPIModel>(InModel));
	}
	else if(ModelClass == UWebAPIProperty::StaticClass())
	{
		return FWebAPIPropertyViewModel::Create(InParentViewModel, Cast<UWebAPIProperty>(InModel));
	}
	else if(ModelClass == UWebAPIService::StaticClass())
	{
		return FWebAPIServiceViewModel::Create(InParentViewModel, Cast<UWebAPIService>(InModel));
	}
	else if(ModelClass == UWebAPIOperation::StaticClass())
	{
		return FWebAPIOperationViewModel::Create(InParentViewModel, Cast<UWebAPIOperation>(InModel));
	}
	else if(ModelClass == UWebAPIParameter::StaticClass())
	{
		return FWebAPIParameterViewModel::Create(InParentViewModel, Cast<UWebAPIParameter>(InModel));
	}
	else
	{
		checkNoEntry();
		return nullptr;
	}
}
