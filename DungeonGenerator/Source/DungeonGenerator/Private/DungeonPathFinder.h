// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DungeonPathFinder.generated.h"

struct FDungeonPathNode
{
	FVector NodeLocation = FVector::ZeroVector;
	TArray<FVector> Path;
	float G = 0;
	float F = 0;
	float H = 0;

	FDungeonPathNode(){}
	
	FDungeonPathNode(FVector InNodeLocation)
		: NodeLocation(InNodeLocation)
	{
		Path.Add(NodeLocation);
	}

	FDungeonPathNode(const FDungeonPathNode& Node, FVector InNodeLocation)
	{
		NodeLocation = InNodeLocation;
		for (const FVector& PathNode : Node.Path)
		{
			Path.Add(PathNode);
		}
		Path.Add(InNodeLocation);
	}
		
	bool operator==(const FDungeonPathNode& Other) const
	{
		const bool SameLocation = NodeLocation.Equals(Other.NodeLocation);
		
		return SameLocation;
	}

	bool operator <(const FDungeonPathNode& Other) const
	{
		return F > Other.F;
	}
	bool operator <=(const FDungeonPathNode& Other) const
	{
		return F >= Other.F;
	}
};

struct FPathObstacle
{
	FVector Location = FVector::ZeroVector;
	FVector Extent = FVector::ZeroVector;
	float AdditionalCost = FLT_MAX;
};

UCLASS(Abstract)
class UDungeonPathFinder : public UObject
{
	GENERATED_BODY()
public:
	virtual void Initialize(FVector StartPoint, FVector EndLocation);
	
	bool Evaluate();
	virtual void Debug(float LifeTime = -1.0f);
protected:
	virtual bool HasReachedDestiny(FVector& Out_EndLocation) const;
	virtual void BuildPath();
	virtual void GetConnectedNodes(TArray<FDungeonPathNode>& Connections) const {};
	virtual void FillMetrics(FDungeonPathNode& ForNode, const FDungeonPathNode& PreviousNode);
public:
	TArray<FVector> PathResult;
	TArray<FPathObstacle> Obstacles;
	
protected:
	FVector PathEndLocation;
	FVector PathStartLocation;
	TArray<FDungeonPathNode> OpenNodes;
	TArray<FDungeonPathNode> ClosedNodes;
	FDungeonPathNode CurrentNode;
};

UCLASS()
class UDungeonHallwayPathFinder : public UDungeonPathFinder
{
	GENERATED_BODY()
public:
	UDungeonHallwayPathFinder();
	virtual void Initialize(FVector StartPoint, FVector EndLocation) override;
	void FillAdditionalValidConnectionDirections();
	virtual void Debug(float LifeTime = -1.0f) override;
protected:
	virtual bool HasReachedDestiny(FVector& Out_EndLocation) const override;
	virtual void BuildPath() override;
	virtual void GetConnectedNodes(TArray<FDungeonPathNode>& Connections) const override;
	virtual void FillMetrics(FDungeonPathNode& ForNode, const FDungeonPathNode& PreviousNode) override;
public:
	TArray<FVector> CoreValidConnectionDirection;
	TArray<FVector> ValidConnectionDirection;
	FVector StartRoomExtent;
	FVector EndRoomExtent;
	float MaxSlopeAngle;
	float HallWaySegmentLength;
};