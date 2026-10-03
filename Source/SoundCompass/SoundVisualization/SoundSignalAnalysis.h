#pragma once

// Standard C++ only: the same analysis runs in Unreal and in the standalone tests.
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

namespace SoundCompassSignal
{
constexpr int WindowSize = 2048;
constexpr int WaveformSize = 256;
constexpr int BandCount = 32;
constexpr double Pi = 3.14159265358979323846;
constexpr float SilenceDb = -80.0f;

inline float Decibels(double Amplitude)
{
    return static_cast<float>(std::max(static_cast<double>(SilenceDb),
        20.0 * std::log10(std::max(Amplitude, 0.00000001))));
}

struct Analysis
{
    std::array<float, WaveformSize> Waveform{};
    std::array<float, BandCount> SpectrumDb{};
    std::array<float, BandCount> FrequenciesHz{};
    float Rms = 0.0f;
    float Peak = 0.0f;
    float RmsDb = SilenceDb;
    float DominantFrequencyHz = 0.0f;
    bool Valid = false;

    Analysis() { SpectrumDb.fill(SilenceDb); }
};

inline void Fft(std::array<std::complex<double>, WindowSize>& Values)
{
    for (int i = 1, j = 0; i < WindowSize; ++i)
    {
        int bit = WindowSize >> 1;
        for (; j & bit; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(Values[i], Values[j]); }
    }
    for (int length = 2; length <= WindowSize; length <<= 1)
    {
        const auto step = std::polar(1.0, -2.0 * Pi / length);
        for (int offset = 0; offset < WindowSize; offset += length)
        {
            std::complex<double> weight(1.0, 0.0);
            for (int i = 0; i < length / 2; ++i)
            {
                const auto a = Values[offset + i];
                const auto b = weight * Values[offset + i + length / 2];
                Values[offset + i] = a + b;
                Values[offset + i + length / 2] = a - b;
                weight *= step;
            }
        }
    }
}

// Interleaved PCM. Analyse channels independently so opposite-phase stereo
// and sounds panned entirely to the right do not disappear through downmixing.
inline Analysis Analyze(const float* Samples, int Frames, int Channels, int SampleRate)
{
    Analysis Out;
    if (!Samples || Frames != WindowSize || Channels < 1 || Channels > 8 || SampleRate < 1000)
    {
        return Out;
    }
    double sumSquares = 0.0;
    double strongestEnergy = -1.0;
    int strongestChannel = 0;
    std::array<double, WindowSize / 2 + 1> binPower{};
    std::array<std::complex<double>, WindowSize> values;
    double windowSum = 0.0;
    std::array<double, WindowSize> window;
    for (int i = 0; i < WindowSize; ++i)
    {
        window[i] = 0.5 - 0.5 * std::cos(2.0 * Pi * i / (WindowSize - 1));
        windowSum += window[i];
    }
    for (int channel = 0; channel < Channels; ++channel)
    {
        double mean = 0.0;
        double energy = 0.0;
        for (int i = 0; i < WindowSize; ++i)
        {
            const float sample = Samples[i * Channels + channel];
            if (!std::isfinite(sample)) { return Analysis(); }
            mean += sample;
            energy += sample * static_cast<double>(sample);
            Out.Peak = std::max(Out.Peak, std::abs(sample));
        }
        sumSquares += energy;
        if (energy > strongestEnergy) { strongestEnergy = energy; strongestChannel = channel; }
        mean /= WindowSize;
        for (int i = 0; i < WindowSize; ++i)
        {
            values[i] = (Samples[i * Channels + channel] - mean) * window[i];
        }
        Fft(values);
        for (int i = 1; i <= WindowSize / 2; ++i)
        {
            const double scale = i == WindowSize / 2 ? 1.0 : 2.0;
            const double amplitude = std::abs(values[i]) * scale / windowSum;
            binPower[i] += amplitude * amplitude / Channels;
        }
    }
    Out.Rms = static_cast<float>(std::sqrt(sumSquares / (Frames * Channels)));
    Out.RmsDb = Decibels(Out.Rms);

    // Peak-preserving reduction of the strongest channel, in chronological order.
    for (int point = 0; point < WaveformSize; ++point)
    {
        float peak = 0.0f;
        for (int i = point * WindowSize / WaveformSize; i < (point + 1) * WindowSize / WaveformSize; ++i)
        {
            const float sample = Samples[i * Channels + strongestChannel];
            if (std::abs(sample) > std::abs(peak)) { peak = sample; }
        }
        Out.Waveform[point] = peak;
    }

    const double nyquist = SampleRate * 0.5;
    const double minHz = 40.0;
    const double maxHz = std::min(16000.0, nyquist);
    for (int band = 0; band < BandCount; ++band)
    {
        const double low = minHz * std::pow(maxHz / minHz, band / static_cast<double>(BandCount));
        const double high = minHz * std::pow(maxHz / minHz, (band + 1) / static_cast<double>(BandCount));
        Out.FrequenciesHz[band] = static_cast<float>(std::sqrt(low * high));
        const int first = std::max(1, static_cast<int>(std::ceil(low * WindowSize / SampleRate)));
        const int last = std::min(WindowSize / 2, static_cast<int>(std::floor(high * WindowSize / SampleRate)));
        double power = 0.0;
        for (int i = first; i <= last; ++i) { power = std::max(power, binPower[i]); }
        Out.SpectrumDb[band] = Decibels(std::sqrt(power));
    }
    const auto best = std::max_element(binPower.begin() + 1, binPower.end());
    if (*best > 0.00000001)
    {
        Out.DominantFrequencyHz = static_cast<float>(std::distance(binPower.begin(), best) *
            static_cast<double>(SampleRate) / WindowSize);
    }
    Out.Valid = true;
    return Out;
}

struct Bearing
{
    double Degrees = 0.0;
    double DistanceMeters = 0.0;
    double ElevationMeters = 0.0;
};

inline Bearing CalculateBearing(double Forward, double Right, double Up)
{
    return {std::atan2(Right, Forward) * 180.0 / Pi,
        std::sqrt(Forward * Forward + Right * Right + Up * Up) / 100.0, Up / 100.0};
}
}
