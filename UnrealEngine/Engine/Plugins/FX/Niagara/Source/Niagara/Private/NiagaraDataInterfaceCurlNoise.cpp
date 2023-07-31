// Copyright Epic Games, Inc. All Rights Reserved.

#include "NiagaraDataInterfaceCurlNoise.h"
#include "NiagaraShader.h"
#include "NiagaraShaderParametersBuilder.h"
#include "ShaderParameterUtils.h"
#include "Math/IntVector.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(NiagaraDataInterfaceCurlNoise)

static const FName SampleNoiseFieldName(TEXT("SampleNoiseField"));

// ----------------------------------------------------- start noise helpers -----------------------------------------------------
// Fairly straightforward CPU implementation of the equivalent HLSL code found in Engine/Shaders/Private/Random.ush
// Also contains a partial implementation of some math types and functions found in HLSL but not in UE

// TODO(mv): Move this to a separate NiagaraMath unit.  
struct FNiagaraUIntVector
{
	uint32 X;
	uint32 Y;
	uint32 Z;

	FNiagaraUIntVector() : X(0), Y(0), Z(0) {}
	FNiagaraUIntVector(uint32 X, uint32 Y, uint32 Z) : X(X), Y(Y), Z(Z) {}
	FNiagaraUIntVector(uint32 Value) : X(Value), Y(Value), Z(Value) {}
	FNiagaraUIntVector(FIntVector Values) : X(Values.X), Y(Values.Y), Z(Values.Z) {}

	FNiagaraUIntVector operator+(FNiagaraUIntVector Rhs)
	{
		return FNiagaraUIntVector{ X + Rhs.X, Y + Rhs.Y, Z + Rhs.Z };
	}

	FNiagaraUIntVector operator*(FNiagaraUIntVector Rhs)
	{
		return FNiagaraUIntVector{ X * Rhs.X, Y * Rhs.Y, Z * Rhs.Z };
	}

	FNiagaraUIntVector operator>>(uint32 Shift)
	{
		return FNiagaraUIntVector(X >> Shift, Y >> Shift, Z >> Shift);
	}

	FNiagaraUIntVector operator&(FNiagaraUIntVector Rhs)
	{
		return FNiagaraUIntVector{ X & Rhs.X, Y & Rhs.Y, Z & Rhs.Z };
	}

	uint32& operator[](int Index)
	{
		if (Index == 0) return X;
		if (Index == 1) return Y;
		if (Index == 2) return Z;
		check(false); 
		return X;
	}

	const uint32& operator[](int Index) const
	{
		if (Index == 0) return X;
		if (Index == 1) return Y;
		if (Index == 2) return Z;
		check(false);
		return X;
	}
};

FNiagaraUIntVector Rand3DPCG16(FIntVector p)
{
	FNiagaraUIntVector v = FNiagaraUIntVector(p);
	
	v = v * 1664525u + 1013904223u;

	v.X += v.Y*v.Z;
	v.Y += v.Z*v.X;
	v.Z += v.X*v.Y;
	v.X += v.Y*v.Z;
	v.Y += v.Z*v.X;
	v.Z += v.X*v.Y;

	return v >> 16u;
}

FVector NiagaraVectorFrac(FVector v)
{
	return FVector(FMath::Frac(v.X), FMath::Frac(v.Y), FMath::Frac(v.Z));
}

FVector NoiseTileWrap(FVector v, bool bTiling, float RepeatSize)
{
	FVector vv = bTiling ? (NiagaraVectorFrac(v / RepeatSize) * RepeatSize) : v;
	return vv;
}

struct FNiagaraMatrix4x3
{
	FVector Row0;
	FVector Row1;
	FVector Row2;
	FVector Row3;

	FNiagaraMatrix4x3() : Row0(FVector()), Row1(FVector()), Row2(FVector()), Row3(FVector()) {}
	FNiagaraMatrix4x3(FVector Row0, FVector Row1, FVector Row2, FVector Row3) : Row0(Row0), Row1(Row1), Row2(Row2), Row3(Row3) {}

	FVector& operator[](int Row)
	{
		if (Row == 0) return Row0;
		if (Row == 1) return Row1;
		if (Row == 2) return Row2;
		if (Row == 3) return Row3;
		check(false);
		return Row0;
	}
	const FVector& operator[](int Row) const
	{
		if (Row == 0) return Row0;
		if (Row == 1) return Row1;
		if (Row == 2) return Row2;
		if (Row == 3) return Row3;
		check(false);
		return Row0;
	}
};

FVector NiagaraVectorFloor(FVector v) {
	return FVector(FGenericPlatformMath::FloorToFloat(v.X),
		           FGenericPlatformMath::FloorToFloat(v.Y),
		           FGenericPlatformMath::FloorToFloat(v.Z));
};

FVector NiagaraVectorStep(FVector v, FVector u)
{
	return FVector(u.X >= v.X ? 1 : 0, 
		           u.Y >= v.Y ? 1 : 0, 
		           u.Z >= v.Z ? 1 : 0);
}

FVector NiagaraVectorSwizzle(FVector v, uint32 x, uint32 y, uint32 z)
{
	return FVector(v[x], v[y], v[z]);
}

FVector NiagaraVectorMin(FVector u, FVector v)
{
	return FVector(u.X < v.X ? u.X : v.X, 
		           u.Y < v.Y ? u.Y : v.Y, 
		           u.Z < v.Z ? u.Z : v.Z);
}

FVector NiagaraVectorMax(FVector u, FVector v)
{
	return FVector(u.X > v.X ? u.X : v.X,
		           u.Y > v.Y ? u.Y : v.Y,
	           	   u.Z > v.Z ? u.Z : v.Z);
} 

FNiagaraMatrix4x3 SimplexCorners(FVector v)
{
	FVector tet = NiagaraVectorFloor(v + v.X / 3 + v.Y / 3 + v.Z / 3);
	FVector base = tet - tet.X / 6 - tet.Y / 6 - tet.Z / 6;
	FVector f = v - base;

	FVector g = NiagaraVectorStep(NiagaraVectorSwizzle(f, 1, 2, 0), f), h = FVector(1.0f) - NiagaraVectorSwizzle(g, 2, 0, 1);
	FVector a1 = NiagaraVectorMin(g, h) - 1. / 6., a2 = NiagaraVectorMax(g, h) - 1. / 3.;

	return FNiagaraMatrix4x3(base, base + a1, base + a2, base + 0.5);
}

FVector4 NiagaraVector4Saturate(const FVector4& v) {
	return FVector4(FMath::Clamp(v.X, 0.0f, 1.0f), FMath::Clamp(v.Y, 0.0f, 1.0f), FMath::Clamp(v.Z, 0.0f, 1.0f), FMath::Clamp(v.W, 0.0f, 1.0f));
}

FVector4 SimplexSmooth(FNiagaraMatrix4x3 f)
{
	const float scale = 1024. / 375.;
	FVector4 d = FVector4(FVector::DotProduct(f[0], f[0]), FVector::DotProduct(f[1], f[1]), FVector::DotProduct(f[2], f[2]), FVector::DotProduct(f[3], f[3]));
	FVector4 s = NiagaraVector4Saturate(2.0f * d);
	return scale * (FVector4(1.0f, 1.0f, 1.0f, 1.0f) + s * (FVector4(-3.0f, -3.0f, -3.0f, -3.0f) + s * (FVector4(3.0f, 3.0f, 3.0f, 3.0f) - s)));
}

struct FNiagaraMatrix3x4
{
	FVector4 row0;
	FVector4 row1;
	FVector4 row2;

	FNiagaraMatrix3x4() : row0(FVector4(0.0, 0.0, 0.0, 0.0)), row1(FVector4(0.0, 0.0, 0.0, 0.0)), row2(FVector4(0.0, 0.0, 0.0, 0.0)) {}
	FNiagaraMatrix3x4(const FVector4& row0, const FVector4& row1, const FVector4& row2) : row0(row0), row1(row1), row2(row2) {}

	FVector4& operator[](int row)
	{
		if (row == 0) return row0;
		if (row == 1) return row1;
		if (row == 2) return row2;
		check(false);
		return row0;
	}
	const FVector4& operator[](int row) const
	{
		if (row == 0) return row0;
		if (row == 1) return row1;
		if (row == 2) return row2;
		check(false); 
		return row0; 
	}
};

FNiagaraMatrix3x4 SimplexDSmooth(FNiagaraMatrix4x3 f)
{
	const float scale = 1024. / 375.;
	FVector4 d = FVector4(FVector::DotProduct(f[0], f[0]), FVector::DotProduct(f[1], f[1]), FVector::DotProduct(f[2], f[2]), FVector::DotProduct(f[3], f[3]));
	FVector4 s = NiagaraVector4Saturate(2 * d);
	s = -12 * FVector4(scale, scale, scale, scale) + s * (24 * FVector4(scale, scale, scale, scale) - s * 12 * scale);

	return FNiagaraMatrix3x4(
		s * FVector4(f[0][0], f[1][0], f[2][0], f[3][0]),
		s * FVector4(f[0][1], f[1][1], f[2][1], f[3][1]),
		s * FVector4(f[0][2], f[1][2], f[2][2], f[3][2]));
}

FNiagaraUIntVector FNiagaraUIntVectorSwizzle(FNiagaraUIntVector v, int x, int y, int z)
{
	return FNiagaraUIntVector(v[x], v[y], v[z]);
}

FVector FNiagaraUIntVectorToFVector(FNiagaraUIntVector v)
{
	return FVector(v.X, v.Y, v.Z);
}

FVector MulFVector4AndFNiagaraMatrix4x3(const FVector4& lhs, FNiagaraMatrix4x3 rhs)
{
	return FVector(lhs[0] * rhs[0][0] + lhs[1] * rhs[1][0] + lhs[2] * rhs[2][0] + lhs[3] * rhs[3][0],
		           lhs[0] * rhs[0][1] + lhs[1] * rhs[1][1] + lhs[2] * rhs[2][1] + lhs[3] * rhs[3][1],
	           	   lhs[0] * rhs[0][2] + lhs[1] * rhs[1][2] + lhs[2] * rhs[2][2] + lhs[3] * rhs[3][2]);
}

FVector MulFNiagaraMatrix3x4FAndVector4(const FNiagaraMatrix3x4& lhs, const FVector4& rhs)
{
	return FVector(lhs[0][0] * rhs[0] + lhs[0][1] * rhs[1] + lhs[0][2] * rhs[2] + lhs[0][3] * rhs[3],
		           lhs[1][0] * rhs[0] + lhs[1][1] * rhs[1] + lhs[1][2] * rhs[2] + lhs[1][3] * rhs[3],
		           lhs[2][0] * rhs[0] + lhs[2][1] * rhs[1] + lhs[2][2] * rhs[2] + lhs[2][3] * rhs[3]);
}

#define MGradientMask FNiagaraUIntVector(0x8000, 0x4000, 0x2000)
#define MGradientScale FVector(1. / 0x4000, 1. / 0x2000, 1. / 0x1000)

FNiagaraMatrix3x4 JacobianSimplex_ALU(FVector v)
{
	FNiagaraMatrix4x3 T = SimplexCorners(v);
	FNiagaraUIntVector rand;
	FNiagaraMatrix4x3 gvec[3], fv;
	FNiagaraMatrix3x4 grad;

	fv[0] = v - T[0];
	rand = Rand3DPCG16(FIntVector(NiagaraVectorFloor(6 * T[0] + 0.5)));
	gvec[0][0] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 0, 0, 0) & MGradientMask)) * MGradientScale - 1;
	gvec[1][0] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 1, 1, 1) & MGradientMask)) * MGradientScale - 1;
	gvec[2][0] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 2, 2, 2) & MGradientMask)) * MGradientScale - 1;
	grad[0][0] = FVector::DotProduct(gvec[0][0], fv[0]);
	grad[1][0] = FVector::DotProduct(gvec[1][0], fv[0]);
	grad[2][0] = FVector::DotProduct(gvec[2][0], fv[0]);

	fv[1] = v - T[1];
	rand = Rand3DPCG16(FIntVector(NiagaraVectorFloor(6 * T[1] + 0.5)));
	gvec[0][1] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 0, 0, 0) & MGradientMask)) * MGradientScale - 1;
	gvec[1][1] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 1, 1, 1) & MGradientMask)) * MGradientScale - 1;
	gvec[2][1] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 2, 2, 2) & MGradientMask)) * MGradientScale - 1;
	grad[0][1] = FVector::DotProduct(gvec[0][1], fv[1]);
	grad[1][1] = FVector::DotProduct(gvec[1][1], fv[1]);
	grad[2][1] = FVector::DotProduct(gvec[2][1], fv[1]);

	fv[2] = v - T[2];
	rand = Rand3DPCG16(FIntVector(NiagaraVectorFloor(6 * T[2] + 0.5)));
	gvec[0][2] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 0, 0, 0) & MGradientMask)) * MGradientScale - 1;
	gvec[1][2] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 1, 1, 1) & MGradientMask)) * MGradientScale - 1;
	gvec[2][2] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 2, 2, 2) & MGradientMask)) * MGradientScale - 1;
	grad[0][2] = FVector::DotProduct(gvec[0][2], fv[2]);
	grad[1][2] = FVector::DotProduct(gvec[1][2], fv[2]);
	grad[2][2] = FVector::DotProduct(gvec[2][2], fv[2]);

	fv[3] = v - T[3];
	rand = Rand3DPCG16(FIntVector(NiagaraVectorFloor(6 * T[3] + 0.5)));
	gvec[0][3] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 0, 0, 0) & MGradientMask)) * MGradientScale - 1;
	gvec[1][3] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 1, 1, 1) & MGradientMask)) * MGradientScale - 1;
	gvec[2][3] = FVector(FNiagaraUIntVectorToFVector(FNiagaraUIntVectorSwizzle(rand, 2, 2, 2) & MGradientMask)) * MGradientScale - 1;
	grad[0][3] = FVector::DotProduct(gvec[0][3], fv[3]);
	grad[1][3] = FVector::DotProduct(gvec[1][3], fv[3]);
	grad[2][3] = FVector::DotProduct(gvec[2][3], fv[3]);

	FVector4 sv = SimplexSmooth(fv);
	FNiagaraMatrix3x4 ds = SimplexDSmooth(fv);

	FNiagaraMatrix3x4 jacobian;
	jacobian[0] = FVector4(MulFVector4AndFNiagaraMatrix4x3(sv, gvec[0]) + MulFNiagaraMatrix3x4FAndVector4(ds, grad[0]), Dot4(sv, grad[0]));
	jacobian[1] = FVector4(MulFVector4AndFNiagaraMatrix4x3(sv, gvec[1]) + MulFNiagaraMatrix3x4FAndVector4(ds, grad[1]), Dot4(sv, grad[1]));
	jacobian[2] = FVector4(MulFVector4AndFNiagaraMatrix4x3(sv, gvec[2]) + MulFNiagaraMatrix3x4FAndVector4(ds, grad[2]), Dot4(sv, grad[2]));

	return jacobian;
}

#undef MGradientMask
#undef MGradientScale 

// ----------------------------------------------------- end noise helpers -----------------------------------------------------


UNiagaraDataInterfaceCurlNoise::UNiagaraDataInterfaceCurlNoise(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
	, Seed(0)
{
	OffsetFromSeed = FNiagaraUIntVectorToFVector(Rand3DPCG16(FIntVector(Seed, Seed, Seed))) / 100.0;

	Proxy.Reset(new FNiagaraDataInterfaceProxyCurlNoise(OffsetFromSeed));
}

void UNiagaraDataInterfaceCurlNoise::PostInitProperties()
{
	Super::PostInitProperties();

	if (HasAnyFlags(RF_ClassDefaultObject))
	{
		ENiagaraTypeRegistryFlags Flags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
		FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), Flags);
	}
}

void UNiagaraDataInterfaceCurlNoise::PostLoad()
{
	Super::PostLoad();
	OffsetFromSeed = FNiagaraUIntVectorToFVector(Rand3DPCG16(FIntVector(Seed, Seed, Seed))) / 100.0;

	MarkRenderDataDirty();
}

#if WITH_EDITOR

void UNiagaraDataInterfaceCurlNoise::PreEditChange(FProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	
	// Flush the rendering thread before making any changes to make sure the 
	// data read by the compute shader isn't subject to a race condition.
	// TODO(mv): Solve properly using something like a RT Proxy.
	FlushRenderingCommands();
}
void UNiagaraDataInterfaceCurlNoise::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraDataInterfaceCurlNoise, Seed))
	{
		// NOTE: Calculate the offset based on the seed on-change instead of on every invocation for every particle...
		OffsetFromSeed = FNiagaraUIntVectorToFVector(Rand3DPCG16(FIntVector(Seed, Seed, Seed))) / 100.0;
	}

	MarkRenderDataDirty();
}

#endif

bool UNiagaraDataInterfaceCurlNoise::CopyToInternal(UNiagaraDataInterface* Destination) const
{
	if (!Super::CopyToInternal(Destination))
	{
		return false;
	}
	UNiagaraDataInterfaceCurlNoise* DestinationCurlNoise = CastChecked<UNiagaraDataInterfaceCurlNoise>(Destination);
	DestinationCurlNoise->Seed = Seed;
	DestinationCurlNoise->OffsetFromSeed = OffsetFromSeed;
	DestinationCurlNoise->MarkRenderDataDirty();

	return true;
}

bool UNiagaraDataInterfaceCurlNoise::Equals(const UNiagaraDataInterface* Other) const
{
	if (!Super::Equals(Other))
	{
		return false;
	}
	const UNiagaraDataInterfaceCurlNoise* OtherCurlNoise = CastChecked<const UNiagaraDataInterfaceCurlNoise>(Other);
	return OtherCurlNoise->Seed == Seed;
}

void UNiagaraDataInterfaceCurlNoise::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
	FNiagaraFunctionSignature Sig;
	Sig.Name = SampleNoiseFieldName;
	Sig.bMemberFunction = true;
	Sig.bRequiresContext = false;
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("NoiseField")));
	Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("XYZ")));
	Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Value")));
	//Sig.Owner = *GetName();

	OutFunctions.Add(Sig);
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraDataInterfaceCurlNoise, SampleNoiseField);
void UNiagaraDataInterfaceCurlNoise::GetVMExternalFunction(const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction &OutFunc)
{
	check(BindingInfo.Name == SampleNoiseFieldName);
	check(BindingInfo.GetNumInputs() == 3 && BindingInfo.GetNumOutputs() == 3);
	NDI_FUNC_BINDER(UNiagaraDataInterfaceCurlNoise, SampleNoiseField)::Bind(this, OutFunc);
}

void UNiagaraDataInterfaceCurlNoise::SampleNoiseField(FVectorVMExternalFunctionContext& Context)
{
	VectorVM::FExternalFuncInputHandler<float> XParam(Context);
	VectorVM::FExternalFuncInputHandler<float> YParam(Context);
	VectorVM::FExternalFuncInputHandler<float> ZParam(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleX(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleY(Context);
	VectorVM::FExternalFuncRegisterHandler<float> OutSampleZ(Context);

	for (int32 InstanceIdx = 0; InstanceIdx < Context.GetNumInstances(); ++InstanceIdx)
	{
		FVector InCoords = FVector(XParam.GetAndAdvance(), YParam.GetAndAdvance(), ZParam.GetAndAdvance());

		// See comments to JacobianSimplex_ALU in Random.ush
		FNiagaraMatrix3x4 J = JacobianSimplex_ALU(InCoords + OffsetFromSeed);
		*OutSampleX.GetDestAndAdvance() = J[1][2] - J[2][1];
		*OutSampleY.GetDestAndAdvance() = J[2][0] - J[0][2];
		*OutSampleZ.GetDestAndAdvance() = J[0][1] - J[1][0];
	}
}

#if WITH_EDITORONLY_DATA
bool UNiagaraDataInterfaceCurlNoise::AppendCompileHash(FNiagaraCompileHashVisitor* InVisitor) const
{
	bool bSuccess = Super::AppendCompileHash(InVisitor);
	bSuccess &= InVisitor->UpdateShaderParameters<FShaderParameters>();
	return bSuccess;
}

bool UNiagaraDataInterfaceCurlNoise::GetFunctionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo, int FunctionInstanceIndex, FString& OutHLSL)
{
	static const TCHAR* FormatSample = TEXT(R"(
		void {FunctionName}(float3 In_XYZ, out float3 Out_Value)
		{
			// NOTE(mv): The comments in random.ush claims that the unused part is optimized away, so it only uses 6 out of 12 values in our case.
			float3x4 J = JacobianSimplex_ALU(In_XYZ + {OffsetFromSeedName}, false, 1.0);
			Out_Value = float3(J[1][2]-J[2][1], J[2][0]-J[0][2], J[0][1]-J[1][0]); // See comments to JacobianSimplex_ALU in Random.ush
		}
	)");
	TMap<FString, FStringFormatArg> ArgsSample;
	ArgsSample.Add(TEXT("FunctionName"), FunctionInfo.InstanceName);
	ArgsSample.Add(TEXT("OffsetFromSeedName"), ParamInfo.DataInterfaceHLSLSymbol + TEXT("_OffsetFromSeed"));
	OutHLSL += FString::Format(FormatSample, ArgsSample);
	return true;
}

void UNiagaraDataInterfaceCurlNoise::GetParameterDefinitionHLSL(const FNiagaraDataInterfaceGPUParamInfo& ParamInfo, FString& OutHLSL)
{
	OutHLSL.Appendf(TEXT("float3 %s_OffsetFromSeed;\n"), *ParamInfo.DataInterfaceHLSLSymbol);
}
#endif

void UNiagaraDataInterfaceCurlNoise::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
	ShaderParametersBuilder.AddNestedStruct<FShaderParameters>();
}

void UNiagaraDataInterfaceCurlNoise::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
	FNiagaraDataInterfaceProxyCurlNoise& DIProxy = Context.GetProxy<FNiagaraDataInterfaceProxyCurlNoise>();
	
	FShaderParameters* ShaderParameters = Context.GetParameterNestedStruct<FShaderParameters>();
	ShaderParameters->OffsetFromSeed = FVector3f(DIProxy.OffsetFromSeed);
}

void UNiagaraDataInterfaceCurlNoise::PushToRenderThreadImpl()
{
	FNiagaraDataInterfaceProxyCurlNoise* RT_Proxy = GetProxyAs<FNiagaraDataInterfaceProxyCurlNoise>();

	FVector RT_Offset = OffsetFromSeed;

	// Push Updates to Proxy.
	ENQUEUE_RENDER_COMMAND(FUpdateDIColorCurve)(
		[RT_Proxy, RT_Offset](FRHICommandListImmediate& RHICmdList)
	{
		RT_Proxy->OffsetFromSeed = RT_Offset;
	});
}

