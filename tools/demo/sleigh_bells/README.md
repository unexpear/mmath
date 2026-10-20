Sleigh bells pro check 1. Reference: Wikimedia Commons File:Sleigh_bells.wav (CC0, The Midnite Wolf, AKG C414). Harness sb1.cpp (engine include/mmathen/instruments/sleigh_bell.h). Render: sb1 sbpro1.f64 9.8 6 3. Scoreboard: python3 sbscore.py out.png title ref.wav name=clip.f64

Pro3 (pattern): the reference's shake/strike timeline transcribed by gentl.py/sbpattern.py into ref_timeline.txt (engine text form, mmathen::parseSleighBellTimeline). Render: sbpat ref_timeline.txt sbpro3.f64 9.8125 6 3. Compare: python3 cmp.py ref.wav model.wav (pattern), acc.py (accent shape, band floors), listen.py ref.wav model.wav out.png (spectrograms, close-ups, ring, rattle).

Pro4: 38-partial measured ring, damping x2.5, strap flex + bell knocks (off by default, sweep in sleigh_bell.h comments), level table re-run (gentl.py CAL_*). Seed-averaged A/B: python3 avg.py ref.wav NAME (NAME_<seed>.f64; SB_* env overrides in sbpat).
