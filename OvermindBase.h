// © 2020 GP Games, Double Trouble Games

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ANephilimBase.h"
#include "EnemyBase.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "OvermindBase.generated.h"

USTRUCT(BlueprintType)
struct FEnemySpawnInfo
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		TSubclassOf<AEnemyBase> EnemyType;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0.0", ClampMax = "100.0", UIMin = "0.0", UIMax = "100.0"))
		float Weight;
	UPROPERTY(BlueprintReadWrite, EditAnywhere, meta = (ClampMin = "0.0", UIMin = "0.0"))
		int32 Quantity;
};

UCLASS()
class NIGHTFALL_API AOvermindBase : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AOvermindBase();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	// Pool of flags indicating which enemies are available on the enemy pool, "false" is ready to be used.
	UPROPERTY()
		TArray<bool> ActiveEnemies;
	// Game mode being used.
	AGameMode_NephilimBase* GameMode;
	// Damage type required to kill enemy when too far from player.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Setup")
		TSubclassOf<UDamageType> OutOfBoundsDamageType;
	// Timer handle for spawn cooldown.
	FTimerHandle Timer;
	// The center point in between all players.
	UPROPERTY(BlueprintReadOnly)
		FVector AverageGroupLocation;

public:

	// Used to deactivate Overmind's tick functions.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		bool OvermindEnabled;

	// Array of players on the map.
	UPROPERTY(BlueprintReadOnly)
		TArray<AANephilimBase*> PlayerArray;

	// Dynamic pool of enemies controlled by this Overmind.
	UPROPERTY(BlueprintReadOnly)
		TArray<AEnemyBase*> EnemyArray;

	// Information on what to spawn and how many
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		TArray<FEnemySpawnInfo> SpawnInfo;

	// Delay before the first spawn in the map
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		float StartSpawnDelay;

	// Cooldown between each enemy spawn.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		float CooldownSpawn;

	// Closest distance an enemy can be spawned.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		float InnerSpawnRadius;

	// Longest distance an enemy can be spawned.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		float OuterSpawnRadius;

	// Range that Overmind will consider Enemy too far from player and kill it.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		float KillEnemyRange;

	// Navmesh Data
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Setup")
		ANavigationData* NavData;

	// Information on what to spawn and how many
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Debug")
		bool bVisionLineTrace;

	// Deprecated, map of enemies available to spawn on a weight.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Deprecated")
		TMap<TSubclassOf<AEnemyBase>, float> EnemyMap;

	// Deprecated, extra enemies to spawn.
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Deprecated")
		int32 ExtraEnemies;

	// Called every frame
	virtual void Tick(float DeltaTime) override;

	// Spawn Enemy on a radius around player based on difficulty.
	UFUNCTION(BlueprintCallable)
		void SpawnEnemy();

	// If enemy is too far from player, kill it.
	UFUNCTION(BlueprintCallable)
		void EnemyDistanceCheck();

	// Monitor player's health and return an integer between 1 and 4, being 4 when player is almost dying.
	UFUNCTION(BlueprintCallable)
		int PlayerCondition();

	// Used by enemy when killed by the player, it asks the Overmind to be deleted from the EnemyArray.
	UFUNCTION(BlueprintCallable)
		void EnemyKilled(AEnemyBase* Enemy);

	/*
	* Add a type of enemy to the array of enemies the Overmind can use.
	* This function only adds tier mob at the moment.
	* @param	Enemy Type: The enemy to be spawned.
	* @param	Weight: Frequency this enemy will be picked from the pool.
	* @param	Quantity: How many of this enemy type should be added to the pool.
	*/
	UFUNCTION(BlueprintCallable)
		void AddEnemies(AEnemyBase* EnemyType, float Weight = 50.0f);

	UFUNCTION(BlueprintCallable)
		void EnableOvermind(bool Enabled);

private:

	// Initialize pool of anemies in begin play.
	void StartEnemyPool();

	// Renturns index of first available enemy or -1 if no enemy is available.
	int32 FirstAvailableEnemy();

	// Get Enemy from Pool and place it in the world.
	void MakeEnemy();

	// Get the fardest player to gank it.
	int32 GetFardestPlayer();
	/*
	* Pick a random enemy from the available choices.
	* @return -1 if no available choice.
	*/
	int32 RandomEnemyType();

	// Reset enemy and put back on the map.
	void PlaceEnemy(int32 PoolIndex, FVector Location);

	// Simulate enemy vision.
	void EnemyVision();

	// Calculate average group location.
	void GroupLocation();

	void LookForManuallyPlacedEnemies();

	float AngleBetweenVectors(const FVector& v1, const FVector& v2);

	void CleanEnemyArray(TArray<int32> Indices);


};

