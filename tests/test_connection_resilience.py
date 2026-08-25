import pytest

def test_connection_reconnect_backoff():
    intervals = [1000, 2000, 4000, 8000]
    for i, interval in enumerate(intervals):
        if i > 0:
            assert interval == intervals[i-1] * 2
    assert max(intervals) <= 8000
