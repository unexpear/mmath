#pragma once

// Snare drum: the 14 x 5.5 in shell of yamahaTourCustomKit(), a batter and
// a snare-side head coupled by the closed cavity (CavityDrumVoice), a
// wooden stick tip, and snare strands colliding with the snare head
// (CavityDrumVoice::attachSnareWires). Constants are labelled at each line.

#include <cmath>
#include <utility>
#include <vector>

#include "cavity_drum.h"
#include "drum.h"
#include "mallet_voice.h"

namespace mmathen {

// Heads: Mylar (PET film) density 1390 kg/m^3 (DuPont Mylar product
// data, VERIFY). Batter: Remo Ambassador, 10 mil single ply (Remo
// catalogue); snare side: Remo Ambassador Snare Side, 3 mil (Remo).
inline constexpr double kMylarDensityKgPerM3 = 1390.0;  // VERIFY
inline constexpr double kSnareBatterThicknessM = 10.0 * 25.4e-6;
inline constexpr double kSnareSideThicknessM = 3.0 * 25.4e-6;
// Tuning: (0,1) of each head before cavity coupling. The snare-side head
// is usually tuned above the batter (Rossing, Science of Percussion
// Instruments, 2000, ch. 2, VERIFY ratio). TUNED to the VSCO 2 CE Snare2
// "HitSN" spectra (head peak there 236-244 Hz).
inline constexpr double kSnareBatterHz = 200.0;  // TUNED: coupled peak 232-236 Hz snares on (ref 236-244), 208 Hz off (ref 220); 175 gave 212 / 192
inline constexpr double kSnareSideHz = 240.0;    // TUNED
inline constexpr double kSnareHeadQ = 42.0;      // TUNED to snares off: t(-40 dB) 262 ms (ref HitNS 282-290); Q 120 rang 840 ms
// Stick: a 5A hickory stick weighs about 50 g; the moving mass at the tip
// is taken as 15 g (VERIFY). Wood tip on film: stiff, short contact.
inline constexpr double kSnareStickMassKg = 0.015;       // VERIFY
inline constexpr double kSnareStickStiffness = 2.5e5;    // N/m^1.5, TUNED (4e6 put a click 20 dB over the buzz; 5e5 -> 2.5e5 lowered the 300 ms crest 27 -> 22.5 dB at 3 m/s)
inline constexpr double kSnareStickExponent = 1.5;       // Hertz contact
inline constexpr double kSnareStrikeRadialFraction = 0.3;  // off-centre stroke, UNSOURCED

// Strands: a standard set has 20 strands (Puresound / Remo 20-strand
// 14 in sets). Each strand is a distributed string in one-sided contact
// with the snare head (S. Bilbao, JASA 131, 914, 2012; A. Torin & M.
// Newton, "Collisions in drum membranes: a preliminary study on a
// simplified system", ISMA 2014, 401-406). String values from Torin &
// Newton Table 1: steel 7800 kg/m^3, cross-section radius 0.18-0.29 mm
// (0.235 mm taken), tension 0.2-2 N (1.1 N taken), loss sigma0 2 1/s and
// sigma1 0.001 m^2/s (read as mode decay sigma0 + sigma1 beta^2, VERIFY
// the paper's factor convention). Until step 9 each strand was a lumped
// 0.3 g mass on a Q 8 resonator (TUNED): all strands slapped the head at
// once (crest over the first 10 ms 8.7 dB at 3 m/s, 11.6 at 6, against
// 5.7-6.4 in the VSCO Snare2 HitSN samples) and the Q 8 resonators drained
// the head (t(-20 dB) 25-32 ms, reference 63-100 ms). Contact stiffness
// and preload remain lumped-fit values (Torin & Newton do not give one
// set for a full snare); contactStiffness is the whole strand's, shared
// over its contact points.
inline constexpr double kSnareStrandRadiusM = 0.235e-3;     // Torin & Newton 2014 Table 1 (0.18-0.29 mm), middle
inline constexpr double kSnareStrandTensionN = 1.1;         // Torin & Newton 2014 Table 1 (0.2-2 N), middle
inline constexpr double kSnareStrandDensityKgPerM3 = 7800.0;  // steel, Torin & Newton 2014 Table 1
inline constexpr double kSnareStrandLoss0 = 2.0;            // 1/s, Torin & Newton 2014 Table 1
inline constexpr double kSnareStrandLoss1 = 0.001;          // m^2/s, Torin & Newton 2014 Table 1
// String modes and contact points per strand: numerical. 16 / 16 agrees
// with 24 / 24 and 32 / 32 within the run-to-run scatter of the buzz
// (3 m/s: t(-20 dB) 99-108 ms, crest 10 ms 6.0-6.1 dB, late >2 kHz
// -11.3..-12.8 dB); 32 modes on 16 points under-resolved the strings.
inline constexpr int kSnareStrandModes = 16;
inline constexpr int kSnareStrandContacts = 16;
[[nodiscard]] inline SnareWires snareWireSet() {
  SnareWires w;
  w.count = 20;
  w.stringModes = kSnareStrandModes;
  w.contactsPerStrand = kSnareStrandContacts;
  w.stringTensionN = kSnareStrandTensionN;
  w.stringLinearDensityKgPerM = kSnareStrandDensityKgPerM3 * constants::pi * kSnareStrandRadiusM * kSnareStrandRadiusM;
  w.stringLoss0PerSecond = kSnareStrandLoss0;
  w.stringLoss1M2PerSecond = kSnareStrandLoss1;
  w.contactStiffness = 3.0e6;      // TUNED on the distributed strands (2e6: late >2 kHz -12.6 dB; 5e6: crest 10 ms 7.7 dB; 2e7, the lumped value: 11.8 dB)
  w.contactExponent = 1.5;         // Hertz
  w.contactDampingPerMeterPerSecond = 0.5;  // TUNED (lumped fit, kept)
  w.preloadMeters = 5.0e-7;        // TUNED (lumped fit, kept; 2e-5 kept the strands stuck: no buzz)
  w.spanFraction = 0.6;
  return w;
}

// The strand contacts are stiff and stepped explicitly against the head;
// at 48 kHz a 2e6 N/m^1.5 contact self-oscillated (constant level after
// the hit). The voice runs at kSnareOversample times the output rate and
// averages each block of samples (boxcar decimation, as
// OversampledBowedStringVoice). Convergence (3 m/s, snares on): 8x left
// late >2 kHz buzz 2-20 dB above 16x / 24x / 48x (numerical chatter);
// 16x agrees with 48x within 1 dB on every band and 5 ms on t(-20 dB).
// With the distributed strands (step 9) 8x, 16x and 32x agree within
// 0.2 dB and 10 ms.
inline constexpr int kSnareOversample = 16;  // convergence, see above
class SnareDrumVoice final : public IVoice {
 public:
  SnareDrumVoice(CavityDrumVoice inner, SampleRateHz rate, int factor)
      : inner_(std::move(inner)), rate_(rate), factor_(factor) {}
  [[nodiscard]] SampleRateHz sampleRate() const override { return rate_; }
  [[nodiscard]] Result<FrameCount, QuantityError> renderBlock(double* out, FrameCount n) override {
    using R = Result<FrameCount, QuantityError>;
    const auto count = static_cast<std::size_t>(n.value());
    if (count == 0) return R::success(n);
    if (out == nullptr) return R::failure(QuantityError::InvalidArgument);
    buffer_.resize(count * static_cast<std::size_t>(factor_));
    auto done = inner_.renderBlock(buffer_.data(), FrameCount{buffer_.size()});
    if (!done.ok()) return R::failure(done.error());
    for (std::size_t i = 0; i < count; ++i) {
      double sum = 0.0;
      for (int k = 0; k < factor_; ++k) sum += buffer_[i * static_cast<std::size_t>(factor_) + static_cast<std::size_t>(k)];
      out[i] = sum / factor_;
    }
    return R::success(n);
  }
  void reset() override { inner_.reset(); }
  [[nodiscard]] const CavityDrumVoice& inner() const noexcept { return inner_; }

 private:
  CavityDrumVoice inner_;
  SampleRateHz rate_;
  int factor_ = 1;
  std::vector<double> buffer_;
};

// Head tuning and stick values snareDrumHit uses; the defaults are the
// constants above (a caller may retune a drum without editing them).
struct SnareDrumTuning {
  double batterHz = kSnareBatterHz;
  double sideHz = kSnareSideHz;
  double headQ = kSnareHeadQ;
  double stickStiffness = kSnareStickStiffness;
};

// One snare hit. speed: stick speed in m/s (about 1 pp to 6 ff, VERIFY).
[[nodiscard]] inline Result<SnareDrumVoice, QuantityError> snareDrumHit(
    double speed, bool snaresOn, SampleRateHz rate, double distanceMeters = 1.0,
    const SnareWires* wires = nullptr, const SnareDrumTuning& tune = SnareDrumTuning{}) {
  using R = Result<SnareDrumVoice, QuantityError>;
  auto kit = yamahaTourCustomKit();
  if (!kit.ok()) return R::failure(kit.error());
  const MembraneShell& shell = kit.value().snare55;
  const double sb = kMylarDensityKgPerM3 * kSnareBatterThicknessM;
  const double ss = kMylarDensityKgPerM3 * kSnareSideThicknessM;
  auto tension = [&](double sigma, double hz) {
    const double alpha = 2.404825557695773;
    const double wave = constants::twoPi * shell.headRadius.value() * hz / alpha;
    return sigma * wave * wave;
  };
  const double tb = tension(sb, tune.batterHz), ts = tension(ss, tune.sideHz);
  auto batter = tuneMembraneHead(shell, tune.batterHz, sb * 0.999, sb * 1.001, tb * 0.9, tb * 1.1);
  auto side = tuneMembraneHead(shell, tune.sideHz, ss * 0.999, ss * 1.001, ts * 0.9, ts * 1.1);
  if (!batter.ok()) return R::failure(batter.error());
  if (!side.ok()) return R::failure(side.error());
  const MalletStrike stick{kSnareStrikeRadialFraction, 0.0, kSnareStickMassKg, speed,
                           tune.stickStiffness, kSnareStickExponent, 0.0, tune.headQ};
  auto voice = CavityDrumVoice::create(batter.value(), &side.value(), shell, stick, 4, 4,
                                       SampleRateHz{rate.value() * kSnareOversample},
                                       101325.0, 293.15, distanceMeters);
  if (!voice.ok()) return R::failure(voice.error());
  auto v = std::move(voice).value();
  if (snaresOn) {
    if (!v.attachSnareWires(wires != nullptr ? *wires : snareWireSet())) return R::failure(QuantityError::InvalidArgument);
  } else {
    v.enableLocalRadiation();
  }
  return R::success(SnareDrumVoice{std::move(v), rate, kSnareOversample});
}

}  // namespace mmathen
