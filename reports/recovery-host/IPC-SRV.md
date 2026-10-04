# CTR srv:/session IPC recovery checkpoint

This checkpoint reconstructs the first guest-visible CTR named-port and service-session path.

## SVCs added

- `0x2D ConnectToPort`
- `0x32 SendSyncRequest`

The modified CTR SVC ABI is preserved:

- `ConnectToPort(Handle* out, VAddr name)`: input name pointer in `r1`, Result in `r0`, output handle in `r1`.
- `SendSyncRequest(Handle handle)`: input handle/result in `r0`.

Unknown/unsupported SVCs remain explicit runner stops.

## IPC command buffer

The current thread's command buffer is read/written at:

`TLS + 0x80`

with the CTR size of 0x100 bytes / 64 words.

Recovered IPC encoding includes:

- command ID in bits 31:16
- normal-word count in bits 11:6
- translate-word count in bits 5:0
- move-handle descriptor `0x10`
- copy-handle descriptor
- CallingPid descriptor `0x20`

## Named port

The first named port is `srv:`.

The port path reproduces the CTR distinctions between:

- bad/unknown named port: kernel NotFound
- overlong named-port name: PortNameTooLong
- unknown service requested through srv: ServiceNotRegistered

A successful ConnectToPort creates an ordinary process handle to an HLE client session.

## srv: commands restored

### 0x0001 RegisterClient

Validates the CallingPid descriptor shape and returns a normal success response.

### 0x0005 GetServiceHandle

Parses:

- 8-byte service name
- explicit name length
- flags

and returns either:

- ResultSuccess plus a moved service-session handle, or
- the appropriate srv result code.

The router also retains the historical success behavior for the simple Subscribe /
Unsubscribe / PublishToSubscriber stubs. Other unimplemented srv commands remain
explicit failures.

## Service-session router

`IpcRouter::RegisterService` now allows later HLE services to register an
8-character-or-shorter service name with a request handler.

A returned service handle is a real handle-table object referencing the selected
session handler. A subsequent SendSyncRequest reads the same guest TLS command
buffer, dispatches to that handler, and writes the response back to guest memory.

## End-to-end ROM-free IPC proof

The committed IPC test performs the complete guest-facing chain:

1. writes the string `srv:` into guest memory;
2. executes ConnectToPort through SvcBridge;
3. writes a RegisterClient request into TLS+0x80;
4. executes SendSyncRequest and verifies the response header/result;
5. registers an authored `echo:` service;
6. writes srv:GetServiceHandle for `echo:`;
7. receives and validates the moved session handle;
8. writes a second IPC request into the same TLS command buffer;
9. executes SendSyncRequest through the returned `echo:` session;
10. verifies that the service response reached guest memory.

Additional tests cover unknown ports, overlong port names, unregistered services,
and invalid session handles.

## CI

GitHub Actions run **37166400462** passes with:

- GCC: PASS
- Clang: PASS

Both compilers pass:

- ctr_kernel_test
- ctr_runner_test
- ctr_ipc_test

## Current boundary

This establishes the reusable named-port/session mechanism required by the game.
It does not yet recreate the title-specific startup services such as filesystem,
graphics, HID, DSP, APT, etc.

The surviving Recovery F/J documentation proves that the old runner eventually
had working service behavior through interactive gameplay, but the status files do
not preserve a complete ordered IPC/service trace. The next recovery step is
therefore to drive the real reconstructed game AOT tree until its first
unregistered service/request and implement services in observed order rather than
fabricating a broad service set.
