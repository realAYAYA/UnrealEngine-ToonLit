// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/Array.h"
#include "MetasoundDataReference.h"
#include "MetasoundEnvironment.h"
#include "MetasoundNodeInterface.h"
#include "MetasoundOperatorInterface.h"
#include "Templates/UniquePtr.h"
#include "UObject/NameTypes.h"


namespace Metasound
{
	// Forward Declare
	class IOperatorBuilder;

	/** IOperatorBuildError
	 *
	 * This interface is intended for errors encountered when building an GraphOperator.
	 */
	class IOperatorBuildError
	{
		public:
			virtual ~IOperatorBuildError() = default;

			/** Returns the type of error. */
			virtual const FName& GetErrorType() const = 0;

			/** Returns a human readable error description. */
			virtual const FText& GetErrorDescription() const = 0;

			/** Returns an array of destinations associated with the error. */
			virtual const TArray<FInputDataDestination>& GetInputDataDestinations() const = 0;
			
			/** Returns an array of sources associated with the error. */
			virtual const TArray<FOutputDataSource>& GetOutputDataSources() const = 0;

			/** Returns an array of Nodes associated with the error. */
			virtual const TArray<const INode*>& GetNodes() const = 0;

			/** Returns an array of edges associated with the error. */
			virtual const TArray<FDataEdge>& GetDataEdges() const = 0;
	};

	/** Array of build errors. */
	using FBuildErrorArray = TArray<TUniquePtr<IOperatorBuildError>>;

	/** Structure of all resulting data generated during graph operator build. */
	struct FBuildResults
	{
		/** An array of errors. Errors can be added if issues occur while creating an IOperator. */
		FBuildErrorArray Errors;

		/** Internal data references if enabled by build settings (not populated if disabled). */
		TMap<FGuid, FDataReferenceCollection> InternalDataReferences;
	};

	/** FCreateOperatorParams holds the parameters provided to operator factories
	 * during the creation of an IOperator
	 */
	struct FCreateOperatorParams
	{
		/** The node associated with this factory and the desired IOperator. */
		const INode& Node;

		/** General operator settings for the graph. */
		const FOperatorSettings& OperatorSettings;

		/** Collection of input parameters available for to an IOperator. */
		const FDataReferenceCollection& InputDataReferences;

		/** Environment settings available. */
		const FMetasoundEnvironment& Environment;

		/** Pointer to builder actively building graph. */
		const IOperatorBuilder* Builder = nullptr;
	};

	/** FBuildOperatorParams holds the parameters provided to operator factories
	 * during the creation of an IOperator
	 */
	struct FBuildOperatorParams
	{
		/** The node associated with this factory and the desired IOperator. */
		const INode& Node;

		/** General operator settings for the graph. */
		const FOperatorSettings& OperatorSettings;

		/** Input data references for an IOperator */
		const FInputVertexInterfaceData& InputData;

		/** Environment settings available. */
		const FMetasoundEnvironment& Environment;

		/** Pointer to builder actively building graph. */
		const IOperatorBuilder* Builder = nullptr;
	};

	/** Parameters for building an operator from a graph. 
	  *
	  * This object is slated to be deprecated in UE 5.2
	  */
	struct FBuildGraphParams
	{
		/** Reference to graph being built. */
		const IGraph& Graph;

		/** General operator settings for the graph. */
		const FOperatorSettings& OperatorSettings;

		/** Collection of input parameters available for an IOperator. */
		const FDataReferenceCollection& InputDataReferences;

		/** Environment settings available. */
		const FMetasoundEnvironment& Environment;
	};

	/** Parameters for building an operator from a graph. */
	struct FBuildGraphOperatorParams
	{
		/** Reference to graph being built. */
		const IGraph& Graph;

		/** General operator settings for the graph. */
		const FOperatorSettings& OperatorSettings;

		/** Bound input data available for an IOperator. */
		const FInputVertexInterfaceData& InputData;

		/** Environment settings available. */
		const FMetasoundEnvironment& Environment;
	};

	/** Convenience template for adding build errors.
	 *
	 * The function can be used in the following way:
	 * 
	 * FBuildErrorArray MyErrorArray;
	 * AddBuildError<FMyBuildErrorType>(MyErrorArray, MyBuildErrorConstructorArgs...);
	 *
	 * @param OutErrors - Array which holds the errors.
	 * @param Args - Constructor arguments for the error.
	 */
	template<typename ErrorType, typename... ArgTypes>
	void AddBuildError(FBuildErrorArray& OutErrors, ArgTypes&&... Args)
	{
		OutErrors.Add(MakeUnique<ErrorType>(Forward<ArgTypes>(Args)...));
	}


	/** IOperatorFactory
	 *
	 * IOperatorFactory defines an interface for building an IOperator from an INode.  In practice,
	 * each INode returns its own IOperatorFactory through the INode::GetDefaultOperatorFactory() 
	 * member function.
	 */
	class METASOUNDGRAPHCORE_API IOperatorFactory
	{
		public:
			virtual ~IOperatorFactory() = default;

			virtual TUniquePtr<IOperator> CreateOperator(const FBuildOperatorParams& InParams, FBuildResults& OutResults) = 0;

			UE_DEPRECATED(5.1, "Use CreateOperator overload providing the FBuildResults struct.")
			virtual TUniquePtr<IOperator> CreateOperator(const FCreateOperatorParams& InParams, FBuildErrorArray& OutErrors)
			{
				// Map inputs to new structures
				FInputVertexInterfaceData InputData(InParams.Node.GetVertexInterface().GetInputInterface());
				InputData.Bind(InParams.InputDataReferences);

				FBuildOperatorParams Params2 { InParams.Node, InParams.OperatorSettings, InputData, InParams.Environment, InParams.Builder };

				// Create operator
				FBuildResults Results;
				TUniquePtr<IOperator> Operator = CreateOperator(Params2, Results);

				// Map new output to old output
				OutErrors = MoveTemp(Results.Errors);

				return MoveTemp(Operator);
			}
	};


	/** IOperatorBuilder
	 *
	 * Defines an interface for building a graph of operators from a graph of nodes. 
	 */
	class IOperatorBuilder 
	{
		public:
			/** A TUniquePtr of an IOperatorBuildError */
			using FBuildErrorPtr = TUniquePtr<IOperatorBuildError>;

			virtual ~IOperatorBuilder() = default;

			/** Build a graph operator from a graph.
			 *
			 * @params InParams - Input parameters for building a graph.
			 * @param OutErrors - An array of errors which occurred during while building the operator.
			 *
			 * @return A unique pointer to the built IOperator. Null if build failed.
			 */
			UE_DEPRECATED(5.1, "Use BuildGraphOperator overload providing the FBuildResults struct.")
			virtual TUniquePtr<IOperator> BuildGraphOperator(const FBuildGraphParams& InParams, FBuildErrorArray& OutErrors) const 
			{ 
				FInputVertexInterfaceData InputData(InParams.Graph.GetVertexInterface().GetInputInterface());
				InputData.Bind(InParams.InputDataReferences);
				FBuildGraphOperatorParams NewParams{InParams.Graph, InParams.OperatorSettings, InputData, InParams.Environment};
				FBuildResults Results;

				TUniquePtr<IOperator> GraphOperator = BuildGraphOperator(NewParams, Results);

				OutErrors.Append(MoveTemp(Results.Errors));
				
				return MoveTemp(GraphOperator);
			}

			/** Build a graph operator from a graph.
			 *
			 * @params InParams - Input parameters for building a graph.
			 * @param OutResults - Results data pertaining to the given build operator result.
			 *
			 * @return A unique pointer to the built IOperator. Null if build failed.
			 */
			virtual TUniquePtr<IOperator> BuildGraphOperator(const FBuildGraphOperatorParams& InParams, FBuildResults& OutResults) const = 0;
	};
}
