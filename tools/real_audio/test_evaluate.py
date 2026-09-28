"""Scoring contracts: timing, exclusions, voicing, and word edit distance."""
import unittest

from evaluate import chord_cells, pitch_reference, ratio, reduce_label, score_chords, score_pitch, score_words
from prepare import select_guitar


class EvaluationTests(unittest.TestCase):
    def test_duration_weighting_and_boundary_split(self):
        metric, intervals = score_chords([[0, 3, "C:maj"], [3, 4, "G:maj"]], [[0, 2, "C:maj"], [2, 4, "G:maj"]])
        self.assertAlmostEqual(metric["accuracy"], .75)
        self.assertEqual([r[:2] for r in intervals], [[0, 2], [2, 3], [3, 4]])

    def test_unsupported_is_excluded_not_correct(self):
        m, intervals = score_chords([[0, 2, "C:sus4"], [2, 3, "C:maj"]], [[0, 3, "C:maj"]])
        self.assertEqual(m["scoredSeconds"], 1)
        self.assertEqual(m["unsupportedSeconds"], 2)
        self.assertAlmostEqual(m["coverage"], 1/3)
        self.assertIsNone(intervals[0][4])

    def test_all_unsupported_is_unknown(self):
        m, _ = score_chords([[0, 1, "X"]], [[0, 1, "C:maj"]])
        self.assertIsNone(m["accuracy"])
        self.assertIsNone(ratio(0, 0))

    def test_no_silence_padding_or_gap_credit(self):
        m, _ = score_chords([[0, 1, "N"], [2, 4, "C:maj"]], [[.5, 3, "N"]])
        self.assertEqual(m["comparedSeconds"], 1.5)
        self.assertEqual(m["uncoveredSeconds"], 1.5)
        self.assertAlmostEqual(m["accuracy"], 1/3)

    def test_chord_mapping_matches_established_rules(self):
        self.assertEqual(reduce_label("Db:maj7/3"), "C#:maj")
        self.assertEqual(reduce_label("A:min7"), "A:min")
        self.assertIsNone(reduce_label("C:maj(*5)"))
        self.assertIsNone(reduce_label("C:dim"))
        self.assertEqual(reduce_label("N"), "N")

    def test_centers_get_only_one_hop_of_coverage(self):
        raw = {"config": {"hop": 2}, "sampleRate": 10, "samples": 20,
               "chords": [[.4, 0, 7], [.6, 0, 7]]}
        cells = chord_cells(raw, 2)
        self.assertAlmostEqual(cells[0][0], .3)
        self.assertAlmostEqual(cells[-1][1], .7)
        self.assertEqual(cells[0][2], "G:maj")

    def test_bad_reference_fails_closed(self):
        for ref in ([[0, 2, "C:maj"], [1, 3, "G:maj"]], [[0, 0, "N"]]):
            with self.assertRaises(ValueError): score_chords(ref, [[0, 3, "N"]])
        with self.assertRaises(ValueError): score_chords([[0, 1, "N"]], [[2, 3, "N"]])

    def test_pitch_requires_full_single_note_window(self):
        contours = {0: [[.5, {"frequency": 440, "voiced": True}]]}
        self.assertEqual(pitch_reference(.5, .1, [[0, 1, 69, 0]], contours), 440)
        self.assertIsNone(pitch_reference(.5, .1, [[.49, 1, 69, 0]], contours))
        self.assertIsNone(pitch_reference(.5, .1, [[0, 1, 69, 0], [.54, .7, 72, 1]], contours))
        self.assertIsNone(pitch_reference(.6, .1, [[0, 1, 69, 0]], contours))

    def test_unvoiced_and_octave_errors_count_as_misses(self):
        raw = {"config": {"frameSize": 10}, "sampleRate": 100,
               "pitch": [[.5, 440, True, 1], [.5, 440, False, .1], [.5, 880, True, 1]]}
        ref = {"notes": [[0, 1, 69, 0]], "contours": {0: [[.5, {"frequency": 440, "voiced": True}]]}}
        m, _ = score_pitch(raw, ref)
        self.assertEqual(m["eligibleFrames"], 3)
        self.assertAlmostEqual(m["accuracy50c"], 1/3)
        self.assertAlmostEqual(m["voicedRecall"], 2/3)

    def test_word_edits_and_normalization(self):
        m = score_words("The cat sat.", "the dog sat here")
        self.assertEqual((m["substitutions"], m["insertions"], m["deletions"]), (1, 1, 0))
        self.assertAlmostEqual(m["wer"], 2/3)
        self.assertEqual(score_words("DON'T STOP", "Don’t stop!")["wer"], 0)
        self.assertEqual(score_words("a a b", "a b")["deletions"], 1)
        self.assertIsNone(score_words("", "hello")["wer"])
        self.assertEqual(score_words("hello", "")["wer"], 1)

    def test_selection_is_balanced_without_predictions(self):
        names = [f"{p:02d}_{s}{k}-100-C_{m}.jams" for p in range(6)
                 for s in ("BN", "Funk", "Jazz", "Rock", "SS") for k in (1, 2, 3)
                 for m in ("comp", "solo")]
        result = select_guitar(names)
        self.assertEqual(len(result), 36)
        self.assertEqual(sum(mode == "chords" for _, mode in result), 30)
        self.assertEqual(result, select_guitar(list(reversed(names))))


if __name__ == "__main__":
    unittest.main()
