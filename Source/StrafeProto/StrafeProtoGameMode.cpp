// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StrafeProtoGameMode.h"
#include "StrafeProtoHUD.h"
#include "StrafeProtoCharacter.h"
#include "UObject/ConstructorHelpers.h"

AStrafeProtoGameMode::AStrafeProtoGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = AStrafeProtoHUD::StaticClass();
}
