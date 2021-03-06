// © 2020 GP Games, Double Trouble Games


#include "OvermindBase.h"
#include "GameMode_NephilimBase.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Components/CapsuleComponent.h"

// Sets default values
AOvermindBase::AOvermindBase() :
	KillEnemyRange(10000.0f), NavData(nullptr), InnerSpawnRadius(3000.0f), OuterSpawnRadius(6000.0f), ExtraEnemies(0), CooldownSpawn(2.0f), StartSpawnDelay(5.0f),
	bVisionLineTrace(false), OvermindEnabled(true)
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;
}

// Called when the game starts or when spawned
void AOvermindBase::BeginPlay()
{
	Super::BeginPlay();
	GameMode = Cast<AGameMode_NephilimBase>(GetWorld()->GetAuthGameMode());
	PlayerArray = GameMode->GetActivePlayers();
	StartEnemyPool();
	LookForManuallyPlacedEnemies();

	// Spawn timer
	GetWorldTimerManager().SetTimer(Timer, this, &AOvermindBase::MakeEnemy, CooldownSpawn, true, StartSpawnDelay);
}

// Called every frame
void AOvermindBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("Overmind Active = %s"), OvermindEnabled ? TEXT("True") : TEXT("False")));
	if (OvermindEnabled)
	{
		GroupLocation();
		EnemyVision();
		EnemyDistanceCheck();
	}
}

void AOvermindBase::SpawnEnemy()
{
	int32 difficulty = PlayerCondition();

	// Safety Check.
	if (NavData && GetWorld())
	{
		// Check if the types of enemies to spawn were set on editor.
		if (EnemyMap.Num() > 0)
		{
			// There is a limit of enemies on the map.
			if (EnemyArray.Num() <= (difficulty + ExtraEnemies) && PlayerArray.Num() > 0)
			{
				FVector PlayerLocation = PlayerArray[0]->GetActorLocation();
				FVector PlayerForward = PlayerArray[0]->GetActorForwardVector();

				// Get a location to spawn inside of a radius.
				FNavLocation SpawnLocation;
				bool bRandomPoint = NavData->GetRandomReachablePointInRadius(PlayerLocation, OuterSpawnRadius, SpawnLocation);

				FHitResult Result;
				FCollisionQueryParams QueryParams;
				QueryParams.AddIgnoredActor(PlayerArray[0]);
				for (int32 i = 0; i < EnemyArray.Num(); ++i)
				{
					if (EnemyArray[i])
					{
						QueryParams.AddIgnoredActor(EnemyArray[i]);
					}
				}

				GetWorld()->LineTraceSingleByObjectType(
					Result, PlayerLocation, SpawnLocation.Location,
					(ECollisionChannel::ECC_WorldDynamic && ECollisionChannel::ECC_WorldStatic), QueryParams);

				// Make sure player cannot see spawn point.
				float dotProduct = Dot3(PlayerForward, SpawnLocation.Location);
				if (dotProduct < 0.0f && Result.bBlockingHit)
				{
					// Make sure that the spawn point is between inner and outer SpawnRadius.
					FVector DistanceToTarget;
					if (DistanceToTarget.Distance(PlayerLocation, SpawnLocation) > InnerSpawnRadius)
					{
						// Spawn parameter to handle collision. If anything is colliding, don't spwan.
						FActorSpawnParameters SpawnParameter;
						SpawnParameter.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;

						// Goes through all types of enemies in the map and make an array out of the different types.
						TArray<TSubclassOf<class AEnemyBase>> TypeOfEnemiesArray;
						EnemyMap.GenerateKeyArray(TypeOfEnemiesArray);

						// Randomize which enemy type will be spawned based on the weight given on BP.
						int32 ClosestIndex = 0;
						float ClosestValue = 100;
						float RandomWeight = FMath::FRand();
						TArray<float> Weights;
						EnemyMap.GenerateValueArray(Weights);
						for (int32 i = 0; i < Weights.Num(); ++i)
						{
							if (abs(Weights[i] - RandomWeight) < abs(ClosestValue - RandomWeight))
							{
								ClosestIndex = i;
								ClosestValue = Weights[i];
							}
						}

						// Spawn the enemy.
						AEnemyBase* NewEnemy = GetWorld()->SpawnActor<AEnemyBase>(TypeOfEnemiesArray[ClosestIndex].Get(), SpawnLocation.Location, FRotator(0, 0, 0), SpawnParameter);
						if (NewEnemy)
						{
							// Attach a controller to enemy.
							//NewEnemy->SpawnDefaultController();

							// Set the Overmind controlling the enemy.
							NewEnemy->Overmind = this;
							EnemyArray.Add(NewEnemy);
						}
					}
				}
			}
		}
		else
		{
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "\nOvermind: Spawn Error (Add a type of enemy on EnemyMap drop down)");
		}
	}
	else
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "\nOvermind: Spawn Error (Maybe NavMesh is null?)");
	}
}

void AOvermindBase::EnemyDistanceCheck()
{
	// Check for dead enemies or too far from player.
	for (int32 i = 0; i < EnemyArray.Num(); ++i)
	{
		if (!EnemyArray[i])
		{
			continue;
		}
		if (!EnemyArray[i]->bIsActive)
		{
			continue;
		}

		float Distance = FVector::Dist(AverageGroupLocation, EnemyArray[i]->GetActorLocation());
		// Is enemy too far and should be despawned?
		if (Distance > KillEnemyRange)
		{
			EnemyKilled(EnemyArray[i]);
		}
		else
		{
			// Make sure there is a target.
			if (!EnemyArray[i]->TargetActor)
			{
				continue;
			}

			Distance = FVector::Dist(EnemyArray[i]->TargetActor->GetActorLocation(), EnemyArray[i]->GetActorLocation());
			// Loose track of target if it is farther than 150% of sight radius.
			if (Distance > EnemyArray[i]->SightRadius * 1.5f)
			{
				EnemyArray[i]->bInCombat = false;
			}
			EnemyArray[i]->DistanceToTarget = Distance;
		}

	}
}

int AOvermindBase::PlayerCondition()
{
	// The higher the difficulty the more enemies will spawn;
	int32 difficulty = 0;
	for (int32 i = 0; i < PlayerArray.Num(); ++i)
	{
		float MaxHP = PlayerArray[i]->GetMaxHealth();
		float HP = PlayerArray[i]->GetHealth();

		float HPPercent = HP / MaxHP;

		if (HPPercent > 0.75f)
		{
			difficulty += 1;
		}
		else if (HPPercent < 0.75f && HPPercent > 0.50f)
		{
			difficulty += 2;
		}
		else if (HPPercent < 0.50f && HPPercent > 0.25f)
		{
			difficulty += 3;
		}
		else
		{
			difficulty += 4;
		}
	}
	return difficulty;
}

void AOvermindBase::EnemyKilled(AEnemyBase* Enemy)
{
	int32 TargetEnemy = EnemyArray.Find(Enemy);
	if (TargetEnemy != INDEX_NONE)
	{
		EnemyArray[TargetEnemy]->SetActorHiddenInGame(true);
		EnemyArray[TargetEnemy]->GetCharacterMovement()->GravityScale = 0;
		EnemyArray[TargetEnemy]->SetActorEnableCollision(false);
		EnemyArray[TargetEnemy]->ActivateAI(false);
		EnemyArray[TargetEnemy]->bIsActive = false;
		EnemyArray[TargetEnemy]->bInCombat = false;
		ActiveEnemies[TargetEnemy] = false;
	}
}

void AOvermindBase::PlaceEnemy(int32 PoolIndex, FVector Location)
{
	EnemyArray[PoolIndex]->SetHealth(EnemyArray[PoolIndex]->GetMaxHealth());
	EnemyArray[PoolIndex]->GetCharacterMovement()->GravityScale = 1;
	EnemyArray[PoolIndex]->SetActorLocation(Location);
	EnemyArray[PoolIndex]->SetActorHiddenInGame(false);
	EnemyArray[PoolIndex]->SetActorEnableCollision(true);
	EnemyArray[PoolIndex]->ActivateAI(true);
	EnemyArray[PoolIndex]->SetMovementEnabled(true);
	EnemyArray[PoolIndex]->bIsActive = true;
	ActiveEnemies[PoolIndex] = true;
}

void AOvermindBase::StartEnemyPool()
{
	// Spawn parameter to handle collision.
	FActorSpawnParameters SpawnParameter;
	SpawnParameter.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AEnemyBase* NewEnemy = nullptr;
	// How many enemy types are there?
	for (int32 i = 0; i < SpawnInfo.Num(); i++)
	{
		// How many of enemies of this type do I need?
		for (int32 j = 0; j < SpawnInfo[i].Quantity; j++)
		{
			// Add enemy to the list.
			NewEnemy = GetWorld()->SpawnActor<AEnemyBase>(SpawnInfo[i].EnemyType, FVector(0, 0, 0), FRotator(0, 0, 0), SpawnParameter);
			if (NewEnemy)
			{
				NewEnemy->Overmind = this;
				NewEnemy->bIsActive = false;
				NewEnemy->SetActorHiddenInGame(true);
				NewEnemy->GetCharacterMovement()->GravityScale = 0;
				NewEnemy->SetActorEnableCollision(false);

				EnemyArray.Add(NewEnemy);
				ActiveEnemies.Add(false);
			}
		}
	}
}

int32 AOvermindBase::FirstAvailableEnemy()
{
	// Randomize which enemy type will be spawned based on the weight given on BP.
	int32 EnemyTypeIndex = RandomEnemyType();
	if (EnemyTypeIndex == -1)
	{
		return -1;
	}

	int32 AnyAvailableOption = -1;
	for (int32 i = 0; i < ActiveEnemies.Num(); ++i)
	{
		// Kinda pointless the bool pool if I need this validity check on EnemyArray[i]...
		if (ActiveEnemies[i] == false && EnemyArray[i])
		{
			AnyAvailableOption = i;
			if (EnemyArray[i]->IsA(SpawnInfo[EnemyTypeIndex].EnemyType))
			{
				return i;
			}
		}
	}
	if (AnyAvailableOption != -1)
	{
		return AnyAvailableOption;
	}
	return -1;
}

void AOvermindBase::MakeEnemy()
{
	// =============== Sanity Checks! ===============

	// I hope this test is useless, but ... do I have a world?
	if (!GetWorld())
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "\nOvermind: Spawn Error (No World, really?????)");
		return;
	}

	// Do I have a NavMesh to work with?
	if (!NavData)
	{
		GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, "\nOvermind: Spawn Error (Maybe NavMesh is null?)");
		return;
	}

	// Does the pool have an available spot?
	int32 PoolIndex = FirstAvailableEnemy();
	if (PoolIndex == -1)
	{
		return;
	}

	// =============== Get a place for the enemy on the map ===============

	int32 PlayerIndex = GetFardestPlayer();
	FVector PlayerLocation = PlayerArray[PlayerIndex]->GetActorLocation();
	FVector PlayerForward = PlayerArray[PlayerIndex]->GetActorForwardVector();

	// Get a location to spawn inside of a radius.
	FNavLocation SpawnLocation;
	bool bRandomPoint = NavData->GetRandomReachablePointInRadius(PlayerLocation, OuterSpawnRadius, SpawnLocation);

	// Make sure that the spawn point is between inner and outer SpawnRadius.
	if (FVector().Distance(PlayerLocation, SpawnLocation) < InnerSpawnRadius)
	{
		return;
	}

	FHitResult Result;
	FCollisionQueryParams QueryParams;

	// What can the enemy not hide behind?
	for (int32 i = 0; i < PlayerArray.Num(); ++i)
	{
		// Not behind players...
		QueryParams.AddIgnoredActor(PlayerArray[i]);
	}
	for (int32 i = 0; i < EnemyArray.Num(); ++i)
	{
		if (EnemyArray[i] && EnemyArray[i]->bIsActive)
		{
			// Not behind other enemies...
			QueryParams.AddIgnoredActor(EnemyArray[i]);
		}
	}

	// Check if there is somewhere I can hide behind...
	GetWorld()->LineTraceSingleByObjectType
	(
		Result, PlayerLocation, SpawnLocation.Location,	(ECollisionChannel::ECC_WorldDynamic && ECollisionChannel::ECC_WorldStatic), QueryParams
	);

	// Make sure player cannot see spawn point.
	FVector DifferenceVec = SpawnLocation.Location - PlayerLocation;
	DifferenceVec.Normalize();
	float dotProduct = AngleBetweenVectors(PlayerForward, DifferenceVec);
	if (dotProduct > 90.0f)
	{
		// Dot product is positive, therefore I am in front of the player ...
		if (!Result.bBlockingHit)
		{
			// There is nothing in front of the spawn point, therefore it is visible ...
			return;
		}
	}

	// Make a point above the location to place the enemy.
	FVector OffsetLocation = SpawnLocation.Location + FVector(0, 0, 500);

	// Check where is the floor before placing the enemy...
	GetWorld()->LineTraceSingleByObjectType
	(
		Result, OffsetLocation, SpawnLocation.Location - FVector(0, 0, 500), (ECollisionChannel::ECC_WorldDynamic && ECollisionChannel::ECC_WorldStatic), QueryParams
	);

	float CapsuleHalfHeight = EnemyArray[PoolIndex]->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();

	PlaceEnemy(PoolIndex, Result.ImpactPoint + FVector(0,0,CapsuleHalfHeight));
}

int32 AOvermindBase::GetFardestPlayer()
{
	int32 Index = 0;
	if (PlayerArray.Num() > 1)
	{
		// Decide which player is fardest from the group ... Lets gank him!
		float FardestDistance = 0;
		float NewDistance;
		for (int32 i = 0; i < PlayerArray.Num(); ++i)
		{
			NewDistance = FVector().Distance(PlayerArray[i]->GetActorLocation(), AverageGroupLocation);
			if (NewDistance > FardestDistance)
			{
				FardestDistance = NewDistance;
				Index = i;
			}
		}
	}
	return Index;
}

int32 AOvermindBase::RandomEnemyType()
{
	// Number of enemies types I can choose from.
	int32 EnemyOptions = SpawnInfo.Num();
	TArray<float> Weights;
	float TotalWeight = 0;
	for (int32 i = 0; i < EnemyOptions; i++)
	{
		TotalWeight += SpawnInfo[i].Weight;
	}
	
	float RandomNum = FMath::FRandRange(0.0f, 100.0f);

	// Add Weights as percentages and compare if the random number is in between percentages, returning the array position of the best candidate.
	for (int32 i = 0; i < EnemyOptions; i++)
	{
		Weights.Add((SpawnInfo[i].Weight * 100) / TotalWeight);
		if (i == 0)
		{
			if ((RandomNum >= 0) && (RandomNum <= Weights[i]))
			{
				return i;
			}
		}
		else if (i > 0 && i < EnemyOptions - 1)
		{
			if ((RandomNum > Weights[i - 1]) && (RandomNum <= Weights[i]))
			{
				return i;
			}
		}
		else if (i == EnemyOptions - 1)
		{
			return i;
		}
	}
	// Making sure at least one thing returns...
	return -1;
}

void AOvermindBase::EnemyVision()
{
	//TArray<int32> MarkedForDelete;
	// Check each enemy that does not have a target and give them a target if possible.
	for (int32 i = 0; i < EnemyArray.Num(); ++i)
	{
		// Check if enemies where destroyed somehow...
		if (!EnemyArray[i])
		{
			//MarkedForDelete.Add(i);
			continue;
		}

		// If this enemy is not active, leave it alone ... LEAVE BRITNEY ALONE!!
		if (!EnemyArray[i]->bIsActive)
		{
			continue;
		}

		// If this enemy is already in combat, leave it alone ...
		if (EnemyArray[i]->bInCombat)
		{
			continue;
		}

		TArray<AANephilimBase*> TargetCandidates;
		TArray<float> PlayerDistance;
		float Distance;

		// Which player is close enough?
		for (int32 j = 0; j < PlayerArray.Num(); ++j)
		{
			// Is this player at sight range?
			FVector PlayerLocation = PlayerArray[j]->GetActorLocation();
			Distance = FVector().Dist(EnemyArray[i]->GetActorLocation(), PlayerLocation);
			if (Distance > EnemyArray[i]->SightRadius)
			{
				continue;
			}

			// Is this player in front of this enemy?
			FVector EnemyForward = EnemyArray[i]->GetActorForwardVector();
			FVector DifferenceVector = EnemyArray[i]->GetActorLocation() - PlayerLocation;
			DifferenceVector.Normalize();
			float Angle = AngleBetweenVectors(EnemyForward, DifferenceVector);
			if (Angle < 90)
			{
				continue;
			}

			// Use line trace to know if something is blocking the vision to this player.
			TArray < TEnumAsByte < EObjectTypeQuery > > ObjectTypes;
			ObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery1); // I hope this is world static
			ObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery2); // I hope this is world dynamic
			ObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery3); // I hope this is pawn
			ObjectTypes.Add(EObjectTypeQuery::ObjectTypeQuery4); // I hope this is phisical body

			// Ignore enemies when tracing.
			TArray < AActor* > ActorsToIgnore;
			for (int32 k = 0; k < EnemyArray.Num(); ++k)
			{
				if (EnemyArray[k])
				{
					ActorsToIgnore.Add(EnemyArray[k]);
				}
			}

			// Did I hit something?	
			bool bHit;
			FHitResult HitResult;
			if (bVisionLineTrace)
			{
				bHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), EnemyArray[i]->GetActorLocation(), PlayerArray[j]->GetActorLocation(), ObjectTypes, false, ActorsToIgnore, EDrawDebugTrace::ForDuration, HitResult, true);
			}
			else
			{
				bHit = UKismetSystemLibrary::LineTraceSingleForObjects(GetWorld(), EnemyArray[i]->GetActorLocation(), PlayerArray[j]->GetActorLocation(), ObjectTypes, false, ActorsToIgnore, EDrawDebugTrace::None, HitResult, true);
			}
			if (!bHit)
			{
				continue;
			}

			// Is it a Nephilim?
			AANephilimBase* TargetHit = Cast<AANephilimBase>(HitResult.GetActor());
			if (!TargetHit)
			{
				continue;
			}

			// Is the target on the "Player" team?
			if (TargetHit->Team != ETeams::Player)
			{
				continue;
			}

			TargetCandidates.Add(PlayerArray[j]);
			PlayerDistance.Add(Distance);
		}

		// Is there someone to target?
		if (TargetCandidates.Num() == 0 || PlayerDistance.Num() == 0)
		{
			// Does this enemy already have a target?
			//if (!EnemyArray[i]->TargetActor)
			//{
			//	continue;
			//}
			//// Is the target close or should the enemy lose track?
			//if (Distance > EnemyArray[i]->SightRadius)
			//{
			//	EnemyArray[i]->bInCombat = false;
			//}
			continue;
		}

		// Which player is closest?
		int32 ClosestIndex = 0;
		for (int32 j = 1; j < PlayerDistance.Num(); ++j)
		{
			if (PlayerDistance[j] < PlayerDistance[ClosestIndex])
			{
				ClosestIndex = j;
			}
		}

		// Set target!
		EnemyArray[i]->TargetActor = TargetCandidates[ClosestIndex];
		EnemyArray[i]->bInCombat = true;
		EnemyArray[i]->GetMovementComponent()->StopMovementImmediately();
	}

	//if (MarkedForDelete.Num() > 0)
	//{
	//	CleanEnemyArray(MarkedForDelete);
	//}
}

void AOvermindBase::GroupLocation()
{
	if (!PlayerArray[0])
	{
		PlayerArray = GameMode->GetActivePlayers();
		//return;
	}
	AverageGroupLocation = PlayerArray[0]->GetActorLocation();;
	if (PlayerArray.Num() > 1)
	{
		// Average position of the group.
		for (int32 i = 1; i < PlayerArray.Num(); ++i)
		{
			AverageGroupLocation += PlayerArray[i]->GetActorLocation();
		}
		AverageGroupLocation /= PlayerArray.Num();
	}
}

void AOvermindBase::AddEnemies(AEnemyBase* _EnemyType, float _Weight)
{
	if (!_EnemyType)
	{
		return;
	}
	FEnemySpawnInfo Info;
	Info.EnemyType = _EnemyType->StaticClass();
	Info.Weight = _Weight;
	Info.Quantity = 1;
	bool FoundInfo = false;

	// Check if this type of enemy type is already on the list.
	for (int32 i = 0; i < SpawnInfo.Num(); ++i)
	{
		if (SpawnInfo[i].EnemyType != _EnemyType->StaticClass())
		{
			continue;
		}
		FoundInfo = true;
		SpawnInfo[i].Quantity++;
		break;
	}

	// If this is a new type, add to the list.
	if (!FoundInfo)
	{
		SpawnInfo.Add(Info);
	}

	// Add enemy to the list.
	if (_EnemyType)
	{
		_EnemyType->Overmind = this;
		_EnemyType->bIsActive = true;

		EnemyArray.Add(_EnemyType);
		ActiveEnemies.Add(true);
	}
}

void AOvermindBase::LookForManuallyPlacedEnemies()
{
	TArray<AActor*> AllEnemies;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), AEnemyBase::StaticClass(), AllEnemies);

	AEnemyBase* Enemy;
	for (int32 i = 0; i < AllEnemies.Num(); ++i)
	{
		Enemy = Cast<AEnemyBase>(AllEnemies[i]);
		if (!Enemy)
		{
			continue;
		}

		// Is this enemy already assigned to an Overmind?
		if (Enemy->Overmind)
		{
			continue;
		}

		// Don't add bosses
		if (Enemy->Tier == EEnemyTiers::Boss)
		{
			continue;
		}

		Enemy->Overmind = this;
		Enemy->bIsActive = true;
		EnemyArray.Add(Enemy);
		ActiveEnemies.Add(true);
	}
}

float AOvermindBase::AngleBetweenVectors(const FVector& A, const FVector& B)
{
	float Magnitude1 = sqrtf((A.X * A.X) + (A.Y * A.Y) + (A.Z * A.Z));
	float Magnitude2 = sqrtf((B.X * B.X) + (B.Y * B.Y) + (B.Z * B.Z));
	float DotProduct = (A.X * B.X) + (A.Y * B.Y) + (A.Z * B.Z);
	float Cossine = DotProduct / (Magnitude1 * Magnitude2);
	float Angle = acosf(Cossine) * 180.0 / PI;

	// ========= Prints for testing results ============
	//FString CosStr = FString::SanitizeFloat(Cossine);
	//GEngine->AddOnScreenDebugMessage(-1, 1.0, FColor::Blue, *CosStr);
	//FString AngleStr = FString::SanitizeFloat(Angle);
	//GEngine->AddOnScreenDebugMessage(-1, 1.0, FColor::Cyan, *AngleStr);

	return Angle;
}

void AOvermindBase::CleanEnemyArray(TArray<int32> Indices)
{
	// Remove bad pointers and resize the array.
	for (int32 i = 0; i < Indices.Num(); ++i)
	{
		EnemyArray.RemoveAt(Indices[i]);
		ActiveEnemies.RemoveAt(Indices[i]);
	}
	EnemyArray.Shrink();
	ActiveEnemies.Shrink();
}

void AOvermindBase::EnableOvermind(bool Enabled)
{
	OvermindEnabled = Enabled;
}