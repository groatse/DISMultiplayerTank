#include "Core/PlayerControllerDISTanks.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Tanks/TankDISTanks.h"

void APlayerControllerDISTanks::SetupInputComponent()
{
	Super::SetupInputComponent();

	BuildInputMappings();

	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		InputSubsystem->AddMappingContext(TankMappingContext, 0);
	}

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent))
	{
		// Completed bindings deliver the zeroed value when the key is released.
		EnhancedInput->BindAction(ThrustAction, ETriggerEvent::Triggered, this, &APlayerControllerDISTanks::OnThrustInput);
		EnhancedInput->BindAction(ThrustAction, ETriggerEvent::Completed, this, &APlayerControllerDISTanks::OnThrustInput);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Triggered, this, &APlayerControllerDISTanks::OnTurnInput);
		EnhancedInput->BindAction(TurnAction, ETriggerEvent::Completed, this, &APlayerControllerDISTanks::OnTurnInput);
		EnhancedInput->BindAction(FireAction, ETriggerEvent::Started, this, &APlayerControllerDISTanks::OnFireInput);
	}
}

void APlayerControllerDISTanks::BuildInputMappings()
{
	TankMappingContext = NewObject<UInputMappingContext>(this, TEXT("TankMappingContext"));

	ThrustAction = NewObject<UInputAction>(this, TEXT("ThrustAction"));
	ThrustAction->ValueType = EInputActionValueType::Axis1D;
	MapActionKey(ThrustAction, EKeys::W, false);
	MapActionKey(ThrustAction, EKeys::S, true);
	MapActionKey(ThrustAction, EKeys::Up, false);
	MapActionKey(ThrustAction, EKeys::Down, true);

	TurnAction = NewObject<UInputAction>(this, TEXT("TurnAction"));
	TurnAction->ValueType = EInputActionValueType::Axis1D;
	MapActionKey(TurnAction, EKeys::D, false);
	MapActionKey(TurnAction, EKeys::A, true);
	MapActionKey(TurnAction, EKeys::Right, false);
	MapActionKey(TurnAction, EKeys::Left, true);

	FireAction = NewObject<UInputAction>(this, TEXT("FireAction"));
	FireAction->ValueType = EInputActionValueType::Boolean;
	MapActionKey(FireAction, EKeys::SpaceBar, false);
}

void APlayerControllerDISTanks::MapActionKey(UInputAction* TargetAction, const FKey& Key, bool bNegateValue)
{
	FEnhancedActionKeyMapping& KeyMapping = TankMappingContext->MapKey(TargetAction, Key);
	if (bNegateValue)
	{
		KeyMapping.Modifiers.Add(NewObject<UInputModifierNegate>(this));
	}
}

void APlayerControllerDISTanks::OnThrustInput(const FInputActionValue& ActionValue)
{
	if (ATankDISTanks* ControlledTank = GetControlledTank())
	{
		ControlledTank->SetThrustInput(ActionValue.Get<float>());
	}
}

void APlayerControllerDISTanks::OnTurnInput(const FInputActionValue& ActionValue)
{
	if (ATankDISTanks* ControlledTank = GetControlledTank())
	{
		ControlledTank->SetTurnInput(ActionValue.Get<float>());
	}
}

void APlayerControllerDISTanks::OnFireInput(const FInputActionValue& ActionValue)
{
	if (ATankDISTanks* ControlledTank = GetControlledTank())
	{
		ControlledTank->RequestFire();
	}
}

ATankDISTanks* APlayerControllerDISTanks::GetControlledTank() const
{
	return Cast<ATankDISTanks>(GetPawn());
}
