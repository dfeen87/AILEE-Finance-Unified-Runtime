# Finance hardening informed by AILEE Trust Layer

Review date: 2026-11-02. Finance baseline:
`73921d2f9ec09fe7a401fdcc4c9c604680564c2a` (merged Pass D). Reference:
Trust Layer `de238cd4a9edd53f978bc33a02f77f891887f263` (v10.0.2).
This is an unreleased v25.0.0 review candidate (PR #108). The v24.0.0 BEDROCK release remains historical; source metadata on this branch now targets v25.0.0 without publishing a tag or deployment artifact.

## Comparison and corrected boundaries

| Trust Layer invariant | Confirmed finance gap | Correction |
|---|---|---|
| Validate evidence before clamps/scoring | Infinite SELL position returned infinite Level-0 and fallback quantities; NaN volatility/liquidity could pass as healthy | Finite typed quantity/context validation, bounded fallback, final mutable-decision validation, strict JSON evidence |
| Hard bounds govern fallback | C++ `max_position_abs=.02` still returned fallback `.1` | All fallback routes apply the final hard cap; malformed knobs and impossible quorum rejected |
| Failed updates preserve accepted history | Invalid native config replaced active config; window shrink could use old instead of newest history | Candidate validation before commit; chronology-preserving ring repack |
| Malformed evidence never becomes authority | Unified/advisory helpers defaulted NaN to healthy values; native decision engine ignored a bound unified denial | Reject malformed evidence; zero-exposure native denial; finite and capacity guards across governance; staged temporal/stress updates |
| Independent consensus/quorum | Simulation and installed-wheel fallback engines diverged; duplicate IDs/nonfinite evidence were accepted | One dependency-free Python model engine, validated snapshots, unique model IDs, finite calculations, atomic history commits |
| Temporal evidence remains governing | Replayed clocks/frames could reset state through caller defaults; kernel recreated stateful operators | Monotonic clock/unified/stream cursors, persistent session operator instances, typed sequence validation and wrap guards |
| Complete canonical lineage | Python ledger used a short unlinked partial digest; native delimiter text could collide and file append restarted genesis | Canonical full decision/context SHA-256 chain; native control-character rejection, bounded text, durable verified restart journal |
| Explicit degraded/unknown execution | Broker `{}` manufactured `SUBMITTED_SUCCESS`; IDs were text-extracted; accepted orders were treated as positions | Strict bounded JSON acknowledgment, truthful pending state, no automatic retry or fictitious fill |
| Safety envelope survives integration | Unchecked sizing, ignored limit/ref, unbound replay identity, silent mock backend | Explicit bounded BUY LIMIT, durable identity reservation, finite configuration/equity/quantity checks; Python backend explicitly simulation-only |
| Mathematical evidence has no authorization | Existing HFT rectangles were conflated with canonical sampled integration; durations differed across runtimes; mass was floored from zero | Separate exact Python/C++ sampled engine, typed failures, validated compact evidence, audit attachment, noninterference tests; safe legacy heuristic boundary |

The original 138-case Python boundary matrix produced 122 failures and 16 passes
before correction. The initial five native test groups all failed. The model
engine regressions recorded 58 baseline failures; the advisory ingress matrix
recorded 76 failures and four passes. The original broker separately returned
`SUBMITTED_SUCCESS` for `{}` and for a real ID whose formatting defeated textual
extraction, and returned an ID for an explicitly rejected status. These probes
used local synthetic data and servers. No production broker was contacted.

## Execution, persistence and migration

See [execution integrity](execution-integrity-hardening.md) for the complete
broker migration. Non-mock execution requires an explicit bounded BUY LIMIT,
validated client identity, endpoint/mode/consent binding, and a private durable
journal. Market quantity orders, directional SELL without holdings proof,
unknown retries and unsupported durable-file semantics fail closed. An accepted
order stays pending until trusted external broker/position reconciliation.
Changing journal paths or deleting reservations is not supported recovery.

Native `AuditLogger(filename)` now owns a private regular `<filename>.journal`,
locks one writer, fsyncs records, verifies the complete chain on restart, and
continues decision IDs/hashes. Torn/corrupt records, symlinks/hard links,
insecure ownership/mode, ambiguous control characters and more than ten
contributors are rejected. The ten-contributor bound preserves the 256-byte
record ABI without silently omitting evidence. Journal storage is bounded at
64 MiB and requires explicit archival/rotation; records are never silently
discarded. CSV is a derived human-readable export. A legacy CSV without a
recoverable journal cannot reconstruct nanoseconds/float precision and is
rejected for explicit migration, rather than inventing a new chain.

The native journal's binary schema is versioned and supports the tested native
ABI; cross-endian or other-ABI migration is not inferred. File-backed native
audit/non-mock broker execution fails closed where private locking/durability
has not been implemented (including Windows). Default in-memory audit remains
available. The legacy reference getter requires caller synchronization during
concurrent writes; use serialized reporting/verification for a stable view.
Unkeyed hashes provide integrity checks, not authentication against an actor
who controls the process/account and can rewrite both records and hashes.

Python ledger records include the complete decision/context and previous hash;
ordinary kernel calls now share their operator's session chain. That chain is
process-local. It is not the authoritative broker intent store or a claim of
durable settled balances. Python volume execution rejects `mock_mode=False`
because no Python broker provider exists. Audit failure rolls back simulated
order/position/hysteresis state and locks execution.

Native unified faults latch until explicit state/metrics reset. Python unified
and stream cursors persist per operator/kernel session; recreating a session
does not supply cross-process feed authentication/replay storage. Durable broker
identities provide the independent execution restart boundary.

## Dependencies and delivery

The declared Python dependency audit found six vulnerable pinned packages
(the scanner reported 34 advisory entries, including aliases/duplicates).
Requests, urllib3, Flask, Jinja2, pytest and python-dotenv were upgraded; the
fresh declared-dependency scan reports no known vulnerabilities. CI now repeats
the audit, uses a read-only token, immutable action revisions and a job timeout;
Dependabot configuration covers Python and Actions updates.

The initial hosted Python 3.10 run caught another platform-resolution issue:
its supported Pillow version was vulnerable, while the local Python 3.12
resolver selected patched Pillow 13. CI now uses the tested Python 3.12 and
declares `pillow>=13.0.0`; development dependencies require Python >=3.11.
The pure standard-library SDK is separate from those development dependencies.
The security gate remains strict; no vulnerability was ignored to pass it.

The vendored cpp-httplib 0.14.3 was replaced with upstream 0.64.0 at immutable
commit `24e58ce5b25d55045af4cd6f463e83a8082fbe6b`. Its setup script verifies
SHA-256 `805169049e6a6e9ba7729d7e01d14ced825029c88a277586602a6b38df4d5357`
before replacing the header. Review included the upstream
[security advisories](https://github.com/yhirose/cpp-httplib/security/advisories).
macOS links the required Security/CoreFoundation frameworks; explicit OpenSSL
CA-bundle configuration remains supported with certificate verification enabled.
TLS rejection and no-redirect controls remain enforced. Telemetry/dashboard
WebSocket listeners now bind loopback by default.

## Verification

Local environment: macOS arm64, Python 3.12.12, pytest 9.0.3, Apple Clang 21,
OpenSSL 3 and CMake 4.4.4. Commands use an isolated venv; macOS TLS links the
Security/CoreFoundation frameworks. Test servers bind only loopback and use
synthetic credentials. No live broker was used.

| Executed check | Result |
|---|---|
| Complete Python, `python -m pytest tests -q` | **942 passed** |
| Complete optimized Python, `python -O -m pytest tests -q` | **942 passed**, expected pytest optimization warning |
| `make test_suite` and `./test_suite` | **197 run, zero failed** |
| CMake Release with `-DAILLE_BUILD_TESTS=ON`, CTest | **10/10 groups passed**, including canonical math and durable audit restart |
| Native boundary suite with ASan/UBSan | **10/10 groups passed**, no diagnostics |
| Actual broker integrity driver under ASan/UBSan | **42 passed**, no diagnostics; macOS leak detection disabled |
| SDK Debug/Release build, install, relocation and external consumer | Passed |
| REST C++ lifecycle and Python HTTP/SIGTERM/rebind/collision checks | Passed |
| Installed wheel without development dependencies | Passed within complete suite, including canonical mathematical evidence and malformed quantity protection |
| Declared Python dependency audit | No known vulnerabilities in the resolved declared dependencies |
| `compileall`, `pip check`, `git diff --check` | Passed |
| Immutable HTTP dependency fetch and SHA-256 verification | Passed |

The broker sanitizer harness preserves the actual test bodies and production
sources, changing only compiler flags and the temporary harness's source-root
path. This is focused sanitizer evidence, not a full-process leak or platform
certification. Full Docker image execution and Windows/Linux hosted CI results
are not inferred from the Docker context/export check or local macOS runs.

No assertion was removed to obtain a pass. Tests whose historical contract
expected NaN to become a healthy advisory now expect rejection. Transport
drivers supply the new bounded limit/client-ref/journal prerequisites while
retaining exact request/schema, certificate, consent, redirect and failure
expectations. Docker context export checks still verify secret exclusion; their
isolated configuration now preserves PATH-installed Buildx discovery.
The legacy HFT floor tests retain their positive-floor case with a small
positive mass and separately assert that zero mass is rejected/marked invalid.

## Remaining external gates

This source review cannot establish the absence of all vulnerabilities or
production financial correctness. Live accounts, real fills, shared account
limits, external calibration/unit conventions, platform certification, physical
hardware, external storage authenticity and deployment penetration testing were
not exercised. Unsupported/unknown execution states are denied rather than
treated as verified. See [mathematical evidence](../MATHEMATICAL_ENGINE.md).

GitHub reported `main` unprotected. This connection has no branch-protection
mutation capability, the local CLI is unauthenticated, and the available browser
is signed out. Enforcing required CI/review and blocking force pushes/deletion
therefore needs authenticated repository administration. No protection is
claimed to have been enabled. Hosted CI is separate from local evidence and
must complete before merge. This branch is left unmerged.

These changes include intentional compatibility restrictions and a new ledger
hash contract. The changes are staged for reviewed major release v25.0.0, not a drop-in
production patch. Publish only after hosted CI, migration and deployment
validation. Historical reports are preserved.
