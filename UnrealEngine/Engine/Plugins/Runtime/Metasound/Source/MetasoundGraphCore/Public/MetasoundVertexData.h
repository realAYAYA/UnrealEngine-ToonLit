// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/AllOf.h"
#include "Containers/Array.h"
#include "Containers/SortedMap.h"
#include "MetasoundDataFactory.h"
#include "MetasoundDataReference.h"
#include "MetasoundDataReferenceCollection.h"
#include "MetasoundLog.h"
#include "MetasoundOperatorSettings.h"
#include "MetasoundVertex.h"

#include <type_traits>

/** Enable runtime tests for compatible access types between vertex access types
 * and data references bound to the vertex. */
#define ENABLE_METASOUND_ACCESS_TYPE_COMPATIBILITY_TEST DO_CHECK

namespace Metasound
{
	namespace MetasoundVertexDataPrivate
	{
		// Tests to see that the access type of the data reference is compatible 
		// with the access type of the vertex.
#if ENABLE_METASOUND_ACCESS_TYPE_COMPATIBILITY_TEST
		void METASOUNDGRAPHCORE_API CheckAccessTypeCompatibility(const FDataVertex& InDataVertex, const FAnyDataReference& InDataReference);
#else
		FORCEINLINE void CheckAccessTypeCompatibility(const FDataVertex& InDataVertex, const FAnyDataReference& InDataReference) {}
#endif // #if DO_CHECK

		METASOUNDGRAPHCORE_API EVertexAccessType DataReferenceAccessTypeToVertexAccessType(EDataReferenceAccessType InReferenceAccessType);

		// Helper for getting a EVertexAccessType from a TData*Reference<DataType> object
		template<typename ...>
		struct TGetVertexAccess
		{
		};

		template<typename DataType>
		struct TGetVertexAccess<TDataReadReference<DataType>>
		{
			static constexpr EVertexAccessType VertexAccess = EVertexAccessType::Reference;
		};

		template<typename DataType>
		struct TGetVertexAccess<TDataWriteReference<DataType>>
		{
			static constexpr EVertexAccessType VertexAccess = EVertexAccessType::Reference;
		};

		template<typename DataType>
		struct TGetVertexAccess<TDataValueReference<DataType>>
		{
			static constexpr EVertexAccessType VertexAccess = EVertexAccessType::Value;
		};
		

		// An input binding which connects an FInputVertex to a IDataReference
		class METASOUNDGRAPHCORE_API FInputBinding
		{
		public:

			FInputBinding(FInputDataVertex&& InVertex);
			FInputBinding(const FInputDataVertex& InVertex);
			FInputBinding(const FVertexName& InVertexName, FAnyDataReference&& InReference);

			template<typename DataType>
			void BindRead(TDataReadReference<DataType>& InOutDataReference)
			{
				if (Data.IsSet())
				{
					InOutDataReference = Data->GetDataReadReference<DataType>();
				}
				else
				{
					Set(InOutDataReference);
				}
			}

			template<typename DataType>
			void BindRead(TDataWriteReference<DataType>& InOutDataReference)
			{
				if (Data.IsSet())
				{
					InOutDataReference = Data->GetDataWriteReference<DataType>();
				}
				else
				{
					Set(TDataReadReference<DataType>(InOutDataReference));
				}
			}

			template<typename DataType>
			void BindWrite(TDataWriteReference<DataType>& InOutDataReference)
			{
				if (Data.IsSet())
				{
					InOutDataReference = Data->GetDataWriteReference<DataType>();
				}
				else
				{
					Set(InOutDataReference);
				}
			}

			void Bind(FAnyDataReference& InOutDataReference);
			void Bind(FInputBinding& InBinding);

			// Set the data reference, overwriting any existing bound data references
			template<typename DataReferenceType>
			void Set(const DataReferenceType& InDataReference)
			{
				check(Vertex.DataTypeName == InDataReference.GetDataTypeName());
				Data.Emplace(InDataReference);
				CheckAccessTypeCompatibility(Vertex, *Data);
			}

			// Set the data reference, overwriting any existing bound data references
			void Set(FAnyDataReference&& InAnyDataReference);

			const FInputDataVertex& GetVertex() const;

			bool IsBound() const;

			EDataReferenceAccessType GetAccessType() const;

			const FAnyDataReference* GetDataReference() const;

			FDataReferenceID GetDataReferenceID() const;

			// Get data read reference assuming data is bound and read or write accessible.
			template<typename DataType>
			TDataReadReference<DataType> GetDataReadReference() const
			{
				check(Data.IsSet());
				return Data->GetDataReadReference<DataType>();
			}

			// Get the bound data read reference if it exists. Otherwise create and 
			// return a data read reference by constructing one using the Vertex's 
			// default literal.
			template< typename DataType > 
			TDataReadReference<DataType> GetOrCreateDefaultDataReadReference(const FOperatorSettings& InSettings) const
			{
				if (Data.IsSet())
				{
					return Data->GetDataReadReference<DataType>();
				}
				else
				{
					return TDataReadReferenceLiteralFactory<DataType>::CreateExplicitArgs(InSettings, Vertex.GetDefaultLiteral());
				}
			}

			// Get the bound data read reference if it exists. Otherwise create and 
			// return a data read reference by constructing one the supplied constructor 
			// arguments.
			template<typename DataType, typename ... ConstructorArgTypes>
			TDataReadReference<DataType> GetOrConstructDataReadReference(ConstructorArgTypes&&... ConstructorArgs) const
			{
				if (Data.IsSet())
				{
					return Data->GetDataReadReference<DataType>();
				}

				return TDataReadReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}

			// Get data write reference assuming data is bound and write accessible.
			template<typename DataType> 
			TDataWriteReference<DataType> GetDataWriteReference() const
			{
				check(Data.IsSet());
				return Data->GetDataWriteReference<DataType>();
			}

			// Get the bound data write reference if it exists. Otherwise create and 
			// return a data write reference by constructing one using the Vertex's 
			// default literal.
			template< typename DataType > 
			TDataWriteReference<DataType> GetOrCreateDefaultDataWriteReference(const FOperatorSettings& InSettings) const
			{
				if (Data.IsSet())
				{
					return Data->GetDataWriteReference<DataType>();
				}
				return TDataWriteReferenceLiteralFactory<DataType>::CreateExplicitArgs(InSettings, Vertex.GetDefaultLiteral());
			}

			// Get the bound data write reference if it exists. Otherwise create and 
			// return a data write reference by constructing one the supplied constructor 
			// arguments.
			template<typename DataType, typename ... ConstructorArgTypes>
			TDataWriteReference<DataType> GetOrConstructDataWriteReference(ConstructorArgTypes&&... ConstructorArgs) const
			{
				if (Data.IsSet())
				{
					return Data->GetDataWriteReference<DataType>();
				}
				return TDataWriteReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}

			// Gets the value of the bound data reference if it exists. Returns
			// nullptr if not bound.
			template<typename DataType>
			const DataType* GetValue() const
			{
				if (Data.IsSet())
				{
					return Data->GetValue<DataType>();
				}
				return nullptr;
			}


			// Gets the value of the bound data reference if it exists. Otherwise
			// create and return a value by constructing one using the Vertex's 
			// default literal.
			template<typename DataType>
			DataType GetOrCreateDefaultValue(const FOperatorSettings& InSettings) const
			{
				if (Data.IsSet())
				{
					if (const DataType* Value = Data->GetValue<DataType>())
					{
						return *Value;
					}
				}
				return TDataTypeLiteralFactory<DataType>::CreateExplicitArgs(InSettings, Vertex.GetDefaultLiteral());
			}

			// Set the value with a constant value reference.
			template<typename DataType>
			void SetValue(const DataType& InValue)
			{
				check(Vertex.DataTypeName == GetMetasoundDataTypeName<DataType>());
				Data.Emplace(TDataValueReference<DataType>::CreateNew(InValue));
				CheckAccessTypeCompatibility(Vertex, *Data);
			}

		private:

			FInputDataVertex Vertex;
			TOptional<FAnyDataReference> Data;
		};


		// Factory for creating a new FInputBinding from a IDataReference derived class and a vertex name.
		struct FInputBindingFactory
		{
			template<typename DataReferenceType>
			static FInputBinding CreateBinding(const FVertexName& InVertexName, const DataReferenceType& InRef)
			{
				FInputDataVertex Vertex(InVertexName, InRef.GetDataTypeName(), FDataVertexMetadata{}, TGetVertexAccess<DataReferenceType>::VertexAccess);
				return FInputBinding(Vertex);
			}
		};


		// Binds an IDataReference to a FOutputDataVertex
		class METASOUNDGRAPHCORE_API FOutputBinding
		{
		public:
			FOutputBinding(FOutputDataVertex&& InVertex);
			FOutputBinding(const FOutputDataVertex& InVertex);
			FOutputBinding(const FVertexName& InVertexName, FAnyDataReference&& InReference);

			template<typename DataType>
			void BindValue(const TDataValueReference<DataType>& InOutDataReference)
			{
				Set(InOutDataReference);
			}

			template<typename DataType>
			void BindRead(TDataReadReference<DataType>& InOutDataReference)
			{
				Set(InOutDataReference);
			}

			template<typename DataType>
			void BindRead(TDataValueReference<DataType>& InOutDataReference)
			{
				Set(TDataReadReference<DataType>(InOutDataReference));
			}

			template<typename DataType>
			void BindRead(TDataWriteReference<DataType>& InOutDataReference)
			{
				Set(TDataReadReference<DataType>(InOutDataReference));
			}

			template<typename DataType>
			void BindWrite(TDataWriteReference<DataType>& InOutDataReference)
			{
				Set(InOutDataReference);
			}

			void Bind(FAnyDataReference& InOutDataReference);
			void Bind(FOutputBinding& InBinding);

			// Set the data reference, overwriting any existing bound data references
			template<typename DataReferenceType>
			void Set(const DataReferenceType& InDataReference)
			{
				check(Vertex.DataTypeName == InDataReference.GetDataTypeName());
				Data.Emplace(InDataReference);
				CheckAccessTypeCompatibility(Vertex, *Data);
			}

			// Set the data reference, overwriting any existing bound data references
			void Set(FAnyDataReference&& InAnyDataReference);
			
			const FOutputDataVertex& GetVertex() const;

			bool IsBound() const;

			EDataReferenceAccessType GetAccessType() const;

			// Get data reference 
			const FAnyDataReference* GetDataReference() const;

			FDataReferenceID GetDataReferenceID() const;

			// Get data read reference assuming data is bound and read or write accessible.
			template<typename DataType>
			TDataReadReference<DataType> GetDataReadReference() const
			{
				check(Data.IsSet());
				return Data->GetDataReadReference<DataType>();
			}

			// Get the bound data read reference if it exists. Otherwise create and 
			// return a data read reference by constructing one the supplied constructor 
			// arguments.
			template<typename DataType, typename ... ConstructorArgTypes>
			TDataReadReference<DataType> GetOrConstructDataReadReference(ConstructorArgTypes&&... ConstructorArgs) const
			{
				if (Data.IsSet())
				{
					return Data->GetDataReadReference<DataType>();
				}

				return TDataReadReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}

			// Get data write reference assuming data is bound and write accessible.
			template<typename DataType> 
			TDataWriteReference<DataType> GetDataWriteReference() const
			{
				check(Data.IsSet());
				return Data->GetDataWriteReference<DataType>();
			}

			// Get the bound data write reference if it exists. Otherwise create and 
			// return a data write reference by constructing one the supplied constructor 
			// arguments.
			template<typename DataType, typename ... ConstructorArgTypes>
			TDataWriteReference<DataType> GetOrConstructDataWriteReference(ConstructorArgTypes&&... ConstructorArgs) const
			{
				if (Data.IsSet())
				{
					return Data->GetDataWriteReference<DataType>();
				}
				return TDataWriteReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}

			// Gets the value of the bound data reference if it exists. Returns
			// nullptr if not bound.
			template<typename DataType>
			const DataType* GetValue() const
			{
				if (Data.IsSet())
				{
					return Data->GetValue<DataType>();
				}
				return nullptr;
			}

			// Set the value with a constant value reference.
			template<typename DataType>
			void SetValue(const DataType& InValue)
			{
				Data.Emplace(TDataValueReference<DataType>::CreateNew(InValue));
				CheckAccessTypeCompatibility(Vertex, *Data);
			}

		private:

			FOutputDataVertex Vertex;
			TOptional<FAnyDataReference> Data;
		};

		// Factory for creating FOutputBinding from an IDataReference derived object.
		struct FOutputBindingFactory
		{
			template<typename DataReferenceType>
			static FOutputBinding CreateBinding(const FVertexName& InVertexName, const DataReferenceType& InRef)
			{
				FOutputDataVertex Vertex(InVertexName, InRef.GetDataTypeName(), FDataVertexMetadata{}, TGetVertexAccess<DataReferenceType>::VertexAccess);
				return FOutputBinding(Vertex);
			}
		};
	}

	/** Convenience for using a TSortedMap with FVertexName Key type.
	 *
	 * This template makes it convenient to create a TSortedMap with an FVertexName 
	 * while also avoiding compilation errors incurred from using the FName default
	 * "less than" operator in the TSortedMap implementation. 
	 *
	 * - FVertexName is an alias to FName. 
	 * - TSortedMap<FName, ValueType> fails to compile since the "less than" operator
	 *   specific implementation needs to be chosen (FastLess vs LexicalLess)
	 * - Due to the template argument order of TSortedMap this also forces you to
	 *   choose the allocator. 
	 * - This is all a bit of an annoyance to do every time we use a TSortedMap 
	 *   with an FVertexName as the key.
	 */
	template<typename ValueType>
	using TSortedVertexNameMap = TSortedMap<FVertexName, ValueType, FDefaultAllocator, FNameFastLess>;

	/** MetaSound Binding 
	 *
	 * MetaSound IOperators read and write to shared data. For example: a TDataWriteReference<float> 
	 * written to by one operator may be read by several other operators holding
	 * onto TDataReadReference<float>. Binding empowers sharing of the underlying 
	 * objects (a `float` in the example) between IOperators and minimizing copying
	 * of data. In general, it is assumed that any individual node does not need 
	 * to worry about the specific implications of a binding, but can rather assume 
	 * that data references are managed by external systems. 
	 *
	 * There are 2 known scenarios where an IOperator must be aware of what binding
	 * can do.
	 *
	 * 1. If the IOperator internally caches raw pointers to the underlying data 
	 * of a data reference.
	 *
	 * 2. If the IOperator manages multiple internal IOperators with their own 
	 * shared connections (e.g. FGraphOperator)
	 *
	 * In the case that a IOperator does need to know manage bound connections, it's
	 * important that the IOperator follows the binding rules.
	 *
	 * General Binding Rules:
	 * - Binding cannot change the access type of an existing data reference.
	 * - IOperators will ignore new TDataValueReferences because Value references
	 *   cannot be updated after the operator has been constructed. 
	 *
	 * Input Binding Rules:
	 * - Binding an input may update the underlying data object to point to
	 * a new object.
	 * - When updating input pointers of underlying data, a data reference from
	 * and outer scope should replace that of an inner scope. For example, a graph 
	 * representing multiple IOperators can override the input pointers of individual
	 * IOperators, but an individual IOperator cannot override the input pointers of 
	 * the containing graph.
	 *
	 *
	 * Output Binding Rules:
	 * - Binding an output may NOT update the location of the underlying data object.
	 * - When determining output pointers of underlying data, a data reference from
	 *  the inner scope can override the data reference of an outerscope. For example, 
	 *  a graph representing multiple IOperators cannot override the output pointers 
	 *  of individual IOperators, but an individual IOperator can override the output 
	 *  pointers of the containing graph.
	 *
	 *
	 *  These rules apply to any method on the FInputVertexInterfaData or 
	 *  FOutptuVertexInterfaceData which begins with `Bind`
	 */

	// Forward declare
	struct FVertexDataState;

	/** An input vertex interface with optionally bound data references. */
	class METASOUNDGRAPHCORE_API FInputVertexInterfaceData
	{
		using FInputBinding = MetasoundVertexDataPrivate::FInputBinding;

	public:

		using FRangedForIteratorType = typename TArray<FInputBinding>::RangedForIteratorType;
		using FRangedForConstIteratorType = typename TArray<FInputBinding>::RangedForConstIteratorType;

		/** Construct with an FInputVertexInterface. This will default to a unfrozen vertex interface. */
		FInputVertexInterfaceData();

		/** Construct with an FInputVertexInterface. This will default to a frozen vertex interface. */
		FInputVertexInterfaceData(const FInputVertexInterface& InVertexInterface);

		/** Returns true if the vertex interface is frozen. */
		bool IsVertexInterfaceFrozen() const;

		/** Set whether the vertex interface is frozen or not. 
		 *
		 * If frozen, attempts to access vertices which do not already exist will result in an error. 
		 *
		 * If not frozen, attempts to bind to a missing vertex will automatically
		 * add the missing vertex.
		 */
		void SetIsVertexInterfaceFrozen(bool bInIsVertexInterfaceFrozen);

		/** Set the value of a vertex. */
		template<typename DataType>
		void SetValue(const FVertexName& InVertexName, const DataType& InValue)
		{
			auto CreateBinding = [&]()
			{
				FInputDataVertex Vertex(InVertexName, GetMetasoundDataTypeName<DataType>(), FDataVertexMetadata{}, EVertexAccessType::Value);
				return FInputBinding(Vertex);
			};

			auto BindData = [&](FInputBinding& Binding) { Binding.SetValue<DataType>(InValue); };
			
			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a read vertex from a read reference. Slated for deprecation. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataReadReference<DataType>& InOutDataReference)
		{
			BindReadVertex(InVertexName, const_cast<TDataReadReference<DataType>&>(InOutDataReference));
		}

		/** Bind a read vertex from a read reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, TDataReadReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FInputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FInputBinding& Binding) { Binding.BindRead(InOutDataReference); };
			
			Apply(InVertexName, CreateBinding, BindData);
		}
		
		/** Bind a read vertex from a write reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, TDataWriteReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FInputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FInputBinding& Binding) { Binding.BindRead(InOutDataReference); };
			
			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a write vertex from a write reference. Slated for deprecation. */
		template<typename DataType>
		void BindWriteVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InOutDataReference)
		{
			BindWriteVertex(InVertexName, const_cast<TDataWriteReference<DataType>&>(InOutDataReference));
		}

		/** Bind a write vertex from a write reference. */
		template<typename DataType>
		void BindWriteVertex(const FVertexName& InVertexName, TDataWriteReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FInputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FInputBinding& Binding) { Binding.BindWrite(InOutDataReference); };
			
			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a vertex with a any data reference. Slated for deprecation. */
		void BindVertex(const FVertexName& InVertexName, const FAnyDataReference& InOutDataReference);
		/** Bind a vertex with a any data reference. */
		void BindVertex(const FVertexName& InVertexName, FAnyDataReference& InOutDataReference);

		/** Bind vertex data using other vertex data. Slated for deprecation. */
		void Bind(const FInputVertexInterfaceData& InOutInputVertexData);
		/** Bind vertex data using other vertex data. */
		void Bind(FInputVertexInterfaceData& InOutInputVertexData);

		/** Sets a vertex to use a data reference, ignoring any existing data bound to the vertex. */
		void SetVertex(const FVertexName& InVertexName, const FAnyDataReference& InDataReference);
		/** Sets a vertex to use a data reference, ignoring any existing data bound to the vertex. */
		void SetVertex(const FVertexName& InVertexName, FAnyDataReference&& InDataReference);

		/** Set a data references to vertices with matching vertex names. Ignores any existing data bound to the vertex. */
		void Set(const FDataReferenceCollection& InCollection);

		/** Convert vertex data to a data reference collection. */
		FDataReferenceCollection ToDataReferenceCollection() const;

		/** Return the vertex associated with the vertex name. */
		const FInputDataVertex& GetVertex(const FVertexName& InVertexName) const;

		/** Add a vertex. VertexInterfaceData must be unfrozen. */
		void AddVertex(const FInputDataVertex& InVertex);

		/** Remove a vertex. VertexInterfaceData must be unfrozen. */
		void RemoveVertex(const FVertexName& InVertexName);

		/** Returns true if a vertex with the given vertex name exists and is bound
		 * to a data reference. */
		bool IsVertexBound(const FVertexName& InVertexName) const;

		/** Returns the access type of a bound vertex. If the vertex does not exist
		 * or if it is unbound, this will return EDataReferenceAccessType::None
		 */
		EDataReferenceAccessType GetVertexDataAccessType(const FVertexName& InVertexName) const;

		/** Returns true if all vertices in the FInputVertexInterface are bound to 
		 * data references. */
		bool AreAllVerticesBound() const;

		FRangedForIteratorType begin()
		{
			return Bindings.begin();
		}

		FRangedForIteratorType end()
		{
			return Bindings.end();
		}

		FRangedForConstIteratorType begin() const
		{
			return Bindings.begin();
		}

		FRangedForConstIteratorType end() const
		{
			return Bindings.end();
		}

		/** Find data reference bound to vertex. Returns a nullptr if no data reference is bound. */
		const FAnyDataReference* FindDataReference(const FVertexName& InVertexName) const;

		/** Returns the current value of a vertex. */
		template<typename DataType>
		const DataType* GetValue(const FVertexName& InVertexName) const
		{
			const FInputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetValue<DataType>();
		}

		/**  Gets the value of the bound data reference if it exists. Otherwise
		 * create and return a value by constructing one using the Vertex's 
		 * default literal. */
		template<typename DataType>
		DataType GetOrCreateDefaultValue(const FVertexName& InVertexName, const FOperatorSettings& InSettings) const
		{
			const FInputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetOrCreateDefaultValue<DataType>(InSettings);
		}

		/** Get data read reference assuming data is bound and read or write accessible. */
		template<typename DataType> 
		TDataReadReference<DataType> GetDataReadReference(const FVertexName& InVertexName) const
		{
			const FInputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataReadReference<DataType>();
		}

		/** Get the bound data read reference if it exists. Otherwise create and 
		 * return a data read reference by constructing one using the Vertex's 
		 * default literal.
		 */
		template<typename DataType> 
		TDataReadReference<DataType> GetOrCreateDefaultDataReadReference(const FVertexName& InVertexName, const FOperatorSettings& InSettings) const
		{
			if (const FInputBinding* Binding = Find(InVertexName))
			{
				return Binding->GetOrCreateDefaultDataReadReference<DataType>(InSettings);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Cannot check for existing bound data."), *InVertexName.ToString());
			}

			return TDataReadReferenceLiteralFactory<DataType>::CreateExplicitArgs(InSettings, FLiteral::CreateInvalid());
		}

		/** Get the bound data read reference if it exists. Otherwise create and 
		 * return a data read reference by constructing one the supplied constructor 
		 * arguments.
		 */
		template<typename DataType, typename ... ConstructorArgTypes>
		TDataReadReference<DataType> GetOrConstructDataReadReference(const FVertexName& InVertexName, ConstructorArgTypes&&... ConstructorArgs) const
		{
			if (const FInputBinding* Binding = Find(InVertexName))
			{
				return Binding->GetOrConstructDataReadReference<DataType>(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Cannot check for existing bound data."), *InVertexName.ToString());
			}

			return TDataReadReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
		}


		/** Get data write reference assuming data is bound and write accessible. */
		template<typename DataType> 
		TDataWriteReference<DataType> GetDataWriteReference(const FVertexName& InVertexName) const
		{
			const FInputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataWriteReference<DataType>();
		}

		/** Get the bound data write reference if it exists. Otherwise create and 
		 * return a data write reference by constructing one using the Vertex's 
		 * default literal.
		 */
		template<typename DataType> 
		TDataWriteReference<DataType> GetOrCreateDefaultDataWriteReference(const FVertexName& InVertexName, const FOperatorSettings& InSettings) const
		{
			if (const FInputBinding* Binding = Find(InVertexName))
			{
				return Binding->GetOrCreateDefaultDataWriteReference<DataType>(InSettings);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Cannot check for existing bound data."), *InVertexName.ToString());
			}

			return TDataWriteReferenceLiteralFactory<DataType>::CreateAny(InSettings, FLiteral::CreateInvalid());
		}

		/** Get the bound data write reference if it exists. Otherwise create and 
		 * return a data write reference by constructing one the supplied constructor 
		 * arguments.
		 */
		template<typename DataType, typename ... ConstructorArgTypes>
		TDataWriteReference<DataType> GetOrConstructDataWriteReference(const FVertexName& InVertexName, ConstructorArgTypes&&... ConstructorArgs) const
		{
			if (const FInputBinding* Binding = Find(InVertexName))
			{
				return Binding->GetOrConstructDataWriteReference<DataType>(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Cannot check for existing bound data."), *InVertexName.ToString());
			}

			return TDataWriteReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
		}
	
	private:
		friend METASOUNDGRAPHCORE_API void GetVertexInterfaceDataState(const FInputVertexInterfaceData& InVertexInterface, TArray<FVertexDataState>& OutState);
		friend METASOUNDGRAPHCORE_API void CompareVertexInterfaceDataToPriorState(const FInputVertexInterfaceData& InCurrentInterface, const TArray<FVertexDataState>& InPriorState, TSortedVertexNameMap<FAnyDataReference>& OutUpdates);

		void Apply(const FVertexName& InVertexName, TFunctionRef<FInputBinding ()> InCreateFunc, TFunctionRef<void (FInputBinding&)> InApplyFunc);

		FInputBinding* Find(const FVertexName& InVertexName);
		const FInputBinding* Find(const FVertexName& InVertexName) const;

		FInputBinding* FindChecked(const FVertexName& InVertexName);
		const FInputBinding* FindChecked(const FVertexName& InVertexName) const;

		bool bIsVertexInterfaceFrozen = false;
		TArray<FInputBinding> Bindings;
	};


	/** An output vertex interface with optionally bound data references. */
	class METASOUNDGRAPHCORE_API FOutputVertexInterfaceData
	{
		using FOutputBinding = MetasoundVertexDataPrivate::FOutputBinding;

	public:

		using FRangedForIteratorType = typename TArray<FOutputBinding>::RangedForIteratorType;
		using FRangedForConstIteratorType = typename TArray<FOutputBinding>::RangedForConstIteratorType;

		/** Construct with an FOutputVertexInterface. This will default to a unfrozen vertex interface. */
		FOutputVertexInterfaceData();

		/** Construct with an FOutputVertexInterface. This will default to a frozen vertex interface. */
		FOutputVertexInterfaceData(const FOutputVertexInterface& InVertexInterface);

		/** Returns true if the vertex interface is frozen. */
		bool IsVertexInterfaceFrozen() const;

		/** Set whether the vertex interface is frozen or not. 
		 *
		 * If frozen, attempts to access vertices which do not already exist will result in an error. 
		 *
		 * If not frozen, attempts to bind to a missing vertex will automatically
		 * add the missing vertex.
		 */
		void SetIsVertexInterfaceFrozen(bool bInIsVertexInterfaceFrozen);

		/** Set the value of a vertex. */
		template<typename DataType>
		void SetValue(const FVertexName& InVertexName, const DataType& InValue)
		{
			BindValueVertex<DataType>(InVertexName, TDataValueReference<DataType>::CreateNew(InValue));
		}

		/** Bind a value vertex from a value reference. */
		template<typename DataType>
		void BindValueVertex(const FVertexName& InVertexName, const TDataValueReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FOutputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FOutputBinding& Binding) { Binding.BindValue(InOutDataReference); };

			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a read vertex from a value reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataValueReference<DataType>& InOutDataReference)
		{
			BindReadVertex(InVertexName, static_cast<TDataReadReference<DataType>>(InOutDataReference));
		}

		/** Bind a read vertex from a read reference. Slated for deprecation. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataReadReference<DataType>& InOutDataReference)
		{
			BindReadVertex(InVertexName, const_cast<TDataReadReference<DataType>&>(InOutDataReference));
		}

		/** Bind a read vertex from a read reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, TDataReadReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FOutputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FOutputBinding& Binding) { Binding.BindRead(InOutDataReference); };

			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a read vertex from a write reference. Slated for deprecation. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InOutDataReference)
		{
			BindReadVertex(InVertexName, const_cast<TDataWriteReference<DataType>&>(InOutDataReference));
		}

		/** Bind a read vertex from a write reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, TDataWriteReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FOutputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FOutputBinding& Binding) { Binding.BindRead(InOutDataReference); };

			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a write vertex from a write reference. Slated for deprecation. */
		template<typename DataType>
		void BindWriteVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InOutDataReference)
		{
			BindWriteVertex(InVertexName, const_cast<TDataWriteReference<DataType>&>(InOutDataReference));
		}

		/** Bind a write vertex from a write reference. */
		template<typename DataType>
		void BindWriteVertex(const FVertexName& InVertexName, TDataWriteReference<DataType>& InOutDataReference)
		{
			auto CreateBinding = [&]()
			{
				return MetasoundVertexDataPrivate::FOutputBindingFactory::CreateBinding(InVertexName, InOutDataReference);
			};

			auto BindData = [&](FOutputBinding& Binding) { Binding.BindWrite(InOutDataReference); };

			Apply(InVertexName, CreateBinding, BindData);
		}

		/** Bind a vertex with a any data reference. Slated for deprecation. */
		void BindVertex(const FVertexName& InVertexName, const FAnyDataReference& InOutDataReference);
		/** Bind a vertex with a any data reference. */
		void BindVertex(const FVertexName& InVertexName, FAnyDataReference& InOutDataReference);

		/** Bind vertex data using other vertex data. Slated for deprecation. */
		void Bind(const FOutputVertexInterfaceData& InOutOutputVertexData);
		/** Bind vertex data using other vertex data. */
		void Bind(FOutputVertexInterfaceData& InOutOutputVertexData);

		/** Set a data reference to a vertex, ignoring any existing data bound to the vertex*/
		void SetVertex(const FVertexName& InVertexName, const FAnyDataReference& InDataReference);
		/** Set a data reference to a vertex, ignoring any existing data bound to the vertex*/
		void SetVertex(const FVertexName& InVertexName, FAnyDataReference&& InDataReference);

		/** Bind a data references to vertices with matching vertex names. */
		void Set(const FDataReferenceCollection& InCollection);

		/** Converts the vertex data to a data reference collection. */
		FDataReferenceCollection ToDataReferenceCollection() const;

		/** Returns true if a vertex with the given vertex name exists and is bound
		 * to a data reference. */
		bool IsVertexBound(const FVertexName& InVertexName) const;

		/** Return the vertex associated with the vertex name. */
		const FOutputDataVertex& GetVertex(const FVertexName& InVertexName) const;

		/** Add a vertex. VertexInterfaceData must be unfrozen. */
		void AddVertex(const FOutputDataVertex& InVertex);

		/** Remove a vertex. VertexInterfaceData must be unfrozen. */
		void RemoveVertex(const FVertexName& InVertexName);

		/** Returns the access type of a bound vertex. If the vertex does not exist
		 * or if it is unbound, this will return EDataReferenceAccessType::None
		 */
		EDataReferenceAccessType GetVertexDataAccessType(const FVertexName& InVertexName) const;

		/** Returns true if all vertices in the FInputVertexInterface are bound to 
		 * data references. */
		bool AreAllVerticesBound() const;

		FRangedForIteratorType begin()
		{
			return Bindings.begin();
		}

		FRangedForIteratorType end()
		{
			return Bindings.end();
		}

		FRangedForConstIteratorType begin() const
		{
			return Bindings.begin();
		}

		FRangedForConstIteratorType end() const
		{
			return Bindings.end();
		}

		/** Find data reference bound to vertex. Returns a nullptr if no data reference is bound. */
		const FAnyDataReference* FindDataReference(const FVertexName& InVertexName) const;

		/** Returns the current value of a vertex. */
		template<typename DataType>
		const DataType* GetValue(const FVertexName& InVertexName) const
		{
			const FOutputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetValue<DataType>();
		}


		/** Get data read reference assuming data is bound and read or write accessible. */
		template<typename DataType> 
		TDataReadReference<DataType> GetDataReadReference(const FVertexName& InVertexName) const
		{
			const FOutputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataReadReference<DataType>();
		}

		/** Get the bound data read reference if it exists. Otherwise create and 
		 * return a data read reference by constructing one the supplied constructor 
		 * arguments.
		 */
		template<typename DataType, typename ... ConstructorArgTypes>
		TDataReadReference<DataType> GetOrConstructDataReadReference(const FVertexName& InVertexName, ConstructorArgTypes&&... ConstructorArgs) const
		{
			if (const FOutputBinding* Binding = Find(InVertexName))
			{
				return Binding->GetOrConstructDataReadReference<DataType>(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Cannot check for existing bound data."), *InVertexName.ToString());
			}

			return TDataReadReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
		}


		/** Get data write reference assuming data is bound and write accessible. */
		template<typename DataType> 
		TDataWriteReference<DataType> GetDataWriteReference(const FVertexName& InVertexName) const
		{
			const FOutputBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataWriteReference<DataType>();
		}

		/** Get the bound data write reference if it exists. Otherwise create and 
		 * return a data write reference by constructing one the supplied constructor 
		 * arguments.
		 */
		template<typename DataType, typename ... ConstructorArgTypes>
		TDataWriteReference<DataType> GetOrConstructDataWriteReference(const FVertexName& InVertexName, ConstructorArgTypes&&... ConstructorArgs) const
		{
			if (const FOutputBinding* Binding = Find(InVertexName))
			{
				return Binding->GetOrConstructDataWriteReference<DataType>(Forward<ConstructorArgTypes>(ConstructorArgs)...);
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Cannot check for existing bound data."), *InVertexName.ToString());
			}

			return TDataWriteReference<DataType>::CreateNew(Forward<ConstructorArgTypes>(ConstructorArgs)...);
		}
	
	private:
		friend METASOUNDGRAPHCORE_API void GetVertexInterfaceDataState(const FOutputVertexInterfaceData& InVertexInterface, TArray<FVertexDataState>& OutState);
		friend METASOUNDGRAPHCORE_API void CompareVertexInterfaceDataToPriorState(const FOutputVertexInterfaceData& InCurrentInterface, const TArray<FVertexDataState>& InPriorState, TSortedVertexNameMap<FAnyDataReference>& OutUpdates);

		void Apply(const FVertexName& InVertexName, TFunctionRef<FOutputBinding ()> InCreateFunc, TFunctionRef<void (FOutputBinding&)> InBindFunc);

		FOutputBinding* Find(const FVertexName& InVertexName);
		const FOutputBinding* Find(const FVertexName& InVertexName) const;

		FOutputBinding* FindChecked(const FVertexName& InVertexName);
		const FOutputBinding* FindChecked(const FVertexName& InVertexName) const;

		bool bIsVertexInterfaceFrozen = false;
		TArray<FOutputBinding> Bindings;
	};

	/** A vertex interface with optionally bound data. */
	class METASOUNDGRAPHCORE_API FVertexInterfaceData
	{
	public:

		FVertexInterfaceData() = default;

		/** Construct using an FVertexInterface. */
		FVertexInterfaceData(const FVertexInterface& InVertexInterface);

		/** Set vertex data using other vertex data. */
		void Bind(const FVertexInterfaceData& InVertexData);
		void Bind(FVertexInterfaceData& InVertexData);

		/** Get input vertex interface data. */
		FInputVertexInterfaceData& GetInputs()
		{
			return InputVertexInterfaceData;
		}

		/** Get input vertex interface data. */
		const FInputVertexInterfaceData& GetInputs() const
		{
			return InputVertexInterfaceData;
		}

		/** Get output vertex interface data. */
		FOutputVertexInterfaceData& GetOutputs()
		{
			return OutputVertexInterfaceData;
		}

		/** Get output vertex interface data. */
		const FOutputVertexInterfaceData& GetOutputs() const
		{
			return OutputVertexInterfaceData;
		}

	private:

		FInputVertexInterfaceData InputVertexInterfaceData;
		FOutputVertexInterfaceData OutputVertexInterfaceData;
	};

	/** FVertexDataState encapsulates which data reference a vertex is associated
	 * with. The ID refers to the underlying object associated with the IDataReference.
	 */
	struct FVertexDataState
	{
		FVertexName VertexName;
		FDataReferenceID ID;

		METASOUNDGRAPHCORE_API friend bool operator<(const FVertexDataState& InLHS, const FVertexDataState& InRHS);
		METASOUNDGRAPHCORE_API friend bool operator==(const FVertexDataState& InLHS, const FVertexDataState& InRHS);
	};

	/** Caches a representation of the current data references bound to the vertex interface */
	METASOUNDGRAPHCORE_API void GetVertexInterfaceDataState(const FInputVertexInterfaceData& InVertexInterface, TArray<FVertexDataState>& OutState);
	/** Caches a representation of the current data references bound to the vertex interface */
	METASOUNDGRAPHCORE_API void GetVertexInterfaceDataState(const FOutputVertexInterfaceData& InVertexInterface, TArray<FVertexDataState>& OutState);

	/** Compares the current data bound to the vertex interface with a prior cached state. */
	METASOUNDGRAPHCORE_API void CompareVertexInterfaceDataToPriorState(const FInputVertexInterfaceData& InCurrentInterface, const TArray<FVertexDataState>& InPriorState, TSortedVertexNameMap<FAnyDataReference>& OutUpdates);
	/** Compares the current data bound to the vertex interface with a prior cached state. */
	METASOUNDGRAPHCORE_API void CompareVertexInterfaceDataToPriorState(const FOutputVertexInterfaceData& InCurrentInterface, const TArray<FVertexDataState>& InPriorState, TSortedVertexNameMap<FAnyDataReference>& OutUpdates);
}
