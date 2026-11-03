# Broker execution integrity hardening

This change strengthens the existing Alpaca endpoint/mode/consent boundary. It
uses synthetic loopback servers only; no live account or order was exercised.

## Directional submission contract and migration

Non-mock `submitOrder` requires all of the following:

- A finite positive quantity and an explicit finite positive `limit_price`.
  Quantity-based market orders are rejected: a reference quote cannot enforce a
  hard fill-price ceiling. The adapter does not silently change order type.
- A validated uppercase symbol, BUY side, and a caller-supplied
  `client_ref` of at most 48 safe identifier characters. Fractional quantities
  are preserved instead of truncating to an integer. Non-mock directional SELL
  is rejected until verified holdings can establish that it reduces exposure;
  a SELL limit price only supplies a minimum sale price and cannot cap short
  exposure. Explicit broker-directed close requests remain separate.
- A finite positive configured dollar cap and a notional quantity × limit
  within that cap. A symbol with any durable reserved order remains blocked for
  new identities pending external position/fill reconciliation.
- An explicit `AlpacaConfig.intent_journal_path`, or `ALPACA_INTENT_JOURNAL` when
  using environment configuration. The store must be a regular private file
  owned by the current user, with one exclusive writer. On POSIX, symlinked,
  multiply linked, group/world-accessible, and locked stores are rejected.

The daemon explicitly chooses BUY LIMIT orders at its supplied current price,
clamps allocation to the configured maximum, and provides a unique client ID.
Generic `routeDecision` only routes a valid finite nonzero decision; fallback,
zero direction, and malformed evidence do not authorize execution. Its default
market request remains simulation-only with this adapter; non-mock callers must
construct an explicitly bounded order instead.

## Durable identity and unknown outcomes

Before any HTTP submission, the adapter appends and fsyncs a reservation that
binds client identity to the canonical request, destination, and account-key
identity. Journal creation also syncs the parent directory on POSIX. The file
is exclusively locked for the lifetime of the adapter; OS locks are released
on process exit. A finite storage limit fails closed rather than discarding old
identities. A chained SHA-256 checksum detects accidental corruption or partial
records; it is not authentication against an attacker who can rewrite the file.

A successful HTTP 200/201 response must contain a nonempty top-level string ID
in a complete valid JSON object. Duplicate keys, malformed JSON, nested IDs,
missing IDs, and explicitly rejected/unsupported statuses never become success.
If status is supplied, supported accepted/new/pending-new/partially-filled/filled
states are recognized. Acknowledged IDs are persisted before being returned.
The same request and client ID return the saved ID without another request,
including after restart. A changed payload using the same ID is denied.

Transport failures, server errors, malformed acknowledgments, or failed
acknowledgment persistence leave the reservation in place and lock submission.
A pending reservation found at startup also locks submission. There is no blind
retry or fabricated `SUBMITTED_SUCCESS`. A fresh client ID cannot bypass a
reserved symbol. The adapter does not automatically free reservations following
cancel or flatten acknowledgments: neither proves a settled position.

There is deliberately no automatic reconciliation or journal-reset API. An
external trusted operator/service must reconcile broker orders, fills, and
positions before authorizing another execution session. Deleting/replacing a
journal without that reconciliation defeats replay protection and is unsupported.
Separate journal paths or independent accounts do not provide a global portfolio
limit; they require an external shared account/position authority.

## Truthful daemon state and validation

A broker ID establishes submission acknowledgment, not a filled position. The
non-mock daemon keeps the order pending and stops further execution until
reconciliation; it never marks a broker-accepted order as a confirmed BUY.
Ambiguous submission is also pending. A subsequent safety fault attempts
cancellation of a known pending order while retaining the pending state; an
unknown order ID requires broker reconciliation. A failed flatten retains prior position
state; non-mock acceptance of a close request remains pending, never a claimed
confirmed FLAT. Mock fills retain their explicit simulator behavior.

Malformed runtime market/advisory evidence, unknown initial/current equity,
invalid CLI numbers, and invalid risk configuration cannot use a fabricated
healthy baseline to continue execution. Account equity is parsed as a complete
finite non-negative scalar with full-input checking. IDs and symbols cannot
inject JSON fields or HTTP path segments. Connection/read/write timeouts are
bounded. Existing destination, TLS verification, live-consent, and redirect
rejection controls remain in force.

## Verification

`tests/test_alpaca_integrity.py` adds adversarial loopback tests for acknowledgment
shape and status, durable cached replay/unknown outcomes, changed payloads,
private exclusive stores/corruption/symlinks, numeric and notional limits,
serialization, account evidence, generic routing, and accepted-versus-filled
daemon state. Existing TLS/authorization test requests now supply a bounded
limit, stable client identity, and isolated journal; their exact request-body
assertion requires those fields.

The new journal's POSIX persistence and locking behavior is tested locally on
macOS. Windows directional submission fails closed because equivalent private-store
ownership/ACL guarantees are not yet verified; mock execution remains available.
No cross-platform durability, filesystem/power-loss certification, actual broker
settlement, price slippage, fees, live trading, or external reconciliation result
is inferred from these software tests.

Actual local evidence (Python 3.12, AppleClang, macOS ARM64):

| Check | Result |
| --- | --- |
| Original HEAD loopback acknowledgment probes | `{}` and a spaced real ID returned fabricated `SUBMITTED_SUCCESS`; an explicitly rejected status returned its ID. |
| Integrity + existing TLS + existing endpoint/consent suites | **189 passed**. |
| New integrity suite alone | **42 passed**. |
| Same 42 cases using the actual driver compiled with ASan/UBSan | **42 passed**, no diagnostics; leak detection disabled on macOS. |
| Relevant `git diff --check` | Passed. |

Sanitizer invocation replaces only the test fixture's executable with the
sanitized build; requests, production sources, and expectations are identical.
