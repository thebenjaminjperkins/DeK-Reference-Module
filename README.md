# DeK Reference Module

Reference RP2350 module used for bringing up the DeK protocol.

Current goal:
- SPI communication
- Packet decode
- USB serial monitor diagnostics

Protocol source of truth:
- This repo now consumes the shared protocol implementation from `external/DeK-Protocol`.
- The in-tree `protocol/` directory is no longer the active build input for the firmware target.

Current firmware behavior:
- Runs startup self-tests against the external protocol library for packet encode/decode, HELLO transport flow, and streaming packet reception.
- Exposes the module as an SPI slave on the `pico2_w` default SPI pins: `SCK=18`, `TX=19`, `RX=16`, `CS=17`.
- Accepts inbound protocol packets over SPI, dispatches them in firmware, and currently handles `DEK_MSG_HELLO`.
- Queues a `DEK_MSG_HELLO_ACK` response for valid HELLO packets and shifts idle `0x00` bytes whenever no packet data is queued.
- Reports packet counters, parser errors, and queue status to the VS Code serial monitor over USB.

Bring-up notes:
- Because SPI is full duplex, the module may emit idle `0x00` bytes before a queued response packet appears on MISO.
- Host tooling should scan for the `DK` packet magic bytes at the start of a response instead of assuming the first returned byte begins a packet.
- The serial monitor is now the primary debug surface while the OLED path is offline.
