// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonMapperData.h"

void FDungeonConnection::GetWallConnectionPoints(FVector& out_Point1, FVector& out_Point2) const
{
	FBox StartRoomBox(StartRoom->Location - StartRoom->Extent,StartRoom->Location + StartRoom->Extent);
	FBox EndRoomBox(EndRoom->Location - EndRoom->Extent,EndRoom->Location + EndRoom->Extent);
	
	FVector ImpactNormal;
	float Time;
	FMath::LineExtentBoxIntersection(EndRoomBox, StartRoom->Location, EndRoom->Location, FVector::ZeroVector,out_Point2, ImpactNormal, Time);
	FMath::LineExtentBoxIntersection(StartRoomBox, EndRoom->Location, StartRoom->Location, FVector::ZeroVector,out_Point1, ImpactNormal, Time);
}
