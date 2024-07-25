// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonMapper.h"

#include "DungeonRoom.h"
#include "GeometryScriptLibrary_DungeonGenerationFunctions.h"
#include "NavigationSystem.h"
#include "TriangulatorData.h"
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
	bShowRooms(false),
	bShowConnections(false)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = true;

	DynamicMeshComponent = CreateDefaultSubobject<UDynamicMeshComponent>(TEXT("DynamicMeshComponent"));
	RootComponent = DynamicMeshComponent;


	for (ECorridorType Type : TEnumRange<ECorridorType>())
	{
		HallwayDebugColors.Add(Type, FColor::Orange);
	}
}

void ADungeonMapper::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RunHallwaysCreation();
	RunPhysics(DeltaSeconds);
	Debug();
}

void ADungeonMapper::RunHallwaysCreation()
{
	if(bIsCreatingHallways && !DungeonConnections.IsEmpty())
	{
		if(ConnectionID< 0 && ConnectionID > DungeonConnections.Num())
		{
			bIsCreatingHallways = false;
		}

		FDungeonConnection& EvaluatedConnection = DungeonConnections[ConnectionID];
		if(HallWayPaths.IsEmpty())
		{
			FVector PossibleRoomExitsDirections[4];
			PossibleRoomExitsDirections[0] = FVector::ForwardVector;
			PossibleRoomExitsDirections[1] = FVector::BackwardVector;
			PossibleRoomExitsDirections[2] = FVector::RightVector;
			PossibleRoomExitsDirections[3] = FVector::LeftVector;

			const FVector ConnectionStart = EvaluatedConnection.StartRoom->Location;
			const FVector ConnectionEnd = EvaluatedConnection.EndRoom->Location;

			float ShortestDistanceToEndConnection = MAX_FLT;
			for (int i = 0; i < 4; ++i)
			{
				FVector ExitPoint = ConnectionStart + PossibleRoomExitsDirections[i] * (EvaluatedConnection.StartRoom->Extent);
				float DistanceToEndConnection = FVector::DistSquared(ExitPoint, ConnectionEnd);
			
				if(DistanceToEndConnection < ShortestDistanceToEndConnection)
				{
					ShortestDistanceToEndConnection = DistanceToEndConnection;
					HallWayPaths.Add(ConnectionStart + PossibleRoomExitsDirections[i] * EvaluatedConnection.StartRoom->Extent);
					HallWayPaths.Add(HallWayPaths[0] + PossibleRoomExitsDirections[i] * HallwayData->HallWaySectionDimensions.X);
				}
			}
			return;
		}
		
		
	}
}

void ADungeonMapper::RunPhysics(float DeltaSeconds)
{
	if(bIsCollapsing && !DungeonNodes.IsEmpty())
	{
		bIsCollapsing = false;
		for (int i = 0; i < DungeonNodes.Num(); ++i)
		{
			UDungeonRoomData* FirstNode = DungeonNodes[i];
			FSphere FirstNodeSphere(FirstNode->Location, FirstNode->Extent.Size() + HallwayData->HallWaySectionDimensions.X);
			FVector Force = FVector::ZeroVector;
			DrawDebugSphere(GetWorld(), FirstNode->Location, FirstNodeSphere.W, 32, FColor::Green);
			if(FirstNode->RoomType == ERoomType::Starting || FirstNode->RoomType == ERoomType::End)
			{
				continue;
			}
			
			if(bApplyNodeRepulsion)
			{
				
				for (int j = 0; j < DungeonNodes.Num(); ++j)
				{
				
					UDungeonRoomData* SecondNode = DungeonNodes[j];
					if(FirstNode == SecondNode)
					{
						continue;
					}
					FSphere SecondNodeSphere(SecondNode->Location, SecondNode->Extent.Size() + HallwayData->HallWaySectionDimensions.X);
					FVector InteractionDirection = FirstNode->Location - SecondNode->Location;
					const float NodeSeparation = InteractionDirection.Size();
					const float Distance = NodeSeparation - (FirstNodeSphere.W + SecondNodeSphere.W);
					constexpr float G = 6.6743E-11;
					const float FirstSphereVolume = FirstNodeSphere.GetVolume();
					const float SecondSphereVolume = SecondNodeSphere.GetVolume();
					const float RepulsionMagnitude = G * (FirstSphereVolume)  * (SecondSphereVolume) / (Distance * Distance);
					Force += InteractionDirection.GetSafeNormal() * RepulsionMagnitude;
				}
				DrawDebugDirectionalArrow(GetWorld(), FirstNode->Location, FirstNode->Location+Force,10.0f,  FColor::Blue);
				uint64 InnerKey = GetTypeHash(FirstNode->GetName()+ "RepulsionForce");
				GEngine->AddOnScreenDebugMessage(InnerKey, 2.0f, FColor::Blue, FString::Printf(TEXT("Repulsion: [M]%f | [D]%s"), Force.Length(), *Force.GetSafeNormal().ToString()));
			}			
			
			//Hooks Law Fspring = -K*X
			// X = current length - resting length
			// K = spring Constant
			if(bApplySpringForce)
			{
				for (const FDungeonConnection* Connection : FirstNode->Connections)
				{
					FSphere SecondNodeSphere;
					if(Connection->StartRoom == FirstNode)
					{
						SecondNodeSphere = FSphere(Connection->EndRoom->Location, Connection->EndRoom->Extent.Size());
					}
					else
					{
						SecondNodeSphere = FSphere(Connection->StartRoom->Location, Connection->StartRoom->Extent.Size());
					}
					
					FVector Spring = FirstNodeSphere.Center - SecondNodeSphere.Center;
					const float RestingDistance = FirstNodeSphere.W + SecondNodeSphere.W;
					const float SpringSize = Spring.Size();
					const float X = SpringSize - RestingDistance;
					const FVector SpringForce = -SpringConstant * X * Spring.GetSafeNormal();
					
					DrawDebugPoint(GetWorld(), SecondNodeSphere.Center + Spring.GetSafeNormal() * RestingDistance, 10.0f, FColor::Green);
					DrawDebugPoint(GetWorld(), FirstNodeSphere.Center, 10.0f, FColor::Red);
					DrawDebugPoint(GetWorld(), SecondNodeSphere.Center, 10.0f, FColor::Blue);
					DrawDebugDirectionalArrow(GetWorld(), FirstNodeSphere.Center,FirstNodeSphere.Center + SpringForce, 10.0f, FColor::Red);
					uint64 InnerKey = GetTypeHash(FirstNode->GetName()+ "SpringForce");
					GEngine->AddOnScreenDebugMessage(InnerKey, 2.0f, FColor::Red, FString::Printf(TEXT("Spring: [M]%f | [D]%s"), SpringForce.Length(), *SpringForce.GetSafeNormal().ToString()));
					Force += SpringForce;
				}			
			}
			FirstNode->Velocity += Force;
		}
		
		FVector MinDungeonBounds = DungeonNodes[0]->Location -  DungeonNodes[0]->Extent;
		FVector MaxDungeonBounds =  DungeonNodes[0]->Location +  DungeonNodes[0]->Extent;
		bool IsStaticIteration = true;
		for (UDungeonRoomData* Node : DungeonNodes)
		{
			Node->PrevLocation = Node->Location;
			Node->Location += Node->Velocity*DeltaSeconds;
			Node->Velocity *= SpringForcePreservation;
			
			FVector RoomMin = Node->Location - Node->Extent;
			FVector RoomMax = Node->Location + Node->Extent;
			
			MinDungeonBounds.X = FMath::Min(RoomMin.X, MinDungeonBounds.X);
			MinDungeonBounds.Y = FMath::Min(RoomMin.Y, MinDungeonBounds.Y);
			MinDungeonBounds.Z = FMath::Min(RoomMin.Z, MinDungeonBounds.Z);

			MaxDungeonBounds.X = FMath::Max(RoomMax.X, MaxDungeonBounds.X);
			MaxDungeonBounds.Y = FMath::Max(RoomMax.Y, MaxDungeonBounds.Y);
			MaxDungeonBounds.Z = FMath::Max(RoomMax.Z, MaxDungeonBounds.Z);

			const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
			FBox NavBounds =NavSys->GetNavigableWorldBounds();
			NavBounds = NavBounds.ExpandBy(-Node->Extent);
			Node->Location = NavBounds.GetClosestPointTo(Node->Location);
			if(!Node->Location.Equals(Node->PrevLocation))
			{
				IsStaticIteration = false;
			}
			if(!Node->Velocity.IsNearlyZero(0.0001))
			{
				bIsCollapsing = true;
			}
		}
		DungeonBounds = FBox(MinDungeonBounds, MaxDungeonBounds);
		if(IsStaticIteration)
		{
			CollapsingIterationNotModified++;
		}
		if(CollapsingIterationNotModified >= 10/*MaxStaticIterations*/)
		{
			bIsCollapsing = false;
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
			DrawDebugBox(GetWorld(), DungeonsRoom->Location, DungeonsRoom->Extent,DungeonColor, false, -1, 0, 2);
			DrawDebugDirectionalArrow(GetWorld(), DungeonsRoom->Location, DungeonsRoom->Location + DungeonsRoom->Velocity, 10.0f, FColor::Black, false, -1, 0, 2);
			uint64 InnerKey = GetTypeHash(DungeonsRoom->GetName()+ "Velocity");
			GEngine->AddOnScreenDebugMessage(InnerKey, 2.0f, FColor::Green, FString::Printf(TEXT("Velocity:  [M]%f | [D]%s"), DungeonsRoom->Velocity.Length(), *DungeonsRoom->Velocity.ToString()));
		}
	}

	if(bShowConnections)
	{
		for (const FDungeonConnection& Connection : DungeonConnections)
		{
			DrawDebugLine(GetWorld(), Connection.StartRoom->Location, Connection.EndRoom->Location, FColor::Yellow, false, -1, 0, 10);
		}
	}
	if(bShowHallways)
	{
		for (const UDungeonHallwayData* HallWay : DungeonHallwaysData)
		{
			FColor* DebugColor = HallwayDebugColors.Find(HallWay->Type);
			DrawDebugDirectionalArrow(GetWorld(), HallWay->Start, HallWay->Start + (HallWay->End - HallWay->Start)*0.5f, 1000,DebugColor ? *DebugColor : FColor::Orange, false, -1, 0, 10 );
			DrawDebugLine(GetWorld(), HallWay->Start, HallWay->End, DebugColor ? *DebugColor : FColor::Orange, false, -1, 0, 10);
		}
	}
	if(bShowBounds)
	{
		DrawDebugBox(GetWorld(), DungeonBounds.GetCenter(), DungeonBounds.GetExtent(), FColor::Blue, false, -1, 0, 2);
	}
}

void ADungeonMapper::GenerateDungeonRooms()
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_GenerateRooms);
	const UNavigationSystemV1* NavSys = UNavigationSystemV1::GetCurrent(GetWorld());
	DungeonNodes.Empty(MaxRooms);
	DungeonConnections.Empty();
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

	//Divde spawn area in equal spaced cells of max size of rooms
	const int32 XCells = DungeonBounds.GetExtent().X / (RoomXExtent.Y);
	const int32 YCells = DungeonBounds.GetExtent().Y / (RoomYExtent.Y);
	const int32 ZCells = DungeonBounds.GetExtent().Z / (RoomZExtent.Y);

	FBox CellBounds(FVector::ZeroVector, FVector::ZeroVector);
	CellBounds = CellBounds.ExpandBy(FVector(DungeonBounds.GetExtent().X / XCells,  DungeonBounds.GetExtent().Y / YCells, DungeonBounds.GetExtent().Z / ZCells));
	const FVector CellStartingLocation = DungeonBounds.Min - CellBounds.Min;

	FRandomStream RandomStream(RandomSeed);
	FVector MinDungeonBounds = FVector::ZeroVector;
	FVector MaxDungeonBounds = FVector::ZeroVector;
	
	const int32 RoomsToGenerate = RandomStream.RandRange(FMath::Min(MinRooms,XCells * YCells * ZCells) , FMath::Min(MaxRooms,XCells * YCells * ZCells));
	TArray<FVector> UsedUpCells;
	for(int32 i = 0; i < RoomsToGenerate; i++)
	{
		// Randomise the size of the room, and in what cell we want to spawn it.
		// reduce the bounds of the cell by the size of the room, so we are sure the room won't go outside the spawn area.
		// randomise a location in the narrowed down Cell bounds.
		const FVector CellExtend = CellBounds.GetExtent()*2.0f;
		
		const int32 RoomXSize = RandomStream.RandRange(RoomXExtent.X, RoomXExtent.Y);
		const int32 RoomYSize = RandomStream.RandRange(RoomYExtent.X, RoomYExtent.Y);
		const int32 RoomZSize = RandomStream.RandRange(RoomZExtent.X, RoomZExtent.Y);
		const FVector RoomSize(RoomXSize, RoomYSize, RoomZSize);

		//Make sure we dont use a cell already with a room
		FVector CellCoord = FVector::ZeroVector;
		do
		{
			const int32 XCell = RandomStream.RandRange(0, XCells - 1);
			const int32 YCell = RandomStream.RandRange(0, YCells - 1);
			const int32 ZCell = RandomStream.RandRange(0, ZCells - 1);
			CellCoord = FVector(XCell, YCell, ZCell);
			
		}while(UsedUpCells.Contains(CellCoord));
		
		UsedUpCells.Add(CellCoord);
		
		const float XPosition = CellCoord.X*CellExtend.X+CellStartingLocation.X;
		const float YPosition = CellCoord.Y*CellExtend.Y+CellStartingLocation.Y;
		const float ZPosition = CellCoord.Z*CellExtend.Z+CellStartingLocation.Z;

		FBox SpawnerCellBounds = CellBounds.ExpandBy( -FVector(RoomXSize, RoomYSize ,RoomZSize));
		SpawnerCellBounds = SpawnerCellBounds.MoveTo(FVector(XPosition, YPosition, ZPosition));
	
		const FVector RoomLocation = RandomStream.RandPointInBox(SpawnerCellBounds);

		UDungeonRoomData* NewRoom = DuplicateObject(RoomData, this);
		NewRoom->Location = RoomLocation;
		NewRoom->Extent = RoomSize;
		
		DungeonNodes.Add(NewRoom);
		
		FVector RoomMin = RoomLocation - RoomSize;
		FVector RoomMax = RoomLocation + RoomSize;

		if(i==0)
		{
			MinDungeonBounds = RoomMin;
			MaxDungeonBounds = RoomMax;
		}
		else
		{
			MinDungeonBounds.X = FMath::Min(RoomMin.X, MinDungeonBounds.X);
			MinDungeonBounds.Y = FMath::Min(RoomMin.Y, MinDungeonBounds.Y);
			MinDungeonBounds.Z = FMath::Min(RoomMin.Z, MinDungeonBounds.Z);

			MaxDungeonBounds.X = FMath::Max(RoomMax.X, MaxDungeonBounds.X);
			MaxDungeonBounds.Y = FMath::Max(RoomMax.Y, MaxDungeonBounds.Y);
			MaxDungeonBounds.Z = FMath::Max(RoomMax.Z, MaxDungeonBounds.Z);
		}
	}
	DungeonBounds = FBox(MinDungeonBounds, MaxDungeonBounds);
}

void ADungeonMapper::ConnectRooms()
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_ConnectRooms);
	DungeonConnections.Empty();
	DungeonHallwaysData.Empty();
	TArray<FTetrahedron> Tetrahedrons;
	if(DungeonNodes.IsEmpty())
	{
		return;
	}
	
	//Generate a big enough tetra so that it engulf all vertex(rooms) ************************************************//
	float minX = DungeonNodes[0]->Location.X;
	float minY = DungeonNodes[0]->Location.Y;
	float minZ = DungeonNodes[0]->Location.Z;
	float maxX = minX;
	float maxY = minY;
	float maxZ = minZ;

	for (const UDungeonRoomData* Room : DungeonNodes) {
		if (Room->Location.X < minX) minX = Room->Location.X;
		if (Room->Location.X > maxX) maxX = Room->Location.X;
		if (Room->Location.Y < minY) minY = Room->Location.Y;
		if (Room->Location.Y > maxY) maxY = Room->Location.Y;
		if (Room->Location.Z < minZ) minZ = Room->Location.Z;
		if (Room->Location.Z > maxZ) maxZ = Room->Location.Z;
	}

	float dx = maxX - minX;
	float dy = maxY - minY;
	float dz = maxZ - minZ;
	float deltaMax = FMath::Max3(dx, dy, dz) * 2;
	
	const FVector V1 = FVector(minX - 1, minY - 1, minZ - 1);
	const FVector V2 = FVector(maxX + deltaMax, minY - 1, minZ - 1);
	const FVector V3 =  FVector(minX - 1, maxY + deltaMax, minZ - 1);
	const FVector V4 = FVector(minX - 1, minY - 1, maxZ + deltaMax);

	UDungeonRoomData* DummyRoom1 = DuplicateObject(RoomData, this);
	DummyRoom1->Location = V1;
	DummyRoom1->Extent = FVector::ZeroVector;
	DummyRoom1->RoomType = ERoomType::Dummy;
	
	UDungeonRoomData* DummyRoom2 = DuplicateObject(RoomData, this);
	DummyRoom2->Location = V2;
	DummyRoom2->Extent = FVector::ZeroVector;
	DummyRoom2->RoomType = ERoomType::Dummy;
	
	UDungeonRoomData* DummyRoom3 = DuplicateObject(RoomData, this);
	DummyRoom3->Location = V3;
	DummyRoom3->Extent = FVector::ZeroVector;
	DummyRoom3->RoomType = ERoomType::Dummy;
	
	UDungeonRoomData* DummyRoom4 = DuplicateObject(RoomData, this);
	DummyRoom4->Location = V4;
	DummyRoom4->Extent = FVector::ZeroVector;
	DummyRoom4->RoomType = ERoomType::Dummy;
	
	Tetrahedrons.Add(FTetrahedron(DummyRoom1, DummyRoom2, DummyRoom3, DummyRoom4));
	//****************************************************************************************************************//
	
	for (UDungeonRoomData* DungeonsNode : DungeonNodes)
	{
		EvaluateVertex(DungeonsNode, Tetrahedrons);
	}
	
	GenerateConnectionFromTetras(Tetrahedrons);
}

void ADungeonMapper::SimplifyConnections()
{
	if(DungeonNodes.IsEmpty())
	{
		return;
	}
	SCOPE_SECONDS_ACCUMULATOR(STAT_SimplifyConections);
	//To simplify the hallways generated by the Delauney algo we are going to path from a fixed room to all other and
	//removing all edges not used in any path
	UDungeonRoomData** StartingRoom = DungeonNodes.FindByPredicate([](const UDungeonRoomData* Room){ return Room->RoomType == ERoomType::Starting;});
	FRandomStream RandomStream(RandomSeed);
	if(!StartingRoom)
	{
		const int32 StartingRoomIdx = RandomStream.RandRange(0, DungeonNodes.Num() - 1);
		StartingRoom = &DungeonNodes[StartingRoomIdx];
		(*StartingRoom)->RoomType = ERoomType::Starting;
	}
		
	TArray<UDungeonRoomData*> PathedRooms;
	TArray<FDungeonPath> Paths;
	PathedRooms.Add(*StartingRoom);

	
	for (int RoomIdx = 0; RoomIdx < DungeonNodes.Num(); ++RoomIdx)
	{
		UDungeonRoomData* RoomToPath = DungeonNodes[RoomIdx];
		if(PathedRooms.Contains(RoomToPath))
		{
			continue;
		}
		FDungeonPath NewPath;
		NewPath.Start = *StartingRoom;
		NewPath.End = RoomToPath;
		
		//Start Pathing
		TArray<FPathNode> OpenRooms;
		TArray<FPathNode> ClosedRooms;
		OpenRooms.Add(*StartingRoom);
		
		while (!OpenRooms.IsEmpty())
		{
			FPathNode CurrentRoom = OpenRooms.Pop();
			ClosedRooms.Add(CurrentRoom.Room);

			if(CurrentRoom.Room == RoomToPath)
			{
				NewPath.AssignPath(CurrentRoom.Path);
				Paths.Add(NewPath);
				break;
			}

			TArray<FPathNode> ConnectedRooms;
			for (const FDungeonConnection* HallWay : CurrentRoom.Room->Connections)
			{
				ConnectedRooms.Add(FPathNode(CurrentRoom, HallWay));
			}

			for (FPathNode& Room : ConnectedRooms)
			{
				if(ClosedRooms.Contains(Room))
				{
					continue;
				}

				Room.G = CurrentRoom.G + FVector::DistSquared(CurrentRoom.Room->Location, Room.Room->Location);
				Room.H = FVector::DistSquared(RoomToPath->Location, Room.Room->Location);
				Room.F = Room.G + Room.H;

				int32 RoomIndex = OpenRooms.Find(Room);
				if(RoomIndex != INDEX_NONE && Room < OpenRooms[RoomIndex])
				{
					continue;
				}
				
				OpenRooms.Push(Room);
			}
			OpenRooms.Sort();
		}
		PathedRooms.Add(RoomToPath);
	}

	//if you whant to keep some hallways even if they are not used in any path jus add the conditions below
	//EG: Randomize when returning true to have a chance to not remove the hallway.
	DungeonConnections.RemoveAll([&Paths](const FDungeonConnection& HallWay)
	{
		for (const FDungeonPath& Path : Paths)
		{
			for (const FDungeonConnection* PathHallway : Path.Path)
			{
				const bool StartRoomDup = PathHallway->StartRoom == HallWay.StartRoom || PathHallway->StartRoom == HallWay.EndRoom;
				const bool EndRoomDup = PathHallway->EndRoom == HallWay.StartRoom || PathHallway->EndRoom == HallWay.EndRoom;
				if(StartRoomDup && EndRoomDup)
				{
					return false;
				}
			}
		}
		return true;
	});

	for (UDungeonRoomData* DungeonsRoom : DungeonNodes)
	{
		DungeonsRoom->Connections.Empty();
	}

	for (const FDungeonConnection& Connection : DungeonConnections)
	{
		Connection.StartRoom->Connections.Add(&Connection);
		Connection.EndRoom->Connections.Add(&Connection);
	}
	
	//Marking the room at the end of the longest path as the boss room
	UDungeonRoomData* FurthestRoom = *StartingRoom;
	float FurthestDistance = 0;
	
	for (const FDungeonPath& Path : Paths)
	{
		if(Path.DistanceTraveled > FurthestDistance)
		{
			FurthestDistance = Path.DistanceTraveled;
			FurthestRoom = Path.End;
		}
	}
	FurthestRoom->RoomType = ERoomType::End;
}

void ADungeonMapper::Collapse()
{
	bIsCollapsing = true;
	CollapsingIterationNotModified = 0;
}

UDungeonHallwayData* ADungeonMapper::CreateHallwayEdgePoint(UDungeonRoomData* ConnectedRoom, FVector Start, FVector End)
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
		NewHallway->End = NewHallway->Start + NewHallway->Direction * (NewHallway->HallWaySectionDimensions.X * 0.5f);
		const bool IsVerticalCorridor = NewHallway->Direction.GetAbs().Equals(FVector::UpVector);
		NewHallway->Type = IsVerticalCorridor ? ECorridorType::VRoomConnection : ECorridorType::HRoomConnection;
		return NewHallway;
	}
	return nullptr;
}

void ADungeonMapper::CreateHallways()
{
	DungeonHallwaysData.Empty();
	//bIsCreatingHallways = true;
	//ConnectionID = 0;
	//return;
	//creating hallways based on room connections
	for (const FDungeonConnection& Connection : DungeonConnections)
	{
		///Divide conection into segments based on vector components///

		FVector PossibleRoomExitsDirections[4];
		PossibleRoomExitsDirections[0] = FVector::ForwardVector;
		PossibleRoomExitsDirections[1] = FVector::BackwardVector;
		PossibleRoomExitsDirections[2] = FVector::RightVector;
		PossibleRoomExitsDirections[3] = FVector::LeftVector;
		
		const FVector ConnectionStart = Connection.StartRoom->Location;
		const FVector ConnectionEnd = Connection.EndRoom->Location;
		FVector ConnectionComponents[4];
		ConnectionComponents[0] = ConnectionStart;
		ConnectionComponents[3] = ConnectionEnd;
		
		{
			float ShortestDistanceToEndConnection = MAX_FLT;
			for (int i = 0; i < 4; ++i)
			{
				FVector ExitPoint = ConnectionStart + PossibleRoomExitsDirections[i] * (Connection.StartRoom->Extent);
				// float Distance2DToEndConnection = FVector::DistSquared2D(ExitPoint, ConnectionEnd);
				// if(Distance2DToEndConnection < HallwayData->HallWaySectionDimensions.X*HallwayData->HallWaySectionDimensions.X)
				// {
				// 	continue;
				// }
				float DistanceToEndConnection = FVector::DistSquared(ExitPoint, ConnectionEnd);
				
				if(DistanceToEndConnection < ShortestDistanceToEndConnection)
				{
					ShortestDistanceToEndConnection = DistanceToEndConnection;
					ConnectionComponents[1] = PossibleRoomExitsDirections[i];
				}
			}
		}

		ConnectionComponents[0] = ConnectionStart + ConnectionComponents[1] * (Connection.StartRoom->Extent);
		
		{
			float ShortestDistanceToEndConnection = MAX_FLT;
			for (int i = 0; i < 4; ++i)
			{
				FVector EExitPoint = ConnectionEnd + PossibleRoomExitsDirections[i] * (Connection.EndRoom->Extent);
				FVector StartToEnd = EExitPoint - ConnectionComponents[0];
				FVector SHallwaySegment = ConnectionComponents[0] + ConnectionComponents[1] *  StartToEnd * 0.5f;
				FPlane Plane(ConnectionComponents[1],ConnectionComponents[0] + SHallwaySegment);
				FVector InterectionPoint;
				bool Interect = FMath::SegmentPlaneIntersection(EExitPoint, EExitPoint + PossibleRoomExitsDirections[i] * (Connection.EndRoom->Extent), Plane, InterectionPoint);
				if(Interect)
				{
					continue;
				}
				FVector ExitPoint = ConnectionEnd + PossibleRoomExitsDirections[i] * (Connection.EndRoom->Extent);
				float DistanceToEndConnection = FVector::DistSquared(ExitPoint, ConnectionComponents[0]);
				if(DistanceToEndConnection < ShortestDistanceToEndConnection)
				{
					ShortestDistanceToEndConnection = DistanceToEndConnection;
					ConnectionComponents[2] = PossibleRoomExitsDirections[i];
				}
			}
		}

		
		ConnectionComponents[3] = ConnectionEnd + ConnectionComponents[2] * (Connection.EndRoom->Extent);

		FVector StartToEnd = ConnectionComponents[3] - ConnectionComponents[0];
		ConnectionComponents[1] = ConnectionComponents[0] + ConnectionComponents[1] *  StartToEnd.GetAbs() * 0.5f;
		ConnectionComponents[2] = ConnectionComponents[3] + ConnectionComponents[2] *  StartToEnd.GetAbs() * 0.5f;

		const FVector Slope = ConnectionComponents[2] - ConnectionComponents[1];
		const FVector SlopeDirection = Slope.GetSafeNormal();
		FVector SlopeBase = Slope;
		SlopeBase.Z = 0.f;
		SlopeBase.Normalize();
		float Dot = FVector::DotProduct(SlopeDirection, SlopeBase);
		float SlopeAngle = FMath::RadiansToDegrees(FMath::Acos(Dot));
		if(SlopeAngle > MaxHallwaySlope)
		{
			float TanAngl = FMath::Tan(FMath::DegreesToRadians(MaxHallwaySlope));
			float Adjacent = Slope.Z/TanAngl;
			Adjacent*=0.5f;
			FVector Direction = ConnectionComponents[3] -  ConnectionComponents[2];
			ConnectionComponents[2] = ConnectionComponents[3] - Direction.GetSafeNormal()*(Direction.Length() - Adjacent);
			Direction = ConnectionComponents[1] -  ConnectionComponents[0];
			ConnectionComponents[1] = ConnectionComponents[0] + Direction.GetSafeNormal()*(Direction.Length() - Adjacent);
		}
		/////////////////////////////////////////////////////////////
		
		///Create the doors info for the hallway, and the star and end connector to the respective rooms ////
		FVector Start = ConnectionComponents[0];
		FVector End = ConnectionComponents[1];
		UDungeonHallwayData* NewHallwayConnection = CreateHallwayEdgePoint(Connection.StartRoom, Start, End);
		if(NewHallwayConnection)
		{
			DungeonHallwaysData.AddUnique(NewHallwayConnection);
		}
		
		Start =  ConnectionComponents[3];
		End = ConnectionComponents[2];
		NewHallwayConnection = CreateHallwayEdgePoint(Connection.EndRoom, Start, End);
		if(NewHallwayConnection)
		{
			DungeonHallwaysData.AddUnique(NewHallwayConnection);
		}
		////////////////////////////////////////////////////////////

		/// create the hallway data from the conneciton components
		for (int i = 0; i < 3; ++i)
		{
			if(ConnectionComponents[i].Equals(ConnectionComponents[i + 1]))
			{
				continue;
			}
			
			UDungeonHallwayData* NewHallway = DuplicateObject(HallwayData, this);
			NewHallway->Start = ConnectionComponents[i];
			NewHallway->End = ConnectionComponents[i + 1];
			NewHallway->Direction = (NewHallway->End - NewHallway->Start).GetSafeNormal();
			const bool IsStairs = (NewHallway->End.Z - NewHallway->Start.Z) != 0;
			NewHallway->Type = IsStairs ? ECorridorType::Stairs : ECorridorType::HStraight;
			
			DungeonHallwaysData.AddUnique(NewHallway);
		}
		////////////////////////////////////////////////////////////
	}
	
	// some hallways might go through dungeon rooms, so we should split them to prevent this
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
	
						UDungeonHallwayData* NewHallwayConnection = CreateHallwayEdgePoint(Room, NewHallway->End, NewHallway->Start);
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
	
						UDungeonHallwayData* NewHallwayConnection = CreateHallwayEdgePoint(Room,NewHallway->End, NewHallway->Start);
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
	UDynamicMesh* DynMesh = DynamicMeshComponent->GetDynamicMesh();
	DynMesh->Reset();
	DungeonConnections.Empty();
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

void ADungeonMapper::NextStep()
{
	FlushPersistentDebugLines(GetWorld());
}

void ADungeonMapper::EvaluateVertex(UDungeonRoomData* Vertex, TArray<FTetrahedron>& OutTetrahedrons) const
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_ConnectRooms);
	TArray<FTriangle> Triangles;
	Triangles.Reserve(OutTetrahedrons.Num() * 4);
	
	for (FTetrahedron& Tetra : OutTetrahedrons)
	{
		// if vert falls inside cirmushpere then this tetra is invalid
		//we break it up into its triangles
		if(Tetra.CircumSphereContains(Vertex))
		{
			Tetra.bIsBad = true;
			Triangles.Add(FTriangle(Tetra.Vert1, Tetra.Vert2, Tetra.Vert3));
			Triangles.Add(FTriangle(Tetra.Vert1, Tetra.Vert2, Tetra.Vert4));
			Triangles.Add(FTriangle(Tetra.Vert1, Tetra.Vert3, Tetra.Vert4));
			Triangles.Add(FTriangle(Tetra.Vert2, Tetra.Vert3, Tetra.Vert4));
		}
	}

	//we mark as invalid triangles any duplicates
	for (int i = 0; i < Triangles.Num(); ++i)
	{
		for (int j = i + 1; j < Triangles.Num(); ++j)
		{
			if(FTriangle::AlmostEqual(Triangles[i], Triangles[j]))
			{
				Triangles[i].bIsBad = true;
				Triangles[j].bIsBad = true;
			}
		}
	}

	OutTetrahedrons.RemoveAll([](const FTetrahedron& Tetra)
	{
		return Tetra.bIsBad;
	});
	Triangles.RemoveAll([](const FTriangle& Triangle)
	{
		return Triangle.bIsBad;
	});

	//generate new tetrahedrons from remaining triangles and new vertex
	for (const FTriangle& Triangle : Triangles)
	{
		FTetrahedron NewTetra(Triangle.Vert1, Triangle.Vert2, Triangle.Vert3, Vertex);
		OutTetrahedrons.Add(NewTetra);
	}
}

void ADungeonMapper::GenerateConnectionFromTetras(const TArray<FTetrahedron>& Tetrahedrons)
{
	SCOPE_SECONDS_ACCUMULATOR(STAT_ConnectRooms);

	DungeonConnections.Empty();
	DungeonHallwaysData.Empty();
	for (UDungeonRoomData* DungeonsRoom : DungeonNodes)
	{
		DungeonsRoom->Connections.Empty();
	}

	//we try to create a hallway from each tetrahedron edge
	for (const FTetrahedron& Tetra : Tetrahedrons)
	{
		TryCreateConnection(Tetra.Vert1, Tetra.Vert2);
		TryCreateConnection(Tetra.Vert2, Tetra.Vert3);
		TryCreateConnection(Tetra.Vert3, Tetra.Vert1);
		TryCreateConnection(Tetra.Vert4, Tetra.Vert1);
		TryCreateConnection(Tetra.Vert4, Tetra.Vert2);
		TryCreateConnection(Tetra.Vert4, Tetra.Vert3);
	}

	for (const FDungeonConnection& HallWay : DungeonConnections)
	{
		HallWay.StartRoom->Connections.Add(&HallWay);
		HallWay.EndRoom->Connections.Add(&HallWay);
	}
}

void ADungeonMapper::TryCreateConnection(UDungeonRoomData* StartRoom, UDungeonRoomData* EndRoom)
{
	FVector StartPoint = StartRoom->Location;
	FVector EndPoint = EndRoom->Location;

	//invalid if edge is connected to any of the dummy vertex created for the super tetrahedron
	if(StartRoom->RoomType == ERoomType::Dummy || EndRoom->RoomType == ERoomType::Dummy)
	{
		return;
	}
	
	for (const UDungeonRoomData* DungeonRoom : DungeonNodes)
	{
		if(DungeonRoom == StartRoom || DungeonRoom == EndRoom)
		{
			continue;
		}
		
		FBox RoomBounds(DungeonRoom->Location - DungeonRoom->Extent, DungeonRoom->Location + DungeonRoom->Extent);
		FVector Direction = EndPoint - StartPoint;

		//invalid if edge would cross another room
		const bool Intersect = FMath::LineBoxIntersection(RoomBounds, StartPoint, EndPoint, Direction,Direction.Reciprocal());
		if(Intersect)
		{
			return;
		}
	}
	const FDungeonConnection HallWay(StartRoom->Location,StartRoom, EndRoom->Location, EndRoom);
	DungeonConnections.AddUnique(HallWay);
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
					Start += HallWaySegment.GetSafeNormal()*DungeonHallway->HallWaySectionDimensions.X;
					End -= HallWaySegment.GetSafeNormal()*DungeonHallway->HallWaySectionDimensions.X;
					HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallway
						(
							HallwayMesh
							, PrimitiveOptions
							, Start
							, End
							, DungeonHallway->HallWaySectionDimensions.X
							,  DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true,3,0,0
						);

					 // HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallowedBox
					 // 					(
					 // 							HallwayMesh
					 // 							, PrimitiveOptions
					 // 							, FTransform(HallWaySegment.Rotation(), Start + HallWaySegment.GetSafeNormal() * HallWaySegment.Length() * 0.5)
					 // 							,  HallWaySegment.Length() - (DungeonHallway->HallWaySectionDimensions.X *0.5f)
					 // 							, DungeonHallway->HallWaySectionDimensions.X
					 // 							,  DungeonHallway->HallWaySectionDimensions.Y
					 // 							, DungeonHallway->WallThickness
					 // 							, true, 3, 0
						// 						, 0,EGeometryScriptPrimitiveOriginMode::Center
					 // 						);
					
					const FTransform FloorLocation(HallWaySegment.Rotation(), Start + (HallWaySegment * 0.5f) - FVector(0,0,DungeonHallway->HallWaySectionDimensions.Y * 0.5f - DungeonHallway->WallThickness * 0.5f));
					FKBoxElem FloorShape(HallWaySegment.Length(), DungeonHallway->HallWaySectionDimensions.X, DungeonHallway->WallThickness);
					FloorShape.SetTransform(FloorLocation);
					DungeonCollision.AggGeom.BoxElems.Add(FloorShape);
				}
				break;
			case ECorridorType::HCorner:
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
						);
					// const FTransform FloorLocation(HallWaySegment.Rotation(), CornerPoint - FVector(0,0,DungeonHallway->HallWaySectionDimensions.Y * 0.5f - DungeonHallway->WallThickness * 0.5f));
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
							, true, HallWaySegment.Length()/125 + 1, DungeonHallway->HallWaySectionDimensions.X/125 + 1
							, DungeonHallway->HallWaySectionDimensions.Y/125 + 1
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
					// 	, FTransform(HallWaySegment.GetSafeNormal2D().Rotation(), Start + FVector::DownVector*DungeonHallway->HallWaySectionDimensions.Y * 0.5f)
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
					HallwayMesh = UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallowedBox
						(
							HallwayMesh
							, PrimitiveOptions
							, FTransform(DungeonHallway->Direction.Rotation(), DungeonHallway->Start + (HallWaySegment))
							, DungeonHallway->HallWaySectionDimensions.X
							, DungeonHallway->HallWaySectionDimensions.X
							,  DungeonHallway->HallWaySectionDimensions.Y
							, DungeonHallway->WallThickness
							, true, HallWaySegment.Length()/125 + 1, DungeonHallway->HallWaySectionDimensions.X/125 + 1
							, DungeonHallway->HallWaySectionDimensions.Y/125 + 1, EGeometryScriptPrimitiveOriginMode::Center
						);
				}
				break;
			case ECorridorType::VRoomConnection:
				HallwayMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendCylinder
						(
							HallwayMesh
							, PrimitiveOptions
							,FTransform(DungeonHallway->Direction.Rotation() + FRotator(-90, 0,0), DungeonHallway->Start + DungeonHallway->Direction * (DungeonHallway->HallWaySectionDimensions.Y * 0.5f))
							, DungeonHallway->HallWaySectionDimensions.X * 0.5f
							, DungeonHallway->HallWaySectionDimensions.Y * 0.5f
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
					, FTransform(HallWaySegment.Rotation(), Start + (HallWaySegment * 0.5f))
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
						, FTransform(HallWaySegment.GetSafeNormal2D().Rotation(), Start + (HallWaySegment * 0.5f))
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

FVector FHallWayPathFinder::FromWorldToGridCoord(const FVector& WorldCoord) const
{
	const int32 GridX = FMath::RoundToInt32((WorldCoord.X - GridStartingLocation.X)/CellExtend.X);
	const int32 GridY = FMath::RoundToInt32((WorldCoord.Y - GridStartingLocation.Y)/CellExtend.Y);
	const int32 GridZ = FMath::RoundToInt32((WorldCoord.Z - GridStartingLocation.Z)/CellExtend.Z);
	return FVector(GridX, GridY, GridZ);
}

FVector FHallWayPathFinder::FromGridToWorldCoord(const FVector GridCoord) const
{
	const float XPosition = GridCoord.X*CellExtend.X+GridStartingLocation.X;
	const float YPosition = GridCoord.Y*CellExtend.Y+GridStartingLocation.Y;
	const float ZPosition = GridCoord.Z*CellExtend.Z+GridStartingLocation.Z;
	return FVector(XPosition, YPosition, ZPosition);
}

void FHallWayPathFinder::FillConnectedCells(const FHallWayPathNode& CurrentCell, const FVector& CurrentCellGridCoord, TArray<FHallWayPathNode>& OutConnectedRooms) const
{
	const FVector Front(CurrentCellGridCoord.X + 1, CurrentCellGridCoord.Y, CurrentCellGridCoord.Z);
	const FVector Back(CurrentCellGridCoord.X - 1, CurrentCellGridCoord.Y, CurrentCellGridCoord.Z);
	const FVector Right(CurrentCellGridCoord.X, CurrentCellGridCoord.Y + 1, CurrentCellGridCoord.Z);
	const FVector Left(CurrentCellGridCoord.X, CurrentCellGridCoord.Y - 1, CurrentCellGridCoord.Z);
	const FVector Up(CurrentCellGridCoord.X, CurrentCellGridCoord.Y, CurrentCellGridCoord.Z + 1);
	const FVector Down(CurrentCellGridCoord.X, CurrentCellGridCoord.Y, CurrentCellGridCoord.Z- 1);

	if(Front.X < MaxCells.X)
	{
		FVector WorldCoord = FromGridToWorldCoord(Front);
		OutConnectedRooms.Add(FHallWayPathNode(CurrentCell, WorldCoord));
	}
	if(Back.X > 0)
	{
		FVector WorldCoord = FromGridToWorldCoord(Back);
		OutConnectedRooms.Add(FHallWayPathNode(CurrentCell, WorldCoord));
	}
	if(Right.Y < MaxCells.Y)
	{
		FVector WorldCoord = FromGridToWorldCoord(Right);
		OutConnectedRooms.Add(FHallWayPathNode(CurrentCell, WorldCoord));
	}
	if(Left.Y > 0)
	{
		FVector WorldCoord = FromGridToWorldCoord(Left);
		OutConnectedRooms.Add(FHallWayPathNode(CurrentCell, WorldCoord));
	}
	if(Up.Z < MaxCells.Z)
	{
		FVector WorldCoord = FromGridToWorldCoord(Up);
		OutConnectedRooms.Add(FHallWayPathNode(CurrentCell, WorldCoord));
	}
	if(Down.Z > 0)
	{
		FVector WorldCoord = FromGridToWorldCoord(Down);
		OutConnectedRooms.Add(FHallWayPathNode(CurrentCell, WorldCoord));
	}
}

void FHallWayPathFinder::CalculateHallwayPath(const FVector& StartingPoint, const FVector& EndPoint)
{
	CurrentPath = FHallWayPath();
	CurrentPath.Start = StartingPoint;
	CurrentPath.End = EndPoint;

	const FVector StartingCell = FromWorldToGridCoord(CurrentPath.Start);
	const FVector StartingCellCenter = FromGridToWorldCoord(StartingCell);
	//Start Pathing
	OpenRooms.Empty();
	ClosedRooms.Empty();
	OpenRooms.Add(StartingCellCenter);
	while (!OpenRooms.IsEmpty())
	{
		if(EvaluateNextNode())
		{
			break;
		}
	}
}

bool FHallWayPathFinder::EvaluateNextNode()
{
	FVector EndGridCoord = FromWorldToGridCoord(CurrentPath.End);
	FHallWayPathNode CurrentCell = OpenRooms.Pop();
	FVector GridCoord = FromWorldToGridCoord(CurrentCell.NodeLocation);
		
	ClosedRooms.Add(CurrentCell.NodeLocation);

	if(GridCoord.Equals(EndGridCoord))
	{
		CurrentPath.AssignPath(CurrentCell.Path);
		Paths.Add(CurrentPath);
		return true;
	}

	TArray<FHallWayPathNode> ConnectedRooms;
	FillConnectedCells(CurrentCell, GridCoord, ConnectedRooms);

	for (FHallWayPathNode& Cell : ConnectedRooms)
	{
		if(ClosedRooms.Contains(Cell))
		{
			continue;
		}

		Cell.G = CurrentCell.G + FVector::DistSquared(CurrentCell.NodeLocation, Cell.NodeLocation);
		Cell.H = FVector::DistSquared(CurrentPath.End, Cell.NodeLocation);
		Cell.F = Cell.G + Cell.H;

		if(OpenRooms.Contains(Cell))
		{
			continue;
		}
		OpenRooms.Push(Cell);
	}
	OpenRooms.Sort();
	return false;
}

void FHallWayPathFinder::DebugRender(UWorld* World)
{
	for (const FHallWayPathNode& Room : OpenRooms)
	{
		DrawDebugBox(World, Room.NodeLocation, CellExtend*0.5f, FColor::Green, true, -1,0, 2.0f);
		for (int i = 0; i < Room.Path.Num() - 1; ++i)
		{
			const FVector Start = Room.Path[i];
			const FVector End = Room.Path[i + 1];
			DrawDebugLine(World,Start, End, FColor::Orange, true, -1,0,10 );
		}
	}
	
	const FHallWayPathNode& NextRoom = OpenRooms.Last();
	for (int i = 0; i < NextRoom.Path.Num() - 1; ++i)
	{
		const FVector Start = NextRoom.Path[i];
		const FVector End = NextRoom.Path[i + 1];
		DrawDebugLine(World,Start, End, FColor::Yellow, true, -1,0,10 );
	}
	
	for (const FHallWayPathNode& Room : ClosedRooms)
	{
		DrawDebugBox(World, Room.NodeLocation, CellExtend*0.5f, FColor::Red, true, -1,0, 2.0f);
	}
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