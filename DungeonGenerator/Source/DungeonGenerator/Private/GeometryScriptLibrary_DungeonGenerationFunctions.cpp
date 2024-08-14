// Fill out your copyright notice in the Description page of Project Settings.


#include "GeometryScriptLibrary_DungeonGenerationFunctions.h"

#include "DynamicMeshEditor.h"
#include "UDynamicMesh.h"
#include "DynamicMesh/MeshTransforms.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"

#define LOCTEXT_NAMESPACE "UGeometryScriptLibrary_DungeonGenerationFunctions"

using namespace UE::Geometry;

UE::Geometry::FMeshShapeGenerator& FHallWayCornerGenerator::Generate()
{
	FIndex3i N = EdgeVertices;
	N.A = FMath::Max(3, N.A);
	N.B = FMath::Max(2, N.B);
	N.C = FMath::Max(2, N.C);

	FVector3d Nscale(1.0 / (N.A-1), 1.0 / (N.B-1), 1.0 / (N.C-1));

	FIndex3i NInternal = N;
	NInternal.A -= 2; NInternal.B -= 2; NInternal.C -= 2;

	FIndex3i NTri = N;
	NTri.A--; NTri.B--; NTri.C--;
	int NumUVsAndNormals = 2 * (N.A * N.B + N.B * N.C + N.C * N.A);
	int NumVertices = 8 + (NInternal.A + NInternal.B + NInternal.C) * 4 + (NInternal.A*NInternal.B + NInternal.B*NInternal.C + NInternal.C*NInternal.A) * 2;
	int NumTriangles = 4 * (NTri.A * NTri.B + NTri.B * NTri.C + NTri.C * NTri.A);
	
	SetBufferSizes(NumVertices, NumTriangles, NumUVsAndNormals, NumUVsAndNormals);

	int FaceDimOrder[3]{ 1, 2, 0 }; // ordering from IndexUtil.cpp

	FIndex3i VerticesPerFace(N.B * N.C, N.A * N.C, N.A * N.B);
	TArray<TArray<int>> FaceVertIndices;
	FaceVertIndices.SetNum(6);
	for (int Dim = 0; Dim < 3; Dim++)
	{
		int FaceIdxForDim = FaceDimOrder[Dim] * 2;
		int VertexNum = VerticesPerFace[Dim];
		FaceVertIndices[FaceIdxForDim].Init(-1,VertexNum);
		FaceVertIndices[FaceIdxForDim+1].Init(-1,VertexNum);
	}

	const FVector StartToCorner = BendPoint - StartPoint;
	const FVector EndToCorner = EndPoint - BendPoint;
	const FVector StartPerp = FVector::CrossProduct(FVector::UpVector, StartToCorner.GetSafeNormal2D()).GetSafeNormal();
	const FVector EndPerp = FVector::CrossProduct(FVector::UpVector, EndToCorner.GetSafeNormal2D()).GetSafeNormal();
	
	//Start Face
	const FVector StartT = StartPoint + FVector::UpVector * (Height * 0.5f);
	const FVector StartB = StartPoint - FVector::UpVector * (Height * 0.5f);
	const FVector EndT = EndPoint + FVector::UpVector * (Height * 0.5f);
	const FVector EndB = EndPoint - FVector::UpVector * (Height * 0.5f);
	
	const FVector StartTL = StartT + StartPerp * (Width * 0.5f);
	const FVector StartTR = StartT - StartPerp * (Width * 0.5f);
	const FVector StartBL = StartB + StartPerp * (Width * 0.5f);
	const FVector StartBR = StartB - StartPerp * (Width * 0.5f);
	
	//End Face
	const FVector EndTL = EndT + EndPerp * (Width * 0.5f);
	const FVector EndTR = EndT - EndPerp * (Width * 0.5f);
	const FVector EndBL = EndB + EndPerp * (Width * 0.5f);
	const FVector EndBR = EndB - EndPerp * (Width * 0.5f);
	

	FVector BendTL = FMath::LinePlaneIntersection(EndTL, EndTL + EndToCorner, StartTL, StartPerp);
	if(BendTL.ContainsNaN())
	{
		const FVector Perp = FVector::CrossProduct(EndPerp, FVector::UpVector).GetSafeNormal();
		BendTL = FMath::LinePlaneIntersection(EndTL, EndTL + EndToCorner, BendPoint, Perp);
	}
	FVector BendTR = FMath::LinePlaneIntersection(EndTR, EndTR + EndToCorner, StartTR, StartPerp);
	if(BendTR.ContainsNaN())
	{
		const FVector Perp = FVector::CrossProduct(EndPerp, FVector::UpVector).GetSafeNormal();
		BendTR = FMath::LinePlaneIntersection(EndTR, EndTR + EndToCorner, BendPoint, Perp);
	}
	FVector BendBL = FMath::LinePlaneIntersection(EndBL, EndBL + EndToCorner, StartBL, StartPerp);
	if(BendBL.ContainsNaN())
	{
		const FVector Perp = FVector::CrossProduct(EndPerp, FVector::UpVector).GetSafeNormal();
		BendBL = FMath::LinePlaneIntersection(EndBL, EndBL + EndToCorner, BendPoint, Perp);
	}
	FVector BendBR = FMath::LinePlaneIntersection(EndBR, EndBR + EndToCorner, StartBR, StartPerp);
	if(BendBR.ContainsNaN())
	{
		const FVector Perp = FVector::CrossProduct(EndPerp, FVector::UpVector).GetSafeNormal();
		BendBR = FMath::LinePlaneIntersection(EndBR, EndBR + EndToCorner, BendPoint, Perp);
	}

	Vertices[0] = StartBR;
	Vertices[1] = EndBR;
	Vertices[2] = EndBL;
	Vertices[3] = StartBL;
	Vertices[4] = StartTR ;
	Vertices[5] = EndTR;
	Vertices[6] = EndTL;
	Vertices[7] = StartTL;
	Vertices[8] = BendBR;
	Vertices[9] = BendTR;
	Vertices[10] = BendBL;
	Vertices[11] = BendTL;
	
	int D[3][3]{ { 1,2,0 }, { 2,0,1 }};
	auto ToFaceV = [&N, &D](int Dim, int D0, int D1)
	{
		return D0 + N[D[0][Dim]] * D1;
	};
	
	for (int i = 0; i < 8; ++i)
	{
		//A = Back/Front
		//B = Right/Left
		//C = Bottom/Top
		FIndex3i CornerSides = FIndex3i(
			(((i & 1) != 0) ^ ((i & 2) != 0)) ? 1 : 0,
			((i / 2) % 2 == 0) ? 0 : 1,
			(i < 4) ? 0 : 1);
		for (int Dim = 0; Dim < 3; Dim++)
		{
			int FaceIdx = FaceDimOrder[Dim]*2 + CornerSides[Dim];
			int D0Ind = CornerSides[D[0][Dim]] * (N[D[0][Dim]] - 1);
			int D1Ind = CornerSides[D[1][Dim]] * (N[D[1][Dim]] - 1);
			int CornerVInd = ToFaceV(Dim, D0Ind, D1Ind);
			if(VerticesPerFace[Dim] > CornerVInd && FaceVertIndices[FaceIdx][CornerVInd] < 0)
			{
				FaceVertIndices[FaceIdx][CornerVInd] = i;
			}
		}
	}
	
	int CurrentVertIndex = 8;
	for (int Dim = 0; Dim < 3; Dim++)
	{
		int EdgeLen = N[Dim];
		if (EdgeLen <= 2)
		{
			continue; // no internal edge vertices on this dimension
		}
		int MajorFaceInd = FaceDimOrder[Dim] * 2;
		int FaceInds[2]{ -1,-1 };
		int DSides[2]{ 0,0 };
		for (DSides[0] = 0; DSides[0] < 2; DSides[0]++)
		{
			FaceInds[0] = FaceDimOrder[D[0][Dim]] * 2 + DSides[0];
			for (DSides[1] = 0; DSides[1] < 2; DSides[1]++)
			{
				FaceInds[1] = FaceDimOrder[D[1][Dim]] * 2 + DSides[1];
				int MajorCornerInd = ToFaceV(Dim, DSides[0] * (N[D[0][Dim]] - 1), DSides[1] * (N[D[1][Dim]] - 1));
				FVector3d Corners[2]
				{
					Vertices[FaceVertIndices[MajorFaceInd  ][MajorCornerInd]],
					Vertices[FaceVertIndices[MajorFaceInd+1][MajorCornerInd]]
				};
					
				for (int EdgeVert = 1; EdgeVert + 1 < EdgeLen; EdgeVert++)
				{
					for (int WhichFace = 0; WhichFace < 2; WhichFace++) // each edge is shared by two faces (w/ major axes of subdim 0 and subdim 1 respectively)
					{
						int FaceDim = D[WhichFace][Dim];
						int SubDims[2];
						SubDims[1 - WhichFace] = EdgeVert;
						SubDims[WhichFace] = NTri[D[WhichFace][FaceDim]] * DSides[1-WhichFace];
						int FaceV = ToFaceV(FaceDim, SubDims[0], SubDims[1]);
						FaceVertIndices[FaceInds[WhichFace]][FaceV] = CurrentVertIndex;
					}
					CurrentVertIndex++;
				}
			}
		}
	}
	
	const FVector Extents[2]{FVector(StartToCorner.Length(), Width, Height), FVector(EndToCorner.Length(), Width, Height)};
	double MaxDimension[2] = {MaxAbsElement(Extents[0]), MaxAbsElement(Extents[1])};
	FQuat Rotations[2] = {FQuat(StartToCorner.Rotation()), FQuat(EndToCorner.Rotation())};
	
	
	// create the face triangles and UVs+normals
	int CurrentTriIdx = 0;
	int CurrentUVIdx = 0;
	int CurrentQuadIdx = 0;
	
	for (int Dim = 0; Dim < 3; Dim++)
		{
			float UVScale = (1.0f / (CurrentUVIdx < 4  ? (float)MaxDimension[1] : (float)MaxDimension[0]));
			int FaceIdxBase = FaceDimOrder[Dim]*2;
		
			// UV-specific minor axes + flips; manually set to match default UnrealEngine cube texture arrangement
			int Minor1Flip[3] = { -1, 1, 1 };
			int Minor2Flip[3] = { -1, -1, 1 };

			// UV scales for D0, D1
			double FaceWidth = FMathd::Abs(CurrentUVIdx < 4  ? Extents[1][D[0][Dim]] : Extents[0][D[0][Dim]]);
			double FaceHeight = FMathd::Abs(CurrentUVIdx < 4  ? Extents[1][D[1][Dim]]: Extents[0][D[1][Dim]]);
			double WidthUVScale = FaceWidth * UVScale;
			double HeightUVScale = FaceHeight * UVScale;
		
			for (int Side = 0; Side < 2; Side++)
			{
				int SideOpp = 1 - Side;
				int SideSign = Side * 2 - 1;

				FVector3f Normal(0, 0, 0);
				Normal[Dim] = float(2 * Side - 1);
				Normal = (FVector3f)(FQuat(CurrentUVIdx < 4  ? Rotations[1] : Rotations[0])*(FVector3d)Normal);
				
				int MajorFaceInd = FaceIdxBase + Side;

				int FaceUVStartInd = CurrentUVIdx;
				// set all the UVs and normals
				FVector2f UV;
				int UVXDim = Dim == 1 ? 1 : 0;	// which dim (of D0,D1) follows the horizontal UV coordinate
				int UVYDim = 1 - UVXDim;		// which dim (of D0,D1) follows the vertical UV coordinate
				
				for (int D0 = 0; D0 < N[D[0][Dim]]; D0++)
				{
					for (int D1 = 0; D1 < N[D[1][Dim]]; D1++)
					{
						// put the grid coordinates (centered at 0,0) into the UVs
						UV[UVXDim] = float (D0 * Nscale[D[0][Dim]] - .5);
						UV[UVYDim] = float (D1 * Nscale[D[1][Dim]] - .5);
						// invert axes to match the desired UV patterns & so the opp faces are not backwards
						UV.X *= float(SideSign * Minor1Flip[Dim]);
						UV.Y *= float(Minor2Flip[Dim]);
						// recenter and scale up
						UV[UVXDim] = float ( (UV[UVXDim] + .5f) * WidthUVScale);
						UV[UVYDim] = float ( (UV[UVYDim] + .5f) * HeightUVScale);
						UVs[CurrentUVIdx] = UV;
						Normals[CurrentUVIdx] = Normal;
						UVParentVertex[CurrentUVIdx] = FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)];
						NormalParentVertex[CurrentUVIdx] = FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)];
						CurrentUVIdx++;
					}
				}
				
				// set all the triangles
				for (int D0 = 0; D0 + 1 < N[D[0][Dim]]; D0++)
				{
					for (int D1 = 0; D1 + 1 < N[D[1][Dim]]; D1++)
					{
						SetTriangle(CurrentTriIdx,
								FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)],
								FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+SideOpp, D1+Side)],
								FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+1, D1+1)]
							);
						SetTriangleUVs(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+Side + (D0+SideOpp) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]]
						);
						SetTriangleNormals(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+Side + (D0+SideOpp) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]]
						);
						SetTrianglePolygon(CurrentTriIdx, MajorFaceInd);
						CurrentTriIdx++;

						SetTriangle(CurrentTriIdx,
							FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)],
							FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+1, D1+1)],
							FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+Side, D1+SideOpp)]
						);
						SetTriangleUVs(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]],
							FaceUVStartInd + D1+SideOpp + (D0+Side) * N[D[1][Dim]]
						);
						SetTriangleNormals(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]],
							FaceUVStartInd + D1+SideOpp + (D0+Side) * N[D[1][Dim]]
						);
						SetTrianglePolygon(CurrentTriIdx,  MajorFaceInd);
						CurrentTriIdx++;
						CurrentQuadIdx++;
					}
				}
			}
		}
	
	return *this;
}

FMeshShapeGenerator& FHallWayGenerator::Generate()
{

	FIndex3i N = EdgeVertices;
	N.A = FMath::Max(2, N.A);
	N.B = FMath::Max(2, N.B);
	N.C = FMath::Max(2, N.C);

	FVector3d Nscale(1.0 / (N.A-1), 1.0 / (N.B-1), 1.0 / (N.C-1));

	FIndex3i NInternal = N;
	NInternal.A -= 2; NInternal.B -= 2; NInternal.C -= 2;

	FIndex3i NTri = N;
	NTri.A--; NTri.B--; NTri.C--;
	int NumUVsAndNormals = 2 * (N.A * N.B + N.B * N.C + N.C * N.A);
	int NumVertices = 8 + (NInternal.A + NInternal.B + NInternal.C) * 4 + (NInternal.A*NInternal.B + NInternal.B*NInternal.C + NInternal.C*NInternal.A) * 2;
	int NumTriangles = 4 * (NTri.A * NTri.B + NTri.B * NTri.C + NTri.C * NTri.A);
	
	SetBufferSizes(NumVertices, NumTriangles, NumUVsAndNormals, NumUVsAndNormals);

	int FaceDimOrder[3]{ 1, 2, 0 }; // ordering from IndexUtil.cpp
	int D[2][3]{ { 1,2,0 }, { 2,0,1 } };

	FIndex3i VerticesPerFace(N.B * N.C, N.A * N.C, N.A * N.B);
	TArray<TArray<int>> FaceVertIndices;
	FaceVertIndices.SetNum(6);
	for (int Dim = 0; Dim < 3; Dim++)
	{
		int FaceIdxForDim = FaceDimOrder[Dim] * 2;
		int VertexNum = VerticesPerFace[Dim];
		FaceVertIndices[FaceIdxForDim].SetNum(VertexNum);
		FaceVertIndices[FaceIdxForDim+1].SetNum(VertexNum);
	}
	
	const FVector HallwayDirection = EndPoint - StartPoint;
	const FVector Norm2D = HallwayDirection.GetSafeNormal2D();
	const FVector DirectionPerp = FVector::CrossProduct(FVector::UpVector, Norm2D).GetSafeNormal();

	auto ToFaceV = [&N, &D](int Dim, int D0, int D1)
	{
		return D0 + N[D[0][Dim]] * D1;
	};

	// create the corners and distribute them into the face mapping
	// corners [ (-x,-y), (x,-y), (x,y), (-x,y) ], -z, then +z
	//
	//   7---4       
	//   |\  |\                     
	//   3-\-0 \                     
	//    \ 6---5                    
	//     \|   |                     
	//      2---1                   
	float UpOrDown = 1;
	float LeftOrRight = 1;
	FVector FaceOrigin[8] = {StartPoint, EndPoint, EndPoint, StartPoint, StartPoint, EndPoint, EndPoint, StartPoint};
	for (int i = 0; i < 8; ++i)
	{
		
		UpOrDown = i%4 == 0 ? -UpOrDown : UpOrDown;
		LeftOrRight = i%2 == 0 ? -LeftOrRight : LeftOrRight;
		
		const FVector Start = FaceOrigin[i] + (FVector::UpVector*UpOrDown) * (Height * 0.5f);
		Vertices[i] = Start + (DirectionPerp*LeftOrRight) * (Width * 0.5f);
		FIndex3i CornerSides = FIndex3i(
			(((i & 1) != 0) ^ ((i & 2) != 0)) ? 1 : 0,
			((i / 2) % 2 == 0) ? 0 : 1,
			(i < 4) ? 0 : 1);
		for (int Dim = 0; Dim < 3; Dim++)
		{
			int FaceIdx = FaceDimOrder[Dim]*2 + CornerSides[Dim];
			int D0Ind = CornerSides[D[0][Dim]] * (N[D[0][Dim]] - 1);
			int D1Ind = CornerSides[D[1][Dim]] * (N[D[1][Dim]] - 1);
			int CornerVInd = ToFaceV(Dim, D0Ind, D1Ind);
			FaceVertIndices[FaceIdx][CornerVInd] = i;
		}
	}

	// create the internal (non-corner) edge vertices and distribute them into the face mapping
	int CurrentVertIndex = 8;
	for (int Dim = 0; Dim < 3; Dim++)
	{
		int EdgeLen = N[Dim];
		if (EdgeLen <= 2)
		{
			continue; // no internal edge vertices on this dimension
		}
		int MajorFaceInd = FaceDimOrder[Dim] * 2;
		int FaceInds[2]{ -1,-1 };
		int DSides[2]{ 0,0 };
		for (DSides[0] = 0; DSides[0] < 2; DSides[0]++)
		{
			FaceInds[0] = FaceDimOrder[D[0][Dim]] * 2 + DSides[0];
			for (DSides[1] = 0; DSides[1] < 2; DSides[1]++)
			{
				FaceInds[1] = FaceDimOrder[D[1][Dim]] * 2 + DSides[1];
				int MajorCornerInd = ToFaceV(Dim, DSides[0] * (N[D[0][Dim]] - 1), DSides[1] * (N[D[1][Dim]] - 1));
				FVector3d Corners[2]
				{
					Vertices[FaceVertIndices[MajorFaceInd  ][MajorCornerInd]],
					Vertices[FaceVertIndices[MajorFaceInd+1][MajorCornerInd]]
				};
					
				for (int EdgeVert = 1; EdgeVert + 1 < EdgeLen; EdgeVert++)
				{
					Vertices[CurrentVertIndex] = Lerp(Corners[0], Corners[1], EdgeVert * Nscale[Dim]);
					for (int WhichFace = 0; WhichFace < 2; WhichFace++) // each edge is shared by two faces (w/ major axes of subdim 0 and subdim 1 respectively)
					{
						int FaceDim = D[WhichFace][Dim];
						int SubDims[2];
						SubDims[1 - WhichFace] = EdgeVert;
						SubDims[WhichFace] = NTri[D[WhichFace][FaceDim]] * DSides[1-WhichFace];
						int FaceV = ToFaceV(FaceDim, SubDims[0], SubDims[1]);
						FaceVertIndices[FaceInds[WhichFace]][FaceV] = CurrentVertIndex;
					}
					CurrentVertIndex++;
				}
			}
		}
	}

	// create the internal (non-corner, non-edge) face vertices and distribute them into the face mapping
	for (int Dim = 0; Dim < 3; Dim++)
	{
		int FaceIdxBase = FaceDimOrder[Dim]*2;
		int FaceInternalVNum = NInternal[D[0][Dim]] * NInternal[D[1][Dim]];
		if (FaceInternalVNum <= 0)
		{
			continue;
		}

		for (int Side = 0; Side < 2; Side++)
		{
			int MajorFaceInd = FaceIdxBase + Side;
			for (int D0 = 1; D0 + 1 < N[D[0][Dim]]; D0++)
			{
				int BotInd = ToFaceV(Dim, D0, 0);
				int TopInd = ToFaceV(Dim, D0, N[D[1][Dim]] - 1);

				FVector3d Edges[2]
				{
					Vertices[FaceVertIndices[MajorFaceInd][BotInd]],
					Vertices[FaceVertIndices[MajorFaceInd][TopInd]]
				};
				for (int D1 = 1; D1 + 1 < N[D[1][Dim]]; D1++)
				{
					Vertices[CurrentVertIndex] = Lerp(Edges[0], Edges[1], D1 * Nscale[D[1][Dim]]);
					FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)] = CurrentVertIndex;
					CurrentVertIndex++;
				}
			}
		}
	}

	const FVector StartToEnd = EndPoint - StartPoint;
	const FVector Extents(StartToEnd.Length(), Width, Height);
	double MaxDimension = MaxAbsElement(Extents);
	float UVScale = (1.0f / (float)MaxDimension);
	
	// create the face triangles and UVs+normals
	int CurrentTriIdx = 0;
	int CurrentUVIdx = 0;
	int CurrentQuadIdx = 0;
	
	for (int Dim = 0; Dim < 3; Dim++)
		{
			int FaceIdxBase = FaceDimOrder[Dim]*2;
		
			// UV-specific minor axes + flips; manually set to match default UnrealEngine cube texture arrangement
			int Minor1Flip[3] = { -1, 1, 1 };
			int Minor2Flip[3] = { -1, -1, 1 };

			// UV scales for D0, D1
			double FaceWidth = FMathd::Abs(Extents[D[0][Dim]]);
			double FaceHeight = FMathd::Abs(Extents[D[1][Dim]]);
			double WidthUVScale = FaceWidth * UVScale;
			double HeightUVScale = FaceHeight * UVScale;
		
			for (int Side = 0; Side < 2; Side++)
			{
				int SideOpp = 1 - Side;
				int SideSign = Side * 2 - 1;

				FVector3f Normal(0, 0, 0);
				Normal[Dim] = float(2 * Side - 1);
				Normal = (FVector3f)(FQuat(StartToEnd.Rotation())*(FVector3d)Normal);
				
				int MajorFaceInd = FaceIdxBase + Side;

				int FaceUVStartInd = CurrentUVIdx;
				// set all the UVs and normals
				FVector2f UV;
				int UVXDim = Dim == 1 ? 1 : 0;	// which dim (of D0,D1) follows the horizontal UV coordinate
				int UVYDim = 1 - UVXDim;		// which dim (of D0,D1) follows the vertical UV coordinate

				for (int D0 = 0; D0 < N[D[0][Dim]]; D0++)
				{
					for (int D1 = 0; D1 < N[D[1][Dim]]; D1++)
					{
						// put the grid coordinates (centered at 0,0) into the UVs
						UV[UVXDim] = float (D0 * Nscale[D[0][Dim]] - .5);
						UV[UVYDim] = float (D1 * Nscale[D[1][Dim]] - .5);
						// invert axes to match the desired UV patterns & so the opp faces are not backwards
						UV.X *= float(SideSign * Minor1Flip[Dim]);
						UV.Y *= float(Minor2Flip[Dim]);
						// recenter and scale up
						UV[UVXDim] = float ( (UV[UVXDim] + .5f) * WidthUVScale);
						UV[UVYDim] = float ( (UV[UVYDim] + .5f) * HeightUVScale);
						UVs[CurrentUVIdx] = UV;
						Normals[CurrentUVIdx] = Normal;
						UVParentVertex[CurrentUVIdx] = FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)];
						NormalParentVertex[CurrentUVIdx] = FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)];
						CurrentUVIdx++;
					}
				}
				
				// set all the triangles
				for (int D0 = 0; D0 + 1 < N[D[0][Dim]]; D0++)
				{
					for (int D1 = 0; D1 + 1 < N[D[1][Dim]]; D1++)
					{
						SetTriangle(CurrentTriIdx,
								FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)],
								FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+SideOpp, D1+Side)],
								FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+1, D1+1)]
							);
						SetTriangleUVs(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+Side + (D0+SideOpp) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]]
						);
						SetTriangleNormals(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+Side + (D0+SideOpp) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]]
						);
						SetTrianglePolygon(CurrentTriIdx, MajorFaceInd);
						CurrentTriIdx++;

						SetTriangle(CurrentTriIdx,
							FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0, D1)],
							FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+1, D1+1)],
							FaceVertIndices[MajorFaceInd][ToFaceV(Dim, D0+Side, D1+SideOpp)]
						);
						SetTriangleUVs(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]],
							FaceUVStartInd + D1+SideOpp + (D0+Side) * N[D[1][Dim]]
						);
						SetTriangleNormals(CurrentTriIdx,
							FaceUVStartInd + D1 + (D0) * N[D[1][Dim]],
							FaceUVStartInd + D1+1 + (D0+1) * N[D[1][Dim]],
							FaceUVStartInd + D1+SideOpp + (D0+Side) * N[D[1][Dim]]
						);
						SetTrianglePolygon(CurrentTriIdx,  MajorFaceInd);
						CurrentTriIdx++;
						CurrentQuadIdx++;
					}
				}
			}
		}
	return *this;
}

static void ApplyPrimitiveOptionsToMesh(
	FDynamicMesh3& Mesh, 
	FGeometryScriptPrimitiveOptions PrimitiveOptions)
{
	if (PrimitiveOptions.PolygroupMode == EGeometryScriptPrimitivePolygroupMode::SingleGroup)
	{
		for (int32 tid : Mesh.TriangleIndicesItr())
		{
			Mesh.SetTriangleGroup(tid, 0);
		}
	}
	if (PrimitiveOptions.bFlipOrientation)
	{
		Mesh.ReverseOrientation(true);
		if (Mesh.HasAttributes())
		{
			FDynamicMeshNormalOverlay* Normals = Mesh.Attributes()->PrimaryNormals();
			for (int elemid : Normals->ElementIndicesItr())
			{
				Normals->SetElement(elemid, -Normals->GetElement(elemid));
			}
		}
	}
}

static void AppendPrimitive(
	UDynamicMesh* TargetMesh,
	UE::Geometry::FMeshShapeGenerator* Generator,
	FGeometryScriptPrimitiveOptions PrimitiveOptions)
{
	if (TargetMesh->IsEmpty())
	{
		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			EditMesh.Copy(Generator);
			ApplyPrimitiveOptionsToMesh(EditMesh, PrimitiveOptions);
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	}
	else
	{
		FDynamicMesh3 TempMesh(Generator);
		ApplyPrimitiveOptionsToMesh(TempMesh, PrimitiveOptions);
		TargetMesh->EditMesh([&](FDynamicMesh3& EditMesh)
		{
			FMeshIndexMappings TmpMappings;
			FDynamicMeshEditor Editor(&EditMesh);
			Editor.AppendMesh(&TempMesh, TmpMappings);
		}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
	}
	
}

UDynamicMesh* UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallowedBox(UDynamicMesh* TargetMesh,
                                                                                   FGeometryScriptPrimitiveOptions PrimitiveOptions, FTransform Transform, float DimensionX, float DimensionY,
                                                                                   float DimensionZ, float WallThickness, bool OpenEdges, int32 StepsX, int32 StepsY,
                                                                                   int32 StepsZ, EGeometryScriptPrimitiveOriginMode Origin, UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("DungeonGenerationFunctions::AppendHallowedBox", "AppendHallowedBox: TargetMesh is Null"));
		return TargetMesh;
	}

	TargetMesh = UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
				(
				TargetMesh
					, PrimitiveOptions
					, Transform
					,  DimensionX
					, DimensionY
					,  DimensionZ
					, StepsX, StepsY, StepsZ
					, Origin
					, Debug
				);
	UDynamicMesh* BoolMesh = NewObject<UDynamicMesh>();

	UGeometryScriptLibrary_MeshPrimitiveFunctions::AppendBox
				(
				BoolMesh
					, PrimitiveOptions
					, Transform
					, OpenEdges ? DimensionX : DimensionX - WallThickness
					, DimensionY - WallThickness
					,  DimensionZ - WallThickness
					, StepsX, StepsY, StepsZ
					, Origin
					, Debug
				);

	FGeometryScriptMeshBooleanOptions BoolOptions;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
				(
					TargetMesh,
					FTransform::Identity,
					BoolMesh,
					FTransform::Identity,
					EGeometryScriptBooleanOperation::Subtract,
					BoolOptions		
				);
	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallwayCorner(UDynamicMesh* TargetMesh,
	FGeometryScriptPrimitiveOptions PrimitiveOptions, FVector Start, FVector BendPoint, FVector End, float DimensionY,
	float DimensionZ, float WallThickness, bool OpenEdges, bool OpenTop,
	UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("PrimitiveFunctions_AppendHallwayCorner", "AppendHallwayCorner: TargetMesh is Null"));
		return TargetMesh;
	}

	FHallWayCornerGenerator CornerGenerator;
	CornerGenerator.EdgeVertices = FIndex3i(0, 0, 0);
	CornerGenerator.StartPoint = Start;
	CornerGenerator.BendPoint = BendPoint;
	CornerGenerator.EndPoint = End;
	CornerGenerator.Width = DimensionY;
	CornerGenerator.Height = DimensionZ;
	CornerGenerator.Generate();

	AppendPrimitive(TargetMesh, &CornerGenerator, PrimitiveOptions);
		
	if(!OpenEdges)
	{
		FVector StartToBend = (BendPoint - Start).GetSafeNormal();
		Start += StartToBend*WallThickness;

		FVector EndToBend = (BendPoint - End).GetSafeNormal();
		End += EndToBend*WallThickness;
	}
	
	CornerGenerator = FHallWayCornerGenerator();
	CornerGenerator.EdgeVertices = FIndex3i(0, 0, 0);
	CornerGenerator.StartPoint = Start + FVector::UpVector * (OpenTop ?  WallThickness : 0.0f);;
	CornerGenerator.BendPoint = BendPoint + FVector::UpVector * (OpenTop ?  WallThickness : 0.0f);;
	CornerGenerator.EndPoint = End + FVector::UpVector * (OpenTop ?  WallThickness : 0.0f);;
	CornerGenerator.Width = DimensionY - WallThickness;
	CornerGenerator.Height = DimensionZ - (OpenTop ?  0.0f : WallThickness);
	CornerGenerator.Generate();

	UDynamicMesh* BoolMesh = NewObject<UDynamicMesh>();
	AppendPrimitive(BoolMesh, &CornerGenerator, PrimitiveOptions);
	
	FGeometryScriptMeshBooleanOptions BoolOptions;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
				(
					TargetMesh,
					FTransform::Identity,
					BoolMesh,
					FTransform::Identity,
					EGeometryScriptBooleanOperation::Subtract,
					BoolOptions		
				);
	
	return TargetMesh;
}

UDynamicMesh* UGeometryScriptLibrary_DungeonGenerationFunctions::AppendHallway(UDynamicMesh* TargetMesh,
                                                                               FGeometryScriptPrimitiveOptions PrimitiveOptions, FVector Start, FVector End, float DimensionY,
                                                                               float DimensionZ, float WallThickness, bool OpenEdges, bool OpenTop, int32 StepsX, int32 StepsY, int32 StepsZ, UGeometryScriptDebug* Debug)
{
	if (TargetMesh == nullptr)
	{
		UE::Geometry::AppendError(Debug, EGeometryScriptErrorType::InvalidInputs, LOCTEXT("PrimitiveFunctions_AppendStaircaseHallway", "AppendStaircaseHallway: TargetMesh is Null"));
		return TargetMesh;
	}

	FHallWayGenerator HallwayGenerator;
	HallwayGenerator.EdgeVertices = FIndex3i(FMath::Max(0, StepsX), FMath::Max(0, StepsY), FMath::Max(0, StepsZ));
	HallwayGenerator.StartPoint = Start;
	HallwayGenerator.EndPoint = End;
	HallwayGenerator.Width = DimensionY;
	HallwayGenerator.Height = DimensionZ;
	HallwayGenerator.Generate();
	AppendPrimitive(TargetMesh, &HallwayGenerator, PrimitiveOptions);
	
	if(!OpenEdges)
	{
		FVector StartToEnd = (End - Start).GetSafeNormal();
		Start -= StartToEnd*WallThickness;
		End += StartToEnd*WallThickness;
	}
	
	HallwayGenerator = FHallWayGenerator();
	HallwayGenerator.EdgeVertices = FIndex3i(FMath::Max(0, StepsX), FMath::Max(0, StepsY), FMath::Max(0, StepsZ));
	HallwayGenerator.StartPoint = Start + FVector::UpVector * (OpenTop ?  WallThickness : 0.0f);
	HallwayGenerator.EndPoint = End + FVector::UpVector * (OpenTop ?  WallThickness : 0.0f);
	HallwayGenerator.Width = DimensionY - WallThickness;
	HallwayGenerator.Height = DimensionZ - (OpenTop ?  0.0f : WallThickness);
	HallwayGenerator.Generate();
	
	UDynamicMesh* BoolMesh = NewObject<UDynamicMesh>();
	AppendPrimitive(BoolMesh, &HallwayGenerator, PrimitiveOptions);
	
	FGeometryScriptMeshBooleanOptions BoolOptions;
	UGeometryScriptLibrary_MeshBooleanFunctions::ApplyMeshBoolean
				(
					TargetMesh,
					FTransform::Identity,
					BoolMesh,
					FTransform::Identity,
					EGeometryScriptBooleanOperation::Subtract,
					BoolOptions		
				);
	return TargetMesh;
}
