# Units Design — Strict Dimensions + Semantic Strong Types

Status: locked · C++20 concepts + constexpr

## Rule

Naked `float`/`double` never cross module boundaries for measured quantities.
All quantities are strong types. Invalid combinations **fail at compile time**.

```cpp
FrequencyHz{440.0} + TimeSeconds{2.0}  // must NOT compile
```

Motivation — these all look like `440` but are incompatible:

```text
440 Hz · 440 rad/s · 440 samples · 440 s · 440 beats · 440 BPM
440 cents · 440 Pa · 440 V · 440 Ω
```

## Two kinds of quantities

**Physical (SI-dimensioned):**

```text
FrequencyHz, AngularFrequencyRadPerSec, TimeSeconds, DistanceMeters,
VelocityMetersPerSecond, AccelerationMetersPerSecondSquared,
PressurePascals, DensityKgPerM3, TemperatureKelvin,
PhaseRadians, Amplitude, Decibels, PowerWatts, SampleRateHz, SampleIndex
```

**Domain (musically/acoustically distinct, often dimensionless but NOT interchangeable):**

```text
Frequency            // cycles/s, musical/physical frequency
AngularFrequency     // rad/s — distinct from Frequency, explicit conversion only
SampleRate           // frames/s — distinct from Frequency
Phase                // radians, dimensionless but tagged
TimePoint            // instant (t_0 + offset); Duration is an interval — distinct types
Duration
FrameIndex           // integer position n
FrameCount           // integer count N (exact equality, never epsilon)
FrequencyRatio       // dimensionless, > 0 required for cents
SignedAudioValue     // digital sample, ±, unitless with declared export scale
Cents, Beat, TempoBPM, Pitch, AmplitudeLinear, GainLinear, PressurePascals, VoltageVolts
```

A unit name alone does not identify the quantity (cf. NIST SP 811 §4):
Frequency and AngularFrequency share inverse-time dimension; radians are
dimensionless yet carry information cancellation discards. Therefore
**cycles→radians conversion is always explicit** (`ω = 2π·f` function, never
implicit): `440 Hz` must never silently become `440 rad/s`.

Example: phase is dimensionless, but `PhaseRadians{3.14}` must not accept
an arbitrary scalar. `FrequencyRatio{1.5}` ≠ `AmplitudeLinear{0.5}` ≠ `NormalizedPosition{0.5}`.

## Legitimate operations: the operator table (ADR-018)

The tag alone does not define arithmetic. Only these exist (all else has no
overload and fails to compile):

```text
Duration + Duration -> Duration      Duration - Duration -> Duration
-TimePoint - TimePoint  -> Duration
TimePoint + Duration  -> TimePoint    TimePoint - Duration -> TimePoint
Frequency / Frequency -> FrequencyRatio (Result; non-positive -> DomainError)
AngularFrequency x Duration -> Phase (Result; symmetric overload exists)
toAngular(Frequency) -> AngularFrequency (Result; explicit function)
scalar x/scaling: Duration, Frequency, AngularFrequency, Phase, Pressure,
                  Cents, Beat, Amplitude, Gain, SignedAudioValue only
```

Deliberately absent: `TimePoint+TimePoint`, `Frequency+Duration`,
`Frequency+AngularFrequency`, `Frequency x Duration -> Phase` (would drop
2*pi), `FrameCount+FrameCount` (use tryIncrement/tryDecrement),
`FrequencyRatio+FrequencyRatio`, any `Quantity*Quantity`, any implicit
conversion between quantity types or to/from `double`.

```cpp
auto period = unwrap(period(FrequencyHz{440.0}));  // Result<DurationSeconds, QuantityError>
static_assert(std::same_as<decltype(period), DurationSeconds>);
// T = 1/f = 0.00227272727 s
```

```cpp
FrequencyHz{440} + TimeSeconds{1};  // compile error (no overload)
```

## Canonical storage (ADR-018)

Each public quantity has one documented representation; alternative units
enter only through named conversions (`durationFromMilliseconds`,
`frequencyFromKilohertz`); no runtime unit registry in Phase 0:

```text
Duration, TimePoint: seconds · Frequency: cycles/s · AngularFrequency: rad/s
Phase: radians · SampleRate: frames/s · FrequencyRatio: positive ratio
Cents: signed cents · FrameCount, FrameIndex: uint64 frames
```

## Validity: factories + Result (ADR-018)

Other required conversions (all explicit, named functions):

```
ω = 2π·f            (Frequency → AngularFrequency, explicit)
cents(r) = 1200·log2(r),  r > 0 required, else defined error
ratio(c) = 2^(c/1200)
t_n = t_0 + n / fs  (FrameIndex + SampleRate → TimePoint)
T = 1/f · 1 Hz = 1/s
```

## Validity is separate from type correctness

> Strong types protect meaning. Checked factories and fallible conversions
> protect valid ranges. Validated inputs do not guarantee a valid result.

Creation (`makeSampleRate`, `makeFrequencyRatio`, ...): `SampleRate <= 0`
and `FrequencyRatio <= 0` rejected; negative Cents, unwrapped Phase, and
finite out-of-range SignedAudioValue accepted; NaN/inf rejected. Conversions
(`period`, `cents`, `ratio`, `timeAt`, `frameCountForDuration`,
`beatDuration`, ...) return `Result<T, QuantityError>` (project-owned;
C++20 has no std::expected): bad inputs -> InvalidArgument/DomainError,
over/underflow -> RangeError. Nothing clamps, normalizes, or substitutes
defaults silently.

## Implementation sketch

- `dimension.h`: compile-time dimension tags.
- `quantity.h`: `Quantity<Value, Dimension, Tag>` template, explicit ctors,
  only whitelisted operators; `value()` accessor spelled out (no implicit `double`).
- `unit.h`: user literals (`440_Hz`, `0.5_s` — optional, keep minimal in MVP).
- `physical_units.h` / `music_units.h`: concrete aliases + conversion fns.
- Concepts: `template<FrequencyQuantity F> constexpr auto period(F f);`
  rather than generic arithmetic.

Requires: `concepts`, `constexpr`/`consteval`, `std::numbers`, `std::span`
(where useful), `<filesystem>` only in sink/examples, **no third-party deps**.

## Conventions

- `constexpr` for quantity arithmetic where meaningful (e.g. `constexpr FrequencyHz A4{440.0}`).
  C++20 does **not** provide portable `constexpr std::sin/cos/log2/pow` — compile-time
  tests cover quantity arithmetic; transcendental functions are runtime-tested.
  Never write replacement trig functions just to force `static_assert`.
- No implicit conversions to/from `double`.
- `.value()` extraction only at deliberate numerical or external boundaries
  (kernels, file I/O, interop). Formulas written by stripping every quantity to
  `double` defeat the unit system — review flag.
- Domain quantities get distinct tags even when dimensionally identical.
