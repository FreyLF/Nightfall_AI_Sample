// © 2020, GP Games and Double Trouble Games


#include "EnemyBase.h"
#include "OvermindBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/KismetSystemLibrary.h"

/**
 * Set default values.
 */
AEnemyBase::AEnemyBase() : Variation(0)
{
    World = GetWorld();
	// Set this character to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

    // Inherited Section
    Team = ETeams::Enemy;
    bIsInvincible = false;
    bLockInPlace = false;
}

void AEnemyBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

/**
 * Aggro is the calculation of Threat
 * Threat is measured based on the current health of the enemy.
 * Threat is backwards, it goes from [1-0], being 0 highest Threat.
 */
void AEnemyBase::Aggro()
{
	Threat = GetHealth() / GetMaxHealth();
}

/**
* Perform a basic attack.
*/
bool AEnemyBase::AttackNormal_Implementation()
{
    return false;
}

/**
* Perform a special attack.
*/
bool AEnemyBase::AttackSpecial_Implementation()
{
	return false;   
}

/**
* Pick which ability to use.
*/
bool AEnemyBase::CloseGap_Implementation(float MinRange, float MaxRange)
{
	// If distance is 0 we have a problem...
	if (DistanceToTarget > 0.1f)
	{
		// Is this distance in a specific range?
		if (((MinRange < DistanceToTarget) && (DistanceToTarget < MaxRange)) == true)
		{
			return true;
		}
	}
    return false;
}

void AEnemyBase::GetDistance(const AANephilimBase* TargetNephilim)
{
    if (TargetNephilim != nullptr)
    {
		DistanceToTarget = FVector::Dist(this->GetActorLocation(), TargetNephilim->GetActorLocation());
    }
}

bool AEnemyBase::ActionPicker_Implementation(const AANephilimBase* TargetNephilim)
{
    if (TargetNephilim)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 1, FColor::Purple, "Hi");
        //for (uint8 i = 0; i < Abilities.Num(); ++i)
        //{
        //    if (Abilities[i].Cooldown <= 0)
        //    {
        //
        //    }
        //}


        return true;
    }
    return false;
}

int32 AEnemyBase::RandomVariation(float Small, float Medium, float Large)
{
    Variation = FMath::FRand();

    TArray<float> Weights = { Small, Medium, Large };
    Weights.Sort();

    //FString v = FString::SanitizeFloat(Weights[i]);
    //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, *v);

    if (Weights[1] >= Variation)
    {
        return 0;
    }
    if (Weights[0] < Variation && Weights[2] > Variation)
    {
        return 1;
    }
    return 2;
}

void AEnemyBase::CharacterDeath_Implementation(float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser)
{
    Super::CharacterDeath_Implementation(Damage, DamageType, InstigatedBy, DamageCauser);
    if (Overmind)
    {
        //GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "\nEnemyBase: Killing");
        //Overmind->EnemyKilled(this);
    }
}

FVector AEnemyBase::GetThrowVelocity(FVector& Direction, FVector StartLocation, FVector TargetLocation, float Angle)
{
    //  == Fixed Angle Projectile Velocity Calculation == 

    //const float Gravity = GetWorld()->GetGravityZ() * -1; // Gravity. (Must be a positive value)
    const float Gravity = 980.f; // This is the same as the line above (unless the project settings have been changed)
    const float Theta = (Angle * PI / 180); // Launch angle in radians (Angle being the launch angle in degrees)

    //FVector Direction = TargetLocation - StartLocation;
    Direction = TargetLocation - StartLocation;
    float HeightDifference = Direction.Z;
    Direction.Z = 0; // Remove height from direction
    float Distance = Direction.Size();

    const float V = (Distance / cos(Theta)) * FMath::Sqrt((Gravity * 1) / (2 * (Distance * tan(Theta) - HeightDifference)));
    FVector VelocityOutput = FVector(V * cos(Theta), 0, V * sin(Theta));

    Direction.Z = VelocityOutput.Z;

    return VelocityOutput;
}

AANephilimBase* AEnemyBase::SightBubble
(
    const TArray<TEnumAsByte<EObjectTypeQuery>>& ObjectTypes,
    bool bTraceComplex,
    const TArray<AActor*>& ActorsToIgnore,
    TArray<FHitResult>& OutHits,
    bool bDebug
)
{
    // Sight Bubble
    FVector StartLocation = GetActorLocation() + GetActorForwardVector() * (50 - SightRadius);
    FVector EndLocation = StartLocation + (0, 0, 1);
    
    TArray<FHitResult> SphereHits;
    bool FoundSomething;
    if (bDebug)
    {
        FoundSomething = UKismetSystemLibrary::SphereTraceMultiForObjects(World, StartLocation, EndLocation, SightRadius, ObjectTypes, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::ForDuration, SphereHits, true);
    }
    else
    {
        FoundSomething = UKismetSystemLibrary::SphereTraceMultiForObjects(World, StartLocation, EndLocation, SightRadius, ObjectTypes, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::None, SphereHits, true);
    }
    
    if (!FoundSomething)
    {
        return nullptr;
    }

    // Are my perseptions valid targets?

    int32 Target = -1;

    for (int32 i = 0; i < SphereHits.Num(); ++i)
    {
        // Is this entry a Nephilim?
        if (!SphereHits[i].GetActor()->IsA<AANephilimBase>())
        {
            continue;
        }

        // Can I trace a line towards this entry?
        EndLocation = SphereHits[i].GetActor()->GetActorLocation();
        TArray<FHitResult> Hit;
        FHitResult HitResult;

        bool isHit;
        if (bDebug)
        {
            // With debug lines
            isHit = UKismetSystemLibrary::LineTraceSingleForObjects(World, GetActorLocation(), EndLocation, ObjectTypes, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true);
            //isHit = UKismetSystemLibrary::LineTraceMultiForObjects(WorldContextObject, GetActorLocation(), EndLocation, ObjectTypes, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::ForDuration, Hit, true);
        }
        else
        {
            // No debug lines
            isHit = UKismetSystemLibrary::LineTraceSingleForObjects(World, GetActorLocation(), EndLocation, ObjectTypes, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true);
            //isHit = UKismetSystemLibrary::LineTraceMultiForObjects(WorldContextObject, GetActorLocation(), EndLocation, ObjectTypes, bTraceComplex, ActorsToIgnore, EDrawDebugTrace::None, Hit, true);
        }

        // Did I see something?
        if (!isHit)
        {
            continue;
        }
        AANephilimBase* TargetHit = Cast<AANephilimBase>(HitResult.GetActor());
        // Is it a Nephilim?
        if (!TargetHit)
        {
            continue;
        }
        // Is it on team "Player"?
        if (TargetHit->Team != ETeams::Player)
        {
            continue;
        }

        // Do I have a target already?
        if (Target == -1)
        {
            Target = i;
        }
        else
        {
            // Is the new entry closer than the previous?
            if (SphereHits[i].Distance < SphereHits[Target].Distance)
            {
                Target = i;
            }
        }
    }
    return Cast<AANephilimBase>(SphereHits[Target].GetActor());
}

void AEnemyBase::ActivateAI_Implementation(bool Activate)
{

}
