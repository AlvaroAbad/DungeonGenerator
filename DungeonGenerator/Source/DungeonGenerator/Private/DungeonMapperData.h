// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DungeonMapperData.generated.h"

struct FDungeonDoor;
struct FDungeonConnection;

UENUM()
enum class ERoomType : uint8
{
	Starting,
	Mid,
	End,
	Dummy,
};

UENUM(BlueprintType)
enum class ECorridorType : uint8
{
	HStraight,
	HCorner,
	StairConnection,
	Stairs,
	HCross,
	VCross,
	HRoomConnection,
	VRoomConnection,
	Count UMETA(Hidden)
};

UCLASS(Blueprintable, BlueprintType)
class UDungeonRoomData : public UDataAsset
{
	GENERATED_BODY()

public:
	FVector Location;
	FVector Extent;
	ERoomType RoomType = ERoomType::Mid;
	TArray<const FDungeonConnection*> Connections;
	TArray<FDungeonDoor> Doors;

	FVector Velocity = FVector::ZeroVector;
	FVector PrevLocation = FVector::ZeroVector;
	UPROPERTY(EditDefaultsOnly)
	float WallThickness = 0;
	UPROPERTY(EditDefaultsOnly)
	UStaticMesh* DoorMesh = nullptr;
	UPROPERTY(EditDefaultsOnly)
	UMaterialInterface* WallMaterial;

	bool operator==(const UDungeonRoomData* Other) const
	{
		return Location.Equals(Other->Location) && Extent.Equals(Other->Extent);
	}
};

USTRUCT(BlueprintType)
struct FDungeonDoor
{
	GENERATED_BODY()
	FTransform Transform = FTransform::Identity;
	UDungeonRoomData* ConnectingRoom[2]{nullptr, nullptr};
	FDungeonDoor()
	{}
	
	FDungeonDoor(const FTransform& T, UDungeonRoomData* Room1 = nullptr, UDungeonRoomData* Room2 = nullptr)
		: Transform(T)
		{
			ConnectingRoom[0] = Room1;
			ConnectingRoom[1] = Room2;
		}
};

UCLASS(Blueprintable, BlueprintType)
class UDungeonHallwayData : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere)
	float WallThickness = 0.0f;
	UPROPERTY(EditAnywhere)
	float StepRise = 20.0f;
	//X - Width. Y - Height
	UPROPERTY(EditAnywhere)
	FVector2D HallWaySectionDimensions = FVector2D::ZeroVector;

	UPROPERTY(EditDefaultsOnly)
	UMaterialInterface* WallMaterial;

	FVector Start;
	FVector End;
	FVector Direction;
	ECorridorType Type;
	bool bIsInvalid = false;
	
	bool operator==(const UDungeonHallwayData* Other) const
	{
		const float MarginRadius = FMath::Sqrt(HallWaySectionDimensions.X * HallWaySectionDimensions.X + HallWaySectionDimensions.Y * HallWaySectionDimensions.Y);
		const bool StartPointsMatch = FVector::PointsAreNear(Start, Other->Start, MarginRadius);
		const bool EndPointsMatch = FVector::PointsAreNear(End, Other->End, MarginRadius);
		const bool StartEndPointsMatch = FVector::PointsAreNear(Start, Other->End, MarginRadius);
		const bool EndStartPointsMatch = FVector::PointsAreNear(End, Other->Start, MarginRadius);

		return (StartPointsMatch && EndPointsMatch) || (StartEndPointsMatch && EndStartPointsMatch);
	}
};

USTRUCT(BlueprintType)
struct FDungeonConnection
{
	GENERATED_BODY()

	UPROPERTY()
	UDungeonRoomData* StartRoom;
	UPROPERTY()
	UDungeonRoomData* EndRoom;

	void GetWallConnectionPoints(FVector& out_Point1, FVector& out_Point2) const;
	FDungeonConnection()
	: StartRoom(nullptr)
	, EndRoom(nullptr)
	{
		
	}

	FDungeonConnection(const FVector& InStartPoint, UDungeonRoomData* InStartRoom, const FVector& InEndPoint, UDungeonRoomData* EndRoom)
	: StartRoom(InStartRoom)
	, EndRoom(EndRoom)
	{}
	
	bool operator==(const FDungeonConnection& Other) const
	{
		const bool StartRoomDup = StartRoom == Other.StartRoom || StartRoom == Other.EndRoom;
		const bool EndRoomDup = EndRoom == Other.StartRoom || EndRoom == Other.EndRoom;
		return StartRoomDup && EndRoomDup;
	}
};