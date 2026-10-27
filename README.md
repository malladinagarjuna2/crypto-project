# crypto-project: 3D Chaotic Refractive Cipher

An educational encryption system written in **C11**. A secret 3D "crystal" of glass cubes is grown from your key, and rays of light are traced through it with **Snell's law** and **total internal reflection**. The paths of those rays drive a **16-round Feistel block cipher**, which runs in **CTR mode** with **HMAC-SHA256** tamper protection. A separate **attack suite** then tries to break it.

> ⚠️ **Learning project, not for real secrets.** This cipher has had no independent cryptanalysis. To protect real data, use AES-256-GCM or ChaCha20-Poly1305.

📖 For the full illustrated explanation with interactive demos, open [`docs/how-it-works.html`](docs/how-it-works.html) in a browser.
🎬 To watch the real cipher in action, run the **[simulation GUI](#simulation-gui)**: `python tools/refcrypt_gui.py`.

![Ray simulation](docs/images/gui-ray-simulation.png)

---

## Contents
1. [How it works](#how-it-works)
2. [Requirements](#requirements)
3. [Build](#build)
4. [Usage](#usage)
5. [Simulation GUI](#simulation-gui)
6. [How to test it](#how-to-test-it)
7. [Results](#results)
8. [Project layout](#project-layout)
9. [Design decisions](#design-decisions)
10. [Limitations](#limitations)

---

## How it works

```
256-bit key ──HKDF-SHA256──┬─ grid seed ─► 4D hyperchaotic map (RK4) ⊕ HMAC stream
                           │                 └─► 16×16×16 crystal, n ∈ [1.0, 2.417] in Q16.16
                           ├─ S-box seed ─► key-dependent byte permutation
                           ├─ 16 × 64-bit round keys
                           └─ MAC key

Round function  F(x, i) = trace a ray encoded by (x ⊕ K_i) through the crystal:
                          48 boundary events (refraction / total internal reflection / wall),
                          S-box mixing at every step → 64-bit digest of the journey
Block cipher    16-round Feistel network on 128-bit blocks, using F
File format     "RFC1" ‖ nonce (12 B) ‖ CTR ciphertext ‖ HMAC-SHA256 tag (32 B)
```

1. **Key schedule:** your 32-byte key is expanded with HKDF-SHA256 into independent subkeys.
2. **Crystal:** a 4D chaotic system, mixed with HMAC output, gives every one of the 4,096 cubes a refractive index between 1.0 (air) and 2.417 (diamond).
3. **Ray tracing:** the input chooses a ray's entry point and angle. At every cube face the ray either bends (Snell's law) or bounces back (total internal reflection). All maths is **Q16.16 fixed-point**, so results are bit-identical on every computer.
4. **Feistel network:** the ray tracer is the round function. Feistel decryption only ever runs F *forwards*, so decryption is always exact.
5. **CTR + HMAC:** a random nonce makes every encryption unique, and the HMAC tag rejects any modified file before decrypting.

---

## Requirements

- GCC with C11 support (tested with **MinGW GCC 6.3** on Windows 11)
- `make` (`mingw32-make` on Windows)
- *For the GUI and plots:* Python 3 with `matplotlib` and `numpy` (`pip install matplotlib numpy`). Tkinter is included with the standard Windows Python installer.

No other libraries are needed. SHA-256, HMAC and HKDF are implemented in this repo.

---

## Build

```sh
git clone https://github.com/malladinagarjuna2/crypto-project.git
cd crypto-project
mingw32-make all        # Linux/macOS: make all
```

This produces three programs:

| Program | Purpose |
|---|---|
| `refcrypt.exe` | encrypt / decrypt files, generate keys, trace rays |
| `tests.exe` | unit tests |
| `attack.exe` | cryptanalysis suite |

---

## Usage

```sh
./refcrypt.exe keygen my.key                        # create a random 256-bit key (keep it secret!)
./refcrypt.exe enc my.key notes.txt notes.enc       # encrypt (output = input size + 48 bytes)
./refcrypt.exe dec my.key notes.enc notes_back.txt  # verify tag, then decrypt
./refcrypt.exe trace my.key 0123456789abcdef ray.csv  # export one ray path as CSV
./refcrypt.exe grid my.key crystal.csv              # demo only: export the crystal
./refcrypt.exe rounds my.key 48656c6c6f2c20776f726c6421212121  # demo only: show 16 rounds
```

Prefer clicking to typing? See the [Simulation GUI](#simulation-gui).

---

## Simulation GUI

An interactive desktop app that **animates the cipher**: watch a photon fly through the secret crystal, watch the Feistel rounds scramble a block, then tamper with ciphertext and see it rejected. It doesn't reimplement anything. Every result comes from the real C programs (`refcrypt.exe`, `attack.exe`).

### Run it

```sh
mingw32-make all                  # 1. build the C programs first
pip install matplotlib numpy      # 2. one-time (Tkinter ships with Python)
python tools/refcrypt_gui.py      # 3. launch
```

A random key is generated at start-up. Use **🎲 New key / 📂 Load key… / 💾 Save key…** in the top bar to change it; every tab updates immediately.

### 🔦 Ray simulation

![Ray simulation tab](docs/images/gui-ray-simulation.png)

The heart of the demo. A yellow photon travels through the 16×16×16 crystal, whose faint dots are coloured by refractive index.

- **Markers:** green = entry, black = refraction, red ✖ = total internal reflection, orange ■ = bounce off the outer wall.
- **Event log:** explains every step with real numbers, e.g. `TIR x-face n 2.373→1.700 θ₁=79.0° > θc=45.7°`.
- **Controls:** ▶ Play / ⏸ Pause, ⏭ *Next event* (step through one event at a time), ⏩ *End*, and a speed slider.
- **Flip 1 bit & compare:** changes one input bit and relaunches. The old path stays grey, the new one takes a completely different route, and the panel reports how many of F's 64 output bits changed (ideal ≈ 50%).

### 🔄 Feistel rounds

![Feistel rounds tab](docs/images/gui-feistel-rounds.png)

Type a 16-character block and press **Run 16 rounds**.

- The table fills in round by round: L, R, the ray input `R ⊕ Kᵢ`, and F's output. Click any row to see the ray that round used.
- The **avalanche chart** runs a second copy with one flipped bit and plots how many of the 128 state bits differ. It jumps to ~64 (random) within 2 rounds.
- At the end it shows the ciphertext and confirms decryption gives back the exact original.

### 🔒 Encrypt / Decrypt

![Encrypt / Decrypt tab](docs/images/gui-encrypt-decrypt.png)

The ciphertext is colour-coded: purple = `RFC1` magic, orange = random nonce, blue = ciphertext, green = HMAC tag.

| Button | What it demonstrates |
|---|---|
| 🔒 Encrypt / 🔓 Decrypt | normal round trip |
| ⚡ Tamper 1 bit | flips one bit (highlighted red). Decrypt then fails with **AUTHENTICATION FAILED** |
| 🗝 Decrypt with wrong key | rejected, nothing decrypted |
| 🔁 Encrypt again | same message, new nonce → ~50% of ciphertext bits differ |
| 📂 Load file… / 💾 Save .enc… | encrypt any file and save the result |

### 💎 Crystal

![Crystal tab](docs/images/gui-crystal.png)

Slide through the 16 depth layers of the key-generated crystal, and see the histogram of all 4,096 refractive indices (spread evenly between 1.0 and 2.417). Press **New key** and the entire crystal changes.

### ⚔ Attack lab

Runs `attack.exe` with your chosen number of megabytes and streams the report live (PASS in green, FAIL in red): avalanche matrix, linearity test, codebook attack and statistical tests. It takes about 45 s plus 7 s per MB.

### Notes

- The GUI uses two **demo-only** commands: `refcrypt grid` (dumps the crystal) and `refcrypt rounds` (dumps round values). They reveal key-derived data, so never use them with a real key.
- **"refcrypt not built" on start-up:** run `mingw32-make all` in the project folder first.
- **`ModuleNotFoundError: matplotlib`:** run `pip install matplotlib numpy` for the same Python you launch the GUI with.
- Temporary files go in a system temp folder that is deleted when you close the window.

---

## How to test it

### 1. Automatic tests

```sh
mingw32-make test        # unit tests          → "67 passed, 0 failed"
mingw32-make check-cli   # end-to-end CLI test → "CLI tests: all passed"
mingw32-make attack      # attack suite, 2 MB  → "all checks passed"   (~1 min)
./attack.exe --mb 10     # full 10 MB version                          (~3 min)
```

The **unit tests** cover:
- fixed-point arithmetic
- official SHA-256, HMAC and HKDF test vectors (FIPS 180-4, RFC 4231, RFC 5869)
- crystal and key-schedule determinism and avalanche
- Snell's law and total internal reflection physics against worked examples
- Feistel round trips
- tamper rejection at every byte position

The **CLI test** encrypts and decrypts empty, small and 200 KB random files. It also checks the 48-byte overhead, that each encryption uses a fresh nonce, and that wrong keys and tampered files are rejected.

### 2. Try it by hand

```sh
./refcrypt.exe keygen my.key
echo "my secret message" > secret.txt
./refcrypt.exe enc my.key secret.txt secret.enc
cat secret.enc                                   # random-looking bytes
./refcrypt.exe dec my.key secret.enc back.txt
cat back.txt                                     # → my secret message
```

### 3. Check that it detects attacks

```sh
# Wrong key
./refcrypt.exe keygen other.key
./refcrypt.exe dec other.key secret.enc x.txt    # → AUTHENTICATION FAILED

# Tampering: change one byte
cp secret.enc bad.enc
printf '\001' | dd of=bad.enc bs=1 seek=20 count=1 conv=notrunc
./refcrypt.exe dec my.key bad.enc x.txt          # → AUTHENTICATION FAILED

# Same file, encrypted twice → different ciphertexts (random nonce)
./refcrypt.exe enc my.key secret.txt secret2.enc
cmp secret.enc secret2.enc                       # → files differ
```

### 4. Visualise a ray

```sh
./refcrypt.exe trace my.key 0123456789abcdef ray.csv
pip install matplotlib
python tools/plot_ray.py ray.csv                 # interactive 3D plot
python tools/plot_ray.py ray.csv ray.png         # or save as an image
```

Change one hex digit of the input and the path becomes completely different.

---

## Results

Measured with `attack.exe` on a random key:

| Test | Result | Meaning |
|---|---|---|
| SAC, 1 round | 13% mean flip, 75% bad cells | correctly flagged as weak (the test works) |
| SAC, 3 → 16 rounds | 50.0% mean, worst cell 0.117 from ideal | full diffusion |
| Key avalanche | 50.3% of ciphertext bits change | 1 key bit affects everything |
| Linearity, 1 / 16 rounds | 100% / 0% | not a linear (LTI) system |
| Codebook attack on the **original** naive design | 100% of secret recovered | why the design was changed |
| Codebook attack on the **final** cipher | ~1% (chance) | attack fails |
| Entropy (10 MB) | 7.999984 bits/byte | indistinguishable from noise |
| Chi-square p / serial corr. (10 MB) | 0.936 / −0.00016 | uniform, no correlation |
| NIST monobit / runs p (10 MB) | 0.558 / 0.790 | pass |

Throughput is about **140 KB/s**. That's slow by design: each 16-byte block traces 16 rays × 48 boundary events in integer maths.

---

## Project layout

```
crypto-project/
├── Makefile
├── docs/
│   ├── how-it-works.html      interactive explainer
│   └── images/                GUI screenshots used in this README
├── include/                   headers
├── src/
│   ├── fixed.c                Q16.16 fixed-point maths
│   ├── sha256.c hmac.c hkdf.c hashing and key derivation
│   ├── keysched.c             key schedule + KDF byte stream
│   ├── chaos.c                4D hyperchaotic map (RK4)
│   ├── grid.c                 16×16×16 crystal generator
│   ├── sbox.c                 key-dependent S-box
│   ├── raytrace.c             3D ray tracer (Snell / TIR)
│   ├── feistel.c              16-round Feistel cipher
│   ├── mode.c                 CTR mode + HMAC file format
│   ├── rng.c                  OS secure randomness
│   └── main.c                 refcrypt command-line tool
├── attack/                    SAC, linearity, codebook attack, statistics
├── tests/                     unit tests + CLI test script
└── tools/
    ├── refcrypt_gui.py        interactive simulation GUI (Tkinter + matplotlib)
    └── plot_ray.py            3D ray path plotter
```

Every source file ends with a plain-language **EXPLANATION** section describing what the code does.

---

## Design decisions

The first draft of this design had several flaws. These are the fixes:

| Original idea | Problem | Fix |
|---|---|---|
| Encrypt one byte per ray | Only 256 possible inputs, so a codebook breaks it (the attack suite proves this) | 128-bit Feistel blocks |
| Decrypt by tracing rays backwards | Fixed-point rounding makes the reverse path drift | Feistel: decryption only runs F forwards |
| "CBC" XOR applied after encryption | The attacker can undo it, and there is no IV | CTR mode with a random nonce |
| No integrity check | Ciphertext bits can be flipped silently | Encrypt-then-MAC (HMAC-SHA256) |
| Q16.16 range written as 100000–241700 | That is decimal scaling, not Q16.16 | 65536–158401 (= 1.0–2.417) |
| Security from chaos alone | Fixed-point chaos falls into cycles | Chaos ⊕ HMAC stream |

---

## Limitations

- No independent security analysis. Passing statistical tests does not prove a cipher is secure.
- Table lookups (S-box, grid) depend on secret data, so cache-timing side channels are not eliminated.
- Slow compared with standard ciphers.
- Whole files are loaded into memory.

---

## Explanation (in simple words)

This project encrypts files using an imaginary crystal made of 4,096 tiny glass cubes, each bending light by a different, secret amount. The secret key decides how the crystal is built. To encrypt, the program shoots rays of light through the crystal and records where they come out. That result, scrambled over 16 rounds, becomes a stream of random-looking bytes that is mixed with your file. A fingerprint (the HMAC tag) is added so any tampering is detected. Decryption rebuilds the same crystal from the key and undoes the mixing.

The attack programs then try to break it. They check that changing one bit changes about half the output, that the cipher isn't linear, that a codebook attack fails (it succeeds against the original naive design, which shows why the design was changed), and that the encrypted output looks statistically like random noise.
