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
	: Super(), MaxSlopeAngle(0), HallWaySegmentLength(0)
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
	PathResult.Empty();
	PathStartLocation = StartPoint;
	PathEndLocation = EndLocation;
	
	OpenNodes.Add(FDungeonPathNode(StartPoint));
	FillMetrics(OpenNodes.Last(), FDungeonPathNode());
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
	const FVector HallwayExtent = (Segment.GetAbs() + NegatedDirection * HallWaySegmentLength) * DIVIDE_BY_2;
	const FBox HalwaySegmentBox(MidPoint - (HallwayExtent + 1.0f), MidPoint + (HallwayExtent + 1.0f));
	DrawDebugBox(GetWorld(), HalwaySegmentBox.GetCenter(), HalwaySegmentBox.GetExtent(), FColor::Emerald, false, 1.0f);
	if(HalwaySegmentBox.IsInsideOrOn(PathEndLocation))
	{
		const FVector SegmentStartToPathEnd = PathEndLocation - SegmentStart;
		const float Dot = FVector::DotProduct(SegmentStartToPathEnd, Segment.GetSafeNormal());
		Out_EndLocation = SegmentStart + SegmentNormal * Dot;
		return true;
	}
	
	return false;

}

void UDungeonHallwayPathFinder::BuildPath()
{
	Super::BuildPath();
	PathResult.Add(PathEndLocation);
}

void UDungeonHallwayPathFinder::GetConnectedNodes(TArray<FDungeonPathNode>& Connections) const
{
	Connections.Empty();
	FVector CurrentLocation = CurrentNode.NodeLocation;
	FVector PrevLocation = CurrentNode.Path.Num() > 1 ? CurrentNode.Path.Last(1) : CurrentNode.NodeLocation;
	
	const bool CurrentlyInsideObstacle = Obstacles.ContainsByPredicate([&CurrentLocation](const FPathObstacle& Obstacle)
		{
			FBox ObstacleBox(Obstacle.Location - (Obstacle.Extent - 1.0f), Obstacle.Location + (Obstacle.Extent - 1.0f));
			return ObstacleBox.IsInside(CurrentLocation);
		});
	if(CurrentlyInsideObstacle)
	{
		return;
	}

	Algo::Sort(Obstacles, [&CurrentLocation, &PrevLocation ](const FPathObstacle& A, const FPathObstacle& B)
	{
		bool DontSwap = false;
		const FBox ObstacleA(A.Location - A.Extent, A.Location + A.Extent);
		const FBox ObstacleB(B.Location - B.Extent, B.Location + B.Extent);
		const FVector ClosestPointToA = ObstacleA.GetClosestPointTo(CurrentLocation);
		const FVector ClosestPointToB = ObstacleB.GetClosestPointTo(CurrentLocation);
		const FVector LastSegment = (CurrentLocation - PrevLocation).GetSafeNormal();
		const FVector CurrentToClosestA = ClosestPointToA - CurrentLocation;
		const FVector CurrentToClosestB = ClosestPointToB - CurrentLocation;
		const float DistA = CurrentToClosestA.SizeSquared();
		const float DistB = CurrentToClosestB.SizeSquared();

		if( FMath::IsNearlyEqual(DistA,DistB, 0.001f))
		{
			DontSwap = CurrentToClosestA.GetSafeNormal().Equals(LastSegment) || !CurrentToClosestB.GetSafeNormal().Equals(LastSegment);
		}
		else
		{
			DontSwap = DistA < DistB;
		}
		
		return  DontSwap;
	});

	const FPathObstacle& ClosestObstacle = Obstacles[0];
	const FBox ObstacleBox(ClosestObstacle.Location - ClosestObstacle.Extent, ClosestObstacle.Location + ClosestObstacle.Extent);
	if(ObstacleBox.IsInsideOrOn(CurrentLocation))
	{
		for (int i = 0; i < CoreValidConnectionDirection.Num(); ++i)
		{
			const FVector ValidNextPoint = CurrentLocation + CoreValidConnectionDirection[i] * (HallWaySegmentLength * DIVIDE_BY_2);
			if(ObstacleBox.IsInsideOrOn(ValidNextPoint))
			{
				continue;
			}
			Connections.Add(FDungeonPathNode(CurrentNode, ValidNextPoint));
		}
	}
	else
	{
		FVector CurrentToEnd = PathEndLocation - CurrentLocation;
		FVector LastPathDirection = (CurrentLocation - PrevLocation).GetSafeNormal();
		LastPathDirection = UDungeonGenerationHelperFunctions::RoundToUnitVector(LastPathDirection);
		FVector LastPathRightVector = FVector::CrossProduct(LastPathDirection.GetSafeNormal(), FVector::UpVector);
		FVector NegatedLastPathRightVector = UDungeonGenerationHelperFunctions::NegateUnitVector(LastPathRightVector);
		float Dot = FVector::DotProduct(CurrentToEnd.GetSafeNormal(), LastPathRightVector.GetSafeNormal());

		FVector PathDirection = LastPathRightVector;
		for (int i = 0; i < 2; ++i)
		{
			FVector CornerPoint =  PathDirection * ClosestObstacle.Extent;
			CornerPoint = CurrentLocation * NegatedLastPathRightVector + CornerPoint + (ClosestObstacle.Location * PathDirection);

			float DoorToCornerDistance = FVector::Dist(CornerPoint, CurrentLocation) + HallWaySegmentLength * DIVIDE_BY_2;
			CornerPoint = CurrentLocation + PathDirection * DoorToCornerDistance;

			const bool CrossesObstacle = Obstacles.ContainsByPredicate([&](const FPathObstacle& Obstacle)
					{
						FBox ObstacleBox(Obstacle.Location - Obstacle.Extent, Obstacle.Location + Obstacle.Extent);

						const FVector SegmentStart = CornerPoint;
						const FVector SegmentEnd = CurrentLocation;
						const FVector Segment = SegmentEnd - SegmentStart;
						const FVector NegatedDirection(!Segment.GetSafeNormal().X, !Segment.GetSafeNormal().Y, !Segment.GetSafeNormal().Z);
						const FVector MidPoint = (Segment) * DIVIDE_BY_2 + SegmentStart;
						const FVector HallwayExtent = (Segment.GetAbs() + NegatedDirection * HallWaySegmentLength) * DIVIDE_BY_2;
						const FBox HallwaySegmentBox(MidPoint - (HallwayExtent - 1.0f), MidPoint + (HallwayExtent - 1.0f));

						FBox OverlappingBox = ObstacleBox.Overlap(HallwaySegmentBox);
						
						if(!OverlappingBox.IsValid)
						{
							return false;
						}
						OverlappingBox = OverlappingBox.ExpandBy(HallWaySegmentLength * DIVIDE_BY_2);
						FVector HitNormal = FVector::ZeroVector;
						float HitTime = 0.0f;
						const bool Hit = FMath::LineExtentBoxIntersection(OverlappingBox, CurrentLocation, CornerPoint, FVector::ZeroVector, CornerPoint, HitNormal, HitTime);
						return Hit && HitTime > 0.0f;
					});

			Connections.Add(FDungeonPathNode(CurrentNode,CornerPoint));
			PathDirection *=-1;
		}
	}
	
}

void UDungeonHallwayPathFinder::FillMetrics(FDungeonPathNode& ForNode, const FDungeonPathNode& PreviousNode)
{
	Super::FillMetrics(ForNode, PreviousNode);
}
