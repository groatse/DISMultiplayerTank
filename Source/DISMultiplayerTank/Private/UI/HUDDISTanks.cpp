#include "UI/HUDDISTanks.h"

#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Networking/PeerRegistryDISTanks.h"

void AHUDDISTanks::DrawHUD()
{
	Super::DrawHUD();

	UGameInstance* GameInstance = GetGameInstance();
	UPeerRegistryDISTanks* Registry = GameInstance ? GameInstance->GetSubsystem<UPeerRegistryDISTanks>() : nullptr;
	if (!Registry || !Canvas)
	{
		return;
	}

	UFont* HudFont = GEngine->GetLargeFont();

	DrawText(FString::Printf(TEXT("ROUND %d"), Registry->GetCurrentRoundNumber()), FLinearColor::White, 20.0f, 20.0f, HudFont, 1.5f);

	// One slot-colored score entry per known player, laid out as a centered row.
	const TArray<int32> ApplicationIDs = Registry->GetAllApplicationIDsBySlot();
	const float EntrySpacing = 170.0f;
	float EntryX = Canvas->SizeX * 0.5f - (ApplicationIDs.Num() - 1) * EntrySpacing * 0.5f - 40.0f;
	for (const int32 ApplicationID : ApplicationIDs)
	{
		const int32 SlotIndex = Registry->GetSlotForApplication(ApplicationID);
		const bool bIsLocal = ApplicationID == Registry->GetLocalApplicationID();
		const FString EntryText = FString::Printf(TEXT("P%d%s %d"), SlotIndex + 1, bIsLocal ? TEXT("*") : TEXT(""), Registry->GetScoreForApplication(ApplicationID));
		DrawText(EntryText, UPeerRegistryDISTanks::GetSlotColor(SlotIndex) * 2.5f, EntryX, 20.0f, HudFont, 2.0f);
		EntryX += EntrySpacing;
	}

	if (!Registry->IsMatchReady())
	{
		const FString WaitingText = TEXT("WAITING FOR PLAYERS...");
		float TextWidth = 0.0f;
		float TextHeight = 0.0f;
		GetTextSize(WaitingText, TextWidth, TextHeight, HudFont, 2.5f);
		DrawText(WaitingText, FLinearColor::White, (Canvas->SizeX - TextWidth) * 0.5f, Canvas->SizeY * 0.4f, HudFont, 2.5f);
	}
}
