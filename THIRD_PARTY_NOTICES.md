# Third-party code and attribution

## Recursive temporal H-index baseline

The Recurs baseline is attributed to the authors' implementation accompanying:

**Lutz Oettershagen, Nils M. Kriege, and Petra Mutzel.**
*A Higher-Order Temporal H-Index for Evolving Networks.* KDD 2023.

- [Paper](https://doi.org/10.1145/3580305.3599242)
- [Preprint](https://arxiv.org/abs/2305.16001)
- [Upstream repository](https://gitlab.com/tgpublic/tgh)
- Upstream revision recorded in the supplied source package:
  `6c81866b41f72d8608e7111caa73d7b99e264518`

Under `src/vendor/`, the bundled upstream components are `TemporalGraph/TemporalGraphs.h`,
`TemporalGraph/TemporalGraphs.cpp`, `TemporalHIndex/thix_recursive.h`, and
`TemporalHIndex/thix_recursive.cpp`.

The local Recurs adapter provides the common graph input format, timing, and
vertex-score output. The compatibility header supplies the `uint` alias used
by the upstream source.

The supplied source snapshot contains no upstream license file. This notice
records attribution and provenance; it does not grant a license to the
upstream code.

## Streaming baseline

STREAM and its instrumented variant are local reimplementations of the
STREAM algorithm described in the same paper. They are distinct from the
authors' upstream source and support unit transition times.
