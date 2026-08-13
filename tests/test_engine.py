import unittest

from market.engine import SuccessWeights, opportunity_score, success_rate


class TestSuccessRate(unittest.TestCase):
    def test_default_weights(self):
        self.assertAlmostEqual(success_rate(0.9, 0.5), 0.7)
        self.assertAlmostEqual(success_rate(1.0, 1.0), 1.0)
        self.assertAlmostEqual(success_rate(0.0, 0.0), 0.0)

    def test_custom_weights(self):
        w = SuccessWeights(popularity=0.3, demand=0.7)
        self.assertAlmostEqual(success_rate(1.0, 0.0, w), 0.3)
        self.assertAlmostEqual(success_rate(0.0, 1.0, w), 0.7)

    def test_clamped(self):
        self.assertEqual(success_rate(2.0, -1.0), 0.5)
        self.assertEqual(success_rate(-5.0, 5.0), 0.5)


class TestOpportunityScore(unittest.TestCase):
    def test_monotonic_in_both(self):
        s = opportunity_score(0.5, 0.3)
        self.assertGreater(opportunity_score(0.9, 0.3), s)
        self.assertGreater(opportunity_score(0.5, 0.6), s)

    def test_zero_sr_or_margin(self):
        self.assertEqual(opportunity_score(0.0, 0.3), 0.0)
        self.assertEqual(opportunity_score(0.9, 0.0), 0.0)

    def test_examples(self):
        self.assertAlmostEqual(opportunity_score(0.9, 0.4), 100 * 0.9 * (0.4 / 0.3) ** 0.5, places=6)
        self.assertAlmostEqual(opportunity_score(0.3, 0.6), 100 * 0.3 * (0.6 / 0.3) ** 0.5, places=6)

    def test_margin_ref_validation(self):
        with self.assertRaises(ValueError):
            opportunity_score(0.5, 0.3, margin_ref=0)


if __name__ == "__main__":
    unittest.main()
