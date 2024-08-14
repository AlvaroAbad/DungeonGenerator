// Fill out your copyright notice in the Description page of Project Settings.


#include "DungeonGenerationHelperFunctions.h"

FVector UDungeonGenerationHelperFunctions::NegateUnitVector(const FVector& UnitVector)
{
	return FVector(!UnitVector.X, !UnitVector.Y, !UnitVector.Z);
}

FVector UDungeonGenerationHelperFunctions::RoundToUnitVector(const FVector& UnitVector)
{
	float X = UnitVector.X;
	float Y = UnitVector.Y;
	float Z = UnitVector.Z;

	const bool XgY = FMath::Abs(X) > FMath::Abs(Y);
	const bool XgZ = FMath::Abs(X) > FMath::Abs(Z);
	const bool YgZ = FMath::Abs(Y) > FMath::Abs(Z);

	if(XgY && XgZ)
	{
		X = FMath::RoundFromZero(X);
		Y = 0;
		Z = 0;
	}
	else if(YgZ)
	{
		X = 0;
		Y = FMath::RoundFromZero(Y);
		Z = 0;
	}
	else
	{
		X = 0;
		Y = 0;
		Z = FMath::RoundFromZero(Z);
	}
	return FVector(X, Y, Z);
}
