// Fill out your copyright notice in the Description page of Project Settings.


#include "TriangulatorData.h"

#include "DungeonMapperData.h"

bool FTetrahedron::ContainsVert(const UDungeonRoomData* Vert) const
{
	return Vert->Location.Equals(Vert1->Location) || Vert->Location.Equals(Vert2->Location) || Vert->Location.Equals(Vert3->Location) || Vert->Location.Equals(Vert4->Location);
}

bool FTetrahedron::ContainsVert(const FVector& Vert) const
{
	return Vert.Equals(Vert1->Location) || Vert.Equals(Vert2->Location) || Vert.Equals(Vert3->Location) || Vert.Equals(Vert4->Location);
}

bool FTetrahedron::CircumSphereContains(const UDungeonRoomData* Vert) const
{
	const float Dist = FVector::DistSquared(Vert->Location, CircumCenter);
	return Dist <= CircumCenterSqrt;
}

bool FTetrahedron::CircumSphereContains(const FVector& Vert) const
{
	const float Dist = FVector::DistSquared(Vert, CircumCenter);
	return Dist <= CircumCenterSqrt;
}

void FTetrahedron::CalculateCircumSphere()
{
	const FVector Location1 = Vert1->Location;
	const FVector Location2 = Vert2->Location;
	const FVector Location3 = Vert3->Location;
	const FVector Location4 = Vert4->Location;

	const float A = UE::Math::TMatrix<double>
	(
		UE::Math::TPlane(FVector4(Location1.X, Location2.X, Location3.X, Location4.X)),
		UE::Math::TPlane(FVector4(Location1.Y,Location2.Y, Location3.Y, Location4.Y)),
		UE::Math::TPlane(FVector4(Location1.Z, Location2.Z, Location3.Z, Location4.Z)),
		UE::Math::TPlane(FVector4(1, 1, 1, 1))
		).Determinant();

	const float V1SizeSqr = Location1.SizeSquared();
	const float V2SizeSqr = Location2.SizeSquared();
	const float V3SizeSqr = Location3.SizeSquared();
	const float V4SizeSqr = Location4.SizeSquared();

	const float DX = UE::Math::TMatrix<double>
	(
		UE::Math::TPlane(FVector4(V1SizeSqr, V2SizeSqr, V3SizeSqr, V4SizeSqr)),
		UE::Math::TPlane(FVector4(Location1.Y, Location2.Y, Location3.Y, Location4.Y)),
		UE::Math::TPlane(FVector4(Location1.Z, Location2.Z, Location3.Z, Location4.Z)),
		UE::Math::TPlane(FVector4(1, 1, 1, 1))
		).Determinant();

	const float DY = -(UE::Math::TMatrix<double>
	(
		UE::Math::TPlane(FVector4(V1SizeSqr, V2SizeSqr, V3SizeSqr, V4SizeSqr)),
		UE::Math::TPlane(FVector4(Location1.X, Location2.X, Location3.X, Location4.X)),
		UE::Math::TPlane(FVector4(Location1.Z, Location2.Z, Location3.Z, Location4.Z)),
		UE::Math::TPlane(FVector4(1, 1, 1, 1))
		).Determinant());

	const float DZ = UE::Math::TMatrix<double>
	(
		UE::Math::TPlane(FVector4(V1SizeSqr, V2SizeSqr, V3SizeSqr, V4SizeSqr)),
		UE::Math::TPlane(FVector4(Location1.X, Location2.X, Location3.X, Location4.X)),
		UE::Math::TPlane(FVector4(Location1.Y, Location2.Y, Location3.Y, Location4.Y)),
		UE::Math::TPlane(FVector4(1, 1, 1, 1))
		).Determinant();

	const float C = UE::Math::TMatrix<double>
	(
		UE::Math::TPlane(FVector4(V1SizeSqr, V2SizeSqr, V3SizeSqr, V4SizeSqr)),
		UE::Math::TPlane(FVector4(Location1.X, Location2.X, Location3.X, Location4.X)),
		UE::Math::TPlane(FVector4(Location1.Y, Location2.Y, Location3.Y, Location4.Y)),
		UE::Math::TPlane(FVector4(Location1.Z, Location2.Z, Location3.Z, Location4.Z))
		).Determinant();

	CircumCenter = FVector(
		DX/(2*A),
		DY/(2*A),
		DZ/(2*A)
		);
	
	CircumCenterSqrt =  ((DX * DX) + (DY * DY) + (DZ * DZ) - (4 * A * C)) / (4 * A * A);
}

bool FTriangle::AlmostEqual(const FTriangle& T1, const FTriangle& T2)
{
	const bool DupV1 = T1.Vert1->Location.Equals(T2.Vert1->Location) ||  T1.Vert1->Location.Equals(T2.Vert2->Location) ||  T1.Vert1->Location.Equals(T2.Vert3->Location);
	const bool DupV2 = T1.Vert2->Location.Equals(T2.Vert1->Location) ||  T1.Vert2->Location.Equals(T2.Vert2->Location) ||  T1.Vert2->Location.Equals(T2.Vert3->Location);
	const bool DupV3 = T1.Vert3->Location.Equals(T2.Vert1->Location) ||  T1.Vert3->Location.Equals(T2.Vert2->Location) ||  T1.Vert3->Location.Equals(T2.Vert3->Location);
	return DupV1 && DupV2 && DupV3;
}