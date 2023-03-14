// Copyright Epic Games, Inc. All Rights Reserved.
#include "OculusAudioGeometryComponent.h"
#include "OculusAudioMaterialComponent.h"
#include "OculusAudioContextManager.h"
#include "OculusAudio.h"
#include "AudioDevice.h"
#include "IOculusAudioPlugin.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Runtime/Core/Public/Serialization/CustomVersion.h"
#include "Materials/MaterialInstanceDynamic.h"


#define COMPILE_AS_DEBUG_MODE 0

#if COMPILE_AS_DEBUG_MODE
#  pragma optimize("", off) // worse perf, better debugging
#endif


#define OVRA_AUDIO_GEOMETRY_COMPONENT_LATEST_VERSION 1

static const FGuid UOculusAudioGeometryGUID( 0x7aa25488, 0x40d74391, 0xad87d335, 0x347cfae1 ); // random GUID, guaranteed to be random.
static const FCustomVersionRegistration UOculusAudioGeometryGUIDRegistration( UOculusAudioGeometryGUID, OVRA_AUDIO_GEOMETRY_COMPONENT_LATEST_VERSION, TEXT("OculusAudioGeometryVersion") );

UOculusAudioGeometryComponent::UOculusAudioGeometryComponent()
	: ovrGeometry(nullptr)
	, CachedContext(nullptr)
{
	bAutoActivate = true;
	PrimaryComponentTick.bCanEverTick = true;
	bWantsInitializeComponent = true;
}

UOculusAudioGeometryComponent::~UOculusAudioGeometryComponent()
{
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
// Note: a child (attached) static mesh will have it's geometry merged into the parent. Therefore, its ovrGeometry data member will be nullptr below and this is why
// we need the Uploaded member.
void UOculusAudioGeometryComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ovrGeometry == nullptr)
	{
		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(GetOwner());
		check(Actor != nullptr);

		UStaticMesh* Mesh = Actor->GetStaticMeshComponent()->GetStaticMesh();
		check(Mesh != nullptr);
		check(Mesh->bAllowCPUAccess);

		UploadGeometry();
	}

	check(ovrGeometry != nullptr);

	// 3D transform
	FTransform Transform = GetOwner()->GetTransform();
	if (!Transform.Equals(PreviousTransform))
	{
		// UE:		x:forward, y:right, z:up
		// Oculus:  x:right,   y:up,	z:backward
		FVector Right = Transform.GetScaledAxis(EAxis::Y);
		FVector Up = Transform.GetScaledAxis(EAxis::Z);
		FVector Backward = -Transform.GetScaledAxis(EAxis::X);
		FVector Position = Transform.GetLocation();
		
		float TransformMatrix[16] =
		{
			(float)Right.Y,	(float)Right.Z,	(float)-Right.X,	0.0f,
			(float)Up.Y,	   (float)Up.Z,	   (float)-Up.X,	   0.0f,
			(float)Backward.Y, (float)Backward.Z, (float)-Backward.X, 0.0f,
			(float)Position.Y, (float)Position.Z, (float)-Position.X, 0.0f,
		};
		
		ovrResult Result = OVRA_CALL(ovrAudio_AudioGeometrySetTransform)(ovrGeometry, TransformMatrix);
		if (Result != ovrSuccess) {
			UE_LOG(LogAudio, Warning, TEXT("Failed at setting new audio propagation mesh transform!"));
			return;
		}

		PreviousTransform = Transform;
	}
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
bool UOculusAudioGeometryComponent::UploadGeometry()
{
	ovrAudioContext Context = GetContext();
	if (Context == nullptr)
		return false;

	check(ovrGeometry == nullptr);

	// static mesh actor
	AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(GetOwner());
	check(SMActor != nullptr);

	// actual mesh
	UStaticMesh* Mesh = SMActor->GetStaticMeshComponent()->GetStaticMesh();
	if (Mesh == nullptr)
		return false; //PAS TEMP
	check(Mesh != nullptr);

	// LB: silently filter out invisible geometry (might be used for something else)
	if (!Mesh->HasValidRenderData()) {
		UE_LOG(LogAudio, Warning, TEXT("Found static mesh actor with acoustic geometry component while invisible."));
		return false;
	}

	// our internal geometry object into which we'll merge this static mesh actor and all its attached meshes
	ovrResult Result = OVRA_CALL(ovrAudio_CreateAudioGeometry)(Context, &ovrGeometry);
	if (Result != ovrSuccess) {
		UE_LOG(LogAudio, Warning, TEXT("Failed creating acoustic geometry object."));
		return false;
	}

	ovrAudioMaterial ovrMaterial = nullptr;
	// if there is a material component apply that material
	UOculusAudioMaterialComponent* MaterialComponent = static_cast<UOculusAudioMaterialComponent*>(SMActor->GetComponentByClass(UOculusAudioMaterialComponent::StaticClass()));
	if (MaterialComponent)
	{
		Result = OVRA_CALL(ovrAudio_CreateAudioMaterial)(Context, &ovrMaterial);
		check(Result == ovrSuccess);

		MaterialComponent->ConstructMaterial(ovrMaterial);
	}

#if 0 // TODO: map acoustic materials to visual/phyiscal materials
	// The goal here is to build a hash table with unique graphic materials used by the acoustic geometry and automatically mapping them with
	// our corresponding audio materials. A possible alternative is to do the mapping manually by adding a user data field in the UE material
	// definition and manually tagging each graphic material with an audio material in the editor. 
	UStaticMeshComponent* StaticMeshComponent = SMActor->GetStaticMeshComponent();
	for (int x = 0; x < StaticMeshComponent->GetNumMaterials(); ++x) {
		//UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(StaticMeshComponent->GetMaterial(x), nullptr);
		UMaterialInterface* Material = StaticMeshComponent->GetMaterial(x);
		//UE_LOG(LogAudio, Warning, TEXT("Found material: %s"), *Material->GetName());
	}
#endif

	TArray<FVector> MergedVertices;
	TArray<uint32> MergedIndices;
	TArray<ovrAudioMeshGroup> ovrMeshGroups;
	int32 numVerticesCur = 0;

	// root (parent) mesh always
	AppendStaticMesh(Mesh, FTransform(), MergedVertices, MergedIndices, ovrMeshGroups,
		ovrMaterial);

	if (IncludeChildren)
	{
		AActor* Actor = GetOwner();
		AppendChildMeshes(Actor, Actor->GetTransform(), Context,
			MergedVertices, MergedIndices, ovrMeshGroups,
			ovrMaterial);
	}

	ovrAudioMesh ovrMesh = { };
	ovrAudioMeshVertices ovrVertices = { 0 };

	ovrVertices.vertices = MergedVertices.GetData();
	ovrVertices.vertexCount = MergedVertices.Num();
	check(ovrVertices.vertexCount != 0);
	ovrVertices.vertexType = ovrAudioScalarType_Float32;

	ovrMesh.vertices = ovrVertices;

	ovrAudioMeshIndices ovrIndices = { 0 };
	ovrIndices.indices = MergedIndices.GetData();
	ovrIndices.indexCount = MergedIndices.Num();
	check(ovrIndices.indexCount != 0);
	ovrIndices.indexType = ovrAudioScalarType_UInt32;

	ovrMesh.indices = ovrIndices;
	ovrMesh.groups = ovrMeshGroups.GetData();
	ovrMesh.groupCount = ovrMeshGroups.Num();
	check(ovrMesh.groupCount != 0);

	Result = OVRA_CALL(ovrAudio_AudioGeometryUploadMesh)(ovrGeometry, &ovrMesh);
	if (Result != ovrSuccess) {
		UE_LOG(LogAudio, Warning, TEXT("Failed adding geometry to the audio propagation sub-system!"));
	}

	return (Result == ovrSuccess);
}

// ------------------------------------------------------------------------------------------------------------------------------------------------------------
void UOculusAudioGeometryComponent::AppendStaticMesh(UStaticMesh* Mesh,
													 const FTransform& Transform,
													 TArray<FVector>& MergedVertices,
													 TArray<uint32>& MergedIndices,
													 TArray<ovrAudioMeshGroup>& ovrMeshGroups,
													 ovrAudioMaterial ovrMaterial)
{
	const int32 IndexOffset = MergedVertices.Num();
	const int32 FirstIndex = MergedIndices.Num();

	// Lowest LOD
	const FStaticMeshLODResources& Model = Mesh->GetRenderData()->LODResources[0];
	const FPositionVertexBuffer& VertexBuffer = Model.VertexBuffers.PositionVertexBuffer;
	const int32 VertexCount = VertexBuffer.GetNumVertices();

	// append this mesh's vertices to the merged array after coordinate system conversion
	for (int32 Index = 0; Index < VertexCount; Index++) {
		const FVector Vertex = Transform.TransformPosition((FVector)VertexBuffer.VertexPosition(Index));
		MergedVertices.Add(OculusAudioSpatializationAudioMixer::ToOVRVector(Vertex));
	}

	// offset this mesh's vertex indices with the current count within the whole hierarchy
	// PAS: TODO support 16-bit indices Model.IndexBuffer.Is32Bit();
	TArray<uint32> MeshIndices;
	Model.IndexBuffer.GetCopy(MeshIndices);
	const int32 IndexCount = MeshIndices.Num();
	for (int32 Index = 0; Index < IndexCount; Index++) {
		MeshIndices[Index] += IndexOffset;
	}
	
	// append this mesh's indices to the merged array
	MergedIndices += MeshIndices;

	// loop over each section in lowest LOD
	for (int32 Index = 0; Index < Model.Sections.Num(); Index++)
	{
		ovrAudioMeshGroup MeshGroup = { 0 };

		MeshGroup.faceCount		= Model.Sections[Index].NumTriangles;
		MeshGroup.faceType		= ovrAudioFaceType_Triangles;
		MeshGroup.indexOffset	= Model.Sections[Index].FirstIndex + FirstIndex;
		MeshGroup.material		= ovrMaterial; // TODO: material per section

		ovrMeshGroups.Add(MeshGroup);
	}
}

void UOculusAudioGeometryComponent::AppendChildMeshes(AActor* CurrentActor, const FTransform& RootTransform, ovrAudioContext Context,
	TArray<FVector>& MergedVertices, TArray<uint32>& MergedIndices, TArray<ovrAudioMeshGroup>& ovrMeshGroups, 
	ovrAudioMaterial ovrMaterial)
{
	TArray<AActor*> AttachedActors;
	CurrentActor->GetAttachedActors(AttachedActors);

	for (AActor* Actor : AttachedActors)
	{
		AStaticMeshActor* SMActor = Cast<AStaticMeshActor>(Actor);
		UStaticMesh* Mesh = SMActor->GetStaticMeshComponent()->GetStaticMesh();
		check(Mesh != nullptr);

		UOculusAudioGeometryComponent* GeometryComponent = static_cast<UOculusAudioGeometryComponent*>(SMActor->GetComponentByClass(UOculusAudioGeometryComponent::StaticClass()));
		if (GeometryComponent != nullptr)
		{
			// this child has it's own geometry component
			continue;
		}

		// silently filter out invisible attached geometry (might be used for something else)
		// or an attached (child) mesh with no geometry component (acoustically inactive)
		if (!Mesh->HasValidRenderData()) {
			continue;
		}
		
		ovrAudioMaterial ovrChildMaterial = ovrMaterial;

		// if we have a child geometry component with a valid material preset, it overrides the root mesh's material preset (inherited from parent by default).
		UOculusAudioMaterialComponent* ChildMaterial = static_cast<UOculusAudioMaterialComponent*>(SMActor->GetComponentByClass(UOculusAudioMaterialComponent::StaticClass()));
		if (ChildMaterial != nullptr)
		{
			ovrResult Result = OVRA_CALL(ovrAudio_CreateAudioMaterial)(Context, &ovrChildMaterial);
			check(Result == ovrSuccess);

			ChildMaterial->ConstructMaterial(ovrChildMaterial);
		}

		// serialize the transform relative to the root
		FTransform Transform = Actor->GetTransform().GetRelativeTransform(RootTransform);

		// merge this attached static mesh into the root propagation mesh
		AppendStaticMesh(Mesh,
			Transform,
			MergedVertices,
			MergedIndices,
			ovrMeshGroups,
			ovrChildMaterial);

		// traverse children and append those too
		AppendChildMeshes(Actor, RootTransform, Context, MergedVertices, MergedIndices, ovrMeshGroups,
			ovrChildMaterial);
	}
}

ovrAudioContext UOculusAudioGeometryComponent::GetContext(UWorld* World)
{
	if (CachedContext == nullptr)
	{
		ovrAudioContext PluginContext = FOculusAudioLibraryManager::Get().GetPluginContext();
		if (PluginContext != nullptr)
		{
			CachedContext = PluginContext;
		}
		else
		{
			FOculusAudioPlugin* Plugin = &FModuleManager::GetModuleChecked<FOculusAudioPlugin>("OculusAudio");
			check(Plugin != nullptr);

			// Note: getting the context from the spatializer, should we get it from the reverb instead?
			FString OculusSpatializerPluginName = Plugin->GetSpatializationPluginFactory()->GetDisplayName();
			FString CurrentSpatializerPluginName = AudioPluginUtilities::GetDesiredPluginName(EAudioPlugin::SPATIALIZATION);
			if (CurrentSpatializerPluginName.Equals(OculusSpatializerPluginName)) // we have a match!
			{
				if (World == nullptr)
				{
					World = GetOwner()->GetWorld();
				}
				if (World == nullptr)
				{
					// couldn't get a World
					return nullptr;
				}

				FAudioDevice* AudioDevice = World->GetAudioDevice().GetAudioDevice();
				if (AudioDevice == nullptr)
				{
					// This happens when cooking for native UE AudioMixer integration
					CachedContext = FOculusAudioContextManager::GetOrCreateSerializationContext(this);
				}
				else
				{
					CachedContext = FOculusAudioContextManager::GetContextForAudioDevice(AudioDevice);

					if (!CachedContext)
					{
						CachedContext = FOculusAudioContextManager::CreateContextForAudioDevice(AudioDevice);
					}
				}

				OculusAudioSpatializationAudioMixer* Spatializer = static_cast<OculusAudioSpatializationAudioMixer*>(AudioDevice->GetSpatializationPluginInterface().Get());
				if (Spatializer == nullptr || Spatializer->ClassID != OculusAudioSpatializationAudioMixer::MIXER_CLASS_ID)
				{
					UE_LOG(LogAudio, Warning, TEXT("Invalid Spatialization Plugin specified, make sure the Spatialization Plugin is set to OculusAudio and AudioMixer is enabled!"));
					return nullptr;
				}
			}
		}
	}

	return CachedContext;
}

void UOculusAudioGeometryComponent::Serialize(FArchive & Ar)
{
	// Tell the archive we are using a custom version.
	Ar.UsingCustomVersion( UOculusAudioGeometryGUID );

	Super::Serialize(Ar);

	struct Delta {
		static size_t Read(void* userData, void* bytes, size_t byteCount) {
			FArchive* Archive = static_cast<FArchive*>(userData);
			check(Archive->IsLoading());
			int64 CurrentPosition = Archive->Tell();
			int64 TotalSize = Archive->TotalSize();
			if ((CurrentPosition + static_cast<int64>(byteCount)) > TotalSize)
			{
				// Note: After copy/paste/undo TotalSize returns -1 and Serialize fails
				return 0;
			}

			Archive->Serialize(bytes, byteCount);
			return Archive->GetError() ? 0 : byteCount;
		}
		static size_t Write(void* userData, const void* bytes, size_t byteCount) {
			FArchive* Archive = static_cast<FArchive*>(userData);
			check(Archive->IsSaving());
			Archive->Serialize(const_cast<void*>(bytes), byteCount);
			return Archive->GetError() ? 0 : byteCount;
		}
		static int64_t Seek(void* userData, int64_t seekOffset) {
			FArchive* Archive = static_cast<FArchive*>(userData);
			int64 Start = Archive->Tell();
			Archive->Seek(seekOffset);
			return 0;
		}
	};

	ovrAudioSerializer Serializer;
	Serializer.read = Delta::Read;
	Serializer.write = Delta::Write;
	Serializer.seek = Delta::Seek;
	Serializer.userData = &Ar;

	if (Ar.IsSaving())
	{
		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(GetOwner());
		if (Actor == nullptr)
			return; // this happens when adding blueprint to actor with component already attached

		UStaticMeshComponent* MeshComponent = Actor->GetStaticMeshComponent();
		if (MeshComponent == nullptr) // this happens with actor copy/paste/delete
			return;

		UStaticMesh* Mesh = MeshComponent->GetStaticMesh();

		// LB 8/1/18: when duplicating static mesh actors in the editor, we get here once with a NULL Mesh ptr, not sure why (could be that the original and the
		// dupe instances share the (single) actual mesh data and a serialization callback happens before the dupe has updated the mesh ptr?)
		//check(Mesh != nullptr);
		if (Mesh == nullptr) 
			return;

		// if we have anything to save, it means the content changed for this component in the editor so we need to do the full export from scratch
		// but 1st cleanup any existing (stale) data:
		if (ovrGeometry != nullptr) {
			ovrResult Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometry);
			check(Result == ovrSuccess);
			ovrGeometry = nullptr;
		}

		// upload the new geometry and material preset	
		UploadGeometry();

#if 0 // PAS TEMP
		{
			char path[256] = { 0 };
			static int counter = 0;
			sprintf(path, "G:/TEMP/Propagation/MeshDump/%s-%d.obj", TCHAR_TO_ANSI(*Mesh->GetName()), counter++);
			ovrResult Result = OVRA_CALL(ovrAudio_AudioGeometryWriteMeshFileObj)(ovrGeometry, path);
			check(Result == ovrSuccess);
		}
#endif
#if 0 // PAS TEMP
		{
			char path[256] = { 0 };
			static int counter = 0;
			sprintf(path, "G:/Dev/Oculus/depot/Software/AudioSDK/Main/Tests/PropagTestData/%s-%d.geo", TCHAR_TO_ANSI(*Mesh->GetName()), counter++);
			ovrResult Result = OVRA_CALL(ovrAudio_AudioGeometryWriteMeshFile)(ovrGeometry, path);
			check(Result == ovrSuccess);
		}
#endif

		// now save cooked data
		if (ovrGeometry != nullptr) {
			ovrResult Result = OVRA_CALL(ovrAudio_AudioGeometryWriteMeshData)(ovrGeometry, &Serializer);
			check(Result == ovrSuccess);

			Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometry);
			check(Result == ovrSuccess);
			ovrGeometry = nullptr;
		}
	}
	else if (Ar.IsLoading())
	{
		// Read the file data so unreal doesn't complain that we didn't read all the data if the version differs.
		ovrAudioContext Context = FOculusAudioContextManager::GetOrCreateSerializationContext(this);
		ovrResult Result = OVRA_CALL(ovrAudio_CreateAudioGeometry)(Context, &ovrGeometry);
		check(Result == ovrSuccess);

		int64 PreviousPosition = Ar.Tell();
		Result = OVRA_CALL(ovrAudio_AudioGeometryReadMeshData)(ovrGeometry, &Serializer);
		if (Result != ovrSuccess)
		{
			Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometry);
			check(Result == ovrSuccess);

			ovrGeometry = nullptr;
			Ar.Seek(PreviousPosition);
		}
		
		// If the version number is out of date, upload the mesh from the scene.
		int32 customVersion = Ar.CustomVer( UOculusAudioGeometryGUID );
		if (ovrGeometry != nullptr && customVersion != OVRA_AUDIO_GEOMETRY_COMPONENT_LATEST_VERSION)
		{
			// if we have anything to save, it means the content changed for this component in the editor so we need to do the full export from scratch
			// but 1st cleanup any existing (stale) data:
			if (ovrGeometry != nullptr) {
				Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometry);
				check(Result == ovrSuccess);
				ovrGeometry = nullptr;
			}

			// upload the new geometry and material preset	
			UploadGeometry();
		}
	}
}

void UOculusAudioGeometryComponent::PostLoad()
{
	Super::PostLoad();

#if WITH_EDITOR
	if (ovrGeometry != nullptr)
	{
		AActor* Actor = GetOwner();
		if (Actor != nullptr)
		{
			UWorld* World = Actor->GetWorld();
			if (World != nullptr)
			{
				if (World->WorldType != EWorldType::Game &&
					World->WorldType != EWorldType::PIE)
				{
					ovrResult Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometry);
					check(Result == ovrSuccess);
					ovrGeometry = nullptr;
				}
			}
		}
	}
#endif
}

void UOculusAudioGeometryComponent::BeginDestroy()
{
	Super::BeginDestroy();

	if (ovrGeometry != nullptr && CachedContext != nullptr)
	{
		ovrResult Result = OVRA_CALL(ovrAudio_DestroyAudioGeometry)(ovrGeometry);
		check(Result == ovrSuccess);
		ovrGeometry = nullptr;
	}
}

#if COMPILE_AS_DEBUG_MODE
#  pragma optimize("", on) //PAS - TEMP - DELETE THIS - better debugging
#endif
