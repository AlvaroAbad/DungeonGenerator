// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DungeonMapperData.h"
#include "GameFramework/Actor.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "DungeonRoom.generated.h"

class UDynamicMesh;
class UDynamicMeshPool;
class UDynamicMeshComponent;
struct FDungeonNode;

UCLASS()
class ADungeonRoom : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ADungeonRoom();
	void InitializeRoom(const UDungeonRoomData* Node);
	void CreateDoors(UDynamicMesh*& DynMesh, const UDungeonRoomData* DungeonsNode, TArray<UMaterialInterface*>& OutMaterialList);

private:
	/** Access the compute mesh pool */
	UDynamicMeshPool* GetComputeMeshPool();
	/** Request a compute mesh from the Pool, which will return a previously-allocated mesh or add and return a new one. If the Pool is disabled, a new UDynamicMesh will be allocated and returned. */
	UDynamicMesh* AllocateComputeMesh();
	/** Release a compute mesh back to the Pool */
	bool ReleaseComputeMesh(UDynamicMesh* Mesh);
private:
	UPROPERTY(Category = "Dungeon Mapper", VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;
	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshPool> DynamicMeshPool;
	FGeometryScriptSimpleCollision DungeonCollision;
};
