#include "../include/DelaunayChecker.h"
#include "../../inc/json.h"

namespace gdg
{
DelaunayChecker::DelaunayChecker(const Input &inputRef, Output &outputRef)
    : input(inputRef), output(outputRef), predWrapper(inputRef.pointVec, outputRef.infPt)
{
}

size_t DelaunayChecker::getVertexCount() const
{
    const TriHVec &triVec = output.triVec;
    std::set<int>  vertSet;
    // Add vertices
    for (auto tri : triVec)
    {
        vertSet.insert(tri._v, tri._v + DEG);
    }
    return vertSet.size();
}

size_t DelaunayChecker::getSegmentCount() const
{
    const TriHVec &triVec = output.triVec;
    const int      triNum = (int)triVec.size();

    std::set<Edge> segSet;

    //    // Read segments
    //    Edge segArr[TriSegNum];
    //    for (int ti = 0; ti < triNum; ++ti)
    //    {
    //        const Tri &tri = triVec[ti];
    //        GpuDel::getEdges(tri, segArr);
    //        segSet.insert(segArr, segArr + TriSegNum);
    //    }
    return segSet.size();
}

size_t DelaunayChecker::getTriangleCount()
{
    return output.triVec.size();
}

void DelaunayChecker::checkEuler()
{
    const auto v = getVertexCount();
    std::cout << "Vertex: " << v;

    const auto e = getSegmentCount();
    std::cout << " Edge: " << e;

    const auto f = getTriangleCount();
    std::cout << " Triangle: " << f;

    const auto euler = v - e + f;
    std::cout << " Euler: " << euler << std::endl;

    std::cout << "Euler check: " << ((1 != euler) ? " ***Fail***" : " Pass") << std::endl;
}

void printTriAndOpp(int ti, const Tri &tri, const TriOpp &opp)
{
    printf("triIdx: %d [ %d %d %d ] ( %d:%d %d:%d %d:%d )\n",
           ti,
           tri._v[0],
           tri._v[1],
           tri._v[2],
           opp.getOppTri(0),
           opp.getOppVi(0),
           opp.getOppTri(1),
           opp.getOppVi(1),
           opp.getOppTri(2),
           opp.getOppVi(2));
}

void DelaunayChecker::checkAdjacency() const
{
    const TriHVec    triVec = output.triVec;
    const TriOppHVec oppVec = output.triOppVec;

    for (int ti0 = 0; ti0 < (int)triVec.size(); ++ti0)
    {
        const Tri    &tri0 = triVec[ti0];
        const TriOpp &opp0 = oppVec[ti0];

        for (int vi = 0; vi < DEG; ++vi)
        {
            if (-1 == opp0._t[vi])
                continue;

            const int ti1   = opp0.getOppTri(vi);
            const int vi0_1 = opp0.getOppVi(vi);

            const Tri    &tri1 = triVec[ti1];
            const TriOpp &opp1 = oppVec[ti1];

            if (-1 == opp1._t[vi0_1])
            {
                std::cout << "Fail4!" << std::endl;
                continue;
            }

            if (ti0 != opp1.getOppTri(vi0_1))
            {
                std::cout << "Not opp of each other! Tri0: " << ti0 << " Tri1: " << ti1 << std::endl;
                printTriAndOpp(ti0, tri0, opp0);
                printTriAndOpp(ti1, tri1, opp1);
                continue;
            }

            if (vi != opp1.getOppVi(vi0_1))
            {
                std::cout << "Vi mismatch! Tri0: " << ti0 << "Tri1: " << ti1 << std::endl;
                continue;
            }
        }
    }

    std::cout << "Adjacency check: Pass\n";
}

void DelaunayChecker::checkOrientation()
{
    const TriHVec triVec = output.triVec;

    int count = 0;

    for (auto t : triVec)
    {
        const Orient ord = predWrapper.doOrient2DFastExactSoS(t._v[0], t._v[1], t._v[2]);
        if (OrientNeg == ord)
            ++count;
    }

    std::cout << "Orient check: ";
    if (count)
        std::cout << "***Fail*** Wrong orient: " << count;
    else
        std::cout << "Pass";
    std::cout << "\n";
}

void DelaunayChecker::checkDelaunay()
{
    const TriHVec    triVec = output.triVec;
    const TriOppHVec oppVec = output.triOppVec;

    const int triNum  = (int)triVec.size();
    int       failNum = 0;

    for (int botTi = 0; botTi < triNum; ++botTi)
    {
        const Tri    botTri = triVec[botTi];
        const TriOpp botOpp = oppVec[botTi];

        for (int botVi = 0; botVi < DEG; ++botVi) // Face neighbours
        {
            // No face neighbour or facing constraint
            if (-1 == botOpp._t[botVi] || botOpp.isOppConstraint(botVi))
                continue;

            const int topVi = botOpp.getOppVi(botVi);
            const int topTi = botOpp.getOppTri(botVi);

            if (topTi < botTi)
                continue; // Neighbour will check

            const Tri  topTri  = triVec[topTi];
            const int  topVert = topTri._v[topVi];
            const Side side    = predWrapper.doIncircle(botTri, topVert);

            if (SideIn != side)
                continue;

            ++failNum;
        }
    }

    std::cout << "\nDelaunay check: ";

    if (failNum == 0)
        std::cout << "Pass" << std::endl;
    else
        std::cout << "***Fail*** Failed faces: " << failNum << std::endl;
}

void DelaunayChecker::checkConstraints()
{
    if (input.constraintVec.empty())
        return;

    const TriHVec  triVec  = output.triVec;
    TriOppHVec    &oppVec  = output.triOppVec;
    const EdgeHVec consVec = input.constraintVec;

    const int triNum  = (int)triVec.size();
    int       failNum = 0;

    // Clear any existing opp constraint info.
    for (int i = 0; i < triNum; ++i)
        for (int j = 0; j < 3; ++j)
            if (oppVec[i]._t[j] != -1)
                oppVec[i].setOppConstraint(j, false);

    // Create a vertex to triangle map
    IntHVec vertTriMap(predWrapper.pointNum(), -1);

    for (int i = 0; i < triNum; ++i)
        for (int v : triVec[i]._v)
            vertTriMap[v] = i;

    // Check the constraints
    for (int i = 0; i < consVec.size(); ++i)
    {
        Edge constraint = consVec[i];

        const int startIdx = vertTriMap[constraint._v[0]];

        if (startIdx < 0)
        {
            ++failNum;
            continue;
        }

        int triIdx = startIdx;
        int vi     = triVec[triIdx].getIndexOf(constraint._v[0]);

        // Walk around the starting vertex to find the constraint edge
        const int MaxWalking = 1000000;
        int       j          = 0;

        for (; j < MaxWalking; ++j)
        {
            const Tri &tri      = triVec[triIdx];
            TriOpp    &opp      = oppVec[triIdx];
            const int  nextVert = tri._v[(vi + 2) % 3];

            // The constraint is already inserted
            if (nextVert == constraint._v[1])
            {
                vi = (vi + 1) % DEG;
                j  = INT_MAX;
                break;
            }

            // Rotate
            if (opp._t[(vi + 1) % DEG] == -1)
                break;

            triIdx = opp.getOppTri((vi + 1) % DEG);
            vi     = opp.getOppVi((vi + 1) % DEG);
            vi     = (vi + 1) % DEG;

            if (triIdx == startIdx)
                break;
        }

        // If not found, rotate the other direction
        if (j < MaxWalking)
        {
            triIdx = startIdx;
            vi     = triVec[triIdx].getIndexOf(constraint._v[0]);

            for (; j < MaxWalking; ++j)
            {
                const Tri &tri      = triVec[triIdx];
                const int  nextVert = tri._v[(vi + 1) % 3];

                if (nextVert == constraint._v[1])
                {
                    vi = (vi + 2) % DEG;
                    j  = INT_MAX;
                    break;
                }

                // Rotate
                const TriOpp &opp = oppVec[triIdx];

                if (opp._t[(vi + 2) % DEG] == -1)
                    break;

                triIdx = opp.getOppTri((vi + 2) % DEG);
                vi     = opp.getOppVi((vi + 2) % DEG);
                vi     = (vi + 2) % DEG;

                if (triIdx == startIdx)
                    break;
            }
        }

        if (j == INT_MAX) // Found
        {
            TriOpp &opp = oppVec[triIdx];

            const int oppTri = opp.getOppTri(vi);
            const int oppVi  = opp.getOppVi(vi);

            opp.setOppConstraint(vi, true);
            oppVec[oppTri].setOppConstraint(oppVi, true);
        }
        else
        {
            if (j >= MaxWalking)
                std::cout << "Vertex degree too high; Skipping constraint " << i << std::endl;
            ++failNum;
        }
    }

    std::cout << "\nConstraint check: ";

    if (failNum == 0)
        std::cout << "Pass" << std::endl;
    else
        std::cout << "***Fail*** Missing constraints: " << failNum << std::endl;
}
}