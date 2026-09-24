#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "HUDDISTanks.generated.h"

/** Canvas-drawn HUD showing the slot-colored score list, round number, and match state text. */
UCLASS()
class DISMULTIPLAYERTANK_API AHUDDISTanks : public AHUD
{
	GENERATED_BODY()

public:
	/** Draws the score row, round number, and waiting text from the peer registry's state. */
	virtual void DrawHUD() override;
};
