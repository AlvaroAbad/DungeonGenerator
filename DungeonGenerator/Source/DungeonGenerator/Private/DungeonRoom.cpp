// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonRoom.h"

#include "DungeonMapperData.h"
#include "UDynamicMesh.h"
#include "Components/DynamicMeshComponent.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshMaterialFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "GeometryScript/MeshQueryFunctions.h"
#include "GeometryScript/MeshTransformFunctions.h"

// Sets default values
ADungeonRoom::ADungeonRoom()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
	
	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	DynamicMeshComponent->SetCollisionProfileName("BlockAll");
	RootComponent = DynamicMeshComponent;
}

void ADungeonRoom::InitializeRoom(const UDungeonRoomData* Node)
{
	UDynamicMesh* MainDynMesh = DynamicMeshComponent->GetDynamicMesh();
	MainDynMesh->Reset();
	TArray<UMaterialInterface*> MaterialList;
	MaterialList.Add(Node->WallMaterial);
	
	FGeometryScriptPrimitiveOptions PrimitiveOptions;
	MainDynMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
	(
	MainDynMesh
		, PrimitiveOptions
		, FTransform::Identity
		, Node->Extent.X*2.0f
		, Node->Extent.Y*2.0f
		,  Node->Extent.Z*2.0f
		, Node->Extent.X*2.0f/125 + 1, Node->Extent.Y*2.0f/125 + 1, Node->Extent.Z*2.0f/125 + 1
		, EGeometryScriptPrimitiveOriginMode::Center
	);
	
	const FTransform FloorLocation(FVector::ZeroVector - FVector(0,0,Node->Extent.Z - Node->WallThickness * 0.5f));
	FKBoxElem FloorShape(Node->Extent.X, Node->Extent.Y, Node->WallThickness);
	FloorShape.SetTransform(FloorLocation);
	DungeonCollision.AggGeom.BoxElems.Add(FloorShape);

	// FGeometryScriptMergeSimpleCollisionOptions MergeOptions;
	// bool bHasMerged = false;
	// DungeonCollision = UGeometryScriptLibrary_CollisionFunctions::MergeSimpleCollisionShapes(DungeonCollision, MergeOptions, bHasMerged);
	// UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(DungeonCollision, DynamicMeshComponent, Options);

	//make space inside
	UDynamicMesh* ToolMesh = AllocateComputeMesh();
	ToolMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
	(
		ToolMesh
		, PrimitiveOptions
		, FTransform::Identity*FTransform(FVector::UpVector*Node->WallThickness)
		, Node->Extent.X*2.0f - Node->WallThickness * 2.0f
		, Node->Extent.Y*2.0f - Node->WallThickness * 2.0f
		,  Node->Extent.Z*2.0f
		, Node->Extent.X*2.0f/125 + 1, Node->Extent.Y*2.0f/125 + 1, Node->Extent.Z*2.0f/125 + 1
		, EGeometryScriptPrimitiveOriginMode::Center
	);
	
	FGeometryScriptMeshBooleanOptions BoolOptions;
	MainDynMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
	(
		MainDynMesh,
		 FTransform::Identity,
		ToolMesh,
		 FTransform::Identity,
		EGeometryScriptBooleanOperation::Subtract,
		BoolOptions		
	);
	ReleaseComputeMesh(ToolMesh);
	CreateDoors(MainDynMesh, Node, MaterialList);
	DynamicMeshComponent->ConfigureMaterialSet(MaterialList);
}

void ADungeonRoom::CreateDoors(UDynamicMesh*& DynMesh, const UDungeonRoomData* DungeonsNode, TArray<UMaterialInterface*>& OutMaterialList)
{
	for (const FTransform OriginalDoorTransform : DungeonsNode->Doors)
	{
		FTransform DoorTransform = OriginalDoorTransform.GetRelativeTransform(GetActorTransform());
		if(DoorTransform.GetUnitAxis(EAxis::X).GetAbs().Equals(FVector::UpVector))
		{
			continue;
		}

		UDynamicMesh* ToolMesh = AllocateComputeMesh();
		UStaticMesh* DoorMesh = DungeonsNode->DoorMesh;

		FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
		FGeometryScriptMeshReadLOD RequestedLOD;
		EGeometryScriptOutcomePins Outcome;
	
		ToolMesh = UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(
			DoorMesh,
			ToolMesh,
			AssetOptions,
			RequestedLOD,
			Outcome
		);
		FBox DoorBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(ToolMesh);
		ToolMesh = UGeometryScriptLibrary_MeshTransformFunctions::TranslatePivotToLocation(ToolMesh, DoorBounds.Min + FVector(DoorBounds.GetExtent().X, DoorBounds.GetExtent().Y, 0.0f));
		DoorBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(ToolMesh);

		//Scale door to fit wall
		float DoorHeight = DoorBounds.GetExtent().Z * 2.0f;
		float RoomWallHeight = DungeonsNode->Extent.Z * 2.0f - DungeonsNode->WallThickness * 2.0f;
		float ScaleRate = DoorHeight > RoomWallHeight ? RoomWallHeight / DoorHeight : 1.0f;
		ToolMesh = UGeometryScriptLibrary_MeshTransformFunctions::ScaleMesh(ToolMesh, FVector(ScaleRate));

		//Adjust Door mesh to wall(center height, and scale to fit)
		FVector DoorForward = DoorTransform.GetUnitAxis(EAxis::X);
		if(DoorForward.Equals(FVector::ForwardVector)  || DoorForward.Equals(FVector::BackwardVector) || DoorForward.Equals(FVector::RightVector) || DoorForward.Equals(FVector::LeftVector))
		{
			FVector Translation(-DungeonsNode->WallThickness*0.5f, 0.0f, -RoomWallHeight * 0.5);
			Translation = DoorTransform.TransformVector(Translation);
			DoorTransform.AddToTranslation(Translation);
		}

		//poke a hole through the wall for the door
		UDynamicMesh* BooleanMesh = AllocateComputeMesh();
		constexpr FGeometryScriptPrimitiveOptions PrimitiveOptions;
		DoorBounds = UGeometryScriptLibrary_MeshQueryFunctions::GetMeshBoundingBox(ToolMesh);
		BooleanMesh =  UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
					(
						BooleanMesh
						, PrimitiveOptions
						, DoorTransform
						, (DoorBounds.GetExtent().X + DungeonsNode->WallThickness) * 2.0f
						, DoorBounds.GetExtent().Y * 2.0f
						,  DoorBounds.GetExtent().Z * 2.0f
						, 0, 0, 0
						, EGeometryScriptPrimitiveOriginMode::Base
					);
		
		FGeometryScriptMeshBooleanOptions BoolOptions;
		DynMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
				(
					DynMesh,
					FTransform::Identity,
					BooleanMesh,
					FTransform::Identity,
					EGeometryScriptBooleanOperation::Subtract,
					BoolOptions		
				);
		
		ReleaseComputeMesh(BooleanMesh);
		// Finished poking a hole
		
		const TArray<FStaticMaterial> DoorMaterials = DoorMesh->GetStaticMaterials();
		TArray<UMaterialInterface* > OldMaterialList;
		for (int MaterialIdx = 0; MaterialIdx < DoorMaterials.Num(); ++MaterialIdx)
		{
			UMaterialInterface* DoorMaterial = DoorMaterials[MaterialIdx].MaterialInterface;
			OldMaterialList.Add(DoorMaterial);
			if(!OutMaterialList.Contains(DoorMaterial))
			{
				OutMaterialList.Add(DoorMaterial);
			}
		}
		ToolMesh = UGeometryScriptLibrary_MeshMaterialFunctions::RemapToNewMaterialIDsByMaterial(ToolMesh, OldMaterialList, OutMaterialList);
		DynMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
				(
					DynMesh,
					FTransform::Identity,
					ToolMesh,
					DoorTransform,
					EGeometryScriptBooleanOperation::Union,
					BoolOptions		
				);
		ReleaseComputeMesh(ToolMesh);
	}
}

UDynamicMeshPool* ADungeonRoom::GetComputeMeshPool()
{
	if (DynamicMeshPool == nullptr)
	{
		DynamicMeshPool = NewObject<UDynamicMeshPool>();
	}
	return DynamicMeshPool;
}

UDynamicMesh* ADungeonRoom::AllocateComputeMesh()
{
	if (UDynamicMeshPool* UsePool = GetComputeMeshPool())
	{
		return UsePool->RequestMesh();
	}
	return NewObject<UDynamicMesh>();
}

bool ADungeonRoom::ReleaseComputeMesh(UDynamicMesh* Mesh)
{
	if (Mesh)
	{
		UDynamicMeshPool* UsePool = GetComputeMeshPool();
		if (UsePool != nullptr)
		{
			UsePool->ReturnMesh(Mesh);
			return true;
		}
	}
	return false;
}
