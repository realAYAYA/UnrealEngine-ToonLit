// Copyright Epic Games, Inc. All Rights Reserved.

#include "NeuralMorphNetwork.h"
#include "NeuralMorphModel.h"
#include "UObject/SoftObjectPath.h"
#include "HAL/FileManagerGeneric.h"
#include "Math/UnrealMathUtility.h"

#if NEURALMORPHMODEL_USE_ISPC
	#include "NeuralMorphNetwork.ispc.generated.h"
#endif

//--------------------------------------------------------------------------
// UNeuralMorphMLP
//--------------------------------------------------------------------------
void UNeuralMorphMLP::Empty()
{
	Layers.Empty();
}

bool UNeuralMorphMLP::IsEmpty() const
{
	return GetNumLayers() == 0;
}

int32 UNeuralMorphMLP::GetNumInputs() const
{
	return !Layers.IsEmpty() ? Layers[0]->NumInputs * Layers[0]->Depth : 0;
}

int32 UNeuralMorphMLP::GetNumOutputs() const
{
	return !Layers.IsEmpty() ? Layers[Layers.Num() - 1]->NumOutputs * Layers[Layers.Num() - 1]->Depth : 0;
}

int32 UNeuralMorphMLP::GetMaxNumLayerInputs() const
{
	int32 MaxValue = 0;
	for (UNeuralMorphMLPLayer* Layer : Layers)
	{
		MaxValue = FMath::Max<int32>(MaxValue, Layer->NumInputs * Layer->Depth);
	}
	return MaxValue;
}

int32 UNeuralMorphMLP::GetMaxNumLayerOutputs() const
{
	int32 MaxValue = 0;
	for (UNeuralMorphMLPLayer* Layer : Layers)
	{
		MaxValue = FMath::Max<int32>(MaxValue, Layer->NumOutputs * Layer->Depth);
	}
	return MaxValue;
}

UNeuralMorphMLPLayer* UNeuralMorphMLP::LoadNetworkLayer(FArchive& FileReader)
{
	UNeuralMorphMLPLayer* Layer = NewObject<UNeuralMorphMLPLayer>(this);

	struct FLayerHeader
	{
		int32 NumInputs;
		int32 NumOutputs;
		int32 NumWeights;
		int32 NumBiases;
	};

	FLayerHeader LayerHeader;
	FileReader.Serialize(&LayerHeader, sizeof(FLayerHeader));
	if (FileReader.IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load a layer header for the network!"));
		Layer->ConditionalBeginDestroy();
		return nullptr;
	}

	Layer->NumInputs = LayerHeader.NumInputs;
	Layer->NumOutputs = LayerHeader.NumOutputs;
	check(LayerHeader.NumInputs * LayerHeader.NumOutputs > 0);
	Layer->Depth = LayerHeader.NumWeights / (LayerHeader.NumInputs * LayerHeader.NumOutputs);

	Layer->Weights.AddZeroed(LayerHeader.NumWeights);
	FileReader.Serialize(Layer->Weights.GetData(), LayerHeader.NumWeights * sizeof(float));
	if (FileReader.IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load layer weights of the network!"));
		Layer->ConditionalBeginDestroy();
		return nullptr;
	}

	Layer->Biases.AddZeroed(LayerHeader.NumBiases);
	FileReader.Serialize(Layer->Biases.GetData(), LayerHeader.NumBiases * sizeof(float));
	if (FileReader.IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load layer biases of the network!"));
		Layer->ConditionalBeginDestroy();
		return nullptr;
	}

	return Layer;
}


bool UNeuralMorphMLP::Load(FArchive& FileReader)
{
	Empty();

	// Read the FOURCC, to identify the file type.
	char FOURCC[4] {' ', ' ', ' ', ' '};
	FileReader.Serialize(FOURCC, 4);
	if (FileReader.IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to read the FOURCC!"));
		Empty();
		return false;
	}

	if (FOURCC[0] != 'N' || FOURCC[1] != 'M' || FOURCC[2] != 'L' || FOURCC[3] != 'P')	// NMLP (Neural MLP)
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("The file is not a valid valid neural morph MLP file type!"));
		Empty();
		return false;
	}

	// Load the version number.
	int32 Version = -1;
	FileReader.Serialize(&Version, sizeof(int32));
	if (FileReader.IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load version"));
		Empty();
		return false;
	}

	if (Version != 1)
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("The Neural Morph MLP file '%s' is of an unknown version (Version=%d)!"), *FileReader.GetArchiveName(), Version);
		Empty();
		return false;
	}

	int32 NumLayers = -1;
	FileReader.Serialize(&NumLayers, sizeof(int32));
	if (FileReader.IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load number of layers"));
		Empty();
		return false;
	}

	// Load the weights and biases of the main network.
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		UNeuralMorphMLPLayer* Layer = LoadNetworkLayer(FileReader);
		if (Layer == nullptr)
		{
			Empty();
			return false;
		}
		Layers.Add(Layer);
		UE_LOG(LogNeuralMorphModel, Verbose, TEXT("MLP Layer %d --> NumWeights=%d (%dx%dx%d)   NumBiases=%d"), LayerIndex, Layer->Weights.Num(), Layer->NumInputs, Layer->NumOutputs, Layer->Depth, Layer->Biases.Num());
	}

	UE_LOG(LogNeuralMorphModel, Display, TEXT("Successfullly loaded neural morph MLP from file '%s'"), *FileReader.GetArchiveName());
	return true;
}

int32 UNeuralMorphMLP::GetNumLayers() const
{
	return Layers.Num();
}

UNeuralMorphMLPLayer& UNeuralMorphMLP::GetLayer(int32 Index) const
{
	return *Layers[Index].Get();
}

//--------------------------------------------------------------------------
// UNeuralMorphNetwork
//--------------------------------------------------------------------------
void UNeuralMorphNetwork::Empty()
{
	InputMeans.Empty();
	InputStd.Empty();

	if (MainMLP)
	{
		MainMLP->ConditionalBeginDestroy();
		MainMLP = nullptr;
	}

	if (GroupMLP)
	{
		GroupMLP->ConditionalBeginDestroy();
		GroupMLP = nullptr;
	}

	Mode = ENeuralMorphMode::Global;
	NumMorphsPerBone = 0;
	NumBones = 0;
	NumCurves = 0;
	NumGroups = 0;
	NumItemsPerGroup = 0;
	NumFloatsPerCurve = 1;
}

bool UNeuralMorphNetwork::IsEmpty() const
{
	return !MainMLP;
}

UNeuralMorphMLP* UNeuralMorphNetwork::LoadMLP(FArchive& FileReader)
{
	UNeuralMorphMLP* MLP = NewObject<UNeuralMorphMLP>(this);
	if (!MLP->Load(FileReader))
	{
		MLP->ConditionalBeginDestroy();
		MLP = nullptr;		
	}
	return MLP;
}

bool UNeuralMorphNetwork::Load(const FString& Filename)
{
	UE_LOG(LogNeuralMorphModel, Display, TEXT("Loading Neural Morph Network from file '%s'"), *Filename);
	Empty();

	// Create the file reader.
	TUniquePtr<FArchive> FileReader(IFileManager::Get().CreateFileReader(*Filename));
	if (!FileReader.IsValid())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to create file reader to read file '%s'!"), *Filename);
		Empty();
		return false;
	}

	// Read the FOURCC, to identify the file type.
	char FOURCC[4] {' ', ' ', ' ', ' '};
	FileReader->Serialize(FOURCC, 4);
	if (FileReader->IsError())
	{
		FileReader->Close();
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to read the FOURCC!"));
		Empty();
		return false;
	}

	if (FOURCC[0] != 'N' || FOURCC[1] != 'M' || FOURCC[2] != 'M' || FOURCC[3] != 'N')	// NMMN (Neural Morph Model Network)
	{
		FileReader->Close();
		UE_LOG(LogNeuralMorphModel, Error, TEXT("The file is not a valid valid neural morph network file type!"));
		Empty();
		return false;
	}

	// Load the version number.
	int32 Version = -1;
	FileReader->Serialize(&Version, sizeof(int32));
	if (FileReader->IsError())
	{
		FileReader->Close();
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load version"));
		Empty();
		return false;
	}

	if (Version != 3)
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("The Neural Morph Network file '%s' is of an unknown version (Version=%d)!"), *Filename, Version);
		FileReader->Close();
		Empty();
		return false;
	}

	// Load the header with info.
	struct FInfoHeader
	{
		int32 Mode;						// 0=Local, 1=Global
		int32 NumInputs;				// How many float inputs in the input layer?
		int32 NumHiddenLayers;			// How many hidden layers in the network (layers excluding input and output layer)?
		int32 NumUnitsPerHiddenLayer;	// How many neurons for each hidden layer?
		int32 NumOutputs;				// The number of units in the output layer.
		int32 NumMorphsPerBone;			// The number of morph targets per bone, if set Mode == 0. Otherwise ignored.
		int32 NumBones;					// The number of bones used as input.
		int32 NumCurves;				// The number of curves used as input.
		int32 NumFloatsPerCurve;		// The number of floats per curve.
		int32 NumGroups;				// How many groups are there?
		int32 NumItemsPerGroup;			// How many items (bones/curves) per group?
	};

	FInfoHeader Info;
	FileReader->Serialize(&Info, sizeof(FInfoHeader));
	if (FileReader->IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load info header!"));
		FileReader->Close();
		Empty();
		return false;
	}

	Mode = (Info.Mode == 0) ? ENeuralMorphMode::Local : ENeuralMorphMode::Global;
	NumMorphsPerBone = Info.NumMorphsPerBone;
	NumBones = Info.NumBones;
	NumCurves = Info.NumCurves;
	NumFloatsPerCurve = Info.NumFloatsPerCurve;
	NumGroups = Info.NumGroups;
	NumItemsPerGroup = Info.NumItemsPerGroup;

	// Read the input standard deviation and means.
	InputMeans.SetNumZeroed(Info.NumInputs);
	InputStd.SetNumZeroed(Info.NumInputs);
	FileReader->Serialize(InputStd.GetData(), Info.NumInputs * sizeof(float));
	if (FileReader->IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load input standard deviations!"));
		FileReader->Close();
		Empty();
		return false;
	}

	FileReader->Serialize(InputMeans.GetData(), Info.NumInputs * sizeof(float));
	if (FileReader->IsError())
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load input means!"));
		FileReader->Close();
		Empty();
		return false;
	}

	// Load the main network.
	MainMLP = LoadMLP(*FileReader);
	if (!MainMLP)
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load main MLP!"));
		FileReader->Close();
		Empty();
		return false;
	}

	// If we're in local mode and we have groups, load the network for that.
	if (NumGroups > 0 && Mode == ENeuralMorphMode::Local)
	{
		GroupMLP = LoadMLP(*FileReader);
		if (!GroupMLP)
		{
			UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to load group MLP!"));
			FileReader->Close();
			Empty();
			return false;
		}
	}

	// Clean up and return the result.
	const bool bSuccess = FileReader->Close();
	if (bSuccess)
	{
		UE_LOG(LogNeuralMorphModel, Display, TEXT("Successfullly loaded neural morph network from file '%s'"), *FileReader->GetArchiveName());
	}
	else
	{
		UE_LOG(LogNeuralMorphModel, Error, TEXT("Failed to close file reader."));
		Empty();
	}

	return bSuccess;
}

UNeuralMorphNetworkInstance* UNeuralMorphNetwork::CreateInstance()
{
	UNeuralMorphNetworkInstance* Instance = NewObject<UNeuralMorphNetworkInstance>(this);
	Instance->Init(this);
	return Instance;
}

int32 UNeuralMorphNetwork::GetNumBones() const
{
	return NumBones;
}

int32 UNeuralMorphNetwork::GetNumCurves() const
{
	return NumCurves;
}

int32 UNeuralMorphNetwork::GetNumMorphsPerBone() const
{
	return NumMorphsPerBone;
}

ENeuralMorphMode UNeuralMorphNetwork::GetMode() const
{
	return Mode;
}

const TArrayView<const float> UNeuralMorphNetwork::GetInputMeans() const
{
	return InputMeans;
}

const TArrayView<const float> UNeuralMorphNetwork::GetInputStds() const
{
	return InputStd;
}

int32 UNeuralMorphNetwork::GetNumFloatsPerCurve() const
{
	return NumFloatsPerCurve;
}

int32 UNeuralMorphNetwork::GetNumGroups() const
{
	return NumGroups;
}

int32 UNeuralMorphNetwork::GetNumItemsPerGroup() const
{
	return NumItemsPerGroup;
}

int32 UNeuralMorphNetwork::GetNumInputs() const
{
	return MainMLP ? MainMLP->GetNumInputs() : 0;
}

int32 UNeuralMorphNetwork::GetNumOutputs() const
{
	int32 NumOutputs = MainMLP ? MainMLP->GetNumOutputs() : 0;
	NumOutputs += GroupMLP ? GroupMLP->GetNumOutputs() : 0; 
	return NumOutputs;
}

UNeuralMorphMLP* UNeuralMorphNetwork::GetMainMLP() const
{
	return MainMLP.Get();
}

UNeuralMorphMLP* UNeuralMorphNetwork::GetGroupMLP() const
{
	return GroupMLP.Get();
}


//--------------------------------------------------------------------------
// UNeuralMorphNetworkInstance
//--------------------------------------------------------------------------

TArrayView<float> UNeuralMorphNetworkInstance::GetInputs()
{ 
	return TArrayView<float>(Inputs.GetData(), Inputs.Num());
}

TArrayView<const float> UNeuralMorphNetworkInstance::GetInputs() const
{ 
	return TArrayView<const float>(Inputs.GetData(), Inputs.Num());
}

TArrayView<float> UNeuralMorphNetworkInstance::GetGroupInputs()
{ 
	return TArrayView<float>(GroupInputs.GetData(), GroupInputs.Num());
}

TArrayView<const float> UNeuralMorphNetworkInstance::GetGroupInputs() const
{ 
	return TArrayView<const float>(GroupInputs.GetData(), GroupInputs.Num());
}

TArrayView<float> UNeuralMorphNetworkInstance::GetOutputs()
{ 
	return TArrayView<float>(Outputs.GetData(), Outputs.Num());
}

TArrayView<const float> UNeuralMorphNetworkInstance::GetOutputs() const
{ 
	return TArrayView<const float>(Outputs.GetData(), Outputs.Num());
}

const UNeuralMorphNetwork& UNeuralMorphNetworkInstance::GetNeuralNetwork() const
{ 
	return *Network.Get();
}

void UNeuralMorphNetworkInstance::Init(UNeuralMorphNetwork* InNeuralNetwork)
{
	Network = InNeuralNetwork;
	const UNeuralMorphMLP* MainMLP = Network->GetMainMLP();
	const UNeuralMorphMLP* GroupMLP = Network->GetGroupMLP();

	Inputs.SetNumZeroed(Network->GetNumInputs());
	Outputs.SetNumZeroed(Network->GetNumOutputs());	// This contains concatenated outputs of both main and group MLP's.

	if (GroupMLP)
	{
		GroupInputs.SetNumZeroed(GroupMLP->GetNumInputs());
	}

	// Find the largest layer unit size and pre-allocate a buffer of that size.
	const int32 MainMaxNumInputs = MainMLP ? MainMLP->GetMaxNumLayerInputs() : 0;
	const int32 MainMaxNumOutputs = MainMLP ? MainMLP->GetMaxNumLayerOutputs() : 0;
	const int32 GroupMaxNumInputs = GroupMLP ? GroupMLP->GetMaxNumLayerInputs() : 0;
	const int32 GroupMaxNumOutputs = GroupMLP ? GroupMLP->GetMaxNumLayerOutputs() : 0;

	// Find the maximum of the inputs and outputs of both main and group mlp's.
	int32 MaxNumUnits = FMath::Max<int32>(MainMaxNumInputs, GroupMaxNumInputs);
	MaxNumUnits = FMath::Max3<int32>(MaxNumUnits, MainMaxNumOutputs, GroupMaxNumOutputs);

	TempInputArray.SetNumZeroed(MaxNumUnits);
	TempOutputArray.SetNumZeroed(MaxNumUnits);
}

void UNeuralMorphNetworkInstance::RunGlobalModel(const FRunSettings& RunSettings)
{
	float* RESTRICT TempInputBuffer = RunSettings.TempInputBuffer;
	float* RESTRICT TempOutputBuffer = RunSettings.TempOutputBuffer;

	checkfSlow(Network->GetNumFloatsPerCurve() == 1, TEXT("Expecting the number of floats per curve to be 1 in global mode."));

	const UNeuralMorphMLP& MainMLP = *Network->GetMainMLP();

	const int32 NumLayers = MainMLP.GetNumLayers();
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const UNeuralMorphMLPLayer& CurLayer = MainMLP.GetLayer(LayerIndex);
		const int32 NumLayerInputs = CurLayer.NumInputs;
		const int32 NumLayerOutputs = CurLayer.NumOutputs;

		// Copy the inputs to the temp input buffer for the first layer.
		if (LayerIndex == 0)
		{
			const float* const RESTRICT NetworkInputs = RunSettings.InputBuffer;
			for (int32 Index = 0; Index < NumLayerInputs; ++Index)
			{
				TempInputBuffer[Index] = NetworkInputs[Index];
			}
		}

		// If we reached the last layer, output to the final buffer.
		if (LayerIndex == NumLayers - 1)
		{
			TempOutputBuffer = RunSettings.OutputBuffer;
		}

		#if NEURALMORPHMODEL_USE_ISPC
			const float* const RESTRICT LayerWeights = CurLayer.Weights.GetData();
			const float* const RESTRICT LayerBiases = CurLayer.Biases.GetData();
			ispc::MorphNeuralNetwork_LayerForward(TempOutputBuffer, TempInputBuffer, LayerWeights, LayerBiases, NumLayerInputs, NumLayerOutputs);
		#else
			// Init the outputs to the bias.
			const float* const RESTRICT LayerBiases = CurLayer.Biases.GetData();
			for (int32 Index = 0; Index < NumLayerOutputs; ++Index)
			{
				TempOutputBuffer[Index] = LayerBiases[Index];
			}

			// Multiply layer inputs with the weights.
			const float* const RESTRICT LayerWeights = CurLayer.Weights.GetData();
			for (int32 InputIndex = 0; InputIndex < NumLayerInputs; ++InputIndex)
			{
				const float InputValue = TempInputBuffer[InputIndex];
				const int32 Offset = InputIndex * NumLayerOutputs;
				for (int32 OutputIndex = 0; OutputIndex < NumLayerOutputs; ++OutputIndex)
				{
					TempOutputBuffer[OutputIndex] += InputValue * LayerWeights[Offset + OutputIndex];
				}
			}

			// Apply ELU activation.
			for (int32 Index = 0; Index < NumLayerOutputs; ++Index)
			{
				const float X = TempOutputBuffer[Index];
				TempOutputBuffer[Index] = (X > 0.0f) ? X : FMath::InvExpApprox(-X) - 1.0f;
			}
		#endif

		// The outputs are now input to the next layer.
		if (LayerIndex < NumLayers - 1)
		{
			// Swap the inputs and outputs, as the outputs are the input to the next layer.
			float* const SwapTemp = TempInputBuffer;
			TempInputBuffer = TempOutputBuffer;
			TempOutputBuffer = SwapTemp;
		}
	}
}

void UNeuralMorphNetworkInstance::RunLocalMLP(UNeuralMorphMLP& MLP, const FRunSettings& RunSettings)
{
	float* RESTRICT TempInputBuffer = RunSettings.TempInputBuffer;
	float* RESTRICT TempOutputBuffer = RunSettings.TempOutputBuffer;

	checkfSlow(Network->GetNumFloatsPerCurve() == 6, TEXT("Expecting num floats per curve to be 6 in local mode."));

	const int32 NumLayers = MLP.GetNumLayers();
	for (int32 LayerIndex = 0; LayerIndex < NumLayers; ++LayerIndex)
	{
		const UNeuralMorphMLPLayer& CurLayer = MLP.GetLayer(LayerIndex);
		const int32 NumInputsPerBlock = CurLayer.NumInputs;
		const int32 NumBlocks = CurLayer.Depth;

		// Copy the inputs to the temp buffer.
		if (LayerIndex == 0)
		{
			const float* const RESTRICT NetworkInputs = RunSettings.InputBuffer;
			const int32 NumInputs = CurLayer.NumInputs * CurLayer.Depth;
			for (int32 Index = 0; Index < NumInputs; ++Index)
			{
				TempInputBuffer[Index] = NetworkInputs[Index];
			}
		}

		// If we reached the last layer, output to the final buffer.
		if (LayerIndex == NumLayers - 1)
		{
			TempOutputBuffer = RunSettings.OutputBuffer;
		}

		// Init the output buffer to the bias values.
		const float* const RESTRICT LayerBiases = CurLayer.Biases.GetData();
		const int32 NumOutputsPerBlock = CurLayer.NumOutputs;
		const int32 NumOutputs = CurLayer.NumOutputs * CurLayer.Depth;
		for (int32 Index = 0; Index < NumOutputs; ++Index)
		{
			TempOutputBuffer[Index] = LayerBiases[Index];
		}

		const float* const RESTRICT LayerWeights = CurLayer.Weights.GetData();
		for (int32 BlockIndex = 0; BlockIndex < NumBlocks; ++BlockIndex)
		{
			const int32 BlockOutputOffset = BlockIndex * NumOutputsPerBlock;
			const int32 BlockInputOffset = BlockIndex * NumInputsPerBlock;

			// Multiply layer inputs with the weights.
			const int32 WeightOffset = BlockIndex * (NumInputsPerBlock * NumOutputsPerBlock);
			for (int32 InputIndex = 0; InputIndex < NumInputsPerBlock; ++InputIndex)
			{
				const float InputValue = TempInputBuffer[BlockInputOffset + InputIndex];
				const int32 InputOffset = InputIndex * NumOutputsPerBlock;
				for (int32 OutputIndex = 0; OutputIndex < NumOutputsPerBlock; ++OutputIndex)
				{
					TempOutputBuffer[BlockOutputOffset + OutputIndex] += InputValue * LayerWeights[WeightOffset + InputOffset + OutputIndex];
				}
			}
		} // For all blocks.

		// Apply ELU activation.
		#if NEURALMORPHMODEL_USE_ISPC
			ispc::MorphNeuralNetwork_Activation_ELU(TempOutputBuffer, NumOutputs);
		#else
			for (int32 OutputIndex = 0; OutputIndex < NumOutputs; ++OutputIndex)
			{
				const float X = TempOutputBuffer[OutputIndex];
				TempOutputBuffer[OutputIndex] = (X > 0.0f) ? X : FMath::InvExpApprox(-X) - 1.0f;
			}
		#endif

		// The outputs are now input to the next layer.
		if (LayerIndex < NumLayers - 1)
		{
			// Swap the inputs and outputs, as the outputs are the input to the next layer.
			float* const SwapTemp = TempInputBuffer;
			TempInputBuffer = TempOutputBuffer;
			TempOutputBuffer = SwapTemp;
		}
	} // For all layers.
}

void UNeuralMorphNetworkInstance::Run()
{	
	TRACE_CPUPROFILER_EVENT_SCOPE(UNeuralMorphNetwork::Run)

	// Setup the buffer pointers.
	FRunSettings RunSettings;
	RunSettings.TempInputBuffer  = TempInputArray.GetData();
	RunSettings.TempOutputBuffer = TempOutputArray.GetData();
	RunSettings.InputBuffer		 = Inputs.GetData();
	RunSettings.OutputBuffer	 = Outputs.GetData();

	// Run the network.
	if (Network->GetMode() == ENeuralMorphMode::Global)
	{
		RunGlobalModel(RunSettings);
	}
	else
	{
		checkSlow(Network->GetMode() == ENeuralMorphMode::Local)
		RunLocalMLP(*Network->GetMainMLP(), RunSettings);
		if (Network->GetGroupMLP())
		{
			FRunSettings GroupRunSettings;
			GroupRunSettings.TempInputBuffer	= TempInputArray.GetData();
			GroupRunSettings.TempOutputBuffer	= TempOutputArray.GetData();
			GroupRunSettings.InputBuffer		= GroupInputs.GetData();
			GroupRunSettings.OutputBuffer		= Outputs.GetData() + Network->GetMainMLP()->GetNumOutputs();	// Output values after the main network's outputs.
			RunLocalMLP(*Network->GetGroupMLP(), GroupRunSettings);
		}
	}
}
