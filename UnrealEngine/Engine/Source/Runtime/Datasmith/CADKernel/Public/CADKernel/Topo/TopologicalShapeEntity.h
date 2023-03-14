// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/MetadataDictionary.h"
#include "CADKernel/Topo/TopologicalEntity.h"

namespace UE::CADKernel
{
class FModelMesh;
class FTopologicalFace;
class FTopologyReport;

class CADKERNEL_API FTopologicalShapeEntity : public FTopologicalEntity
{
private:
	FTopologicalShapeEntity* HostedBy = nullptr;
	FMetadataDictionary Dictionary;

public:

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FTopologicalEntity::Serialize(Ar);
		Dictionary.Serialize(Ar);
		SerializeIdent(Ar, &HostedBy);
	}

	const FMetadataDictionary& GetMetadataDictionary() const
	{
		return Dictionary;
	}

	void ExtractMetaData(TMap<FString, FString>& OutMetaData) const
	{
		Dictionary.ExtractMetaData(OutMetaData);
	}

	void CompleteMetadata();

	virtual int32 FaceCount() const = 0;
	virtual void GetFaces(TArray<FTopologicalFace*>& OutFaces) = 0;

	/**
	 * Each face of model is set by its orientation. This allow to make oriented mesh and to keep the face orientation in topological function.
	 * Marker2 of spread face is set. It must be reset after the process
	 */
	virtual void SpreadBodyOrientation() = 0;

#ifdef CADKERNEL_DEV
	virtual void FillTopologyReport(FTopologyReport& Report) const = 0;
#endif

	FTopologicalShapeEntity* GetHost()
	{
		return HostedBy;
	}

	void SetHost(FTopologicalShapeEntity* Body)
	{
		HostedBy = Body;
	}

	void ResetHost()
	{
		HostedBy = nullptr;
	}

	void SetHostId(const int32 InHostId)
	{
		Dictionary.SetHostId(InHostId);
	}

	int32 GetHostId() const
	{
		return Dictionary.GetHostId();
	}

	void SetLayer(const int32 InLayerId)
	{
		Dictionary.SetLayer(InLayerId);
	}

	void SetName(const FString& InName)
	{
		Dictionary.SetName(InName);
	}

	bool HasName() const
	{
		return Dictionary.HasName();
	}

	const TCHAR* GetName() const
	{
		return Dictionary.GetName();
	}

	void SetColorId(const uint32& InColorId)
	{
		Dictionary.SetColorId(InColorId);
	}

	uint32 GetColorId() const
	{
		return Dictionary.GetColorId();
	}

	void SetMaterialId(const uint32& InMaterialId)
	{
		Dictionary.SetMaterialId(InMaterialId);
	}

	uint32 GetMaterialId() const
	{
		return Dictionary.GetMaterialId();
	}

	void SetPatchId(int32 InPatchId)
	{
		Dictionary.SetPatchId(InPatchId);
	}

	int32 GetPatchId() const
	{
		return Dictionary.GetPatchId();
	}

	void RemoveOfHost()
	{
		GetHost()->Remove(this);
	}

	virtual void Remove(const FTopologicalShapeEntity*) = 0;

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

};

} // namespace UE::CADKernel

