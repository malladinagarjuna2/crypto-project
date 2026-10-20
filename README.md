# crypto-project

**3D Chaotic Refractive Cipher**: an educational block cipher written in C11. A secret 3D crystal of refractive indices is grown from your key. Rays of "light" are traced through it using Snell's law and total internal reflection, and the result is used as the round function of a 16-round Feistel network running in CTR mode with HMAC-SHA256 authentication.

> ⚠️ This is a learning/research project. It has had no independent cryptanalysis. Use AES-GCM or ChaCha20-Poly1305 for real data.

For the full illustrated explanation with interactive demos, open [docs/how-it-works.html](docs/how-it-works.html) in a browser.

## Build

Requires GCC (tested with MinGW GCC 6.3 on Windows) and make.

```sh
mingw32-make all          # builds refcrypt.exe, tests.exe, attack.exe
mingw32-make test         # unit tests (hash vectors, physics, round trips, tampering)
mingw32-make check-cli    # end-to-end test of the command-line tool
mingw32-make attack       # cryptanalysis suite (2 MB), or: ./attack.exe --mb 10
```

On Linux/macOS use `make`.

## Usage

```sh
./refcrypt.exe keygen my.key                       # random 256-bit key
./refcrypt.exe enc my.key notes.txt notes.enc      # encrypt (+48 bytes)
./refcrypt.exe dec my.key notes.enc notes.txt      # verify tag, then decrypt
./refcrypt.exe trace my.key 0123456789abcdef ray.csv
python tools/plot_ray.py ray.csv                   # 3D plot of that ray
```

## Architecture

```
256-bit key ──HKDF-SHA256──┬─ grid seed ─► 4D hyperchaotic RK4 ⊕ HMAC stream ─► 16×16×16 crystal (Q16.16 n ∈ [1.0, 2.417])
                           ├─ S-box seed ─► key-dependent byte permutation
                           ├─ 16 × 64-bit round keys
                           └─ MAC key
F(x, i)   = ray trace of (x ⊕ K_i): 48 boundary events (Snell / TIR / wall), S-box mixing, 64-bit digest
Block     = 16-round Feistel on 128 bits, F above
File      = "RFC1" ‖ nonce(12) ‖ CTR ciphertext ‖ HMAC-SHA256 tag(32)
```

| Path | Contents |
|---|---|
| `src/fixed.c` | Q16.16 fixed-point math (bit-exact on every CPU) |
| `src/sha256.c`, `hmac.c`, `hkdf.c` | Hashing and key derivation, checked against FIPS/RFC vectors |
| `src/keysched.c`, `chaos.c`, `grid.c`, `sbox.c` | Key schedule, chaotic map, crystal, S-box |
| `src/raytrace.c` | 3D voxel ray tracer with vector Snell's law and TIR |
| `src/feistel.c`, `mode.c`, `rng.c`, `main.c` | Block cipher, CTR + HMAC, OS randomness, CLI |
| `attack/` | SAC, impulse/linearity, codebook attack, statistics |
| `tests/` | Unit tests and CLI test script |

## Attack suite results (2 MB run)

| Test | Result |
|---|---|
| SAC, 1 round | 13% mean, 75% bad cells (correctly flagged as weak) |
| SAC, 3+ rounds → 16 rounds | 50.0% mean, worst cell 0.117 from ideal |
| Key avalanche (1 key bit) | 50.3% of ciphertext bits change |
| Linearity, 1 round / 16 rounds | 100% (caught) / 0% |
| Codebook attack, original plan design | 100% of secret recovered |
| Codebook attack, final cipher | ~1% (chance level) |
| Ciphertext entropy | 7.99990 bits/byte |
| Chi-square p / serial corr / monobit p / runs p | 0.128 / +0.0002 / 0.784 / 0.093 |

Throughput is about 140 KB/s. That's slow on purpose: every 16-byte block traces 16 rays × 48 boundary events in integer math.

## Explanation (in simple words)

This project encrypts files using an imaginary crystal made of 4,096 tiny glass cubes, each bending light by a different, secret amount. The secret key decides how the crystal is built. To encrypt, the program shoots rays of light through the crystal and records where they come out. That result, scrambled further over 16 rounds, becomes a stream of random-looking bytes that is mixed with your file. A fingerprint (HMAC tag) is added so any tampering is detected. Decryption rebuilds the same crystal from the key and reverses the process.

The `attack/` programs then try to break it: they check that changing one bit changes about half the output, that the cipher isn't linear, that a codebook attack fails (it does succeed against the original naive design, which shows why the design was changed), and that the ciphertext looks statistically like random noise.
