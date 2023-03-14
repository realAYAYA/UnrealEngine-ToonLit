// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/TVariant.h"
#include "Templates/Casts.h"
#include "UObject/Class.h"
#include "UObject/Object.h"
#include <type_traits>


namespace UE::MVVM
{

	/** */
	template<bool bConst>
	struct TObjectVariant
	{
	private:
		friend TObjectVariant<!bConst>;

		using ContainerType = std::conditional_t<bConst, const void*, void*>;

		struct FScriptStructStackedContainer
		{
			FScriptStructStackedContainer() = default;
			FScriptStructStackedContainer(const UScriptStruct* InScriptStruct, ContainerType InContainer)
				: ScriptStruct(InScriptStruct), Container(InContainer)
			{}
			const UScriptStruct* ScriptStruct = nullptr;
			ContainerType Container = nullptr;
		};

		struct FScriptStructAllocatedContainer
		{
			FScriptStructAllocatedContainer() = default;
			FScriptStructAllocatedContainer(const FScriptStructAllocatedContainer&) = delete;
			FScriptStructAllocatedContainer& operator=(const FScriptStructAllocatedContainer&) = delete;

			FScriptStructAllocatedContainer(const UScriptStruct* InScriptStruct)
				: ScriptStruct(InScriptStruct), Container(nullptr)
			{
				if (InScriptStruct)
				{
					Container = FMemory::Malloc(InScriptStruct->GetStructureSize() ? InScriptStruct->GetStructureSize() : 1);
					InScriptStruct->InitializeStruct(Container);
				}
			}
			~FScriptStructAllocatedContainer()
			{
				if (Container)
				{
					const UScriptStruct* ScriptStructPtr = ScriptStruct.Get();
					if (ensureAlwaysMsgf(ScriptStructPtr, TEXT("The script struct is null, that may cause resource leak.")))
					{
						ScriptStruct->DestroyStruct(Container);
					}
					FMemory::Free(Container);
				}
			}
			TWeakObjectPtr<const UScriptStruct> ScriptStruct = nullptr;
			ContainerType Container = nullptr;
		};

		using UScriptStructType = FScriptStructStackedContainer;
		using UObjectType = std::conditional_t<bConst, const UObject, UObject>;
		using VariantType = TVariant<FEmptyVariantState, UScriptStructType, UObjectType*>;

	public:
		TObjectVariant() = default;

		explicit TObjectVariant(UObjectType* InValue)
		{
			Binding = VariantType(TInPlaceType<UObjectType*>(), InValue);
		}

		explicit TObjectVariant(const UScriptStruct* InScriptStruct, ContainerType InValue)
		{
			Binding = VariantType(TInPlaceType<UScriptStructType>(), InScriptStruct, InValue);
		}

		UE_NODISCARD bool IsUObject() const
		{
			return Binding.template IsType<UObjectType*>();
		}

		UE_NODISCARD UObjectType* GetUObject() const
		{
			return Binding.template Get<UObjectType*>();
		}

		UE_NODISCARD const UClass* GetUClass() const
		{
			UObjectType* Instance = Binding.template Get<UObjectType*>();
			return Instance ? Instance->GetClass() : nullptr;
		}

		void SetUObject(UObjectType* InValue)
		{
			Binding.template Set<UObjectType*>(InValue);
		}

		UE_NODISCARD bool IsUScriptStruct() const
		{
			return Binding.template IsType<UScriptStructType>();
		}

		UE_NODISCARD ContainerType GetUScriptStructData() const
		{
			return Binding.template Get<UScriptStructType>().Container;
		}

		UE_NODISCARD const UScriptStruct* GetUScriptStruct() const
		{
			return Binding.template Get<UScriptStructType>().ScriptStruct;
		}

		void SetUScriptStruct(const UScriptStruct* InScriptStruct, ContainerType InValue)
		{
			Binding.template Set<UScriptStructType>(FScriptStructStackedContainer(InScriptStruct, InValue));
		}

		UE_NODISCARD ContainerType GetData() const
		{
			return IsUObject() ? reinterpret_cast<ContainerType>(GetUObject()) : IsUScriptStruct() ? GetUScriptStructData() : nullptr;
		}

		UE_NODISCARD const UStruct* GetOwner() const
		{
			if (IsUObject())
			{
				return GetUClass();
			}
			else if (IsUScriptStruct())
			{
				return Binding.template Get<UScriptStructType>().ScriptStruct;
			}
			return nullptr;
		}

		UE_NODISCARD bool IsEmpty() const
		{
			return Binding.template IsType<FEmptyVariantState>();
		}

		UE_NODISCARD bool IsNull() const
		{
			return IsUObject() ? GetUObject() == nullptr : IsUScriptStruct() ? GetUScriptStructData() == nullptr : true;
		}

		void Reset()
		{
			Binding = VariantType();
		}

		template<bool bOtherConst>
		bool operator==(const TObjectVariant<bOtherConst>& B) const
		{
			if (Binding.GetIndex() != B.Binding.GetIndex())
			{
				return false;
			}
			if (IsEmpty())
			{
				return true;
			}
			if (IsUObject())
			{
				return GetUObject() == B.GetObject();
			}
			return GetUScriptStructData() == B.GetScriptStructData();
		}

		template<bool bOtherConst>
		bool operator!=(const TObjectVariant<bOtherConst>& B) const
		{
			return !(*this == B);
		}

		friend int32 GetTypeHash(const TObjectVariant<bConst>& Variant)
		{
			if (Variant.IsObject())
			{
				return GetTypeHash(Variant.GetObject());
			}
			if (Variant.IsScriptStruct())
			{
				return GetTypeHash(Variant.GetScriptStructData());
			}
			return 0;
		}

	private:
		VariantType Binding;
	};

	/** */
	struct FObjectVariant : public TObjectVariant<false>
	{
	public:
		using TObjectVariant<false>::TObjectVariant;
		using TObjectVariant<false>::operator==;
	};

	/** */
	struct FConstObjectVariant : public TObjectVariant<true>
	{
	public:
		using TObjectVariant<true>::TObjectVariant;
		using TObjectVariant<true>::operator==;

		FConstObjectVariant(const FObjectVariant& OtherVariant)
		{
			if (OtherVariant.IsUObject())
			{
				SetUObject(OtherVariant.GetUObject());
			}
			else if (OtherVariant.IsUScriptStruct())
			{
				SetUScriptStruct(CastChecked<UScriptStruct>(OtherVariant.GetOwner()), OtherVariant.GetUScriptStructData());
			}
		}

		FConstObjectVariant& operator=(const FObjectVariant& OtherVariant)
		{
			*this = FConstObjectVariant(OtherVariant);
			return *this;
		}
	};

} //namespace
