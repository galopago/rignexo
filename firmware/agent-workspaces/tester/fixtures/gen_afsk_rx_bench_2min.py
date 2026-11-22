"""Generate ~2 min Bell-202 AFSK WAV + human/agent-readable manifest."""
import math, struct, wave, json
from pathlib import Path

OUT_DIR = Path("/home/libertad/Desktop/repos/rignexo/firmware/agent-workspaces/tester/fixtures")
WAV_PATH = OUT_DIR / "afsk_rx_bench_2min.wav"
MANIFEST_PATH = OUT_DIR / "afsk_rx_bench_2min_manifest.md"
JSON_PATH = OUT_DIR / "afsk_rx_bench_2min_manifest.json"

SR = 48000
BAUD = 1200
SPB = SR // BAUD  # 40

def crc16_x25(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = ((crc >> 1) ^ 0x8408) if (crc & 1) else (crc >> 1)
    return (~crc) & 0xFFFF

def build_hdlc_bits(payload: bytes, open_flags: int, close_flags: int):
    bits, ones = [], 0
    def push(bit, stuff):
        nonlocal ones
        bits.append(bit & 1)
        if not stuff:
            ones = 0
            return
        if bit:
            ones += 1
            if ones == 5:
                bits.append(0)
                ones = 0
        else:
            ones = 0
    def byte_raw(b):
        for i in range(8):
            push((b >> i) & 1, False)
    def byte_stuffed(b):
        for i in range(8):
            push((b >> i) & 1, True)
    for _ in range(open_flags):
        byte_raw(0x7E)
    fcs = crc16_x25(payload)
    body = payload + bytes([fcs & 0xFF, (fcs >> 8) & 0xFF])
    for b in body:
        byte_stuffed(b)
    for _ in range(close_flags):
        byte_raw(0x7E)
    return bits, fcs

def modulate(bits, amp=0.90, invert_pcm=False, swap_tones=False, invert_nrzi=False):
    mark = True
    phase = 0.0
    samples = []
    for bit in bits:
        if invert_nrzi:
            if bit == 1:
                mark = not mark
        else:
            if bit == 0:
                mark = not mark
        if swap_tones:
            hz = 2200.0 if mark else 1200.0
        else:
            hz = 1200.0 if mark else 2200.0
        for _ in range(SPB):
            phase += 2 * math.pi * hz / SR
            if phase > 2 * math.pi:
                phase -= 2 * math.pi
            s = int(amp * 32767 * math.sin(phase))
            if invert_pcm:
                s = -s
            samples.append(max(-32767, min(32767, s)))
    return samples

def silence(ms):
    return [0] * int(SR * ms / 1000)

def tone_burst(hz, ms, amp=0.5):
    n = int(SR * ms / 1000)
    return [int(amp * 32767 * math.sin(2 * math.pi * hz * i / SR)) for i in range(n)]

# ---- Packet plan (~2 minutes) ----
# Each entry: id, payload, open_flags, close_flags, amp, invert_pcm, swap_tones, invert_nrzi, gap_ms_after, note
packets = []

def add(**kw):
    packets.append(kw)

# Warm-up markers (not AX.25) — for ear / scope sync
# Then many nominal packets, then stress variants

# Section A: nominal short payloads, long preamble (like TXDELAY~500ms+ flags)
for i in range(1, 11):
    add(
        id=f"A{i:02d}",
        section="A_nominal",
        payload=f"RGNX-A-{i:02d}".encode("ascii"),
        open_flags=60,
        close_flags=8,
        amp=0.90,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=400,
        note="nominal NRZI/tones; ~500ms preamble; expect KISS DATA exact payload",
    )

# Section B: longer payloads
for i in range(1, 6):
    body = f"RGNX-B-{i:02d}-" + ("X" * (20 * i))
    add(
        id=f"B{i:02d}",
        section="B_long",
        payload=body.encode("ascii"),
        open_flags=60,
        close_flags=8,
        amp=0.90,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=500,
        note=f"longer payload {len(body)} bytes; stress HDLC length/FCS",
    )

# Section C: short preamble (harder for PLL)
for i in range(1, 6):
    add(
        id=f"C{i:02d}",
        section="C_short_preamble",
        payload=f"RGNX-C-{i:02d}".encode("ascii"),
        open_flags=15,
        close_flags=5,
        amp=0.90,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=400,
        note="short preamble (~100ms); harder bit sync",
    )

# Section D: very long preamble
for i in range(1, 4):
    add(
        id=f"D{i:02d}",
        section="D_long_preamble",
        payload=f"RGNX-D-{i:02d}".encode("ascii"),
        open_flags=120,
        close_flags=10,
        amp=0.90,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=400,
        note="long preamble (~1s flags); helps weak PLL / timer skew",
    )

# Section E: amplitude variants
for i, amp in enumerate([0.35, 0.55, 0.75, 0.95], start=1):
    add(
        id=f"E{i:02d}",
        section="E_amplitude",
        payload=f"RGNX-E-AMP{amp:.2f}".encode("ascii"),
        open_flags=60,
        close_flags=8,
        amp=amp,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=400,
        note=f"amplitude {amp:.2f} full-scale",
    )

# Section F: polarity / mapping variants (diagnose disc/NRZI)
add(id="F01", section="F_polarity", payload=b"RGNX-F-INV-PCM", open_flags=60, close_flags=8,
    amp=0.90, invert_pcm=True, swap_tones=False, invert_nrzi=False, gap_ms_after=400,
    note="invert PCM (phase flip); NRZI differential should still OK if disc clean")
add(id="F02", section="F_polarity", payload=b"RGNX-F-SWAP-TONES", open_flags=60, close_flags=8,
    amp=0.90, invert_pcm=False, swap_tones=True, invert_nrzi=False, gap_ms_after=400,
    note="swap 1200/2200 mapping; NRZI should still OK")
add(id="F03", section="F_polarity", payload=b"RGNX-F-INV-NRZI", open_flags=60, close_flags=8,
    amp=0.90, invert_pcm=False, swap_tones=False, invert_nrzi=True, gap_ms_after=400,
    note="WRONG NRZI (1=transition); expect FAIL unless firmware inverted")
add(id="F04", section="F_polarity", payload=b"RGNX-F-INV-BOTH", open_flags=60, close_flags=8,
    amp=0.90, invert_pcm=True, swap_tones=True, invert_nrzi=False, gap_ms_after=400,
    note="invert PCM + swap tones")

# Section G: binary / escapes-relevant bytes (KISS host side later)
add(id="G01", section="G_binary", payload=bytes([0x00, 0x01, 0x02, 0xC0, 0xDB, 0xFF, 0x7E]),
    open_flags=60, close_flags=8, amp=0.90, invert_pcm=False, swap_tones=False, invert_nrzi=False,
    gap_ms_after=400, note="binary incl 0xC0/0xDB/0x7E; air FCS separate from KISS escape")
add(id="G02", section="G_binary", payload=bytes(range(32)),
    open_flags=60, close_flags=8, amp=0.90, invert_pcm=False, swap_tones=False, invert_nrzi=False,
    gap_ms_after=400, note="0x00..0x1F")

# Section H: repeated identical (check drops / queue)
for i in range(1, 6):
    add(
        id=f"H{i:02d}",
        section="H_repeat",
        payload=b"RGNX-H-REPEAT",
        open_flags=50,
        close_flags=6,
        amp=0.90,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=250,
        note="identical payload burst; tight spacing",
    )

# Section I: end markers
for i in range(1, 4):
    add(
        id=f"I{i:02d}",
        section="I_end",
        payload=f"RGNX-END-{i}".encode("ascii"),
        open_flags=80,
        close_flags=10,
        amp=0.92,
        invert_pcm=False,
        swap_tones=False,
        invert_nrzi=False,
        gap_ms_after=600,
        note="end-of-file markers",
    )

# Build audio timeline
# v2: fill nearly the full ~2 min with packet cycles — DO NOT pad with long
# silence (v1 mistake: ~40 s of packets + ~80 s dead air broke loop benches).
TARGET_CONTENT_S = 118.0  # leave room for end beep + short tail
CYCLE_GAP_MS = 800  # pause between full A..I cycles

samples = []
samples += silence(500)
samples += tone_burst(1000, 300, 0.4)  # ear sync beep
samples += silence(700)

manifest_entries = []
cycle = 0

while (len(samples) / SR) < TARGET_CONTENT_S:
    cycle += 1
    for p in packets:
        if (len(samples) / SR) >= TARGET_CONTENT_S:
            break
        bits, fcs = build_hdlc_bits(p["payload"], p["open_flags"], p["close_flags"])
        audio = modulate(
            bits,
            amp=p["amp"],
            invert_pcm=p["invert_pcm"],
            swap_tones=p["swap_tones"],
            invert_nrzi=p["invert_nrzi"],
        )
        start = len(samples) / SR
        samples.extend(audio)
        end = len(samples) / SR
        samples.extend(silence(p["gap_ms_after"]))
        entry = {
            "id": f"{p['id']}_c{cycle}",
            "base_id": p["id"],
            "cycle": cycle,
            "section": p["section"],
            "t_start_s": round(start, 3),
            "t_end_s": round(end, 3),
            "duration_s": round(end - start, 3),
            "payload_ascii": p["payload"].decode("ascii", errors="replace"),
            "payload_hex": p["payload"].hex(),
            "payload_len": len(p["payload"]),
            "fcs_hex": f"{fcs:04x}",
            "open_flags": p["open_flags"],
            "close_flags": p["close_flags"],
            "amp": p["amp"],
            "invert_pcm": p["invert_pcm"],
            "swap_tones": p["swap_tones"],
            "invert_nrzi": p["invert_nrzi"],
            "gap_ms_after": p["gap_ms_after"],
            "expect_kiss": not p["invert_nrzi"],  # F03 intentionally wrong NRZI
            "note": p["note"],
        }
        if p["id"] == "F03":
            entry["expect_kiss"] = False
            entry["expect_note"] = "EXPECT NO DECODE (inverted NRZI on purpose)"
        else:
            entry["expect_note"] = (
                "EXPECT KISS DATA payload exactly (no FCS)"
                if entry["expect_kiss"]
                else "variant"
            )
        manifest_entries.append(entry)
    if (len(samples) / SR) < TARGET_CONTENT_S:
        samples.extend(silence(CYCLE_GAP_MS))

samples.extend(silence(400))
samples.extend(tone_burst(1500, 400, 0.4))
samples.extend(silence(300))

total_s = len(samples) / SR

# Hard gate: reject v1 failure mode (short burst + long dead pad).
# Inter-packet gaps are normal; a contiguous silence run >>1 s after start is not.
def _silence_runs(buf, thr=200, win=None):
    if win is None:
        win = SR // 20  # 50 ms
    runs = []
    run_t0 = None
    for i in range(0, len(buf), win):
        chunk = buf[i : i + win]
        t0 = i / SR
        quiet = (not chunk) or max(abs(x) for x in chunk) < thr
        if quiet:
            if run_t0 is None:
                run_t0 = t0
        elif run_t0 is not None:
            runs.append((run_t0, t0))
            run_t0 = None
    if run_t0 is not None:
        runs.append((run_t0, len(buf) / SR))
    return runs

silence_runs = _silence_runs(samples)
# Ignore lead-in before first packet (~1.5 s) when scoring "dead pad"
post_start_runs = [(a, b) for a, b in silence_runs if b > 2.0]
max_gap = max((b - a) for a, b in post_start_runs) if post_start_runs else 0.0
last_pkt_end = manifest_entries[-1]["t_end_s"] if manifest_entries else 0.0
air_s = sum(e["duration_s"] for e in manifest_entries)
# "Span coverage": first packet → last packet should fill most of the file
span = last_pkt_end - manifest_entries[0]["t_start_s"] if manifest_entries else 0.0
span_frac = span / total_s if total_s > 0 else 0.0
active_s = air_s  # on-air AFSK (excludes gaps); reported for humans
active_frac = span_frac

if total_s < 110.0 or last_pkt_end < 110.0 or max_gap > 2.5 or span_frac < 0.90:
    raise SystemExit(
        f"REFUSING to write fixture: dur={total_s:.1f}s last_pkt={last_pkt_end:.1f}s "
        f"max_silence_gap={max_gap:.1f}s span_frac={100 * span_frac:.1f}% "
        f"air={air_s:.1f}s — need packets through ~2 min, no long dead pad"
    )

# Write WAV
with wave.open(str(WAV_PATH), "w") as w:
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(SR)
    w.writeframes(b"".join(struct.pack("<h", s) for s in samples))

# JSON for agent
meta = {
    "file": str(WAV_PATH.name),
    "format": "WAV RIFF, PCM S16LE, mono, 48000 Hz",
    "duration_s": round(total_s, 3),
    "fixture_version": 2,
    "design_note": (
        "v2 fills ~2 min with repeated A..I cycles. Generator refuses to write "
        "if effective audio <85% (v1 mistake: ~40 s packets + ~80 s silence)."
    ),
    "on_air_afs_s": round(air_s, 3),
    "packet_span_s": round(span, 3),
    "packet_span_frac": round(span_frac, 4),
    "max_silence_gap_s": round(max_gap, 3),
    "modulation": "Bell-202 AFSK 1200/2200 baud 1200, NRZI 0=transition (except F03)",
    "hdlc": "flags 0x7E, bit-stuff, FCS CRC-16/X.25 LE appended on air",
    "kiss_expectation": "TNC should emit KISS DATA with payload bytes only (no FCS)",
    "sync_beep_start_s": 0.5,
    "sync_beep_end_s": round(0.5 + 0.3, 3),
    "cycles": cycle,
    "packet_count": len(manifest_entries),
    "packets": manifest_entries,
}
JSON_PATH.write_text(json.dumps(meta, indent=2), encoding="utf-8")

# Markdown manifest (easy for human + agent)
lines = []
lines.append("# AFSK RX bench WAV manifest")
lines.append("")
lines.append(f"- **File:** `{WAV_PATH.name}`")
lines.append(f"- **Path:** `agent-workspaces/tester/fixtures/{WAV_PATH.name}`")
lines.append(f"- **Format:** PCM S16LE mono **48000 Hz**")
lines.append(f"- **Duration:** **{total_s:.1f} s** (~{total_s/60:.2f} min)")
lines.append(f"- **Fixture version:** **2** (full-duration packet cycles)")
lines.append(f"- **Packets:** {len(manifest_entries)} across **{cycle}** cycle(s)")
lines.append("- **Air format:** Bell-202 AFSK, HDLC flags + stuff + CRC-16/X.25 (LE) FCS")
lines.append("- **KISS expect:** DATA port0, **payload only** (firmware strips FCS)")
lines.append("- **Lead-in:** silence + **1000 Hz beep 300 ms** @ t≈0.5 s (ear sync)")
lines.append("- **Tail:** **1500 Hz beep 400 ms** near end")
lines.append(
    f"- **Packet span:** {manifest_entries[0]['t_start_s']:.1f}s → "
    f"{last_pkt_end:.1f}s ({100 * span_frac:.0f}% of file); "
    f"on-air AFSK **{air_s:.1f} s**; max silence gap **{max_gap:.2f} s**"
)
lines.append(
    "- **Gate:** refuse write if last packet <110 s or silence gap >2.5 s "
    "(v1 dead-pad bug)"
)

lines.append("")
lines.append("## How to use")
lines.append("")
lines.append("1. Play this WAV from an **external** player into the TNC audio input path.")
lines.append("2. TNC in mode 4; host CDC open; log KISS DATA payloads.")
lines.append("3. Match received payloads to the table (`payload_ascii` / `payload_hex`).")
lines.append("4. **F03** is intentionally wrong NRZI → should **not** decode.")
lines.append("")
lines.append("## Packet table")
lines.append("")
lines.append("| ID | t_start | t_end | section | payload | flags open/close | amp | variants | expect |")
lines.append("| --- | ---: | ---: | --- | --- | --- | ---: | --- | --- |")
for e in manifest_entries:
    var = []
    if e["invert_pcm"]:
        var.append("inv_pcm")
    if e["swap_tones"]:
        var.append("swap")
    if e["invert_nrzi"]:
        var.append("inv_nrzi")
    var_s = ",".join(var) if var else "—"
    exp = "YES" if e["expect_kiss"] else "NO"
    pay = e["payload_ascii"].replace("|", "/")
    if len(pay) > 28:
        pay = pay[:25] + "..."
    lines.append(
        f"| {e['id']} | {e['t_start_s']:.2f} | {e['t_end_s']:.2f} | {e['section']} | `{pay}` | "
        f"{e['open_flags']}/{e['close_flags']} | {e['amp']:.2f} | {var_s} | {exp} |"
    )

lines.append("")
lines.append("## Per-packet detail")
lines.append("")
for e in manifest_entries:
    lines.append(f"### {e['id']} — {e['section']}")
    lines.append("")
    lines.append(f"- **Time:** {e['t_start_s']:.3f}s → {e['t_end_s']:.3f}s ({e['duration_s']:.3f}s air)")
    lines.append(f"- **Payload ASCII:** `{e['payload_ascii']}`")
    lines.append(f"- **Payload hex:** `{e['payload_hex']}`")
    lines.append(f"- **Len:** {e['payload_len']} ; **FCS (on air):** `0x{e['fcs_hex']}`")
    lines.append(f"- **Flags:** open={e['open_flags']} close={e['close_flags']}")
    lines.append(
        f"- **Mod:** amp={e['amp']}, invert_pcm={e['invert_pcm']}, "
        f"swap_tones={e['swap_tones']}, invert_nrzi={e['invert_nrzi']}"
    )
    lines.append(f"- **Expect:** {e['expect_note']}")
    lines.append(f"- **Note:** {e['note']}")
    lines.append("")

MANIFEST_PATH.write_text("\n".join(lines) + "\n", encoding="utf-8")

print(f"WAV:  {WAV_PATH} ({WAV_PATH.stat().st_size} bytes, {total_s:.2f}s)")
print(f"MD:   {MANIFEST_PATH}")
print(f"JSON: {JSON_PATH}")
print(f"packets: {len(manifest_entries)}")