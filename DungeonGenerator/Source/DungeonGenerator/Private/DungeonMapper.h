// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DungeonMapperData.h"
#include "TriangulatorData.h"
#include "GameFramework/Actor.h"
#include "GeometryScript/GeometryScriptTypes.h"
#include "DungeonMapper.generated.h"


class ADungeonRoom;
class UTextRenderComponent;
class UDynamicMeshPool;
class UDynamicMesh;
class UDynamicMeshComponent;
class UProceduralMeshComponent;
struct FDungeonNode;
struct FDungeonConnection;
class ANavMeshBoundsVolume;
class UDungeonHallwayPathFinder;

DECLARE_LOG_CATEGORY_EXTERN(LogDungeonGenerator, Log, All);

// PROFILER INTEGRATION //
DECLARE_STATS_GROUP(TEXT("Procedural Dungeon"), STATGROUP_ProcDungeon, STATCAT_DungeonMapper);

DECLARE_CYCLE_STAT_EXTERN(TEXT("Dungeon Mapper / Generate Rooms"), STAT_GenerateRooms, STATGROUP_ProcDungeon, DUNGEONGENERATOR_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dungeon Mapper / Connect Rooms"), STAT_ConnectRooms, STATGROUP_ProcDungeon, DUNGEONGENERATOR_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dungeon Mapper / Simplify Connections"), STAT_SimplifyConections, STATGROUP_ProcDungeon, DUNGEONGENERATOR_API);
DECLARE_CYCLE_STAT_EXTERN(TEXT("Dungeon Mapper / Render Dungeon"), STAT_RenderDungeon, STATGROUP_ProcDungeon, DUNGEONGENERATOR_API);

UENUM(blueprintType)
enum class EHallwayGenerationMethod : uint8
{
	Basic,
	PathFinding
};
USTRUCT()
struct FDungeonPath
{
	GENERATED_BODY()
	UPROPERTY()
	UDungeonRoomData* Start = nullptr;
	UPROPERTY()
	UDungeonRoomData* End = nullptr;
	TArray<const FDungeonConnection*> Path;
	float DistanceTraveled = 0;
	void AssignPath(TArray<const FDungeonConnection*>& NewPath)
	{
		Path = NewPath;
		for (const FDungeonConnection* HallWay : Path)
		{
			DistanceTraveled+= FVector::Dist(HallWay->EndRoom->Location, HallWay->StartRoom->Location);
		}
	}
};


USTRUCT()
struct FHallWayPath
{
	GENERATED_BODY()
	FVector Start = FVector::ZeroVector;
	FVector End = FVector::ZeroVector;
	TArray<FVector> Path;
	float DistanceTraveled = 0;
	void AssignPath(TArray<FVector>& NewPath)
	{
		DistanceTraveled = 0;
		Path = NewPath;
		for (int i = 0; i < Path.Num() - 1; ++i)
		{
			DistanceTraveled += FVector::Dist(Path[i + 1], Path[i]);
		}
	}
};

struct FPathNode
{
	UDungeonRoomData* Room = nullptr;
	TArray<const FDungeonConnection*> Path;
	float G = 0;
	float F = 0;
	float H = 0;
	FPathNode(UDungeonRoomData* InRoom)
		: Room(InRoom)
	{}

	FPathNode(const FPathNode& Node,const FDungeonConnection* InHallway)
	{
		if(InHallway->StartRoom != Node.Room)
		{
			Room = InHallway->StartRoom;
		}
		else if(InHallway->EndRoom != Node.Room)
		{
			Room = InHallway->EndRoom;
		}
		for (const FDungeonConnection* Hallway : Node.Path)
		{
			Path.Add(Hallway);
		}
		Path.Add(InHallway);
	}
		
	bool operator==(const FPathNode& Other) const
	{
		return Room == Other.Room;
	}

	bool operator<(const FPathNode& Other) const
	{
		return F > Other.F;
	}
};

struct FHallWayPathNode
{
	FVector NodeLocation;
	TArray<FVector> Path;
	float G = 0;
	float F = 0;
	float H = 0;
	FHallWayPathNode(FVector InNodeLocation)
		: NodeLocation(InNodeLocation)
	{
		Path.Add(NodeLocation);
	}

	FHallWayPathNode(const FHallWayPathNode& Node, FVector InNodeLocation)
	{
		NodeLocation = InNodeLocation;
		for (const FVector& PathNode : Node.Path)
		{
			Path.Add(PathNode);
		}
		Path.Add(InNodeLocation);
	}
		
	bool operator==(const FHallWayPathNode& Other) const
	{
		return NodeLocation.Equals(Other.NodeLocation);
	}

	bool operator<(const FHallWayPathNode& Other) const
	{
		return F > Other.F;
	}
};

UCLASS(BlueprintType, Blueprintable)
class DUNGEONGENERATOR_API ADungeonMapper : public AActor
{
	GENERATED_BODY()

public:
	ADungeonMapper(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	virtual bool ShouldTickIfViewportsOnly() const override { return true; };

	virtual void Tick(float DeltaSeconds) override;

	//Override - AActor - START
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	//Override - AActor - END
private:
	void RunHallwaysCreation();
	void Debug();
	FTransform GenerateDoorOnRoomWall(FRandomStream RandomStream, const FVector& RoomSize, const FVector& DoorForward, const FVector& SlidingVector) const;
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dungeon Mapper|Generators")
	void GenerateDungeonRooms();
	UDungeonHallwayData* CreateConnectionFromEdgePoint(UDungeonRoomData* ConnectedRoom, FVector Start, FVector End);
	void FindNextOrphanDoor();
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dungeon Mapper|Generators")
	void CreateHallways();
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dungeon Mapper|Generators")
	void RenderDungeon();
	UFUNCTION(CallInEditor, BlueprintCallable, Category = "Dungeon Mapper|Generators")
	void ClearAll();

	//Room Creation
	UDungeonRoomData* CreateRoom(FRandomStream& RandomStream);

	//Hallway Creation
	UDungeonHallwayData* FixHallwayCrossingRoom(UDungeonRoomData* Room, const FVector& Start, const FVector& End);
	void CreateHallwaysFromPath(const TArray<FVector>& Path);
	
	//Rendering
	void RenderHallWays(UDynamicMesh* DynMesh, const UDungeonHallwayData* DungeonHallway);
	void HallowHallWays(UDynamicMesh* DynamicMesh, const UDungeonHallwayData* DungeonHallway);
	
	/** Access the compute mesh pool */
	UDynamicMeshPool* GetComputeMeshPool();
	/** Request a compute mesh from the Pool, which will return a previously-allocated mesh or add and return a new one. If the Pool is disabled, a new UDynamicMesh will be allocated and returned. */
	UDynamicMesh* AllocateComputeMesh();
	/** Release a compute mesh back to the Pool */
	bool ReleaseComputeMesh(UDynamicMesh* Mesh);
public:
	UPROPERTY(EditAnywhere, meta = (ClampMin="0"), category = "Dungeon Mapper|Generation")
	int32 MinRooms;
	UPROPERTY(EditAnywhere, meta = (ClampMin="0"), category = "Dungeon Mapper|Generation")
	int32 MaxRooms;
	//Half depth of a room
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation", meta = (ClampMin = "0", UIMin = "0"))
	FVector2D RoomXExtent;
	//half width of a room
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation", meta = (ClampMin = "0", UIMin = "0"))
	FVector2D RoomYExtent;
	//half height of a room
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation", meta = (ClampMin = "0", UIMin = "0"))
	FVector2D RoomZExtent;
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation")
	FName RandomSeed;
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation")
	float MaxHallwaySlope = 45.0f;
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation")
	EHallwayGenerationMethod HallWayGenerationMethod = EHallwayGenerationMethod::Basic;
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation Data")
	UDungeonRoomData* RoomData;
	UPROPERTY(EditAnywhere, category = "Dungeon Mapper|Generation Data")
	UDungeonHallwayData* HallwayData;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	bool bShowRooms;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	bool bShowHallways;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	bool bShowBounds;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	bool bPreventCrossing;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	bool bCreateCorners;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	bool bHallwayToRoomConnection;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	TMap<ECorridorType, FColor> HallwayDebugColors;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, category = "Dungeon Mapper|Debug")
	float TickInterval = 0.0f;

protected:
	
	UPROPERTY()
	TArray<UDungeonRoomData*> DungeonNodes;
	UPROPERTY()
	TArray<ADungeonRoom*> DungeonRooms;
	UPROPERTY()
	TArray<UDungeonHallwayData*> DungeonHallwaysData;
	FBox DungeonBounds;
	
	UPROPERTY(Category = "Dungeon Mapper", VisibleAnywhere, BlueprintReadOnly, meta = (ExposeFunctionCategories = "Mesh,Rendering,Physics,Components|StaticMesh", AllowPrivateAccess = "true"))
	TObjectPtr<UDynamicMeshComponent> DynamicMeshComponent;

	UPROPERTY(Transient)
	TObjectPtr<UDynamicMeshPool> DynamicMeshPool;
	FGeometryScriptSimpleCollision DungeonCollision;

	//HallCreation Variables
	bool bIsCreatingHallways = false;
	int32 RoomIdx = 0;
	int32 DoorIdx = 0;
	
	UPROPERTY()
	UDungeonHallwayPathFinder* DungeonHallwayPathFinder = nullptr;

};