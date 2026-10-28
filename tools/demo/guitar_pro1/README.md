# Guitar pro check 1 (Romance Anónimo)

- Score: Romance Anónimo / Spanish Romance, anon., 3/4, Mutopia 2006 (CC BY-SA 2.5), https://www.mutopiaproject.org/ftp/Anonymous/spanish-romance/ . No tempo marking in the score; the Mutopia MIDI uses q=90.
- Licence: `guitar72.csv` (and `../guitar_pro2/guitar72dyn.csv`) are adapted from that Mutopia edition and are licensed **CC BY-SA 2.5** (https://creativecommons.org/licenses/by-sa/2.5/), not under the repository's MIT licence.
- Reference recording: Jim Greeninger, "Romance Anónimo (Jeux interdits).ogg", Wikimedia Commons, CC0. Excerpt 1.9-20.9 s.
- Tempo: q = 72, from the reference's 2.5 s bar period (onset autocorrelation). Estimate, VERIFY. Score CSV is guitar.csv x1.25 (guitar72.csv).
- Render: solo4 harness, mmath classicalGuitar + SoundboardVoice (classicalGuitarBodyModes), pluck pos 0.5, no samples. Studio = dry convolved with Genesis 6 IR (studio_g6_hh_W_48k_eq2oct.f64), harness-side.
- Diagnostics: audiodiag/diag.py (equal RMS, 17 s) -> guitar-diag1.pdf, scorecard.json; nn.py note-to-note stats.
- The violin-specific "likely cause" text in diag.py rows does not apply to guitar; see the parent report for guitar causes.
