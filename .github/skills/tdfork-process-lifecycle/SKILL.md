---
name: tdfork-process-lifecycle
description: 'Own, record, stop, and verify build, test, Telegram, updater, debugger, watcher, and capture processes for serialized work in this checkout.'
---

# Process Lifecycle

This is the single normative source for exclusive checkout ownership, process
tracking, and cleanup.

## Exclusive Ownership

One role at a time may access this checkout. Before mutation or branch switch,
inspect visible processes for this repository and `out`, acquire exclusive
ownership, and ensure no user-owned editor, debugger, build, Telegram instance,
or other process is using them. Unknown processes are ambient and must not be
stopped.

Run finite commands in the foreground by default. Use background execution
only for a process that must remain alive while verified work continues. Record
it immediately in a transient ledger outside the repository.

## Identity And Ledger

Prefer a native process handle. Otherwise record PID, full executable path, and
process start time together because Windows can reuse PIDs. The ledger lives in
a session artifact or temporary directory outside product source and contains:

```text
kind:
owner/session:
native handle or PID/executable/start time:
working directory:
purpose:
owned resource:
output/capture path:
stop result:
verification timestamp:
```

The empty form is:

```text
Processes: <none>
```

Track every session-owned CMake, MSBuild or Ninja, compiler, linker, Qt code
generator, test, Telegram application, updater, crash reporter, debugger,
watcher, log collector, and capture process. A child process belongs to the
session only when it descends from an owned launch or was otherwise identified
before launch with an exact ownership mechanism.

## Waiting And Cleanup

Use bounded waits based on the command's expected behavior. On success,
failure, cancellation, or timeout:

1. Stop accepting new work.
2. Request normal termination for each owned process where possible.
3. Wait a bounded interval and record the result.
4. If escalation is necessary, act only on the exact verified handle or
   PID/executable/start-time identity and its verified owned process tree.
5. Re-enumerate independently and record the verification timestamp.
6. Delete ephemeral logs and captures only after they are no longer needed and
   only from their exact session artifact or temporary path.

Never stop by executable name, broad pattern, port alone, stale PID, or guessed
parentage. Never kill shared toolchain services, installed Telegram clients,
IDE processes, another session, or an unknown process. If a user-owned process
locks an output, report the exact lock and ask the user to close it.

Do not switch branches, hand back, or release checkout ownership while an owned
process or descendant still uses the checkout or build directory. If cleanup
cannot be independently verified, report the task blocked with the unresolved
ledger entry.

Do not place logs, screenshots, recordings, dumps, or process ledgers in
product source. If durable evidence is intentionally committed, it must be an
approved documentation artifact rather than transient capture output.
