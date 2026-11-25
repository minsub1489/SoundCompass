#if WITH_DEV_AUTOMATION_TESTS

#include "SoundVisualization/SoundOverlaySender.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSoundOverlaySenderTest, "SoundCompass.Overlay.Sender",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSoundOverlaySenderTest::RunTest(const FString& Parameters)
{
    using namespace SoundOverlay;
    TestEqual(TEXT("Conversation"), ToOverlayClass(TEXT("Conversation")), FString(TEXT("Voice")));
    TestEqual(TEXT("Alert"), ToOverlayClass(TEXT("Alert")), FString(TEXT("Alarm")));
    TestEqual(TEXT("Throw"), ToOverlayClass(TEXT("Throw")), FString(TEXT("Throwable")));
    TestEqual(TEXT("Ambience"), ToOverlayClass(TEXT("Ambience")), FString(TEXT("Other")));
    TestEqual(TEXT("Helicopter"), ToOverlayClass(TEXT("Helicopter")), FString(TEXT("Vehicle")));
    TestEqual(TEXT("Jackhammer"), ToOverlayClass(TEXT("Jackhammer")), FString(TEXT("Other")));
    TestEqual(TEXT("Gunshot"), ToOverlayClass(TEXT("Gunshot")), FString(TEXT("Gunshot")));
    TestEqual(TEXT("Dog"), ToOverlayClass(TEXT("Dog")), FString(TEXT("Dog")));
    TestEqual(TEXT("glassbreak"), ToOverlayClass(TEXT("glassbreak")), FString(TEXT("GlassBreak")));

    FSoundCompassSourceFrame Playing;
    Playing.SourceId = 7;
    Playing.Category = TEXT("Conversation");
    Playing.AzimuthDegrees = 45.0f;
    Playing.DistanceMeters = 12.0f;
    Playing.ElevationMeters = 1.0f;
    Playing.EstimatedListenerLevel = 1.7f;
    Playing.bPlaying = true;
    FSoundCompassSourceFrame Finished = Playing;
    Finished.SourceId = 8;
    Finished.bPlaying = false;

    TSharedPtr<FJsonObject> Root;
    const FString Json = BuildMessage({Playing, Finished}, 12.5);
    TestTrue(TEXT("Message is JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) && Root.IsValid());
    if (!Root.IsValid()) { return false; }
    TestEqual(TEXT("v"), Root->GetIntegerField(TEXT("v")), 1);
    TestEqual(TEXT("src"), Root->GetStringField(TEXT("src")), FString(TEXT("ue")));
    TestEqual(TEXT("mode"), Root->GetStringField(TEXT("mode")), FString(TEXT("ground_truth")));
    const TArray<TSharedPtr<FJsonValue>>& Events = Root->GetArrayField(TEXT("events"));
    TestEqual(TEXT("Only playing frames"), Events.Num(), 1);
    if (Events.Num() != 1) { return false; }
    const TSharedPtr<FJsonObject> Event = Events[0]->AsObject();
    TestEqual(TEXT("id"), Event->GetIntegerField(TEXT("id")), 7);
    TestEqual(TEXT("cls"), Event->GetStringField(TEXT("cls")), FString(TEXT("Voice")));
    TestEqual(TEXT("az"), Event->GetNumberField(TEXT("az")), 45.0);
    TestEqual(TEXT("fb"), Event->GetNumberField(TEXT("fb")), 1.0);
    TestEqual(TEXT("dist_m"), Event->GetNumberField(TEXT("dist_m")), 12.0);
    TestEqual(TEXT("dist_bin"), Event->GetStringField(TEXT("dist_bin")), FString(TEXT("mid")));
    TestEqual(TEXT("dist_src"), Event->GetStringField(TEXT("dist_src")), FString(TEXT("ue")));
    TestEqual(TEXT("level clamped"), Event->GetNumberField(TEXT("level")), 1.0);
    return true;
}

#endif
