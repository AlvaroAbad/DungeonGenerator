// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonGenerationHelperFunctions.generated.h"

/**
 * 
 */
UCLASS()
class UDungeonGenerationHelperFunctions : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category = "DungeonGenerator", meta=(ScriptMethod))
	static FVector NegateUnitVector(const FVector& UnitVector);

	UFUNCTION(BlueprintCallable, Category = "DungeonGenerator", meta=(ScriptMethod))
	static FVector RoundToUnitVector(const FVector& UnitVector);
};
