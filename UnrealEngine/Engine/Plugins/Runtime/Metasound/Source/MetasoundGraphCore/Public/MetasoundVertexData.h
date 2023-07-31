// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Algo/AllOf.h"
#include "Containers/Array.h"
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
		template<typename V>
		using EnableIfIsInputDataVertex = std::enable_if_t<std::is_same_v<FInputDataVertex, V>>;

		// Tests to see that the access type of the data reference is compatible 
		// with the access type of the vertex.
#if ENABLE_METASOUND_ACCESS_TYPE_COMPATIBILITY_TEST
		void METASOUNDGRAPHCORE_API CheckAccessTypeCompatibility(const FDataVertex& InDataVertex, const FAnyDataReference& InDataReference);
#else
		FORCEINLINE void CheckAccessTypeCompatibility(const FDataVertex& InDataVertex, const FAnyDataReference& InDataReference) {}
#endif // #if DO_CHECK

		// Binds a vertex to a data reference.
		template<typename VertexType>
		class TBinding
		{

		public:
			TBinding(VertexType&& InVertex)
			: Vertex(MoveTemp(InVertex))
			{
			}

			TBinding(const VertexType& InVertex)
			: Vertex(InVertex)
			{
			}

			template<typename DataReferenceType>
			void Bind(const DataReferenceType& InDataReference)
			{
				check(Vertex.DataTypeName == InDataReference.GetDataTypeName());
				Data.Emplace(InDataReference);
				CheckAccessTypeCompatibility(Vertex, *Data);
			}

			void Bind(FAnyDataReference&& InAnyDataReference)
			{
				check(Vertex.DataTypeName == InAnyDataReference.GetDataTypeName());
				Data.Emplace(MoveTemp(InAnyDataReference));
				CheckAccessTypeCompatibility(Vertex, *Data);
			}
			
			const VertexType& GetVertex() const
			{
				return Vertex;
			}

			bool IsBound() const
			{
				return Data.IsSet();
			}

			EDataReferenceAccessType GetAccessType() const
			{
				if (Data.IsSet())
				{
					Data->GetAccessType();
				}
				return EDataReferenceAccessType::None;
			}

			// Get data reference 
			const FAnyDataReference* GetDataReference() const
			{
				return Data.GetPtrOrNull();
			}

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
			template<
				typename DataType,
				typename V = VertexType,
				typename = EnableIfIsInputDataVertex<V>
			> 
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
			template<
				typename DataType,
				typename V = VertexType,
				typename = EnableIfIsInputDataVertex<V>
			> 
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
				Data.Emplace(TDataValueReference<DataType>::CreateNew(InValue));
				CheckAccessTypeCompatibility(Vertex, *Data);
			}

		private:

			VertexType Vertex;
			TOptional<FAnyDataReference> Data;
		};
	}

	/** An input vertex interface with optionally bound data references. */
	class METASOUNDGRAPHCORE_API FInputVertexInterfaceData
	{
		using FBinding = MetasoundVertexDataPrivate::TBinding<FInputDataVertex>;

	public:

		using FRangedForIteratorType = typename TArray<FBinding>::RangedForIteratorType;
		using FRangedForConstIteratorType = typename TArray<FBinding>::RangedForConstIteratorType;

		FInputVertexInterfaceData() = default;

		/** Construct with an FInputVertexInterface. */
		FInputVertexInterfaceData(const FInputVertexInterface& InVertexInterface);

		/** Bind a value vertex from a value reference. */
		template<typename DataType>
		void BindValueVertex(const FVertexName& InVertexName, const TDataValueReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{InDataReference});
		}

		/** Bind a read vertex from a value reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataValueReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{static_cast<TDataReadReference<DataType>>(InDataReference)});
		}

		/** Bind a read vertex from a read reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataReadReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{InDataReference});
		}

		/** Bind a read vertex from a write reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{static_cast<TDataReadReference<DataType>>(InDataReference)});
		}

		/** Bind a write vertex from a write reference. */
		template<typename DataType>
		void BindWriteVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{InDataReference});
		}

		/** Bind a data reference to a vertex. */
		void BindVertex(const FVertexName& InVertexName, FAnyDataReference&& InDataReference);

		/** Bind a data reference to a vertex. */
		void BindVertex(const FVertexName& InVertexName, const FAnyDataReference& InDataReference);

		/** Bind vertex data using other vertex data. */
		void Bind(const FInputVertexInterfaceData& InInputVertexData);

		/** Bind a data references to vertices with matching vertex names. */
		void Bind(const FDataReferenceCollection& InCollection);

		/** Convert vertex data to a data reference collection. */
		FDataReferenceCollection ToDataReferenceCollection() const;

		/** Return the vertex associated with the vertex name. */
		const FInputDataVertex& GetVertex(const FVertexName& InVertexName) const;

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
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetValue<DataType>();
		}

		/**  Gets the value of the bound data reference if it exists. Otherwise
		 * create and return a value by constructing one using the Vertex's 
		 * default literal. */
		template<typename DataType>
		DataType GetOrCreateDefaultValue(const FVertexName& InVertexName, const FOperatorSettings& InSettings) const
		{
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetOrCreateDefaultValue<DataType>(InSettings);
		}

		/** Set the value of a vertex. */
		template<typename DataType>
		void SetValue(const FVertexName& InVertexName, const DataType& InValue)
		{
			FBinding* Binding = FindChecked(InVertexName);
			return Binding->SetValue<DataType>(InValue);
		}

		/** Get data read reference assuming data is bound and read or write accessible. */
		template<typename DataType> 
		TDataReadReference<DataType> GetDataReadReference(const FVertexName& InVertexName) const
		{
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataReadReference<DataType>();
		}

		/** Get the bound data read reference if it exists. Otherwise create and 
		 * return a data read reference by constructing one using the Vertex's 
		 * default literal.
		 */
		template<typename DataType> 
		TDataReadReference<DataType> GetOrCreateDefaultDataReadReference(const FVertexName& InVertexName, const FOperatorSettings& InSettings) const
		{
			if (const FBinding* Binding = Find(InVertexName))
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
			if (const FBinding* Binding = Find(InVertexName))
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
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataWriteReference<DataType>();
		}

		/** Get the bound data write reference if it exists. Otherwise create and 
		 * return a data write reference by constructing one using the Vertex's 
		 * default literal.
		 */
		template<typename DataType> 
		TDataWriteReference<DataType> GetOrCreateDefaultDataWriteReference(const FVertexName& InVertexName, const FOperatorSettings& InSettings) const
		{
			if (const FBinding* Binding = Find(InVertexName))
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
			if (const FBinding* Binding = Find(InVertexName))
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

		FBinding* Find(const FVertexName& InVertexName);
		const FBinding* Find(const FVertexName& InVertexName) const;

		FBinding* FindChecked(const FVertexName& InVertexName);
		const FBinding* FindChecked(const FVertexName& InVertexName) const;

		TArray<FBinding> Bindings;
	};

	/** An output vertex interface with optionally bound data references. */
	class METASOUNDGRAPHCORE_API FOutputVertexInterfaceData
	{
		using FBinding = MetasoundVertexDataPrivate::TBinding<FOutputDataVertex>;

	public:

		using FRangedForIteratorType = typename TArray<FBinding>::RangedForIteratorType;
		using FRangedForConstIteratorType = typename TArray<FBinding>::RangedForConstIteratorType;

		FOutputVertexInterfaceData() = default;

		/* Construct using an FOutputVertexInterface. */
		FOutputVertexInterfaceData(const FOutputVertexInterface& InVertexInterface);

		/** Bind a value vertex from a value reference. */
		template<typename DataType>
		void BindValueVertex(const FVertexName& InVertexName, const TDataValueReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{InDataReference});
		}

		/** Bind a read vertex from a value reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataValueReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{static_cast<TDataReadReference<DataType>>(InDataReference)});
		}

		/** Bind a read vertex from a read reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataReadReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{InDataReference});
		}

		/** Bind a read vertex from a write reference. */
		template<typename DataType>
		void BindReadVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{static_cast<TDataReadReference<DataType>>(InDataReference)});
		}

		/** Bind a write vertex from a write reference. */
		template<typename DataType>
		void BindWriteVertex(const FVertexName& InVertexName, const TDataWriteReference<DataType>& InDataReference)
		{
			BindVertex(InVertexName, FAnyDataReference{InDataReference});
		}

		/** Bind a data reference to a vertex. */
		void BindVertex(const FVertexName& InVertexName, FAnyDataReference&& InDataReference);

		/** Bind a data reference to a vertex. */
		void BindVertex(const FVertexName& InVertexName, const FAnyDataReference& InDataReference);

		/** Bind vertex data using other vertex data. */
		void Bind(const FOutputVertexInterfaceData& InOutputVertexData);

		/** Bind a data references to vertices with matching vertex names. */
		void Bind(const FDataReferenceCollection& InCollection);

		/** Converts the vertex data to a data reference collection. */
		FDataReferenceCollection ToDataReferenceCollection() const;

		/** Returns true if a vertex with the given vertex name exists and is bound
		 * to a data reference. */
		bool IsVertexBound(const FVertexName& InVertexName) const;

		/** Return the vertex associated with the vertex name. */
		const FOutputDataVertex& GetVertex(const FVertexName& InVertexName) const;

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
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetValue<DataType>();
		}

		/** Set the value of a vertex. */
		template<typename DataType>
		void SetValue(const FVertexName& InVertexName, const DataType& InValue)
		{
			FBinding* Binding = FindChecked(InVertexName);
			return Binding->SetValue<DataType>(InValue);
		}

		/** Get data read reference assuming data is bound and read or write accessible. */
		template<typename DataType> 
		TDataReadReference<DataType> GetDataReadReference(const FVertexName& InVertexName) const
		{
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataReadReference<DataType>();
		}

		/** Get the bound data read reference if it exists. Otherwise create and 
		 * return a data read reference by constructing one the supplied constructor 
		 * arguments.
		 */
		template<typename DataType, typename ... ConstructorArgTypes>
		TDataReadReference<DataType> GetOrConstructDataReadReference(const FVertexName& InVertexName, ConstructorArgTypes&&... ConstructorArgs) const
		{
			if (const FBinding* Binding = Find(InVertexName))
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
			const FBinding* Binding = FindChecked(InVertexName);
			return Binding->GetDataWriteReference<DataType>();
		}

		/** Get the bound data write reference if it exists. Otherwise create and 
		 * return a data write reference by constructing one the supplied constructor 
		 * arguments.
		 */
		template<typename DataType, typename ... ConstructorArgTypes>
		TDataWriteReference<DataType> GetOrConstructDataWriteReference(const FVertexName& InVertexName, ConstructorArgTypes&&... ConstructorArgs) const
		{
			if (const FBinding* Binding = Find(InVertexName))
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

		FBinding* Find(const FVertexName& InVertexName);
		const FBinding* Find(const FVertexName& InVertexName) const;

		FBinding* FindChecked(const FVertexName& InVertexName);
		const FBinding* FindChecked(const FVertexName& InVertexName) const;

		TArray<FBinding> Bindings;
	};


	/** A vertex interface with optionally bound data. */
	class FVertexInterfaceData
	{
	public:

		FVertexInterfaceData() = default;

		/** Construct using an FVertexInterface. */
		FVertexInterfaceData(const FVertexInterface& InVertexInterface)
		: InputVertexInterfaceData(InVertexInterface.GetInputInterface())
		, OutputVertexInterfaceData(InVertexInterface.GetOutputInterface())
		{
		}

		/** Bind vertex data using other vertex data. */
		void Bind(const FVertexInterfaceData& InVertexData);

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
}
