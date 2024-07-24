// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "DungeonMapperData.h"

struct FDungeonNode;

struct FTetrahedron
{
	FTetrahedron()
	: Vert1(nullptr)
	, Vert2(nullptr)
	, Vert3(nullptr)
	, Vert4(nullptr)
	, bIsBad(false)
	, CircumCenterSqrt(0)
	{
	}

	FTetrahedron(UDungeonRoomData* Vert1, UDungeonRoomData* Vert2, UDungeonRoomData* Vert3, UDungeonRoomData* Vert4)
		: Vert1(Vert1),
		  Vert2(Vert2),
		  Vert3(Vert3),
		  Vert4(Vert4)
	{
		CalculateCircumSphere();
	}

	FTetrahedron(const FVector& Vert1, const FVector& Vert2, const FVector& Vert3, const FVector& Vert4)
	: Vert1(nullptr)
	, Vert2(nullptr)
	, Vert3(nullptr)
	, Vert4(nullptr)
	{
		CalculateCircumSphere();
	}
	
	bool ContainsVert(const UDungeonRoomData* Vert) const;
	bool ContainsVert(const FVector& Vert) const;
	bool CircumSphereContains(const UDungeonRoomData* Vert) const;
	bool CircumSphereContains(const FVector& Vert) const;
	
	UDungeonRoomData* Vert1;
	UDungeonRoomData* Vert2;
	UDungeonRoomData* Vert3;
	UDungeonRoomData* Vert4;

	
	bool bIsBad;

	FVector CircumCenter;
	float CircumCenterSqrt;

private:
	void CalculateCircumSphere();
};


struct FTriangle
{
	FTriangle()
	: Vert1(nullptr)
	, Vert2(nullptr)
	, Vert3(nullptr)
	, bIsBad(false)
	{
	}

	FTriangle(UDungeonRoomData* Vert1, UDungeonRoomData* Vert2, UDungeonRoomData* Vert3)
	: Vert1(Vert1)
	, Vert2(Vert2)
	, Vert3(Vert3)
	, bIsBad(false)
	{
	}
	
	static bool AlmostEqual(const FTriangle& T1, const FTriangle& T2);
	
	UDungeonRoomData* Vert1;
	UDungeonRoomData* Vert2;
	UDungeonRoomData* Vert3;
	
	bool bIsBad;
};