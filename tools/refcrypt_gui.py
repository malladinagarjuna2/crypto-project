"""Refractive Cipher - interactive simulation GUI.

Run from the project folder after building (mingw32-make all):
    python tools/refcrypt_gui.py

All cryptography is done by the real C programs (refcrypt.exe / attack.exe);
this window only drives them and visualises what they report.
"""
import csv
import math
import os
import queue
import random
import re
import shutil
import subprocess
import tempfile
import threading
import tkinter as tk
from pathlib import Path
from tkinter import filedialog, messagebox, ttk

import matplotlib
import numpy as np

matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg  # noqa: E402
from matplotlib.figure import Figure  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
SUFFIX = ".exe" if os.name == "nt" else ""
REFCRYPT = ROOT / f"refcrypt{SUFFIX}"
ATTACK = ROOT / f"attack{SUFFIX}"
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)
GRID = 16
N_MIN, N_MAX = 1.0, 2.417
MONO = ("Consolas", 10)
EVENT_STYLE = {            # colour, marker, size
    "start":   ("#2ca02c", "o", 9),
    "refract": ("#333333", ".", 7),
    "tir":     ("#d62728", "X", 9),
    "wall":    ("#ff7f0e", "s", 6),
}


# ============================================================================
# Engine: thin wrapper around the C command-line tools
# ============================================================================
class Engine:
    def __init__(self):
        self.tmp = Path(tempfile.mkdtemp(prefix="refcrypt_gui_"))
        self.key = self.tmp / "current.key"
        self._grid = None

    def _run(self, *args, exe=REFCRYPT):
        return subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True,
                              creationflags=NO_WINDOW)

    @staticmethod
    def _check(proc):
        if proc.returncode != 0:
            raise RuntimeError((proc.stderr or proc.stdout or "command failed").strip())

    # ---- keys ----
    def new_key(self, path=None):
        path = Path(path or self.key)
        self._check(self._run("keygen", path))
        if path == self.key:
            self._grid = None
        return path

    def load_key(self, src):
        data = Path(src).read_bytes()
        if len(data) != 32:
            raise ValueError(f"key file must be exactly 32 bytes (this one is {len(data)})")
        self.key.write_bytes(data)
        self._grid = None

    def save_key(self, dst):
        shutil.copyfile(self.key, dst)

    def fingerprint(self):
        b = self.key.read_bytes()
        return f"{b[:4].hex()}…{b[-4:].hex()}"

    # ---- crystal / rays / rounds ----
    def grid(self):
        if self._grid is None:
            out = self.tmp / "grid.csv"
            self._check(self._run("grid", self.key, out))
            data = np.loadtxt(out, delimiter=",", skiprows=1)
            g = np.zeros((GRID, GRID, GRID))
            g[data[:, 0].astype(int), data[:, 1].astype(int), data[:, 2].astype(int)] = data[:, 3]
            self._grid = g                     # indexed [z][y][x]
        return self._grid

    def trace(self, hex_in):
        out = self.tmp / "ray.csv"
        proc = self._run("trace", self.key, hex_in, out)
        self._check(proc)
        with open(out, newline="") as f:
            rows = list(csv.DictReader(f))
        pts = [{"p": np.array([float(r["x"]), float(r["y"]), float(r["z"])]),
                "d": np.array([float(r["dx"]), float(r["dy"]), float(r["dz"])]),
                "event": r["event"]} for r in rows]
        m = re.search(r"output 0x([0-9a-fA-F]+)", proc.stderr)
        return pts, (m.group(1) if m else "?")

    def rounds(self, block: bytes):
        proc = self._run("rounds", self.key, block.hex())
        self._check(proc)
        rounds, out, dec = [], None, None
        for r in list(csv.reader(proc.stdout.splitlines()))[1:]:
            if r[0] == "out":
                out = (int(r[1], 16), int(r[2], 16))
            elif r[0] == "dec":
                dec = (int(r[1], 16), int(r[2], 16), r[3] == "ok")
            else:
                rounds.append({"i": int(r[0]), "L": int(r[1], 16), "R": int(r[2], 16),
                               "fin": int(r[3], 16), "f": int(r[4], 16)})
        return rounds, out, dec

    # ---- files ----
    def encrypt(self, data: bytes) -> bytes:
        src, dst = self.tmp / "pt.bin", self.tmp / "ct.enc"
        src.write_bytes(data)
        self._check(self._run("enc", self.key, src, dst))
        return dst.read_bytes()

    def decrypt(self, blob: bytes, key=None):
        src, dst = self.tmp / "in.enc", self.tmp / "out.bin"
        if dst.exists():
            dst.unlink()
        src.write_bytes(blob)
        proc = self._run("dec", key or self.key, src, dst)
        if proc.returncode == 0:
            return True, dst.read_bytes()
        msg = proc.stderr.strip()
        return False, msg.split(": ", 1)[-1] if str(src) in msg else msg

    def cleanup(self):
        shutil.rmtree(self.tmp, ignore_errors=True)


# ============================================================================
# helpers
# ============================================================================
def cell_of(point):
    return np.clip(np.floor(point).astype(int), 0, GRID - 1)


def n_at(grid, c):
    return grid[c[2], c[1], c[0]]


def popcount(v):
    return bin(v).count("1")


def explain_event(pts, k, grid):
    """Human-readable physics for event k of a traced path."""
    pt = pts[k]
    ev = pt["event"]
    if ev == "start":
        return f"enters front face at x={pt['p'][0]:.2f}, y={pt['p'][1]:.2f}"
    prev = pts[k - 1]
    d_in = prev["d"]
    c_in = cell_of((prev["p"] + pt["p"]) / 2)
    axis = None
    for a in range(3):
        if d_in[a] == 0:
            continue
        face = c_in[a] + (1 if d_in[a] > 0 else 0)
        if abs(pt["p"][a] - face) < 1e-3:
            axis = a
            break
    if axis is None:
        axis = int(np.argmax(np.abs(d_in)))
    theta = math.degrees(math.acos(min(1.0, abs(d_in[axis]))))
    ax_name = "xyz"[axis]
    n1 = n_at(grid, c_in)
    if ev == "wall":
        return f"hits outer {ax_name}-wall at θ={theta:.1f}° → mirror bounce"
    c_out = c_in.copy()
    c_out[axis] += 1 if d_in[axis] > 0 else -1
    n2 = n_at(grid, cell_of(c_out + 0.5))
    if ev == "refract":
        s = min(1.0, n1 / n2 * math.sin(math.radians(theta)))
        return (f"{ax_name}-face  n {n1:.3f}→{n2:.3f}  "
                f"θ₁={theta:.1f}° → θ₂={math.degrees(math.asin(s)):.1f}°")
    crit = math.degrees(math.asin(n2 / n1)) if n1 > n2 else 90.0
    return f"{ax_name}-face  n {n1:.3f}→{n2:.3f}  θ₁={theta:.1f}° > θc={crit:.1f}° → TIR"


def setup_axes3d(ax, title):
    ax.set_xlim(0, GRID); ax.set_ylim(0, GRID); ax.set_zlim(0, GRID)
    ax.set_xlabel("x"); ax.set_ylabel("y"); ax.set_zlabel("z (depth)")
    ax.set_box_aspect((1, 1, 1))
    ax.set_title(title, fontsize=10)


def draw_crystal(ax, grid, alpha=0.10):
    z, y, x = np.indices(grid.shape)
    sc = ax.scatter(x.ravel() + 0.5, y.ravel() + 0.5, z.ravel() + 0.5, c=grid.ravel(),
                    cmap="viridis", vmin=N_MIN, vmax=N_MAX, s=6, alpha=alpha, depthshade=False)
    return sc


# ============================================================================
# Tab 1: animated 3D ray simulation
# ============================================================================
class RayTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master)
        self.app, self.eng = app, app.eng
        self.pts, self.out, self.prev_pts, self.prev_out = [], None, None, None
        self.frames, self.fi, self.logged = [], 0, -1
        self.playing, self.job = False, None

        left = ttk.Frame(self)
        left.pack(side="left", fill="both", expand=True)
        right = ttk.Frame(self, padding=10)
        right.pack(side="right", fill="y")

        self.fig = Figure(figsize=(7.2, 6.4), dpi=100)
        self.ax = self.fig.add_subplot(projection="3d")
        self.canvas = FigureCanvasTkAgg(self.fig, master=left)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)

        ttk.Label(right, text="Ray input (64-bit hex)", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        self.in_var = tk.StringVar(value="0123456789abcdef")
        ttk.Entry(right, textvariable=self.in_var, width=22, font=MONO).pack(anchor="w", pady=3)
        row = ttk.Frame(right); row.pack(anchor="w", pady=2)
        ttk.Button(row, text="Random", command=self.random_input).pack(side="left")
        ttk.Button(row, text="Flip 1 bit & compare", command=self.flip_bit).pack(side="left", padx=4)
        ttk.Button(right, text="▶  Launch ray", command=self.launch).pack(fill="x", pady=(8, 2))

        row = ttk.Frame(right); row.pack(fill="x", pady=2)
        self.play_btn = ttk.Button(row, text="⏸ Pause", command=self.toggle_play, width=9)
        self.play_btn.pack(side="left")
        ttk.Button(row, text="⏭ Next event", command=self.step_event).pack(side="left", padx=3)
        ttk.Button(row, text="⏩ End", command=self.finish, width=6).pack(side="left")

        ttk.Label(right, text="Speed").pack(anchor="w", pady=(8, 0))
        self.speed = tk.IntVar(value=3)
        ttk.Scale(right, from_=1, to=12, variable=self.speed, orient="horizontal").pack(fill="x")
        self.show_crystal = tk.BooleanVar(value=True)
        self.show_prev = tk.BooleanVar(value=True)
        ttk.Checkbutton(right, text="Show crystal", variable=self.show_crystal,
                        command=self.redraw).pack(anchor="w", pady=(6, 0))
        ttk.Checkbutton(right, text="Show previous ray (grey)", variable=self.show_prev,
                        command=self.redraw).pack(anchor="w")

        self.now_lbl = ttk.Label(right, text="", wraplength=330, foreground="#1f4e79",
                                 font=("Segoe UI", 10, "bold"))
        self.now_lbl.pack(anchor="w", pady=(10, 2))
        self.out_lbl = ttk.Label(right, text="", font=MONO)
        self.out_lbl.pack(anchor="w")
        self.cmp_lbl = ttk.Label(right, text="", wraplength=330)
        self.cmp_lbl.pack(anchor="w", pady=(2, 6))

        ttk.Label(right, text="Event log", font=("Segoe UI", 10, "bold")).pack(anchor="w")
        lf = ttk.Frame(right); lf.pack(fill="both", expand=True)
        self.log = tk.Listbox(lf, font=("Consolas", 9), width=52, height=18)
        sb = ttk.Scrollbar(lf, command=self.log.yview)
        self.log.configure(yscrollcommand=sb.set)
        self.log.pack(side="left", fill="both", expand=True)
        sb.pack(side="right", fill="y")

        self.after(200, self.launch)

    # ---- input helpers ----
    def random_input(self):
        self.in_var.set(f"{random.getrandbits(64):016x}")
        self.launch()

    def flip_bit(self):
        v = self._parse()
        if v is None:
            return
        bit = random.randrange(64)
        self.in_var.set(f"{v ^ (1 << bit):016x}")
        self.launch(note=f"flipped input bit {bit}")

    def _parse(self):
        s = self.in_var.get().strip().lower().removeprefix("0x")
        if not re.fullmatch(r"[0-9a-f]{1,16}", s):
            messagebox.showerror("Invalid input", "Enter up to 16 hex digits (0-9, a-f).")
            return None
        return int(s, 16)

    # ---- simulation ----
    def launch(self, note=None):
        v = self._parse()
        if v is None:
            return
        try:
            pts, out = self.eng.trace(f"{v:016x}")
        except RuntimeError as e:
            messagebox.showerror("trace failed", str(e))
            return
        if self.pts:
            self.prev_pts, self.prev_out = self.pts, self.out
        self.pts, self.out = pts, out
        self.frames = []
        for i in range(len(pts) - 1):
            seg = float(np.linalg.norm(pts[i + 1]["p"] - pts[i]["p"]))
            sub = max(2, int(seg * 5))
            self.frames += [(i, s / sub) for s in range(sub)]
        self.frames.append((len(pts) - 1, 0.0))
        self.fi, self.logged = 0, -1
        self.log.delete(0, "end")
        self.out_lbl.config(text="F output: (ray still travelling…)")
        if self.prev_out and note:
            self.cmp_lbl.config(text=f"{note}: waiting for the new ray to exit…")
        elif not note:
            self.cmp_lbl.config(text="")
        self._note = note
        self.redraw()
        self.playing = True
        self.play_btn.config(text="⏸ Pause")
        self._schedule()

    def redraw(self):
        """Rebuild the static scene (crystal, previous ray) and artists."""
        ax = self.ax
        ax.cla()
        setup_axes3d(ax, "Photon travelling through the secret 16×16×16 crystal")
        if self.show_crystal.get():
            draw_crystal(ax, self.eng.grid())
        if self.show_prev.get() and self.prev_pts:
            pp = np.array([q["p"] for q in self.prev_pts])
            ax.plot(pp[:, 0], pp[:, 1], pp[:, 2], color="#999999", lw=1, alpha=0.7, label="previous ray")
        (self.trail,) = ax.plot([], [], [], color="royalblue", lw=2, label="current ray")
        self.ev_lines = {}
        for ev, (col, mk, ms) in EVENT_STYLE.items():
            (self.ev_lines[ev],) = ax.plot([], [], [], ls="none", marker=mk, color=col, ms=ms, label=ev)
        (self.photon,) = ax.plot([], [], [], ls="none", marker="o", ms=13, mfc="#ffe066", mec="black")
        ax.legend(loc="upper left", fontsize=8)
        if self.frames:
            self.render()
        else:
            self.canvas.draw_idle()

    def render(self):
        i, frac = self.frames[self.fi]
        pts = self.pts
        p0 = pts[i]["p"]
        pos = p0 if i == len(pts) - 1 else p0 + (pts[i + 1]["p"] - p0) * frac
        trail = np.array([q["p"] for q in pts[:i + 1]] + [pos])
        self.trail.set_data_3d(trail[:, 0], trail[:, 1], trail[:, 2])
        self.photon.set_data_3d([pos[0]], [pos[1]], [pos[2]])
        for ev, line in self.ev_lines.items():
            sel = np.array([q["p"] for q in pts[:i + 1] if q["event"] == ev]).reshape(-1, 3)
            line.set_data_3d(sel[:, 0], sel[:, 1], sel[:, 2])
        grid = self.eng.grid()
        for k in range(self.logged + 1, i + 1):
            text = explain_event(pts, k, grid)
            ev = pts[k]["event"]
            self.log.insert("end", f"{k:2d} {ev.upper():7s} {text}")
            self.log.itemconfig("end", foreground=EVENT_STYLE[ev][0])
            self.log.see("end")
            self.now_lbl.config(text=f"Event {k}/{len(pts) - 1}: {ev.upper()}\n{text}")
        self.logged = max(self.logged, i)
        if self.fi == len(self.frames) - 1:
            self._finished()
        self.canvas.draw_idle()

    def _finished(self):
        counts = {e: sum(q["event"] == e for q in self.pts) for e in ("refract", "tir", "wall")}
        self.now_lbl.config(text=f"Ray exited after {len(self.pts) - 1} events: "
                                 f"{counts['refract']} refractions, {counts['tir']} TIR, "
                                 f"{counts['wall']} wall bounces")
        self.out_lbl.config(text=f"F output: 0x{self.out}")
        if self.prev_out and self._note:
            d = popcount(int(self.out, 16) ^ int(self.prev_out, 16))
            self.cmp_lbl.config(text=f"{self._note} → {d}/64 output bits changed "
                                     f"({d / 64:.0%}; ideal ≈ 50%). The grey path is the old ray.")
        self.playing = False
        self.play_btn.config(text="▶ Play")

    def _schedule(self):
        if self.job:
            self.after_cancel(self.job)
        self.job = self.after(40, self._tick)

    def _tick(self):
        self.job = None
        if not self.playing or not self.frames:
            return
        self.fi = min(len(self.frames) - 1, self.fi + int(self.speed.get()))
        self.render()
        if self.playing:
            self._schedule()

    def toggle_play(self):
        if not self.frames:
            return
        if self.fi >= len(self.frames) - 1:
            self.fi, self.logged = 0, -1
            self.log.delete(0, "end")
        self.playing = not self.playing
        self.play_btn.config(text="⏸ Pause" if self.playing else "▶ Play")
        if self.playing:
            self._schedule()

    def step_event(self):
        if not self.frames:
            return
        self.playing = False
        self.play_btn.config(text="▶ Play")
        cur = self.frames[self.fi][0]
        while self.fi < len(self.frames) - 1 and self.frames[self.fi][0] <= cur:
            self.fi += 1
        self.render()

    def finish(self):
        if self.frames:
            self.playing = False
            self.fi = len(self.frames) - 1
            self.render()

    def on_key_change(self):
        self.prev_pts = self.prev_out = None
        self.pts = []
        self.launch()


# ============================================================================
# Tab 2: Feistel rounds simulation
# ============================================================================
class FeistelTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master)
        self.app, self.eng = app, app.eng
        self.job, self.rounds = None, []

        left = ttk.Frame(self, padding=10)
        left.pack(side="left", fill="both", expand=True)
        right = ttk.Frame(self)
        right.pack(side="right", fill="both", expand=True)

        ttk.Label(left, text="Plaintext block (16 characters)",
                  font=("Segoe UI", 10, "bold")).pack(anchor="w")
        self.pt_var = tk.StringVar(value="Hello, world!!!!")
        ttk.Entry(left, textvariable=self.pt_var, width=30, font=MONO).pack(anchor="w", pady=3)
        ttk.Button(left, text="▶  Run 16 rounds", command=self.run).pack(anchor="w", pady=4)
        ttk.Label(left, wraplength=560, justify="left", text=(
            "Each round:  new L = R,   new R = L ⊕ F(R).   F shoots a ray into the crystal "
            "from (R ⊕ round key). The chart on the right runs a second copy of the cipher "
            "on the same block with ONE bit flipped and counts how many of the 128 state bits "
            "differ after every round (the avalanche effect).")).pack(anchor="w", pady=(0, 6))

        cols = ("round", "L", "R", "fin", "f")
        self.tree = ttk.Treeview(left, columns=cols, show="headings", height=17)
        for c, title, w in zip(cols, ("Round", "L (left half)", "R (right half)",
                                      "ray input R⊕Kᵢ", "F output"), (55, 140, 140, 140, 140)):
            self.tree.heading(c, text=title)
            self.tree.column(c, width=w, anchor="center")
        self.tree.pack(fill="both", expand=True)
        self.tree.bind("<<TreeviewSelect>>", self.on_select)
        self.result = ttk.Label(left, text="", font=MONO, justify="left")
        self.result.pack(anchor="w", pady=6)

        self.fig = Figure(figsize=(5.6, 6.6), dpi=100)
        self.ax3 = self.fig.add_subplot(2, 1, 1, projection="3d")
        self.ax2 = self.fig.add_subplot(2, 1, 2)
        self.fig.subplots_adjust(hspace=0.35)
        self.canvas = FigureCanvasTkAgg(self.fig, master=right)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        self._reset_axes()

    def _block(self):
        b = self.pt_var.get().encode("utf-8")[:16]
        return b + b"\x00" * (16 - len(b))

    def _reset_axes(self):
        self.ax3.cla()
        setup_axes3d(self.ax3, "Ray used by F in the selected round")
        self.ax2.cla()
        self.ax2.set_xlim(0, 16); self.ax2.set_ylim(0, 128)
        self.ax2.axhline(64, color="grey", ls="--", lw=1)
        self.ax2.text(0.2, 67, "ideal: 64 of 128 bits (random)", fontsize=8, color="grey")
        self.ax2.set_xlabel("round"); self.ax2.set_ylabel("bits different")
        self.ax2.set_title("Avalanche: 1-bit input change spreading", fontsize=10)
        self.canvas.draw_idle()

    def run(self):
        if self.job:
            self.after_cancel(self.job)
        block = self._block()
        bit = random.randrange(128)
        flipped = bytearray(block)
        flipped[bit // 8] ^= 0x80 >> (bit % 8)
        try:
            self.rounds, self.out, self.dec = self.eng.rounds(block)
            r2, out2, _ = self.eng.rounds(bytes(flipped))
        except RuntimeError as e:
            messagebox.showerror("rounds failed", str(e))
            return
        self.hd = [popcount(a["L"] ^ b["L"]) + popcount(a["R"] ^ b["R"]) for a, b in zip(self.rounds, r2)]
        self.hd.append(popcount(self.out[0] ^ out2[0]) + popcount(self.out[1] ^ out2[1]))
        self.flip_bit = bit
        self.tree.delete(*self.tree.get_children())
        self.result.config(text=f"plaintext  = {block.hex()}   (flipped copy: bit {bit})")
        self._reset_axes()
        self.k = 0
        self._step()

    def _step(self):
        if self.k < len(self.rounds):
            r = self.rounds[self.k]
            iid = self.tree.insert("", "end", values=(r["i"], f"{r['L']:016x}", f"{r['R']:016x}",
                                                      f"{r['fin']:016x}", f"{r['f']:016x}"))
            self.tree.see(iid)
            self.tree.selection_set(iid)        # triggers on_select -> draws the ray
            self._plot_hd(self.k)
            self.k += 1
            self.job = self.after(550, self._step)
        else:
            self._plot_hd(len(self.hd) - 1)
            L, R = self.out
            ok = self.dec[2]
            self.result.config(text=(
                f"plaintext  = {self._block().hex()}\n"
                f"ciphertext = {L:016x}{R:016x}\n"
                f"decrypted  = {self.dec[0]:016x}{self.dec[1]:016x}  "
                f"{'✓ exact match' if ok else '✗ MISMATCH'}\n"
                f"1-bit change (bit {self.flip_bit}) → {self.hd[-1]}/128 ciphertext bits differ"))
            self.job = None

    def _plot_hd(self, upto):
        ax = self.ax2
        for line in list(ax.lines)[1:]:
            line.remove()
        ax.plot(range(upto + 1), self.hd[:upto + 1], marker="o", color="#d62728")
        self.canvas.draw_idle()

    def on_select(self, _evt=None):
        sel = self.tree.selection()
        if not sel:
            return
        i = int(self.tree.item(sel[0])["values"][0])
        r = self.rounds[i]
        pts, _ = self.eng.trace(f"{r['fin']:016x}")
        p = np.array([q["p"] for q in pts])
        ax = self.ax3
        ax.cla()
        setup_axes3d(ax, f"Round {i}: ray for F(R ⊕ K{i})")
        ax.plot(p[:, 0], p[:, 1], p[:, 2], color="royalblue", lw=1.5)
        for ev, (col, mk, ms) in EVENT_STYLE.items():
            s = np.array([q["p"] for q in pts if q["event"] == ev]).reshape(-1, 3)
            ax.plot(s[:, 0], s[:, 1], s[:, 2], ls="none", marker=mk, color=col, ms=ms - 2)
        self.canvas.draw_idle()

    def on_key_change(self):
        if self.job:
            self.after_cancel(self.job)
            self.job = None
        self.tree.delete(*self.tree.get_children())
        self.result.config(text="key changed: press Run")
        self._reset_axes()


# ============================================================================
# Tab 3: encrypt / decrypt / tamper
# ============================================================================
class CipherTab(ttk.Frame):
    REGIONS = (("magic", "#7b2cbf"), ("nonce", "#e67e22"), ("cipher", "#1f6feb"), ("tag", "#2e8b57"))

    def __init__(self, master, app):
        super().__init__(master, padding=10)
        self.app, self.eng = app, app.eng
        self.blob, self.tampered = None, set()

        ttk.Label(self, text="Message", font=("Segoe UI", 10, "bold")).grid(row=0, column=0, sticky="w")
        self.msg = tk.Text(self, height=6, width=60, font=MONO, wrap="word")
        self.msg.insert("1.0", "Meet at the lab at 9pm. The vault code is 4471-2290.")
        self.msg.grid(row=1, column=0, sticky="nsew")

        bar = ttk.Frame(self)
        bar.grid(row=2, column=0, sticky="w", pady=6)
        for text, cmd in (("🔒 Encrypt", self.encrypt), ("🔓 Decrypt", self.decrypt),
                          ("⚡ Tamper 1 bit", self.tamper), ("🗝 Decrypt with wrong key", self.wrong_key),
                          ("🔁 Encrypt again", self.encrypt_again), ("📂 Load file…", self.load_file),
                          ("💾 Save .enc…", self.save_enc)):
            ttk.Button(bar, text=text, command=cmd).pack(side="left", padx=2)

        legend = ttk.Frame(self)
        legend.grid(row=3, column=0, sticky="w")
        for name, col in self.REGIONS + (("tampered byte", "#ff0000"),):
            tk.Label(legend, text=f"■ {name}", fg=col, font=("Segoe UI", 9, "bold")).pack(side="left", padx=6)

        self.hex = tk.Text(self, height=16, width=60, font=MONO, wrap="none")
        self.hex.grid(row=4, column=0, sticky="nsew", pady=4)
        for name, col in self.REGIONS:
            self.hex.tag_configure(name, foreground=col)
        self.hex.tag_configure("tampered", background="#ff4d4d", foreground="white")

        self.status = tk.Label(self, text="", font=("Segoe UI", 11, "bold"), anchor="w", justify="left")
        self.status.grid(row=5, column=0, sticky="we", pady=4)
        ttk.Label(self, text="Decrypted output", font=("Segoe UI", 10, "bold")).grid(row=6, column=0, sticky="w")
        self.out = tk.Text(self, height=5, width=60, font=MONO, wrap="word")
        self.out.grid(row=7, column=0, sticky="nsew")
        self.columnconfigure(0, weight=1)
        self.rowconfigure(4, weight=1)

    def _say(self, text, color):
        self.status.config(text=text, fg=color)

    def _region(self, i):
        n = len(self.blob)
        if i < 4:
            return "magic"
        if i < 16:
            return "nonce"
        if i >= n - 32:
            return "tag"
        return "cipher"

    def _render(self):
        self.hex.delete("1.0", "end")
        shown = self.blob[:2048]
        for i, b in enumerate(shown):
            if i % 16 == 0:
                self.hex.insert("end", f"{i:06x}  ")
            tags = ("tampered",) if i in self.tampered else (self._region(i),)
            self.hex.insert("end", f"{b:02x}", tags)
            self.hex.insert("end", "\n" if i % 16 == 15 else " ")
        if len(self.blob) > len(shown):
            self.hex.insert("end", f"\n… ({len(self.blob) - len(shown)} more bytes)")

    def encrypt(self, data=None):
        data = data if data is not None else self.msg.get("1.0", "end-1c").encode("utf-8")
        try:
            self.blob = self.eng.encrypt(data)
        except RuntimeError as e:
            messagebox.showerror("encrypt failed", str(e))
            return
        self.tampered = set()
        self._render()
        self.out.delete("1.0", "end")
        self._say(f"Encrypted {len(data)} bytes → {len(self.blob)} bytes "
                  f"(+4 magic, +12 random nonce, +32 HMAC tag)", "#1f4e79")

    def encrypt_again(self):
        old = self.blob
        self.encrypt()
        if old and self.blob and len(old) == len(self.blob):
            body = slice(16, len(old) - 32)
            diff = sum(popcount(a ^ b) for a, b in zip(old[body], self.blob[body]))
            total = 8 * (len(old) - 48)
            if total:
                self._say(f"Same message, new random nonce → {diff}/{total} ciphertext bits differ "
                          f"({diff / total:.0%}). An eavesdropper can't tell the messages are equal.",
                          "#1f4e79")

    def decrypt(self, key=None, label="correct key"):
        if not self.blob:
            self._say("Encrypt something first.", "#a00")
            return
        ok, res = self.eng.decrypt(self.blob, key)
        self.out.delete("1.0", "end")
        if ok:
            self.out.insert("1.0", res.decode("utf-8", errors="replace"))
            self._say(f"✔ Tag verified with the {label}, decrypted {len(res)} bytes.", "#2e8b57")
        else:
            self.out.insert("1.0", "(nothing was decrypted)")
            self._say(f"✘ {res}", "#d62728")

    def tamper(self):
        if not self.blob:
            self._say("Encrypt something first.", "#a00")
            return
        blob = bytearray(self.blob)
        lo, hi = (16, len(blob) - 32) if len(blob) > 48 else (len(blob) - 32, len(blob))
        i = random.randrange(lo, hi)
        bit = random.randrange(8)
        blob[i] ^= 1 << bit
        self.blob = bytes(blob)
        self.tampered.add(i)
        self._render()
        self._say(f"⚡ An attacker flipped bit {bit} of byte {i} ({self._region(i)}). Now press Decrypt.",
                  "#b35900")

    def wrong_key(self):
        other = self.eng.tmp / "wrong.key"
        self.eng.new_key(other)
        self.decrypt(key=other, label="wrong key")

    def load_file(self):
        path = filedialog.askopenfilename()
        if path:
            data = Path(path).read_bytes()
            self.msg.delete("1.0", "end")
            self.msg.insert("1.0", f"[file: {path}, {len(data)} bytes]")
            self.encrypt(data)

    def save_enc(self):
        if not self.blob:
            return
        path = filedialog.asksaveasfilename(defaultextension=".enc")
        if path:
            Path(path).write_bytes(self.blob)

    def on_key_change(self):
        if self.blob:
            self._say("Key changed: the old ciphertext now needs the old key. Try Decrypt!", "#b35900")


# ============================================================================
# Tab 4: crystal explorer
# ============================================================================
class CrystalTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master, padding=6)
        self.app, self.eng = app, app.eng
        top = ttk.Frame(self)
        top.pack(fill="x")
        ttk.Label(top, text="Depth slice z =").pack(side="left")
        self.z = tk.IntVar(value=0)
        self.z_lbl = ttk.Label(top, text="0", width=3)
        ttk.Scale(top, from_=0, to=GRID - 1, variable=self.z, orient="horizontal", length=320,
                  command=lambda _v: self.draw()).pack(side="left", padx=6)
        self.z_lbl.pack(side="left")
        self.info = ttk.Label(top, text="")
        self.info.pack(side="left", padx=20)

        self.fig = Figure(figsize=(11, 5.6), dpi=100)
        self.ax = self.fig.add_subplot(1, 2, 1)
        self.axh = self.fig.add_subplot(1, 2, 2)
        self.canvas = FigureCanvasTkAgg(self.fig, master=self)
        self.canvas.get_tk_widget().pack(fill="both", expand=True)
        self.cbar = None
        self.draw()

    def draw(self):
        g = self.eng.grid()
        z = int(round(self.z.get()))
        self.z_lbl.config(text=str(z))
        self.ax.cla()
        im = self.ax.imshow(g[z], origin="lower", cmap="viridis", vmin=N_MIN, vmax=N_MAX,
                            extent=(0, GRID, 0, GRID))
        self.ax.set_title(f"Refractive index n in slice z = {z}", fontsize=10)
        self.ax.set_xlabel("x"); self.ax.set_ylabel("y")
        if self.cbar is None:
            self.cbar = self.fig.colorbar(im, ax=self.ax, label="n  (1.0 = air … 2.417 = diamond)")
        self.axh.cla()
        self.axh.hist(g.ravel(), bins=40, color="#5b8def")
        self.axh.set_title("Distribution of n over all 4096 cells", fontsize=10)
        self.axh.set_xlabel("n"); self.axh.set_ylabel("cells")
        self.info.config(text=f"min {g.min():.3f}   max {g.max():.3f}   mean {g.mean():.3f}   "
                              f"(grown from the key: new key → whole new crystal)")
        self.canvas.draw_idle()

    def on_key_change(self):
        self.draw()


# ============================================================================
# Tab 5: attack lab
# ============================================================================
class AttackTab(ttk.Frame):
    def __init__(self, master, app):
        super().__init__(master, padding=10)
        self.app = app
        self.proc, self.q = None, queue.Queue()
        top = ttk.Frame(self)
        top.pack(fill="x")
        ttk.Label(top, text="Megabytes for statistics:").pack(side="left")
        self.mb = tk.IntVar(value=1)
        ttk.Spinbox(top, from_=1, to=10, textvariable=self.mb, width=4).pack(side="left", padx=4)
        self.btn = ttk.Button(top, text="⚔  Run attack suite", command=self.run)
        self.btn.pack(side="left", padx=8)
        ttk.Label(top, text="(≈ 45 s + 7 s per MB; runs the real attack.exe on a fresh random key)").pack(side="left")
        self.text = tk.Text(self, font=MONO, wrap="none")
        self.text.pack(fill="both", expand=True, pady=6)
        self.text.tag_configure("pass", foreground="#2e8b57", font=("Consolas", 10, "bold"))
        self.text.tag_configure("fail", foreground="#d62728", font=("Consolas", 10, "bold"))
        self.text.tag_configure("head", foreground="#1f4e79", font=("Consolas", 10, "bold"))

    def run(self):
        if not ATTACK.exists():
            messagebox.showerror("missing", f"{ATTACK.name} not found. Build it with: mingw32-make all")
            return
        self.text.delete("1.0", "end")
        self.btn.state(["disabled"])
        self.proc = subprocess.Popen([str(ATTACK), "--mb", str(self.mb.get())], stdout=subprocess.PIPE,
                                     stderr=subprocess.STDOUT, text=True, bufsize=1, creationflags=NO_WINDOW)
        threading.Thread(target=self._reader, daemon=True).start()
        self.after(100, self._poll)

    def _reader(self):
        for line in self.proc.stdout:
            self.q.put(line)
        self.q.put(None)

    def _poll(self):
        try:
            while True:
                line = self.q.get_nowait()
                if line is None:
                    self.btn.state(["!disabled"])
                    self.proc = None
                    return
                tag = ("pass" if "[PASS]" in line else "fail" if "[FAIL]" in line
                       else "head" if line.startswith(("[", "===")) else None)
                self.text.insert("end", line, tag)
                self.text.see("end")
        except queue.Empty:
            pass
        self.after(100, self._poll)

    def stop(self):
        if self.proc and self.proc.poll() is None:
            self.proc.kill()


# ============================================================================
# main window
# ============================================================================
class App(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Refractive Cipher - Simulation Lab")
        self.geometry("1320x840")
        self.eng = Engine()
        self.eng.new_key()

        bar = ttk.Frame(self, padding=(10, 6))
        bar.pack(fill="x")
        ttk.Label(bar, text="3D Chaotic Refractive Cipher", font=("Segoe UI", 13, "bold")).pack(side="left")
        self.key_lbl = ttk.Label(bar, text="", font=MONO)
        self.key_lbl.pack(side="left", padx=20)
        for text, cmd in (("🎲 New key", self.new_key), ("📂 Load key…", self.load_key),
                          ("💾 Save key…", self.save_key)):
            ttk.Button(bar, text=text, command=cmd).pack(side="left", padx=2)
        ttk.Label(bar, text="educational cipher - use AES-GCM for real data",
                  foreground="#a00").pack(side="right")

        nb = ttk.Notebook(self)
        nb.pack(fill="both", expand=True)
        self.tabs = []
        for cls, title in ((RayTab, "  🔦 Ray simulation  "), (FeistelTab, "  🔄 Feistel rounds  "),
                           (CipherTab, "  🔒 Encrypt / Decrypt  "), (CrystalTab, "  💎 Crystal  "),
                           (AttackTab, "  ⚔ Attack lab  ")):
            tab = cls(nb, self)
            nb.add(tab, text=title)
            self.tabs.append(tab)
        self._update_key_label()
        self.protocol("WM_DELETE_WINDOW", self.on_close)

    def _update_key_label(self):
        self.key_lbl.config(text=f"key: {self.eng.fingerprint()}  (256-bit)")

    def _key_changed(self):
        self._update_key_label()
        for t in self.tabs:
            if hasattr(t, "on_key_change"):
                t.on_key_change()

    def new_key(self):
        self.eng.new_key()
        self._key_changed()

    def load_key(self):
        path = filedialog.askopenfilename(title="Load 32-byte key file")
        if path:
            try:
                self.eng.load_key(path)
            except (OSError, ValueError) as e:
                messagebox.showerror("Load key", str(e))
                return
            self._key_changed()

    def save_key(self):
        path = filedialog.asksaveasfilename(defaultextension=".key")
        if path:
            self.eng.save_key(path)

    def on_close(self):
        for t in self.tabs:
            if hasattr(t, "stop"):
                t.stop()
        self.eng.cleanup()
        self.destroy()


def main():
    if not REFCRYPT.exists():
        root = tk.Tk()
        root.withdraw()
        messagebox.showerror("refcrypt not built",
                             f"{REFCRYPT} was not found.\n\nBuild it first:\n  cd {ROOT}\n  mingw32-make all")
        return
    App().mainloop()


if __name__ == "__main__":
    main()

# =====================================================================
# EXPLANATION (in simple words)
# ---------------------------------------------------------------------
# This is a desktop app (made with Tkinter, Python's built-in window
# toolkit, plus matplotlib for the charts) that lets you SEE the cipher
# working. It never does the cryptography itself. It runs the real C
# programs refcrypt.exe and attack.exe and draws what they report.
#
# Engine: a helper that calls refcrypt.exe with the right arguments
#   (keygen, enc, dec, trace, grid, rounds) and reads the results.
#   The current key is kept in a temporary folder that is deleted when
#   you close the window.
#
# Tab 1 - Ray simulation:
#   A yellow "photon" flies through the 3D crystal, step by step. Each
#   event appears as it happens: black dot = refraction, red X = total
#   internal reflection, orange square = outer wall. The event log
#   explains the physics with the real numbers (n1 -> n2, the angle in,
#   the angle out, the critical angle). "Flip 1 bit & compare" changes
#   one bit of the input, so you can watch the new ray take a completely
#   different path (the old one stays in grey) and see that about half
#   of F's 64 output bits change.
#
# Tab 2 - Feistel rounds:
#   Encrypts one 16-character block and adds the 16 rounds to the
#   table one by one. Click any round to see the ray its F used. The
#   chart runs a copy with ONE input bit flipped and plots how many of
#   the 128 bits differ after each round: watch it jump to ~64 (50%).
#   At the end it shows the ciphertext and proves decryption is exact.
#
# Tab 3 - Encrypt / Decrypt:
#   Type a message and encrypt it. The output is coloured by part: purple
#   magic "RFC1", orange nonce, blue ciphertext, green HMAC tag. Try
#   "Tamper 1 bit" or "Decrypt with wrong key": decryption is refused.
#   "Encrypt again" shows that the same message gives a completely
#   different ciphertext each time.
#
# Tab 4 - Crystal: a slider moves through the 16 depth slices of the
#   secret crystal and shows each cell's refractive index as a colour,
#   with a histogram of all 4096 values.
#
# Tab 5 - Attack lab: runs attack.exe and streams its report live,
#   with PASS in green and FAIL in red.
#
# The top bar lets you make, load or save a key. Every tab updates
# when the key changes.
# =====================================================================
