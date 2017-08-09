#pragma once

#include "GameFramework/CharacterMovementComponent.h"

#include "StrafeCharacterMovementComponent.generated.h"

UCLASS()
class UStrafeCharacterMovementComponent: public UCharacterMovementComponent
{
	GENERATED_UCLASS_BODY()

public:
	virtual void CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration) override;
	virtual void PhysFalling(float DeltaTime, int32 Iterations) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bStrafeJumpEnabled;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float StrafingMultiplier;

	/** Debugging jump visualization. */
	UPROPERTY()
	bool bDrawJumps;
};