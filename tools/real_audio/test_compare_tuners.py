import unittest
from compare_tuners import latest, reference_at, score, summarize


def row(t, hz=440, voiced=True):
    return [t, t - .09, hz, voiced, .9, -20, 1]


class ComparisonTests(unittest.TestCase):
    def test_predictions_are_causal(self):
        rows = [row(.1), row(.2, 880)]
        self.assertIsNone(latest(rows, .09))
        self.assertEqual(latest(rows, .19)[2], 440)
        self.assertEqual(latest(rows, .2)[2], 880)

    def test_reference_requires_full_isolated_window(self):
        ref = {"notes": [[.1, 1, 69, 0]], "contours": {0: [[.2, {"voiced": True, "frequency": 440}]]}}
        self.assertEqual(reference_at(ref, .2, .1), (0, 440))
        self.assertIsNone(reference_at(ref, .2, .09))
        ref["notes"].append([.18, .3, 72, 1])
        self.assertIsNone(reference_at(ref, .2, .1))

    def test_unvoiced_and_octaves_are_misses_with_same_denominator(self):
        raw = {"sampleRate": 44100, "samples": 44100,
               "desktop": [row(.2), row(.3)], "f3": [row(.2, 880), row(.3, 0, False)]}
        result = score(raw, nominal=69)["metrics"]
        self.assertEqual(result["desktop"]["accuracy50c"], 1)
        self.assertEqual(result["f3"]["accuracy50c"], 0)
        self.assertEqual(result["f3"]["octaveErrors"], 1)
        self.assertEqual(result["desktop"]["eligibleFrames"], result["f3"]["eligibleFrames"])
        self.assertEqual(result["f3"]["missedVoicingRate"], .5)

    def test_future_correct_frame_cannot_replace_current_wrong_frame(self):
        raw = {"sampleRate": 44100, "samples": 44100,
               "desktop": [row(.2)], "f3": [row(.19, 880), row(.21)]}
        self.assertEqual(score(raw, nominal=69)["metrics"]["f3"]["accuracy50c"], 0)

    def test_no_evidence_is_unknown(self):
        result = summarize([], {"desktop": [], "f3": []}, 1)
        self.assertIsNone(result["desktop"]["accuracy50c"])
        self.assertIsNone(result["f3"]["medianResidualStepCents"])

    def test_tight_tuning_error_is_separate_from_note_accuracy(self):
        raw = {"sampleRate": 44100, "samples": 44100,
               "desktop": [row(.2, 440 * 2 ** (20 / 1200))], "f3": [row(.2)]}
        result = score(raw, nominal=69)["metrics"]
        self.assertEqual(result["desktop"]["accuracy50c"], 1)
        self.assertEqual(result["desktop"]["accuracy10c"], 0)


if __name__ == "__main__":
    unittest.main()
