// Copyright Epic Games, Inc. All Rights Reserved.

#include "DatasmithIFCImporter.h"

#include "DatasmithIFCImportOptions.h"
#include "IFC/IFCReader.h"
#include "IFC/IFCStaticMeshFactory.h"

#include "DatasmithAssetImportData.h"
#include "DatasmithImportOptions.h"
#include "DatasmithSceneFactory.h"
#include "DatasmithUtils.h"
#include "IDatasmithSceneElements.h"
#include "ObjectTemplates/DatasmithStaticMeshTemplate.h"
#include "Utility/DatasmithMeshHelper.h"

#include "AnalyticsEventAttribute.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogDatasmithIFCImport);

#define LOCTEXT_NAMESPACE "DatasmithIFCImporter"

FDatasmithIFCImporter::FDatasmithIFCImporter(TSharedRef<IDatasmithScene>& OutScene, UDatasmithIFCImportOptions* InOptions)
	: DatasmithScene(OutScene)
	, IFCReader(new IFC::FFileReader())
	, IFCStaticMeshFactory(new IFC::FStaticMeshFactory())
	, ImportOptions(InOptions)
{
}

FDatasmithIFCImporter::~FDatasmithIFCImporter()
{
}

void FDatasmithIFCImporter::SetImportOptions(UDatasmithIFCImportOptions* InOptions)
{
	ImportOptions = InOptions;
}

const TArray<IFC::FLogMessage>& FDatasmithIFCImporter::GetLogMessages() const
{
	LogMessages.Append(IFCReader->GetLogMessages());
	LogMessages.Append(IFCStaticMeshFactory->GetLogMessages());
	return LogMessages;
}

bool FDatasmithIFCImporter::OpenFile(const FString& InFileName)
{
	LogMessages.Empty();

	if (!IFCReader->ReadFile(InFileName))
	{
		LogMessages.Emplace(EMessageSeverity::Error, TEXT("Can't read IFC file: ") + InFileName);
		return false;
	}

	return true;
}

FString FDatasmithIFCImporter::GetFilteredName(const FString& Name)
{
	return FDatasmithUtils::SanitizeObjectName(Name);
}

FString FDatasmithIFCImporter::GetUniqueName(const FString& Name)
{
	FString FinalName = GetFilteredName(Name);

	if (ImportedActorNames.Contains(FinalName))
	{
		FinalName.AppendInt(ImportedActorNames.Num());
	}
	ImportedActorNames.Add(FinalName);

	return FinalName;
}

FString FDatasmithIFCImporter::ConvertGlobalIDToName(const FString& GlobalId)
{
	TArray<uint8> Bytes;
	Bytes.SetNumUninitialized(GlobalId.Len());
	int32 NumBytes = StringToBytes(GlobalId, Bytes.GetData(), Bytes.Num());
	return BytesToHex(Bytes.GetData(), NumBytes);
}

TSharedPtr<IDatasmithMeshActorElement> FDatasmithIFCImporter::CreateStaticMeshActor(const IFC::FObject& InObject, int64 MeshID)
{
	FString ActorName = ConvertGlobalIDToName(InObject.GlobalId);

	// check that no actors have this name, "shouldn't happen" as Global ID is 22 characters globally-unique
	// but replaced 'check' with warning so it doesn't assert(as happened once)
	if (ActorName != GetUniqueName(ActorName))
	{
		ActorName = GetUniqueName(ActorName);
		LogMessages.Emplace(EMessageSeverity::Warning, TEXT("Non-unique Global ID: ") + InObject.GlobalId);
	}

	FString ActorLabel = InObject.Type + TEXT("_") + InObject.Name; // duplication
	FString MeshName = TEXT("DefaultName");
	FString* MeshNamePtr = ImportedMeshes.Find(MeshID);

	if (MeshNamePtr == nullptr)
	{
		MeshName = ActorName + TEXT("_Mesh");

		TSharedRef<IDatasmithMeshElement> MeshElement = FDatasmithSceneFactory::CreateMesh(*MeshName);
		MeshElement->SetLabel(*ActorLabel);
		MeshElement->SetFileHash(IFCStaticMeshFactory->ComputeHash(InObject));

		MeshName = MeshElement->GetName();
		ImportedMeshes.Add(MeshID, MeshName);
		MeshElementToIFCMeshIndex.Add(&MeshElement.Get(), MeshID);
		IFCMeshIndexToMeshElement.Add(MeshID, MeshElement);

		DatasmithScene->AddMesh(MeshElement);
	}
	else
	{
		MeshName = *MeshNamePtr;
	}

	TSharedPtr<IDatasmithMeshActorElement> MeshActorElement = FDatasmithSceneFactory::CreateMeshActor(*ActorName);
	MeshActorElement->SetLabel(*ActorLabel);
	MeshActorElement->SetStaticMeshPathName(*MeshName);
	return MeshActorElement;
}

TSharedPtr<IDatasmithBaseMaterialElement> FDatasmithIFCImporter::CreateMaterial(const IFC::FMaterial& InMaterial)
{
	if (ImportedMaterials.Contains(InMaterial.ID))
	{
		return ImportedMaterials[InMaterial.ID];
	}

	FString MaterialName = GetUniqueName(GetFilteredName(InMaterial.Name));

	TSharedRef<IDatasmithUEPbrMaterialElement> MaterialElement = FDatasmithSceneFactory::CreateUEPbrMaterial(*MaterialName);

	{
		if (IDatasmithMaterialExpression* Expression = MaterialElement->AddMaterialExpression(EDatasmithMaterialExpressionType::ConstantColor))
		{
			// using Diffuse Color for Base Color
			Expression->SetName(TEXT("Color"));
			IDatasmithMaterialExpressionColor* ConstantColor = static_cast<IDatasmithMaterialExpressionColor*>(Expression);
			ConstantColor->GetColor() = InMaterial.DiffuseColour;
			MaterialElement->GetBaseColor().SetExpression(ConstantColor);
		}

		if (InMaterial.Transparency > 0)
		{
			if (IDatasmithMaterialExpression* Expression = MaterialElement->AddMaterialExpression(EDatasmithMaterialExpressionType::ConstantScalar))
			{
				Expression->SetName(TEXT("Opacity"));
				IDatasmithMaterialExpressionScalar* ConstantScalar = static_cast<IDatasmithMaterialExpressionScalar*>(Expression);
				ConstantScalar->GetScalar() = 1.f - InMaterial.Transparency;
				MaterialElement->GetOpacity().SetExpression(ConstantScalar);
				MaterialElement->SetBlendMode(EBlendMode::BLEND_Translucent);
			}
		}
		else
		{
			MaterialElement->SetBlendMode(EBlendMode::BLEND_Opaque);
		}

		MaterialElement->SetTwoSided(IFC::ESurfaceSide::Both == InMaterial.SurfaceSide);
	}

	ImportedMaterials.Add(InMaterial.ID, MaterialElement);
	return MaterialElement;
}

TSharedPtr<IDatasmithActorElement> FDatasmithIFCImporter::ConvertNode(const IFC::FObject& InObject)
{
	TSharedPtr<IDatasmithActorElement> ActorElement;

	ActorElement = ConvertMeshNode(InObject);
	if (!ActorElement.IsValid())
	{
		FString ActorName = ConvertGlobalIDToName(InObject.GlobalId);
		check(ActorName == GetUniqueName(ActorName));
		FString ActorLabel = InObject.Type + TEXT("_") + InObject.Name; // duplication
		ActorElement = FDatasmithSceneFactory::CreateActor(*ActorName);
		ActorElement->SetLabel(*ActorLabel);
	}

	{
		ActorElement->SetLayer(*InObject.Type);
		SetActorElementTransform(ActorElement, InObject.Transform);

		ActorElement->AddTag(*FString::Printf(TEXT("Ifc.Object.GlobalId.%s"), *InObject.GlobalId));
		ActorElement->AddTag(*FString::Printf(TEXT("Ifc.Object.Type.%s"), *InObject.Type));

		if (InObject.Properties.Num() > 0)
		{
			TSharedRef< IDatasmithMetaDataElement > MetaDataElement = FDatasmithSceneFactory::CreateMetaData(ActorElement->GetName());
			MetaDataElement->SetAssociatedElement(ActorElement);

			for (IFC::FProperty* PropertyObject : InObject.Properties)
			{
				for (IFC::FPropertyValue& PropertyValue : PropertyObject->Values)
				{
					TSharedRef< IDatasmithKeyValueProperty > KeyValueProperty = FDatasmithSceneFactory::CreateKeyValueProperty(*PropertyValue.Name);

					KeyValueProperty->SetValue(*PropertyValue.NominalValue);
					KeyValueProperty->SetPropertyType(EDatasmithKeyValuePropertyType::String);

					MetaDataElement->AddProperty(KeyValueProperty);
				}
			}

			DatasmithScene->AddMetaData(MetaDataElement);
		}
	}

	bool CreateAuxNodes = false;

	if (InObject.DecomposedBy.Num())
	{
		TSharedPtr<IDatasmithActorElement> Decomposition = ActorElement;
		if (CreateAuxNodes)
		{
			Decomposition = FDatasmithSceneFactory::CreateActor(*GetUniqueName(TEXT("decomposition")));
			ActorElement->AddChild(Decomposition);
		}

		// TODO: sort by name
		for (int32 PartIndex : InObject.DecomposedBy)
		{
			Decomposition->AddChild(ConvertNode(IFCReader->GetObjects()[PartIndex]));
		}
	}

	if (InObject.ContainsElements.Num())
	{
		TSharedPtr<IDatasmithActorElement> Containment = ActorElement;
		if (CreateAuxNodes)
		{
			Containment = FDatasmithSceneFactory::CreateActor(*GetUniqueName(TEXT("containment")));
			ActorElement->AddChild(Containment);
		}

		for (int32 PartIndex : InObject.ContainsElements)
		{

			Containment->AddChild(ConvertNode(IFCReader->GetObjects()[PartIndex]));
		}
	}

	ImportedIFCInstances.Add(InObject.IfcInstance);
	return ActorElement;
}

TSharedPtr<IDatasmithMeshActorElement> FDatasmithIFCImporter::ConvertMeshNode(const IFC::FObject& InObject)
{
	TSharedPtr<IDatasmithMeshActorElement> Result;
	int64 ShapeId = InObject.MappedShapeId ? InObject.MappedShapeId : InObject.ShapeId;
	const IFC::FObject* ObjectPtr = IFCReader->FindObjectFromShapeId(ShapeId);

	if (ObjectPtr == nullptr || ObjectPtr->bHasGeometry == false || ObjectPtr->bVisible == false)
	{
		return Result;
	}

	TSharedPtr<IDatasmithMeshActorElement> NodeActor = CreateStaticMeshActor(InObject, ShapeId);
	if (NodeActor.IsValid())
	{
		int32 MaterialID = 0;
		for (int64 MatObject : ObjectPtr->Materials)
		{
			if (ImportedMaterials.Contains(MatObject))
			{
				TSharedRef<IDatasmithMaterialIDElement> MaterialIDElement(FDatasmithSceneFactory::CreateMaterialId(ImportedMaterials[MatObject]->GetName()));
				MaterialIDElement->SetId(MaterialID);

				TSharedRef<IDatasmithMeshElement>* MeshElement = IFCMeshIndexToMeshElement.Find(ShapeId);
				if (MeshElement != nullptr)
				{
					(*MeshElement)->SetMaterial(MaterialIDElement->GetName(), MaterialIDElement->GetId());
				}

				MaterialID++;
			}
		}

		Result = NodeActor;
	}
	return Result;
}

bool FDatasmithIFCImporter::SendSceneToDatasmith()
{
	IFCReader->PostReadFile(0.01f, true);

	// Setup importer
	IFCStaticMeshFactory->SetUniformScale(100.f * IFCReader->GetLengthUnit());

	// Preparation.
	MeshElementToIFCMeshIndex.Empty();
	IFCMeshIndexToMeshElement.Empty();
	ImportedMeshes.Empty();
	ImportedMaterials.Empty();
	ImportedActorNames.Empty();

#ifdef WITH_IFC_ENGINE_LIB
	for (auto& OneMaterial : IFCReader->GetMaterialsMap())
	{
		TSharedPtr<IDatasmithBaseMaterialElement> MaterialElement = CreateMaterial(OneMaterial.Value);
		if (MaterialElement.IsValid())
		{
			DatasmithScene->AddMaterial(MaterialElement);
		}
	}

	for (int32 ProjectIndex : IFCReader->GetProjects())
	{
		const IFC::FObject& Project = IFCReader->GetObjects()[ProjectIndex];
		DatasmithScene->AddActor(ConvertNode(Project));
	}

	// Import all unreferenced objects
	TSharedPtr<IDatasmithActorElement> UnreferencedRoot = FDatasmithSceneFactory::CreateActor(TEXT("Unreferenced objects"));
	for (const IFC::FObject& IFCObjectRef : IFCReader->GetObjects())
	{
		if (IFCObjectRef.bRootObject && !ImportedIFCInstances.Contains(IFCObjectRef.IfcInstance))
		{
			UnreferencedRoot->AddChild(ConvertNode(IFCObjectRef));
		}
	}
	if (UnreferencedRoot->GetChildrenCount() > 0)
	{
		DatasmithScene->AddActor(UnreferencedRoot);
	}
#endif

	return true;
}

void FDatasmithIFCImporter::GetGeometriesForMeshElementAndRelease(const TSharedRef<IDatasmithMeshElement> MeshElement, TArray<FMeshDescription>& OutMeshDescriptions)
{
	if (int64* MeshIndexPtr = MeshElementToIFCMeshIndex.Find(&MeshElement.Get()))
	{
#ifdef WITH_IFC_ENGINE_LIB
		const IFC::FObject* pIFCObject = IFCReader->FindObjectFromShapeId(*MeshIndexPtr);

		if (pIFCObject != nullptr)
		{
			FMeshDescription MeshDescription;
			DatasmithMeshHelper::PrepareAttributeForStaticMesh(MeshDescription);

			IFCStaticMeshFactory->FillMeshDescription(pIFCObject, &MeshDescription);

			OutMeshDescriptions.Add(MoveTemp(MeshDescription));
		}
#endif
	}
}

FString FDatasmithIFCImporter::GetMeshGlobalId(const TSharedRef<IDatasmithMeshElement> MeshElement)
{
	if (int64* MeshIndexPtr = MeshElementToIFCMeshIndex.Find(&MeshElement.Get()))
	{
#ifdef WITH_IFC_ENGINE_LIB
		const IFC::FObject* pIFCObject = IFCReader->FindObjectFromShapeId(*MeshIndexPtr);

		if (pIFCObject != nullptr)
		{
			return pIFCObject->GlobalId;
		}
#endif
	}

	return FString("");
}

void FDatasmithIFCImporter::SetupCustomAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& EventAttributes) const
{
	if (IFCReader == nullptr)
	{
		return;
	}

	EventAttributes.Emplace(FAnalyticsEventAttribute(TEXT("IFCVersion"), IFCReader->GetIFCVersion()));
}

void FDatasmithIFCImporter::UnloadScene()
{
	MeshElementToIFCMeshIndex.Empty();
	ImportedMeshes.Empty();
	ImportedActorNames.Empty();

	IFCReader->CleanUp();

	IFCReader.Reset(new IFC::FFileReader());
	IFCStaticMeshFactory.Reset(new IFC::FStaticMeshFactory());
}

void FDatasmithIFCImporter::SetActorElementTransform(TSharedPtr<IDatasmithActorElement> ActorElement, const FTransform &Transform)
{
	check(Transform.GetRotation().IsNormalized());
	check(!Transform.GetScale3D().IsNearlyZero());
	ActorElement->SetRotation(Transform.GetRotation());
	ActorElement->SetScale(Transform.GetScale3D());
	ActorElement->SetTranslation(Transform.GetTranslation() * IFCStaticMeshFactory->GetUniformScale());
}

void FDatasmithIFCImporter::ApplyMaterials()
{
#ifdef WITH_IFC_ENGINE_LIB
	for (const IFC::FObject& IFCObjectRef : IFCReader->GetObjects())
	{
		int64 ShapeId = IFCObjectRef.MappedShapeId ? IFCObjectRef.MappedShapeId : IFCObjectRef.ShapeId;
		const IFC::FObject* ObjectPtr = IFCReader->FindObjectFromShapeId(ShapeId);

		if (ObjectPtr == nullptr || ObjectPtr->bHasGeometry == false)
		{
			continue;
		}

		int32 MaterialID = 0;
		for (int64 MatObject : ObjectPtr->Materials)
		{
			if (ImportedMaterials.Contains(MatObject))
			{
				TSharedRef<IDatasmithMeshElement>* MeshElement = IFCMeshIndexToMeshElement.Find(ShapeId);
				if (MeshElement != nullptr)
				{
					(*MeshElement)->SetMaterial(ImportedMaterials[MatObject]->GetName(), MaterialID);
				}

				MaterialID++;
			}
		}
	}
#endif
}

#undef LOCTEXT_NAMESPACE
