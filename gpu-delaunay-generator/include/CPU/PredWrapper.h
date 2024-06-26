#ifndef GDEL2D_PREPWRAPPER_H
#define GDEL2D_PREPWRAPPER_H

#include "../CommonTypes.h"

namespace gdg
{
// Shewchuk predicate declarations
void exactinit();

double orient2d(const double *pa, const double *pb, const double *pc);

double orient2dzero(const double *pa, const double *pb, const double *pc);

double incircle(const double *pa, const double *pb, const double *pc, const double *pd);

class PredWrapper
{
  private:
    const Point *_pointArr;
    Point        _ptInfty;
    size_t       _pointNum;

    PredWrapper();
    static Orient doOrient2DSoSOnly(const double *p0, const double *p1, const double *p2, int v0, int v1, int v2);

  public:
    size_t _infIdx;

    PredWrapper(const Point2DHVec &pointVec, Point ptInfty);

    const Point &getPoint(int idx) const;
    size_t       pointNum() const;

    Orient doOrient2D(int v0, int v1, int v2) const;
    Orient doOrient2DFastExactSoS(int v0, int v1, int v2) const;
    Side   doIncircle(Tri tri, int vert) const;
};

#endif //GDEL2D_PREPWRAPPER_H
}