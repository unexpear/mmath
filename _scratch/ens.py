import io
T='tests/test_wind_tone.cpp'
s=io.open(T,encoding='utf-8',newline='').read(); CR='\r\n' in s; s=s.replace('\r\n','\n')
s=s.replace('#include <mmathen/instruments/cone_valve.h>\n','#include <mmathen/instruments/cone_valve.h>\n#include <mmathen/instruments/ensemble.h>\n',1)
a=s.index('TEST_CASE("wind songs are listening passages"')
body=s[a:]
old='    writePhrase(samples, (stem + ".wav").c_str(), kFs);\n  };'
print(body.count(old))
new='    writePhrase(samples, (stem + ".wav").c_str(), kFs);\n    std::vector<double> starts;\n    for (const auto& n : notes)\n      if (!n.rest && n.tongue) starts.push_back(n.startSec);\n    return std::make_pair(samples, starts);\n  };'
body=body.replace(old,new)
oe='''    play_trumpet(withLeadIn(excerpts::kSaintsTrumpet, "f"), "mmath-ens-saints-trumpet");
    play_clarinet(withLeadIn(excerpts::kSaintsClarinet, "mf"), "mmath-ens-saints-clarinet");
    play_trombone(withLeadIn(excerpts::kSaintsTrombone, "mf"), "mmath-ens-saints-trombone");
'''
assert oe in body
ne='''    auto trumpet = play_trumpet(withLeadIn(excerpts::kSaintsTrumpet, "f"), "mmath-ens-saints-trumpet");
    auto clarinet = play_clarinet(withLeadIn(excerpts::kSaintsClarinet, "mf"), "mmath-ens-saints-clarinet");
    auto trombone = play_trombone(withLeadIn(excerpts::kSaintsTrombone, "mf"), "mmath-ens-saints-trombone");
    // Each part at its -24 LUFS playback level, as the part WAVs, then the
    // engine ensemble: trumpet leads on the 0.28 s conductor beat.
    auto level = [](std::vector<double> x) {
      const double measured = steadyLufs(x, static_cast<std::size_t>(0.5 * kFs), x.size(), kFs);
      double gain = std::pow(10.0, (-24.0 - measured) / 20.0);
      double peak = 0.0;
      for (double v : x) peak = std::max(peak, std::fabs(v));
      const double ceiling = std::pow(10.0, -1.0 / 20.0);
      if (peak * gain > ceiling) gain = ceiling / peak;
      for (double& v : x) v *= gain;
      return x;
    };
    Ensemble band(kFs, 0.28);
    band.addPart("trumpet", level(trumpet.first), trumpet.second, 0.0);
    band.addPart("clarinet", level(clarinet.first), clarinet.second, -5.0);
    band.addPart("trombone", level(trombone.first), trombone.second, -3.0);
    const auto mixed = band.mix();
    for (const auto& part : band.parts())
      std::cout << "ensemble " << part.name << " median onset " << part.medianLatencyMs
                << " ms, shift " << part.shiftMs << " ms\\n";
    REQUIRE(!mixed.empty());
    const auto path = wavOutputPath("mmath-ens-saints-mix.wav");
    auto opened = WavSink::open(path.string(), SampleRateHz{kFs},
                                FrameCount{static_cast<std::uint64_t>(mixed.size())});
    REQUIRE(opened.ok());
    auto sink = std::move(opened).value();
    REQUIRE(sink.consume(mixed.data(), FrameCount{static_cast<std::uint64_t>(mixed.size())}));
    REQUIRE(sink.close().ok());
'''
body=body.replace(oe,ne)
s=s[:a]+body
io.open(T,'w',encoding='utf-8',newline='').write(s.replace('\n','\r\n') if CR else s)
