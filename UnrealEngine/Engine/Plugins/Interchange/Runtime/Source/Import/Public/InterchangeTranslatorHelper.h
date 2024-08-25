// Copyright Epic Games, Inc. All Rights Reserved. 
#pragma once

#include "InterchangeSourceData.h"
#include "InterchangeTranslatorBase.h"
#include "Containers/UnrealString.h"

class UInterchangeResultsContainer;

namespace UE::Interchange::Private
{
	struct INTERCHANGEIMPORT_API FScopedTranslator
	{
	public:
		FScopedTranslator(const FString& PayLoadKey, UInterchangeResultsContainer* Results);
		~FScopedTranslator();

		template <class T>
		const T* GetPayLoadInterface() const
		{
			if(!IsValid())
			{ 
				return nullptr;
			}
			const T* PayloadInterface = Cast<T>(SourceTranslator);
			return PayloadInterface;
		}
	
	private:
		bool IsValid() const;

		TObjectPtr<UInterchangeTranslatorBase> SourceTranslator = nullptr;
		TObjectPtr<UInterchangeSourceData> PayloadSourceData = nullptr;
	};
} //namespace UE::Interchange::Private
