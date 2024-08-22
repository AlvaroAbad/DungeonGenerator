// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonMapper.h"

#include "DungeonGenerationHelperFunctions.h"
#include "DungeonGenerator.h"
#include "DungeonPathFinder.h"
#include "DungeonRoom.h"
#include "GeometryScriptLibrary_DungeonGenerationFunctions.h"
#include "NavigationSystem.h"
#include "GeometryScript/CollisionFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Runtime/GeometryFramework/Public/Components/DynamicMeshComponent.h"

DEFINE_LOG_CATEGORY(LogDungeonGenerator);

// PROFILER INTEGRATION //
DEFINE_STAT(STAT_GenerateRooms);
DEFINE_STAT(STAT_ConnectRooms);
DEFINE_STAT(STAT_SimplifyConections);
DEFINE_STAT(STAT_RenderDungeon);
ENUM_RANGE_BY_COUNT(ECorridorType, ECorridorType::Count)

ADungeonMapper::ADungeonMapper(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer),
	MinRooms(0),
	MaxRooms(0),
	bShowRooms(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;
	PrimaryActorTick.TickInterval = TickInterval;
	
	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	RootComponent = DynamicMeshComponent;


	for (ECorridorType Type : TEnumRange<ECorridorType>())
	{
		HallwayDebugColors.Add(Type, FColor::Orange);
	}

	DungeonHallwayPathFinder = NewObject<UDungeonHallwayPathFinder>(this, TEXT("HallWayPathFinder"));
}

void ADungeonMapper::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RunHallwaysCreation();
	Debug();
}

void ADungeonMapper::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	
	static const FName Name_TickInterval = GET_MEMBER_NAME_CHECKED(ADungeonMapper, TickInterval);
	
	FProperty* MemberPropertyThatChanged = PropertyChangedEvent.MemberProperty;
	const FName MemberPropertyName = MemberPropertyThatChanged != NULL ? MemberPropertyThatChanged->GetFName() : NAME_None;
	if(MemberPropertyName == Name_TickInterval)
	{
		PrimaryActorTick.TickInterval = TickInterval;
	}	
}

void ADungeonMapper::SimplifyHallways()
{
	//Merge Hallways that fallow the same path
	MergeHallways();
	
	if(bCreateCorners)
	{
		CreateCornerHallways();
	}
}

void ADungeonMapper::RunHallwaysCreation()
{
	if(bIsCreatingHallways)
	{
		if(ConnectingStartDoor == nullptr || ConnectingEndDoor == nullptr)
		{
			bIsCreatingHallways = false;
			return;
		}
		
		if(DungeonHallwayPathFinder->Evaluate())
		{
			if(!DungeonHallwayPathFinder->PathResult.IsEmpty())
			{
				for (int i = 0; i <DungeonHallwayPathFinder->PathResult.Num() - 1; ++i)
				{
					UDungeonHallwayData* NewHallway = DuplicateObject(HallwayData, this);
					NewHallway->Start = DungeonHallwayPathFinder->PathResult[i];
					NewHallway->End = DungeonHallwayPathFinder->PathResult[i + 1];
					NewHallway->Direction = (NewHallway->End - NewHallway->Start).GetSafeNormal();
					const bool IsStairs = (NewHallway->End.Z - NewHallway->Start.Z) != 0;
					NewHallway->Type = IsStairs ? ECorridorType::Stairs : ECorridorType::HStraight;
			
					DungeonHallwaysData.AddUnique(NewHallway);
				}
				
				ConnectingStartDoor->ConnectingRoom[1] = ConnectingEndDoor->ConnectingRoom[0];
				ConnectingEndDoor->ConnectingRoom[1] = ConnectingStartDoor->ConnectingRoom[0];
				ConnectingEndDoor = nullptr;
				ConnectingStartDoor = nullptr;
			}
			
			if(DoorsToconnect.Num() < 2)
			{
				ConnectingEndDoor = nullptr;
				ConnectingStartDoor = nullptr;
				SimplifyHallways();
				return;
			}

			if(ConnectingStartDoor != nullptr || ConnectingEndDoor != nullptr)
			{
				DoorsToconnect.Add(ConnectingStartDoor);
				DoorsToconnect.Add(ConnectingEndDoor);				
			}
			
			StartDoorIdx = RandomStream.RandRange(0, DoorsToconnect.Num() - 1);
			ConnectingStartDoor =DoorsToconnect[StartDoorIdx];
			DoorsToconnect.RemoveAt(StartDoorIdx);
			EndDoorIdx = RandomStream.RandRange(0, DoorsToconnect.Num() - 1);
			ConnectingEndDoor = DoorsToconnect[EndDoorIdx];
			DoorsToconnect.RemoveAt(EndDoorIdx);
			
			FVector StartLocation = (ConnectingStartDoor->Transform*FTransform(ConnectingStartDoor->ConnectingRoom[0]->Location)).GetLocation();
			FVector EndDoorLocation = (ConnectingEndDoor->Transform*FTransform(ConnectingEndDoor->ConnectingRoom[0]->Location)).GetLocation();
			DungeonHallwayPathFinder->Initialize(StartLocation, EndDoorLocation);
			DungeonHallwayPathFinder->FillAdditionalValidConnectionDirections();
		}
	}
}

void ADungeonMapper::Debug()
{
	if(bShowRooms)
	{
		for (const UDungeonRoomData* DungeonsRoom : DungeonNodes)
		{
			FColor DungeonColor;
			switch (DungeonsRoom->RoomType) {
			case ERoomType::Starting:
				DungeonColor = FColor::Green;
				break;
			case ERoomType::Mid:
				DungeonColor = FColor::Purple;
				break;
			case ERoomType::End:
				DungeonColor = FColor::Red;
				break;
			default:
				DungeonColor = FColor::White;
			}
			DrawDebugBox(GetWorld(), DungeonsRoom->Location, DungeonsRoom->Extent,DungeonColor, false, TickInterval, 0, 2);
			for (const FBox& SubtractiveElement : DungeonsRoom->SubtractiveElements)
			{
				DrawDebugSolidBox(GetWorld(), SubtractiveElement, FColor::Red, FTransform::Identity, false, TickInterval);
			}
			
			DrawDebugDirectionalArrow(GetWorld(), DungeonsRoom->Location, DungeonsRoom->Location + DungeonsRoom->Velocity, 10.0f, FColor::Black, false, TickInterval, 0, 2);
			uint64 InnerKey = GetTypeHash(DungeonsRoom->GetName() + "Velocity");
			GEngine->AddOnScreenDebugMessage(InnerKey, 2.0f, FColor::Green, FString::Printf(TEXT("Velocity:  [M]%f | [D]%s"), DungeonsRoom->Velocity.Length(), *DungeonsRoom->Velocity.ToString()));
			for (const FDungeonDoor& Door : DungeonsRoom->Doors)
			{
				FTransform WSDoor = Door.Transform*FTransform(DungeonsRoom->Location);
				FPlane DoorPlane(WSDoor.GetLocation(), WSDoor.GetUnitAxis(EAxis::X));
				const bool ConnectedDoor = Door.ConnectingRoom[1] != nullptr;
				DrawDebugSolidPlane(GetWorld(),DoorPlane,  WSDoor.GetLocation(), FVector2D(HallwayData->HallWaySectionDimensions.X * DIVIDE_BY_2, DungeonsRoom->Extent.Z), ConnectedDoor ? FColor::Green : FColor::Purple, false, TickInterval);
			}
		}
	}
	
	if(bShowHallways)
	{
		for (const UDungeonHallwayData* HallWay : DungeonHallwaysData)
		{
			FColor* DebugColor = HallwayDebugColors.Find(HallWay->Type);
			DrawDebugDirectionalArrow(GetWorld(), HallWay->Start, HallWay->Start + (HallWay->End - HallWay->Start)*DIVIDE_BY_2, 1000,DebugColor ? *DebugColor : FColor::Orange, false, TickInterval, 0, 10 );
			DrawDebugLine(GetWorld(), HallWay->Start, HallWay->End, DebugColor ? *DebugColor : FColor::Orange, false, TickInterval, 0, 10);
			if(HallWay->Type == ECorridorType::HCorner)
			{
				FVector CornerMidPoint = HallWay->Start + (HallWay->End - HallWay->Start)*DIVIDE_BY_2;
				FVector CornerPoint = CornerMidPoint + HallWay->Direction * FVector::Dist(HallWay->End, HallWay->Start) * DIVIDE_BY_2;
				DrawDebugLine(GetWorld(), HallWay->Start, CornerPoint, DebugColor ? *DebugColor : FColor::Orange, false, TickInterval, 0, 10);
				DrawDebugLine(GetWorld(), CornerPoint, HallWay->End, DebugColor ? *DebugColor : FColor::Orange, false, TickInterval, 0, 10);
			}
		}
		if(bIsCreatingHallways && DungeonHallwayPathFinder)
		{
			DungeonHallwayPathFinder->Debug(TickInterval);
		}
	}
	if(bShowBounds)
	{
		DrawDebugBox(GetWorld(), DungeonBounds.GetCenter(), DungeonBounds.GetExtent(), FColor::Blue, false, TickInterval, 0, 2);
	}
}

FTransform ADungeonMapper::GenerateDoorOnRoomWall(const FVector& RoomSize, const FVector& DoorForward, const FVector& SlidingVector) const
{
	float DoorTime = RandomStream.FRandRange(-DIVIDE_BY_2, DIVIDE_BY_2);
	FVector DoorLocation = DoorForward * RoomSize + SlidingVector * ((RoomSize * 2.0f - HallwayData->HallWaySectionDimensions.X) * DoorTime);
	//We use FQuat::FindBetweenNormals instead of FVectorToOrientationQuad because last one give some floating point imprecision witch result on not returning correct unit axis
	return FTransform(FQuat::FindBetweenNormals(FVector::ForwardVector, DoorForward), DoorLocation);
}

void ADungeonMapper::GenerateDungeonRooms()
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateRooms);
	const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	DungeonNodes.Empty(MaxRooms);
	DungeonHallwaysData.Empty();
	for (ADungeonRoom* DungeonRoom : DungeonRooms)
	{
		DungeonRoom->Destroy();
	}
	DungeonRooms.Empty();
	if(!NavSys)
	{
		return;
	}
	DungeonBounds = NavSys->GetNavigableWorldBounds();
	RandomStream = FRandomStream(RandomSeed);
	TArray<FDungeonDoor*> DoorsCreated;
	
	const int32 XCells = DungeonBounds.GetExtent().X / (RoomXExtent.Y);
	const int32 YCells = DungeonBounds.GetExtent().Y / (RoomYExtent.Y);
	const int32 ZCells = DungeonBounds.GetExtent().Z / (RoomZExtent.Y);
	const int32 RoomsToGenerate = RandomStream.RandRange(FMath::Min(MinRooms,XCells * YCells * ZCells) , FMath::Min(MaxRooms,XCells * YCells * ZCells));
	
	UDungeonRoomData* NewRoomNode = CreateRoom();

	for (FDungeonDoor& Door : NewRoomNode->Doors)
	{
		DoorsCreated.Add(&Door);
	}
	
	DungeonNodes.Add(NewRoomNode);

	FVector RoomMin = -NewRoomNode->Extent;
	FVector RoomMax = NewRoomNode->Extent;
	FVector MinDungeonBounds = RoomMin;
	FVector MaxDungeonBounds = RoomMax;
	
	for (int IdxRoom = 0; IdxRoom < RoomsToGenerate - 1; ++IdxRoom)
	{
		if(DoorsCreated.Num() <= 1)
		{
			NewRoomNode = CreateRoom();
			NewRoomNode->Location.Z = MinDungeonBounds.Z - RoomZExtent.Y;
			for (FDungeonDoor& Door : NewRoomNode->Doors)
			{
				DoorsCreated.Add(&Door);
			}
			DungeonNodes.Add(NewRoomNode);
			FBox Room(NewRoomNode->Location - NewRoomNode->Extent, NewRoomNode->Location + NewRoomNode->Extent);
			
			MinDungeonBounds.X = FMath::Min(Room.Min.X, MinDungeonBounds.X);
			MinDungeonBounds.Y = FMath::Min(Room.Min.Y, MinDungeonBounds.Y);
			MinDungeonBounds.Z = FMath::Min(Room.Min.Z, MinDungeonBounds.Z);

			MaxDungeonBounds.X = FMath::Max(Room.Max.X, MaxDungeonBounds.X);
			MaxDungeonBounds.Y = FMath::Max(Room.Max.Y, MaxDungeonBounds.Y);
			MaxDungeonBounds.Z = FMath::Max(Room.Max.Z, MaxDungeonBounds.Z);
			continue;
		}

		//choose a random Orphaned door to connect the room
		int32 ConnectingDoorIdx = RandomStream.RandRange(0, DoorsCreated.Num() - 1);
		
		if(RandomStream.RandRange(0,1))
		{
			//create a hallway first to connect this new room
			NewRoomNode = CreateRoomOfSize(RandomStream.FRandRange(HallwayData->HallWaySectionDimensions.X * 2.0f, HallwayData->HallWaySectionDimensions.X * 10.0f),HallwayData->HallWaySectionDimensions.X, HallwayData->HallWaySectionDimensions.Y, 2);
			NewRoomNode->RoomType = ERoomType::Hallway;
			AdjustRoomToDoor(NewRoomNode, DoorsCreated[ConnectingDoorIdx]);
			int32 OriginalDoorsCreatedSize = DoorsCreated.Num() - 1;
			for (int IdxDoor = 0; IdxDoor < NewRoomNode->Doors.Num(); ++IdxDoor)
			{
				if(NewRoomNode->Doors[IdxDoor].ConnectingRoom[1] != nullptr)
				{
					continue;
				}
				DoorsCreated.Add(&NewRoomNode->Doors[IdxDoor]);
			}
			DoorsCreated.RemoveAt(ConnectingDoorIdx);
			DungeonNodes.Add(NewRoomNode);
			ConnectingDoorIdx = RandomStream.RandRange(FMath::Min(OriginalDoorsCreatedSize, DoorsCreated.Num() - 1), DoorsCreated.Num() - 1);
		}
		
		//create room
		NewRoomNode = CreateRoom();
		AdjustRoomToDoor(NewRoomNode, DoorsCreated[ConnectingDoorIdx]);

		FBox Room(NewRoomNode->Location - NewRoomNode->Extent, NewRoomNode->Location + NewRoomNode->Extent);
		//if Room is overlaping other rooms, add the overlapping part to be removed of the room when rendering
		TArray<UDungeonRoomData*> OverlappingRooms = DungeonNodes.FilterByPredicate([&Room](const UDungeonRoomData* Node)
		{
			FBox OtherRoom(Node->Location - (Node->Extent - 0.1f), Node->Location + (Node->Extent - 0.1f));
			return Room.Intersect(OtherRoom);
		});

		for (const UDungeonRoomData* OverlappingRoom : OverlappingRooms)
		{
			const FBox OtherRoom(OverlappingRoom->Location - (OverlappingRoom->Extent - 0.1f), OverlappingRoom->Location + (OverlappingRoom->Extent - 0.1f));
			FBox SubtractiveElement = OtherRoom.Overlap(Room);
			if(SubtractiveElement.IsValid)
			{
				NewRoomNode->SubtractiveElements.Add(SubtractiveElement);
			}
		}

		//Add doors to the pool of orphaned doors
		for (int IdxDoor = 0; IdxDoor < NewRoomNode->Doors.Num(); ++IdxDoor)
		{
			if(NewRoomNode->Doors[IdxDoor].ConnectingRoom[1] != nullptr)
			{
				continue;
			}
			DoorsCreated.Add(&NewRoomNode->Doors[IdxDoor]);
		}
		
		DoorsCreated.RemoveAt(ConnectingDoorIdx);
		
		DungeonNodes.Add(NewRoomNode);
				
		MinDungeonBounds.X = FMath::Min(Room.Min.X, MinDungeonBounds.X);
		MinDungeonBounds.Y = FMath::Min(Room.Min.Y, MinDungeonBounds.Y);
		MinDungeonBounds.Z = FMath::Min(Room.Min.Z, MinDungeonBounds.Z);

		MaxDungeonBounds.X = FMath::Max(Room.Max.X, MaxDungeonBounds.X);
		MaxDungeonBounds.Y = FMath::Max(Room.Max.Y, MaxDungeonBounds.Y);
		MaxDungeonBounds.Z = FMath::Max(Room.Max.Z, MaxDungeonBounds.Z);
	}
	DungeonBounds = FBox(MinDungeonBounds, MaxDungeonBounds);
	
	for (UDungeonRoomData* Node : DungeonNodes)
	{
		Node->Doors.RemoveAll([](const FDungeonDoor& Door){ return !Door.IsValid || Door.ConnectingRoom[1] == nullptr; });
		if(Node->RoomType == ERoomType::Hallway)
		{
			FBox NodeBox(Node->Location - Node->Extent, Node->Location + Node->Extent);
			FBox OverlappingBox = DungeonBounds.Overlap(NodeBox);
			FVector OldLocation = Node->Location;
			Node->Location = OverlappingBox.GetCenter();
			Node->Extent = OverlappingBox.GetExtent();
			FVector Translation = Node->Location - OldLocation;
			
			for (FDungeonDoor& Door : Node->Doors)
			{
				Door.Transform.AddToTranslation(-Translation);
			}
		}
	}
}

UDungeonHallwayData* ADungeonMapper::CreateConnectionFromEdgePoint(UDungeonRoomData* ConnectedRoom, FVector Start, FVector End)
{
	if(!bHallwayToRoomConnection)
	{
		return nullptr;
	}
	FTransform StartDoorTransform;
	const FRotator StartRoomDoorRotation = (End - Start).Rotation();
	StartDoorTransform = FTransform(StartRoomDoorRotation, Start);
	ConnectedRoom->Doors.Add(StartDoorTransform);
	if(FVector::Distance(End, Start) >= HallwayData->HallWaySectionDimensions.X)
	{//Create Start door connection hallway
		UDungeonHallwayData* NewHallway = DuplicateObject(HallwayData, this);
		NewHallway->Direction = StartDoorTransform.GetUnitAxis(EAxis::X);
		NewHallway->Start = StartDoorTransform.GetLocation();
		NewHallway->End = NewHallway->Start + NewHallway->Direction * (NewHallway->HallWaySectionDimensions.X * DIVIDE_BY_2);
		const bool IsVerticalCorridor = NewHallway->Direction.GetAbs().Equals(FVector::UpVector);
		NewHallway->Type = IsVerticalCorridor ? ECorridorType::VRoomConnection : ECorridorType::HRoomConnection;
		return NewHallway;
	}
	return nullptr;
}

void ADungeonMapper::FindNextOrphanDoor(int32& RoomIdx, int32& DoorIdx)
{
	bool FoundOrphanDoor = false;
	for (RoomIdx; RoomIdx < DungeonNodes.Num(); ++RoomIdx)
	{
		UDungeonRoomData* Node = DungeonNodes[RoomIdx];
		for (DoorIdx; DoorIdx < Node->Doors.Num(); ++DoorIdx)
		{
			FDungeonDoor& Door = Node->Doors[DoorIdx];
			if(Door.ConnectingRoom[1] == nullptr)
			{
				FoundOrphanDoor = true;
				break;
			}
		}
		if(FoundOrphanDoor)
		{
			break;
		}
		DoorIdx=0;
	}
}

void ADungeonMapper::MergeHallways()
{
	int32 NumHallways = DungeonHallwaysData.Num();
	for (int i = 0; i < NumHallways; ++i)
	{
		UDungeonHallwayData* FirstHallway = DungeonHallwaysData[i];
		if(FirstHallway->Type != ECorridorType::HStraight && FirstHallway->Type != ECorridorType::Stairs && FirstHallway->Type != ECorridorType::HRoomConnection)
		{
			continue;
		}
		for (int j = i + 1; j < NumHallways; ++j)
		{
			UDungeonHallwayData* SecondHallway = DungeonHallwaysData[j];
			if(SecondHallway->Type != ECorridorType::HStraight && SecondHallway->Type != ECorridorType::Stairs && SecondHallway->Type != ECorridorType::HRoomConnection)
			{
				continue;
			}
	
	
			bool AreColinear = FirstHallway->Direction.GetAbs().Equals(SecondHallway->Direction.GetAbs());
			if(!AreColinear)
			{
				continue;
			}
			
			const FVector FirstInvertedDirection(!FirstHallway->Direction.X,!FirstHallway->Direction.Y, !FirstHallway->Direction.Z);
			const FVector SecondInvertedDirection(!SecondHallway->Direction.X,!SecondHallway->Direction.Y, !SecondHallway->Direction.Z);
			const FVector FirstNegatedStart = FirstHallway->Start*FirstInvertedDirection;
			const FVector SecondNegatedStart = SecondHallway->Start*SecondInvertedDirection;
			if(!FirstNegatedStart.Equals(SecondNegatedStart))
			{
				continue;
			}
	
			if(FirstHallway->Direction.Equals(SecondHallway->Direction))
			{
				if(FirstHallway->Start.Equals(SecondHallway->End))
				{
					FirstHallway->Start = SecondHallway->Start;
					SecondHallway->bIsInvalid = true;
				}
				else if(FirstHallway->End.Equals(SecondHallway->Start))
				{
					FirstHallway->End = SecondHallway->End;
					SecondHallway->bIsInvalid = true;
				}
				else if(FirstHallway->Start.Equals(SecondHallway->Start) || FirstHallway->End.Equals(SecondHallway->End))
				{
					if(FVector::Dist(FirstHallway->Start, FirstHallway->End) > FVector::Dist(SecondHallway->Start, SecondHallway->End))
					{
						SecondHallway->bIsInvalid = true;
					}
					else
					{
						FirstHallway->bIsInvalid = true;
					}
				}
			}
			else
			{
				if(FirstHallway->Start.Equals(SecondHallway->Start))
				{
					FirstHallway->Start = SecondHallway->End;
					SecondHallway->bIsInvalid = true;
				}
				else if(FirstHallway->End.Equals(SecondHallway->End))
				{
					FirstHallway->End = SecondHallway->Start;
					SecondHallway->bIsInvalid = true;
				}
				else if(FirstHallway->Start.Equals(SecondHallway->End) || FirstHallway->End.Equals(SecondHallway->Start))
				{
					if(FVector::Dist(FirstHallway->Start, FirstHallway->End) > FVector::Dist(SecondHallway->Start, SecondHallway->End))
					{
						SecondHallway->bIsInvalid = true;
					}
					else
					{
						FirstHallway->bIsInvalid = true;
					}
				}
			}
		}
	}
	
	DungeonHallwaysData.RemoveAll([](const UDungeonHallwayData* OtherHallWay)
	{
		return OtherHallWay->bIsInvalid;
	});
}

void ADungeonMapper::CreateCornerHallways()
{
	int NumHallways = DungeonHallwaysData.Num();
	for (int i = 0; i < NumHallways; ++i)
	{
		UDungeonHallwayData* FirstHallway = DungeonHallwaysData[i];
		if(	FirstHallway->Type != ECorridorType::HStraight
			&& FirstHallway->Type != ECorridorType::Stairs
			&& FirstHallway->Type != ECorridorType::HRoomConnection)
		{
			continue;
		}
		
		for (int j = i + 1; j < NumHallways; ++j)
		{
			UDungeonHallwayData* SecondHallway = DungeonHallwaysData[j];
			if(	SecondHallway->Type != ECorridorType::HStraight
			&& SecondHallway->Type != ECorridorType::Stairs
			&& SecondHallway->Type != ECorridorType::HRoomConnection)
			{
				continue;
			}
	
			FVector IntersectionPoint1 = FVector::ZeroVector;
			FVector IntersectionPoint2 = FVector::ZeroVector;
			FMath::SegmentDistToSegmentSafe(FirstHallway->Start, FirstHallway->End, SecondHallway->Start, SecondHallway->End, IntersectionPoint1, IntersectionPoint2);
			bool bIntersect = IntersectionPoint1.Equals(IntersectionPoint2);
			if(bIntersect)
			{

				UDungeonHallwayData* HallwayOrder[2];
				FVector CornerPoint = IntersectionPoint1;
				if(FirstHallway->End.Equals(CornerPoint) || SecondHallway->Start.Equals(CornerPoint))
				{
					HallwayOrder[0] = FirstHallway;
					HallwayOrder[1] = SecondHallway;
				}
				else if(FirstHallway->Start.Equals(CornerPoint) || SecondHallway->End.Equals(CornerPoint))
				{
					HallwayOrder[0] = SecondHallway;
					HallwayOrder[1] = FirstHallway;
				}
				else //need to resolve when the case of a + intersection
				{
					continue;
				}
	 				
				const FVector HallwayStartDirection = (CornerPoint - HallwayOrder[0]->Start).GetSafeNormal();
				const FVector HallwayEndDirection = (HallwayOrder[1]->End - CornerPoint).GetSafeNormal();

				//NewCorner->HallWaySectionDimensions.X * DIVIDE_BY_2 is the minimum size so that we actually get a corner shape
				// less than that and angles start to break.
				UDungeonHallwayData* NewCorner = DuplicateObject(HallwayData, this);
				NewCorner->Start = CornerPoint - HallwayStartDirection * NewCorner->HallWaySectionDimensions.X * DIVIDE_BY_2;
				NewCorner->End = CornerPoint + HallwayEndDirection * NewCorner->HallWaySectionDimensions.X * DIVIDE_BY_2;

				const FVector CornerMidPoint = NewCorner->Start + ((NewCorner->End - NewCorner->Start) * 0.5);
				NewCorner->Direction = (CornerPoint - CornerMidPoint).GetSafeNormal();

				const bool IsStairs = (NewCorner->End.Z - NewCorner->Start.Z) != 0;
				NewCorner->Type = IsStairs ? ECorridorType::StairConnection : ECorridorType::HCorner;

				DungeonHallwaysData.AddUnique(NewCorner);
				
				float OGDistance = FVector::Dist(HallwayOrder[0]->End, HallwayOrder[0]->Start);
				float EndToNewEndDistance = FVector::Dist(HallwayOrder[0]->End, NewCorner->Start);
				if(OGDistance < EndToNewEndDistance)
				{
					HallwayOrder[0]->bIsInvalid = true;
				}
				else
				{
					HallwayOrder[0]->End = NewCorner->Start;
				}

				OGDistance = FVector::Dist(HallwayOrder[1]->End, HallwayOrder[1]->Start);
				EndToNewEndDistance = FVector::Dist(HallwayOrder[1]->Start, NewCorner->End);
				
				if(OGDistance < EndToNewEndDistance)
				{
					HallwayOrder[1]->bIsInvalid = true;
				}
				else
				{
					HallwayOrder[1]->Start = NewCorner->End;
				}
				
				if(HallwayOrder[0]->Start.Equals(HallwayOrder[0]->End))
				{
					HallwayOrder[0]->bIsInvalid = true;
				}
				if(HallwayOrder[1]->Start.Equals(HallwayOrder[1]->End))
				{
					HallwayOrder[1]->bIsInvalid = true;
				}
			}
		}
	}
	DungeonHallwaysData.RemoveAll([](const UDungeonHallwayData* OtherHallWay)
	{
		return OtherHallWay->bIsInvalid;
	});
}

void ADungeonMapper::CreateHallways()
{
	DungeonHallwaysData.Empty();
	int32 RoomIdx = 0;
	int32 DoorIdx = 0;

	DoorsToconnect.Empty();
	FindNextOrphanDoor(RoomIdx, DoorIdx);
	while(DungeonNodes.IsValidIndex(RoomIdx))
	{
		DoorsToconnect.Add(&DungeonNodes[RoomIdx]->Doors[DoorIdx]);
		DoorIdx++;
		FindNextOrphanDoor(RoomIdx, DoorIdx);
	}
	
	if(HallWayGenerationMethod == EHallwayGenerationMethod::PathFinding)
	{
		if(DoorsToconnect.Num() < 2)
		{
			return;
		}
		DungeonHallwayPathFinder->Obstacles.Empty();
		for (UDungeonRoomData* Node : DungeonNodes)
		{
			FPathObstacle NewObstacle;
			NewObstacle.Location = Node->Location;
			NewObstacle.Extent = Node->Extent;
			DungeonHallwayPathFinder->Obstacles.Add(NewObstacle);
		}
		
		bIsCreatingHallways = true;
		DungeonHallwayPathFinder->MaxSlopeAngle = MaxHallwaySlope;
		DungeonHallwayPathFinder->HallWaySegmentLength = HallwayData->HallWaySectionDimensions.X;

		
		StartDoorIdx = RandomStream.RandRange(0, DoorsToconnect.Num() - 1);
		ConnectingStartDoor = DoorsToconnect[StartDoorIdx];
		DoorsToconnect.RemoveAt(StartDoorIdx);
		FVector StartLocation = (ConnectingStartDoor->Transform*FTransform(ConnectingStartDoor->ConnectingRoom[0]->Location)).GetLocation();

		EndDoorIdx = RandomStream.RandRange(0, DoorsToconnect.Num() - 1);
		ConnectingEndDoor = DoorsToconnect[EndDoorIdx];
		DoorsToconnect.RemoveAt(EndDoorIdx);
		FVector EndDoorLocation = (ConnectingEndDoor->Transform*FTransform(ConnectingEndDoor->ConnectingRoom[0]->Location)).GetLocation();
		
		DungeonHallwayPathFinder->Initialize(StartLocation, EndDoorLocation);
		DungeonHallwayPathFinder->FillAdditionalValidConnectionDirections();
		return;
	}

	/*The Idea:
	 * We select two doors to connect, then from the starting door we fallow the perimeter of the rooms until we reach the end door
	 */
	FindNextOrphanDoor(RoomIdx, DoorIdx);
	while (RoomIdx < DungeonNodes.Num())
	{

		
		FDungeonDoor& Start = DungeonNodes[RoomIdx]->Doors[DoorIdx];
		FVector StartDoorLocation = (Start.Transform*FTransform(DungeonNodes[RoomIdx]->Location)).GetLocation();
		FVector StartHallway = StartDoorLocation + Start.Transform.GetUnitAxis(EAxis::X) * (HallwayData->HallWaySectionDimensions.X * DIVIDE_BY_2);
		
		DoorIdx++;
		FindNextOrphanDoor(RoomIdx, DoorIdx);
		
		if(RoomIdx >= DungeonNodes.Num())
		{
			break;
		}

		FDungeonDoor& End = DungeonNodes[RoomIdx]->Doors[DoorIdx];
		FVector EndDoorLocation = (End.Transform*FTransform(DungeonNodes[RoomIdx]->Location)).GetLocation();
		FVector EndHallway = EndDoorLocation + End.Transform.GetUnitAxis(EAxis::X) * (HallwayData->HallWaySectionDimensions.X * DIVIDE_BY_2);
		
		//Add Start conection
		UDungeonHallwayData* NewStartHallway = DuplicateObject(HallwayData, this);
		NewStartHallway->Start = StartDoorLocation;
		NewStartHallway->End = StartHallway;
		NewStartHallway->Direction = (NewStartHallway->End - NewStartHallway->Start).GetSafeNormal();
		NewStartHallway->Type = ECorridorType::HRoomConnection;
		DungeonHallwaysData.AddUnique(NewStartHallway);
		
		
		
		//End Connection
		UDungeonHallwayData* NewEndHallway = DuplicateObject(HallwayData, this);
		NewEndHallway->Start = EndHallway;
		NewEndHallway->End = EndDoorLocation;
		NewEndHallway->Direction = (NewEndHallway->End - NewEndHallway->Start).GetSafeNormal();
		NewEndHallway->Type = ECorridorType::HRoomConnection;
		DungeonHallwaysData.AddUnique(NewEndHallway);
		
		Start.ConnectingRoom[1] = End.ConnectingRoom[0];
		End.ConnectingRoom[1] = Start.ConnectingRoom[0];

		//create conecting hallways
		FVector DoorToDoor = EndHallway - StartHallway;
		FVector HallwayRightVector = UDungeonGenerationHelperFunctions::RoundToUnitVector(Start.Transform.GetUnitAxis(EAxis::Y));
		
		UDungeonRoomData* ConnectingRoom = Start.ConnectingRoom[0];
		FVector NegatedHallwayRightVector = UDungeonGenerationHelperFunctions::NegateUnitVector(HallwayRightVector);
		
		float Dot = FVector::DotProduct(DoorToDoor.GetSafeNormal(), HallwayRightVector.GetSafeNormal());

		FVector HallwayDirection = (HallwayRightVector*Dot).GetSafeNormal();

		FVector CornerPoint =  HallwayDirection * ConnectingRoom->Extent;
		CornerPoint = StartDoorLocation * NegatedHallwayRightVector + CornerPoint + (Start.ConnectingRoom[0]->Location * HallwayDirection);
		DrawDebugPoint(GetWorld(), CornerPoint, 10.0f, FColor::Green, true);
		float DoorToCornerDistance = FVector::Dist(CornerPoint, StartDoorLocation) + HallwayData->HallWaySectionDimensions.X * DIVIDE_BY_2;
		CornerPoint = StartHallway + HallwayDirection * DoorToCornerDistance;
		//CornerPoint = CornerPoint*NegatedHallwayRightVector+EndHallway*HallwayRightVector.GetAbs();
				
		UDungeonHallwayData* NewStartMidHallway = DuplicateObject(HallwayData, this);
		NewStartMidHallway->Start = StartHallway;
		NewStartMidHallway->End = CornerPoint;
		NewStartMidHallway->Direction = (NewStartMidHallway->End - NewStartMidHallway->Start).GetSafeNormal();
		NewStartMidHallway->Type = ECorridorType::HStraight;
		DungeonHallwaysData.AddUnique(NewStartMidHallway);

		UDungeonHallwayData* NewEndMidHallway = DuplicateObject(HallwayData, this);
		NewEndMidHallway->Start = CornerPoint;
		NewEndMidHallway->End = EndHallway;
		NewEndMidHallway->Direction = (NewEndMidHallway->End - NewEndMidHallway->Start).GetSafeNormal();
		NewEndMidHallway->Type = ECorridorType::HStraight;
		DungeonHallwaysData.AddUnique(NewEndMidHallway);
		
		DoorIdx++;
        FindNextOrphanDoor(RoomIdx, DoorIdx);
	}
	
	//Merge Hallways that fallow the same path
	MergeHallways();

	if(bCreateCorners)
	{
		/// Create Corner sections of hallways
	 	CreateCornerHallways();
	}	
}

void ADungeonMapper::RenderDungeon()
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_RenderDungeon);
	UDynamicMesh* MainDynMesh = DynamicMeshComponent->GetDynamicMesh();
	MainDynMesh->Reset();
	
	for (ADungeonRoom* DungeonRoom : DungeonRooms)
	{
		DungeonRoom->Destroy();
	}
	DungeonRooms.Empty();
	
	UWorld* World = GetWorld();
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	for (UDungeonRoomData* DungeonNode : DungeonNodes)
	{
		ADungeonRoom* NewRoom = World->SpawnActor<ADungeonRoom>(ADungeonRoom::StaticClass(), DungeonNode->Location,FRotator::ZeroRotator, SpawnParams);
		NewRoom->InitializeRoom(DungeonNode);
		DungeonRooms.Add(NewRoom);
	}
	
	for (const UDungeonHallwayData* DungeonHallway : DungeonHallwaysData)
	{
		RenderHallWays(MainDynMesh, DungeonHallway);
	}
}

void ADungeonMapper::ClearAll()
{
	FlushPersistentDebugLines(GetWorld());
	UDynamicMesh* DynMesh = DynamicMeshComponent->GetDynamicMesh();
	DynMesh->Reset();
	DungeonHallwaysData.Empty();
	DungeonNodes.Empty();
	DoorsToconnect.Empty();
	ConnectingEndDoor = nullptr;
	ConnectingStartDoor = nullptr;
	for (ADungeonRoom* DungeonRoom : DungeonRooms)
	{
		DungeonRoom->Destroy();
	}
	DungeonRooms.Empty();
	SET_FLOAT_STAT(STAT_GenerateRooms, 0.0f);
	SET_FLOAT_STAT(STAT_ConnectRooms, 0.0f);
	SET_FLOAT_STAT(STAT_SimplifyConections, 0.0f);
	SET_FLOAT_STAT(STAT_RenderDungeon, 0.0f);
}

UDungeonRoomData* ADungeonMapper::CreateRoom()
{
	int32 RoomXSize = RandomStream.RandRange(RoomXExtent.X, RoomXExtent.Y);
	int32 RoomYSize = RandomStream.RandRange(RoomYExtent.X, RoomYExtent.Y);
	int32 RoomZSize = RandomStream.RandRange(RoomZExtent.X, RoomZExtent.Y);

	return CreateRoomOfSize(RoomXSize, RoomYSize, RoomZSize, 1);
}

UDungeonRoomData* ADungeonMapper::CreateRoomOfSize(float RoomXSize, float RoomYSize, float RoomZSize, int32 MinimumNumDoors)
{
	UDungeonRoomData* NewRoom = DuplicateObject(RoomData, this);
	NewRoom->Location = DungeonBounds.GetCenter();
	NewRoom->Extent = FVector(RoomXSize, RoomYSize, RoomZSize);

	int32 NumDoors = RandomStream.RandRange(MinimumNumDoors, 4);
	FVector ValidDoorDirections[4] {FVector::ForwardVector, FVector::RightVector, FVector::BackwardVector, FVector::LeftVector};
	TSet<int32> UsedDirections;
	for (int IdxDoor = 0; IdxDoor < NumDoors; ++IdxDoor)
	{
		
		int32 DoorDirection = RandomStream.RandRange(0, 3);
		while (UsedDirections.Contains(DoorDirection))
		{
			DoorDirection = RandomStream.RandRange(0, 3);
		}
		UsedDirections.Add(DoorDirection);
		FVector DoorForward = ValidDoorDirections[DoorDirection];
		FVector SlidingVector = ValidDoorDirections[(DoorDirection + 1)%2];
		FTransform DoorTransform = GenerateDoorOnRoomWall(NewRoom->Extent, DoorForward, SlidingVector);
		NewRoom->Doors.Add(FDungeonDoor(DoorTransform, NewRoom));
	}
	
	return NewRoom;
}

void ADungeonMapper::AdjustRoomToDoor(UDungeonRoomData* Room, FDungeonDoor* Door)
{
	FTransform DoorTransform = Door->Transform * FTransform(Door->ConnectingRoom[0]->Location);
	DoorTransform.ConcatenateRotation(FQuat(FRotator(0,180,0)));
	//Move room so at least one door is connected to selected door
	int32 DoorToConnectIdx = RandomStream.RandRange(0, Room->Doors.Num() - 1);
	FDungeonDoor& DoorToConnect = Room->Doors[DoorToConnectIdx];
	const FTransform WSDoorToConnect = DoorToConnect.Transform*FTransform(Room->Location);
	FTransform Translation = (WSDoorToConnect).GetRelativeTransformReverse(DoorTransform);
	FBox RoomBox(Room->Location - Room->Extent, Room->Location + Room->Extent);
	RoomBox = RoomBox.TransformBy(Translation);
	
	Room->Location = RoomBox.GetCenter();
	Room->Extent = RoomBox.GetExtent();

	//Update the doors of this room with the new rotation of the room
	for (int IdxDoor = 0; IdxDoor < Room->Doors.Num(); ++IdxDoor)
	{
		FTransform WSDoorCreated = Room->Doors[IdxDoor].Transform*FTransform(FVector::ZeroVector);
		Room->Doors[IdxDoor].Transform = (WSDoorCreated*Translation).GetRelativeTransform(FTransform(Room->Location));
		if(IdxDoor == DoorToConnectIdx)
		{
			Room->Doors[IdxDoor].ConnectingRoom[1] = Door->ConnectingRoom[0];
			continue;
		}

		WSDoorCreated = Room->Doors[IdxDoor].Transform*FTransform(Room->Location);
		const bool IsInsideRoom = DungeonNodes.ContainsByPredicate([&WSDoorCreated](const UDungeonRoomData* Node)
		{
			FBox OtherRoom(Node->Location - Node->Extent, Node->Location + Node->Extent);
			return OtherRoom.IsInside(WSDoorCreated.GetLocation());
		});
		if(IsInsideRoom)
		{
			Room->Doors[IdxDoor].IsValid = false;
			continue;
		}
	}
	Door->ConnectingRoom[1] = Room;
}

UDungeonHallwayData* ADungeonMapper::FixHallwayCrossingRoom(UDungeonRoomData* Room, const FVector& Start, const FVector& End)
{
	FBox RoomBox(Room->Location - Room->Extent, Room->Location + Room->Extent);

	FVector HitLocation = FVector::ZeroVector;
	FVector HitNormal = FVector::ZeroVector;
	float HitTime = 0.0f;
	const bool Interect = FMath::LineExtentBoxIntersection(RoomBox, Start, End, FVector::ZeroVector, HitLocation, HitNormal, HitTime);
	if(Interect && HitTime > 0.0f &&  !FMath::IsNearlyEqual(HitTime, 1.0f, 0.00001f) )
	{
		UDungeonHallwayData* NewHallway = DuplicateObject(HallwayData, this);
		NewHallway->Start = Start;
		NewHallway->End = HitLocation;
		NewHallway->Type = ECorridorType::HStraight;
		NewHallway->Direction = (NewHallway->End - NewHallway->Start).GetSafeNormal();
		return NewHallway;
	}
	return nullptr;
}

void ADungeonMapper::RenderHallWays(UDynamicMesh* DynMesh, const UDungeonHallwayData* DungeonHallway)
{

		UDynamicMesh* HallwayMesh = AllocateComputeMesh();
		FGeometryScriptMeshBooleanOptions BoolOptions;
	
			FVector Start = DungeonHallway->Start;
			FVector End = DungeonHallway->End;
			const FVector HallWaySegment = End - Start;

			FGeometryScriptPrimitiveOptions PrimitiveOptions;

			switch (DungeonHallway->Type)
			{
			case ECorridorType::HStraight:
				{
					HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallway
						(
							HallwayMesh
							, PrimitiveOptions
							, Start
							, End
							, DungeonHallway->HallWaySectionDimensions.X
							,  DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true,
							true,
							3,0, 0
						);

					 // HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallowedBox
					 // 					(
					 // 							HallwayMesh
					 // 							, PrimitiveOptions
					 // 							, FTransform(HallWaySegment.Rotation(), Start + HallWaySegment.GetSafeNormal() * HallWaySegment.Length() * 0.5)
					 // 							,  HallWaySegment.Length() - (DungeonHallway->HallWaySectionDimensions.X *DIVIDE_BY_2)
					 // 							, DungeonHallway->HallWaySectionDimensions.X
					 // 							,  DungeonHallway->HallWaySectionDimensions.Y
					 // 							, DungeonHallway->WallThickness
					 // 							, true, 3, 0
						// 						, 0,EGeometryScriptPrimitiveOriginMode::Center
					 // 						);
					
					const FTransform FloorLocation(HallWaySegment.Rotation(), Start + (HallWaySegment * DIVIDE_BY_2) - FVector(0,0,DungeonHallway->HallWaySectionDimensions.Y * DIVIDE_BY_2 - DungeonHallway->WallThickness * DIVIDE_BY_2));
					FKBoxElem FloorShape(HallWaySegment.Length(), DungeonHallway->HallWaySectionDimensions.X, DungeonHallway->WallThickness);
					FloorShape.SetTransform(FloorLocation);
					DungeonCollision.AggGeom.BoxElems.Add(FloorShape);
				}
				break;
			case ECorridorType::HCorner:
				{
					FVector CornerMidPoint = DungeonHallway->Start + (DungeonHallway->End - DungeonHallway->Start)*DIVIDE_BY_2;
					FVector CornerPoint = CornerMidPoint + DungeonHallway->Direction * FVector::Dist(DungeonHallway->End, DungeonHallway->Start) * DIVIDE_BY_2;
					HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallwayCorner
						(
							HallwayMesh
							, PrimitiveOptions
							, Start
							, CornerPoint
							, End
							, DungeonHallway->HallWaySectionDimensions.X
							,  DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true
							, true
						);
					// const FTransform FloorLocation(HallWaySegment.Rotation(), CornerPoint - FVector(0,0,DungeonHallway->HallWaySectionDimensions.Y * DIVIDE_BY_2 - DungeonHallway->WallThickness * DIVIDE_BY_2));
					// FKBoxElem FloorShape(DungeonHallway->HallWaySectionDimensions.X, DungeonHallway->HallWaySectionDimensions.X, DungeonHallway->WallThickness);
					// FloorShape.SetTransform(FloorLocation);
					// DungeonCollision.AggGeom.BoxElems.Add(FloorShape);
				}
				break;
			case ECorridorType::StairConnection:
				{
					FVector CornerPoint = Start + DungeonHallway->Direction * DungeonHallway->HallWaySectionDimensions.X;
					HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallwayCorner
						(
							HallwayMesh
							, PrimitiveOptions
							, Start
							, CornerPoint
							, End
							, DungeonHallway->HallWaySectionDimensions.X
							,  DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true
							, true
						);
				}
				break;
			case ECorridorType::Stairs:
				{
					Start += HallWaySegment.GetSafeNormal()*DungeonHallway->HallWaySectionDimensions.X;
					End -= HallWaySegment.GetSafeNormal()*DungeonHallway->HallWaySectionDimensions.X;
					HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallway
						(
							HallwayMesh
							, PrimitiveOptions
							, Start
							, End
							, DungeonHallway->HallWaySectionDimensions.X
							, DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true, true, HallWaySegment.Length()/125 + 1
							, DungeonHallway->HallWaySectionDimensions.X/125 + 1, DungeonHallway->HallWaySectionDimensions.Y/125 + 1
						);
					
					// float StairRise = HallWaySegment.Z;
					// int32 TotalSteps = StairRise/DungeonHallway->StepRise;
					// float StairRun = HallWaySegment.Size2D();
					// float StepRun = StairRun/TotalSteps;
					//
					// HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendLinearStairs
					// (
					// 	HallwayMesh
					// 	, PrimitiveOptions
					// 	, FTransform(HallWaySegment.GetSafeNormal2D().Rotation(), Start + FVector::DownVector*DungeonHallway->HallWaySectionDimensions.Y * DIVIDE_BY_2)
					// 	, DungeonHallway->HallWaySectionDimensions.X
					// 	, DungeonHallway->StepRise
					// 	, StepRun
					// 	, TotalSteps
					// 	, true
					// 	);
				}
				break;
			case ECorridorType::HCross:
				break;
			case ECorridorType::VCross:
				break;
			case ECorridorType::HRoomConnection:
				{
					HallwayMesh = HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallway
						(
							HallwayMesh
							, PrimitiveOptions
							, Start
							, End
							, DungeonHallway->HallWaySectionDimensions.X
							,  DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true,true,3,0, 0
						);
				}
				break;
			case ECorridorType::VRoomConnection:
				HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder
						(
							HallwayMesh
							, PrimitiveOptions
							,FTransform(DungeonHallway->Direction.Rotation() + FRotator(-90, 0,0), DungeonHallway->Start + DungeonHallway->Direction * (DungeonHallway->HallWaySectionDimensions.Y * DIVIDE_BY_2))
							, DungeonHallway->HallWaySectionDimensions.X * DIVIDE_BY_2
							, DungeonHallway->HallWaySectionDimensions.Y * DIVIDE_BY_2
							, 30
							,HallWaySegment.Length()/125 + 1
							,true
							, EGeometryScriptPrimitiveOriginMode::Base
						);
				break;
			}
		
		DynMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
				(
					DynMesh,
					FTransform::Identity,
					HallwayMesh,
					FTransform::Identity,
					EGeometryScriptBooleanOperation::Union,
					BoolOptions		
				);
		ReleaseComputeMesh(HallwayMesh);
}

void ADungeonMapper::HallowHallWays(UDynamicMesh* DynamicMesh, const UDungeonHallwayData* DungeonHallway)
{
		UDynamicMesh* HallwayMesh = AllocateComputeMesh();
		FGeometryScriptMeshBooleanOptions BoolOptions;

			const FVector Start = DungeonHallway->Start;
			const FVector End = DungeonHallway->End;
			const FVector HallWaySegment = End - Start;
			if(HallWaySegment.Length() == 0.0f)
			{
				ReleaseComputeMesh(HallwayMesh);
				return;
			}
	
		FGeometryScriptPrimitiveOptions PrimitiveOptions;
		switch (DungeonHallway->Type) {
		case ECorridorType::HStraight:
			{
				HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
				(
					HallwayMesh
					, PrimitiveOptions
					, FTransform(HallWaySegment.Rotation(), Start + (HallWaySegment * DIVIDE_BY_2))
					, HallWaySegment.Length()
					, DungeonHallway->HallWaySectionDimensions.X - DungeonHallway->WallThickness * 2.0f
					,  DungeonHallway->HallWaySectionDimensions.Y - DungeonHallway->WallThickness * 2.0f
					, HallWaySegment.Length()/125 + 1, DungeonHallway->HallWaySectionDimensions.X/125 + 1, DungeonHallway->HallWaySectionDimensions.Y/125 + 1
					, EGeometryScriptPrimitiveOriginMode::Center
				);
			}
			break;
		case ECorridorType::HCorner:
			{
				FVector CornerPoint = Start + DungeonHallway->Direction + DungeonHallway->HallWaySectionDimensions.X;
				FVector StartSegment = CornerPoint - Start;
				HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
					(
						HallwayMesh
						, PrimitiveOptions
						, FTransform(StartSegment.Rotation(), CornerPoint)
						, DungeonHallway->HallWaySectionDimensions.X - DungeonHallway->WallThickness * 2.0f
						, DungeonHallway->HallWaySectionDimensions.X - DungeonHallway->WallThickness * 2.0f
						,  DungeonHallway->HallWaySectionDimensions.Y - DungeonHallway->WallThickness * 2.0f
						, HallWaySegment.Length()/125 + 1, DungeonHallway->HallWaySectionDimensions.X/125 + 1, DungeonHallway->HallWaySectionDimensions.Y/125 + 1
						, EGeometryScriptPrimitiveOriginMode::Center
					);
			}
			break;
		case ECorridorType::StairConnection:
			{
				FVector CornerPoint = Start + DungeonHallway->Direction * DungeonHallway->HallWaySectionDimensions.X;
				FVector StartSegment = CornerPoint - Start;
				float Height = FMath::Abs(Start.Z - End.Z) + DungeonHallway->HallWaySectionDimensions.Y;
				HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
					(
						HallwayMesh
						, PrimitiveOptions
						, FTransform(StartSegment.Rotation(), CornerPoint)
						, DungeonHallway->HallWaySectionDimensions.X - DungeonHallway->WallThickness * 2.0f
						, DungeonHallway->HallWaySectionDimensions.X - DungeonHallway->WallThickness * 2.0f
						,  DungeonHallway->HallWaySectionDimensions.Y - DungeonHallway->WallThickness * 2.0f
						, HallWaySegment.Length()/125 + 1, DungeonHallway->HallWaySectionDimensions.X/125 + 1, DungeonHallway->HallWaySectionDimensions.Y/125 + 1
						, EGeometryScriptPrimitiveOriginMode::Center
					);
			}
			break;
		case ECorridorType::Stairs:
			{
				HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
					(
						HallwayMesh
						, PrimitiveOptions
						, FTransform(HallWaySegment.GetSafeNormal2D().Rotation(), Start + (HallWaySegment * DIVIDE_BY_2))
						, HallWaySegment.Size2D()
						, DungeonHallway->HallWaySectionDimensions.X - DungeonHallway->WallThickness * 2.0f
						,  HallWaySegment.Z + DungeonHallway->HallWaySectionDimensions.Y - DungeonHallway->WallThickness * 2.0f
						, HallWaySegment.Length()/125 + 1, DungeonHallway->HallWaySectionDimensions.X/125 + 1, DungeonHallway->HallWaySectionDimensions.Y/125 + 1
						, EGeometryScriptPrimitiveOriginMode::Center
					);
			}
			break;
		case ECorridorType::HCross:
			break;
		case ECorridorType::VCross:
			break;
		}
	
		DynamicMesh = UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
					(
						DynamicMesh,
						FTransform::Identity,
						HallwayMesh,
						FTransform::Identity,
						EGeometryScriptBooleanOperation::Subtract,
						BoolOptions		
					);
		ReleaseComputeMesh(HallwayMesh);
}

UDynamicMeshPool* ADungeonMapper::GetComputeMeshPool()
{
	if (DynamicMeshPool == nullptr)
	{
		DynamicMeshPool = NewObject<UDynamicMeshPool>();
	}
	return DynamicMeshPool;
}

UDynamicMesh* ADungeonMapper::AllocateComputeMesh()
{
	if (UDynamicMeshPool* UsePool = GetComputeMeshPool())
	{
		return UsePool->RequestMesh();
	}
	return NewObject<UDynamicMesh>();
}

bool ADungeonMapper::ReleaseComputeMesh(UDynamicMesh* Mesh)
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