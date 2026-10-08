#pragma once

// Audition excerpts as note data. Pitch is concert (A4 = 440). Duration is
// seconds at the score tempo. A tongued note is articulated; a note under a
// slur that a previous note opened is not. The first note of a render still
// uses the blow's own tongue. Dynamics are the score marking. The gain that
// multiplies blowing pressure stays 1: an 8% drop flattened this saxophone's
// low register, and the oscillators are not a measured dynamic range.
//
// Public domain, as of 2026. No engraved edition is copied. Grace notes are
// left out: an acciaccatura is shorter than a bore retune.

namespace mmathen {
namespace excerpts {

struct Note {
  const char* name;
  double hertz;
  double seconds;
  bool tongued;
  bool vibrato;
  const char* dynamic;
};

// Mozart, Clarinet Concerto K.622, second movement, opening. The clarinet
// in A part (written, key of F) sounded a minor third down, quarter = 60.
// Gershwin's Rhapsody in Blue glissando is not this line: the bore glide is
// 35 ms at the start of a slur, not a smear across a bar.
inline const Note kClarinet[] = {
    {"A3", 220.00, 1.0, true, false, "p"},  {"D4", 293.66, 1.5, false, false, "p"},
    {"F4", 349.23, 0.5, false, false, "p"}, {"F4", 349.23, 0.5, true, false, "p"},
    {"E4", 329.63, 0.5, false, false, "p"}, {"D4", 293.66, 1.0, true, false, "p"},
    {"rest", 0.0, 1.0, false, false, "p"},  {"A3", 220.00, 1.5, true, false, "p"},
    {"D4", 293.66, 0.5, false, false, "p"}, {"F4", 349.23, 0.5, false, false, "p"},
    {"A4", 440.00, 0.5, false, false, "p"}, {"A4", 440.00, 0.5, true, false, "p"},
    {"G4", 392.00, 0.5, false, false, "p"}, {"F4", 349.23, 1.0, true, false, "p"},
    {"rest", 0.0, 1.0, false, false, "p"},
};

// Mussorgsky, Pictures at an Exhibition, "Il vecchio castello", the melody
// Ravel gave the alto saxophone. Concert pitch (the piano's), dotted quarter
// = 50. From the D# pickup through the low G# cadence. Acciaccaturas omitted.
// Vibrato on the long tied notes: the solo is marked con espressione.
inline const Note kSaxophone[] = {
    {"D#4", 311.13, 0.4, true, false, "p"},  {"G#4", 415.30, 2.8, false, true, "p"},
    {"B4", 493.88, 0.4, false, false, "p"},  {"A#4", 466.16, 0.4, false, false, "p"},
    {"G#4", 415.30, 0.4, false, false, "p"}, {"E4", 329.63, 0.4, false, false, "p"},
    {"G#4", 415.30, 0.4, false, false, "p"}, {"G#4", 415.30, 0.8, false, false, "p"},
    {"D#4", 311.13, 0.4, false, false, "p"}, {"F#4", 369.99, 1.6, false, true, "p"},
    {"E4", 329.63, 0.4, false, false, "p"},  {"D#4", 311.13, 0.4, false, false, "p"},
    {"E4", 329.63, 0.4, false, false, "p"},  {"D#4", 311.13, 0.4, false, false, "p"},
    {"C#4", 277.18, 0.4, false, false, "p"}, {"D#4", 311.13, 0.4, false, false, "p"},
    {"G#3", 207.65, 2.4, false, true, "p"},
};

// Debussy, Prélude à l'après-midi d'un faune, opening flute, bars 1–2.
// Quarter = 44, the score's metronome. The statement and its repeat. The
// whole descent is one slur; the repeat takes a new breath.
inline const Note kFlute[] = {
    {"C#5", 554.37, 3.068181818, true, false, "p"},
    {"B4", 493.88, 0.340909091, false, false, "p"},
    {"A#4", 466.16, 0.227272727, false, false, "p"},
    {"A4", 440.00, 0.227272727, false, false, "p"},
    {"G#4", 415.30, 0.227272727, false, false, "p"},
    {"G4", 392.00, 1.022727273, false, false, "p"},
    {"A4", 440.00, 0.340909091, false, false, "p"},
    {"B4", 493.88, 0.340909091, false, false, "p"},
    {"C5", 523.25, 0.340909091, false, false, "p"},
    {"C#5", 554.37, 3.068181818, true, false, "p"},
    {"B4", 493.88, 0.340909091, false, false, "p"},
    {"A#4", 466.16, 0.227272727, false, false, "p"},
    {"A4", 440.00, 0.227272727, false, false, "p"},
    {"G#4", 415.30, 0.227272727, false, false, "p"},
    {"G4", 392.00, 1.022727273, false, false, "p"},
    {"A4", 440.00, 0.340909091, false, false, "p"},
    {"B4", 493.88, 0.340909091, false, false, "p"},
    {"C5", 523.25, 0.340909091, false, false, "p"},
};

// Mussorgsky / Ravel, Pictures, Promenade, opening. Piano right hand, the
// monophonic bars before the first chord, quarter = 91, tenuto tongued.
// The following bar is harmonized; it is not this single line.
inline const Note kTrumpet[] = {
    {"G4", 392.00, 60.0 / 91.0, true, false, "f"},
    {"F4", 349.23, 60.0 / 91.0, true, false, "f"},
    {"Bb4", 466.16, 60.0 / 91.0, true, false, "f"},
    {"C5", 523.25, 30.0 / 91.0, true, false, "f"},
    {"F5", 698.46, 30.0 / 91.0, false, false, "f"},
    {"D5", 587.33, 60.0 / 91.0, false, false, "f"},
    {"C5", 523.25, 30.0 / 91.0, true, false, "f"},
    {"F5", 698.46, 30.0 / 91.0, false, false, "f"},
    {"D5", 587.33, 60.0 / 91.0, false, false, "f"},
    {"Bb4", 466.16, 60.0 / 91.0, true, false, "f"},
    {"C5", 523.25, 60.0 / 91.0, true, false, "f"},
    {"G4", 392.00, 60.0 / 91.0, true, false, "f"},
    {"F4", 349.23, 60.0 / 91.0, true, false, "f"},
};

// Ravel, Boléro, trombone theme (theme B), quarter = 72, mf sostenuto.
// From the opening Bb through the repeated high Db. A glissando in the part
// is not a slide here: brass length changes at the note. The later
// acciaccatura is outside this cut.
inline const Note kTrombone[] = {
    {"Bb4", 466.16, 1.25 * (60.0 / 72.0), true, false, "mf"},
    {"A4", 440.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"G4", 392.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"F4", 349.23, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"Bb4", 466.16, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"C5", 523.25, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"A4", 440.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"G4", 392.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"Bb4", 466.16, 0.50 * (60.0 / 72.0), false, false, "mf"},
    {"A4", 440.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"G4", 392.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"Bb4", 466.16, 1.00 * (60.0 / 72.0), false, false, "mf"},
    {"A4", 440.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"Bb4", 466.16, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"A4", 440.00, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"G4", 392.00, 1.50 * (60.0 / 72.0), false, false, "mf"},
    {"F4", 349.23, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"E4", 329.63, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"D4", 293.66, 0.25 * (60.0 / 72.0), false, false, "mf"},
    {"E4", 329.63, 2.00 * (60.0 / 72.0), false, false, "mf"},
    {"rest", 0.0, 0.50 * (60.0 / 72.0), false, false, "mf"},
    {"Bb4", 466.16, 0.50 * (60.0 / 72.0), true, false, "mf"},
    {"C5", 523.25, 0.50 * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, 1.00 * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, 1.00 * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, 0.50 * (60.0 / 72.0), true, false, "mf"},
    {"Db5", 554.37, 0.50 * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, 0.50 * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, (1.0 / 3.0) * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, (1.0 / 3.0) * (60.0 / 72.0), false, false, "mf"},
    {"Db5", 554.37, (1.0 / 3.0) * (60.0 / 72.0), false, false, "mf"},
};

template <typename NoteArray>
double excerptSeconds(const NoteArray& notes) {
  double total = 0.0;
  for (const Note& note : notes) total += note.seconds;
  return total;
}

}  // namespace excerpts
}  // namespace mmathen
