#ifndef GDEL2D_KERCOMMON_H
#define GDEL2D_KERCOMMON_H

#include "../CommonTypes.h"
#include <device_atomic_functions.h>

namespace gdg
{
__forceinline__ __device__ int getCurThreadIdx()
{
    return static_cast<int>(blockIdx.x * blockDim.x + threadIdx.x);
}

__forceinline__ __device__ int getThreadNum()
{
    return static_cast<int>(gridDim.x * blockDim.x);
}

//////////////////////////////////////////////////////////// Helper functions //

#ifdef REAL_TYPE_FP32
struct __align__(8) RealType2
{
    float _c0, _c1;
};
#else
struct __align__(16) RealType2
{
    double _c0, _c1;
};
#endif

__forceinline__ __device__ FlipItem loadFlip(FlipItem *flipArr, int idx)
{
    int4 t = ((int4 *)flipArr)[idx];

    FlipItem flip = {t.x, t.y, t.z, t.w};

    return flip;
}

__forceinline__ __device__ void storeFlip(FlipItem *flipArr, int idx, const FlipItem &item)
{
    int4 t = {item._v[0], item._v[1], item._t[0], item._t[1]};

    ((int4 *)flipArr)[idx] = t;
}

// Escape -1 (special value)
__forceinline__ __device__ int makePositive(int v)
{
    CudaAssert(v < 0) return -(v + 2);
}

// Escape -1 (special value)
__forceinline__ __device__ int makeNegative(int v)
{
    CudaAssert(v >= 0) return -(v + 2);
}

__forceinline__ __host__ __device__ int encode(int triIdx, int vi)
{
    return (triIdx << 2) | vi;
}

__forceinline__ __host__ __device__ void decode(int code, int *idx, int *vi)
{
    *idx = (code >> 2);
    *vi  = (code & 3);
}

__forceinline__ __device__ void voteForFlip(int *triVoteArr, int botTi, int topTi, int botVi)
{
    const int voteVal = encode(botTi, botVi);
    atomicMin(&triVoteArr[botTi], voteVal);
    atomicMin(&triVoteArr[topTi], voteVal);
}

// idx    Constraint index
// vi     The vertex opposite the next intersected edge. vi = 3 if this is the last triangle
//        --> vi+1 is on the right, vi+2 is on the left of the constraint
// side   Which side of the constraint the vertex vi lies on; 0-cw, 1-ccw, 2-start, 3-end
__forceinline__ __device__ int encode_constraint(int idx, int vi, int side)
{
    return (idx << 4) | (vi << 2) | side;
}

__forceinline__ __device__ int decode_cIdx(int label)
{
    return (label >> 4);
}

__forceinline__ __device__ int decode_cVi(int label)
{
    return (label >> 2) & 3;
}

__forceinline__ __device__ int decode_cSide(int label)
{
    return (label & 3);
}
}
#endif //GDEL2D_KERCOMMON_H