// D20 — SVG visualization (Catch2).
//
// The figures are checked by their geometry, which is exact by
// construction: bar heights for levels at the top, middle and below the
// range; the log-frequency axis mapping fMin and fMax to the frame edges;
// spectrogram pooling taking the maximum; colour-ramp end stops; hatched
// "no data" bands; XML escaping; determinism; and file round trip.

#include <catch2/catch_test_macros.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

#include <mmathen/analysis/spectrum.h>
#include <mmathen/plot/svg.h>
#include <mmathen/units/physical_units.h>

using namespace mmathen;

namespace {
template <typename T, typename E>
T unwrap(Result<T, E> r) {
  REQUIRE(r.ok());
  return std::move(r).value();
}

std::size_t countOf(const std::string& s, const std::string& needle) {
  std::size_t n = 0;
  for (std::size_t p = s.find(needle); p != std::string::npos;
       p = s.find(needle, p + 1))
    ++n;
  return n;
}

// All values of attribute `attr` on elements containing `marker`, in order.
std::vector<std::string> attrValues(const std::string& svg,
                                    const std::string& marker,
                                    const std::string& attr) {
  std::vector<std::string> out;
  for (std::size_t p = svg.find(marker); p != std::string::npos;
       p = svg.find(marker, p + 1)) {
    const std::size_t end = svg.find('>', p);
    const std::string elem = svg.substr(p, end - p);
    const std::string key = " " + attr + "=\"";
    const std::size_t a = elem.find(key);
    if (a == std::string::npos) continue;
    const std::size_t b = elem.find('"', a + key.size());
    out.push_back(elem.substr(a + key.size(), b - a - key.size()));
  }
  return out;
}

BandLevel level(double centre, double db, std::size_t bins = 4) {
  return BandLevel{FrequencyBand{FrequencyHz{centre * 0.9},
                                 FrequencyHz{centre},
                                 FrequencyHz{centre * 1.1}},
                   std::pow(10.0, db / 10.0), db, bins};
}
}  // namespace

TEST_CASE("band bars map levels to exact heights", "[d20plot]") {
  PlotStyle style;  // 900 x 420, -90..0 dB: plot frame height 342
  const auto svg = unwrap(svgBandBars(
      {level(125.0, 0.0), level(250.0, -45.0), level(500.0, -200.0),
       level(1000.0, 12.0)},
      style));
  const auto heights = attrValues(svg, "<rect class=\"bar\"", "height");
  REQUIRE(heights.size() == 4);
  CHECK(heights[0] == "342.00");  // at dbMax: full
  CHECK(heights[1] == "171.00");  // mid-range: half
  CHECK(heights[2] == "0.00");    // below dbMin: clamped to empty
  CHECK(heights[3] == "342.00");  // above dbMax: clamped to full
  const auto ys = attrValues(svg, "<rect class=\"bar\"", "y");
  CHECK(ys[0] == "34.00");   // top of frame
  CHECK(ys[2] == "376.00");  // bottom of frame (34 + 342)
  CHECK(svg.rfind("<svg", 0) == 0);
  CHECK(svg.size() >= 7);
  CHECK(svg.substr(svg.size() - 7) == "</svg>\n");
}

TEST_CASE("empty bands are drawn as no data, not silence", "[d20plot]") {
  const auto svg = unwrap(svgBandBars(
      {level(31.6, kSpectrumFloorDb, 0), level(63.1, -20.0)}, PlotStyle{}));
  CHECK(countOf(svg, "class=\"nodata\"") == 1);
  CHECK(countOf(svg, "class=\"bar\"") == 1);
}

TEST_CASE("figures are deterministic and escape their text", "[d20plot]") {
  PlotStyle style;
  style.title = "EQ <before & after>";
  const std::vector<BandLevel> bars = {level(1000.0, -12.3)};
  const auto a = unwrap(svgBandBars(bars, style));
  const auto b = unwrap(svgBandBars(bars, style));
  CHECK(a == b);
  CHECK(a.find("EQ &lt;before &amp; after&gt;") != std::string::npos);
  CHECK(a.find("<before") == std::string::npos);
}

TEST_CASE("spectrum line uses an exact log-frequency axis", "[d20plot]") {
  Spectrum s;
  s.binHz = 10.0;
  s.amplitude.assign(101, std::pow(10.0, -90.0 / 20.0));  // bins 0..1000 Hz
  s.power.assign(101, 0.0);
  s.amplitude[1] = 1.0;    // 10 Hz at 0 dB
  s.amplitude[100] = 1.0;  // 1000 Hz at 0 dB
  const auto svg = unwrap(svgSpectrum(s, FrequencyHz{10.0}, FrequencyHz{1000.0},
                                      PlotStyle{}));
  const auto pts = attrValues(svg, "<polyline class=\"spectrum\"", "points");
  REQUIRE(pts.size() == 1);
  // First point: fMin at the left edge (x0 = 56), top (0 dB -> y = 34).
  CHECK(pts[0].rfind("56.00,34.00", 0) == 0);
  // Last point: fMax at the right edge (56 + 828 = 884), 0 dB.
  const std::string last = "884.00,34.00";
  CHECK(pts[0].size() >= last.size());
  CHECK(pts[0].substr(pts[0].size() - last.size()) == last);
  // 100 Hz sits exactly halfway on a two-decade log axis, at -90 dB (bottom).
  CHECK(pts[0].find("470.00,376.00") != std::string::npos);
}

TEST_CASE("spectrogram pools by maximum and uses the ramp ends",
          "[d20plot]") {
  Spectrogram g;
  g.frames = 4;
  g.bins = 4;
  g.binHz = 100.0;
  g.hopSeconds = 0.01;
  g.frameSeconds = 0.02;
  g.levelDb.assign(16, -200.0);
  g.levelDb[0 * 4 + 1] = 0.0;  // frame 0, bin 1: pooled into cell (0, 0)
  const auto svg = unwrap(svgSpectrogram(g, FrequencyHz{300.0}, 2, 2,
                                         PlotStyle{}));
  // 1 background + 4 cells.
  CHECK(countOf(svg, "<rect ") == 5);
  CHECK(countOf(svg, "fill=\"#fcf0aa\"") == 1);  // top stop: the peak cell
  CHECK(countOf(svg, "fill=\"#0d083c\"") == 3);  // bottom stop: the rest
}

TEST_CASE("visualizers reject invalid input", "[d20plot]") {
  PlotStyle bad;
  bad.dbMin = 0.0;
  bad.dbMax = -10.0;
  CHECK(svgBandBars({level(100.0, -3.0)}, bad).error() ==
        QuantityError::InvalidArgument);
  CHECK(svgBandBars({}, PlotStyle{}).error() == QuantityError::InvalidArgument);
  CHECK(svgBandBars({level(100.0, std::nan(""))}, PlotStyle{}).error() ==
        QuantityError::InvalidArgument);
  Spectrum s;
  s.binHz = 1.0;
  s.amplitude.assign(8, 1.0);
  CHECK(svgSpectrum(s, FrequencyHz{100.0}, FrequencyHz{10.0}, PlotStyle{})
            .error() == QuantityError::InvalidArgument);
  CHECK(svgSpectrogram(Spectrogram{}, FrequencyHz{100.0}, 4, 4, PlotStyle{})
            .error() == QuantityError::InvalidArgument);
}

TEST_CASE("writeTextFile round-trips exact bytes", "[d20plot]") {
  const std::string path = "d20_roundtrip_test.svg";
  const std::string text = "<svg>\nline two\r\nend</svg>\n";
  CHECK(unwrap(writeTextFile(path, text)) == text.size());
  std::ifstream f(path, std::ios::binary);
  const std::string back((std::istreambuf_iterator<char>(f)),
                         std::istreambuf_iterator<char>());
  f.close();
  CHECK(back == text);
  std::remove(path.c_str());
  CHECK(writeTextFile("no_such_dir_d20/x/y.svg", text).error() ==
        QuantityError::RangeError);
}
