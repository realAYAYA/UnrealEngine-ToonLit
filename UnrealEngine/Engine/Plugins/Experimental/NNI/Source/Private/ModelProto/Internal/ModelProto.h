// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ModelProto.generated.h"



/* Enum classes
 *****************************************************************************/

UENUM()
enum class EAttributeProtoAttributeType
{
	UNDEFINED = 0,
	FLOAT = 1,
	INT = 2,
	STRING = 3,
	TENSOR = 4,
	GRAPH = 5,
	SPARSE_TENSOR = 11,
	FLOATS = 6,
	INTS = 7,
	STRINGS = 8,
	TENSORS = 9,
	GRAPHS = 10,
	SPARSE_TENSORS = 12
};

UENUM()
enum class ETensorProtoDataType
{
	UNDEFINED = 0,
	// Basic types.
	FLOAT = 1,   // float
	UINT8 = 2,   // uint8_t
	INT8 = 3,    // int8_t
	UINT16 = 4,  // uint16_t
	INT16 = 5,   // int16_t
	INT32 = 6,   // int32_t
	INT64 = 7,   // int64_t
	STRING = 8,  // string
	BOOL = 9,    // bool

	// IEEE754 half-precision floating-point format (16 bits wide).
	// This format has 1 sign bit, 5 exponent bits, and 10 mantissa bits.
	FLOAT16 = 10,

	DOUBLE = 11,
	UINT32 = 12,
	UINT64 = 13,
	COMPLEX64 = 14,     // complex with float32 real and imaginary components
	COMPLEX128 = 15,    // complex with float64 real and imaginary components

	// Non-IEEE floating-point format based on IEEE754 single-precision
	// floating-point number truncated to 16 bits.
	// This format has 1 sign bit, 8 exponent bits, and 7 mantissa bits.
	BFLOAT16 = 16

	// Future extensions go here.
};

UENUM()
enum class ETensorProtoDataLocation
{
	DEFAULT = 0,
	EXTERNAL = 1
};



/* Structs
 *****************************************************************************/

/**
 * Level 7 - FTensorProtoSegment
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTensorProtoSegment
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int64 Begin;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int64 End;

	FTensorProtoSegment();

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 7 - FStringStringEntryProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * StringStringEntryProto follows the pattern for cross-proto-version maps.
 * See https://developers.google.com/protocol-buffers/docs/proto3#maps
 */
USTRUCT()
struct MODELPROTO_API FStringStringEntryProto
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Key;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Value;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 7 - FTensorShapeProtoDimension
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTensorShapeProtoDimension
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int64 DimValue;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString DimParam;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Denotation;

	FTensorShapeProtoDimension();

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 6 - FTensorShapeProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Defines a tensor shape. A dimension can be either an integer value
 * or a symbolic variable. A symbolic variable represents an unknown
 * dimension.
 */
USTRUCT()
struct MODELPROTO_API FTensorShapeProto
{
	GENERATED_BODY()

public:
	/**
	 * A dimension can be either an integer value or a symbolic
	 * variable. A symbolic variable represents an unknown dimension.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FTensorShapeProtoDimension> Dim;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 6 - FTensorProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Tensors
 * A serialized tensor value.
 */
USTRUCT()
struct MODELPROTO_API FTensorProto
{
	GENERATED_BODY()

public:
	/**
	 * The shape of the tensor.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int64> Dimensions;

	/**
	 * The data type of the tensor.
	 * This field MUST have a valid TensorProto.DataType value
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ETensorProtoDataType DataType;

	/**
	 * For very large tensors, we may want to store them in chunks, in which
	 * case the following fields will specify the segment that is stored in
	 * the current TensorProto.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTensorProtoSegment Segment;

	UPROPERTY()
	TArray<float> FloatData;
	UPROPERTY()
	TArray<int32> Int32Data;
	UPROPERTY()
	TArray<FString> StringData;
	UPROPERTY()
	TArray<int64> Int64Data;

	/**
	 * Optionally, a name for the tensor.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	/**
	 * A human-readable documentation for this tensor. Markdown is allowed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString DocString;


	UPROPERTY()
	TArray<uint8> RawData;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FStringStringEntryProto> ExternalData;

	/**
	 * If value not set, data is stored in raw_data (if set) otherwise in type-specified field.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	ETensorProtoDataLocation DataLocation;
	UPROPERTY()
	TArray<double> DoubleData;
	UPROPERTY()
	TArray<uint64> UInt64Data;

	FTensorProto();

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;

private:
	static ETensorProtoDataType StringToDataType(const FString& InString);

	static ETensorProtoDataLocation StringToDataLocation(const FString& InString);
};

/**
 * Level 5 - FTypeProtoSequence
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTypeProtoSequence
{
	GENERATED_BODY()
};

/**
 * Level 5 - FTypeProtoTensor
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTypeProtoTensor
{
	GENERATED_BODY()

public:
	/**
	 * This field MUST NOT have the value of UNDEFINED
	 * This field MUST have a valid TensorProto.DataType value
	 * This field MUST be present for this version of the IR.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int32 ElemType;

	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTensorShapeProto Shape;

	FTypeProtoTensor();

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 5 - FTypeProtoMap
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTypeProtoMap
{
	GENERATED_BODY()

public:
	/**
	 * This field MUST have a valid TensorProto.DataType value 
	 * This field MUST be present for this version of the IR. 
	 * This field MUST refer to an integral type ([U]INT{8|16|32|64}) or STRING
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int32 KeyType;

	/**
	 * This field MUST be present for this version of the IR.
	 */
	//UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	// FTypeProto ValueType; // Not implemented yet because of Circular-dependency

	FTypeProtoMap();
};

/**
 * Level 5 - FSparseTensorProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FSparseTensorProto
{
	GENERATED_BODY()

public:
	/**
	 * The sequence of non-default values are encoded as a tensor of shape [NNZ].
	 * The default-value is zero for numeric tensors, and empty-string for string tensors.
	 * values must have a non-empty name present which serves as a name for SparseTensorProto
	 * when used in sparse_initializer list.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTensorProto Values;

	/**
	 * The indices of the non-default values, which may be stored in one of two formats.
	 * (a) Indices can be a tensor of shape [NNZ, rank] with the [i,j]-th value
	 * corresponding to the j-th index of the i-th value (in the values tensor).
	 * (b) Indices can be a tensor of shape [NNZ], in which case the i-th value
	 * must be the linearized-index of the i-th value (in the values tensor).
	 * The linearized-index can be converted into an index tuple (k_1,...,k_rank)
	 * using the shape provided below.
	 * The indices must appear in ascending order without duplication.
	 * In the first format, the ordering is lexicographic-ordering:
	 * e.g., index-value [1,4] must appear before [2,1]
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTensorProto Indices;

	/**
	 * The shape of the underlying dense-tensor: [dim_1, dim_2, ... dim_rank]
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int64> Dimensions;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 4 - FTypeProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Types
 * The standard ONNX data types.
 *
 * NOTE:  DNN-only implementations of ONNX MAY elect to not support non-tensor values
 * as input and output to graphs and nodes. These types are needed to naturally
 * support classical ML operators. DNN operators SHOULD restrict their input
 * and output types to tensors.
 */
USTRUCT()
struct MODELPROTO_API FTypeProto
{
	GENERATED_BODY()

public:
	/**
	 * The type of a tensor.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTypeProtoTensor TensorType;

	/**
	 * An optional denotation can be used to denote the whole
	 * type with a standard semantic description as to what is
	 * stored inside. Refer to https://github.com/onnx/onnx/blob/master/docs/TypeDenotation.md#type-denotation-definition
	 * for pre-defined type denotations.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Denotation;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 4 - FAttributeProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FAttributeProto
{
	GENERATED_BODY()

public:
	/**
	 * The name field MUST be present for this version of the IR.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	/**
	 * A human-readable documentation for this attribute. Markdown is allowed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString DocString;

	/**
	 * The type field MUST be present for this version of the IR.
	 * For 0.0.1 versions of the IR, this field was not defined, and
	 * implementations needed to use has_field heuristics to determine
	 * which value field was in use.  For IR_VERSION 0.0.2 or later, this
	 * field MUST be set and match the f|i|s|t|... field in use.  This
	 * change was made to accommodate proto3 implementations.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	EAttributeProtoAttributeType Type;

	/**
	 * Exactly ONE of the following fields must be present for this version of the IR
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	float F;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int64 I;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString S;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTensorProto T;
// 	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
// 	TSharedPtr<FGraphProto> G; /** Used TSharedPtr<> to break the circular dependency */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FSparseTensorProto SparseTensor;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<float> Floats;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<int64> Integers;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FString> Strings;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FTensorProto> Tensors;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FSparseTensorProto> SparseTensors;
// 	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
// 	TArray<TSharedPtr<FGraphProto>> Graphs; /** Used TSharedPtr<> to break the circular dependency */


	FAttributeProto();

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;

private:
	static EAttributeProtoAttributeType StringToAttributeType(const FString& InString);
};

/**
 * Level 3 - FValueInfoProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Defines information on value, including the name, the type, and
 * the shape of the value.
 */
USTRUCT()
struct MODELPROTO_API FValueInfoProto
{
	GENERATED_BODY()

public:
	/**
	 *This field MUST be present in this version of the IR.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	/**
	 * This field MUST be present in this version of the IR for
	 * inputs and outputs of the top-level graph.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FTypeProto Type;

	/**
	 * A human-readable documentation for this value. Markdown is allowed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString DocString;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 3 - FTensorAnnotation
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTensorAnnotation
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString TensorName;

	/**
	* <key, value> pairs to annotate tensor specified by <tensor_name> above.
	* The keys used in the mapping below must be pre-defined in ONNX spec.
	* For example, for 8-bit linear quantization case, 'SCALE_TENSOR',
	* 'ZERO_POINT_TENSOR' will be pre-defined as quantization parameter keys.
	*/
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FStringStringEntryProto> QuantParameterTensorNames;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 3 - FNodeProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Nodes
 *
 * Computation graphs are made up of a DAG of nodes, which represent what is
 * commonly called a "layer" or "pipeline stage" in machine learning frameworks.
 *
 * For example, it can be a node of type "Conv" that takes in an image, a filter
 * tensor and a bias tensor, and produces the convolved output.
 */
USTRUCT()
struct MODELPROTO_API FNodeProto
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FString> Input;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FString> Output;

	/**
	 * An optional identifier for this node in a graph.
	 * This field MAY be absent in this version of the IR.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	/**
	 * The symbolic identifier of the Operator to execute.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString OperatorType;

	/**
	 * The domain of the OperatorSet that specifies the operator named by op_type.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Domain;

	/**
	 * Additional named attributes.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FAttributeProto> Attribute;

	/**
	 * A human-readable documentation for this node. Markdown is allowed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString DocString;

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 2 - FGraphProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Graphs
 *
 * A graph defines the computational logic of a model and is comprised of a parameterized
 * list of nodes that form a directed acyclic graph based on their inputs and outputs.
 * This is the equivalent of the "network" or "graph" in many deep learning / neural network
 * frameworks.
 */
USTRUCT()
struct MODELPROTO_API FGraphProto
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	bool bIsLoaded;

	/**
	 * The nodes in the graph, sorted topologically.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FNodeProto> Node;

	/**
	 * The name of the graph.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Name;

	/**
	 * A list of named tensor values, used to specify constant inputs of the graph.
	 * Each TensorProto entry must have a distinct name (within the list) that
	 * MAY also appear in the input list.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FTensorProto> Initializer;

	/**
	 * Initializers (see above) stored in sparse format.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FSparseTensorProto> SparseInitializer;

	/**
	 * A human-readable documentation for this graph. Markdown is allowed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString DocString;

	/**
	 * The inputs and outputs of the graph.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FValueInfoProto> Input;
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FValueInfoProto> Output;

	/**
	 * Information for the values in the graph. The ValueInfoProto.name's
	 * must be distinct. It is optional for a value to appear in value_info list.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FValueInfoProto> ValueInfo;

	/**
	 * This field carries information to indicate the mapping among a tensor and its
	 * quantization parameter tensors. For example:
	 * For tensor 'a', it may have {'SCALE_TENSOR', 'a_scale'} and {'ZERO_POINT_TENSOR', 'a_zero_point'} annotated,
	 * which means, tensor 'a_scale' and tensor 'a_zero_point' are scale and zero point of tensor 'a' in the model.
	 */
	TArray<FTensorAnnotation> QuantizationAnnotation;

	FGraphProto();

	FORCEINLINE bool IsLoaded() { return bIsLoaded; }

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 1 - FTrainingInfoProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 */
USTRUCT()
struct MODELPROTO_API FTrainingInfoProto
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	bool bIsLoaded;

	/**
	 * This field describes a graph to compute the initial tensors
	 * upon starting the training process. Initialization graph has no input
	 * and can have multiple outputs. Usually, trainable tensors in neural 
	 * networks are randomly initialized. To achieve that, for each tensor,
     * the user can put a random number operator such as RandomNormal or
	 * RandomUniform in TrainingInfoProto.initialization.node and assign its
	 * random output to the specific tensor using "initialization_binding".
	 * This graph can also set the initializers in "algorithm" in the same
	 * TrainingInfoProto; a use case is resetting the number of training
	 * iteration to zero.
	 * 
	 * By default, this field is an empty graph and its evaluation does not 
	 * produce any output. Thus, no initializer would be changed by default.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FGraphProto Initialization;

	/**
	 * This field represents a training algorithm step. Given required inputs,
	 * it computes outputs to update initializers in its own or inference graph's 
	 * initializer lists. In general, this field contains loss node, gradient node,
	 * optimizer node, increment of iteration count.
	 * 
	 * An execution of the training algorithm step is performed by executing the
	 * graph obtained by combining the inference graph (namely "ModelProto.graph")
	 * and the "algorithm" graph. That is, the actual the actual
	 * input/initializer/output/node/value_info/sparse_initializer list of
	 * the training graph is the concatenation of
	 * "ModelProto.graph.input/initializer/output/node/value_info/sparse_initializer"
	 * and "algorithm.input/initializer/output/node/value_info/sparse_initializer"
	 * in that order. This combined graph must satisfy the normal ONNX conditions.
	 * Now, let's provide a visualization of graph combination for clarity.
	 * Let the inference graph (i.e., "ModelProto.graph") be
	 *    tensor_a, tensor_b -> MatMul -> tensor_c -> Sigmoid -> tensor_d
	 * and the "algorithm" graph be
	 *    tensor_d -> Add -> tensor_e
	 * The combination process results 
	 *    tensor_a, tensor_b -> MatMul -> tensor_c -> Sigmoid -> tensor_d -> Add -> tensor_e
	 * 
	 * Notice that an input of a node in the "algorithm" graph may reference the
	 * output of a node in the inference graph (but not the other way round). Also, inference
	 * node cannot reference inputs of "algorithm". With these restrictions, inference graph
	 * can always be run independently without training information.
	 * 
	 * By default, this field is an empty graph and its evaluation does not
	 * produce any output. Evaluating the default training step never
	 * update any initializers.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FGraphProto Algorithm;

	/**
	 * This field specifies the bindings from the outputs of "initialization" to
	 * some initializers in "ModelProto.graph.initializer" and
	 * some initializers in "ModelProto.graph.initializer" and
	 * the "algorithm.initializer" in the same TrainingInfoProto.
	 * See "update_binding" below for details.
	 * 
	 * By default, this field is empty and no initializer would be changed
	 * by the execution of "initialization".
	*/
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FStringStringEntryProto> InitializationBinding;

	/**
	 * Gradient-based training is usually an iterative procedure. In one gradient
	 * descent iteration, we apply
	 * 
	 * x = x - r * g
	 * 
	 * where "x" is the optimized tensor, "r" stands for learning rate, and "g" is
	 * gradient of "x" with respect to a chosen loss. To avoid adding assignments
	 * into the training graph, we split the update equation into
	 * 
	 * y = x - r * g
	 * x = y
	 * 
	 * The user needs to save "y = x - r * g" into TrainingInfoProto.algorithm. To
	 * tell that "y" should be assigned to "x", the field "update_binding" may
	 * contain a key-value pair of strings, "x" (key of StringStringEntryProto)
	 * and "y" (value of StringStringEntryProto).
	 * For a neural network with multiple trainable (mutable) tensors, there can
	 * be multiple key-value pairs in "update_binding".
	 * 
	 * The initializers appears as keys in "update_binding" are considered
	 * mutable variables. This implies some behaviors
	 * as described below.
	 * 
	 * 1. We have only unique keys in all "update_binding"s so that two
	 *	  variables may not have the same name. This ensures that one
	 *	  variable is assigned up to once.
	 * 2. The keys must appear in names of "ModelProto.graph.initializer" or
	 *    "TrainingInfoProto.algorithm.initializer".
	 * 3. The values must be output names of "algorithm" or "ModelProto.graph.output".
	 * 4. Mutable variables are initialized to the value specified by the
	 *    corresponding initializer, and then potentially updated by
	 *    "initializer_binding"s and "update_binding"s in "TrainingInfoProto"s.
	 * 
	 * This field usually contains names of trainable tensors
	 * (in ModelProto.graph), optimizer states such as momentums in advanced
	 * stochastic gradient methods (in TrainingInfoProto.graph),
	 * and number of training iterations (in TrainingInfoProto.graph).
	 * 
	 * By default, this field is empty and no initializer would be changed
	 * by the execution of "algorithm".
	*/
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FStringStringEntryProto> UpdateBinding;

	FTrainingInfoProto();

	FORCEINLINE bool IsLoaded() { return bIsLoaded; }

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 1 - FOperatorSetIdProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Operator Sets
 * OperatorSets are uniquely identified by a (domain, opset_version) pair.
 */
USTRUCT()
struct MODELPROTO_API FOperatorSetIdProto
{
	GENERATED_BODY()

public:
	/**
	 * The domain of the operator set being identified.
	 * The empty string ("") or absence of this field implies the operator
	 * set that is defined as part of the ONNX specification.
	 * This field MUST be present in this version of the IR when referring to any other operator set.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	FString Domain;

	/**
	 * The version of the operator set being identified.
	 * This field MUST be present in this version of the IR.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	int64 Version;

	FOperatorSetIdProto();

	bool LoadFromString(const FString& InProtoString, const int32 InLevel);

	FString ToString(const FString& InLineStarted = TEXT("")) const;
};

/**
 * Level 0 - FModelProto
 * It follows the ONNX standard: https://github.com/onnx/onnx/blob/master/onnx/onnx.proto3
 *
 * Models
 *
 * ModelProto is a top-level file/container format for bundling a ML model and
 * associating its computation graph with metadata.
 *
 * The semantics of the model are described by the associated GraphProto's.
 */
USTRUCT()
struct MODELPROTO_API FModelProto
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	bool bIsLoaded;

	/**
	 * The version of the IR this model targets. See Version enum above.
	 * This field MUST be present.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	int64 IRVersion;

	/**
	 * The OperatorSets this model relies on.
	 * All ModelProtos MUST have at least one entry that
	 * specifies which version of the ONNX OperatorSet is
	 * being imported.
	 *
	 * All nodes in the ModelProto's graph will bind against the operator
	 * with the same-domain/same-op_type operator with the HIGHEST version
	 * in the referenced operator sets.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FOperatorSetIdProto> OpsetImport;

	/**
	 * The name of the framework or tool used to generate this model.
	 * This field SHOULD be present to indicate which implementation/tool/framework
	 * emitted the model.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FString ProducerName;

	/**
	 * The version of the framework or tool used to generate this model.
	 * This field SHOULD be present to indicate which implementation/tool/framework
	 * emitted the model.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FString ProducerVersion;

	/**
	 * Domain name of the model.
     * We use reverse domain names as name space indicators. For example:
     * `com.facebook.fair` or `com.microsoft.cognitiveservices`
     *
     * Together with `model_version` and GraphProto.name, this forms the unique identity of
     * the graph.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FString Domain;

	/**
	 * The version of the graph encoded. See Version enum below.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	int64 ModelVersion;

	/**
	 * A human-readable documentation for this model. Markdown is allowed.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FString DocString;

	/**
	 * The parameterized graph that is evaluated to execute the model.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference");
	FGraphProto Graph;

	/**
	 * Named metadata values; keys should be distinct.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FStringStringEntryProto> MetadataProps;

	/**
	* Training-specific information. Sequentially executing all stored
	 * `TrainingInfoProto.algorithm`s and assigning their outputs following
	 * the corresponding `TrainingInfoProto.update_binding`s is one training
	 * iteration. Similarly, to initialize the model (as if training hasn't
	 * happened), the user should sequentially execute all stored
	 * `TrainingInfoProto.initialization`s and assigns their outputs using
	 * `TrainingInfoProto.initialization_binding`s.
	 * If this field is empty, the training behavior of the model is undefined.
	 */
	UPROPERTY(VisibleAnywhere, Category = "Neural Network Inference")
	TArray<FTrainingInfoProto> TrainingInfo;

	FModelProto();

	FORCEINLINE bool IsLoaded() { return bIsLoaded; }

	bool LoadFromString(const FString& InProtoString, const int32 InLevel = 0);

	FString ToString(const FString& InLineStarted = TEXT("")) const;

	const FGraphProto& GetGraph() const;

	/**
	 * Given the TArray<T>, it will find and return the reference to the Element named InName.
	 */
	template <typename T>
	static const T* FindElementInArray(const FString& InName, const TArray<T>& InArray, const bool bInMustValueBeFound);
};



/* ModelProto template functions
 *****************************************************************************/

template <typename T>
const T* FModelProto::FindElementInArray(const FString& InName, const TArray<T>& InArray, const bool bInMustValueBeFound)
{
	// Iterating over each element name in InArray
	for (const T& Element : InArray)
	{
		if (Element.Name == InName)
		{
			return &Element;
		}
	}

	// Trigger assertion if tensor name does not exist and return
	checkf(!bInMustValueBeFound, TEXT("Element with the name %s could not be found in InArray."), *InName);
	return nullptr;
}
