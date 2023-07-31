// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.0 (2016/06/19)

#pragma once

#include <ThirdParty/GTEngine/GTEngineDEF.h>

namespace gte
{

// A template class to provide 2D array access that conforms to row-major
// order (RowMajor = true) or column-major order (RowMajor = false).  The
template <bool RowMajor, typename Real, int... Dimensions>
class LexicoArray2 {};

// The array dimensions are known only at run time.
template <typename Real>
class LexicoArray2<true, Real>
{
public:
    inline LexicoArray2(int numRows, int numCols, Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    int mNumRows, mNumCols;
    Real* mMatrix;
};

template <typename Real>
class LexicoArray2<false, Real>
{
public:
    inline LexicoArray2(int numRows, int numCols, Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    int mNumRows, mNumCols;
    Real* mMatrix;
};

// The array dimensions are known at compile time.
template <typename Real, int NumRows, int NumCols>
class LexicoArray2<true, Real, NumRows, NumCols>
{
public:
    inline LexicoArray2(Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    Real* mMatrix;
};

template <typename Real, int NumRows, int NumCols>
class LexicoArray2<false, Real, NumRows, NumCols>
{
public:
    inline LexicoArray2(Real* matrix);

    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline Real& operator()(int r, int c);
    inline Real const& operator()(int r, int c) const;

private:
    Real* mMatrix;
};


template <typename Real> inline
LexicoArray2<true, Real>::LexicoArray2(int numRows, int numCols, Real* matrix)
    :
    mNumRows(numRows),
    mNumCols(numCols),
    mMatrix(matrix)
{
}

template <typename Real> inline
int LexicoArray2<true, Real>::GetNumRows() const
{
    return mNumRows;
}

template <typename Real> inline
int LexicoArray2<true, Real>::GetNumCols() const
{
    return mNumCols;
}

template <typename Real> inline
Real& LexicoArray2<true, Real>::operator()(int r, int c)
{
    return mMatrix[c + mNumCols*r];
}

template <typename Real> inline
Real const& LexicoArray2<true, Real>::operator()(int r, int c) const
{
    return mMatrix[c + mNumCols*r];
}



template <typename Real> inline
LexicoArray2<false, Real>::LexicoArray2(int numRows, int numCols, Real* matrix)
    :
    mNumRows(numRows),
    mNumCols(numCols),
    mMatrix(matrix)
{
}

template <typename Real> inline
int LexicoArray2<false, Real>::GetNumRows() const
{
    return mNumRows;
}

template <typename Real> inline
int LexicoArray2<false, Real>::GetNumCols() const
{
    return mNumCols;
}

template <typename Real> inline
Real& LexicoArray2<false, Real>::operator()(int r, int c)
{
    return mMatrix[r + mNumRows*c];
}

template <typename Real> inline
Real const& LexicoArray2<false, Real>::operator()(int r, int c) const
{
    return mMatrix[r + mNumRows*c];
}



template <typename Real, int NumRows, int NumCols> inline
LexicoArray2<true, Real, NumRows, NumCols>::LexicoArray2(Real* matrix)
    :
    mMatrix(matrix)
{
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<true, Real, NumRows, NumCols>::GetNumRows() const
{
    return NumRows;
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<true, Real, NumRows, NumCols>::GetNumCols() const
{
    return NumCols;
}

template <typename Real, int NumRows, int NumCols> inline
Real& LexicoArray2<true, Real, NumRows, NumCols>::operator()(int r, int c)
{
    return mMatrix[c + NumCols*r];
}

template <typename Real, int NumRows, int NumCols> inline
Real const& LexicoArray2<true, Real, NumRows, NumCols>::operator()(int r, int c) const
{
    return mMatrix[c + NumCols*r];
}



template <typename Real, int NumRows, int NumCols> inline
LexicoArray2<false, Real, NumRows, NumCols>::LexicoArray2(Real* matrix)
    :
    mMatrix(matrix)
{
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<false, Real, NumRows, NumCols>::GetNumRows() const
{
    return NumRows;
}

template <typename Real, int NumRows, int NumCols> inline
int LexicoArray2<false, Real, NumRows, NumCols>::GetNumCols() const
{
    return NumCols;
}

template <typename Real, int NumRows, int NumCols> inline
Real& LexicoArray2<false, Real, NumRows, NumCols>::operator()(int r, int c)
{
    return mMatrix[r + NumRows*c];
}

template <typename Real, int NumRows, int NumCols> inline
Real const& LexicoArray2<false, Real, NumRows, NumCols>::operator()(int r, int c) const
{
    return mMatrix[r + NumRows*c];
}

}
