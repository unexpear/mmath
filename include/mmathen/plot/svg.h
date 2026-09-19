#pragma once

// D20 — visualization: spectrum bars, spectrum line and spectrogram as
// standalone SVG documents (text; standard library only, no dependencies).
//
// Every figure is a pure function of its data: identical input gives a
// byte-identical string (tested), numbers are printed with fixed precision
// through snprintf in the default "C" locale (a program that switches the
// global C locale to one with a decimal comma would change the output —
// stated, not handled). Figures carry their own background, axes and
// labels so they read the same anywhere.
//
// Level-to-geometry mappings are exact and tested:
//   bar / line height  h * clamp((L - dbMin) / (dbMax - dbMin), 0, 1)
//   spectrum x         log-frequency: x0 + w * log(f/fMin)/log(fMax/fMin);
//                      bins sharing a pixel column keep only the loudest
//                      (max pooling, as the spectrogram), at its exact x
//   spectrogram cell   max over the pooled block of (frame, bin) values
//                      (pooling never hides a peak), colour from
//                      clamp((L - dbMin)/(dbMax - dbMin)) through a fixed
//                      five-stop ramp (dark blue -> violet -> red ->
//                      orange -> pale yellow), linear between stops.
// Bands that summed zero bins (BandLevel::bins == 0) are drawn as a
// hatched "no data" slot, not as silence.

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "../analysis/spectrum.h"
#include "../core/result.h"
#include "../units/physical_units.h"

namespace mmathen {

struct PlotStyle {
  int width = 900;
  int height = 420;
  double dbMin = -90.0;
  double dbMax = 0.0;
  std::string title;
};

namespace detail {
inline std::string fmt(double v, int decimals = 2) {
  char buf[64];
  std::snprintf(buf, sizeof buf, "%.*f", decimals, v);
  return buf;
}

inline std::string escapeXml(const std::string& s) {
  std::string out;
  for (const char ch : s) {
    switch (ch) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      default: out += ch;
    }
  }
  return out;
}

// Fraction of the dB range, clamped to [0, 1].
inline double levelFraction(double db, double dbMin, double dbMax) {
  const double t = (db - dbMin) / (dbMax - dbMin);
  return t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
}

inline bool validStyle(const PlotStyle& s) {
  return s.width >= 200 && s.width <= 8000 && s.height >= 150 &&
         s.height <= 8000 && std::isfinite(s.dbMin) && std::isfinite(s.dbMax) &&
         s.dbMin < s.dbMax;
}

// Compact frequency label: 31.6, 125, 1k, 2.5k, 16k.
inline std::string freqLabel(double f) {
  if (f >= 1000.0) {
    const double k = f / 1000.0;
    return (std::fabs(k - std::round(k)) < 0.05 ? fmt(std::round(k), 0)
                                                 : fmt(k, 1)) +
           "k";
  }
  if (f >= 100.0) return fmt(std::round(f), 0);
  return fmt(f, 1);
}

// Five-stop colour ramp as a string, t in [0, 1] (see heatColorRgb).
inline std::string rampColor(double t);
inline std::string header(const PlotStyle& s) {
  std::string o = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" +
                  std::to_string(s.width) + "\" height=\"" +
                  std::to_string(s.height) + "\" viewBox=\"0 0 " +
                  std::to_string(s.width) + " " + std::to_string(s.height) +
                  "\" font-family=\"sans-serif\" font-size=\"12\">\n";
  o += "<rect x=\"0\" y=\"0\" width=\"" + std::to_string(s.width) +
       "\" height=\"" + std::to_string(s.height) + "\" fill=\"#ffffff\"/>\n";
  if (!s.title.empty())
    o += "<text x=\"" + std::to_string(s.width / 2) +
         "\" y=\"20\" text-anchor=\"middle\" font-size=\"15\" fill=\"#111111\">" +
         escapeXml(s.title) + "</text>\n";
  return o;
}

// Plot area margins (left, top, right, bottom).
struct Frame {
  double x0, y0, w, h;
};
inline Frame plotFrame(const PlotStyle& s) {
  return Frame{56.0, 34.0, static_cast<double>(s.width) - 72.0,
               static_cast<double>(s.height) - 34.0 - 44.0};
}

// Horizontal dB grid every `step` dB, labelled on the left.
inline std::string dbGrid(const PlotStyle& s, const Frame& fr) {
  const double range = s.dbMax - s.dbMin;
  const double step = range > 60.0 ? 20.0 : 10.0;
  std::string o;
  for (double db = std::ceil(s.dbMin / step) * step; db <= s.dbMax + 1e-9;
       db += step) {
    const double y = fr.y0 + fr.h * (1.0 - levelFraction(db, s.dbMin, s.dbMax));
    o += "<line x1=\"" + fmt(fr.x0) + "\" y1=\"" + fmt(y) + "\" x2=\"" +
         fmt(fr.x0 + fr.w) + "\" y2=\"" + fmt(y) +
         "\" stroke=\"#dddddd\" stroke-width=\"1\"/>\n";
    o += "<text x=\"" + fmt(fr.x0 - 6.0) + "\" y=\"" + fmt(y + 4.0) +
         "\" text-anchor=\"end\" fill=\"#555555\">" + fmt(db, 0) +
         "</text>\n";
  }
  o += "<text x=\"14\" y=\"" + fmt(fr.y0 + fr.h / 2.0) +
       "\" text-anchor=\"middle\" fill=\"#555555\" transform=\"rotate(-90 14 " +
       fmt(fr.y0 + fr.h / 2.0) + ")\">dB</text>\n";
  return o;
}
}  // namespace detail

// Five-stop heat-map colour ramp (dark blue -> violet -> red -> orange ->
// pale yellow), linear between stops, t clamped to [0, 1]. Shared by the
// SVG spectrogram and the desktop app so both draw identical colours.
[[nodiscard]] inline std::array<unsigned char, 3> heatColorRgb(double t) {
  static const double stops[5][3] = {{13, 8, 60},
                                     {110, 30, 140},
                                     {205, 55, 80},
                                     {245, 150, 40},
                                     {252, 240, 170}};
  if (!(t > 0.0)) t = 0.0;
  if (t > 1.0) t = 1.0;
  const double x = t * 4.0;
  int i = static_cast<int>(std::floor(x));
  if (i >= 4) i = 3;
  const double u = x - static_cast<double>(i);
  std::array<unsigned char, 3> rgb{};
  for (int c = 0; c < 3; ++c) {
    const double v = stops[i][c] + (stops[i + 1][c] - stops[i][c]) * u;
    rgb[static_cast<std::size_t>(c)] =
        static_cast<unsigned char>(std::lround(v));
  }
  return rgb;
}

namespace detail {
inline std::string rampColor(double t) {
  const auto rgb = heatColorRgb(t);
  char buf[16];
  std::snprintf(buf, sizeof buf, "#%02x%02x%02x", rgb[0], rgb[1], rgb[2]);
  return buf;
}
}  // namespace detail

// "EQ bars": one bar per band level. Levels must be finite or exactly the
// spectrum floor; style must be valid. Otherwise InvalidArgument.
[[nodiscard]] inline Result<std::string, QuantityError> svgBandBars(
    const std::vector<BandLevel>& levels, const PlotStyle& style) {
  using R = Result<std::string, QuantityError>;
  if (!detail::validStyle(style) || levels.empty())
    return R::failure(QuantityError::InvalidArgument);
  for (const auto& l : levels)
    if (!std::isfinite(l.levelDb))
      return R::failure(QuantityError::InvalidArgument);
  const auto fr = detail::plotFrame(style);
  std::string o = detail::header(style);
  o += "<defs><pattern id=\"nodata\" width=\"6\" height=\"6\" "
       "patternUnits=\"userSpaceOnUse\" patternTransform=\"rotate(45)\">"
       "<line x1=\"0\" y1=\"0\" x2=\"0\" y2=\"6\" stroke=\"#bbbbbb\" "
       "stroke-width=\"2\"/></pattern></defs>\n";
  o += detail::dbGrid(style, fr);
  const double slot = fr.w / static_cast<double>(levels.size());
  const double gap = slot * 0.15;
  const std::size_t labelEvery =
      levels.size() > 40 ? 6 : (levels.size() > 16 ? 3 : 1);
  for (std::size_t i = 0; i < levels.size(); ++i) {
    const auto& l = levels[i];
    const double x = fr.x0 + slot * static_cast<double>(i) + gap / 2.0;
    const double bw = slot - gap;
    if (l.bins == 0) {
      o += "<rect class=\"nodata\" x=\"" + detail::fmt(x) + "\" y=\"" +
           detail::fmt(fr.y0) + "\" width=\"" + detail::fmt(bw) +
           "\" height=\"" + detail::fmt(fr.h) + "\" fill=\"url(#nodata)\"/>\n";
    } else {
      const double frac =
          detail::levelFraction(l.levelDb, style.dbMin, style.dbMax);
      const double bh = fr.h * frac;
      o += "<rect class=\"bar\" x=\"" + detail::fmt(x) + "\" y=\"" +
           detail::fmt(fr.y0 + fr.h - bh) + "\" width=\"" + detail::fmt(bw) +
           "\" height=\"" + detail::fmt(bh) + "\" fill=\"" +
           detail::rampColor(0.25 + 0.6 * frac) + "\"><title>" +
           detail::freqLabel(l.band.center.value()) + " Hz: " +
           detail::fmt(l.levelDb, 1) + " dB</title></rect>\n";
    }
    if (i % labelEvery == 0)
      o += "<text x=\"" + detail::fmt(x + bw / 2.0) + "\" y=\"" +
           detail::fmt(fr.y0 + fr.h + 16.0) +
           "\" text-anchor=\"middle\" fill=\"#555555\">" +
           detail::freqLabel(l.band.center.value()) + "</text>\n";
  }
  o += "<text x=\"" + detail::fmt(fr.x0 + fr.w / 2.0) + "\" y=\"" +
       detail::fmt(fr.y0 + fr.h + 36.0) +
       "\" text-anchor=\"middle\" fill=\"#555555\">band centre (Hz)</text>\n";
  o += "</svg>\n";
  return R::success(std::move(o));
}

// Amplitude spectrum (dBFS) as a polyline on a log-frequency axis from fMin
// to fMax (0 < fMin < fMax). Bins outside [fMin, fMax] are skipped.
[[nodiscard]] inline Result<std::string, QuantityError> svgSpectrum(
    const Spectrum& spectrum, FrequencyHz fMin, FrequencyHz fMax,
    const PlotStyle& style) {
  using R = Result<std::string, QuantityError>;
  const double lo = fMin.value();
  const double hi = fMax.value();
  if (!detail::validStyle(style) || !std::isfinite(lo) || !std::isfinite(hi) ||
      !(lo > 0.0) || !(lo < hi) || spectrum.amplitude.empty())
    return R::failure(QuantityError::InvalidArgument);
  const auto fr = detail::plotFrame(style);
  std::string o = detail::header(style);
  o += detail::dbGrid(style, fr);
  const double span = std::log(hi / lo);
  // Decade gridlines.
  for (double d = std::pow(10.0, std::ceil(std::log10(lo))); d <= hi;
       d *= 10.0) {
    const double x = fr.x0 + fr.w * std::log(d / lo) / span;
    o += "<line x1=\"" + detail::fmt(x) + "\" y1=\"" + detail::fmt(fr.y0) +
         "\" x2=\"" + detail::fmt(x) + "\" y2=\"" + detail::fmt(fr.y0 + fr.h) +
         "\" stroke=\"#dddddd\" stroke-width=\"1\"/>\n";
    o += "<text x=\"" + detail::fmt(x) + "\" y=\"" +
         detail::fmt(fr.y0 + fr.h + 16.0) +
         "\" text-anchor=\"middle\" fill=\"#555555\">" + detail::freqLabel(d) +
         "</text>\n";
  }
  // Max-pool bins per pixel column (a dense spectrum has far more bins than
  // pixels): each column keeps its loudest bin at that bin's exact x, so no
  // peak is hidden and sparse spectra plot every bin unchanged.
  const auto db = amplitudeDb(spectrum);
  std::string pts;
  long lastColumn = -1;
  double bestX = 0.0, bestDb = 0.0;
  auto flush = [&]() {
    if (lastColumn < 0) return;
    const double y =
        fr.y0 + fr.h * (1.0 - detail::levelFraction(bestDb, style.dbMin,
                                                    style.dbMax));
    pts += detail::fmt(bestX) + "," + detail::fmt(y) + " ";
  };
  for (std::size_t k = 1; k < db.size(); ++k) {
    const double f = static_cast<double>(k) * spectrum.binHz;
    if (f < lo || f > hi) continue;
    const double x = fr.x0 + fr.w * std::log(f / lo) / span;
    const long column = static_cast<long>(std::floor(x));
    if (column != lastColumn) {
      flush();
      lastColumn = column;
      bestX = x;
      bestDb = db[k];
    } else if (db[k] > bestDb) {
      bestX = x;
      bestDb = db[k];
    }
  }
  flush();
  if (!pts.empty()) pts.pop_back();
  o += "<polyline class=\"spectrum\" fill=\"none\" stroke=\"#6e1e8c\" "
       "stroke-width=\"1.5\" points=\"" + pts + "\"/>\n";
  o += "<text x=\"" + detail::fmt(fr.x0 + fr.w / 2.0) + "\" y=\"" +
       detail::fmt(fr.y0 + fr.h + 36.0) +
       "\" text-anchor=\"middle\" fill=\"#555555\">frequency (Hz)</text>\n";
  o += "</svg>\n";
  return R::success(std::move(o));
}

// Spectrogram heat map: time left to right, frequency (linear, 0 .. fMax)
// bottom to top, max-pooled to at most maxColumns x maxRows cells.
[[nodiscard]] inline Result<std::string, QuantityError> svgSpectrogram(
    const Spectrogram& g, FrequencyHz fMax, std::size_t maxColumns,
    std::size_t maxRows, const PlotStyle& style) {
  using R = Result<std::string, QuantityError>;
  const double top = fMax.value();
  if (!detail::validStyle(style) || g.frames == 0 || g.bins == 0 ||
      !std::isfinite(top) || !(top > 0.0) || maxColumns < 1 ||
      maxColumns > 2048 || maxRows < 1 || maxRows > 2048)
    return R::failure(QuantityError::InvalidArgument);
  const std::size_t usedBins = (std::min)(
      g.bins, static_cast<std::size_t>(std::floor(top / g.binHz)) + 1);
  const std::size_t cols = (std::min)(maxColumns, g.frames);
  const std::size_t rows = (std::min)(maxRows, usedBins);
  const auto fr = detail::plotFrame(style);
  std::string o = detail::header(style);
  const double cw = fr.w / static_cast<double>(cols);
  const double rh = fr.h / static_cast<double>(rows);
  for (std::size_t c = 0; c < cols; ++c) {
    const std::size_t f0 = c * g.frames / cols;
    const std::size_t f1 = (c + 1) * g.frames / cols;
    for (std::size_t r = 0; r < rows; ++r) {
      const std::size_t b0 = r * usedBins / rows;
      const std::size_t b1 = (r + 1) * usedBins / rows;
      double v = kSpectrumFloorDb;
      for (std::size_t fi = f0; fi < f1; ++fi)
        for (std::size_t bi = b0; bi < b1; ++bi) v = (std::max)(v, g.at(fi, bi));
      o += "<rect x=\"" + detail::fmt(fr.x0 + cw * static_cast<double>(c)) +
           "\" y=\"" +
           detail::fmt(fr.y0 + fr.h - rh * static_cast<double>(r + 1)) +
           "\" width=\"" + detail::fmt(cw + 0.05) + "\" height=\"" +
           detail::fmt(rh + 0.05) + "\" fill=\"" +
           detail::rampColor(detail::levelFraction(v, style.dbMin, style.dbMax)) +
           "\"/>\n";
    }
  }
  // Axes labels.
  const double seconds = g.hopSeconds * static_cast<double>(g.frames - 1) +
                         g.frameSeconds;
  o += "<text x=\"" + detail::fmt(fr.x0) + "\" y=\"" +
       detail::fmt(fr.y0 + fr.h + 16.0) + "\" fill=\"#555555\">0 s</text>\n";
  o += "<text x=\"" + detail::fmt(fr.x0 + fr.w) + "\" y=\"" +
       detail::fmt(fr.y0 + fr.h + 16.0) +
       "\" text-anchor=\"end\" fill=\"#555555\">" + detail::fmt(seconds, 2) +
       " s</text>\n";
  o += "<text x=\"" + detail::fmt(fr.x0 - 6.0) + "\" y=\"" +
       detail::fmt(fr.y0 + 4.0) + "\" text-anchor=\"end\" fill=\"#555555\">" +
       detail::freqLabel(static_cast<double>(usedBins - 1) * g.binHz) +
       "</text>\n";
  o += "<text x=\"" + detail::fmt(fr.x0 - 6.0) + "\" y=\"" +
       detail::fmt(fr.y0 + fr.h) +
       "\" text-anchor=\"end\" fill=\"#555555\">0</text>\n";
  o += "<text x=\"" + detail::fmt(fr.x0 + fr.w / 2.0) + "\" y=\"" +
       detail::fmt(fr.y0 + fr.h + 36.0) +
       "\" text-anchor=\"middle\" fill=\"#555555\">time; frequency (Hz) "
       "upward</text>\n";
  o += "</svg>\n";
  return R::success(std::move(o));
}

// Write text to a file (binary mode, exact bytes). Returns bytes written;
// an unopenable path or a short write is RangeError.
[[nodiscard]] inline Result<std::size_t, QuantityError> writeTextFile(
    const std::string& path, const std::string& text) {
  using R = Result<std::size_t, QuantityError>;
  std::ofstream f(path, std::ios::binary | std::ios::trunc);
  if (!f) return R::failure(QuantityError::RangeError);
  f.write(text.data(), static_cast<std::streamsize>(text.size()));
  if (!f) return R::failure(QuantityError::RangeError);
  return R::success(text.size());
}

}  // namespace mmathen
