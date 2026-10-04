#pragma once

// Instruments — assembly layer (architecture L26–L27; docs/48).
//
// An instrument is objects + materials + excitation. Geometry (a length) is
// not a material (a linear density) and neither is the tuning a player asks
// of the string. This header assembles those three into a course and, for a
// pluck, hands the course to the existing ideal-string solvers (wave1d.h).
// The bridge end is a coupling hook into coupled_string.h: the caller
// supplies a BridgeOscillator. No measured bridge admittance is stored on
// an instrument (VALIDATION.md: unknown properties stay unknown). A single
// body mode, with its resonance, Q, and modal mass supplied by the caller,
// lives in body.h and does not feed back into the string.
//
// A pluck is an initial shape on the ideal string. A bow is not: steady
// Helmholtz motion lives in bow.h. A hammer is not: the hard narrow hammer
// lives in hammer.h. A reed is not: the quasi-static characteristic lives
// in reed.h. None of those three is played as a pluck.
//
// Dependency: this layer may use physics/ and the music pitch→frequency
// adapter. physics/ and core/ do not include instruments/.
//
// Status of a course built here: VERIFIED against the ideal-string law
// f = (1/(2L)) sqrt(T/μ) and against the caller's published table (the
// nominal density is the midpoint of the published range; construction
// fails if the tension that tunes the string leaves the published tension
// interval). UNVALIDATED as radiated sound: body.h is one open-loop mode
// driven by the rigid-bridge force, not a measured admittance and not a
// radiated pressure. The bow model in bow.h is steady Helmholtz motion on
// this ideal string, not a captured transient and not a radiated tone.

#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <optional>
#include <string>
#include <utility>

#include "../core/result.h"
#include "../music/pitch.h"
#include "../physics/coupled_string.h"
#include "../physics/wave1d.h"
#include "../units/physical_units.h"

namespace mmathen {

// Guettler's tables are printed to 0.1 Hz. Agreement means within one
// printed unit, which covers ordinary rounding and his cello D3 (146.9 Hz
// against the adapter's 146.83 Hz). A different pitch class still fails.
inline constexpr double kTabulatedHertzTolerance = 0.1;

// Shared by violin, viola, cello, and double bass. Nominal linear density is the
// arithmetic midpoint of a published g/m range; tension is then the
// ideal-string tension at the tuned frequency. Harp and harpsichord courses
// use the same law; their tables are not four-string bowed rows.
inline constexpr const char* kGuettlerBowedStringSource =
    "Knut Guettler, \"Some typical properties of bowed strings\", "
    "https://knutsacoustics.com/files/Typical-string-properties.pdf "
    "(table values contributed with Fan Tao, D'Addario, and Michel Simane, "
    "Corelli/Savarez). Nominal linear density is the arithmetic midpoint of "
    "the published g/m range; tension is the ideal-string tension at the "
    "tuned frequency and must lie inside the published tension range.";

inline constexpr const char* kHyperphysicsPlayingRange =
    "HyperPhysics, \"Playing Ranges of Instruments\", Georgia State "
    "University, "
    "http://hyperphysics.phy-astr.gsu.edu/hbase/Music/orchins.html . "
    "Approximate orchestral compass, not a mechanical stop.";

inline constexpr std::size_t kStringInstrumentCourses = 4;
// makeStringInstrument moves four built courses by index.
static_assert(kStringInstrumentCourses == 4);

enum class ExcitationKind {
  Pluck,   // realized: InitialShape::pluck on the ideal string
  Bow,     // not an initial shape; steady motion is bow.h
  Hammer,  // not an initial shape; hard narrow hammer is hammer.h
  Reed,    // not an initial shape; quasi-static characteristic is reed.h
};

// Event, not a property of the instrument. positionFraction is measured
// from the nut (0) toward the bridge (1) along the vibrating length.
// Zero is not a playing technique: the nut and the bridge are not interior
// points, and realizeExcitation rejects them.
struct Excitation {
  ExcitationKind kind = ExcitationKind::Pluck;
  double positionFraction = 0.0;
  double amplitudeMeters = 0.0;
};

struct StringGeometry {
  DistanceMeters vibratingLength;
};

struct StringMaterial {
  LinearDensityKgPerM linearDensity;
};

struct StringTuning {
  const char* label;  // scientific pitch; not owned
  FrequencyHz frequency;
};

// Inclusive published interval. Provenance for the annex, not a second
// material and not a second length.
struct TabulatedRange {
  double min = 0.0;
  double max = 0.0;
};

// One string: geometry, material, and the tension that realizes the tuning
// under the ideal-string law. Labels and source point at stable literals.
struct StringCourse {
  StringGeometry geometry;
  StringMaterial material;
  StringTuning tuning;
  TensionNewtons tension;
  TabulatedRange publishedDensityGPerM;
  TabulatedRange publishedTensionN;
  double publishedFrequencyHz = 0.0;
  // False when the source names the pitch and does not print a hertz
  // value. publishedFrequencyHz is then 0 and is not a second frequency.
  bool frequencyPrinted = true;
  const char* source = nullptr;
};

struct BodyGeometry {
  DistanceMeters length;  // corpus / back, not the vibrating string
  const char* source = nullptr;
};

// Approximate compass of sounding pitches. The low end is the lowest open
// string; the high end is a published practical limit, not a stop on the
// fingerboard.
struct PlayingCompass {
  const char* lowLabel = nullptr;
  const char* highLabel = nullptr;
  FrequencyHz low;
  FrequencyHz high;
  double publishedLowHz = 0.0;
  double publishedHighHz = 0.0;
  const char* source = nullptr;
};

struct StringInstrument {
  const char* name = nullptr;
  BodyGeometry body;
  std::array<StringCourse, kStringInstrumentCourses> courses;  // low to high
  PlayingCompass compass;
};

// One row of a published string table. Density is g/m, tension is N, both
// inclusive ranges. The nominal density is their arithmetic midpoint.
// BowedStringRow is the original name; harp and harpsichord rows are the
// same fields.
struct BowedStringRow {
  const char* label;
  double publishedFrequencyHz;
  double linearDensityMinGPerM;
  double linearDensityMaxGPerM;
  double tensionMinN;
  double tensionMaxN;
};

struct StringInstrumentSpec {
  const char* name;
  DistanceMeters bodyLength;
  const char* bodySource;
  DistanceMeters vibratingLength;
  const char* stringSource;
  std::array<BowedStringRow, kStringInstrumentCourses> courses;
  const char* compassLowLabel;
  const char* compassHighLabel;
  double compassPublishedLowHz;
  double compassPublishedHighHz;
  const char* compassSource;
};

[[nodiscard]] inline bool tabulatedHertzAgrees(double modeled,
                                               double published) {
  return std::isfinite(modeled) && std::isfinite(published) &&
         std::fabs(modeled - published) <= kTabulatedHertzTolerance;
}

// `unit` is one place of the source's printing (0.1 Hz when the table
// prints tenths, 1 Hz when it prints whole hertz).
[[nodiscard]] inline bool tabulatedHertzAgrees(double modeled, double published,
                                               double unit) {
  return std::isfinite(modeled) && std::isfinite(published) &&
         std::isfinite(unit) && unit > 0.0 &&
         std::fabs(modeled - published) <= unit;
}

namespace detail {

[[nodiscard]] inline bool positiveFinite(double v) {
  return std::isfinite(v) && v > 0.0;
}

[[nodiscard]] inline bool usableRange(double min, double max) {
  return std::isfinite(min) && std::isfinite(max) && min > 0.0 && max >= min;
}

[[nodiscard]] inline bool text(const char* s) {
  return s != nullptr && s[0] != '\0';
}

}  // namespace detail

// Ideal fixed-fixed string for one course. InvalidArgument if the course
// lengths or densities are not positive and finite.
[[nodiscard]] inline Result<WaveSystem1D, QuantityError> courseString(
    const StringCourse& course) {
  return idealString(course.geometry.vibratingLength, course.tension,
                     course.material.linearDensity);
}

// Open-string fundamental, f1 = (1/(2L)) sqrt(T/μ).
[[nodiscard]] inline Result<FrequencyHz, QuantityError> openStringFrequency(
    const StringCourse& course) {
  return stringMode(course.geometry.vibratingLength, course.tension,
                    course.material.linearDensity, 1);
}

// Attach the bridge end (x = L) to a caller-supplied oscillator. The stock
// instruments do not carry a bridge: there is no measured admittance on
// the course. body.h is a separate open-loop mode, not this coupling.
// InvalidArgument if the oscillator is unusable.
[[nodiscard]] inline Result<CoupledString, QuantityError> coupleToBridge(
    const StringCourse& course, BridgeOscillator bridge) {
  using R = Result<CoupledString, QuantityError>;
  auto system = courseString(course);
  if (!system.ok()) return R::failure(system.error());
  return makeCoupledString(system.value(), course.tension, bridge);
}

// Pluck → InitialShape. Bow, hammer, and reed are DomainError here: none
// is an initial displacement (see bow.h, hammer.h, and reed.h). A position
// outside (0, 1) or a non-finite amplitude is InvalidArgument.
[[nodiscard]] inline Result<InitialShape, QuantityError> realizeExcitation(
    const StringCourse& course, const Excitation& excitation) {
  using R = Result<InitialShape, QuantityError>;
  const double position = excitation.positionFraction;
  const double amplitude = excitation.amplitudeMeters;
  if (!std::isfinite(position) || !std::isfinite(amplitude))
    return R::failure(QuantityError::InvalidArgument);
  if (!(position > 0.0) || !(position < 1.0))
    return R::failure(QuantityError::InvalidArgument);
  if (excitation.kind != ExcitationKind::Pluck)
    return R::failure(QuantityError::DomainError);
  auto system = courseString(course);
  if (!system.ok()) return R::failure(system.error());
  const double x = position * course.geometry.vibratingLength.value();
  return InitialShape::pluck(system.value(), DistanceMeters{x}, amplitude);
}

// Modal sum of a realized pluck at observeFraction in [0, 1]. The error
// against the ideal string is ModalSolution's truncation bound; this
// function does not add a body or a bow.
[[nodiscard]] inline Result<double, QuantityError> modalPluckResponse(
    const StringCourse& course, const Excitation& excitation,
    double observeFraction, TimePointSeconds time, std::size_t terms) {
  using R = Result<double, QuantityError>;
  if (!std::isfinite(observeFraction) || observeFraction < 0.0 ||
      observeFraction > 1.0)
    return R::failure(QuantityError::InvalidArgument);
  auto shape = realizeExcitation(course, excitation);
  if (!shape.ok()) return R::failure(shape.error());
  auto modal = ModalSolution::create(shape.value(), terms);
  if (!modal.ok()) return R::failure(modal.error());
  const double x =
      observeFraction * course.geometry.vibratingLength.value();
  return modal.value().at(DistanceMeters{x}, time);
}

namespace detail {

// Nominal density is the midpoint of the g/m range. Tension is
// T = μ (2 L f)^2. RangeError if that tension leaves the interval.
// `label` is stored as the pointer the caller passed; it must outlive
// the course.
[[nodiscard]] inline Result<StringCourse, QuantityError> courseAtFrequency(
    DistanceMeters vibratingLength, const char* label, double frequency,
    double densityMinGPerM, double densityMaxGPerM, double tensionMinN,
    double tensionMaxN, double publishedFrequencyHz, bool frequencyPrinted,
    const char* source) {
  using R = Result<StringCourse, QuantityError>;
  if (!text(source) || !text(label))
    return R::failure(QuantityError::InvalidArgument);
  if (!positiveFinite(vibratingLength.value()) || !std::isfinite(frequency) ||
      !(frequency > 0.0))
    return R::failure(QuantityError::InvalidArgument);
  if (!usableRange(densityMinGPerM, densityMaxGPerM) ||
      !usableRange(tensionMinN, tensionMaxN))
    return R::failure(QuantityError::InvalidArgument);
  const double gramsPerMeter = 0.5 * (densityMinGPerM + densityMaxGPerM);
  const double linearDensity = gramsPerMeter * 1.0e-3;
  const double waveSpeed = 2.0 * vibratingLength.value() * frequency;
  const double tension = linearDensity * waveSpeed * waveSpeed;
  if (!std::isfinite(tension) || tension < tensionMinN || tension > tensionMaxN)
    return R::failure(QuantityError::RangeError);
  return R::success(StringCourse{
      StringGeometry{vibratingLength},
      StringMaterial{LinearDensityKgPerM{linearDensity}},
      StringTuning{label, FrequencyHz{frequency}},
      TensionNewtons{tension},
      TabulatedRange{densityMinGPerM, densityMaxGPerM},
      TabulatedRange{tensionMinN, tensionMaxN},
      frequencyPrinted ? publishedFrequencyHz : 0.0,
      frequencyPrinted,
      source});
}

}  // namespace detail

// Build one course. The nominal linear density is the midpoint of the
// published g/m range. Tension is T = μ (2 L f)^2 at the pitch-adapter
// frequency of `row.label`, so the open string is that pitch by
// construction. RangeError if that tension leaves the published tension
// interval, or if the published hertz value is not the adapter's pitch
// within `hertzUnit` (one printed unit; the bowed-string tables use
// kTabulatedHertzTolerance). InvalidArgument for a bad length, label,
// range, unit, or source.
[[nodiscard]] inline Result<StringCourse, QuantityError> makeStringCourse(
    DistanceMeters vibratingLength, const BowedStringRow& row,
    const char* source, double hertzUnit = kTabulatedHertzTolerance) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(row.label))
    return R::failure(QuantityError::InvalidArgument);
  auto pitch = makePitch(row.label);
  if (!pitch.ok()) return R::failure(pitch.error());
  const double frequency = pitch.value().frequency.value();
  if (!tabulatedHertzAgrees(frequency, row.publishedFrequencyHz, hertzUnit))
    return R::failure(QuantityError::RangeError);
  return detail::courseAtFrequency(
      vibratingLength, row.label, frequency, row.linearDensityMinGPerM,
      row.linearDensityMaxGPerM, row.tensionMinN, row.tensionMaxN,
      row.publishedFrequencyHz, true, source);
}

// Same ideal-string course when the source names the pitch and does not
// print hertz. There is then no second frequency to agree with.
// publishedFrequencyHz on the course is 0 and frequencyPrinted is false.
// RangeError if the ideal tension leaves the tension interval.
[[nodiscard]] inline Result<StringCourse, QuantityError>
makeStringCourseAtPitch(DistanceMeters vibratingLength, const char* label,
                        double densityMinGPerM, double densityMaxGPerM,
                        double tensionMinN, double tensionMaxN,
                        const char* source) {
  using R = Result<StringCourse, QuantityError>;
  if (!detail::text(label)) return R::failure(QuantityError::InvalidArgument);
  auto pitch = makePitch(label);
  if (!pitch.ok()) return R::failure(pitch.error());
  return detail::courseAtFrequency(
      vibratingLength, label, pitch.value().frequency.value(), densityMinGPerM,
      densityMaxGPerM, tensionMinN, tensionMaxN, 0.0, false, source);
}

// Four courses, low to high, on one body. Compass low must be the lowest
// open string. Compass high must sit strictly above the highest open
// string. RangeError if a course's table does not close, if the courses
// are not ascending, or if the compass is not that pair of pitches.
[[nodiscard]] inline Result<StringInstrument, QuantityError>
makeStringInstrument(const StringInstrumentSpec& spec) {
  using R = Result<StringInstrument, QuantityError>;
  if (!detail::text(spec.name) || !detail::text(spec.bodySource) ||
      !detail::text(spec.stringSource) || !detail::text(spec.compassSource) ||
      !detail::text(spec.compassLowLabel) ||
      !detail::text(spec.compassHighLabel))
    return R::failure(QuantityError::InvalidArgument);
  if (!detail::positiveFinite(spec.bodyLength.value()))
    return R::failure(QuantityError::InvalidArgument);
  // Courses hold quantities, so they are filled before the instrument
  // aggregate is built. optional is only the slot; an empty slot is an
  // error, not a silent string.
  std::array<std::optional<StringCourse>, kStringInstrumentCourses> built;
  for (std::size_t i = 0; i < kStringInstrumentCourses; ++i) {
    auto course = makeStringCourse(spec.vibratingLength, spec.courses[i],
                                   spec.stringSource);
    if (!course.ok()) return R::failure(course.error());
    if (i > 0 && !(course.value().tuning.frequency.value() >
                   built[i - 1]->tuning.frequency.value()))
      return R::failure(QuantityError::RangeError);
    built[i] = std::move(course).value();
  }
  if (std::strcmp(spec.compassLowLabel, built[0]->tuning.label) != 0)
    return R::failure(QuantityError::InvalidArgument);
  auto high = makePitch(spec.compassHighLabel);
  if (!high.ok()) return R::failure(high.error());
  const double highHz = high.value().frequency.value();
  const double lowHz = built[0]->tuning.frequency.value();
  if (!tabulatedHertzAgrees(lowHz, spec.compassPublishedLowHz) ||
      !tabulatedHertzAgrees(highHz, spec.compassPublishedHighHz))
    return R::failure(QuantityError::RangeError);
  const double topOpen =
      built[kStringInstrumentCourses - 1]->tuning.frequency.value();
  if (!(highHz > topOpen)) return R::failure(QuantityError::RangeError);
  return R::success(StringInstrument{
      spec.name,
      BodyGeometry{spec.bodyLength, spec.bodySource},
      {std::move(*built[0]), std::move(*built[1]), std::move(*built[2]),
       std::move(*built[3])},
      PlayingCompass{spec.compassLowLabel, spec.compassHighLabel,
                     FrequencyHz{lowHz}, FrequencyHz{highHz},
                     spec.compassPublishedLowHz, spec.compassPublishedHighHz,
                     spec.compassSource}});
}

[[nodiscard]] inline const StringCourse* findCourse(
    const StringInstrument& instrument, const char* label) {
  if (!detail::text(label)) return nullptr;
  for (const StringCourse& course : instrument.courses)
    if (course.tuning.label != nullptr &&
        std::strcmp(course.tuning.label, label) == 0)
      return &course;
  return nullptr;
}

// Inspectable name plus open-string spellings, low to high.
[[nodiscard]] inline std::string formula(const StringInstrument& instrument) {
  std::string text = instrument.name == nullptr ? std::string{}
                                                : std::string{instrument.name};
  for (const StringCourse& course : instrument.courses) {
    text.push_back(' ');
    if (course.tuning.label != nullptr) text += course.tuning.label;
  }
  return text;
}

}  // namespace mmathen
