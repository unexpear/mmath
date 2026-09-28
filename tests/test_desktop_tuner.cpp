#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "../app/tuner_controls.h"
#include "../app/streaming_tuner.h"
#include "../app/analysis_profiles.h"
#include "../app/precision_tuner.h"
#include "../app/tuner_calibration.h"
#include "../app/harmonic_chords.h"
#include "../app/guitar_chords.h"

TEST_CASE("Desktop tuner windows retain low notes across source rates", "[desktop-tuner]") {
  for (double rate : {8000., 16000., 44100., 48000., 96000., 192000.}) {
    const auto count = app::tunerFrameSize(rate);
    std::vector<double> frame(count);
    for (std::size_t i=0; i<count; ++i)
      frame[i] = .3 * std::sin(2 * std::numbers::pi * 55 * static_cast<double>(i) / rate);
    app::StreamingTuner detector;
    const auto p = detector.process(frame, rate);
    REQUIRE(p.voiced);
    CHECK(std::abs(1200 * std::log2(p.frequencyHz / 55)) < 5);
    CHECK_FALSE(detector.process(std::vector<double>(count), rate).voiced);
  }
}

TEST_CASE("Desktop tuner targets do not silently correct detector errors", "[desktop-tuner]") {
  using namespace app::tuner;
  const double e2 = frequency(40);
  const auto raw = read(e2 * 2, true, 0, 0, 440, false);
  CHECK(raw.deviation == Catch::Approx(1200));
  CHECK(raw.harmonic == 1);
  const auto selected = read(e2 * 2, true, 0, 0, 440, true);
  CHECK(selected.rawHz == raw.rawHz);
  CHECK(selected.hz == Catch::Approx(e2));
  CHECK(selected.harmonic == 2);
  const auto automatic = read(e2 * 2, true, 0, -1, 440, true);
  CHECK(automatic.harmonic == 1);
  CHECK(automatic.hz == raw.hz);
  CHECK(read(e2 * std::pow(2, 60.0 / 1200) * 2, true, 0, 0, 440, true).harmonic == 1);
}

TEST_CASE("Desktop calibration and sharp flat guidance agree", "[desktop-tuner]") {
  using namespace app::tuner;
  CHECK(read(442, true, -1, -1, 442, false).deviation == Catch::Approx(0).margin(1e-9));
  CHECK(read(440, true, -1, -1, 442, false).deviation < 0);
  CHECK(read(445, true, -1, -1, 442, false).deviation > 0);
  CHECK(read(frequency(38), true, 1, 0, 440, false).deviation == Catch::Approx(0).margin(1e-9));
  CHECK_FALSE(read(440, false, -1, -1, 440, false).valid);
  for (double hz : {0.0, -1.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
    const auto r = read(hz, true, 0, 0, 440, true);
    CHECK_FALSE(r.valid);
    CHECK(std::string(verdict(r, 3)) != "In tune");
  }
  CHECK_FALSE(read(440, true, -1, -1, 0, false).valid);
}

TEST_CASE("Imported chord fingerings retain string notes and enharmonics", "[desktop-tuner]") {
  using namespace app::tuner;
  REQUIRE(std::size(shapes) == 42);
  REQUIRE(shapeForState(4));
  CHECK(shapeNotes(*shapeForState(4)) == "E2  B2  E3  G#3  B3  E4");
  REQUIRE(shapeForState(10));
  CHECK(std::string(shapeForState(10)->name) == "Bb");
  CHECK(shapeForState(24) == nullptr);
  CHECK(shapeForState(-1) == nullptr);
  for (const auto& s : shapes) for (int fret : s.frets)
    if (fret > 0) CHECK(fret >= firstFret(s));
}

TEST_CASE("Shared streaming tuner preserves D35 rolling decode", "[desktop-tuner]") {
  using namespace mmathen;
  app::StreamingTuner shared;
  std::vector<std::vector<PitchCandidate>> candidates;
  std::vector<double> times;
  PyinOptions po; po.fMinHz = 50; po.fMaxHz = 1500; po.maxJumpBins = 60;
  // Cross the 24-frame history boundary and include a note transition and silence.
  for (int tick = 1; tick <= 27; ++tick) {
    std::vector<double> frame(4096);
    const double hz = tick < 12 ? 110 : 220;
    for (std::size_t i = 0; i < frame.size(); ++i)
      frame[i] = tick > 25 ? 0 : .3 * std::sin(2 * 3.141592653589793 * hz * static_cast<double>(i) / 44100);
    auto c = pyinCandidates(frame, SampleRateHz{44100}, po);
    REQUIRE(c.ok());
    candidates.push_back(c.value()); times.push_back(static_cast<double>(tick) / 15);
    if (candidates.size() > 24) { candidates.erase(candidates.begin()); times.erase(times.begin()); }
    auto reference = pyinDecode(candidates, times, SampleRateHz{44100}, po);
    REQUIRE(reference.ok());
    const auto actual = shared.process(frame, 44100);
    CHECK(actual.voiced == reference.value().back().estimate.voiced);
    CHECK(actual.frequencyHz == reference.value().back().estimate.frequencyHz);
  }
  shared.reset();
  app::StreamingTuner fresh;
  std::vector<double> silence(4096);
  CHECK(shared.process(silence, 44100).frequencyHz == fresh.process(silence, 44100).frequencyHz);
}

TEST_CASE("Microphone clock correction has the physical sign and never alters files", "[desktop-tuner]") {
  app::tuner::Calibration c;
  for (double ppm : {-500., -20., 20., 500.}) {
    c.reset(); REQUIRE(c.start(440));
    // A faster ADC clock records fewer samples per true period; nominal-rate
    // analysis reads low. This is an arithmetic contract, not hardware evidence.
    const double observed=440/(1+ppm*1e-6);
    for(int i=0;i<45;++i) c.observe(observed,true,-20);
    REQUIRE(c.applied); CHECK_FALSE(c.collecting);
    CHECK(c.ppm()==Catch::Approx(ppm).margin(1e-6));
    CHECK(c.correct(observed,true)==Catch::Approx(440).margin(1e-10));
    CHECK(c.correct(observed,false)==observed);
  }
  c.reset(); CHECK_FALSE(c.applied); CHECK(c.correct(440,true)==440);
  CHECK_FALSE(c.start(std::numeric_limits<double>::quiet_NaN()));
  CHECK_FALSE(c.start(0));
  REQUIRE(c.start(440));
  for(int i=0;i<150;++i) c.observe(i%2?440.5:439.5,true,-20);
  CHECK_FALSE(c.applied); CHECK_FALSE(c.collecting);
  REQUIRE(c.start(440));
  for(int i=0;i<150;++i) c.observe(440,false,-80);
  CHECK_FALSE(c.applied); CHECK_FALSE(c.collecting);
  REQUIRE(c.start(440)); c.cancel(); CHECK_FALSE(c.collecting);
}

TEST_CASE("Fine tracking preserves acquisition and rejects silence and transitions", "[desktop-tuner]") {
  constexpr double rate=44100, hz=220.13;
  app::PrecisionTuner fine; app::StreamingTuner baseline;
  const auto size=app::precisionHistorySize(rate);
  bool refined=false;
  for(int tick=1;tick<=8;++tick) {
    std::vector<double> context(size);
    for(std::size_t i=0;i<size;++i) {
      const double t=static_cast<double>(i)/rate+static_cast<double>(tick)/15;
      context[i]=.2*std::sin(2*std::numbers::pi*hz*t)+.1*std::sin(4*std::numbers::pi*hz*t);
    }
    std::vector<double> tail(context.end()-static_cast<std::ptrdiff_t>(app::tunerFrameSize(rate)),context.end());
    const auto original=baseline.process(tail,rate), p=fine.process(context,rate);
    CHECK(fine.raw.frequencyHz==original.frequencyHz);
    if(tick<4) { CHECK_FALSE(fine.refined); CHECK(p.frequencyHz==original.frequencyHz); }
    refined=refined||fine.refined;
    REQUIRE(p.voiced); CHECK(std::abs(1200*std::log2(p.frequencyHz/hz))<1);
    CHECK(p.periodSamples==Catch::Approx(rate/p.frequencyHz));
  }
  CHECK(refined);
  CHECK_FALSE(fine.process(std::vector<double>(size),rate).voiced);
  CHECK_FALSE(fine.refined);
  fine.reset(); CHECK_FALSE(fine.refined); CHECK_FALSE(fine.raw.voiced);
  CHECK_FALSE(fine.process(std::vector<double>(size),0).voiced);
}

TEST_CASE("Harmonic chord vocabulary keeps extensions and inversion labels", "[desktop-tuner]") {
  const auto vocabulary=app::harmonic::dictionary();
  const auto find=[&](const char* name) { return std::find_if(vocabulary.begin(),vocabulary.end(),[&](const auto& t){return t.label==name;}); };
  const auto major=find("C:maj7/3"), minor=find("D:min7/b7");
  REQUIRE(major!=vocabulary.end()); CHECK(major->triad==0); CHECK(major->bass==4);
  REQUIRE(minor!=vocabulary.end()); CHECK(minor->triad==14); CHECK(minor->bass==0);
  const auto silent=app::harmonic::recognize(std::vector<double>(8192),mmathen::SampleRateHz{44100});
  REQUIRE(silent.frames.size()==1); CHECK(silent.frames[0].label=="N");
  CHECK_THROWS(app::harmonic::recognize({},mmathen::SampleRateHz{44100}));
}

TEST_CASE("Native guitar model shares pitch-class rotations without key hints", "[desktop-chords]") {
  app::guitar::Features x{};
  for (std::size_t j=0;j<x.size();++j) x[j]=static_cast<double>((j*7+3)%17)/17;
  const auto reference=app::guitar::emissions(x,false);
  const auto types=app::guitar::model::types.size();
  for (std::size_t shift=1;shift<12;++shift) {
    app::guitar::Features transposed{};
    for (std::size_t j=0;j<48;++j) transposed[j/12*12+(j%12+shift)%12]=x[j];
    const auto actual=app::guitar::emissions(transposed,false);
    for (std::size_t root=0;root<12;++root) for (std::size_t type=0;type<types;++type)
      CHECK(actual[((root+shift)%12)*types+type]==Catch::Approx(reference[root*types+type]).margin(1e-10));
  }
  const auto silence=app::guitar::emissions(x,true);
  CHECK(std::max_element(silence.begin(),silence.end())==silence.end()-1);
  CHECK(app::guitar::label(0).label=="C:maj");
  CHECK(app::guitar::label(types*12).label=="N");
  CHECK_THROWS(app::guitar::label(types*12+1));
}

TEST_CASE("Native guitar recognition preserves silence and gain invariance", "[desktop-chords]") {
  using mmathen::SampleRateHz;
  for (double rate:{44100.,48000.}) {
    const auto silent=app::guitar::recognize(std::vector<double>(16384),SampleRateHz{rate});
    REQUIRE(silent.frames.size()==5);
    for (const auto& frame:silent.frames) CHECK(frame.label=="N");
    std::vector<double> audio(16384);
    for (std::size_t i=0;i<audio.size();++i) for (double hz:{130.8128,164.8138,195.9977})
      audio[i]+=.1*std::sin(2*std::numbers::pi*hz*static_cast<double>(i)/rate);
    const auto original=app::guitar::recognize(audio,SampleRateHz{rate});
    for(auto& value:audio)value*=.1;
    const auto quiet=app::guitar::recognize(audio,SampleRateHz{rate});
    REQUIRE(original.frames.size()==quiet.frames.size());
    for(std::size_t i=0;i<quiet.frames.size();++i) CHECK(original.frames[i].label==quiet.frames[i].label);
  }
  CHECK_THROWS(app::guitar::recognize({},SampleRateHz{44100}));
  for(double bad:{0.,5000.,std::numeric_limits<double>::infinity(),std::numeric_limits<double>::quiet_NaN()})
    CHECK_THROWS(app::guitar::recognize(std::vector<double>(8192),SampleRateHz{bad}));
  CHECK_THROWS(app::guitar::recognize({std::numeric_limits<double>::quiet_NaN()},SampleRateHz{44100}));
  CHECK(app::guitar::recognize({0},SampleRateHz{44100}).frames[0].label=="N");
}
