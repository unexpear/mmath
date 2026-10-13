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

// Rimsky-Korsakov, Flight of the Bumblebee: the falling chromatic figure,
// hand-written an octave and a fourth down into the clarinet rows A3-A4.
inline const Note kBumblebee[] = {
    // Lead-in rest: the breath and airway settle before the first note.
    {"rest", 0.00, 0.560, false, false, "f"},
    {"A4", 440.00, 0.110, true, false, "f"},
    {"G#4", 415.30, 0.110, false, false, "f"},
    {"G4", 392.00, 0.110, false, false, "f"},
    {"F#4", 369.99, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, true, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, false, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"D#4", 311.13, 0.110, true, false, "f"},
    {"D4", 293.66, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, false, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, true, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"B3", 246.94, 0.110, false, false, "f"},
    {"A#3", 233.08, 0.110, false, false, "f"},
    {"A3", 220.00, 0.600, true, false, "f"},
    {"A4", 440.00, 0.110, true, false, "f"},
    {"G#4", 415.30, 0.110, false, false, "f"},
    {"G4", 392.00, 0.110, false, false, "f"},
    {"F#4", 369.99, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, true, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, false, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"D#4", 311.13, 0.110, true, false, "f"},
    {"D4", 293.66, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, false, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, true, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"B3", 246.94, 0.110, false, false, "f"},
    {"A#3", 233.08, 0.110, false, false, "f"},
    {"A3", 220.00, 0.600, true, false, "f"},
    {"A4", 440.00, 0.110, true, false, "f"},
    {"G#4", 415.30, 0.110, false, false, "f"},
    {"G4", 392.00, 0.110, false, false, "f"},
    {"F#4", 369.99, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, true, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, false, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"D#4", 311.13, 0.110, true, false, "f"},
    {"D4", 293.66, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, false, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, true, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"B3", 246.94, 0.110, false, false, "f"},
    {"A#3", 233.08, 0.110, false, false, "f"},
    {"A3", 220.00, 0.600, true, false, "f"},
    {"A3", 220.00, 0.110, true, false, "f"},
    {"A#3", 233.08, 0.110, false, false, "f"},
    {"B3", 246.94, 0.110, false, false, "f"},
    {"C4", 261.63, 0.110, false, false, "f"},
    {"C#4", 277.18, 0.110, false, false, "f"},
    {"D4", 293.66, 0.110, false, false, "f"},
    {"D#4", 311.13, 0.110, false, false, "f"},
    {"E4", 329.63, 0.110, false, false, "f"},
    {"F4", 349.23, 0.110, false, false, "f"},
    {"F#4", 369.99, 0.110, false, false, "f"},
    {"G4", 392.00, 0.110, false, false, "f"},
    {"G#4", 415.30, 0.110, false, false, "f"},
    {"A4", 440.00, 0.900, false, false, "f"},
};

// Uptown Funk horn riff, hand-written D-minor hits on the tuned trumpet
// pitches. An arrangement, not a transcription of the record.
inline const Note kUptownFunk[] = {
    // Lead-in rest: the breath and airway settle before the first note.
    {"rest", 0.00, 0.560, false, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"F5", 698.46, 0.200, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"rest", 0.00, 0.400, false, false, "f"},
    {"C5", 523.25, 0.200, true, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"Bb4", 466.16, 0.200, true, false, "f"},
    {"C5", 523.25, 0.200, true, false, "f"},
    {"D5", 587.33, 0.600, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"F5", 698.46, 0.200, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"rest", 0.00, 0.400, false, false, "f"},
    {"C5", 523.25, 0.200, true, false, "f"},
    {"D5", 587.33, 0.200, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"Bb4", 466.16, 0.200, true, false, "f"},
    {"C5", 523.25, 0.200, true, false, "f"},
    {"D5", 587.33, 0.600, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"F4", 349.23, 0.200, true, false, "f"},
    {"G4", 392.00, 0.200, true, false, "f"},
    {"Bb4", 466.16, 0.200, true, false, "f"},
    {"C5", 523.25, 0.200, true, false, "f"},
    {"D5", 587.33, 0.600, true, false, "f"},
    {"rest", 0.00, 0.200, false, false, "f"},
    {"D5", 587.33, 0.150, true, false, "f"},
    {"F5", 698.46, 0.800, true, false, "f"},
};

// Lennon-McCartney, A Day in the Life, verse melody, hand-written in G.
inline const Note kDayInTheLife[] = {
    {"B4", 493.88, 0.300, true, false, "mf"},
    {"B4", 493.88, 0.300, true, false, "mf"},
    {"B4", 493.88, 0.300, true, false, "mf"},
    {"A4", 440.00, 0.300, true, false, "mf"},
    {"B4", 493.88, 0.300, true, false, "mf"},
    {"C5", 523.25, 0.300, false, false, "mf"},
    {"B4", 493.88, 0.300, false, false, "mf"},
    {"A4", 440.00, 0.300, false, false, "mf"},
    {"G4", 392.00, 0.900, false, false, "mf"},
    {"rest", 0.00, 0.300, false, false, "mf"},
    {"A4", 440.00, 0.300, true, false, "mf"},
    {"B4", 493.88, 0.300, true, false, "mf"},
    {"B4", 493.88, 0.300, true, false, "mf"},
    {"A4", 440.00, 0.300, true, false, "mf"},
    {"G4", 392.00, 0.300, true, false, "mf"},
    {"A4", 440.00, 0.300, false, false, "mf"},
    {"B4", 493.88, 0.300, false, false, "mf"},
    {"A4", 440.00, 0.300, false, false, "mf"},
    {"G4", 392.00, 0.300, false, false, "mf"},
    {"E4", 329.63, 1.200, false, false, "mf"},
    {"rest", 0.00, 0.300, false, false, "mf"},
    {"G4", 392.00, 0.300, true, false, "mf"},
    {"A4", 440.00, 0.300, true, false, "mf"},
    {"B4", 493.88, 0.600, true, false, "mf"},
    {"D5", 587.33, 1.200, false, false, "mf"},
};

// She'll Be Coming 'Round the Mountain (traditional), in Bb.
inline const Note kMountain[] = {
    // Lead-in rest: the breath and airway settle before the first note.
    {"rest", 0.00, 0.560, false, false, "mf"},
    {"F4", 349.23, 0.280, true, false, "mf"},
    {"G4", 392.00, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 0.280, true, false, "mf"},
    {"G4", 392.00, 0.280, true, false, "mf"},
    {"F4", 349.23, 0.280, true, false, "mf"},
    {"D4", 293.66, 0.280, true, false, "mf"},
    {"F4", 349.23, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 1.120, true, false, "mf"},
    {"rest", 0.00, 0.280, false, false, "mf"},
    {"Bb4", 466.16, 0.280, true, false, "mf"},
    {"C5", 523.25, 0.280, true, false, "mf"},
    {"D5", 587.33, 0.280, true, false, "mf"},
    {"D5", 587.33, 0.280, true, false, "mf"},
    {"D5", 587.33, 0.280, true, false, "mf"},
    {"D5", 587.33, 0.280, true, false, "mf"},
    {"C5", 523.25, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 0.280, true, false, "mf"},
    {"C5", 523.25, 1.400, true, false, "mf"},
    {"rest", 0.00, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, true, false, "mf"},
    {"G4", 392.00, 0.280, true, false, "mf"},
    {"Bb4", 466.16, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"Bb4", 466.16, 1.400, true, false, "mf"},
};

// Taps (traditional bugle call), Bb trumpet concert pitch.
inline const Note kTaps[] = {
    {"F4", 349.23, 0.450, true, false, "mf"},
    {"F4", 349.23, 0.150, true, false, "mf"},
    {"Bb4", 466.16, 1.600, true, false, "mf"},
    {"F4", 349.23, 0.450, true, false, "mf"},
    {"Bb4", 466.16, 0.150, true, false, "mf"},
    {"D5", 587.33, 1.600, true, false, "mf"},
    {"F4", 349.23, 0.300, true, false, "mf"},
    {"Bb4", 466.16, 0.300, true, false, "mf"},
    {"D5", 587.33, 0.900, true, false, "mf"},
    {"F4", 349.23, 0.300, true, false, "mf"},
    {"Bb4", 466.16, 0.300, true, false, "mf"},
    {"D5", 587.33, 0.900, true, false, "mf"},
    {"Bb4", 466.16, 0.300, true, false, "mf"},
    {"D5", 587.33, 0.300, true, false, "mf"},
    {"F5", 698.46, 1.600, true, false, "mf"},
    {"rest", 0.00, 0.400, false, false, "mf"},
};

// London Bridge Is Falling Down (traditional), in G.
inline const Note kLondonBridge[] = {
    {"D5", 587.33, 0.600, true, false, "mf"},
    {"E5", 659.26, 0.200, true, false, "mf"},
    {"D5", 587.33, 0.400, true, false, "mf"},
    {"C5", 523.25, 0.400, true, false, "mf"},
    {"B4", 493.88, 0.400, true, false, "mf"},
    {"C5", 523.25, 0.400, true, false, "mf"},
    {"D5", 587.33, 0.800, true, false, "mf"},
    {"A4", 440.00, 0.400, true, false, "mf"},
    {"B4", 493.88, 0.400, true, false, "mf"},
    {"C5", 523.25, 0.800, true, false, "mf"},
    {"B4", 493.88, 0.400, true, false, "mf"},
    {"C5", 523.25, 0.400, true, false, "mf"},
    {"D5", 587.33, 0.800, true, false, "mf"},
    {"D5", 587.33, 0.600, true, false, "mf"},
    {"E5", 659.26, 0.200, true, false, "mf"},
    {"D5", 587.33, 0.400, true, false, "mf"},
    {"C5", 523.25, 0.400, true, false, "mf"},
    {"B4", 493.88, 0.400, true, false, "mf"},
    {"C5", 523.25, 0.400, true, false, "mf"},
    {"D5", 587.33, 0.800, true, false, "mf"},
    {"A4", 440.00, 0.800, true, false, "mf"},
    {"D5", 587.33, 0.800, true, false, "mf"},
    {"B4", 493.88, 0.400, true, false, "mf"},
    {"G4", 392.00, 1.200, true, false, "mf"},
};

// When the Saints Go Marching In (traditional), Dixieland ensemble in F concert,
// 64 beats at quarter = 214 (0.28 s). Hand-written arrangement on the tuned pitches:
// chords F | C7 | F | Bb | F C7 F. Every part sums to the same 17.92 s grid.
// Lead: the melody.
inline const Note kSaintsTrumpet[] = {
    // Lead-in rest: the breath and airway settle before the first note.
    {"rest", 0.00, 0.560, false, false, "f"},
    {"F4", 349.23, 0.280, true, false, "f"},
    {"A4", 440.00, 0.280, true, false, "f"},
    {"Bb4", 466.16, 0.280, true, false, "f"},
    {"C5", 523.25, 1.400, true, false, "f"},
    {"F4", 349.23, 0.280, true, false, "f"},
    {"A4", 440.00, 0.280, true, false, "f"},
    {"Bb4", 466.16, 0.280, true, false, "f"},
    {"C5", 523.25, 1.400, true, false, "f"},
    {"F4", 349.23, 0.280, true, false, "f"},
    {"A4", 440.00, 0.280, true, false, "f"},
    {"Bb4", 466.16, 0.280, true, false, "f"},
    {"C5", 523.25, 0.560, true, false, "f"},
    {"A4", 440.00, 0.560, true, false, "f"},
    {"F4", 349.23, 0.560, true, false, "f"},
    {"A4", 440.00, 0.560, true, false, "f"},
    {"G4", 392.00, 1.120, true, false, "f"},
    {"rest", 0.00, 0.280, true, false, "f"},
    {"A4", 440.00, 0.280, true, false, "f"},
    {"A4", 440.00, 0.280, true, false, "f"},
    {"G4", 392.00, 0.280, true, false, "f"},
    {"F4", 349.23, 0.840, true, false, "f"},
    {"F4", 349.23, 0.280, true, false, "f"},
    {"A4", 440.00, 0.560, true, false, "f"},
    {"C5", 523.25, 0.280, true, false, "f"},
    {"C5", 523.25, 0.280, true, false, "f"},
    {"Bb4", 466.16, 1.120, true, false, "f"},
    {"rest", 0.00, 0.280, true, false, "f"},
    {"A4", 440.00, 0.280, true, false, "f"},
    {"Bb4", 466.16, 0.280, true, false, "f"},
    {"C5", 523.25, 0.560, true, false, "f"},
    {"A4", 440.00, 0.560, true, false, "f"},
    {"F4", 349.23, 0.560, true, false, "f"},
    {"G4", 392.00, 0.560, true, false, "f"},
    {"F4", 349.23, 1.120, true, false, "f"},
    {"rest", 0.00, 0.560, true, false, "f"},
};

// Obbligato: chord-tone arpeggio, one per beat, A3..A4 (tuned range), tongued at chord changes and bar heads.
inline const Note kSaintsClarinet[] = {
    // Lead-in rest: the breath and airway settle before the first note.
    {"rest", 0.00, 0.560, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"G4", 392.00, 0.280, true, false, "mf"},
    {"Bb3", 233.08, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"E4", 329.63, 0.280, false, false, "mf"},
    {"G4", 392.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"D4", 293.66, 0.280, true, false, "mf"},
    {"Bb3", 233.08, 0.280, true, false, "mf"},
    {"D4", 293.66, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"D4", 293.66, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"Bb3", 233.08, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"F4", 349.23, 0.280, true, false, "mf"},
    {"A4", 440.00, 0.280, false, false, "mf"},
    {"A3", 220.00, 0.280, true, false, "mf"},
    {"C4", 261.63, 0.280, false, false, "mf"},
    {"A4", 440.00, 0.560, true, false, "mf"},
};

// Tailgate: root and fifth per two beats (F4/C5, E4/G4 over C7, Bb4/D5), F4..D5 tuned range.
inline const Note kSaintsTrombone[] = {
    // Lead-in rest: the breath and airway settle before the first note.
    {"rest", 0.00, 0.560, false, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.280, true, false, "mf"},
    {"G4", 392.00, 0.560, true, false, "mf"},
    {"E4", 329.63, 0.560, true, false, "mf"},
    {"G4", 392.00, 0.280, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.280, true, false, "mf"},
    {"D5", 587.33, 0.560, true, false, "mf"},
    {"Bb4", 466.16, 0.560, true, false, "mf"},
    {"D5", 587.33, 0.280, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"E4", 329.63, 0.560, true, false, "mf"},
    {"C5", 523.25, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
    {"F4", 349.23, 0.560, true, false, "mf"},
};

template <typename NoteArray>
double excerptSeconds(const NoteArray& notes) {
  double total = 0.0;
  for (const Note& note : notes) total += note.seconds;
  return total;
}

}  // namespace excerpts
}  // namespace mmathen
