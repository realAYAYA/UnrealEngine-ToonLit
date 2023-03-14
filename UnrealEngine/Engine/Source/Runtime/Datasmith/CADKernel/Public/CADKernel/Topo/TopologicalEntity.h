// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CADKernel/Core/CADKernelArchive.h"
#include "CADKernel/Core/Entity.h"
#include "CADKernel/Core/HaveStates.h"
#include "CADKernel/Core/Types.h"

namespace UE::CADKernel
{
class FModelMesh;
class FTopologicalFace;
class FTopologyReport;

class CADKERNEL_API FTopologicalEntity : public FEntity
{
	friend class FCoreTechBridge;

protected:
	FIdent CtKioId = 0;

public:

	FIdent GetKioId() const
	{
		return CtKioId;
	}

	virtual void Serialize(FCADKernelArchive& Ar) override
	{
		FEntity::Serialize(Ar);
		Ar << CtKioId;
	}

#ifdef CADKERNEL_DEV
	virtual FInfoEntity& GetInfo(FInfoEntity&) const override;
#endif

	const bool IsApplyCriteria() const
	{
		return ((States & EHaveStates::IsApplyCriteria) == EHaveStates::IsApplyCriteria);
	}

	virtual void SetApplyCriteria() const
	{
		States |= EHaveStates::IsApplyCriteria;
	}

	virtual void ResetApplyCriteria()
	{
		States &= ~EHaveStates::IsApplyCriteria;
	}

	bool IsMeshed() const
	{
		return ((States & EHaveStates::IsMeshed) == EHaveStates::IsMeshed);
	}

	virtual void SetMeshed()
	{
		States |= EHaveStates::IsMeshed;
	}

	virtual void ResetMeshed()
	{
		States &= ~EHaveStates::IsMeshed;
	}
};

} // namespace UE::CADKernel

