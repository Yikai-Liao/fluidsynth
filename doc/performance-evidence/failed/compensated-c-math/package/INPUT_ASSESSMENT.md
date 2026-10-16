# Compensated C: read-only mathematical assessment

Date: 2026-10-16. No repository code was compiled or executed. Numbers below come from independent evaluation of the sinusoidal transfer polynomial with coefficients read from the archived source. This is a feasibility screen, not a production or SF2 validation.

## Finding

The omitted pre-emphasis is material. The observed 2.96/5.65 dB losses are predominantly common frequency-dependent attenuation, rather than unavoidable phase ripple. Compensated C merits a narrow mathematical screen before another full experiment. It is not yet a validated replacement for sinc25.

At original-source frequency f=0.40 cycles/sample (theta=pi*f on the 2x grid):

| Kernel | Minimum amplitude | Maximum amplitude | Phase span | Best common boost | Best remaining absolute error |
|---|---:|---:|---:|---:|---:|
| 4-point, fourth-order | 0.711232201886 | 0.711854704708 | 0.007598961 dB | 2.955972287 dB | 0.003799481 dB |
| 6-point, fourth-order | 0.521611720758 | 0.521660149004 | 0.000806391 dB | 5.652649974 dB | 0.000403195 dB |

These extrema are sampled approximations: 10,001 equally spaced centered phases z=-0.5+i/10000. A second grid with f=j/1000, j=0..400, and z=-0.5+i/1000, i=0..1000 found largest phase spans 0.011486642 dB and 0.000806391 dB respectively. A finite grid is not proof of the continuous-frequency maximum. At f=0.40 the sampled maximum residual phase errors relative to the ideal delay were 0.000461057 and 0.000166756 radians. Pre-emphasis does not remove these phase errors.

The archive's static.csv measures the RMS output/reference ratio along a changing-phase sinusoid, not min/max gain over phase. Its single gain figure cannot itself answer whether phase-independent compensation works.

## Exact mathematical test

For each source frequency f, let A_min and A_max be the minimum and maximum response magnitudes over interpolation phase. A positive phase-independent prefilter magnitude P can achieve absolute gain error <=epsilon dB at every phase if and only if

10^(-epsilon/20)/A_min <= P <= 10^(epsilon/20)/A_max.

Equivalently, 20 log10(A_max/A_min) <= 2 epsilon. The minimax choice is P=1/sqrt(A_min*A_max), leaving error epsilon_best=10 log10(A_max/A_min). Thus the project's +/-0.1 dB contract allows at most 0.2 dB peak-to-peak phase spread. If a separate requirement is phase spread <0.1 dB, use that stricter condition as well. These kernels pass both on the sampled grid.

Let a[p,j] be the published z-form coefficient multiplying the jth symmetric sample pair for polynomial power p; j starts at zero for offsets +/-0.5 around the cell midpoint. With theta=pi*f and z=mu-0.5, the centered sinusoidal response is

H(z,f) = 2 sum_j cos((j+0.5)theta)[a[0,j]+a[2,j]z^2+a[4,j]z^4]
         + 2i sum_j sin((j+0.5)theta)[a[1,j]z+a[3,j]z^3].

The ideal response is exp(i theta z); amplitude is abs(H), residual phase is arg(H)-theta*z. These equations, rather than the repository renderer, produced the figures above.

For a stronger cheap screen, at each frequency abs(H)^2 is an even polynomial of degree eight in z, hence a quartic in u=z^2 on [0,1/4]. Evaluate its endpoints and every real root in that interval of its cubic derivative. This removes phase-grid uncertainty without rendering samples; continuous-frequency bounds can then use interval arithmetic/adaptive enclosure. Add the finite prefilter's ripple and quantization to the budget rather than spending all 0.1 dB on the ideal compensation.

## Plain cubic is not an escape at 2x

For ordinary four-point cubic Hermite/Catmull-Rom, the half-phase coefficients are [-1,9,9,-1]/16. On an ideal 2x table, at f=0.40 its half-phase magnitude is

(9/8)cos(pi*f/2) - (1/8)cos(3*pi*f/2) = 0.948771243, or -0.456769743 dB.

The integer-phase gain is exactly one. Those two phases alone force at least 0.45677 dB spread and a best possible common-compensation error >=0.22838 dB. Therefore ideal 2x plus this plain cubic cannot meet the existing +/-0.1 dB all-phase contract through f=0.40, even with phase-independent equalization. This is a mathematical rejection of this specific cubic, not of every polynomial or larger oversampling factor.

## Image/alias and remaining limits

For R(mu,f)=H(mu-0.5,f)exp(-i theta(mu-0.5)), its Fourier coefficients C_k over mu in [0,1] describe reconstruction images. Any phase-independent P multiplies all C_k equally: it cannot improve image/fundamental ratios. Absolute image levels rise with the boost. Images of the ideal 2x table lie at source-normalized f+2k; playback source step r maps them modulo the output sample rate through r(f+2k). Coincident folded components must be summed coherently.

An illustrative independent midpoint quadrature at f=0.40, 32,768 phases, k=-5..5, gave the largest inspected nonzero image/fundamental ratio about -72.56 dB (four-point) and -96.41 dB (six-point). This is neither an all-image bound nor a worst-frequency/ratio guarantee. In particular, four-point must not be sold as meeting an 80 dB image limit on this evidence. The paper's modified SNR is a different, spectrally weighted criterion; do not equate it to the project's measured alias/input target.

Pre-emphasis also does not prevent source content above the output Nyquist from aliasing during upward transposition. That remains a distinct rate-dependent anti-alias requirement. Initial upsampling-filter leakage, coefficient quantization, all relevant image orders and folding ratios must enter the cheap screen before implementation.

The f=0.40 boosts are amplitude factors about 1.405 and 1.917. Preserve floating-point/headroom in the stored representation and prefilter; retaining int16 without an explicit scaling scheme can clip. A frequency-domain boost bound alone is not a bound on every transient's peak. Loop and finite-boundary responses, onset/release transitions, memory expansion and cold-cache costs remain unresolved. Original plus one 2x float table is already about 5x the original int16 bytes; archived loop layouts are larger. No performance benefit has been established.

## Sources

- Niemitalo, sections 6.11-6.12 and 8: https://yehar.com/blog/wp-content/uploads/2009/08/deip.pdf
- Archived coefficients and static-response measurement: https://github.com/Yikai-Liao/fluidsynth/blob/485199bc27ca0698d64aa274b472c07a6390ae00/doc/performance-evidence/failed/oversampled-c/sources/oversampled-c/oversampled_c_screen.cpp
- Archived report: https://github.com/Yikai-Liao/fluidsynth/blob/485199bc27ca0698d64aa274b472c07a6390ae00/doc/performance-evidence/failed/oversampled-c/sources/oversampled-c/REPORT.md

The original plan's frozen contract is +/-0.1 dB through f=0.40, with separate real-sample, alias and boundary gates. Nothing here relaxes those gates.
