// © 2020, GP Games and Double Trouble Games

#pragma once

#include "CoreMinimal.h"
#include "ANephilimBase.h"
#include "EnemyBase.generated.h"

class AOvermindBase;

// Enemy Tiers enum holds the different tiers of enemies in the game.
UENUM(BlueprintType)
enum class EEnemyTiers : uint8
{
	Boss	UMETA(DisplayName = "Boss"),
	Elite	UMETA(DisplayName = "Elite"),
	Mob		UMETA(DisplayName = "Mob"),
};

enum class EEnemyAbilities : uint8
{
	Attack		UMETA(DisplayName = "Attack"),
	Special		UMETA(DisplayName = "Special"),
	CloseGap	UMETA(DisplayName = "Close Gap"),
};

USTRUCT(BlueprintType)
struct FEnemyAbility
{
	GENERATED_BODY()

public:
	FEnemyAbility() : Cooldown(0), AbilityEffect(EEnemyAbilities::Attack)
	{

	}

	float Cooldown;
	EEnemyAbilities AbilityEffect;
};

/**
 * 
 */
UCLASS()
class NIGHTFALL_API AEnemyBase : public AANephilimBase
{
	GENERATED_BODY()

public:

	TArray<FEnemyAbility> Abilities;

	/**
	 * Set default values.
	 */
	AEnemyBase();

	// Called every frame
	virtual void Tick(float DeltaTime) override;


	/**
	* How mad the enemy is at you.
	*/
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings" )
		float Threat;
	
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		float SightRadius;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
		float DistanceToTarget;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
		AANephilimBase* TargetActor;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
		bool bInCombat;

	UPROPERTY(BlueprintReadOnly, Category = "Data")
		bool bIsActive;

	UPROPERTY(BlueprintReadWrite, Category = "Overmind")
		AOvermindBase* Overmind;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Overmind")
		float Variation;

	/**
	 * How powerful this enemy is.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "Settings")
		EEnemyTiers Tier;

	/**
	 * Aggro is the calculation of Threat
	 * Threat is measured based on the current health of the enemy.
	 * Threat is backwards, it goes from [1-0], being 0 highest Threat.
	 */
	UFUNCTION(BlueprintCallable, BlueprintCallable)
		void Aggro();	

	/**
	* Perform a basic attack.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action")
		bool AttackNormal();
		bool AttackNormal_Implementation();

	/**
	* Perform a special attack.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action")
		bool AttackSpecial();
		bool AttackSpecial_Implementation();

	/**
	* Check if DistanceToTarget is in between two values, if so returns true.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action")
		bool CloseGap(float MinRange, float MaxRange);
		bool CloseGap_Implementation(float MinRange, float MaxRange);

	/**
	* Update DistanceToTarget by getting the distance between a target and itself.
	*/
	UFUNCTION(BlueprintCallable, Category = "Information")
		void GetDistance(const AANephilimBase* TargetNephilim);

	/**
	* Pick which ability to use.
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Action")
		bool ActionPicker(const AANephilimBase* TargetNephilim);
		bool ActionPicker_Implementation(const AANephilimBase* TargetNephilim);

	UFUNCTION(BlueprintCallable, Category = "Information")
		int32 RandomVariation(float Small, float Medium, float Large);

	/**
	* Set the rotation of a projectile and return the velocity for the projectile.
	* Use SetVelocityInLocalSpace(this result) to set the velocity of the ProjectileComponent.
	*/
	UFUNCTION(BlueprintCallable, Category = "Calculations")
		FVector GetThrowVelocity(FVector& Direction, FVector StartLocation, FVector TargetLocation, float Angle);

	UFUNCTION(BlueprintCallable, Category = "Calculations")
		AANephilimBase* SightBubble
		(
			const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
			bool bTraceComplex,
			const TArray<AActor*>& ActorsToIgnore, 
			TArray<FHitResult>& OutHits,
			bool bDebug
		);

	// Inform Overmind of this enemy's death.
	void CharacterDeath_Implementation(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser) override;

	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "AI")
		void ActivateAI(bool Activate);
		void ActivateAI_Implementation(bool Activate);

private:
	UObject* World;
};

