p=r"D:\mmathEn_pr2_verify\include\mmathen\instruments\cone_valve.h"; s=open(p,encoding="utf-8").read()
def rep(a,b):
    global s
    assert s.count(a)==1,a[:60]; s=s.replace(a,b)
rep('#include "bore_valve.h"\n','#include "bore_valve.h"\n#include "brass_bore.h"\n')
rep('''      const double incident = readBore();
      const double reflected = cutoff_.step(incident);
      const double incoming = flareOn_ ? -flareLoss_.step(reflected)
                                    : (radiating_ ? openEnd_.step(reflected) : -reflected);''',
'''      const double incident = rfOn_ ? 0.0 : readBore();
      const double reflected = rfOn_ ? 0.0 : cutoff_.step(incident);
      const double incoming = rfOn_ ? convolveHistory(rf_)
                            : (flareOn_ ? -flareLoss_.step(reflected)
                                    : (radiating_ ? openEnd_.step(reflected) : -reflected));''')
rep('''      line_[pos_] = outgoing;
      pos_ = (pos_ + 1) % line_.size();''','''      if (rfOn_) {
        hist_[hpos_] = outgoing;
        hpos_ = (hpos_ + 1) % hist_.size();
      } else {
        line_[pos_] = outgoing;
        pos_ = (pos_ + 1) % line_.size();
      }''')
rep('''      double atBell = incident;
      if (lip_ && brassiness_ > 0.0) atBell = steepenAtBell(incident);
      const double bellFlow = (atBell - incoming) / impedance_ * bellScale_;''','''      double atBell = incident;
      if (!rfOn_ && lip_ && brassiness_ > 0.0) atBell = steepenAtBell(incident);
      const double bellFlow = rfOn_ ? convolveHistory(uf_)
                                    : (atBell - incoming) / impedance_ * bellScale_;''')
rep('''  bool setLipCollision(bool on) {''','''  // Opt-in measured Bb trumpet bore (brass_bore.h): the delay line, wall
  // pole, and bell filter are replaced by the bore's own reflection
  // function and mouth-flow transfer, from the chain-matrix input
  // impedance with Keefe wall loss and Levine-Schwinger radiation. The
  // lip sees the throat characteristic impedance; the cup is the bore's
  // own V0. Fingering inserts the valve loops. Off by default.
  bool useMeasuredTrumpetBore(brass_bore::Fingering fingering = {}) {
    if (!lip_) return false;
    const double fs = rate_.value();
    auto k = brass_bore::makeKernels(fs, density_, speed_, fingering);
    const double zc = density_ * speed_ /
                      (constants::pi * k.throatRadius * k.throatRadius);
    if (!std::isfinite(zc) || !(zc > 0.0)) return false;
    rf_ = std::move(k.reflection);
    uf_ = std::move(k.mouthFlow);
    hist_.assign(rf_.size(), 0.0);
    hpos_ = 0;
    impedance_ = zc;
    cupCoeff_ = zc * k.cupVolume / (density_ * speed_ * speed_) * fs;
    rfOn_ = true;
    return true;
  }
  [[nodiscard]] bool measuredBoreOn() const noexcept { return rfOn_; }

  bool setLipCollision(bool on) {''')
rep('''  double listener_ = 1.0;''','''  double listener_ = 1.0;
  bool rfOn_ = false;
  std::vector<double> rf_, uf_, hist_;
  std::size_t hpos_ = 0;
  // Sum over n >= 1 of kernel[n] * p+(t - n); hist_ holds past p+.
  [[nodiscard]] double convolveHistory(const std::vector<double>& kernel) const {
    const std::size_t n = hist_.size();
    double acc = 0.0;
    std::size_t idx = hpos_;
    for (std::size_t j = 1; j < kernel.size() && j <= n; ++j) {
      idx = (idx == 0) ? n - 1 : idx - 1;
      acc += kernel[j] * hist_[idx];
    }
    return acc;
  }''')
open(p,"w",encoding="utf-8",newline="").write(s); print("patched")
