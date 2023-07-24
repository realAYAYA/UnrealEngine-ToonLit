// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#include "MetasoundBuilderInterface.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"

namespace Metasound
{
	/** FBuildErrorBase
	 *
	 * A general build error which contains a error type and human readable 
	 * error description.
	 */
	class METASOUNDGRAPHCORE_API FBuildErrorBase : public IOperatorBuildError
	{
		public:

			FBuildErrorBase(const FName& InErrorType, const FText& InErrorDescription);

			virtual ~FBuildErrorBase() = default;

			/** Returns the type of error. */
			virtual const FName& GetErrorType() const override;

			/** Returns a human readable error description. */
			virtual const FText& GetErrorDescription() const override;

			/** Returns an array of destinations associated with the error. */
			virtual const TArray<FInputDataDestination>& GetInputDataDestinations() const override;
			
			/** Returns an array of sources associated with the error. */
			virtual const TArray<FOutputDataSource>& GetOutputDataSources() const override;

			/** Returns an array of edges associated with the error. */
			virtual const TArray<FDataEdge>& GetDataEdges() const override;

			/** Returns an array of Nodes associated with the error. */
			virtual const TArray<const INode*>& GetNodes() const override;


		protected:
			// Add input destinations to be associated with error.
			void AddInputDataDestination(const FInputDataDestination& InInputDataDestination);
			void AddInputDataDestinations(TArrayView<const FInputDataDestination> InInputDataDestinations);

			// Add input destinations to be associated with error.
			void AddOutputDataSource(const FOutputDataSource& InOutputDataSource);
			void AddOutputDataSources(TArrayView<const FOutputDataSource> InOutputDataSources);

			// Add edges to be associated with error.
			void AddDataEdge(const FDataEdge& InEdge);
			void AddDataEdges(TArrayView<const FDataEdge> InEdges);

			// Add nodes to be associated with error.
			void AddNode(const INode& InNode);
			void AddNodes(TArrayView<INode const* const> InNodes);

		private:

			FName ErrorType;
			FText ErrorDescription;

			TArray<const INode*> Nodes;
			TArray<FDataEdge> Edges;
			TArray<FInputDataDestination> Destinations;
			TArray<FOutputDataSource> Sources;
	};

	/** FDanglingVertexError
	 *
	 * Caused by FDataEdges, FInputDataDestinations or FOutputDataSources 
	 * pointing to null nodes.
	 */
	class METASOUNDGRAPHCORE_API FDanglingVertexError : public FBuildErrorBase
	{
		public:
			static const FName ErrorType;

			FDanglingVertexError(const FInputDataDestination& InDest);
			FDanglingVertexError(const FOutputDataSource& InSource);
			FDanglingVertexError(const FDataEdge& InEdge);

			virtual ~FDanglingVertexError() = default;
		protected:

			FDanglingVertexError();

		private:
	};

	/** FMissingVertexError
	 *
	 * Caused by a referenced FDataVertex which does not exist on a node.
	 */
	class METASOUNDGRAPHCORE_API FMissingVertexError : public FBuildErrorBase
	{
		public:
			static const FName ErrorType;

			FMissingVertexError(const FInputDataDestination& InDestination);
			FMissingVertexError(const FOutputDataSource& InSource);

			virtual ~FMissingVertexError() = default;

		private:
	};


	/** FDuplicateInputError
	 *
	 * Caused by multiple FDataEdges pointing same FInputDataDestination
	 */
	class METASOUNDGRAPHCORE_API FDuplicateInputError : public FBuildErrorBase
	{
		public:
			static const FName ErrorType;

			FDuplicateInputError(const TArrayView<FDataEdge> InEdges);

			virtual ~FDuplicateInputError() = default;

		private:
	};

	/** FGraphCycleError
	 *
	 * Caused by circular paths in graph.
	 */
	class METASOUNDGRAPHCORE_API FGraphCycleError : public FBuildErrorBase
	{
		public:
			static const FName ErrorType;

			FGraphCycleError(TArrayView<INode const* const> InNodes, const TArray<FDataEdge>& InEdges);

			virtual ~FGraphCycleError() = default;

		private:
	};

	/** FNodePrunedError
	 *
	 * Caused by nodes which are in the graph but unreachable from the graph's
	 * inputs and/or outputs.
	 */
	class METASOUNDGRAPHCORE_API FNodePrunedError : public FBuildErrorBase
	{
		public:
			static const FName ErrorType;

			FNodePrunedError(const INode* InNode);

			virtual ~FNodePrunedError() = default;

		private:
	};

	/** FInternalError
	 *
	 * Caused by internal state or logic errors. 
	 */
	class METASOUNDGRAPHCORE_API FInternalError : public FBuildErrorBase
	{
		public:
			static const FName ErrorType;

			FInternalError(const FString& InFileName, int32 InLineNumber);

			virtual ~FInternalError() = default;

			const FString& GetFileName() const;
			int32 GetLineNumber() const;

		private:
			FString FileName;
			int32 LineNumber;
	};

	/** FMissingInputDataReferenceError
	 *
	 * Caused by IOperators not exposing expected IDataReferences in their input
	 * FDataReferenceCollection
	 */
	class METASOUNDGRAPHCORE_API FMissingInputDataReferenceError : public FBuildErrorBase
	{
		public:

			static const FName ErrorType;
			
			FMissingInputDataReferenceError(const FInputDataDestination& InInputDataDestination);

			virtual ~FMissingInputDataReferenceError() = default;
	};

	/** FMissingOutputDataReferenceError
	 *
	 * Caused by IOperators not exposing expected IDataReferences in their output
	 * FDataReferenceCollection
	 */
	class METASOUNDGRAPHCORE_API FMissingOutputDataReferenceError : public FBuildErrorBase
	{
		public:

			static const FName ErrorType;
			
			FMissingOutputDataReferenceError(const FOutputDataSource& InOutputDataSource);

			virtual ~FMissingOutputDataReferenceError() = default;

	};

	/** FInvalidConnectionDataTypeError
	 *
	 * Caused when edges describe a connection between vertices with different
	 * data types.
	 */
	class METASOUNDGRAPHCORE_API FInvalidConnectionDataTypeError  : public FBuildErrorBase
	{
		public:

			static const FName ErrorType;

			FInvalidConnectionDataTypeError(const FDataEdge& InEdge);

			virtual ~FInvalidConnectionDataTypeError() = default;
	};

	/** FInputReceiverInitializationError
	 *
	 * Caused by Inputs that are set to enable transmission fail to create a receiver.
	 */
	class METASOUNDGRAPHCORE_API FInputReceiverInitializationError : public FBuildErrorBase
	{
		public:

			static const FName ErrorType;
			
			FInputReceiverInitializationError(const INode& InInputNode, const FName& InVertexKey, const FName& InDataType);

			virtual ~FInputReceiverInitializationError() = default;
	};
} // namespace Metasound
