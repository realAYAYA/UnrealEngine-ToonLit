// Copyright Epic Games, Inc. All Rights Reserved.

#include "UObject/DynamicallyTypedValue.h"

UE::FDynamicallyTypedValueType& UE::FDynamicallyTypedValue::NullType()
{
	struct FNullType : FDynamicallyTypedValueType
	{
		FNullType(): FDynamicallyTypedValueType(0, 0, EContainsReferences::DoesNot) {}

		virtual void MarkReachable() {}
		virtual void MarkValueReachable(void* Data, FReferenceCollector& Collector) const {}

		virtual void InitializeValue(void* Data) const {}
		virtual void InitializeValueFromCopy(void* DestData, const void* SourceData) const {}
		virtual void DestroyValue(void* Data) const {}

		virtual void ClearValue(void* Data) const {}
		virtual void SerializeValue(FStructuredArchive::FSlot Slot, void* Data) const {}

		virtual uint32 GetValueHash(const void* Data) const override { return 0; }
		virtual bool AreIdentical(const void* DataA, const void* DataB) const override { return true; }
	};

	static FNullType Singleton;
	return Singleton;
}