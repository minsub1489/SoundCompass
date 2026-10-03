#include "../Source/SoundCompass/SoundVisualization/SoundSignalAnalysis.h"
#include "../Source/SoundCompass/SoundVisualization/SoundImportancePolicy.h"
#include <cstdlib>
#include <iostream>
#include <limits>

using namespace SoundCompassSignal;

int checks = 0;
void Check(bool Condition, const char* Message)
{
    ++checks;
    if (!Condition) { std::cerr << "FAILED: " << Message << '\n'; std::exit(1); }
}

std::vector<float> Tone(int Rate, int Channels, double Frequency, float Amplitude)
{
    std::vector<float> Samples(WindowSize * Channels);
    for (int i = 0; i < WindowSize; ++i)
    {
        for (int c = 0; c < Channels; ++c)
        {
            Samples[i * Channels + c] = Amplitude * std::sin(2 * Pi * Frequency * i / Rate);
        }
    }
    return Samples;
}

int main()
{
    auto Samples = Tone(48000, 1, 1500.0, 0.5f); // exactly FFT bin 64
    auto Result = Analyze(Samples.data(), WindowSize, 1, 48000);
    Check(Result.Valid, "48 kHz input valid");
    Check(std::abs(Result.Rms - 0.5 / std::sqrt(2.0)) < 0.001, "sine RMS");
    Check(std::abs(Result.RmsDb - (-9.0309)) < 0.02, "sine dBFS");
    Check(std::abs(Result.DominantFrequencyHz - 1500) < 1, "sine dominant frequency");
    Check(std::abs(*std::max_element(Result.SpectrumDb.begin(), Result.SpectrumDb.end()) + 6.0206) < 0.1,
        "Hann window gain normalisation");

    Samples = Tone(48000, 2, 1500.0, 0.5f);
    for (int i = 0; i < WindowSize; ++i) { Samples[i * 2 + 1] *= -1; }
    Result = Analyze(Samples.data(), WindowSize, 2, 48000);
    Check(std::abs(Result.Rms - 0.353553) < 0.001, "opposite-phase stereo RMS retained");
    Check(std::abs(Result.DominantFrequencyHz - 1500) < 1, "opposite-phase stereo FFT retained");
    Check(Result.Peak > 0.49, "opposite-phase stereo waveform retained");
    for (int i = 0; i < WindowSize; ++i) { Samples[i * 2] = 0; }
    Result = Analyze(Samples.data(), WindowSize, 2, 48000);
    Check(std::abs(Result.Rms - 0.25) < 0.001, "right-only signal RMS across both channels");
    Check(std::abs(Result.DominantFrequencyHz - 1500) < 1, "right-only signal spectrum");
    Check(*std::max_element(Result.Waveform.begin(), Result.Waveform.end()) > 0.49, "right-only waveform");

    Samples = Tone(44100, 1, 1000.0, 0.75f);
    Result = Analyze(Samples.data(), WindowSize, 1, 44100);
    Check(std::abs(Result.DominantFrequencyHz - 1000) <= 44100.0 / WindowSize, "44.1 kHz bin mapping");
    Samples = Tone(8000, 1, 1000.0, 0.25f);
    Result = Analyze(Samples.data(), WindowSize, 1, 8000);
    Check(Result.FrequenciesHz.back() < 4000, "bands remain below Nyquist");
    Check(std::abs(Result.DominantFrequencyHz - 1000) < 1, "8 kHz bin mapping");
    Samples.assign(WindowSize, 0.0f);
    for (int i = 0; i < WindowSize; ++i) { Samples[i] = (i % 2) ? -0.25f : 0.25f; }
    Result = Analyze(Samples.data(), WindowSize, 1, 8000);
    Check(std::abs(Result.Rms - 0.25f) < 0.001, "Nyquist RMS");
    Check(std::abs(Result.SpectrumDb.back() - Decibels(0.25)) < 0.02, "Nyquist amplitude not doubled");

    Samples.assign(WindowSize, 0.0f);
    Result = Analyze(Samples.data(), WindowSize, 1, 48000);
    Check(Result.Valid && Result.Rms == 0 && Result.DominantFrequencyHz == 0, "silence has no signal");
    Check(Result.RmsDb == SilenceDb, "silence finite dB floor");
    Check(std::all_of(Result.SpectrumDb.begin(), Result.SpectrumDb.end(), [](float Db) { return Db == SilenceDb; }),
        "silence empty spectrum");
    Samples.assign(WindowSize, 0.5f);
    Result = Analyze(Samples.data(), WindowSize, 1, 48000);
    Check(Result.DominantFrequencyHz == 0, "DC removed before FFT");
    Samples.assign(WindowSize, 0.0f);
    Samples[17] = 1.0f;
    Result = Analyze(Samples.data(), WindowSize, 1, 48000);
    Check(Result.Waveform[17 * WaveformSize / WindowSize] == 1, "short impulse preserved in waveform");
    Check(std::abs(Result.Rms - 1.0 / std::sqrt(WindowSize)) < 0.00001, "impulse RMS");
    Samples[4] = std::numeric_limits<float>::quiet_NaN();
    Check(!Analyze(Samples.data(), WindowSize, 1, 48000).Valid, "nonfinite samples rejected");
    Check(!Analyze(nullptr, WindowSize, 1, 48000).Valid, "null samples rejected");
    Check(!Analyze(Samples.data(), 100, 1, 48000).Valid, "incomplete window rejected");
    Check(!Analyze(Samples.data(), WindowSize, 9, 48000).Valid, "unsupported channel count rejected");

    Check(CalculateBearing(1000, 0, 0).Degrees == 0, "front bearing");
    Check(CalculateBearing(0, 1000, 0).Degrees == 90, "right bearing");
    Check(CalculateBearing(0, -1000, 0).Degrees == -90, "left bearing");
    Check(std::abs(CalculateBearing(-1000, 0, 0).Degrees) == 180, "rear bearing");
    const auto Bearing = CalculateBearing(300, 400, 1200);
    Check(std::abs(Bearing.DistanceMeters - 13) < 0.001, "3D centimetres to metres");
    Check(Bearing.ElevationMeters == 12, "height units");

    namespace Importance = SoundCompassImportance;
    const float QuietThreat = Importance::Score(0.95f, 15.0f, -55.0f);
    const float LoudAmbience = Importance::Score(0.10f, 0.0f, 0.0f);
    Check(QuietThreat > LoudAmbience, "quiet distant threat outranks loud nearby ambience");
    Check(QuietThreat > 0.50f && LoudAmbience < 0.50f, "semantic threshold preserves threat and rejects ambience");
    Check(Importance::Score(0.75f, 1.0f, -30.0f) > Importance::Score(0.75f, 19.0f, -30.0f),
        "proximity breaks ties between equally important sounds");
    Check(Importance::Score(0.75f, 5.0f, -10.0f) > Importance::Score(0.75f, 5.0f, -50.0f),
        "loudness is a secondary ranking factor");
    Check(std::abs(Importance::Score(1.0f, 0.0f, 0.0f) - 1.0f) < 0.00001f, "maximum score normalised");
    Check(Importance::Score(-2.0f, 500.0f, -80.0f) == 0.0f, "out-of-range inputs clamped");
    Importance::Weights ZeroWeights;
    ZeroWeights.Semantic = ZeroWeights.Proximity = ZeroWeights.Loudness = 0.0f;
    Check(Importance::Score(0.7f, 5.0f, -30.0f, ZeroWeights) == 0.7f, "zero weights fall back to semantic importance");
    Importance::Weights InvalidWeights;
    InvalidWeights.DistanceRangeMeters = 0.0f;
    InvalidWeights.AudibleFloorDb = 0.0f;
    Check(std::isfinite(Importance::Score(0.7f, 5.0f, -30.0f, InvalidWeights)), "invalid config remains finite");
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    Check(std::isfinite(Importance::Score(NaN, NaN, NaN)), "nonfinite scoring inputs remain finite");
    const std::vector<Importance::Candidate> Candidates{{8,0.85f},{7,0.85f},{9,0.5f},{10,0.49f},{11,0.95f},{12,NaN}};
    auto Selected = Importance::Select(Candidates, 0.50f, 3);
    Check(Selected == std::vector<int>({4,1,0}), "Top-K descending importance with deterministic ties");
    Selected = Importance::Select(Candidates, 0.50f, 8);
    Check(Selected.size() == 4 && Selected.back() == 2, "threshold inclusive and no low-priority filler");
    Check(Importance::Select(Candidates, 0.96f, 3).empty(), "no candidate above threshold gives empty visualization");
    Check(Importance::Select({}, 0.5f, 3).empty(), "empty candidate list safe");
    Check(Importance::Select(Candidates, 0.5f, 0).empty(), "zero selection limit safe");
    Check(Importance::Select(Candidates, 0.0f, 100).size() == 5, "all-audible condition retains every finite candidate");
    std::cout << checks << " checks passed\n";
}
