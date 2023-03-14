// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GeometryFlowNode.h"
#include "GeometryFlowMovableData.h"


namespace UE
{
namespace GeometryFlow
{

using namespace UE::Geometry;


template<typename T, int32 StorageTypeIdentifier>
class TSourceNode : public FNode
{
protected:
	using DataType = TMovableData<T, StorageTypeIdentifier>;

	TSafeSharedPtr<DataType> Value;

public:
	using CppType = T;
	static constexpr int DataTypeIdentifier = StorageTypeIdentifier;

	static const FString OutParamValue() { return TEXT("Value"); }

public:
	TSourceNode()
	{
		T InitialValue;
		Value = MakeSafeShared<DataType>(InitialValue);
		AddOutput(OutParamValue(), MakeUnique<TBasicNodeOutput<T, StorageTypeIdentifier>>());

		UpdateSourceValue(InitialValue);
	}

	void UpdateSourceValue(const T& NewValue)
	{
		Value->SetData(NewValue);
		SetOutput(OutParamValue(), Value);
	}

	virtual void CollectRequirements(const TArray<FString>& Outputs, TArray<FEvalRequirement>& RequiredInputsOut) override
	{
		return;
	}

	virtual void CollectAllRequirements(TArray<FEvalRequirement>& RequiredInputsOut) override
	{
		return;
	}

	virtual void Evaluate(
		const FNamedDataMap& DatasIn,
		FNamedDataMap& DatasOut,
		TUniquePtr<FEvaluationInfo>& EvaluationInfo) override
	{
		if ( ensure(DatasOut.Contains(OutParamValue())) )
		{
			DatasOut.SetData(OutParamValue(), GetOutput(OutParamValue()));
		}
	}
};




/**
 * This macro declares a MovableData, Node Input/Output, and TSourceNode type for a given C++ type/identifier
 * 
 * eg GEOMETRYFLOW_DECLARE_BASIC_TYPES(Int32, int, (int)EDataTypes::Integer) will declare/define:
 *     - TMovableData type named FDataInt32
 *     - TBasicNodeInput type named FInt32Input
 *     - TBasicNodeOutput type named FInt32Output
 *     - TSourceNode type named FInt32SourceNode
 *
 */
#define GEOMETRYFLOW_DECLARE_BASIC_TYPES(TypeName, CppType, TypeIdentifier) \
	typedef TMovableData<CppType, TypeIdentifier> FData##TypeName; \
	typedef TBasicNodeInput<CppType, TypeIdentifier> F##TypeName##Input; \
	typedef TBasicNodeOutput<CppType, TypeIdentifier> F##TypeName##Output; \
	typedef TSourceNode<CppType, TypeIdentifier> F##TypeName##SourceNode; 





/**
 * This macro defines two classes for a given C++ class:
 *   - TMovableData typedef named FData[XYZ]Settings
 *   - TSourceNode typedef named F[XYZ]SettingsSourceNode
 *
 * The assumption is the class has an integer static/constexpr member SettingsType::DataTypeIdentifier that defines the type integer
 */
#define GEOMETRYFLOW_DECLARE_SETTINGS_TYPES(CppType, ReadableName) \
	typedef TMovableData<CppType, CppType::DataTypeIdentifier> FData##ReadableName##Settings; \
	typedef TSourceNode<CppType, CppType::DataTypeIdentifier> F##ReadableName##SettingsSourceNode;






/**
 * Declare basic types for math
 */
GEOMETRYFLOW_DECLARE_BASIC_TYPES(Int32, int, (int)EDataTypes::Integer)
GEOMETRYFLOW_DECLARE_BASIC_TYPES(Float, float, (int)EDataTypes::Float)
GEOMETRYFLOW_DECLARE_BASIC_TYPES(Double, double, (int)EDataTypes::Double)
GEOMETRYFLOW_DECLARE_BASIC_TYPES(Vector3f, FVector3f, (int)EDataTypes::Vector3f)
GEOMETRYFLOW_DECLARE_BASIC_TYPES(Vector3d, FVector3d, (int)EDataTypes::Vector3d)
GEOMETRYFLOW_DECLARE_BASIC_TYPES(Name, FName, (int)EDataTypes::Name)


}	// end namespace GeometryFlow
}	// end namespace UE