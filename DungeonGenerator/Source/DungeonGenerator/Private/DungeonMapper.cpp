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

void ADungeonMapper::CreateHallwaysFromPath(const TArray<FVector>& Path)
{
	for (int i = 0; i <Path.Num() - 1; ++i)
	{
		UDungeonHallwayData* NewHallway = DuplicateObject(HallwayData, this);
		NewHallway->Start = Path[i];
		NewHallway->End = Path[i + 1];
		NewHallway->Direction = (NewHallway->End - NewHallway->Start).GetSafeNormal();
		const bool IsStairs = (NewHallway->End.Z - NewHallway->Start.Z) != 0;
		NewHallway->Type = IsStairs ? ECorridorType::Stairs : ECorridorType::HStraight;
			
		DungeonHallwaysData.AddUnique(NewHallway);
	}

	if(bPreventCrossing)
	{
		TArray< UDungeonHallwayData*> NewHallways;
		for (UDungeonHallwayData* HallWay : DungeonHallwaysData)
		{
			for (UDungeonRoomData* Room : DungeonNodes)
			{
					UDungeonHallwayData* NewHallway = FixHallwayCrossingRoom(Room,  HallWay->Start, HallWay->End);
					if(NewHallway)
					{
						NewHallways.AddUnique(NewHallway);
						HallWay->bIsInvalid = true;
	
						UDungeonHallwayData* NewHallwayConnection = CreateConnectionFromEdgePoint(Room, NewHallway->End, NewHallway->Start);
						if(NewHallwayConnection)
						{
							NewHallways.AddUnique(NewHallwayConnection);
						}
					}
	
					NewHallway = FixHallwayCrossingRoom(Room,  HallWay->End, HallWay->Start);
					if(NewHallway)
					{
						NewHallways.AddUnique(NewHallway);
						HallWay->bIsInvalid = true;
	
						UDungeonHallwayData* NewHallwayConnection = CreateConnectionFromEdgePoint(Room,NewHallway->End, NewHallway->Start);
						if(NewHallwayConnection)
						{
							NewHallways.AddUnique(NewHallwayConnection);
						}
					}
				}
		}
	
		
		DungeonHallwaysData.RemoveAll([](const UDungeonHallwayData* OtherHallWay)
			{
				return OtherHallWay->bIsInvalid;
			});
		DungeonHallwaysData.Append(NewHallways);
	}

	//Merge Hallways that fallow the same path
	int32 NumHallways = DungeonHallwaysData.Num();
	for (int i = 0; i < NumHallways; ++i)
	{
		UDungeonHallwayData* FirstHallway = DungeonHallwaysData[i];
		if(FirstHallway->Type != ECorridorType::HStraight && FirstHallway->Type != ECorridorType::Stairs)
		{
			continue;
		}
		for (int j = i + 1; j < NumHallways; ++j)
		{
			UDungeonHallwayData* SecondHallway = DungeonHallwaysData[j];
			if(SecondHallway->Type != ECorridorType::HStraight && SecondHallway->Type != ECorridorType::Stairs)
			{
				continue;
			}


			bool AreColinear = FirstHallway->Direction.GetAbs().Equals(SecondHallway->Direction.GetAbs());
			if(!AreColinear)
			{
				continue;
			}
			
			const FVector FirstInvertedDirection = UDungeonGenerationHelperFunctions::NegateUnitVector(FirstHallway->Direction);
			const FVector SecondInvertedDirection = UDungeonGenerationHelperFunctions::NegateUnitVector(SecondHallway->Direction);
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
	
	if(bCreateCorners)
	{
		/// Create Corner sections of hallways
	 	NumHallways = DungeonHallwaysData.Num();
	 	for (int i = 0; i < NumHallways; ++i)
	 	{
	 		UDungeonHallwayData* FirstHallway = DungeonHallwaysData[i];
	 		if(FirstHallway->Type != ECorridorType::HStraight && FirstHallway->Type != ECorridorType::Stairs)
	 		{
	 			continue;
	 		}
	 		for (int j = i + 1; j < NumHallways; ++j)
	 		{
	 			UDungeonHallwayData* SecondHallway = DungeonHallwaysData[j];
	 			if(SecondHallway->Type != ECorridorType::HStraight && SecondHallway->Type != ECorridorType::Stairs)
	 			{
	 				continue;
	 			}

	 			FVector IntersectionPoint1 = FVector::ZeroVector;
	 			FVector IntersectionPoint2 = FVector::ZeroVector;
	 			FMath::SegmentDistToSegmentSafe(FirstHallway->Start, FirstHallway->End, SecondHallway->Start, SecondHallway->End, IntersectionPoint1, IntersectionPoint2);
	 			bool bIntersect = IntersectionPoint1.Equals(IntersectionPoint2);
	 			if(bIntersect)
	 			{
	 				
	 				FVector CornerStart;
	 				FVector CornerPoint = IntersectionPoint1;
	 				FVector CornerEnd;
	 				if(FirstHallway->End.Equals(CornerPoint))
	 				{
	 					CornerStart = FirstHallway->Start;
	 					CornerEnd = SecondHallway->End;
	 				}
	 				else if(FirstHallway->Start.Equals(CornerPoint))
	 				{
	 					CornerStart = SecondHallway->Start;
	 					CornerEnd = FirstHallway->End;
	 				}
	 				else if(SecondHallway->End.Equals(CornerPoint))
	 				{
	 					CornerStart = SecondHallway->Start;
	 					CornerEnd = FirstHallway->End;
	 				}
	 				else if(SecondHallway->Start.Equals(CornerPoint))
	 				{
	 					CornerStart = FirstHallway->Start;
	 					CornerEnd = SecondHallway->End;
	 				}
				    else
				    {
					    continue;
				    }
	 				
	 				FVector HallwayDirection = (CornerPoint - CornerStart).GetSafeNormal();
	 			
	 				UDungeonHallwayData* NewCorner = DuplicateObject(HallwayData, this);
	 				NewCorner->Start = CornerPoint - HallwayDirection * NewCorner->HallWaySectionDimensions.X;
	 				NewCorner->End = CornerPoint + (CornerEnd - CornerPoint).GetSafeNormal() * NewCorner->HallWaySectionDimensions.X;
	 				NewCorner->Direction = HallwayDirection;
	 				const bool IsStairs = (NewCorner->End.Z - NewCorner->Start.Z) != 0;
	 				NewCorner->Type = IsStairs ? ECorridorType::StairConnection : ECorridorType::HCorner;
	 				DungeonHallwaysData.AddUnique(NewCorner);
	 				
	 			}
	 		}
	 	}
	}
}

void ADungeonMapper::RunHallwaysCreation()
{
	if(bIsCreatingHallways)
	{
		if(!DungeonNodes.IsValidIndex(RoomIdx))
		{
			bIsCreatingHallways = false;
			return;
		}
		
		if(DungeonHallwayPathFinder->Evaluate())
		{
			CreateHallwaysFromPath(DungeonHallwayPathFinder->PathResult);
			FindNextOrphanDoor();
			if(DungeonNodes.IsValidIndex(RoomIdx))
			{
				FVector StartLocation = DungeonNodes[RoomIdx]->Doors[DoorIdx].Transform.GetLocation();
				DoorIdx++;
				FindNextOrphanDoor();
				DungeonHallwayPathFinder->Initialize(StartLocation, DungeonNodes[RoomIdx]->Doors[DoorIdx].Transform.GetLocation());
				
				DungeonHallwayPathFinder->FillAdditionalValidConnectionDirections();
				
			}
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
			DrawDebugDirectionalArrow(GetWorld(), DungeonsRoom->Location, DungeonsRoom->Location + DungeonsRoom->Velocity, 10.0f, FColor::Black, false, TickInterval, 0, 2);
			uint64 InnerKey = GetTypeHash(DungeonsRoom->GetName()+ "Velocity");
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

FTransform ADungeonMapper::GenerateDoorOnRoomWall(FRandomStream RandomStream, const FVector& RoomSize, const FVector& DoorForward, const FVector& SlidingVector) const
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
	FRandomStream RandomStream(RandomSeed);
	TArray<FDungeonDoor*> DoorsCreated;
	
	const int32 XCells = DungeonBounds.GetExtent().X / (RoomXExtent.Y);
	const int32 YCells = DungeonBounds.GetExtent().Y / (RoomYExtent.Y);
	const int32 ZCells = DungeonBounds.GetExtent().Z / (RoomZExtent.Y);
	const int32 RoomsToGenerate = RandomStream.RandRange(FMath::Min(MinRooms,XCells * YCells * ZCells) , FMath::Min(MaxRooms,XCells * YCells * ZCells));
	
	UDungeonRoomData* NewRoomNode = CreateRoom(RandomStream);

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
		if(DoorsCreated.IsEmpty())
		{
			break;
		}
		int32 ConectingDoorIdx = RandomStream.RandRange(0, DoorsCreated.Num() - 1);
		FTransform ConnectingDoorTransform = DoorsCreated[ConectingDoorIdx]->Transform * FTransform(DoorsCreated[ConectingDoorIdx]->ConnectingRoom[0]->Location);
		ConnectingDoorTransform.ConcatenateRotation(FQuat(FRotator(0,180,0)));
		
		//create room
		NewRoomNode = CreateRoom(RandomStream);

		//Move room so at least one door is conected to selected door
		int32 DoorToConnectIdx = RandomStream.RandRange(0, NewRoomNode->Doors.Num() - 1);
		FDungeonDoor& DoorToConnect = NewRoomNode->Doors[DoorToConnectIdx];
		
		const FTransform WSDoorToConnect = DoorToConnect.Transform*FTransform(NewRoomNode->Location);
		FTransform Translation = (WSDoorToConnect).GetRelativeTransformReverse(ConnectingDoorTransform);

		FBox Room(NewRoomNode->Location - NewRoomNode->Extent, NewRoomNode->Location + NewRoomNode->Extent);
		Room = Room.TransformBy(Translation);
		NewRoomNode->Location = Room.GetCenter();
		NewRoomNode->Extent = Room.GetExtent();
		for (int IdxDoor = 0; IdxDoor < NewRoomNode->Doors.Num(); ++IdxDoor)
		{
			FTransform WSDoorCreated = NewRoomNode->Doors[IdxDoor].Transform*FTransform(FVector::ZeroVector);
			NewRoomNode->Doors[IdxDoor].Transform = (WSDoorCreated*Translation).GetRelativeTransform(FTransform(NewRoomNode->Location));
			if(IdxDoor == DoorToConnectIdx)
			{
				NewRoomNode->Doors[IdxDoor].ConnectingRoom[1] = DoorsCreated[ConectingDoorIdx]->ConnectingRoom[0];
				continue;
			}
			DoorsCreated.Add(&NewRoomNode->Doors[IdxDoor]);
		}
		DoorsCreated[ConectingDoorIdx]->ConnectingRoom[1] = NewRoomNode;
		DoorsCreated.RemoveAt(ConectingDoorIdx);
		DungeonNodes.Add(NewRoomNode);
		
		MinDungeonBounds.X = FMath::Min(Room.Min.X, MinDungeonBounds.X);
		MinDungeonBounds.Y = FMath::Min(Room.Min.X, MinDungeonBounds.Y);
		MinDungeonBounds.Z = FMath::Min(Room.Min.X, MinDungeonBounds.Z);

		MaxDungeonBounds.X = FMath::Max(Room.Max.X, MaxDungeonBounds.X);
		MaxDungeonBounds.Y = FMath::Max(Room.Max.X, MaxDungeonBounds.Y);
		MaxDungeonBounds.Z = FMath::Max(Room.Max.X, MaxDungeonBounds.Z);
	}
	
	DungeonBounds = FBox(MinDungeonBounds, MaxDungeonBounds);
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

void ADungeonMapper::FindNextOrphanDoor()
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

void ADungeonMapper::CreateHallways()
{
	DungeonHallwaysData.Empty();
	RoomIdx = 0;
	DoorIdx = 0;
	
	if(HallWayGenerationMethod == EHallwayGenerationMethod::PathFinding)
	{
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
		
		FindNextOrphanDoor();

		FVector StartLocation = DungeonNodes[RoomIdx]->Doors[DoorIdx].Transform.GetLocation();
	
		DoorIdx++;
		FindNextOrphanDoor();
		
		DungeonHallwayPathFinder->Initialize(StartLocation, DungeonNodes[RoomIdx]->Doors[DoorIdx].Transform.GetLocation());
		DungeonHallwayPathFinder->FillAdditionalValidConnectionDirections();
		return;
	}

	/*The Idea:
	 * We select two doors to connect, then from the starting door we fallow the perimeter of the rooms until we reach the end door
	 */
	FindNextOrphanDoor();
	while (RoomIdx < DungeonNodes.Num())
	{

		
		FDungeonDoor& Start = DungeonNodes[RoomIdx]->Doors[DoorIdx];
		FVector StartDoorLocation = (Start.Transform*FTransform(DungeonNodes[RoomIdx]->Location)).GetLocation();
		FVector StartHallway = StartDoorLocation + Start.Transform.GetUnitAxis(EAxis::X) * (HallwayData->HallWaySectionDimensions.X * DIVIDE_BY_2);
		
		DoorIdx++;
		FindNextOrphanDoor();
		
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
        FindNextOrphanDoor();
	}
	
	//
	// //creating hallways based on room connections
	// for (const FDungeonConnection& Connection : DungeonConnections)
	// {
	// 	///Divide conection into segments based on vector components///
	// 	const FVector ConnectionStart = Connection.StartRoom->Location;
	// 	const FVector ConnectionEnd = Connection.EndRoom->Location;
	// 	FVector ConnectionComponents[4];
	// 	ConnectionComponents[0] = ConnectionStart;
	// 	ConnectionComponents[3] = ConnectionEnd;
	// 	
	// 	{
	// 		float ShortestDistanceToEndConnection = MAX_FLT;
	// 		for (int i = 0; i < 4; ++i)
	// 		{
	// 			FVector ExitPoint = ConnectionStart + DungeonHallwayPathFinder->CoreValidConnectionDirection[i] * (Connection.StartRoom->Extent);
	// 			// float Distance2DToEndConnection = FVector::DistSquared2D(ExitPoint, ConnectionEnd);
	// 			// if(Distance2DToEndConnection < HallwayData->HallWaySectionDimensions.X*HallwayData->HallWaySectionDimensions.X)
	// 			// {
	// 			// 	continue;
	// 			// }
	// 			float DistanceToEndConnection = FVector::DistSquared(ExitPoint, ConnectionEnd);
	// 			
	// 			if(DistanceToEndConnection < ShortestDistanceToEndConnection)
	// 			{
	// 				ShortestDistanceToEndConnection = DistanceToEndConnection;
	// 				ConnectionComponents[1] = DungeonHallwayPathFinder->CoreValidConnectionDirection[i];
	// 			}
	// 		}
	// 	}
	//
	// 	ConnectionComponents[0] = ConnectionStart + ConnectionComponents[1] * (Connection.StartRoom->Extent);
	// 	
	// 	{
	// 		float ShortestDistanceToEndConnection = MAX_FLT;
	// 		for (int i = 0; i < 4; ++i)
	// 		{
	// 			FVector EExitPoint = ConnectionEnd + DungeonHallwayPathFinder->CoreValidConnectionDirection[i] * (Connection.EndRoom->Extent);
	// 			FVector StartToEnd = EExitPoint - ConnectionComponents[0];
	// 			FVector SHallwaySegment = ConnectionComponents[0] + ConnectionComponents[1] *  StartToEnd * DIVIDE_BY_2;
	// 			FPlane Plane(ConnectionComponents[1],ConnectionComponents[0] + SHallwaySegment);
	// 			FVector InterectionPoint;
	// 			bool Interect = FMath::SegmentPlaneIntersection(EExitPoint, EExitPoint + DungeonHallwayPathFinder->CoreValidConnectionDirection[i] * (Connection.EndRoom->Extent), Plane, InterectionPoint);
	// 			if(Interect)
	// 			{
	// 				continue;
	// 			}
	// 			FVector ExitPoint = ConnectionEnd + DungeonHallwayPathFinder->CoreValidConnectionDirection[i] * (Connection.EndRoom->Extent);
	// 			float DistanceToEndConnection = FVector::DistSquared(ExitPoint, ConnectionComponents[0]);
	// 			if(DistanceToEndConnection < ShortestDistanceToEndConnection)
	// 			{
	// 				ShortestDistanceToEndConnection = DistanceToEndConnection;
	// 				ConnectionComponents[2] = DungeonHallwayPathFinder->CoreValidConnectionDirection[i];
	// 			}
	// 		}
	// 	}
	//
	// 	
	// 	ConnectionComponents[3] = ConnectionEnd + ConnectionComponents[2] * (Connection.EndRoom->Extent);
	//
	// 	FVector StartToEnd = ConnectionComponents[3] - ConnectionComponents[0];
	// 	ConnectionComponents[1] = ConnectionComponents[0] + ConnectionComponents[1] *  StartToEnd.GetAbs() * DIVIDE_BY_2;
	// 	ConnectionComponents[2] = ConnectionComponents[3] + ConnectionComponents[2] *  StartToEnd.GetAbs() * DIVIDE_BY_2;
	//
	// 	const FVector Slope = ConnectionComponents[2] - ConnectionComponents[1];
	// 	const FVector SlopeDirection = Slope.GetSafeNormal();
	// 	FVector SlopeBase = Slope;
	// 	SlopeBase.Z = 0.f;
	// 	SlopeBase.Normalize();
	// 	float Dot = FVector::DotProduct(SlopeDirection, SlopeBase);
	// 	float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(Dot));
	// 	if(SlopeAngle > MaxHallwaySlope)
	// 	{
	// 		float TanAngl = FMath::Tan(FMath::DegreesToRadians(MaxHallwaySlope));
	// 		float Adjacent = Slope.Z/TanAngl;
	// 		Adjacent*=DIVIDE_BY_2;
	// 		FVector Direction = ConnectionComponents[3] -  ConnectionComponents[2];
	// 		ConnectionComponents[2] = ConnectionComponents[3] - Direction.GetSafeNormal()*(Direction.Length() - Adjacent);
	// 		Direction = ConnectionComponents[1] -  ConnectionComponents[0];
	// 		ConnectionComponents[1] = ConnectionComponents[0] + Direction.GetSafeNormal()*(Direction.Length() - Adjacent);
	// 	}
	// 	/////////////////////////////////////////////////////////////
	// 	
	// 	///Create the doors info for the hallway, and the star and end connector to the respective rooms ////
	// 	FVector Start = ConnectionComponents[0];
	// 	FVector End = ConnectionComponents[1];
	// 	UDungeonHallwayData* NewHallwayConnection = CreateConnectionFromEdgePoint(Connection.StartRoom, Start, End);
	// 	if(NewHallwayConnection)
	// 	{
	// 		DungeonHallwaysData.AddUnique(NewHallwayConnection);
	// 	}
	// 	
	// 	Start =  ConnectionComponents[3];
	// 	End = ConnectionComponents[2];
	// 	NewHallwayConnection = CreateConnectionFromEdgePoint(Connection.EndRoom, Start, End);
	// 	if(NewHallwayConnection)
	// 	{
	// 		DungeonHallwaysData.AddUnique(NewHallwayConnection);
	// 	}
	// 	////////////////////////////////////////////////////////////
	//
	// 	/// create the hallway data from the conneciton components
	// 	for (int i = 0; i < 3; ++i)
	// 	{
	// 		if(ConnectionComponents[i].Equals(ConnectionComponents[i + 1]))
	// 		{
	// 			continue;
	// 		}
	// 		
	// 		UDungeonHallwayData* NewHallway = DuplicateObject(HallwayData, this);
	// 		NewHallway->Start = ConnectionComponents[i];
	// 		NewHallway->End = ConnectionComponents[i + 1];
	// 		NewHallway->Direction = (NewHallway->End - NewHallway->Start).GetSafeNormal();
	// 		const bool IsStairs = (NewHallway->End.Z - NewHallway->Start.Z) != 0;
	// 		NewHallway->Type = IsStairs ? ECorridorType::Stairs : ECorridorType::HStraight;
	// 		
	// 		DungeonHallwaysData.AddUnique(NewHallway);
	// 	}
	// 	////////////////////////////////////////////////////////////
	// }
	//
	// // some hallways might go through dungeon rooms, so we should split them to prevent this
	// if(bPreventCrossing)
	// {
	// 	TArray< UDungeonHallwayData*> NewHallways;
	// 	for (UDungeonHallwayData* HallWay : DungeonHallwaysData)
	// 	{
	// 		for (UDungeonRoomData* Room : DungeonNodes)
	// 		{
	// 				UDungeonHallwayData* NewHallway = FixHallwayCrossingRoom(Room,  HallWay->Start, HallWay->End);
	// 				if(NewHallway)
	// 				{
	// 					NewHallways.AddUnique(NewHallway);
	// 					HallWay->bIsInvalid = true;
	//
	// 					UDungeonHallwayData* NewHallwayConnection = CreateConnectionFromEdgePoint(Room, NewHallway->End, NewHallway->Start);
	// 					if(NewHallwayConnection)
	// 					{
	// 						NewHallways.AddUnique(NewHallwayConnection);
	// 					}
	// 				}
	//
	// 				NewHallway = FixHallwayCrossingRoom(Room,  HallWay->End, HallWay->Start);
	// 				if(NewHallway)
	// 				{
	// 					NewHallways.AddUnique(NewHallway);
	// 					HallWay->bIsInvalid = true;
	//
	// 					UDungeonHallwayData* NewHallwayConnection = CreateConnectionFromEdgePoint(Room,NewHallway->End, NewHallway->Start);
	// 					if(NewHallwayConnection)
	// 					{
	// 						NewHallways.AddUnique(NewHallwayConnection);
	// 					}
	// 				}
	// 			}
	// 	}
	//
	// 	
	// 	DungeonHallwaysData.RemoveAll([](const UDungeonHallwayData* OtherHallWay)
	// 		{
	// 			return OtherHallWay->bIsInvalid;
	// 		});
	// 	DungeonHallwaysData.Append(NewHallways);
	// }
	//
	//Merge Hallways that fallow the same path
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
	
	if(bCreateCorners)
	{
		/// Create Corner sections of hallways
	 	NumHallways = DungeonHallwaysData.Num();
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
	
	 			FVector IntersectionPoint1 = FVector::ZeroVector;
	 			FVector IntersectionPoint2 = FVector::ZeroVector;
	 			FMath::SegmentDistToSegmentSafe(FirstHallway->Start, FirstHallway->End, SecondHallway->Start, SecondHallway->End, IntersectionPoint1, IntersectionPoint2);
	 			bool bIntersect = IntersectionPoint1.Equals(IntersectionPoint2);
	 			if(bIntersect)
	 			{

	 				UDungeonHallwayData* HallwayOrder[2];
	 				FVector CornerPoint = IntersectionPoint1;
	 				if(FirstHallway->End.Equals(CornerPoint))
	 				{
	 					HallwayOrder[0] = FirstHallway;
	 					HallwayOrder[1] = SecondHallway;
	 				}
	 				else if(FirstHallway->Start.Equals(CornerPoint))
	 				{
	 					HallwayOrder[0] = SecondHallway;
	 					HallwayOrder[1] = FirstHallway;
	 				}
	 				else if(SecondHallway->End.Equals(CornerPoint))
	 				{
	 					HallwayOrder[0] = SecondHallway;
	 					HallwayOrder[1] = FirstHallway;
	 				}
	 				else if(SecondHallway->Start.Equals(CornerPoint))
	 				{
	 					HallwayOrder[0] = FirstHallway;
	 					HallwayOrder[1] = SecondHallway;
	 					
	 				}
				    else
				    {
					    continue;
				    }
	 				
	 				const FVector HallwayStartDirection = (CornerPoint - HallwayOrder[0]->Start).GetSafeNormal();
	 				const FVector HallwayEndDirection = (HallwayOrder[1]->End - CornerPoint).GetSafeNormal();
	 				
	 				UDungeonHallwayData* NewCorner = DuplicateObject(HallwayData, this);
	 				NewCorner->Start = CornerPoint - HallwayStartDirection * (NewCorner->HallWaySectionDimensions.X * DIVIDE_BY_2);
	 				NewCorner->End = CornerPoint + HallwayEndDirection * (NewCorner->HallWaySectionDimensions.X * DIVIDE_BY_2);

	 				const FVector CornerMidPoint = NewCorner->Start + ((NewCorner->End - NewCorner->Start) * 0.5);
	 				NewCorner->Direction = (CornerPoint - CornerMidPoint).GetSafeNormal();

	 				const bool IsStairs = (NewCorner->End.Z - NewCorner->Start.Z) != 0;
	 				NewCorner->Type = IsStairs ? ECorridorType::StairConnection : ECorridorType::HCorner;

	 				DungeonHallwaysData.AddUnique(NewCorner);

	 				HallwayOrder[0]->End = NewCorner->Start;
	 				HallwayOrder[1]->Start = NewCorner->End;
	 			}
	 		}
	 	}
	}	
}

void ADungeonMapper::RenderDungeon()
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_RenderDungeon);
	UDynamicMesh* MainDynMesh = DynamicMeshComponent->GetDynamicMesh();
	MainDynMesh->Reset();
	// FGeometryScriptSetSimpleCollisionOptions Options;
	// DungeonCollision.AggGeom.EmptyElements();
	// UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(DungeonCollision, DynamicMeshComponent, Options);
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
		
	// for (const UDungeonHallwayData* DungeonHallway : DungeonHallwaysData)
	// {
	// 	HallowHallWays(MainDynMesh, DungeonHallway);
	// }

	FGeometryScriptMergeSimpleCollisionOptions MergeOptions;
	bool bHasMerged = false;
	
	//UGeometryScriptLibrary_CollisionFunctions::SetSimpleCollisionOfDynamicMeshComponent(DungeonCollision, DynamicMeshComponent, Options);
	
}

void ADungeonMapper::ClearAll()
{
	FlushPersistentDebugLines(GetWorld());
	UDynamicMesh* DynMesh = DynamicMeshComponent->GetDynamicMesh();
	DynMesh->Reset();
	DungeonHallwaysData.Empty();
	DungeonNodes.Empty();
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

UDungeonRoomData* ADungeonMapper::CreateRoom(FRandomStream& RandomStream)
{
	int32 RoomXSize = RandomStream.RandRange(RoomXExtent.X, RoomXExtent.Y);
	int32 RoomYSize = RandomStream.RandRange(RoomYExtent.X, RoomYExtent.Y);
	int32 RoomZSize = RandomStream.RandRange(RoomZExtent.X, RoomZExtent.Y);

	UDungeonRoomData* NewRoom = DuplicateObject(RoomData, this);
	NewRoom->Location = DungeonBounds.GetCenter();
	NewRoom->Extent = FVector(RoomXSize, RoomYSize, RoomZSize);

	int32 NumDoors = RandomStream.RandRange(1, 4);
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
		FTransform DoorTransform = GenerateDoorOnRoomWall(RandomStream, NewRoom->Extent, DoorForward, SlidingVector);
		NewRoom->Doors.Add(FDungeonDoor(DoorTransform, NewRoom));
	}
	
	return NewRoom;
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
							, true,true,3,0, 0
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