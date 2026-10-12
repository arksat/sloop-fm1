class SloopSimulator extends AudioWorkletProcessor {
  constructor() {
    super(); this.ready = false; this.pos = 1024; this.lastDraw = 0;
    this.port.onmessage = ({data}) => { if (data.type === 'load') this.load(data.wasm, data.master); else if (this.ready) this.input(data); };
  }
  load(bytes, master = 700) {
    try {
      /* ui_pages_test.c carries unused file-output helpers. Emscripten keeps
       * their three WASI imports in this standalone module; the simulator never
       * calls them, so harmless no-op bindings keep the browser module pure. */
      const wasi = { fd_close: () => 0, fd_write: () => 0, fd_seek: () => 0 };
      this.w = new WebAssembly.Instance(new WebAssembly.Module(bytes), { wasi_snapshot_preview1: wasi }).exports;
      this.w._initialize?.(); this.w.web_boot(); this.w.web_master(master); this.memory = this.w.memory;
      this.left = new Float32Array(this.memory.buffer, this.w.web_out_l(), 1024);
      this.right = new Float32Array(this.memory.buffer, this.w.web_out_r(), 1024);
      this.ready = true; this.port.postMessage({type: 'ready'}); this.frame(true);
    } catch (error) { this.port.postMessage({type: 'error', message: error.message}); }
  }
  input(m) {
    if (m.type === 'buttons') this.w.web_buttons(m.mask >>> 0);
    if (m.type === 'keys') this.w.web_keys(m.mask >>> 0);
    if (m.type === 'enc') this.w.web_enc(m.role, m.n);
    if (m.type === 'master') this.w.web_master(m.value);
  }
  frame(force = false) {
    const draws = this.w.web_screen_draws();
    if (!force && draws === this.lastDraw) return;
    this.lastDraw = draws;
    const pixels = new Uint16Array(this.memory.buffer, this.w.web_screen(), 240 * 240);
    const copy = pixels.slice();
    this.port.postMessage({type: 'frame', fb: copy, leds: [this.w.web_lit_buttons(), this.w.web_lit_keys(), 0, 0, 0, 0, 0, 0]}, [copy.buffer]);
  }
  process(_inputs, outputs) {
    const [l, r] = outputs[0];
    if (!this.ready) { l.fill(0); r.fill(0); return true; }
    for (let i = 0; i < l.length; i++) {
      if (this.pos >= 1024) { this.w.web_render(1024); this.pos = 0; this.frame(); }
      l[i] = this.left[this.pos]; r[i] = this.right[this.pos]; this.pos++;
    }
    return true;
  }
}
registerProcessor('sloop-simulator', SloopSimulator);
