// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonPathFinder.h"

#include "DungeonGenerationHelperFunctions.h"
#include "DungeonGenerator.h"

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
}
void UDungeonHallwayPathFinder::Initialize(FVector StartPoint, FVector EndLocation)
{
	OpenNodes.Empty();
	ClosedNodes.Empty();
	PathStartLocation = StartPoint;
	PathEndLocation = EndLocation;
		
	OpenNodes.Add(FDungeonPathNode(StartPoint));
	OpenNodes.Last().G = 0;
	OpenNodes.Last().H = FVector::DistSquared(PathEndLocation, OpenNodes.Last().NodeLocation);
	OpenNodes.Last().F = OpenNodes.Last().G + OpenNodes.Last().H;
}

void UDungeonHallwayPathFinder::Debug(float LifeTime)
{
	Super::Debug(LifeTime);
	// for (int i = 0; i < CoreValidConnectionDirection.Num(); ++i)
	// {
	// 	DrawDebugDirectionalArrow(GetWorld(), CurrentNode.NodeLocation, CurrentNode.NodeLocation + CoreValidConnectionDirection[i] * HallWaySegmentLength, 10, FColor::Magenta, false, LifeTime * 2.0f);
	// }
}

void UDungeonHallwayPathFinder::FillAdditionalValidConnectionDirections()
{
	ValidConnectionDirection.Empty(ValidConnectionDirection.Num());

	FVector ExpectedDirection = (PathEndLocation - PathStartLocation).GetSafeNormal();
	FVector ProjectedDirection = ExpectedDirection.GetSafeNormal2D();

	float Angle = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(ExpectedDirection, ProjectedDirection)));
	Angle = FMath::Min(MaxSlopeAngle, Angle);
		
	// vertical Diagonals
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[0].RotateAngleAxis(Angle,  CoreValidConnectionDirection[2]));// FVector::ForwardVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[1].RotateAngleAxis(Angle,  CoreValidConnectionDirection[2])); //(FVector::BackwardVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[0].RotateAngleAxis(Angle,  CoreValidConnectionDirection[3])); //(FVector::ForwardVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[1].RotateAngleAxis(Angle,  CoreValidConnectionDirection[3])); //(FVector::BackwardVector + FVector::UpVector).GetSafeNormal();
	
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[2].RotateAngleAxis(Angle, CoreValidConnectionDirection[0])); //(FVector::RightVector + FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[3].RotateAngleAxis(Angle, CoreValidConnectionDirection[0])); //(FVector::LeftVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[2].RotateAngleAxis(Angle, CoreValidConnectionDirection[1])); //(FVector::RightVector - FVector::UpVector).GetSafeNormal();
	ValidConnectionDirection.AddUnique(CoreValidConnectionDirection[3].RotateAngleAxis(Angle, CoreValidConnectionDirection[1])); //(FVector::LeftVector + FVector::UpVector).GetSafeNormal();
	
}

bool UDungeonHallwayPathFinder::HasReachedDestiny(FVector& Out_EndLocation) const
{
	//return Super::HasReachedDestiny(Out_EndLocation);
	if(CurrentNode.Path.Num() < 2)
	{
		return false;
	}
	const FVector SegmentStart = CurrentNode.Path.Last(1);
	const FVector SegmentEnd = CurrentNode.NodeLocation;
	const FVector Segment = SegmentEnd - SegmentStart;
	const FVector SegmentNormal = Segment.GetSafeNormal();
	const FVector NegatedDirection(!Segment.GetSafeNormal().X, !Segment.GetSafeNormal().Y, !Segment.GetSafeNormal().Z);
	const FVector MidPoint = (Segment) * DIVIDE_BY_2 + SegmentStart;
	const FVector HallwayExtent = (Segment + NegatedDirection * HallWaySegmentLength) * DIVIDE_BY_2;
	const FBox HalwaySegmentBox(MidPoint - HallwayExtent, MidPoint + HallwayExtent);
	DrawDebugBox(GetWorld(), HalwaySegmentBox.GetCenter(), HalwaySegmentBox.GetExtent(), FColor::Emerald, false, 1.0f);
	if(HalwaySegmentBox.IsInsideOrOn(PathEndLocation))
	{
		const FVector SegmentStartToPathEnd = PathEndLocation - SegmentStart;
		const float Dot = FVector::DotProduct(Segment, SegmentStartToPathEnd);
		Out_EndLocation = SegmentStart + SegmentNormal * Dot;
		return true;
	}
	
	return false;
	// Out_EndLocation = FVector::ZeroVector;
	// if(CurrentNode.Path.Num() < 2)
	// {
	// 	return false;
	// }
	//
	// FBox Room = FBox(PathEndLocation - (EndRoomExtent+FVector(HallWaySegmentLength, HallWaySegmentLength, 0)), PathEndLocation + (EndRoomExtent+FVector(HallWaySegmentLength, HallWaySegmentLength, 0)));
	// if(Room.IsInside(CurrentNode.Path.Last(1)))
	// {
	// 	return false;
	// }
	//
	// for (int i = 0; i < 4; ++i)
	// {
	// 	FPlane PlaneToCheck(PathEndLocation + CoreValidConnectionDirection[i]*(EndRoomExtent+HallWaySegmentLength), CoreValidConnectionDirection[i]);
	//
	// 	const bool PlaneIntersecting = FMath::SegmentPlaneIntersection(CurrentNode.Path.Last(1), CurrentNode.NodeLocation, PlaneToCheck, Out_EndLocation);
	//
	// 	FVector NegatedDirection(!CoreValidConnectionDirection[i].X, !CoreValidConnectionDirection[i].Y, 0);
	// 	FVector2D PlaneSize(NegatedDirection.GetAbs()*EndRoomExtent);
	// 	PlaneSize.X = (PlaneSize.X != 0 ? PlaneSize.X : PlaneSize.Y) - HallWaySegmentLength;
	// 	PlaneSize.Y = EndRoomExtent.Z;
	// 	DrawDebugSolidPlane(GetWorld(),PlaneToCheck,  PathEndLocation + CoreValidConnectionDirection[i]*EndRoomExtent, PlaneSize, FColor::Red, false);
	//
	// 	if(PlaneIntersecting)
	// 	{
	// 		const FVector RoomExpansion = EndRoomExtent + CoreValidConnectionDirection[i].GetAbs() * HallWaySegmentLength - NegatedDirection * HallWaySegmentLength;
	// 		Room = FBox(PathEndLocation - RoomExpansion, PathEndLocation + RoomExpansion);
	// 		if(Room.IsInsideOrOn(Out_EndLocation))
	// 		{
	// 			DrawDebugSolidPlane(GetWorld(),PlaneToCheck,  PathEndLocation + CoreValidConnectionDirection[i]*EndRoomExtent, PlaneSize, FColor::Red, true);
	// 			DrawDebugPoint(GetWorld(), Out_EndLocation, 10.0f, FColor::Green, true);
	// 			//Out_EndLocation += (-CoreValidConnectionDirection[i])*HallWaySegmentLength;
	// 			return true;
	// 		}
	// 	}
	// }
	//
	// return false;
}

void UDungeonHallwayPathFinder::BuildPath()
{
	Super::BuildPath();
	return;
	// TArray<FVector> FinalPath;
	// const FBox StartRoom = FBox(PathStartLocation - StartRoomExtent, PathStartLocation + StartRoomExtent);
	// FinalPath.Add(StartRoom.GetClosestPointTo(CurrentNode.Path[0]));
	// for (int i = 0; i < CurrentNode.Path.Num() - 1; i = i + 2)
	// {
	// 	FinalPath.Add(CurrentNode.Path[i]);
	// }
	// FinalPath.Add(CurrentNode.Path.Last());
	//
	// FBox EndRoom = FBox(PathEndLocation - (EndRoomExtent), PathEndLocation + EndRoomExtent);
	// FinalPath.Add(EndRoom.GetClosestPointTo(CurrentNode.Path.Last()));
	// PathResult = FinalPath;
}

void UDungeonHallwayPathFinder::GetConnectedNodes(TArray<FDungeonPathNode>& Connections) const
{
	Connections.Empty();
	FVector CurrentLocation = CurrentNode.NodeLocation;
	const bool CurrentlyInsideObstacle = Obstacles.ContainsByPredicate([&CurrentLocation](const FPathObstacle& Obstacle)
		{
			FBox ObstacleBox(Obstacle.Location - Obstacle.Extent, Obstacle.Location + Obstacle.Extent);
			return ObstacleBox.IsInside(CurrentLocation);
		});
	if(CurrentlyInsideObstacle)
	{
		return;
	}

	FVector AproachDirection =  CurrentNode.Path.Num() > 1 ? (CurrentNode.NodeLocation - CurrentNode.Path.Last(1)).GetSafeNormal() : FVector::ZeroVector;
	for (int i = 0; i < CoreValidConnectionDirection.Num(); ++i)
	{
		if(CoreValidConnectionDirection[i].Equals(-AproachDirection))
		{
			continue;
		}
		const FVector StartPoint = CurrentNode.NodeLocation;
		const FVector ValidDirection = StartPoint + CoreValidConnectionDirection[i] * (HallWaySegmentLength * DIVIDE_BY_2);
		const bool DirectionEntersObstacle = Obstacles.ContainsByPredicate([&](const FPathObstacle& Obstacle)
		{
			FBox ObstacleBox(Obstacle.Location - Obstacle.Extent, Obstacle.Location + Obstacle.Extent);

			const FVector SegmentNormal = CoreValidConnectionDirection[i];
			const FVector Segment = SegmentNormal * (HallWaySegmentLength * DIVIDE_BY_2);
			const FVector NegatedDirection = UDungeonGenerationHelperFunctions::NegateUnitVector(SegmentNormal);
			const FVector MidPoint = StartPoint + Segment * DIVIDE_BY_2;
			const FVector HallwayExtent = (Segment.GetAbs() + NegatedDirection * HallWaySegmentLength) * DIVIDE_BY_2;
			const FBox HalwaySegmentBox(MidPoint - (HallwayExtent - 1.0f), MidPoint + (HallwayExtent - 1.0f));
			
			return ObstacleBox.Intersect(HalwaySegmentBox);
		});
		if(DirectionEntersObstacle)
		{
			continue;
		}
		Connections.Add(FDungeonPathNode(CurrentNode, ValidDirection));
	}
	
	for (int i = 0; i < ValidConnectionDirection.Num(); ++i)
	{
		if(ValidConnectionDirection[i].Equals(-AproachDirection))
		{
			continue;
		}
		const FVector StartPoint = CurrentNode.NodeLocation;
		const FVector ValidDirection = StartPoint + ValidConnectionDirection[i] * (HallWaySegmentLength * DIVIDE_BY_2);
		const bool DirectionEntersObstacle = Obstacles.ContainsByPredicate([&](const FPathObstacle& Obstacle)
		{
			FBox ObstacleBox(Obstacle.Location - Obstacle.Extent, Obstacle.Location + Obstacle.Extent);

			const FVector SegmentNormal = CoreValidConnectionDirection[i];
			const FVector Segment = SegmentNormal * (HallWaySegmentLength * DIVIDE_BY_2);
			const FVector NegatedDirection = UDungeonGenerationHelperFunctions::NegateUnitVector(SegmentNormal);
			const FVector MidPoint =  StartPoint + Segment * DIVIDE_BY_2;
			const FVector HallwayExtent = (Segment.GetAbs()  + NegatedDirection * HallWaySegmentLength) * DIVIDE_BY_2;
			const FBox HalwaySegmentBox(MidPoint - HallwayExtent, MidPoint + HallwayExtent);
			
			return ObstacleBox.Intersect(HalwaySegmentBox);
		});
		if(DirectionEntersObstacle)
		{
			continue;
		}
		Connections.Add(FDungeonPathNode(CurrentNode, ValidDirection));
	}
}

void UDungeonHallwayPathFinder::FillMetrics(FDungeonPathNode& ForNode, const FDungeonPathNode& PreviousNode)
{
	Super::FillMetrics(ForNode, PreviousNode);
	return;
	// FBox EndRoom = FBox(PathEndLocation - (EndRoomExtent), PathEndLocation + EndRoomExtent);
	//
	// //F,B,L,R
	// FVector ClosesPoint = FVector::ZeroVector;
	// float ClosestDistance = FLT_MAX;
	// for (int i = 0; i < 4; ++i)
	// {
	// 	FVector ExitPoint =  PathEndLocation + CoreValidConnectionDirection[i] * EndRoomExtent;
	// 	float Distance = FVector::Dist(ForNode.NodeLocation, ExitPoint);
	// 	if(ClosestDistance > Distance)
	// 	{
	// 		ClosesPoint = ExitPoint;
	// 	}
	// }
	//
	// ForNode.G = PreviousNode.G + FVector::DistSquared(PreviousNode.NodeLocation, ForNode.NodeLocation) + ForNode.Path.Num();
	// ForNode.H = FVector::DistSquared(ClosesPoint, ForNode.NodeLocation);
	// ForNode.F = ForNode.G + ForNode.H;
}
