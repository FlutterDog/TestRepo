"""Shared pytest collection configuration."""

from lorentz_test.models.tests import TestStatus


# Pytest treats imported classes whose names start with ``Test`` as candidates.
# This enum is application data, not a test container.
TestStatus.__test__ = False
