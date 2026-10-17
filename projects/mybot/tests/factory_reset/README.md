# Factory-reset storage regression tests

```sh
bash projects/mybot/tests/factory_reset/run.sh
ASAN_OPTIONS=detect_leaks=0 FACTORY_RESET_TEST_SANITIZERS=1 \
    bash projects/mybot/tests/factory_reset/run.sh
```

This host harness links the actual `bk7259_factory_reset.c` against the pinned
vendor EasyFlash declarations and an in-memory fault-injection implementation.
It does not copy the production reset logic. Only vendor logging, Wi-Fi types,
and generated SDK configuration are stubbed.

Checks cover valid durable intent and boot resume; invalid CRC, metadata,
version, value and read lengths; failed writes and successful-looking writes
with bad readback; refusal to erase without a verified intent; each product
record failing deletion; deletion that reports success but leaves data;
rebuilding intent when final marker deletion actually succeeds but reports an
error; corrupted owned records; repeated begin/retry; and preservation of
unrelated EasyFlash entries. Network cleanup tests check both portal and STA
are attempted and either failure reaches the caller.

The application owner must stop SDK and provisioning writers before calling
clear; the storage module intentionally has no SDK lifecycle dependency.
This suite does not emulate EasyFlash sector GC or physical flash power loss.
Device tests must still cover interrupted reset, held keys, and Wi-Fi teardown.
