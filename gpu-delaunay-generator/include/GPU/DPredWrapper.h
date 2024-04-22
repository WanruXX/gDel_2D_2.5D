#ifndef GDEL2D_DPREPWRAPPER_H
#define GDEL2D_DPREPWRAPPER_H

#include "../CommonTypes.h"
namespace gdg
{
class DPredWrapper
{
  private:
    Point  *_pointArr    = nullptr;
    int    *_orgPointIdx = nullptr;
    int     _pointNum    = 0;
    double *_predConsts  = nullptr;

    __forceinline__ __device__ Orient doOrient2DFastExact(const double *p0, const double *p1, const double *p2) const;

    static __forceinline__ __device__ Orient
    doOrient2DSoSOnly(const double *p0, const double *p1, const double *p2, int v0, int v1, int v2);

    __forceinline__ __device__ Side doInCircleFastExact(const double *p0,
                                                        const double *p1,
                                                        const double *p2,
                                                        const double *p3) const;

    __forceinline__ __device__ double doOrient1DExact_Lifted(const double *p0, const double *p1) const;

    __forceinline__ __device__ double
    doOrient2DExact_Lifted(const double *p0, const double *p1, const double *p2, bool lifted) const;

    __forceinline__ __device__ Side doInCircleSoSOnly(const double *p0,
                                                      const double *p1,
                                                      const double *p2,
                                                      const double *p3,
                                                      int           v0,
                                                      int           v1,
                                                      int           v2,
                                                      int           v3) const;

  public:
    int _infIdx = 0;

    DPredWrapper() = default;

    void init(Point *pointArr, int pointNum, int *orgPointIdx, int infIdx);

    void cleanup();

    __forceinline__ __device__ __host__ int pointNum() const;

    __forceinline__ __device__ const Point &getPoint(int idx) const;

    __forceinline__ __device__ int getPointIdx(int idx) const;

    __forceinline__ __device__ Orient doOrient2DFast(int v0, int v1, int v2) const;

    __forceinline__ __device__ Orient doOrient2DFastExactSoS(int v0, int v1, int v2) const;

    __forceinline__ __device__ Side doInCircleFast(Tri tri, int vert) const;

    __forceinline__ __device__ Side doInCircleFastExactSoS(Tri tri, int vert) const;

    __forceinline__ __device__ double inCircleDet(Tri tri, int vert) const;
};
}
#endif //GDEL2D_DPREPWRAPPER_H