// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MetasoundFrontendController.h"
#include "MetasoundFrontendDocument.h"
#include "MetasoundFrontendDocumentAccessPtr.h"
#include "Misc/Guid.h"

namespace Metasound
{
	namespace Frontend
	{
		class METASOUNDFRONTEND_API FVariableController : public IVariableController
		{
		public:
			struct FInitParams
			{
				FVariableAccessPtr VariablePtr;
				FGraphHandle OwningGraph;
			};

			FVariableController(const FInitParams& InParams);
			virtual ~FVariableController() = default;

			/** Returns true if the controller is in a valid state. */
			virtual bool IsValid() const override;

			virtual FGuid GetID() const override;
			
			/** Returns the data type name associated with this variable. */
			virtual const FName& GetDataType() const override;

			/** Returns the name associated with this variable. */
			virtual const FName& GetName() const override;

			/** Sets the name associated with this variable. */
			virtual void SetName(const FName& InName) override;
			
#if WITH_EDITOR
			/** Returns the human readable name associated with this variable. */
			virtual FText GetDisplayName() const override;

			/** Sets the human readable name associated with this variable. */
			virtual void SetDisplayName(const FText& InDisplayName) override;

			/** Returns the human readable description associated with this variable. */
			virtual FText GetDescription() const override;

			/** Sets the human readable description associated with this variable. */
			virtual void SetDescription(const FText& InDescription) override;
#endif // WITH_EDITOR

			/** Returns the mutator node associated with this variable. */
			virtual FNodeHandle FindMutatorNode() override;

			/** Returns the mutator node associated with this variable. */
			virtual FConstNodeHandle FindMutatorNode() const override;

			/** Returns the accessor nodes associated with this variable. */
			virtual TArray<FNodeHandle> FindAccessorNodes() override;

			/** Returns the accessor nodes associated with this variable. */
			virtual TArray<FConstNodeHandle> FindAccessorNodes() const override;

			/** Returns the deferred accessor nodes associated with this variable. */
			virtual TArray<FNodeHandle> FindDeferredAccessorNodes() override;

			/** Returns the deferred accessor nodes associated with this variable. */
			virtual TArray<FConstNodeHandle> FindDeferredAccessorNodes() const override;
			
			/** Returns a FGraphHandle to the node which owns this variable. */
			virtual FGraphHandle GetOwningGraph() override;
			
			/** Returns a FConstGraphHandle to the node which owns this variable. */
			virtual FConstGraphHandle GetOwningGraph() const override;

			/** Returns the value for the given variable instance if set. */
			virtual const FMetasoundFrontendLiteral& GetLiteral() const override;

			/** Sets the value for the given variable instance */
			virtual bool SetLiteral(const FMetasoundFrontendLiteral& InLiteral) override;

		private:
			virtual FConstDocumentAccess ShareAccess() const override;
			virtual FDocumentAccess ShareAccess() override;

			TArray<FNodeHandle> GetNodeArray(const TArray<FGuid>& InNodeIDs);
			TArray<FConstNodeHandle> GetNodeArray(const TArray<FGuid>& InNodeIDs) const;

			FVariableAccessPtr VariablePtr;
			FGraphHandle OwningGraph;
		};
	}
}
