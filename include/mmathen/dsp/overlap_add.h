#pragma once

// D26 — streaming overlap-add (STFT) processor.
//
// Frames of N samples (power of two, 16..65536) every hop H = N/2 or N/4.
// Each frame is multiplied by the analysis window, transformed (N-point
// complex FFT, fft.h), handed to the caller's spectral callback, inverse
// transformed, multiplied by the synthesis window and overlap-added. Both
// windows are sqrt of the periodic Hann window, so their product is Hann,
// and periodic Hann shifted by H sums to exactly N/(2H) at every sample;
// the output is scaled by 2H/N. With a callback that leaves the spectrum
// alone the processor is an exact delay (perfect reconstruction to
// roundoff), from the first sample: every frame that covers a sample is
// computed (the input before time 0 is zero, as are the frames made of it).
//
// Latency: exactly N samples (a sample entering in hop k is complete once
// the last of the N/H frames covering it has been added, and leaves in the
// hop after). Declared by latencyFrames() and pinned by the tests.
// The callback receives all N complex bins; it should keep Hermitian
// symmetry (X[N-k] = conj X[k]) — only the real part of the inverse
// transform is used, so an asymmetric change is projected onto a real one.
// Allocation happens at create() only.

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <vector>

#include "../analysis/fft.h"
#include "../core/result.h"

namespace mmathen {

class OverlapAdd {
 public:
  // frameSize: power of two in [16, 65536]; hopDivisor: 2 or 4.
  [[nodiscard]] static Result<OverlapAdd, QuantityError> create(
      std::size_t frameSize, int hopDivisor = 4) {
    using R = Result<OverlapAdd, QuantityError>;
    if (frameSize < 16 || frameSize > 65536 || !isPowerOfTwo(frameSize) ||
        (hopDivisor != 2 && hopDivisor != 4))
      return R::failure(QuantityError::InvalidArgument);
    return R::success(
        OverlapAdd{frameSize, frameSize / static_cast<std::size_t>(hopDivisor)});
  }

  // Stream n samples through; `onFrame(std::vector<std::complex<double>>&)`
  // is called once per hop. in and out may alias.
  template <typename F>
  void process(const double* in, double* out, std::size_t n, F&& onFrame) {
    const std::size_t off = n_ - hop_;
    for (std::size_t i = 0; i < n; ++i) {
      const double x = in[i];
      out[i] = ready_[fill_];
      input_[off + fill_] = x;
      if (++fill_ == hop_) {
        fill_ = 0;
        frame(onFrame);
      }
    }
  }

  void reset() {
    std::fill(input_.begin(), input_.end(), 0.0);
    std::fill(acc_.begin(), acc_.end(), 0.0);
    std::fill(ready_.begin(), ready_.end(), 0.0);
    fill_ = 0;
  }

  [[nodiscard]] std::size_t frameSize() const noexcept { return n_; }
  [[nodiscard]] std::size_t hop() const noexcept { return hop_; }
  [[nodiscard]] std::size_t latencyFrames() const noexcept { return n_; }

 private:
  OverlapAdd(std::size_t n, std::size_t hop)
      : n_(n),
        hop_(hop),
        window_(n),
        input_(n, 0.0),
        acc_(n, 0.0),
        ready_(hop, 0.0),
        bins_(n) {
    for (std::size_t i = 0; i < n; ++i)
      window_[i] = std::sqrt(0.5 - 0.5 * std::cos(constants::twoPi *
                                                  static_cast<double>(i) /
                                                  static_cast<double>(n)));
    scale_ = 2.0 * static_cast<double>(hop) / static_cast<double>(n);
  }

  template <typename F>
  void frame(F& onFrame) {
    for (std::size_t i = 0; i < n_; ++i)
      bins_[i] = std::complex<double>(input_[i] * window_[i], 0.0);
    detail::fftInPlace(bins_, false);
    onFrame(bins_);
    detail::fftInPlace(bins_, true);
    for (std::size_t i = 0; i < n_; ++i)
      acc_[i] += bins_[i].real() * window_[i] * scale_;
    std::copy(acc_.begin(), acc_.begin() + static_cast<std::ptrdiff_t>(hop_),
              ready_.begin());
    std::copy(acc_.begin() + static_cast<std::ptrdiff_t>(hop_), acc_.end(),
              acc_.begin());
    std::fill(acc_.end() - static_cast<std::ptrdiff_t>(hop_), acc_.end(), 0.0);
    std::copy(input_.begin() + static_cast<std::ptrdiff_t>(hop_), input_.end(),
              input_.begin());
  }

  std::size_t n_;
  std::size_t hop_;
  std::vector<double> window_;
  std::vector<double> input_;
  std::vector<double> acc_;
  std::vector<double> ready_;
  std::vector<std::complex<double>> bins_;
  double scale_ = 1.0;
  std::size_t fill_ = 0;
};

}  // namespace mmathen
