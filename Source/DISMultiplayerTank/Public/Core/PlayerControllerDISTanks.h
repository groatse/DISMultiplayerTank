#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "PlayerControllerDISTanks.generated.h"

class ATankDISTanks;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/** Local player controller that builds its Enhanced Input mappings in code and drives the possessed tank. */
UCLASS()
class DISMULTIPLAYERTANK_API APlayerControllerDISTanks : public APlayerController
{
	GENERATED_BODY()

public:
	/** Creates the runtime input actions and mappings and binds their handlers. */
	virtual void SetupInputComponent() override;

protected:
	/** Builds the mapping context and input actions without any content assets. */
	void BuildInputMappings();

	/** Maps one key to an action, optionally negating the key's contribution. */
	void MapActionKey(UInputAction* TargetAction, const FKey& Key, bool bNegateValue);

	/** Applies thrust axis input to the possessed tank. */
	void OnThrustInput(const FInputActionValue& ActionValue);

	/** Applies turn axis input to the possessed tank. */
	void OnTurnInput(const FInputActionValue& ActionValue);

	/** Requests a shot from the possessed tank. */
	void OnFireInput(const FInputActionValue& ActionValue);

	/** Returns the possessed pawn as a tank, or null when unpossessed. */
	ATankDISTanks* GetControlledTank() const;

	UPROPERTY()
	TObjectPtr<UInputMappingContext> TankMappingContext;

	UPROPERTY()
	TObjectPtr<UInputAction> ThrustAction;

	UPROPERTY()
	TObjectPtr<UInputAction> TurnAction;

	UPROPERTY()
	TObjectPtr<UInputAction> FireAction;
};
