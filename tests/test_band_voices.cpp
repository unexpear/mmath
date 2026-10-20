// Band pro-check voices: double bass pizzicato, the per-key piano, the
// bowed cello voice and the soft flute voicing.
#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>
#include <complex>
#include <span>
#include <vector>

#include <mmathen/instruments/double_bass.h>
#include <mmathen/instruments/double_bass_pizz.h>
#include <mmathen/instruments/piano_voice.h>
#include <mmathen/instruments/cello_voice.h>
#include <mmathen/instruments/wind_presets.h>

using namespace mmathen;

namespace {
constexpr double kBandFs = 48000.0;

// Hann-windowed DFT magnitude at hz over [begin, begin + n).
double bandAmplitude(const std::vector<double>& x, std::size_t begin, std::size_t n, double hz) {
  std::complex<double> sum{};
  for (std::size_t i = 0; i < n && begin + i < x.size(); ++i) {
    const double w = 0.5 - 0.5 * std::cos(2.0 * constants::pi * i / double(n - 1));
    sum += w * x[begin + i] * std::polar(1.0, -2.0 * constants::pi * hz * i / kBandFs);
  }
  return std::abs(sum);
}
// Strongest frequency in [lo, hi] at 0.05 Hz steps.
double bandPeak(const std::vector<double>& x, std::size_t begin, std::size_t n, double lo, double hi) {
  double best = lo, bestA = -1.0;
  for (double f = lo; f <= hi; f += 0.05) {
    const double a = bandAmplitude(x, begin, n, f);
    if (a > bestA) { bestA = a; best = f; }
  }
  return best;
}
}  // namespace

TEST_CASE("piano keys interpolate Table III and keep its rows", "[band][piano]") {
  for (const PianoStringSpec& row : chabassier2013Strings()) {
    const int midi = 60 + row.semitonesFromC4;
    auto key = pianoStringForKey(midi);
    REQUIRE(key.ok());
    CHECK(std::fabs(key.value().length.value() - row.length.value()) < 1e-12);
    CHECK(std::fabs(key.value().diameter.value() - row.diameter.value()) < 1e-15);
    CHECK(std::fabs(key.value().densityFactor - row.densityFactor) < 1e-12);
    // Tension from equal temperament lands within 3 % of the printed T0
    // (the printed f0 are not equal-tempered exactly).
    CHECK(std::fabs(key.value().tension.value() / row.tension.value() - 1.0) < 0.03);
  }
  for (int midi = 21; midi <= 108; ++midi) {
    auto key = pianoStringForKey(midi);
    REQUIRE(key.ok());
    auto course = tunePianoString(key.value());
    REQUIRE(course.ok());
    CHECK(std::fabs(course.value().tuning.frequency.value() -
                    440.0 * std::pow(2.0, (midi - 69) / 12.0)) < 1e-6 * 440.0 * 8);
    if (midi >= 53) CHECK(key.value().plainWire);
  }
  CHECK(!pianoStringForKey(20).ok());
  CHECK(!pianoStringForKey(109).ok());
}

TEST_CASE("piano hammers are Chaigne-Askenfelt at C2 and C4", "[band][piano]") {
  const PianoHammer c4 = pianoHammerForKey(60);
  CHECK(std::fabs(c4.massKg - 2.97e-3) < 1e-12);
  CHECK(std::fabs(c4.stiffness - 4.5e9) < 1.0);
  CHECK(std::fabs(c4.exponent - 2.5) < 1e-12);
  const PianoHammer c2 = pianoHammerForKey(36);
  CHECK(std::fabs(c2.massKg - 4.9e-3) < 1e-12);
  CHECK(std::fabs(c2.exponent - 2.3) < 1e-12);
  CHECK(pianoHammerForKey(96).massKg < c4.massKg);
  CHECK(pianoHammerSpeed(0) < pianoHammerSpeed(5));
  CHECK(std::fabs(pianoUprightFeltScale(60) - 0.03) < 1e-12);
  CHECK(std::fabs(pianoUprightFeltScale(93) - 0.1) < 1e-12);
}

TEST_CASE("the piano board sits at the cited bridge mobility", "[band][piano]") {
  for (int midi : {33, 57, 81}) {
    const PianoBoard board = pianoBoardAtKey(midi);
    REQUIRE(!board.modes.empty());
    REQUIRE(board.modes.size() == board.areas.size());
    const double f0 = 440.0 * std::pow(2.0, (midi - 69) / 12.0);
    const double lo = std::clamp(f0 / 1.5, 100.0, 1100.0), hi = std::clamp(f0 * 1.5, 225.0, 2500.0);
    double mean = 0.0;
    for (int i = 0; i < 64; ++i) {
      const double w = 2.0 * constants::pi * lo * std::pow(hi / lo, i / 63.0);
      std::complex<double> y{};
      for (const BodyModeSpec& m : board.modes) {
        const double w0 = 2.0 * constants::pi * m.resonanceHz;
        y += std::complex<double>(0.0, w) /
             (m.modalMassKg * std::complex<double>(w0 * w0 - w * w, w * w0 / m.qualityFactor));
      }
      mean += std::abs(y) / 64.0;
    }
    CHECK(std::fabs(mean / kPianoBridgeMobility - 1.0) < 1e-9);
    for (std::size_t i = 0; i < board.modes.size(); ++i) {
      CHECK(std::isfinite(board.areas[i]));
      CHECK(board.modes[i].resonanceHz <= kPianoBoardMaxHz);
    }
  }
}

TEST_CASE("a piano key speaks its pitch on a coupled unison", "[band][piano]") {
  auto a4 = PianoNoteVoice::create(69, pianoHammerSpeed(3), SampleRateHz{kBandFs});
  REQUIRE(a4.ok());
  auto voice = std::move(a4).value();
  CHECK(voice.strings() == 3);
  std::vector<double> x(48000, 0.0);
  REQUIRE(voice.renderAdd(x.data(), x.size()).ok());
  for (double v : x) REQUIRE(std::isfinite(v));
  const double heard = bandPeak(x, 4800, 24000, 430.0, 450.0);
  CHECK(std::fabs(heard - 440.0) < 1.5);
  CHECK(bandAmplitude(x, 4800, 9600, 440.0) > bandAmplitude(x, 33600, 9600, 440.0));

  auto c2 = PianoNoteVoice::create(36, pianoHammerSpeed(3), SampleRateHz{kBandFs});
  REQUIRE(c2.ok());
  CHECK(c2.value().strings() == 2);
  // Harder strike, louder note.
  auto soft = PianoNoteVoice::create(60, pianoHammerSpeed(0), SampleRateHz{kBandFs});
  auto loud = PianoNoteVoice::create(60, pianoHammerSpeed(5), SampleRateHz{kBandFs});
  REQUIRE(soft.ok());
  REQUIRE(loud.ok());
  std::vector<double> s(9600, 0.0), l(9600, 0.0);
  auto sv = std::move(soft).value();
  auto lv = std::move(loud).value();
  REQUIRE(sv.renderAdd(s.data(), s.size()).ok());
  REQUIRE(lv.renderAdd(l.data(), l.size()).ok());
  CHECK(bandAmplitude(l, 0, 9600, 261.63) > 2.0 * bandAmplitude(s, 0, 9600, 261.63));
}

TEST_CASE("renderUnison couples strings through one board", "[band][piano]") {
  auto spec = pianoStringForKey(69);
  REQUIRE(spec.ok());
  auto course = tunePianoString(spec.value());
  auto stiff = tunedPianoString(spec.value());
  REQUIRE(course.ok());
  REQUIRE(stiff.ok());
  auto ideal = courseString(course.value());
  REQUIRE(ideal.ok());
  auto loss = lossFromT60(ideal.value().speed, FrequencyHz{440.0}, DurationSeconds{8.0},
                          FrequencyHz{2640.0}, DurationSeconds{2.0});
  REQUIRE(loss.ok());
  const BodyModeSpec mode{440.0, 30.0, 5.0};
  const HammerBodyDrive body{std::span<const BodyModeSpec>(&mode, 1), 0.02, 1.0, 101325.0, 293.15};
  auto make = [&] {
    return HammeredStringVoice::create(course.value(), HammerStrike{0.12, 1e-3, 2.5}, loss.value(),
                                       stiff.value().stiffness, 1e8, 2.5, 0.0, 4, 4,
                                       SampleRateHz{kBandFs}, body);
  };
  auto a = make(), b = make(), c = make(), d = make();
  REQUIRE(a.ok());
  REQUIRE(b.ok());
  REQUIRE(c.ok());
  REQUIRE(d.ok());
  auto one = std::move(a).value();
  std::vector<HammeredStringVoice> other;
  other.push_back(std::move(b).value());
  std::vector<double> coupled(24000), alone(24000), plain(24000);
  REQUIRE(one.renderUnison(std::span<HammeredStringVoice>(other), coupled.data(), FrameCount{24000}).ok());
  auto lone = std::move(c).value();
  REQUIRE(lone.renderBlock(alone.data(), FrameCount{24000}).ok());
  auto viaUnison = std::move(d).value();
  REQUIRE(viaUnison.renderUnison(std::span<HammeredStringVoice>(), plain.data(), FrameCount{24000}).ok());
  for (std::size_t i = 0; i < plain.size(); ++i) REQUIRE(plain[i] == alone[i]);
  // Two strings in phase on one light board drive it harder and drain
  // faster than one: louder early, and the early/late ratio is larger.
  const double ce = bandAmplitude(coupled, 0, 4800, 440.0), cl = bandAmplitude(coupled, 9600, 4800, 440.0);
  const double ae = bandAmplitude(alone, 0, 4800, 440.0), al = bandAmplitude(alone, 9600, 4800, 440.0);
  CHECK(ce > 1.5 * ae);
  CHECK(ce / cl > ae / al);
  CHECK(one.renderUnison(std::span<HammeredStringVoice>(&one, 1), coupled.data(), FrameCount{8}).error() ==
        QuantityError::InvalidArgument);
}

TEST_CASE("double bass pizzicato picks the string and rings its pitch", "[band][bass]") {
  auto bass = doubleBass();
  REQUIRE(bass.ok());
  auto e1 = doubleBassNote(bass.value(), 41.2034);
  REQUIRE(e1.ok());
  CHECK(e1.value().stringIndex == 0);
  CHECK(!e1.value().stopped);
  auto b2 = doubleBassNote(bass.value(), 123.47);
  REQUIRE(b2.ok());
  CHECK(b2.value().stringIndex == 3);
  CHECK(b2.value().stopped);
  CHECK(!doubleBassNote(bass.value(), 30.0).ok());
  auto modes = doubleBassBodyModes();
  CHECK(std::fabs(modes[0].resonanceHz - 65.0) < 1e-12);
  CHECK(std::fabs(modes[1].resonanceHz - 115.0) < 1e-12);
  for (const auto& note : {e1.value(), b2.value()}) {
    auto voice = doubleBassPizzVoice(note, 2e-3, SampleRateHz{kBandFs});
    REQUIRE(voice.ok());
    auto v = std::move(voice).value();
    std::vector<double> x(96000);
    REQUIRE(v.renderBlock(x.data(), FrameCount{x.size()}).ok());
    const double f = note.course.tuning.frequency.value();
    const double heard = bandPeak(x, 9600, 48000, f * 0.97, f * 1.03);
    CHECK(std::fabs(1200.0 * std::log2(heard / f)) < 10.0);
    CHECK(bandAmplitude(x, 4800, 19200, f) > bandAmplitude(x, 72000, 19200, f));
  }
  auto open = doubleBassPizzLoss(e1.value().course, false);
  auto stop = doubleBassPizzLoss(e1.value().course, true);
  REQUIRE(open.ok());
  REQUIRE(stop.ok());
}

TEST_CASE("cello voice bows its string at pitch and radiates through the body", "[band][cello]") {
  auto vc = cello();
  REQUIRE(vc.ok());
  CHECK(celloCourseFor(vc.value(), 65.41) == 0);
  CHECK(celloCourseFor(vc.value(), 130.81) == 1);
  CHECK(celloCourseFor(vc.value(), 261.63) == 3);
  const auto modes = celloBodyModes();
  CHECK(std::fabs(modes[0].hz - 104.0) < 1e-12);  // A0, Bynum and Rossing
  CHECK(std::fabs(modes[4].hz - 226.0) < 1e-12);  // A1
  const double f = 196.0;  // G3 on the D string
  auto note = makeCelloNote(vc.value(), f, 1.5, 0.5, SampleRateHz{kBandFs}, false);
  REQUIRE(note.ok());
  auto v = std::move(note).value();
  std::vector<double> force(96000), p(96000, 0.0);
  REQUIRE(v.renderBlock(force.data(), FrameCount{force.size()}).ok());
  auto made = CelloBody::create(SampleRateHz{kBandFs});
  REQUIRE(made.ok());
  auto body = std::move(made).value();
  REQUIRE(body.processAdd(force.data(), p.data(), p.size()));
  for (double y : p) REQUIRE(std::isfinite(y));
  const double heard = bandPeak(p, 24000, 38400, f * 0.97, f * 1.03);
  CHECK(std::fabs(1200.0 * std::log2(heard / f)) < 12.0);
  // Bowed (sustained) while the bow is on, decaying after it lifts.
  CHECK(bandAmplitude(p, 24000, 19200, f) > 1.5 * bandAmplitude(p, 86400, 9600, f));
}

TEST_CASE("soft flute voicing centres the jet and lowers the pressure", "[band][flute]") {
  const FluteVoicing pp = fluteVoicingFor("pp"), pv = fluteVoicingFor("p"),
                     mf = fluteVoicingFor("mf"), none = fluteVoicingFor(nullptr);
  CHECK(pp.aimFraction < pv.aimFraction);
  CHECK(pv.aimFraction < mf.aimFraction);
  CHECK(pp.pressureScale < pv.pressureScale);
  CHECK(pv.pressureScale < 1.0);
  CHECK(pv.lockedBreathFraction > kFluteLockedBreathFraction);
  CHECK(mf.aimFraction == kFluteJetAimFraction);
  CHECK(none.pressureScale == 1.0);
}
