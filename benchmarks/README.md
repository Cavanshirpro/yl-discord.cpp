# Offline benchmarks

This optional tool measures one synthetic dispatch/reply workload. It never logs
in, opens a Gateway connection, or uses a real Discord credential. See the
[method, reproduction and limitations](../docs/BENCHMARKS.md) before interpreting results.

The C++ target is opt-in: `-DYL_DISCORD_BUILD_BENCHMARKS=ON`. Build Release, install
the isolated Python requirements, then use `run.py` to alternate fresh processes
and save all samples plus medians. Results do not establish general superiority.
