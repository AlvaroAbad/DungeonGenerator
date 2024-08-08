// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonPathFinder.h"

void UDungeonPathFinder::Initialize(FVector StartPoint, FVector EndLocation)
{
	OpenNodes.Empty();
	ClosedNodes.Empty();
	OpenNodes.Add(FDungeonPathNode(StartPoint));
	PathEndLocation = EndLocation;
};

bool UDungeonPathFinder::Evaluate()
{
	if(OpenNodes.IsEmpty())
	{
		return true;
	}
	
	CurrentNode = OpenNodes.Pop();
	ClosedNodes.Push(CurrentNode);
	FVector FinalEndLocation;
	if(HasReachedDestiny(FinalEndLocation))
	{
		CurrentNode.NodeLocation = FinalEndLocation;
		CurrentNode.Path[CurrentNode.Path.Num() - 1] = FinalEndLocation;
		BuildPath();
		return true;
	}

	TArray<FDungeonPathNode> ConnectedNodes;
	GetConnectedNodes(ConnectedNodes);
	for(FDungeonPathNode& ConnectedNode : ConnectedNodes)
	{
		if(ClosedNodes.Contains(ConnectedNode))
		{
			continue;
		}

		FillMetrics(ConnectedNode, CurrentNode);
		int32 NodeIndex = OpenNodes.Find(ConnectedNode);
		if(NodeIndex != INDEX_NONE && ConnectedNode <= OpenNodes[NodeIndex])
		{
			continue;
		}
		NodeIndex = ClosedNodes.Find(ConnectedNode);
		if(NodeIndex != INDEX_NONE)
		{
			continue;
		}
		
		OpenNodes.Push(ConnectedNode);
	}
	OpenNodes.Sort();
	return false;
}

void UDungeonPathFinder::Debug(float LifeTime)
{
	for (int i = 0; i < CurrentNode.Path.Num() - 1; ++i)
	{
		DrawDebugLine(GetWorld(), CurrentNode.Path[i], CurrentNode.Path[i + 1], FColor::Orange, false, LifeTime, 0, 10);
	}
}

bool UDungeonPathFinder::HasReachedDestiny(FVector& Out_EndLocation) const
{
	Out_EndLocation = CurrentNode.NodeLocation;
	return CurrentNode.NodeLocation.Equals(PathEndLocation);
}

void UDungeonPathFinder::BuildPath()
{
	PathResult = CurrentNode.Path;
}

void UDungeonPathFinder::FillMetrics(FDungeonPathNode& ForNode, const FDungeonPathNode& PreviousNode)
{
	ForNode.G = PreviousNode.G + FVector::DistSquared(PreviousNode.NodeLocation, ForNode.NodeLocation);
	ForNode.H = FVector::DistSquared(PathEndLocation, ForNode.NodeLocation);
	ForNode.F = ForNode.G + ForNode.H;
}

UDungeonHallwayPathFinder::UDungeonHallwayPathFinder()
	: Super()
{
	CoreValidConnectionDirection.Add(FVector::ForwardVector);
	CoreValidConnectionDirection.Add(FVector::BackwardVector);
	CoreValidConnectionDirection.Add(FVector::RightVector);
	CoreValidConnectionDirection.Add(FVector::LeftVector);
	CoreValidConnectionDirection.Add((FVector::ForwardVector + FVector::RightVector).GetSafeNormal());
	CoreValidConnectionDirection.Add((FVector::BackwardVector + FVector::LeftVector).GetSafeNormal());
	CoreValidConnectionDirection.Add((FVector::ForwardVector + FVector::LeftVector).GetSafeNormal());
	CoreValidConnectionDirection.Add((FVector::BackwardVector + FVector::RightVector).GetSafeNormal());
}
void UDungeonHallwayPathFinder::Initialize(FVector StartPoint, FVector EndLocation)
{
	OpenNodes.Empty();
	ClosedNodes.Empty();
	PathEndLocation = EndLocation;
		
	OpenNodes.Add(FDungeonPathNode(StartPoint + CoreValidConnectionDirection[0] * (StartRoomExtent*2.0f)));
	OpenNodes.Last().G = 0;
	OpenNodes.Last().H = FVector::DistSquared(PathEndLocation, OpenNodes.Last().NodeLocation);
	OpenNodes.Last().F = OpenNodes.Last().G + OpenNodes.Last().H;
					
	OpenNodes.Add(FDungeonPathNode(StartPoint + CoreValidConnectionDirection[1] * (StartRoomExtent*2.0f)));
	OpenNodes.Last().G = 0;
	OpenNodes.Last().H = FVector::DistSquared(PathEndLocation, OpenNodes.Last().NodeLocation);
	OpenNodes.Last().F = OpenNodes.Last().G + OpenNodes.Last().H;
					
	OpenNodes.Add(FDungeonPathNode(StartPoint + CoreValidConnectionDirection[2] * (StartRoomExtent*2.0f)));
	OpenNodes.Last().G = 0;
	OpenNodes.Last().H = FVector::DistSquared(PathEndLocation, OpenNodes.Last().NodeLocation);
	OpenNodes.Last().F = OpenNodes.Last().G + OpenNodes.Last().H;
					
	OpenNodes.Add(FDungeonPathNode(StartPoint + CoreValidConnectionDirection[3] * (StartRoomExtent*2.0f)));
	OpenNodes.Last().G = 0;
	OpenNodes.Last().H = FVector::DistSquared(PathEndLocation, OpenNodes.Last().NodeLocation);
	OpenNodes.Last().F = OpenNodes.Last().G + OpenNodes.Last().H;
	OpenNodes.Sort();
}

void UDungeonHallwayPathFinder::Debug(float LifeTime)
{
	Super::Debug(LifeTime);
	// for (int i = 0; i < CoreValidConnectionDirection.Num(); ++i)
	// {
	// 	DrawDebugDirectionalArrow(GetWorld(), CurrentNode.NodeLocation, CurrentNode.NodeLocation + CoreValidConnectionDirection[i] * HallWaySegmentLength, 10, FColor::Magenta, false, LifeTime * 2.0f);
	// }
}

void UDungeonHallwayPathFinder::FillAdditionalValidConnectionDirections(FVector StartPoint, FVector EndLocation)
{
	ValidConnectionDirection.Empty(ValidConnectionDirection.Num());

	FVector ExpectedDirection = (EndLocation - StartPoint).GetSafeNormal();
	FVector ProjectedDirection = ExpectedDirection.GetSafeNormal2D();

	float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ExpectedDirection, ProjectedDirection)));
	Angle = FMath::Min(MaxSlopeAngle, Angle);
		
	// vertical Diagonals
	ValidConnectionDirection.Add(CoreValidConnectionDirection[0].RotateAngleAxis(Angle,  CoreValidConnectionDirection[2]));// FVector::ForwardVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[1].RotateAngleAxis(Angle,  CoreValidConnectionDirection[2])); //(FVector::BackwardVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[0].RotateAngleAxis(Angle,  CoreValidConnectionDirection[3])); //(FVector::ForwardVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[1].RotateAngleAxis(Angle,  CoreValidConnectionDirection[3])); //(FVector::BackwardVector + FVector::UpVector).GetSafeNormal();
	
	ValidConnectionDirection.Add(CoreValidConnectionDirection[2].RotateAngleAxis(Angle, CoreValidConnectionDirection[0])); //(FVector::RightVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[3].RotateAngleAxis(Angle, CoreValidConnectionDirection[0])); //(FVector::LeftVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[2].RotateAngleAxis(Angle, CoreValidConnectionDirection[1])); //(FVector::RightVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[3].RotateAngleAxis(Angle, CoreValidConnectionDirection[1])); //(FVector::LeftVector + FVector::UpVector).GetSafeNormal();
	
	ValidConnectionDirection.Add(CoreValidConnectionDirection[4].RotateAngleAxis(Angle, CoreValidConnectionDirection[6])); //(FVector::ForwardVector + FVector::RightVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[7].RotateAngleAxis(Angle, CoreValidConnectionDirection[4])); //(FVector::BackwardVector + FVector::LeftVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[4].RotateAngleAxis(Angle, CoreValidConnectionDirection[7])); //(FVector::ForwardVector + FVector::RightVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[7].RotateAngleAxis(Angle, CoreValidConnectionDirection[5])); //(FVector::BackwardVector + FVector::LeftVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[5].RotateAngleAxis(Angle, CoreValidConnectionDirection[7])); //(FVector::ForwardVector + FVector::LeftVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[6].RotateAngleAxis(Angle, CoreValidConnectionDirection[5])); //(FVector::BackwardVector + FVector::RightVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[5].RotateAngleAxis(Angle, CoreValidConnectionDirection[6])); //(FVector::ForwardVector + FVector::LeftVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.Add(CoreValidConnectionDirection[6].RotateAngleAxis(Angle, CoreValidConnectionDirection[4])); //(FVector::BackwardVector + FVector::RightVector + FVector::UpVector).GetSafeNormal();
}

bool UDungeonHallwayPathFinder::HasReachedDestiny(FVector& Out_EndLocation) const
{
	Out_EndLocation = FVector::ZeroVector;
	if(CurrentNode.Path.Num() < 2)
	{
		return false;
	}
	
	FBox Room = FBox(PathEndLocation - (EndRoomExtent+FVector(HallWaySegmentLength, HallWaySegmentLength, 0)), PathEndLocation + (EndRoomExtent+FVector(HallWaySegmentLength, HallWaySegmentLength, 0)));
	if(Room.IsInside(CurrentNode.Path.Last(1)))
	{
		return false;
	}
	
	for (int i = 0; i < 4; ++i)
	{
		FPlane PlaneToCheck(PathEndLocation + CoreValidConnectionDirection[i]*(EndRoomExtent+HallWaySegmentLength), CoreValidConnectionDirection[i]);
		
		const bool PlaneIntersecting = FMath::SegmentPlaneIntersection(CurrentNode.Path.Last(1), CurrentNode.NodeLocation, PlaneToCheck, Out_EndLocation);

		FVector NegatedDirection(!CoreValidConnectionDirection[i].X, !CoreValidConnectionDirection[i].Y, 0);
		FVector2D PlaneSize(NegatedDirection.GetAbs()*EndRoomExtent);
		PlaneSize.X = (PlaneSize.X != 0 ? PlaneSize.X : PlaneSize.Y) - HallWaySegmentLength;
		PlaneSize.Y = EndRoomExtent.Z;
		// DrawDebugSolidPlane(GetWorld(), PlaneToCheck, PathEndLocation + CoreValidConnectionDirection[i]*(EndRoomExtent+HallWaySegmentLength), PlaneSize, FColor::White, false, 1);
		if(PlaneIntersecting)
		{
			const FVector RoomExpansion = EndRoomExtent + CoreValidConnectionDirection[i].GetAbs() * (HallWaySegmentLength + 1) - NegatedDirection * (HallWaySegmentLength + 1);// Additional unit to prevent floating point error
			Room = FBox(PathEndLocation - RoomExpansion, PathEndLocation + RoomExpansion);
			// DrawDebugBox(GetWorld(), Room.GetCenter(), Room.GetExtent(), FColor::Black, false, 1);
			// DrawDebugPoint(GetWorld(),Out_EndLocation, 10.0f, FColor::Magenta, false,  );
			if(Room.IsInsideOrOn(Out_EndLocation))
			{
				DrawDebugPoint(GetWorld(), Out_EndLocation, 10.0f, FColor::Green, true);
				//Out_EndLocation += (-CoreValidConnectionDirection[i])*HallWaySegmentLength;
				return true;
			}
		}
	}
	
	return false;
}

void UDungeonHallwayPathFinder::BuildPath()
{
	TArray<FVector> FinalPath;
	const FBox StartRoom = FBox(PathStartLocation - StartRoomExtent, PathStartLocation + StartRoomExtent);
	FinalPath.Add(StartRoom.GetClosestPointTo(CurrentNode.Path[0]));
	for (int i = 0; i < CurrentNode.Path.Num() - 1; i = i + 2)
	{
		FinalPath.Add(CurrentNode.Path[i]);
	}
	

	if(FVector::Dist(CurrentNode.Path.Last(), FinalPath.Last()) > HallWaySegmentLength)
	{
		FinalPath.Add(CurrentNode.Path.Last());
	}
	FBox EndRoom = FBox(PathEndLocation - (EndRoomExtent), PathEndLocation + EndRoomExtent);
	FVector ConectingPoint = EndRoom.GetClosestPointTo(CurrentNode.Path.Last());
	FinalPath.Last().Z = ConectingPoint.Z;
	FinalPath.Add(ConectingPoint);
	PathResult = FinalPath;
}

void UDungeonHallwayPathFinder::GetConnectedNodes(TArray<FDungeonPathNode>& Connections) const
{
	Connections.Empty();
	FBox Room = FBox(PathEndLocation - (EndRoomExtent + HallWaySegmentLength), PathEndLocation + (EndRoomExtent+FVector(HallWaySegmentLength, HallWaySegmentLength, 0)));
	Room = Room.ExpandBy(FVector(0, 0, FLT_MAX));
	if(Room.IsInsideOrOn(CurrentNode.NodeLocation))
	{
		return;
	}
	Room = Room.ExpandBy(FVector(-HallWaySegmentLength, -HallWaySegmentLength, 0));
	for (int i = 0; i < CoreValidConnectionDirection.Num(); ++i)
	{
		const FVector ValidDirection = CurrentNode.NodeLocation + CoreValidConnectionDirection[i] * HallWaySegmentLength;
		if(Room.IsInside(ValidDirection))
		{
			continue;
		}
		Connections.Add(FDungeonPathNode(CurrentNode, ValidDirection));
	}
	
	for (int i = 0; i < ValidConnectionDirection.Num(); ++i)
	{
		const FVector ValidDirection = CurrentNode.NodeLocation + ValidConnectionDirection[i] * HallWaySegmentLength;
		if(Room.IsInside(ValidDirection))
		{
			continue;
		}
		Connections.Add(FDungeonPathNode(CurrentNode, ValidDirection));
	}
}

void UDungeonHallwayPathFinder::FillMetrics(FDungeonPathNode& ForNode, const FDungeonPathNode& PreviousNode)
{
	FBox EndRoom = FBox(PathEndLocation - (EndRoomExtent), PathEndLocation + EndRoomExtent);

	//F,B,L,R
	FVector ClosesPoint = FVector::ZeroVector;
	float ClosestDistance = FLT_MAX;
	for (int i = 0; i < 4; ++i)
	{
		FVector ExitPoint =  PathEndLocation + CoreValidConnectionDirection[i] * EndRoomExtent;
		float Distance = FVector::Dist(ForNode.NodeLocation, ExitPoint);
		if(ClosestDistance > Distance)
		{
			ClosesPoint = ExitPoint;
		}
	}
	
	ForNode.G = PreviousNode.G + FVector::DistSquared(PreviousNode.NodeLocation, ForNode.NodeLocation) + ForNode.Path.Num();
	ForNode.H = FVector::DistSquared(ClosesPoint, ForNode.NodeLocation);
	ForNode.F = ForNode.G + ForNode.H;
}
