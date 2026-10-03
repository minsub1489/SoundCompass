#pragma once

#include "CoreMinimal.h"

// Author-supplied categories, shared by the scoring policy and the team's icon mapping.
// This classifies metadata; it does not infer a sound's identity from its waveform.
inline FName SoundCompassCategory(FName Category)
{
    const FString Name = Category.ToString().ToLower();
    if (Name.Contains(TEXT("gun")) || Name.Contains(TEXT("bullet"))) { return TEXT("Gunshot"); }
    if (Name.Contains(TEXT("explos")) || Name.Contains(TEXT("grenade"))) { return TEXT("Explosion"); }
    if (Name.Contains(TEXT("foot")) || Name.Contains(TEXT("step"))) { return TEXT("Footstep"); }
    if (Name.Contains(TEXT("door"))) { return TEXT("Door"); }
    if (Name.Contains(TEXT("conversation")) || Name.Contains(TEXT("voice"))) { return TEXT("Conversation"); }
    if (Name.Contains(TEXT("siren")) || Name.Contains(TEXT("alarm")) || Name.Contains(TEXT("alert"))) { return TEXT("Alert"); }
    if (Name.Contains(TEXT("ambien")) || Name.Contains(TEXT("music"))) { return TEXT("Ambience"); }
    return TEXT("Noise");
}
