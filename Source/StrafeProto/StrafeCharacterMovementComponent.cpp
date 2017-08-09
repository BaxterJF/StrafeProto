#include "StrafeCharacterMovementComponent.h"
#include "GameFramework/Character.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPMov, Warning, All);

UStrafeCharacterMovementComponent::UStrafeCharacterMovementComponent(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	//AirControl                       = 1.f;
	//AirControlBoostMultiplier        = 4.f;
	//AirControlBoostVelocityThreshold = 1000.f;

	StrafingMultiplier                 = 1.f;
	bStrafeJumpEnabled                 = true;

	//bDrawJumps                       = true;
}

void UStrafeCharacterMovementComponent::CalcVelocity(float DeltaTime, float Friction, bool bFluid, float BrakingDeceleration)
{
	// Do not update velocity when using root motion or when SimulatedProxy - SimulatedProxy are repped their Velocity
	if (!HasValidData() || HasAnimRootMotion() || DeltaTime < MIN_TICK_TIME || (CharacterOwner && CharacterOwner->Role == ROLE_SimulatedProxy))
	{
		return;
	}

	// We don't want to decelerate the player if he is only 1 frame at the ground.
	if (CharacterOwner->bPressedJump && CharacterOwner->CanJump())
	{
		Friction = 0.0f;
		BrakingDeceleration = 0.0f;
	}

	Friction = FMath::Max(0.f, Friction);
	const float MaxAccel = GetMaxAcceleration();
	float MaxSpeed = GetMaxSpeed();

	// Check if path following requested movement
	bool bZeroRequestedAcceleration = true;
	FVector RequestedAcceleration = FVector::ZeroVector;
	float RequestedSpeed = 0.0f;
	if (ApplyRequestedMove(DeltaTime, MaxAccel, MaxSpeed, Friction, BrakingDeceleration, RequestedAcceleration, RequestedSpeed))
	{
		RequestedAcceleration = RequestedAcceleration.GetClampedToMaxSize(MaxAccel);
		bZeroRequestedAcceleration = false;
	}

	if (bForceMaxAccel)
	{
		// Force acceleration at full speed.
		// In consideration order for direction: Acceleration, then Velocity, then Pawn's rotation.
		if (Acceleration.SizeSquared() > SMALL_NUMBER)
		{
			Acceleration = Acceleration.GetSafeNormal() * MaxAccel;
		}
		else
		{
			Acceleration = MaxAccel * (Velocity.SizeSquared() < SMALL_NUMBER ? UpdatedComponent->GetForwardVector() : Velocity.GetSafeNormal());
		}

		AnalogInputModifier = 1.f;
	}

	// Path following above didn't care about the analog modifier, but we do for everything else below, so get the fully modified value.
	// Use max of requested speed and max speed if we modified the speed in ApplyRequestedMove above.
	MaxSpeed = FMath::Max(RequestedSpeed, MaxSpeed * AnalogInputModifier);

	// Apply braking or deceleration
	const bool bZeroAcceleration = Acceleration.IsZero();
	const bool bVelocityOverMax = IsExceedingMaxSpeed(MaxSpeed);

	// Only apply braking if there is no acceleration, or we are over our max speed and need to slow down to it.
	if ((bZeroAcceleration && bZeroRequestedAcceleration) || bVelocityOverMax)
	{
		const FVector OldVelocity = Velocity;

		const float ActualBrakingFriction = (bUseSeparateBrakingFriction ? BrakingFriction : Friction);
		ApplyVelocityBraking(DeltaTime, ActualBrakingFriction, BrakingDeceleration);

		// Don't allow braking to lower us below max speed if we started above it.
		if (bVelocityOverMax && Velocity.SizeSquared() < FMath::Square(MaxSpeed) && FVector::DotProduct(Acceleration, OldVelocity) > 0.0f)
		{
			Velocity = OldVelocity.GetSafeNormal() * MaxSpeed;
		}
	}
	else if (!bZeroAcceleration)
	{
		// Friction affects our ability to change direction. This is only done for input acceleration, not path following.
		const FVector AccelDir = Acceleration.GetSafeNormal();
		const float VelSize = Velocity.Size();
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * FMath::Min(DeltaTime * Friction, 1.f);
	}

	// Apply fluid friction
	if (bFluid)
	{
		Velocity = Velocity * (1.f - FMath::Min(Friction * DeltaTime, 1.f));
	}


	//@Crafty: This is where the velocity is added in the engine by default
	// Apply acceleration
	/*const float NewMaxSpeed = (IsExceedingMaxSpeed(MaxSpeed)) ? Velocity.Size() : MaxSpeed;
	Velocity += Acceleration * DeltaTime;
	Velocity += RequestedAcceleration * DeltaTime;
	Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);*/

	// Strafe jump
	if (bStrafeJumpEnabled && (MovementMode == MOVE_Falling))
	{
		//FVector NewVelocity = Velocity + Acceleration * DeltaTime;

		//UE_LOG(LogFPMov, Warning, TEXT("CalcVelocity(): Acceleration: {x=%f, y=%f, z=%f"), Acceleration.X, Acceleration.Y, Acceleration.Z);

		FVector CurrentVelocity2D = Velocity.GetSafeNormal2D();
		FVector Acceleration2D = Acceleration.GetSafeNormal2D();

		float Dot = FVector::DotProduct(CurrentVelocity2D, Acceleration2D);

		UE_LOG(LogFPMov, Warning, TEXT("CalcVelocity(): Dot: %f"), Dot);

		Velocity = Velocity + (StrafingMultiplier * Acceleration * FMath::Max<float>(0.5f, 1.f - Dot)) * DeltaTime;

		if (Velocity.Size2D() > CurrentVelocity2D.Size2D())
		{
			Velocity.Normalize();
			Velocity = Velocity * FMath::Max<float>(1.5f, Velocity.Size2D());
		}

		//UE_LOG(LogFPMov, Warning, TEXT("CalcVelocity(): Velocity: {x=%f, y=%f, z=%f"), Velocity.X, Velocity.Y, Velocity.Z);
	}
	else
	{
		// Engine Default
		const float NewMaxSpeed = (IsExceedingMaxSpeed(MaxSpeed)) ? Velocity.Size() : MaxSpeed;
		Velocity += Acceleration * DeltaTime;
		Velocity += RequestedAcceleration * DeltaTime;
		Velocity = Velocity.GetClampedToMaxSize(NewMaxSpeed);
	}

	if (bUseRVOAvoidance)
	{
		CalcAvoidanceVelocity(DeltaTime);
	}
}

void UStrafeCharacterMovementComponent::PhysFalling(float deltaTime, int32 Iterations)
{
	Super::PhysFalling(deltaTime, Iterations);

	//SCOPE_CYCLE_COUNTER(STAT_CharPhysFalling);

	//if (deltaTime < MIN_TICK_TIME)
	//{
	//	return;
	//}

	//FVector FallAcceleration = GetFallingLateralAcceleration(deltaTime);
	//FallAcceleration.Z = 0.f;
	//const bool bHasAirControl = (FallAcceleration.SizeSquared2D() > 0.f);

	//float remainingTime = deltaTime;
	//while ((remainingTime >= MIN_TICK_TIME) && (Iterations < MaxSimulationIterations))
	//{
	//	Iterations++;
	//	const float timeTick = GetSimulationTimeStep(remainingTime, Iterations);
	//	remainingTime -= timeTick;

	//	const FVector OldLocation = UpdatedComponent->GetComponentLocation();
	//	const FQuat PawnRotation = UpdatedComponent->GetComponentQuat();
	//	bJustTeleported = false;

	//	RestorePreAdditiveRootMotionVelocity();

	//	FVector OldVelocity = Velocity;
	//	FVector VelocityNoAirControl = Velocity;

	//	// Apply input
	//	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	//	{
	//		const float MaxDecel = GetMaxBrakingDeceleration();
	//		// Compute VelocityNoAirControl
	//		if (bHasAirControl)
	//		{
	//			// Find velocity *without* acceleration.
	//			TGuardValue<FVector> RestoreAcceleration(Acceleration, FVector::ZeroVector);
	//			TGuardValue<FVector> RestoreVelocity(Velocity, Velocity);
	//			Velocity.Z = 0.f;
	//			CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
	//			VelocityNoAirControl = FVector(Velocity.X, Velocity.Y, OldVelocity.Z);
	//		}

	//		// Compute Velocity
	//		{
	//			// Acceleration = FallAcceleration for CalcVelocity(), but we restore it after using it.
	//			TGuardValue<FVector> RestoreAcceleration(Acceleration, FallAcceleration);
	//			Velocity.Z = 0.f;
	//			CalcVelocity(timeTick, FallingLateralFriction, false, MaxDecel);
	//			Velocity.Z = OldVelocity.Z;
	//		}

	//		// Just copy Velocity to VelocityNoAirControl if they are the same (ie no acceleration).
	//		if (!bHasAirControl)
	//		{
	//			VelocityNoAirControl = Velocity;
	//		}
	//	}

	//	// Apply gravity
	//	const FVector Gravity(0.f, 0.f, GetGravityZ());
	//	Velocity = NewFallVelocity(Velocity, Gravity, timeTick);
	//	VelocityNoAirControl = NewFallVelocity(VelocityNoAirControl, Gravity, timeTick);
	//	const FVector AirControlAccel = (Velocity - VelocityNoAirControl) / timeTick;

	//	ApplyRootMotionToVelocity(timeTick);

	//	if (bNotifyApex && CharacterOwner->Controller && (Velocity.Z <= 0.f))
	//	{
	//		// Just passed jump apex since now going down
	//		bNotifyApex = false;
	//		NotifyJumpApex();
	//	}


	//	// Move
	//	FHitResult Hit(1.f);
	//	FVector Adjusted = 0.5f*(OldVelocity + Velocity) * timeTick;
	//	SafeMoveUpdatedComponent(Adjusted, PawnRotation, true, Hit);

	//	if (!HasValidData())
	//	{
	//		return;
	//	}

	//	float LastMoveTimeSlice = timeTick;
	//	float subTimeTickRemaining = timeTick * (1.f - Hit.Time);

	//	if (IsSwimming()) //just entered water
	//	{
	//		remainingTime += subTimeTickRemaining;
	//		StartSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
	//		return;
	//	}
	//	else if (Hit.bBlockingHit)
	//	{
	//		if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
	//		{
	//			remainingTime += subTimeTickRemaining;
	//			ProcessLanded(Hit, remainingTime, Iterations);
	//			return;
	//		}
	//		else
	//		{
	//			// Compute impact deflection based on final velocity, not integration step.
	//			// This allows us to compute a new velocity from the deflected vector, and ensures the full gravity effect is included in the slide result.
	//			Adjusted = Velocity * timeTick;

	//			// See if we can convert a normally invalid landing spot (based on the hit result) to a usable one.
	//			if (!Hit.bStartPenetrating && ShouldCheckForValidLandingSpot(timeTick, Adjusted, Hit))
	//			{
	//				const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	//				FFindFloorResult FloorResult;
	//				FindFloor(PawnLocation, FloorResult, false);
	//				if (FloorResult.IsWalkableFloor() && IsValidLandingSpot(PawnLocation, FloorResult.HitResult))
	//				{
	//					remainingTime += subTimeTickRemaining;
	//					ProcessLanded(FloorResult.HitResult, remainingTime, Iterations);
	//					return;
	//				}
	//			}

	//			HandleImpact(Hit, LastMoveTimeSlice, Adjusted);

	//			// If we've changed physics mode, abort.
	//			if (!HasValidData() || !IsFalling())
	//			{
	//				return;
	//			}

	//			// Limit air control based on what we hit.
	//			// We moved to the impact point using air control, but may want to deflect from there based on a limited air control acceleration.
	//			if (bHasAirControl)
	//			{
	//				const bool bCheckLandingSpot = false; // we already checked above.
	//				const FVector AirControlDeltaV = LimitAirControl(LastMoveTimeSlice, AirControlAccel, Hit, bCheckLandingSpot) * LastMoveTimeSlice;
	//				Adjusted = (VelocityNoAirControl + AirControlDeltaV) * LastMoveTimeSlice;
	//			}

	//			const FVector OldHitNormal = Hit.Normal;
	//			const FVector OldHitImpactNormal = Hit.ImpactNormal;
	//			FVector Delta = ComputeSlideVector(Adjusted, 1.f - Hit.Time, OldHitNormal, Hit);

	//			// Compute velocity after deflection (only gravity component for RootMotion)
	//			if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
	//			{
	//				const FVector NewVelocity = (Delta / subTimeTickRemaining);
	//				Velocity = HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
	//			}

	//			if (subTimeTickRemaining > KINDA_SMALL_NUMBER && (Delta | Adjusted) > 0.f)
	//			{
	//				// Move in deflected direction.
	//				SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);

	//				if (Hit.bBlockingHit)
	//				{
	//					// hit second wall
	//					LastMoveTimeSlice = subTimeTickRemaining;
	//					subTimeTickRemaining = subTimeTickRemaining * (1.f - Hit.Time);

	//					if (IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit))
	//					{
	//						remainingTime += subTimeTickRemaining;
	//						ProcessLanded(Hit, remainingTime, Iterations);
	//						return;
	//					}

	//					HandleImpact(Hit, LastMoveTimeSlice, Delta);

	//					// If we've changed physics mode, abort.
	//					if (!HasValidData() || !IsFalling())
	//					{
	//						return;
	//					}

	//					// Act as if there was no air control on the last move when computing new deflection.
	//					if (bHasAirControl && Hit.Normal.Z > VERTICAL_SLOPE_NORMAL_Z)
	//					{
	//						const FVector LastMoveNoAirControl = VelocityNoAirControl * LastMoveTimeSlice;
	//						Delta = ComputeSlideVector(LastMoveNoAirControl, 1.f, OldHitNormal, Hit);
	//					}

	//					FVector PreTwoWallDelta = Delta;
	//					TwoWallAdjust(Delta, Hit, OldHitNormal);

	//					// Limit air control, but allow a slide along the second wall.
	//					if (bHasAirControl)
	//					{
	//						const bool bCheckLandingSpot = false; // we already checked above.
	//						const FVector AirControlDeltaV = LimitAirControl(subTimeTickRemaining, AirControlAccel, Hit, bCheckLandingSpot) * subTimeTickRemaining;

	//						// Only allow if not back in to first wall
	//						if (FVector::DotProduct(AirControlDeltaV, OldHitNormal) > 0.f)
	//						{
	//							Delta += (AirControlDeltaV * subTimeTickRemaining);
	//						}
	//					}

	//					// Compute velocity after deflection (only gravity component for RootMotion)
	//					if (subTimeTickRemaining > KINDA_SMALL_NUMBER && !bJustTeleported)
	//					{
	//						const FVector NewVelocity = (Delta / subTimeTickRemaining);
	//						Velocity = HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity() ? FVector(Velocity.X, Velocity.Y, NewVelocity.Z) : NewVelocity;
	//					}

	//					// bDitch=true means that pawn is straddling two slopes, neither of which he can stand on
	//					bool bDitch = ((OldHitImpactNormal.Z > 0.f) && (Hit.ImpactNormal.Z > 0.f) && (FMath::Abs(Delta.Z) <= KINDA_SMALL_NUMBER) && ((Hit.ImpactNormal | OldHitImpactNormal) < 0.f));
	//					SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
	//					if (Hit.Time == 0.f)
	//					{
	//						// if we are stuck then try to side step
	//						FVector SideDelta = (OldHitNormal + Hit.ImpactNormal).GetSafeNormal2D();
	//						if (SideDelta.IsNearlyZero())
	//						{
	//							SideDelta = FVector(OldHitNormal.Y, -OldHitNormal.X, 0).GetSafeNormal();
	//						}
	//						SafeMoveUpdatedComponent(SideDelta, PawnRotation, true, Hit);
	//					}

	//					if (bDitch || IsValidLandingSpot(UpdatedComponent->GetComponentLocation(), Hit) || Hit.Time == 0.f)
	//					{
	//						remainingTime = 0.f;
	//						ProcessLanded(Hit, remainingTime, Iterations);
	//						return;
	//					}
	//					else if (GetPerchRadiusThreshold() > 0.f && Hit.Time == 1.f && OldHitImpactNormal.Z >= WalkableFloorZ)
	//					{
	//						// We might be in a virtual 'ditch' within our perch radius. This is rare.
	//						const FVector PawnLocation = UpdatedComponent->GetComponentLocation();
	//						const float ZMovedDist = FMath::Abs(PawnLocation.Z - OldLocation.Z);
	//						const float MovedDist2DSq = (PawnLocation - OldLocation).SizeSquared2D();
	//						if (ZMovedDist <= 0.2f * timeTick && MovedDist2DSq <= 4.f * timeTick)
	//						{
	//							Velocity.X += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
	//							Velocity.Y += 0.25f * GetMaxSpeed() * (FMath::FRand() - 0.5f);
	//							Velocity.Z = FMath::Max<float>(JumpZVelocity * 0.25f, 1.f);
	//							Delta = Velocity * timeTick;
	//							SafeMoveUpdatedComponent(Delta, PawnRotation, true, Hit);
	//						}
	//					}
	//				}
	//			}
	//		}
	//	}

	//	if (Velocity.SizeSquared2D() <= KINDA_SMALL_NUMBER * 10.f)
	//	{
	//		Velocity.X = 0.f;
	//		Velocity.Y = 0.f;
	//	}

	//	if (bDrawJumps)
	//	{
	//		DrawDebugLine(GetWorld(), OldLocation, CharacterOwner->GetActorLocation(), FColor::Green, true);// , 0.f, 0, 8.f);
	//	}
	//}
}