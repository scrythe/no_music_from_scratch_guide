# nomusic from scratch: the game

This file is done with the help of AI. As I program along, I will hopefully try to modify it, but it will still possibly be largely done by claude. The code will be made by myself however (except for maybe some reference python files and so).

You rebuild the music remover in C, one level at a time, and later make it fast with ISPC and GLSL.
Every level ends with something that works and that a referee checks: a sound you can hear, a picture,
or a file that matches the Python version. Each level grows the last one; nothing is "set up the
project first, see results later".

## How to play

1. Make the reference files once (about 650 MB, in `reference/`):

   ```
   uv run python scripts/export_reference.py
   ```

2. Write your C code in `port/` (one program, `nm.c`, that grows every level).
3. Each level says which file to write into `mine/`. All files are raw little-endian `float`
   (exactly what `fwrite(array, sizeof(float), n, f)` writes), in C order: for a shape
   (1025 × 751 × 2) the last number changes fastest, so element `[f][t][c]` is at
   `(f * 751 + t) * 2 + c`.
4. Ask the referee:

   ```
   uv run python scripts/check.py 6
   ```

   It prints the SNR (how close you are, in dB, higher is better) and whether you **cleared**
   the level (★) or got a **perfect** (★★★). Run it without a number to see the map and your XP.

Tip: let your program take the level as an argument (`./nm 6` runs level 6), so old levels keep
working as you go: they are your regression tests.

Build with:

```
gcc -O2 -march=native -o port/nm port/nm.c -lm
```

| SNR | Meaning |
|---|---|
| exact / > 120 dB | bit-identical or nearly |
| 90–120 dB | correct, normal float rounding |
| 40–90 dB | correct but sloppy precision, or fp16 |
| < 30 dB | a bug, not rounding |

The map:

| World | Levels | You end up with |
|---|---|---|
| 1 Sound | 1–3 | read/write audio, compare results |
| 2 Frequencies | 4–8 | spectrograms, and **music removed with a given mask** |
| 3 Neurons | 9–12 | the layers, and the mask computed by your own code |
| 4 Memory | 13–16 | the GRU, and the **whole network** in C |
| 5 The Tool | 17 | your own `nomusic` for real videos |
| 6 Speed | 18–19 | threads and ISPC |
| 7 GPU | 20–24 | Vulkan + GLSL, up to beating the PyTorch version |

---

# World 1: Sound

## Level 1: Beep

**Goal:** make a sound from nothing.

Write 1 second of a 440 Hz sine at 48000 samples per second, amplitude 0.5:
`sample[n] = 0.5 * sin(2π · 440 · n / 48000)`, for n = 0 … 47999.

- Output: `mine/sine.f32` (48000 floats)
- Listen: `ffplay -f f32le -ar 48000 -ch_layout mono mine/sine.f32`
- Check: `uv run python scripts/check.py 1`

**For ★★★:** if you compute the angle in `float`, the error grows with n. Compute it in `double`
and convert at the end. That is the first lesson of this game: where you lose precision matters.

## Level 2: Echo

**Goal:** read a real file's audio into memory, exactly as the Python version does.

Run ffmpeg as a child process and read its output through a pipe with `popen()`:

```
ffmpeg -v error -nostdin -i samples/before.m4a -map 0:a:0 -af aresample=async=1:first_pts=0 -f f32le -ar 48000 -ac 2 pipe:1
```

You don't know the length in advance: read in blocks and grow the buffer with `realloc`.

ffmpeg gives you the samples **interleaved** (L, R, L, R, …). The reference stores them
**planar**: all left samples, then all right samples, shape (2 × samples). Rearrange them.

- Output: `mine/audio.f32`
- Check: `check.py 2`. It needs to be exact.
- Bonus: write it back with `popen(..., "w")` into `ffmpeg -f f32le -ar 48000 -ac 2 -i pipe:0 out.wav`
  at half volume and listen. (Interleave again before writing!)

## Level 3: The Judge

**Goal:** write the function you will use in every later level.

`float *load_f32(const char *path, size_t *count)` (read a whole file) and
`double snr_db(const float *ref, const float *mine, size_t n)`:

```
SNR = 10 · log10( Σ ref² / Σ (ref − mine)² )
```

Accumulate the sums in `double`. Run your judge on `reference/levels/judge_a.f32` and
`reference/levels/judge_b.f32`.

- Check: `check.py 3 --answer <the number your judge printed>`
- From now on, compare your output to the reference yourself first; the referee is for scoring.

---

# World 2: Frequencies

A spectrogram cuts the sound into short overlapping frames and asks each frame "how much of each
frequency is in you?". The model never sees raw samples; it only edits a spectrogram.

## Level 4: Slow DFT

**Goal:** the frequency content of one frame of 2048 samples.

Input: `reference/levels/frame.f32` (2048 floats). For each frequency bin k = 0 … 1024:

```
re[k] =  Σn x[n] · cos(2π·k·n / 2048)
im[k] = −Σn x[n] · sin(2π·k·n / 2048)
```

Only 1025 bins: the other half is a mirror image for real signals.

- Output: `mine/dft.f32`, shape (1025 × 2): re, im per bin
- Check: `check.py 4`
- **For ★★★:** accumulate in `double`, and use `(k*n) % 2048` in the angle so it stays small.

## Level 5: Fast Fourier

**Goal:** same result, roughly 100× faster.

Write an iterative radix-2 FFT (Cooley-Tukey): reorder the input by bit-reversed index, then
combine pairs, then pairs of pairs, … (11 stages for 2048). Use complex numbers from `<complex.h>`
or your own struct. Time it against level 4 with `clock_gettime`.

- Output: `mine/fft.f32` (1025 × 2), same as level 4
- Check: `check.py 5`, and use your judge to compare it with your own level 4 output
- Achievement: FFT ≥ 100× faster than your DFT

## Level 6: Spectrogram

**Goal:** the STFT of 8 seconds of audio, exactly as the model sees it.

Input: `reference/chunk_input.f32` (384000 samples, 8 s).

1. Window: `w[n] = 0.5 − 0.5 · cos(2π·n / 2048)` (n = 0 … 2047; note: divide by 2048, not 2047).
2. Pad 1024 samples on both sides by **reflection**: the padded signal before x[0] is
   …, x[3], x[2], x[1] (the edge sample is not repeated). Same at the end.
3. Frame t (t = 0 … 750) is padded[t·512 … t·512 + 2047]. Multiply by w and FFT it.
4. Divide everything by `norm = sqrt(Σ w[n]²)` (≈ 27.71).

That gives 751 frames × 1025 bins. The reference shape is **(1025 × 751 × 2)**: frequency first,
then time, then re/im. Your FFT loop produces it time-first, so write it transposed.

- Output: `mine/stft.f32`
- Check: `check.py 6`
- Bonus: write the log magnitude as a grey-scale PGM image (`P5` header, one byte per pixel,
  low frequencies at the bottom). Music shows up as straight horizontal lines, speech as
  wiggly stacked ones. Open it in any image viewer.

## Level 7: Round Trip

**Goal:** turn a spectrogram back into sound (the inverse STFT).

Start from your level 6 spectrogram multiplied back by `norm`. For each frame:

1. Inverse real FFT: rebuild the full 2048-bin spectrum from 1025 bins (bin 2048−k is the complex
   conjugate of bin k; ignore the imaginary parts of bins 0 and 1024), inverse FFT
   (same as your FFT with +sin instead of −sin), divide by 2048.
2. Multiply by the window w again.
3. Add it into an output buffer at position t·512 (overlap-add), and add w² into an
   "envelope" buffer at the same positions.

Then divide output by envelope, and drop the first 1024 samples (the padding); keep 384000.

- Output: `mine/roundtrip.f32` should equal the input
- Check: `check.py 7`
- Bonus: before the inverse, zero every bin above 4 kHz (a bin is 48000 / 2048 = 23.4 Hz wide)
  and listen: muffled. You just wrote a filter.

## Level 8: BOSS, First Removal

**Goal:** remove music for real, with a mask from the reference (for now).

The model's whole output is a **complex mask**: one complex number per bin and frame. Music =
inverse STFT of (spectrogram × mask).

1. Load `reference/mask.f32` (1025 × 751 × 2).
2. Multiply your spectrogram (times `norm`, as in level 7) by it, complex × complex:
   (a+bi)(c+di) = (ac−bd) + (ad+bc)i.
3. Inverse STFT, as in level 7.

- Output: `mine/music_refmask.f32`
- Check: `check.py 8`
- **Listen:** write `input − music` and play it
  (`ffplay -f f32le -ar 48000 -ch_layout mono file`). The music is gone, using your own code.
  The rest of the game is about computing that mask yourself.

---

# World 3: Neurons

A neural network layer is just multiplies and adds with numbers that were learned in training
(the "weights"). From here on you load the real Bandit v2 weights.

## Level 9: Linear

**Goal:** the layer that does nearly all of the model's work.

`y[i][o] = b[o] + Σk x[i][k] · W[o][k]`

The weight matrix is stored (out × in), PyTorch's convention, so it is `W[o][k]`, not `W[k][o]`.
Inputs: `reference/levels/linear_x.f32` (5 × 6), `linear_w.f32` (4 × 6), `linear_b.f32` (4).

- Output: `mine/linear_y.f32` (5 × 4)
- Check: `check.py 9`
- Keep this function: in World 6 and 7 most of the speed comes from making it faster.

## Level 10: LayerNorm

**Goal:** the layer that keeps numbers in a sensible range.

For each row: `mean`, `var = mean((x − mean)²)` (divide by the count, not count−1),
`y = (x − mean) / sqrt(var + 1e-5) · γ + β`.
Inputs: `reference/levels/ln_x.f32` (5 × 6), `ln_w.f32` (γ, 6), `ln_b.f32` (β, 6).

- Output: `mine/ln_y.f32` (5 × 6)
- Check: `check.py 10`

## Level 11: Band Split

**Goal:** the model's first layer, with real weights, on your own spectrogram.

1. **Weight loader:** `reference/weights.txt` has one line per tensor,
   `name offset count dims...`; the floats are in `reference/weights.bin` starting at
   `offset` (counted in floats). Load the whole file once and look tensors up by name.
2. **Bands:** `reference/band_specs.txt` has 64 lines `start end`: bin ranges [start, end).
   They overlap, and they're narrow at low frequencies, wide at high ones (like the piano).
3. For band i, width bw = end − start, and each frame t: build a feature vector of length 2·bw
   from your level 6 spectrogram (normalized, not × norm):
   `re[start], im[start], re[start+1], im[start+1], …`
4. LayerNorm it with `band_split.norm_fc_modules.<i>.norm.weight` / `.norm.bias`,
   then Linear with `band_split.norm_fc_modules.<i>.fc.weight` (128 × 2bw) / `.fc.bias`.

- Output: `mine/band_split.f32`, shape (64 bands × 751 frames × 128)
- Check: `check.py 11`

## Level 12: BOSS, Mask Head

**Goal:** compute the mask yourself, from the network's last features.

We skip the middle of the network for now (World 4): take its output from
`reference/block15.f32` (64 × 751 × 128). For each band i and frame t, with weights under
`mask_estim.music.norm_mlp.<i>.`:

1. LayerNorm the 128 numbers (`norm.weight`, `norm.bias`).
2. Linear to 512 (`hidden.0.weight` 512 × 128, `hidden.0.bias`), then `tanh`.
3. Linear to 4·bw (`output.0.weight`, `output.0.bias`) → o.
4. GLU: `g[j] = o[j] · sigmoid(o[j + 2bw])` for j < 2bw, with `sigmoid(v) = 1 / (1 + e^−v)`.
   g is re, im, re, im, … for the band's bins.
5. Multiply bin k's (re, im) by `mask_estim.music.freq_weights/<i>` [k] and **add** it into
   `mask[start + k][t]`. Bands overlap, so a bin gets contributions from several bands.

- Output: `mine/mask.f32` (1025 × 751 × 2), and `mine/music_b15.f32`: level 8 with your mask
- Check: `check.py 12`
- Everything from the network's last layer to the sound is now yours.

---

# World 4: Memory

The middle of the network is 16 blocks of **GRUs**: a GRU walks along a sequence and carries a
memory vector h from step to step, so each step knows what came before.

## Level 13: GRU Step

**Goal:** one step of a tiny GRU (input size E = 4, memory size H = 3).

Weights in `reference/levels/`: `gru_weight_ih_l0` (3H × E), `gru_weight_hh_l0` (3H × H),
`gru_bias_ih_l0`, `gru_bias_hh_l0` (3H each). The 3H rows are three blocks of H, in the order
**r, z, n**. With `gi = W_ih·x + b_ih` and `gh = W_hh·h + b_hh` (both length 3H):

```
r  = sigmoid(gi_r + gh_r)
z  = sigmoid(gi_z + gh_z)
n  = tanh(gi_n + r · gh_n)        ← r multiplies gh_n including its bias
h' = (1 − z) · n + z · h
```

Inputs: `gru_x0` (4), `gru_h0` (3).

- Output: `mine/gru_h1.f32` (3)
- Check: `check.py 13`

## Level 14: Both Directions

**Goal:** a GRU over a whole sequence, forwards and backwards.

Input `reference/levels/gru_seq_x.f32`: 7 steps × 4. Start with h = 0.

1. Forward: steps t = 0 … 6, output h after each step → `mine/gru_seq_fwd.f32` (7 × 3).
2. Backward: the `…_reverse` weights, steps t = 6 down to 0, also from h = 0. Store each h at
   **its own position t** (not in the order you computed them).
3. Each output row is `[forward h_t, backward h_t]` → `mine/gru_seq_bi.f32` (7 × 6).

- Check: `check.py 14`

## Level 15: Residual Block

**Goal:** the first real block of the network, at full size (E = 128, H = 256).

Weights `tf_model.seqband.0.*`. Input: your level 11 output z (64 × 751 × 128).

1. `y = LayerNorm(z)` per 128-vector (`norm.weight`, `norm.bias`).
2. For each of the 64 bands: a bidirectional GRU (`rnn.weight_ih_l0`, … as in level 14) along the
   751 frames → `mine/block00_gru.f32` (64 × 751 × 512).
3. `out = Linear(gru)` (`fc.weight` 128 × 512, `fc.bias`) **+ z** (the "residual")
   → `mine/block00_fc.f32` (64 × 751 × 128).
4. Swap the first two axes → `mine/block00.f32` (751 × 64 × 128).

- Check: `check.py 15`
- Speed tip that matters later: W_ih·x doesn't depend on h, so compute it for all 751 steps at once
  (one big matrix multiply) before the loop. Only W_hh·h has to be done step by step.

## Level 16: BOSS, The Whole Network

**Goal:** audio in, music out, no Python anywhere.

Run your level 15 block 16 times, block k with weights `tf_model.seqband.<k>.*`, each on the
previous block's output. Because of the swap, block 1 walks along the 64 bands (for each of the
751 frames), block 2 along frames again, and so on. After 16 swaps you are back at (64 × 751 × 128).
Feed that into your level 12 mask head and level 8 removal.

- Output: `mine/block15.f32` and `mine/music.f32`
- Check: `check.py 16`
- Time it. One chunk is about 2 trillion floating-point operations, so single-threaded C takes
  minutes per chunk even with the speed tip from level 15. Level 17 runs 10 chunks: if that is too
  long to wait, do the OpenMP step of level 18 first.

---

# World 5: The Tool

## Level 17: BOSS, Your Own nomusic

**Goal:** a whole file, both channels, and a video with the music removed.

Using your level 2 decoder on `samples/before.m4a` (30 s stereo):

1. Chunks of 384000 samples, overlapping by 48000 (so a new chunk every 336000), starting at
   s = 0, 336000, … while s < length − 48000. The last chunk runs past the end: fill with zeros.
2. Run each chunk through level 16, **each channel separately** (the model is mono).
3. Keep m = min(384000, length − s) samples of the result. Crossfade, with
   `ramp[k] = (k + 1) / 48001` for k < 48000:
   - if s > 0: multiply the first 48000 samples by ramp[k] (fade in)
   - if the chunk doesn't reach the end: multiply its last 48000 samples by 1 − ramp[k] (fade out)
4. Add into a music buffer at position s. The final output is `input − music`.

- Output: `mine/output.f32` (2 × samples, planar like level 2)
- Check: `check.py 17`
- **Victory lap:** mux it into a video, as the Python version does:

  ```
  ffmpeg -i video.webm -f f32le -ar 48000 -ac 2 -i pipe:0 -map 0:v? -map 1:a -c:v copy out.webm
  ```

  Write interleaved samples into that pipe, then watch the video without music.

---

# World 6: Speed

Speed = seconds of audio ÷ seconds your program took. The Python version does about 38× on the
GPU. Measure level 17 on the whole 30 s file, decode to output included.

## Level 18: Many Cores

**Goal:** use all 8 cores, without losing correctness.

1. Profile first: `perf record ./port/nm 17`, then `perf report`. Expect the Linear and GRU loops at
   the top.
2. Compile with `-O3 -march=native -fopenmp`.
3. Matrix multiply: loop order so the innermost loop walks memory in order, and process
   blocks that fit in cache.
4. Threads: the 64 (or 751) GRU rows of a block are independent: `#pragma omp parallel for`.
   So are the two channels and the chunks.
5. Batch: run the GRU for all rows at once, so each step is one matrix × matrix instead of many
   matrix × vector.

- Check: `check.py 18 --speed <your speed>` (output must still pass)
- Clear at 0.25×, perfect at 0.75×

## Level 19: ISPC

**Goal:** SIMD (the CPU's 8- or 16-wide vector instructions) without writing intrinsics.

Install it (`sudo pacman -S ispc`), move the hot loops (matrix multiply, the GRU gate math,
LayerNorm) into `kernels.ispc`, compile with
`ispc -O3 --target=avx2-i32x8 kernels.ispc -o kernels.o -h kernels.h` and link it in.
Then try `--target=avx512skx-x16` (your Ryzen 7700 has AVX-512).

- Check: `check.py 19 --speed <your speed>`
- Clear at 0.75×, perfect at 1.5× (the CPU's ceiling is around 2× for this model)

---

# World 7: GPU

You have `glslc` (GLSL → SPIR-V compiler) and Vulkan installed. `vulkaninfo` lists two devices:
the RX 7900 XT and the CPU's integrated GPU; pick the one whose type is
`VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU`.

## Level 20: Hello GPU

**Goal:** add two arrays on the GPU. Small goal, big level: Vulkan needs a lot of setup.

Instance → physical device → device with a compute queue → three storage buffers
(host-visible memory is fine for now) → descriptor set → compute pipeline from `add.spv`
(`glslc add.comp -o add.spv`) → command buffer with one dispatch → submit → wait → read back.
Shader: `layout(local_size_x = 256)`, `y[i] = a[i] + b[i]`.

Write it as helpers you will reuse: `make_buffer`, `upload`, `download`,
`make_pipeline(spv, n_buffers)`, `dispatch`.

- Inputs `reference/levels/add_a.f32`, `add_b.f32` (1048576 each); output `mine/gpu_add.f32`
- Check: `check.py 20` (must be exact)

## Level 21: Matmul Shader

**Goal:** a fast matrix multiply, the GPU version of level 9.

`reference/levels/matmul_a.f32` (256 × 512) × `matmul_b.f32` (512 × 768). First one thread per
output element (correct, slow), then tiles: each workgroup loads 16 × 16 blocks of A and B into
`shared` memory and works from there. Measure TFLOPS = 2·256·512·768 / seconds / 10¹².

- Output `mine/gpu_matmul.f32` (256 × 768)
- Check: `check.py 21`
- Achievement: > 5 TFLOPS (on bigger matrices too)

## Level 22: GRU on the GPU

**Goal:** level 15 on the GPU.

Shaders for Linear (level 21), LayerNorm and one GRU step for all rows and both directions.
Record all 751 steps in **one** command buffer with a pipeline barrier between steps; one submit
per step would be far too slow.

- Output `mine/gpu_block00.f32`
- Check: `check.py 22`

## Level 23: Whole Chunk on the GPU

**Goal:** level 16 on the GPU, and fast.

The STFT can stay on the CPU (it is under 1% of the work). Put many chunks and both channels in
one batch, so the matrices are big enough to keep the GPU busy.

- Output `mine/gpu_music.f32`
- Check: `check.py 23 --speed <whole-file speed>`; clear at 5×, perfect at 20×

## Level 24: FINAL BOSS, Half Precision

**Goal:** beat the PyTorch version.

Use 16-bit floats (`GL_EXT_shader_explicit_arithmetic_types_float16`) for weights and activations,
keep sums in 32-bit, then use the 7900 XT's matrix units through cooperative matrices
(`GL_KHR_cooperative_matrix`; your driver supports `VK_KHR_cooperative_matrix`) for the
multiplies.

- Output `mine/gpu16_music.f32`
- Check: `check.py 24 --speed <whole-file speed>`; clear at 35 dB / 20×,
  perfect at 45 dB / 38× (faster than the Python version)
