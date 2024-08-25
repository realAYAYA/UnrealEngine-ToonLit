// Copyright Epic Games, Inc. All Rights Reserved.

#include "PCGAlembicInterop.h"

#include "Elements/PCGLoadAlembicElement.h"

#include "PCGComponent.h"
#include "Data/PCGPointData.h"
#include "Elements/Metadata/PCGMetadataElementCommon.h"
#include "Metadata/Accessors/IPCGAttributeAccessorTpl.h"
#include "Metadata/Accessors/PCGAttributeAccessorHelpers.h"
#include "Metadata/Accessors/PCGAttributeAccessorKeys.h"

#include <string>

#define LOCTEXT_NAMESPACE "PCGExternalData_Alembic"

namespace PCGAlembicInterop
{

#if WITH_EDITOR
class FPCGAlembicPositionsAccessor : public IPCGAttributeAccessorT<FPCGAlembicPositionsAccessor>
{
public:
	using Type = FVector;
	using Super = IPCGAttributeAccessorT<FPCGAlembicPositionsAccessor>;

	FPCGAlembicPositionsAccessor(Alembic::AbcGeom::IPoints::schema_type::Sample& Sample)
		: Super(/*bInReadOnly=*/true)
		, SamplePtr(Sample.getPositions())
	{}

	bool GetRangeImpl(TArrayView<FVector> OutValues, int32 Index, const IPCGAttributeAccessorKeys&) const
	{
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			Alembic::Abc::P3fArraySample::value_type Position = (*SamplePtr)[Index + i];
			OutValues[i] = FVector(Position.x, Position.y, Position.z);
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const FVector>, int32, IPCGAttributeAccessorKeys&, EPCGAttributeAccessorFlags)
	{
		return false;
	}

private:
	Alembic::Abc::P3fArraySamplePtr SamplePtr;
};

template<typename T, typename AbcParamType, int32 Extent, int32 SubExtent, bool bUseCStr>
class FPCGAlembicAccessor : public IPCGAttributeAccessorT<FPCGAlembicAccessor<T, AbcParamType, Extent, SubExtent, bUseCStr>>
{
public:
	using Type = T;
	using Super = IPCGAttributeAccessorT<FPCGAlembicAccessor<T, AbcParamType, Extent, SubExtent, bUseCStr>>;

	FPCGAlembicAccessor(Alembic::AbcGeom::ICompoundProperty& Parameters, const FString& PropName)
		: Super(/*bInReadOnly=*/true)
		, Param(Parameters, std::string(TCHAR_TO_UTF8(*PropName)))
		, SamplePtr(Param.getExpandedValue().getVals())
	{}

	bool GetRangeImpl(TArrayView<T> OutValues, int32 Index, const IPCGAttributeAccessorKeys&) const
	{
		for (int32 i = 0; i < OutValues.Num(); ++i)
		{
			T& OutValue = OutValues[i];
			const int32 DataIndex = Index + i;

			if constexpr (bUseCStr && Extent == 1 && SubExtent == 1) // String
			{
				OutValue = (*SamplePtr)[DataIndex].c_str();
			}
			else if constexpr (Extent == 1 && SubExtent == 1) // Scalar
			{
				OutValue = (*SamplePtr)[DataIndex];
			}
			else if constexpr (Extent > 1 && SubExtent == 1)
			{
				for (int32 D = 0; D < Extent; ++D)
				{
					OutValue[D] = (*SamplePtr)[DataIndex][D];
				}
			}
			else if constexpr (Extent == 1 && SubExtent > 1)
			{
				for (int32 D = 0; D < SubExtent; ++D)
				{
					OutValue[D] = (*SamplePtr)[DataIndex * SubExtent + D];
				}
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	bool SetRangeImpl(TArrayView<const T>, int32, IPCGAttributeAccessorKeys&, EPCGAttributeAccessorFlags)
	{
		return false;
	}

private:
	AbcParamType Param;
	using AbcSamplerType = typename AbcParamType::Sample::samp_ptr_type;
	AbcSamplerType SamplePtr;
};

TUniquePtr<const IPCGAttributeAccessor> CreateAlembicPropAccessor(Alembic::AbcGeom::ICompoundProperty& Parameters, Alembic::Abc::PropertyHeader& PropertyHeader, const FString& PropName)
{
	Alembic::Abc::PropertyType PropType = PropertyHeader.getPropertyType();

	Alembic::Abc::DataType DataType = PropertyHeader.getDataType();
	int TypeExtent = DataType.getExtent();

	FString MetadataExtent(PropertyHeader.getMetaData().get("arrayExtent").c_str());
	const int32 SubExtent = (MetadataExtent.Compare("") == 0 ? 1 : FCString::Atoi(*MetadataExtent));

	if (DataType.getPod() == Alembic::Util::kFloat32POD && (TypeExtent == 1 || SubExtent == 1))
	{
		switch (TypeExtent)
		{
		case 1:
			switch (SubExtent)
			{
			case 1: return MakeUnique<FPCGAlembicAccessor<float, Alembic::AbcGeom::IFloatGeomParam, 1, 1, false>>(Parameters, PropName);
			case 2: return MakeUnique<FPCGAlembicAccessor<FVector2D, Alembic::AbcGeom::IFloatGeomParam, 1, 2, false>>(Parameters, PropName);
			case 3: return MakeUnique<FPCGAlembicAccessor<FVector, Alembic::AbcGeom::IFloatGeomParam, 1, 3, false>>(Parameters, PropName);
			case 4: return MakeUnique<FPCGAlembicAccessor<FVector4, Alembic::AbcGeom::IFloatGeomParam, 1, 4, false>>(Parameters, PropName);
			}
		case 2: return MakeUnique<FPCGAlembicAccessor<FVector2D, Alembic::AbcGeom::IV2fGeomParam, 2, 1, false>>(Parameters, PropName);
		case 3: return MakeUnique<FPCGAlembicAccessor<FVector, Alembic::AbcGeom::IV3fGeomParam, 3, 1, false>>(Parameters, PropName);
		case 4: return MakeUnique<FPCGAlembicAccessor<FVector4, Alembic::AbcGeom::IQuatfGeomParam, 4, 1, false>>(Parameters, PropName);
		}
	}
	else if (DataType.getPod() == Alembic::Util::kFloat64POD && (TypeExtent == 1 || SubExtent == 1))
	{
		switch (TypeExtent)
		{
		case 1:
			switch (SubExtent)
			{
			case 1: return MakeUnique<FPCGAlembicAccessor<double, Alembic::AbcGeom::IDoubleGeomParam, 1, 1, false>>(Parameters, PropName);
			case 2: return MakeUnique<FPCGAlembicAccessor<FVector2D, Alembic::AbcGeom::IDoubleGeomParam, 1, 2, false>>(Parameters, PropName);
			case 3: return MakeUnique<FPCGAlembicAccessor<FVector, Alembic::AbcGeom::IDoubleGeomParam, 1, 3, false>>(Parameters, PropName);
			case 4: return MakeUnique<FPCGAlembicAccessor<FVector4, Alembic::AbcGeom::IDoubleGeomParam, 1, 4, false>>(Parameters, PropName);
			}
		case 2: return MakeUnique<FPCGAlembicAccessor<FVector2D, Alembic::AbcGeom::IV2dGeomParam, 2, 1, false>>(Parameters, PropName);
		case 3: return MakeUnique<FPCGAlembicAccessor<FVector, Alembic::AbcGeom::IV3dGeomParam, 3, 1, false>>(Parameters, PropName);
		case 4: return MakeUnique<FPCGAlembicAccessor<FVector4, Alembic::AbcGeom::IQuatdGeomParam, 4, 1, false>>(Parameters, PropName);
		}
	}
	else if ((TypeExtent == 0 || TypeExtent == 1) && SubExtent == 1) // Scalar types
	{
		switch (DataType.getPod())
		{
		case Alembic::Util::kBooleanPOD: return MakeUnique<FPCGAlembicAccessor<bool, Alembic::AbcGeom::IBoolGeomParam, 1, 1, false>>(Parameters, PropName);
		case Alembic::Util::kInt8POD: return MakeUnique<FPCGAlembicAccessor<int32, Alembic::AbcGeom::ICharGeomParam, 1, 1, false>>(Parameters, PropName);
		case Alembic::Util::kInt16POD: return MakeUnique<FPCGAlembicAccessor<int32, Alembic::AbcGeom::IInt16GeomParam, 1, 1, false>>(Parameters, PropName);
		case Alembic::Util::kInt32POD: return MakeUnique<FPCGAlembicAccessor<int32, Alembic::AbcGeom::IInt32GeomParam, 1, 1, false>>(Parameters, PropName);
		case Alembic::Util::kInt64POD: return MakeUnique<FPCGAlembicAccessor<int64, Alembic::AbcGeom::IInt64GeomParam, 1, 1, false>>(Parameters, PropName);
		case Alembic::Util::kUnknownPOD: // fall-through
		case Alembic::Util::kStringPOD: return MakeUnique<FPCGAlembicAccessor<FString, Alembic::AbcGeom::IStringGeomParam, 1, 1, true>>(Parameters, PropName);
		}
	}

	return nullptr;
}

bool CreatePointAccessorAndValidate(FPCGContext* Context, UPCGPointData* PointData, const TUniquePtr<const IPCGAttributeAccessor>& AlembicPropAccessor, const FPCGAttributePropertySelector& PointPropertySelector, const FString& PropName, TUniquePtr<IPCGAttributeAccessor>& PointPropertyAccessor)
{
	if (!PointData || !PointData->Metadata)
	{
		return false;
	}

	if (!AlembicPropAccessor)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("AlembicPropertyNotSupported", "Property '{0}' is not of a supported type."), FText::FromString(PropName)));
		return false;
	}

	UPCGMetadata* PointMetadata = PointData->Metadata;

	// Create attribute if needed
	if (PointPropertySelector.GetSelection() == EPCGAttributePropertySelection::Attribute && !PointMetadata->HasAttribute(FName(PropName)))
	{
		auto CreateAttribute = [PointMetadata, &PropName](auto Dummy)
		{
			using AttributeType = decltype(Dummy);
			return PCGMetadataElementCommon::ClearOrCreateAttribute<AttributeType>(PointMetadata, FName(PropName)) != nullptr;
		};

		if (!PCGMetadataAttribute::CallbackWithRightType(AlembicPropAccessor->GetUnderlyingType(), CreateAttribute))
		{
			PCGE_LOG_C(Warning, GraphAndLog, Context, FText::Format(LOCTEXT("FailedToCreateNewAttribute", "Failed to create new attribute '{0}'"), FText::FromString(PropName)));
			return false;
		}
	}

	PointPropertyAccessor = PCGAttributeAccessorHelpers::CreateAccessor(PointData, PointPropertySelector);

	if (!PointPropertyAccessor)
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("AlembicTargetNotSupported", "Unable to write to target property '{0}.'"), PointPropertySelector.GetDisplayText()));
		return false;
	}

	// Final verification, if we can put the value of input into output
	if (!PCG::Private::IsBroadcastable(AlembicPropAccessor->GetUnderlyingType(), PointPropertyAccessor->GetUnderlyingType()))
	{
		FText InputTypeName = FText::FromString(PCG::Private::GetTypeName(AlembicPropAccessor->GetUnderlyingType()));
		FText OutputTypeName = FText::FromString(PCG::Private::GetTypeName(PointPropertyAccessor->GetUnderlyingType()));

		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("CannotBroadcastTypes", "Cannot convert input type '{0}' into output type '{1}'"), InputTypeName, OutputTypeName));
		return false;
	}

	return true;
}

void ParseAlembicObject(FPCGExternalDataContext* Context, const Alembic::Abc::IObject& Object)
{
	check(Context);
	const UPCGExternalDataSettings* Settings = Context->GetInputSettings<UPCGExternalDataSettings>();
	check(Settings);

	const Alembic::Abc::MetaData& ObjectMetaData = Object.getMetaData();
	const uint32 NumChildren = Object.getNumChildren();

	if (Alembic::AbcGeom::IPoints::matches(ObjectMetaData))
	{
		Alembic::AbcGeom::IPoints Points = Alembic::AbcGeom::IPoints(Object, Alembic::Abc::kWrapExisting);
		Alembic::AbcGeom::IPoints::schema_type::Sample Sample = Points.getSchema().getValue();

		Alembic::Abc::P3fArraySamplePtr Positions = Sample.getPositions();
		uint32 NumPoints = Positions ? Positions->size() : 0;

		if (NumPoints > 0)
		{
			// Create point data & mapping
			UPCGPointData* PointData = NewObject<UPCGPointData>();
			check(PointData);

			UPCGMetadata* PointMetadata = PointData->MutableMetadata();

			FPCGExternalDataContext::FPointDataAccessorsMapping& PointDataAccessorMapping = Context->PointDataAccessorsMapping.Emplace_GetRef();
			PointDataAccessorMapping.Data = PointData;
			PointDataAccessorMapping.Metadata = PointMetadata;
			// We're not going to use the input keys, but we still need to provide something
			PointDataAccessorMapping.RowKeys = MakeUnique<FPCGAttributeAccessorKeysEntries>(PCGInvalidEntryKey);

			TArray<FPCGPoint>& OutPoints = PointData->GetMutablePoints();
			OutPoints.SetNum(NumPoints);

			// If the user has provided a position remapping, don't process points right now, instead push the transformation to the row accessors
			if (const FPCGAttributePropertySelector* RemappedPositions = Settings->AttributeMapping.Find(TEXT("Position")))
			{
				TUniquePtr<const IPCGAttributeAccessor> AlembicPositionAccessor = MakeUnique<FPCGAlembicPositionsAccessor>(Sample);
				TUniquePtr<IPCGAttributeAccessor> PointPropertyAccessor;

				FPCGAttributePropertyOutputSelector PointPositionSelector;
				PointPositionSelector.ImportFromOtherSelector(*RemappedPositions);
				const FString PropName = PointPositionSelector.GetName().ToString();

				if (CreatePointAccessorAndValidate(Context, PointData, AlembicPositionAccessor, PointPositionSelector, PropName, PointPropertyAccessor))
				{
					PointDataAccessorMapping.RowToPointAccessors.Emplace(MoveTemp(AlembicPositionAccessor), MoveTemp(PointPropertyAccessor), PointPositionSelector);
				}
			}
			else // Otherwise, write the positions directly
			{
				for (uint32 PointIndex = 0; PointIndex < NumPoints; ++PointIndex)
				{
					Alembic::Abc::P3fArraySample::value_type Position = (*Positions)[PointIndex];
					OutPoints[PointIndex].Transform.SetLocation(FVector(Position.x, Position.y, Position.z));
				}
			}

			Alembic::AbcGeom::ICompoundProperty Parameters = Points.getSchema().getArbGeomParams();
			for (int Index = 0; Index < Parameters.getNumProperties(); ++Index)
			{
				Alembic::Abc::PropertyHeader PropertyHeader = Parameters.getPropertyHeader(Index);
				FString PropName(PropertyHeader.getName().c_str());

				// We'll parse only properties that affect every point/object
				if (PropertyHeader.getPropertyType() != Alembic::Abc::kArrayProperty &&
					!(PropertyHeader.getPropertyType() == Alembic::Abc::kCompoundProperty && (PropertyHeader.getDataType().getPod() == Alembic::Util::kUnknownPOD || PropertyHeader.getDataType().getPod() == Alembic::Util::kStringPOD)))
				{
					PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("AlembicPropertyType", "Property '{0}' is not supported (expected array)."), FText::FromString(PropName)));
					continue;
				}

				TUniquePtr<const IPCGAttributeAccessor> AlembicPropAccessor = CreateAlembicPropAccessor(Parameters, PropertyHeader, PropName);
				TUniquePtr<IPCGAttributeAccessor> PointPropertyAccessor;

				// Setup attribute property selector
				FPCGAttributePropertySelector PointPropertySelector;
				if (const FPCGAttributePropertySelector* MappedField = Settings->AttributeMapping.Find(PropName))
				{
					PointPropertySelector = *MappedField;
					PropName = PointPropertySelector.GetName().ToString();
				}
				else
				{
					PointPropertySelector.Update(PropName);
				}

				if (CreatePointAccessorAndValidate(Context, PointData, AlembicPropAccessor, PointPropertySelector, PropName, PointPropertyAccessor))
				{
					PointDataAccessorMapping.RowToPointAccessors.Emplace(MoveTemp(AlembicPropAccessor), MoveTemp(PointPropertyAccessor), PointPropertySelector);
				}
			}
		}
	}

	if (NumChildren > 0)
	{
		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			ParseAlembicObject(Context, Object.getChild(ChildIndex));
		}
	}
}

void LoadFromAlembicFile(FPCGLoadAlembicContext* Context, const FString& FileName)
{
	check(Context);

	/** Factory used to generate objects*/
	Alembic::AbcCoreFactory::IFactory& Factory = Context->Factory;
	Alembic::AbcCoreFactory::IFactory::CoreType CompressionType = Alembic::AbcCoreFactory::IFactory::kUnknown;
	/** Archive-typed ABC file */
	Alembic::Abc::IArchive& Archive = Context->Archive;
	/** Alembic typed root (top) object*/
	Alembic::Abc::IObject& TopObject = Context->TopObject;

	Factory.setPolicy(Alembic::Abc::ErrorHandler::kQuietNoopPolicy);
	const size_t ReasonableNumStreams = 12; // This is what we had in Rule Processor historically
	Factory.setOgawaNumStreams(ReasonableNumStreams);

	// Extract Archive and compression type from file
	Archive = Factory.getArchive(TCHAR_TO_UTF8(*FileName), CompressionType);

	if (!Archive.valid())
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("FailedToOpenAlembic", "Failed to open '{0}': Not a valid Alembic file."), FText::FromString(FileName)));
		return;
	}

	// Get Top/root object
	TopObject = Alembic::Abc::IObject(Archive, Alembic::Abc::kTop);
	if (!TopObject.valid())
	{
		PCGE_LOG_C(Error, GraphAndLog, Context, FText::Format(LOCTEXT("FailedToOpenAlembicRoot", "Failed to import '{0}': Alembic root is not valid."), FText::FromString(FileName)));
		return;
	}

	ParseAlembicObject(Context, TopObject);
}

#endif // WITH_EDITOR

}

#undef LOCTEXT_NAMESPACE