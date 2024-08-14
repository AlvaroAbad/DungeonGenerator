// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Generators/MeshShapeGenerator.h"
#include "GeometryScript/MeshPrimitiveFunctions.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "GeometryScriptLibrary_DungeonGenerationFunctions.generated.h"

class UDynamicMesh;

class FHallWayCornerGenerator : public UE::Geometry::FMeshShapeGenerator
{
public:
	UE::Geometry::FIndex3i EdgeVertices { 12,12,12 };
	FVector StartPoint;
	FVector BendPoint;
	FVector EndPoint;
	float Width;
	float Height;
public:
	virtual FMeshShapeGenerator& Generate() override;

};

class FHallWayGenerator : public UE::Geometry::FMeshShapeGenerator
{
public:
	UE::Geometry::FIndex3i EdgeVertices { 8,8,8 };
	FVector StartPoint;
	FVector EndPoint;
	float Width;
	float Height;
public:
	virtual FMeshShapeGenerator& Generate() override;
};

UCLASS()
class UGeometryScriptLibrary_DungeonGenerationFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Primitives", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AppendHallowedBox(
		UDynamicMesh* TargetMesh,
		FGeometryScriptPrimitiveOptions PrimitiveOptions,
		FTransform Transform,
		float DimensionX = 100,
		float DimensionY = 100,
		float DimensionZ = 100,
		float WallThickness = 10,
		bool OpenEdges = true,
		int32 StepsX = 0,
		int32 StepsY = 0,
		int32 StepsZ = 0,
		EGeometryScriptPrimitiveOriginMode Origin = EGeometryScriptPrimitiveOriginMode::Base, UGeometryScriptDebug* Debug = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Primitives", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
	AppendHallwayCorner(
		UDynamicMesh* TargetMesh,
		FGeometryScriptPrimitiveOptions PrimitiveOptions,
		FVector Start,
		FVector BendPoint,
		FVector End,
		float DimensionY = 100,
		float DimensionZ = 100,
		float WallThickness = 10,
		bool OpenEdges = true,
		bool OpenTop = false,
		UGeometryScriptDebug* Debug = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "GeometryScript|Primitives", meta=(ScriptMethod))
	static UPARAM(DisplayName = "Target Mesh") UDynamicMesh* 
		AppendHallway(
		UDynamicMesh* TargetMesh,
		FGeometryScriptPrimitiveOptions PrimitiveOptions,
		FVector Start,
		FVector End,
		float DimensionY = 100,
		float DimensionZ = 100,
		float WallThickness = 10,
		bool OpenEdges = true,
		bool OpenTop = false,
		int32 StepsX = 0, int32 StepsY = 0, int32 StepsZ = 0, UGeometryScriptDebug* Debug = nullptr
	);
};
