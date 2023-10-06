// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "LearningArray.h"

#include "Containers/Map.h"
#include "Containers/StaticArray.h"
#include "Misc/GeneratedTypeName.h"
#include "UObject/NameTypes.h"

/**
* `FArrayMap` is a Map from `FArrayMapKey` to `TLearningArray` where each array 
* object stored as a value can be a different shape, dimensionality, and type.
* 
* To make this data structure easier to use, a type-safe `TArrayMapHandle` object 
* is provided which is basically an integer index that can be used to get a view 
* of an array safely, and without type-casting.
* 
* Arrays in this data structure can also be "linked", which makes the two 
* Keys/Handles which are linked together return a view of the same array data. This 
* is useful to avoid copying over array data between two entries.
* 
* In `Learning` this object is most commonly used to store "InstanceData" - essentially
* all the data required for multiple instances (such as multiple characters) is 
* stored together in a single `FArrayMap` as a set of large arrays. This makes this 
* data extremely efficient and cache-friendly to access when we are processing data for 
* multiple instances at once.
* 
* This structure also provides some basic optimizations for storing single-element arrays
* to also make it a viable option for when you have (for example) a single instance to store
* data for.
*/

class USplineComponent;

namespace UE::Learning
{
	enum class ECompletionMode : uint8;

	/**
	* Key type for FArrayMap. A pair of FNames - generally we can use one to identify the object itself, and one for the variable/member/field.
	*/
	struct LEARNING_API FArrayMapKey
	{
		FName Namespace;
		FName Variable;

		bool operator==(const FArrayMapKey& Other) const
		{
			return (Namespace == Other.Namespace) && (Variable == Other.Variable);
		}

		friend uint32 GetTypeHash(const FArrayMapKey& Other)
		{
			return GetTypeHash(Other.Namespace) ^ GetTypeHash(Other.Variable);
		}
	};


	/**
	* Compile-time function to generate a type id from a C++ type. This is used to check at
	* runtime that when we access an array from the FArrayMap we are casting it to the correct 
	* type.
	*/
	template<typename T> struct TTypeId
	{
		static_assert(sizeof(T) != sizeof(T), "Please specialize this class with a unique id to allow your type to be used in a FArrayMap");
	};
	template<> struct TTypeId<float> { enum { Value = 0 }; };
	template<> struct TTypeId<double> { enum { Value = 1 }; };
	template<> struct TTypeId<bool> { enum { Value = 2 }; };
	template<> struct TTypeId<int8> { enum { Value = 3 }; };
	template<> struct TTypeId<int16> { enum { Value = 4 }; };
	template<> struct TTypeId<int32> { enum { Value = 5 }; };
	template<> struct TTypeId<int64> { enum { Value = 6 }; };
	template<> struct TTypeId<uint8> { enum { Value = 7 }; };
	template<> struct TTypeId<uint16> { enum { Value = 8 }; };
	template<> struct TTypeId<uint32> { enum { Value = 9 }; };
	template<> struct TTypeId<uint64> { enum { Value = 10 }; };
	template<> struct TTypeId<FVector> { enum { Value = 11 }; };
	template<> struct TTypeId<FQuat> { enum { Value = 12 }; };
	template<> struct TTypeId<FTransform> { enum { Value = 13 }; };
	template<> struct TTypeId<ECompletionMode> { enum { Value = 14 }; };
	enum { CustomTypeIdStartIndex = 15 };

	/**
	* Compile-time function to get a debug name for a C++ type
	*/
	template<typename T>
	const TCHAR* CompileTimeTypeName() { return GetGeneratedTypeName<T>(); }

	/**
	* Untyped handle to an array stored in an FArrayMap
	*/
	struct FArrayMapHandle
	{
		int16 Index = INDEX_NONE;

		bool operator==(const FArrayMapHandle& Rhs) const { return Index == Rhs.Index; }
	};

	/**
	* Type-safe handle to an array stored in an FArrayMap
	*/
	template<uint8 DimNum, typename ElementType>
	struct TArrayMapHandle
	{
		int16 Index = INDEX_NONE;

		inline operator FArrayMapHandle() const { return { Index }; }
	};

	// Some helper functions for FArrayMap
	namespace ArrayMapPrivate
	{
		/**
		* Destructor for an array of elements of a given type
		*/
		template<typename ElementType>
		void DestructArrayItems(void* Elements, const int32 DimNum, const int32* Shape)
		{
			int32 Total = Shape[0];
			for (uint8 DimIdx = 1; DimIdx < DimNum; DimIdx++)
			{
				Total *= Shape[DimIdx];
			}

			DestructItems<ElementType, int32>((ElementType*)Elements, Total);
		}

		static inline int32 RoundUpToMultiple(const int32 Value, const int32 Multiple)
		{
			return ((Value + Multiple - 1) / Multiple) * Multiple;
		}
	}

	/**
	* A map of differently sized and typed multi-dimensional arrays
	*/
	struct LEARNING_API FArrayMap
	{
		FArrayMap();
		~FArrayMap();

		/**
		* Get a read-only view of an array.
		*/
		template<uint8 DimNum, typename ElementType>
		TLearningArrayView<DimNum, const ElementType> ConstView(const FArrayMapKey Key) const
		{
			const FArrayMapHandle Handle = Lookup(Key);
			CheckArrayWithKey<DimNum, ElementType>(Key, Handle.Index);
			FDynamicArrayView View = Arrays.GetData()[Handle.Index];
			return TLearningArrayView<DimNum, const ElementType>((ElementType*)View.Data, TLearningArrayShape<DimNum>(View.Shape));
		}

		/**
		* Get a read-only view of an array.
		*/
		template<uint8 DimNum, typename ElementType>
		TLearningArrayView<DimNum, const ElementType> ConstView(const FArrayMapHandle Handle) const
		{
			CheckArray<DimNum, ElementType>(Handle.Index);
			FDynamicArrayView View = Arrays.GetData()[Handle.Index];
			return TLearningArrayView<DimNum, const ElementType>((ElementType*)View.Data, TLearningArrayShape<DimNum>(View.Shape));
		}

		/**
		* Get a read-only view of an array.
		*/
		template<uint8 DimNum, typename ElementType>
		TLearningArrayView<DimNum, const ElementType> ConstView(const TArrayMapHandle<DimNum, ElementType> Handle) const
		{
			CheckArray<DimNum, ElementType>(Handle.Index);
			FDynamicArrayView View = Arrays.GetData()[Handle.Index];
			return TLearningArrayView<DimNum, const ElementType>((ElementType*)View.Data, TLearningArrayShape<DimNum>(View.Shape));
		}

		/**
		* Get a view of an array.
		*/
		template<uint8 DimNum, typename ElementType>
		TLearningArrayView<DimNum, ElementType> View(const FArrayMapKey Key) const
		{
			const FArrayMapHandle Handle = Lookup(Key);
			CheckArrayWithKey<DimNum, ElementType>(Key, Handle.Index);
			FDynamicArrayView View = Arrays.GetData()[Handle.Index];
			return TLearningArrayView<DimNum, ElementType>((ElementType*)View.Data, TLearningArrayShape<DimNum>(View.Shape));
		}

		/**
		* Get a view of an array.
		*/
		template<uint8 DimNum, typename ElementType>
		TLearningArrayView<DimNum, ElementType> View(const FArrayMapHandle Handle) const
		{
			CheckArray<DimNum, ElementType>(Handle.Index);
			FDynamicArrayView View = Arrays.GetData()[Handle.Index];
			return TLearningArrayView<DimNum, ElementType>((ElementType*)View.Data, TLearningArrayShape<DimNum>(View.Shape));
		}

		/**
		* Get a view of an array.
		*/
		template<uint8 DimNum, typename ElementType>
		TLearningArrayView<DimNum, ElementType> View(const TArrayMapHandle<DimNum, ElementType> Handle) const
		{
			CheckArray<DimNum, ElementType>(Handle.Index);
			FDynamicArrayView View = Arrays.GetData()[Handle.Index];
			return TLearningArrayView<DimNum, ElementType>((ElementType*)View.Data, TLearningArrayShape<DimNum>(View.Shape));
		}

		/**
		* Get the type id of an array in the map.
		*/
		int16 TypeId(const FArrayMapKey Key) const;

		/**
		* Get the type id of an array in the map.
		*/
		int16 TypeId(const FArrayMapHandle Handle) const;

		/**
		* Get the type name of an array in the map.
		*/
		const TCHAR* TypeName(const FArrayMapKey Key) const;

		/**
		* Get the type name of an array in the map.
		*/
		const TCHAR* TypeName(const FArrayMapHandle Handle) const;

		/**
		* Get the number of dimensions of an array in the map.
		*/
		uint8 DimNum(const FArrayMapKey Key) const;

		/**
		* Get the number of dimensions of an array in the map.
		*/
		uint8 DimNum(const FArrayMapHandle Handle) const;

		/**
		* Add a new array to the map
		*
		* @param Key		The array key
		* @param Shape		The shape of the array
		* @param Default	The default value of the array
		* @returns			A handle to later retrieve a view of the array for reading or writing
		*/
		template<uint8 DimNum, typename ElementType>
		TArrayMapHandle<DimNum, ElementType> Add(const FArrayMapKey Key, const TLearningArrayShape<DimNum> Shape, const ElementType Default = ElementType())
		{
			static_assert(DimNum <= MaxDimNum, "Cannot add an array of that many dimensions.");

			UE_LEARNING_CHECKF(Arrays.Num() < INT16_MAX,
				TEXT("Maximum number of arrays already registered"));

			UE_LEARNING_CHECKF(!Contains(Key),
				TEXT("There already exists an array with key { \"%s\", \"%s\" }"),
				*Key.Namespace.ToString(), *Key.Variable.ToString());

			TArrayMapHandle<DimNum, ElementType> Handle = { Arrays.Num() };
			Handles.Emplace(Key, Handle);

			/*
			* The FArrayMap object contains a big buffer of raw data for storing scalar data. This is 
			* so that when we are allocating arrays with a single element don't end up creating many small 
			* allocations and indirections. Here we check if the array is a single item and if so we place 
			* it in the big buffer of scalar data, otherwise we allocate some new memory for it as normal
			* using Malloc.
			*/

			const int32 ScalarInsertionIndex = ArrayMapPrivate::RoundUpToMultiple(ScalarOffset, alignof(ElementType));

			FDynamicArrayView ArrayView;
			if (Shape.Total() == 1 && ScalarInsertionIndex + sizeof(ElementType) <= ScalarData.Num() && alignof(ElementType) <= MaxScalarAlignment)
			{
				// Place in Scalar Data
				ArrayView.Data = &ScalarData[ScalarInsertionIndex];
				Flags.Emplace(FlagConstructed);
				ScalarOffset += sizeof(ElementType);
			}
			else
			{
				// Allocate new array data
				ArrayView.Data = FMemory::Malloc(Shape.Total() * sizeof(ElementType), alignof(ElementType));
				Flags.Emplace(FlagConstructed | FlagAllocated);
			}

			// Set Shape
			for (uint8 DimIdx = 0; DimIdx < DimNum; DimIdx++)
			{
				ArrayView.Shape[DimIdx] = Shape[DimIdx];
			}

			// Construct elements for this type
			DefaultConstructItems<ElementType, int32>((ElementType*)ArrayView.Data, Shape.Total());

			// Add array type data to Map
			Arrays.Emplace(ArrayView);
			Destructors.Emplace(ArrayMapPrivate::DestructArrayItems<ElementType>);
			DimNums.Emplace(DimNum);
			TypeIds.Emplace(TTypeId<ElementType>::Value);
			TypeNames.Emplace(CompileTimeTypeName<ElementType>());

			// Set to the default value
			Array::Set(View(Handle), Default);

			return Handle;
		}

		/**
		* Add a view to an existing array to the map. Does not allocate any new storage.
		*
		* @param Key		The array key
		* @param View		View to an existing array
		* @returns			A handle to later retrieve the value of the array for reading or writing
		*/
		template<uint8 DimNum, typename ElementType>
		TArrayMapHandle<DimNum, ElementType> AddView(const FArrayMapKey Key, const TLearningArrayView<DimNum, ElementType> View)
		{
			static_assert(DimNum <= MaxDimNum, "Cannot add an array of that many dimensions");

			UE_LEARNING_CHECKF(Arrays.Num() < INT16_MAX,
				TEXT("Maximum number of arrays already registered"));

			UE_LEARNING_CHECKF(!Contains(Key),
				TEXT("There already exists an array with key { \"%s\", \"%s\" }"),
				*Key.Namespace.ToString(), *Key.Variable.ToString());

			TArrayMapHandle<DimNum, ElementType> Handle = { Arrays.Num() };
			Handles.Emplace(Key, Handle);

			// Since this is an existing array we just create a basic view of it
			FDynamicArrayView ArrayView;
			ArrayView.Data = View.GetData();
			for (uint8 DimIdx = 0; DimIdx < DimNum; DimIdx++)
			{
				ArrayView.Shape[DimIdx] = View.Num(DimIdx);
			}

			// And push the array data as usual
			Flags.Emplace(FlagNone);
			Arrays.Emplace(ArrayView);
			Destructors.Emplace(ArrayMapPrivate::DestructArrayItems<ElementType>);
			DimNums.Emplace(DimNum);
			TypeIds.Emplace(TTypeId<ElementType>::Value);
			TypeNames.Emplace(CompileTimeTypeName<ElementType>());

			return Handle;
		}

		/**
		* Check if an array with the given name has already been added.
		*/
		bool Contains(const FArrayMapKey Key) const;

		/**
		* Lookup the array handle associated with a key.
		*/
		FArrayMapHandle Lookup(const FArrayMapKey Key) const;

		/**
		* Find the array handle associated with a key. Returns `nullptr` if Key is not in Map.
		*/
		const FArrayMapHandle* Find(const FArrayMapKey Key) const;

		/**
		* Clear all array data.
		*/
		void Empty();

		/**
		* Link one array to another such that they share the same data. 
		* Concretely, this function will make asking for a View of the 
		* Dst array return a view of the Src array instead.
		*/
		void LinkKeys(const FArrayMapKey Src, const FArrayMapKey Dst);

		/**
		* Link one array to another such that they share the same data.
		* Concretely, this function will make asking for a View of the
		* Dst array return a view of the Src array instead.
		*/
		void LinkHandles(const FArrayMapHandle Src, const FArrayMapHandle Dst);


		/**
		* Link one array to another as if they were flattened. This can be 
		* useful when the array dimensions don't exactly match such as with
		* one dimension of a single element.
		*/
		void LinkHandlesFlat(const FArrayMapHandle Src, const FArrayMapHandle Dst);

		/**
		* Link one array to another such that they share the same data.
		* Concretely, this function will make asking for a View of the
		* Dst array return a view of the Src array instead. This version
		* provides a type-safe interface for linking so should be used
		* by default.
		*/
		template<uint8 DimNum, typename ElementType>
		void Link(const TArrayMapHandle<DimNum, ElementType> Src, const TArrayMapHandle<DimNum, ElementType> Dst)
		{
			LinkHandles(Src, Dst);
		}

		/**
		* Link one array to another as if they were flattened. This can be
		* useful when the array dimensions don't exactly match such as with
		* one dimension of a single element.
		*/
		template<uint8 SrcDimNum, uint8 DstDimNum, typename ElementType>
		void LinkFlat(const TArrayMapHandle<SrcDimNum, ElementType> Src, const TArrayMapHandle<DstDimNum, ElementType> Dst)
		{
			LinkHandlesFlat(Src, Dst);
		}

		/**
		* Check if the given array has a link.
		*/
		bool HasLink(const FArrayMapKey Dst) const;

		/**
		* Check if the given array has a link.
		*/
		bool HasLink(const FArrayMapHandle Dst) const;

		/**
		* Check if the given array is linked to.
		*/
		bool IsLinkedTo(const FArrayMapKey Src) const;

		/**
		* Check if the given array is linked to.
		*/
		bool IsLinkedTo(const FArrayMapHandle Src) const;

		/**
		* Get all keys in the map.
		*/
		template<typename Allocator = FDefaultAllocator>
		void GetKeys(TArray<FArrayMapKey, Allocator>& OutKeys) const
		{
			Handles.GetKeys(OutKeys);
		}

		/**
		* Get the key associated with a particular handle
		*/
		const FArrayMapKey* FindKey(FArrayMapHandle Handle) const;

	private:

		static constexpr int32 MaxDimNum = 6;
		static constexpr int32 MaxScalarAlignment = 64;
		static constexpr int32 InlineArrayNum = 128;
		static constexpr int32 InlineScalarBytesNum = 2048;

		enum : uint8
		{
			FlagNone = 0,

			// Array is allocated by this map, so must be deallocated
			FlagAllocated = 1 << 0,

			// Array is constructed by this map, so must be destructed
			FlagConstructed = 1 << 1,

			// Array is a link to another array
			FlagLinked = 1 << 2,
		};

		struct FDynamicArrayView
		{
			void* Data = nullptr;
			int32 Shape[MaxDimNum] = { 0 };
		};

		template<uint8 DimNum, typename ElementType>
		void CheckArray(const int32 Index) const
		{
			UE_LEARNING_CHECKF(Index != INDEX_NONE, TEXT("Invalid Index!"));

			UE_LEARNING_CHECKF(DimNums[Index] == DimNum,
				TEXT("Asked for array of %i dimensions, but array has %i dimensions"),
				DimNum, DimNums[Index]);

			UE_LEARNING_CHECKF(TypeIds[Index] == TTypeId<ElementType>::Value,
				TEXT("Asked for array using type %s, but array data is of type %s"),
				CompileTimeTypeName<ElementType>(), TypeNames[Index]);
		}

		template<uint8 DimNum, typename ElementType>
		void CheckArrayWithKey(const FArrayMapKey Key, const int32 Index) const
		{
			UE_LEARNING_CHECKF(Index != INDEX_NONE, TEXT("Invalid Index!"));

			UE_LEARNING_CHECKF(DimNums[Index] == DimNum,
				TEXT("Asked for array { \"%s\", \"%s\" } with %i dimensions, but array has %i dimensions"),
				*Key.Namespace.ToString(), *Key.Variable.ToString(), DimNum, DimNums[Index]);

			UE_LEARNING_CHECKF(TypeIds[Index] == TTypeId<ElementType>::Value,
				TEXT("Asked for array { \"%s\", \"%s\" } using type %s, but array data is of type %s"),
				*Key.Namespace.ToString(), *Key.Variable.ToString(), CompileTimeTypeName<ElementType>(), TypeNames[Index]);
		}

		// Array Data

		TArray<FDynamicArrayView, TInlineAllocator<InlineArrayNum>> Arrays;

		// Handle Map

		TMap<FArrayMapKey, FArrayMapHandle, TInlineSetAllocator<InlineArrayNum>> Handles;

		// Array Metadata

		TArray<TFunctionRef<void(void* Elements, const int32 DimNum, const int32* Shape)>, TInlineAllocator<InlineArrayNum>> Destructors;
		TArray<uint8, TInlineAllocator<InlineArrayNum>> DimNums;
		TArray<uint8, TInlineAllocator<InlineArrayNum>> Flags;
		TArray<uint16, TInlineAllocator<InlineArrayNum>> TypeIds;
		TArray<const TCHAR*, TInlineAllocator<InlineArrayNum>> TypeNames;

		// Scalar Data

		int32 ScalarOffset = 0;
		TStaticArray<uint8, InlineScalarBytesNum, MaxScalarAlignment> ScalarData;
	};
}

