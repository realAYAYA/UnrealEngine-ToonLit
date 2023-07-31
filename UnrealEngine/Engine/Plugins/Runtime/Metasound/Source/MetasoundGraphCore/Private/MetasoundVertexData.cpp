// Copyright Epic Games, Inc. All Rights Reserved.

#include "MetasoundVertexData.h"

#include "MetasoundDataReference.h"
#include "MetasoundLog.h"
#include "MetasoundThreadLocalDebug.h"

namespace Metasound
{
	namespace MetasoundVertexDataPrivate
	{
#if ENABLE_METASOUND_ACCESS_TYPE_COMPATIBILITY_TEST
		void CheckAccessTypeCompatibility(const FDataVertex& InDataVertex, const FAnyDataReference& InDataReference)
		{
			bool bIsCompatible = false;
			const EDataReferenceAccessType ReferenceAccessType = InDataReference.GetAccessType();

			switch (InDataVertex.AccessType)
			{
				case EVertexAccessType::Reference:
					// Reference vertices can accept read, write and value types
					bIsCompatible = (EDataReferenceAccessType::Read == ReferenceAccessType) || (EDataReferenceAccessType::Write == ReferenceAccessType) || (EDataReferenceAccessType::Value == ReferenceAccessType);
					break;

				case EVertexAccessType::Value:
					// value vertices require a value data reference type
					bIsCompatible = EDataReferenceAccessType::Value == ReferenceAccessType;
					break;

				default:
					{
						checkNoEntry();
					}
			}

			checkf(bIsCompatible, TEXT("Vertex access type \"%s\" is incompatible with data access type \"%s\" on vertex \"%s\" on node \"%s\""), *LexToString(InDataVertex.AccessType), *LexToString(ReferenceAccessType), *InDataVertex.VertexName.ToString(), ThreadLocalDebug::GetActiveNodeClassNameAndVersion());
		}
#endif // #if ENABLE_METASOUND_ACCESS_TYPE_COMPATIBILITY_TEST

		template<typename VertexType>
		void EmplaceBindings(TArray<TBinding<VertexType>>& InArray, const TVertexInterfaceGroup<VertexType>& InVertexInterface)
		{
			for (const VertexType& DataVertex : InVertexInterface)
			{
				InArray.Emplace(DataVertex);
			}
		}

		template<typename BindingType>
		BindingType* Find(TArray<BindingType>& InBindings, const FVertexName& InVertexName)
		{
			auto IsVertexWithName = [&InVertexName](const BindingType& InBinding)
			{
				return InBinding.GetVertex().VertexName == InVertexName;
			};
			return InBindings.FindByPredicate(IsVertexWithName);
		}

		template<typename BindingType>
		const BindingType* Find(const TArray<BindingType>& InBindings, const FVertexName& InVertexName)
		{
			auto IsVertexWithName = [&InVertexName](const BindingType& InBinding)
			{
				return InBinding.GetVertex().VertexName == InVertexName;
			};
			return InBindings.FindByPredicate(IsVertexWithName);
		}

		template<typename BindingType>
		void BindVertex(TArray<BindingType>& InBindings, const FVertexName& InVertexName, FAnyDataReference&& InDataReference)
		{
			if (BindingType* Binding = Find(InBindings, InVertexName))
			{
				if (Binding->GetVertex().DataTypeName == InDataReference.GetDataTypeName())
				{
					Binding->Bind(MoveTemp(InDataReference));
				}
				else
				{
					UE_LOG(LogMetaSound, Warning, TEXT("Failed bind vertex with name '%s'. Supplied data type (%s) does not match vertex data type (%s)"), *InVertexName.ToString(), *InDataReference.GetDataTypeName().ToString(), *Binding->GetVertex().DataTypeName.ToString());
				}
			}
			else
			{
				UE_LOG(LogMetaSound, Warning, TEXT("Failed find vertex with name '%s'. Failed to bind vertex data"), *InVertexName.ToString());
			}
		}

		template<typename BindingType>
		void BindVertex(TArray<BindingType>& InBindings, const FVertexName& InVertexName, const FAnyDataReference& InDataReference)
		{
			BindVertex<BindingType>(InBindings, InVertexName, FAnyDataReference{InDataReference});
		}

		template<typename BindingType>
		void Bind(TArray<BindingType>& InTargetBindings, const TArray<BindingType>& InSourceBindings)
		{
			for (const BindingType& SourceBinding : InSourceBindings)
			{
				if (const FAnyDataReference* DataReference = SourceBinding.GetDataReference())
				{
					if (BindingType* TargetBinding = Find(InTargetBindings, SourceBinding.GetVertex().VertexName))
					{
						if (DataReference->GetDataTypeName() == TargetBinding->GetVertex().DataTypeName)
						{
							TargetBinding->Bind(*DataReference);
						}
					}
				}
			}
		}

		template<typename BindingType>
		void Bind(TArray<BindingType>& InBindings, const FDataReferenceCollection& InCollection)
		{
			for (BindingType& Binding : InBindings)
			{
				const FDataVertex& Vertex = Binding.GetVertex();

				if (const FAnyDataReference* DataRef = InCollection.FindDataReference(Vertex.VertexName))
				{
					Binding.Bind(*DataRef);
				}
			}
		}

		template<typename BindingType>
		bool IsVertexBound(const TArray<BindingType>& InBindings, const FVertexName& InVertexName)
		{
			if (const BindingType* Binding = Find(InBindings, InVertexName))
			{
				return Binding->IsBound();
			}
			return false;
		}

		template<typename BindingType>
		bool AreAllVerticesBound(const TArray<BindingType>& InBindings)
		{
			return Algo::AllOf(InBindings, [](const BindingType& Binding) { return Binding.IsBound(); });
		}


		template<typename BindingType>
		EDataReferenceAccessType GetVertexDataAccessType(const TArray<BindingType>& InBindings, const FVertexName& InVertexName)
		{
			if (const BindingType* Binding = Find(InBindings, InVertexName))
			{
				return Binding->GetAccessType();
			}
			return EDataReferenceAccessType::None;
		}

		template<typename BindingType>
		FDataReferenceCollection ToDataReferenceCollection(const TArray<BindingType>& InBindings)
		{
			FDataReferenceCollection Collection;
			for (const BindingType& Binding : InBindings)
			{
				if (const FAnyDataReference* Ref = Binding.GetDataReference())
				{
					Collection.AddDataReference(Binding.GetVertex().VertexName, FAnyDataReference{*Ref});
				}
			}
			return Collection;
		}

		template<typename BindingType>
		const FAnyDataReference* FindDataReference(const TArray<BindingType>& InBindings, const FVertexName& InVertexName)
		{
			if (const BindingType* Binding = Find<BindingType>(InBindings, InVertexName))
			{
				return Binding->GetDataReference();
			}
			return nullptr;
		}
	}

	FInputVertexInterfaceData::FInputVertexInterfaceData(const FInputVertexInterface& InVertexInterface)
	{
		MetasoundVertexDataPrivate::EmplaceBindings(Bindings, InVertexInterface);
	}

	
	void FInputVertexInterfaceData::BindVertex(const FVertexName& InVertexName, FAnyDataReference&& InDataReference)
	{
		MetasoundVertexDataPrivate::BindVertex(Bindings, InVertexName, MoveTemp(InDataReference));
	}

	void FInputVertexInterfaceData::BindVertex(const FVertexName& InVertexName, const FAnyDataReference& InDataReference)
	{
		MetasoundVertexDataPrivate::BindVertex(Bindings, InVertexName, InDataReference);
	}

	void FInputVertexInterfaceData::Bind(const FInputVertexInterfaceData& InVertexData)
	{
		MetasoundVertexDataPrivate::Bind(Bindings, InVertexData.Bindings);
	}

	void FInputVertexInterfaceData::Bind(const FDataReferenceCollection& InCollection)
	{
		MetasoundVertexDataPrivate::Bind(Bindings, InCollection);
	}

	FDataReferenceCollection FInputVertexInterfaceData::ToDataReferenceCollection() const
	{
		return MetasoundVertexDataPrivate::ToDataReferenceCollection(Bindings);
	}

	bool FInputVertexInterfaceData::IsVertexBound(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::IsVertexBound(Bindings, InVertexName);
	}

	const FInputDataVertex& FInputVertexInterfaceData::GetVertex(const FVertexName& InVertexName) const
	{
		return FindChecked(InVertexName)->GetVertex();
	}

	EDataReferenceAccessType FInputVertexInterfaceData::GetVertexDataAccessType(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::GetVertexDataAccessType(Bindings, InVertexName);
	}

	bool FInputVertexInterfaceData::AreAllVerticesBound() const
	{
		return MetasoundVertexDataPrivate::AreAllVerticesBound(Bindings);
	}

	const FAnyDataReference* FInputVertexInterfaceData::FindDataReference(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::FindDataReference(Bindings, InVertexName);
	}

	FInputVertexInterfaceData::FBinding* FInputVertexInterfaceData::Find(const FVertexName& InVertexName)
	{
		return MetasoundVertexDataPrivate::Find(Bindings, InVertexName);
	}

	const FInputVertexInterfaceData::FBinding* FInputVertexInterfaceData::Find(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::Find(Bindings, InVertexName);
	}

	FInputVertexInterfaceData::FBinding* FInputVertexInterfaceData::FindChecked(const FVertexName& InVertexName)
	{
		FBinding* Binding = Find(InVertexName);
		checkf(nullptr != Binding, TEXT("Attempt to access vertex \"%s\" which does not exist on interface."), *InVertexName.ToString());
		return Binding;
	}

	const FInputVertexInterfaceData::FBinding* FInputVertexInterfaceData::FindChecked(const FVertexName& InVertexName) const
	{
		const FBinding* Binding = Find(InVertexName);
		checkf(nullptr != Binding, TEXT("Attempt to access vertex \"%s\" which does not exist on interface."), *InVertexName.ToString());
		return Binding;
	}


	FOutputVertexInterfaceData::FOutputVertexInterfaceData(const FOutputVertexInterface& InVertexInterface)
	{
		MetasoundVertexDataPrivate::EmplaceBindings(Bindings, InVertexInterface);
	}

	
	void FOutputVertexInterfaceData::BindVertex(const FVertexName& InVertexName, FAnyDataReference&& InDataReference)
	{
		MetasoundVertexDataPrivate::BindVertex(Bindings, InVertexName, MoveTemp(InDataReference));
	}

	void FOutputVertexInterfaceData::BindVertex(const FVertexName& InVertexName, const FAnyDataReference& InDataReference)
	{
		MetasoundVertexDataPrivate::BindVertex(Bindings, InVertexName, InDataReference);
	}

	void FOutputVertexInterfaceData::Bind(const FOutputVertexInterfaceData& InVertexData)
	{
		MetasoundVertexDataPrivate::Bind(Bindings, InVertexData.Bindings);
	}

	void FOutputVertexInterfaceData::Bind(const FDataReferenceCollection& InCollection)
	{
		MetasoundVertexDataPrivate::Bind(Bindings, InCollection);
	}

	FDataReferenceCollection FOutputVertexInterfaceData::ToDataReferenceCollection() const
	{
		return MetasoundVertexDataPrivate::ToDataReferenceCollection(Bindings);
	}

	bool FOutputVertexInterfaceData::IsVertexBound(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::IsVertexBound(Bindings, InVertexName);
	}

	const FOutputDataVertex& FOutputVertexInterfaceData::GetVertex(const FVertexName& InVertexName) const
	{
		return FindChecked(InVertexName)->GetVertex();
	}

	EDataReferenceAccessType FOutputVertexInterfaceData::GetVertexDataAccessType(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::GetVertexDataAccessType(Bindings, InVertexName);
	}

	bool FOutputVertexInterfaceData::AreAllVerticesBound() const
	{
		return MetasoundVertexDataPrivate::AreAllVerticesBound(Bindings);
	}

	const FAnyDataReference* FOutputVertexInterfaceData::FindDataReference(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::FindDataReference(Bindings, InVertexName);
	}


	FOutputVertexInterfaceData::FBinding* FOutputVertexInterfaceData::Find(const FVertexName& InVertexName)
	{
		return MetasoundVertexDataPrivate::Find(Bindings, InVertexName);
	}

	const FOutputVertexInterfaceData::FBinding* FOutputVertexInterfaceData::Find(const FVertexName& InVertexName) const
	{
		return MetasoundVertexDataPrivate::Find(Bindings, InVertexName);
	}

	FOutputVertexInterfaceData::FBinding* FOutputVertexInterfaceData::FindChecked(const FVertexName& InVertexName)
	{
		FBinding* Binding = Find(InVertexName);
		checkf(nullptr != Binding, TEXT("Attempt to access vertex \"%s\" which does not exist on interface."), *InVertexName.ToString());
		return Binding;
	}

	const FOutputVertexInterfaceData::FBinding* FOutputVertexInterfaceData::FindChecked(const FVertexName& InVertexName) const
	{
		const FBinding* Binding = Find(InVertexName);
		checkf(nullptr != Binding, TEXT("Attempt to access vertex \"%s\" which does not exist on interface."), *InVertexName.ToString());
		return Binding;
	}

	void FVertexInterfaceData::Bind(const FVertexInterfaceData& InVertexData)
	{
		InputVertexInterfaceData.Bind(InVertexData.GetInputs());
		OutputVertexInterfaceData.Bind(InVertexData.GetOutputs());
	}
}
