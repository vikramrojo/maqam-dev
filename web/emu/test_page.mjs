// Node checks for web/emu/index.html: the inline worklet parses, and the pure helpers behave.
// Run: node web/emu/test_page.mjs
import fs from "node:fs";
import vm from "node:vm";
import assert from "node:assert/strict";
import { fileURLToPath } from "node:url";
import path from "node:path";

const html = fs.readFileSync(path.join(path.dirname(fileURLToPath(import.meta.url)), "index.html"), "utf8");
const block = (id, type) => {
  const m = html.match(new RegExp(`<script ${type ? `type="${type}" ` : ""}id="${id}">([\\s\\S]*?)</script>`));
  assert.ok(m, `script #${id} not found`);
  return m[1];
};
let n = 0;
const ok = (name, fn) => { fn(); n++; console.log("ok -", name); };

const pureSrc = block("pure"), workletSrc = block("worklet", "text/worklet");
const P = vm.runInNewContext(pureSrc + ";SloopPure");
const J = (x) => JSON.parse(JSON.stringify(x));   // across the vm realm

ok("worklet source parses (with SloopPure prepended)", () => {
  new vm.Script(pureSrc + "\n" + workletSrc, { filename: "worklet.js" });
});
ok("page script parses", () => {
  const scripts = [...html.matchAll(/<script>([\s\S]*?)<\/script>/g)];
  assert.equal(scripts.length, 1);
  new vm.Script(scripts[0][1]);
});

ok("USB packets: channel messages", () => {
  assert.equal(P.usbPacket([0x90, 60, 100]), 0x64_3c_90_09);
  assert.equal(P.usbPacket([0x80, 60, 0]), 0x00_3c_80_08);
  assert.equal(P.usbPacket([0xB2, 1, 127]), 0x7f_01_b2_0b);
  assert.equal(P.usbPacket([0xA0, 60, 5]), 0x05_3c_a0_0a);
  assert.equal(P.usbPacket([0xC0, 5]), 0x00_05_c0_0c);          // program change: 2 bytes
  assert.equal(P.usbPacket([0xD1, 33]), 0x00_21_d1_0d);         // channel AT: 2 bytes
  assert.equal(P.usbPacket([0xE0, 0, 64]), 0x40_00_e0_0e);
  assert.equal(P.usbPacket([0x99, 36, 127]), 0x7f_24_99_09);    // channel 10 drums
});
ok("USB packets: realtime and system common", () => {
  for (const s of [0xF8, 0xFA, 0xFB, 0xFC, 0xFE, 0xFF]) assert.equal(P.usbPacket([s]), ((s << 8) | 0x0F) >>> 0);
  assert.equal(P.usbPacket([0xF2, 1, 2]), 0x02_01_f2_03);       // song position: CIN 3
  assert.equal(P.usbPacket([0xF1, 0x10]), 0x00_10_f1_02);       // MTC: CIN 2
  assert.equal(P.usbPacket([0xF3, 4]), 0x00_04_f3_02);
  assert.equal(P.usbPacket([0xF6]), 0x00_00_f6_05);
  assert.equal(P.usbPacket([0x90, 60, 100], 1) & 0xFF, 0x19);   // cable in the high nibble
});
ok("USB packets: dropped inputs", () => {
  for (const m of [[0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7], [0xF7], [0x40, 1], [0xF4], [0xF5], []]) assert.equal(P.usbPacket(m), null);
  assert.ok(P.usbPacket([0x90, 60, 100]) >= 0, "unsigned result");
  assert.equal(P.usbPacket([0x90, 127, 127]) > 0, true);
});
ok("SysEx filter", () => {
  assert.equal(P.isSysex([0xF0, 1, 0xF7]), true);
  assert.equal(P.isSysex([0x90, 60, 1]), false);
  assert.equal(P.trsBytes([0xF0, 0xF7]), null);
  assert.deepEqual(J(P.trsBytes(new Uint8Array([0xF8]))), [0xF8]);
  const i = P.sysexInfo([0xF0, 0x22, 0x24, 0x35, 0x7D, 0xF7]);
  assert.equal(i.len, 6);
  assert.equal(i.head, "F0 22 24 35 7D F7");
  assert.match(P.sysexInfo(new Array(20).fill(1)).head, /\.\.\.$/);
});
ok("dirty-bitmap decode", () => {
  assert.deepEqual(J(P.dirtySectors([0, 0, 0, 0, 0, 0, 0, 0])), []);
  assert.deepEqual(J(P.dirtySectors([1, 0, 0, 0, 0, 0, 0, 0])), [0]);
  assert.deepEqual(J(P.dirtySectors([0x80000000, 1, 0, 0, 0, 0, 0, 0x80000000 | 0])), [31, 32, 255]);
  const all = P.dirtySectors(new Uint32Array(8).fill(0xFFFFFFFF));
  assert.equal(all.length, 256);
  assert.equal(all[255], 255);
});
ok("held notes released on disconnect", () => {
  const h = P.createHeld();
  P.trackNote(h, [0x90, 60, 100]); P.trackNote(h, [0x91, 64, 90]); P.trackNote(h, [0x90, 62, 90]);
  P.trackNote(h, [0x90, 62, 0]);                      // velocity 0 releases
  P.trackNote(h, [0x80, 64, 0]);                      // wrong channel: ch1 note 64 not held
  assert.deepEqual(J(P.releaseAll(h)), [[0x80, 60, 0], [0x81, 64, 0]]);
  assert.deepEqual(J(P.releaseAll(h)), []);
});
ok("held notes: running status and drain budget", () => {
  const h = P.createHeld();
  P.trackNote(h, [0x90, 60, 100]); P.trackNote(h, [64, 100]); P.trackNote(h, [67, 100]); P.trackNote(h, [64, 0]);
  assert.deepEqual(J(P.releaseAll(h)), [[0x80, 60, 0], [0x80, 67, 0]]);
  P.trackNote(h, [0xF8]); P.trackNote(h, [70, 90]);              // realtime keeps running status
  assert.deepEqual(J(P.releaseAll(h)), [[0x80, 70, 0]]);
});
ok("MIDI out decode and sysex assembly", () => {
  const d = P.decodeMidiOut(0x64_3c_90_09);
  assert.deepEqual([d.ch, d.type, d.a, d.b], [1, "note on", 60, 100]);
  assert.equal(P.decodeMidiOut(0x00_3c_80_08).type, "note off");
  assert.equal(P.decodeMidiOut(0x00_3c_99_09).type, "note off");  // velocity 0
  assert.equal(P.decodeMidiOut(0xF8_0F).type, "clock");
  const asm = P.createSysexAsm();
  assert.equal(asm((0x04 | (0xF0 << 8) | (0x22 << 16) | (0x24 << 24)) >>> 0), null);
  assert.deepEqual(J(asm((0x06 | (0x35 << 8) | (0xF7 << 16)) >>> 0)), { len: 5, head: "F0 22 24 35 F7" });
});

// Drive the worklet class in a vm with the AudioWorklet globals faked, using the mock device.
{
  let Proc, posted = [];
  const ctx = vm.createContext({
    AudioWorkletProcessor: class { constructor() { this.port = { postMessage: (m) => posted.push(m) }; } },
    registerProcessor: (_n, c) => { Proc = c; }, currentTime: 0, console, WebAssembly, Uint8Array, Uint16Array, Uint32Array, Float32Array, Set, Date,
  });
  vm.runInContext(pureSrc + "\n" + workletSrc, ctx);
  const p = new Proc();
  await p.onMessage({ type: "load", mock: true, flash: [{ index: 3, bytes: new Uint8Array(4096).fill(7) }], master: 500 });
  const ready = posted.find((m) => m.type === "ready");
  assert.equal(ready.version, "SLOOP mock");
  assert.equal(p.ex.web_flash_dirty() > 0, true);
  const out = [new Float32Array(128), new Float32Array(128)];
  const step = () => { ctx.currentTime += 128 / 44100; p.process([], [out]); };
  p.port.onmessage({ data: { type: "keys", mask: 1 << 5 } }); p.port.onmessage({ data: { type: "buttons", mask: 1 << 7 } });
  p.port.onmessage({ data: { type: "midi", route: "usb", packets: [0x64_3c_90_09] } });
  for (let i = 0; i < 400; i++) step();
  p.port.onmessage({ data: { type: "keys", mask: 0 } }); step(); step(); for (let i = 0; i < 400; i++) step();
  { // a burst of 100 USB packets is fed 32 per quantum, in order
    const seen = [], orig = p.ex.web_midi_usb;
    p.ex.web_midi_usb = (x) => { seen.push(x); return orig(x); };
    p.port.onmessage({ data: { type: "midi", route: "usb", packets: Array.from({ length: 100 }, (_, i) => 0x00_00_90_09 | (i << 16)) } });
    p.port.onmessage({ data: { type: "buttons", mask: 0 } });
    step(); assert.equal(seen.length, 32); step(); assert.equal(seen.length, 64); step(); step(); assert.equal(seen.length, 100);
    assert.deepEqual(seen.map((x) => (x >>> 16) & 255), Array.from({ length: 100 }, (_, i) => i));
    assert.equal(p.q.length, 0);
    p.ex.web_midi_usb = orig; n++; console.log("ok - worklet drains MIDI in bounded chunks, in order");
  }
  const frames = posted.filter((m) => m.type === "frame");
  ok("mock worklet: frames carry fb, LEDs, MIDI out, flash", () => {
    assert.ok(frames.length > 10 && frames[0].fb.length === 240 * 240);
    
    const outs = frames.flatMap((f) => f.midiOut || []).map((q) => P.decodeMidiOut(q).type);
    assert.deepEqual(outs, ["note on", "note off"]);
    assert.ok(frames.some((f) => f.ledsKey === 1 << (60 % 27)));
    const fl = frames.flatMap((f) => f.flash || []);
    assert.deepEqual(fl.map((s) => s.index), [0x97]);
    assert.equal(fl[0].bytes.length, 4096);
    assert.equal(frames.filter((f) => f.flash).length, 1, "clean after send");
  });
  p.port.onmessage({ data: { type: "buttons", mask: (1 << 12) | (1 << 13) } }); for (let i = 0; i < 20; i++) step();
  ok("mock worklet: UBOOT combo raises event bit 0", () => {
    assert.ok(posted.filter((m) => m.type === "frame").some((f) => f.events & 1));
  });
}

console.log(`${n} checks passed`);
