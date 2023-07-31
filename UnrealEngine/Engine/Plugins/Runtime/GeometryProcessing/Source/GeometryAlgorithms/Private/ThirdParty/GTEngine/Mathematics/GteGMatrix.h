// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteGVector.h>
#include <ThirdParty/GTEngine/Mathematics/GteGaussianElimination.h>
#include <algorithm>

// Uncomment these to test for out-of-range indices and size mismatches.
//#define GTE_ASSERT_ON_GMATRIX_INDEX_OUT_OF_RANGE
//#define GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH

namespace gte
{

template <typename Real>
class GMatrix
{
public:
    // The table is length zero and mNumRows and mNumCols are set to zero.
    GMatrix();

    // The table is length numRows*numCols and the elements are initialized
    // to zero.
    GMatrix(int numRow, int numCols);

    // For 0 <= r < numRows and 0 <= c < numCols, element (r,c) is 1 and all
    // others are 0.  If either of r or c is invalid, the zero matrix is
    // created.  This is a convenience for creating the standard Euclidean
    // basis matrices; see also MakeUnit(int,int) and Unit(int,int).
    GMatrix(int numRows, int numCols, int r, int c);

    // The copy constructor, destructor, and assignment operator are generated
    // by the compiler.

    // Member access for which the storage representation is transparent.  The
    // matrix entry in row r and column c is A(r,c).  The first operator()
    // returns a const reference rather than a Real value.  This supports
    // writing via standard file operations that require a const pointer to
    // data.
    void SetSize(int numRows, int numCols);
    inline void GetSize(int& numRows, int& numCols) const;
    inline int GetNumRows() const;
    inline int GetNumCols() const;
    inline int GetNumElements() const;
    inline Real const& operator()(int r, int c) const;
    inline Real& operator()(int r, int c);

    // Member access by rows or by columns.  The input vectors must have the
    // correct number of elements for the matrix size.
    void SetRow(int r, GVector<Real> const& vec);
    void SetCol(int c, GVector<Real> const& vec);
    GVector<Real> GetRow(int r) const;
    GVector<Real> GetCol(int c) const;

    // Member access by 1-dimensional index.  NOTE: These accessors are
    // useful for the manipulation of matrix entries when it does not
    // matter whether storage is row-major or column-major.  Do not use
    // constructs such as M[c+NumCols*r] or M[r+NumRows*c] that expose the
    // storage convention.
    inline Real const& operator[](int i) const;
    inline Real& operator[](int i);

    // Comparisons for sorted containers and geometric ordering.
    inline bool operator==(GMatrix const& mat) const;
    inline bool operator!=(GMatrix const& mat) const;
    inline bool operator< (GMatrix const& mat) const;
    inline bool operator<=(GMatrix const& mat) const;
    inline bool operator> (GMatrix const& mat) const;
    inline bool operator>=(GMatrix const& mat) const;

    // Special matrices.
    void MakeZero();  // All components are 0.
    void MakeUnit(int r, int c);  // Component (r,c) is 1, all others zero.
    void MakeIdentity();  // Diagonal entries 1, others 0, even when nonsquare
    static GMatrix Zero(int numRows, int numCols);
    static GMatrix Unit(int numRows, int numCols, int r, int c);
    static GMatrix Identity(int numRows, int numCols);

protected:
    // The matrix is stored as a 1-dimensional array.  The convention of
    // row-major or column-major is your choice.
    int mNumRows, mNumCols;
    std::vector<Real> mElements;
};

// Unary operations.
template <typename Real>
GMatrix<Real> operator+(GMatrix<Real> const& M);

template <typename Real>
GMatrix<Real> operator-(GMatrix<Real> const& M);

// Linear-algebraic operations.
template <typename Real>
GMatrix<Real> operator+(GMatrix<Real> const& M0, GMatrix<Real> const& M1);

template <typename Real>
GMatrix<Real> operator-(GMatrix<Real> const& M0, GMatrix<Real> const& M1);

template <typename Real>
GMatrix<Real> operator*(GMatrix<Real> const& M, Real scalar);

template <typename Real>
GMatrix<Real> operator*(Real scalar, GMatrix<Real> const& M);

template <typename Real>
GMatrix<Real> operator/(GMatrix<Real> const& M, Real scalar);

template <typename Real>
GMatrix<Real>& operator+=(GMatrix<Real>& M0, GMatrix<Real> const& M1);

template <typename Real>
GMatrix<Real>& operator-=(GMatrix<Real>& M0, GMatrix<Real> const& M1);

template <typename Real>
GMatrix<Real>& operator*=(GMatrix<Real>& M, Real scalar);

template <typename Real>
GMatrix<Real>& operator/=(GMatrix<Real>& M, Real scalar);

// Geometric operations.
template <typename Real>
Real L1Norm(GMatrix<Real> const& M);

template <typename Real>
Real L2Norm(GMatrix<Real> const& M);

template <typename Real>
Real LInfinityNorm(GMatrix<Real> const& M);

template <typename Real>
GMatrix<Real> Inverse(GMatrix<Real> const& M,
    bool* reportInvertibility = nullptr);

template <typename Real>
Real Determinant(GMatrix<Real> const& M);

// M^T
template <typename Real>
GMatrix<Real> Transpose(GMatrix<Real> const& M);

// M*V
template <typename Real>
GVector<Real> operator*(GMatrix<Real> const& M, GVector<Real> const& V);

// V^T*M
template <typename Real>
GVector<Real> operator*(GVector<Real> const& V, GMatrix<Real> const& M);

// A*B
template <typename Real>
GMatrix<Real> operator*(GMatrix<Real> const& A, GMatrix<Real> const& B);

template <typename Real>
GMatrix<Real> MultiplyAB(GMatrix<Real> const& A, GMatrix<Real> const& B);

// A*B^T
template <typename Real>
GMatrix<Real> MultiplyABT(GMatrix<Real> const& A, GMatrix<Real> const& B);

// A^T*B
template <typename Real>
GMatrix<Real> MultiplyATB(GMatrix<Real> const& A, GMatrix<Real> const& B);

// A^T*B^T
template <typename Real>
GMatrix<Real> MultiplyATBT(GMatrix<Real> const& A, GMatrix<Real> const& B);

// M*D, D is square diagonal (stored as vector)
template <typename Real>
GMatrix<Real> MultiplyMD(GMatrix<Real> const& M, GVector<Real> const& D);

// D*M, D is square diagonal (stored as vector)
template <typename Real>
GMatrix<Real> MultiplyDM(GVector<Real> const& D, GMatrix<Real> const& M);

// U*V^T, U is N-by-1, V is M-by-1, result is N-by-M.
template <typename Real>
GMatrix<Real> OuterProduct(GVector<Real> const& U, GVector<Real> const& V);

// Initialization to a diagonal matrix whose diagonal entries are the
// components of D.
template <typename Real>
void MakeDiagonal(GVector<Real> const& D, GMatrix<Real>& M);


template <typename Real>
GMatrix<Real>::GMatrix()
    :
    mNumRows(0),
    mNumCols(0)
{
}

template <typename Real>
GMatrix<Real>::GMatrix(int numRows, int numCols)
{
    SetSize(numRows, numCols);
    std::fill(mElements.begin(), mElements.end(), (Real)0);
}

template <typename Real>
GMatrix<Real>::GMatrix(int numRows, int numCols, int r, int c)
{
    SetSize(numRows, numCols);
    MakeUnit(r, c);
}

template <typename Real>
void GMatrix<Real>::SetSize(int numRows, int numCols)
{
    if (numRows > 0 && numCols > 0)
    {
        mNumRows = numRows;
        mNumCols = numCols;
        mElements.resize(mNumRows * mNumCols);
    }
    else
    {
        mNumRows = 0;
        mNumCols = 0;
        mElements.clear();
    }
}

template <typename Real> inline
void GMatrix<Real>::GetSize(int& numRows, int& numCols) const
{
    numRows = mNumRows;
    numCols = mNumCols;
}

template <typename Real> inline
int GMatrix<Real>::GetNumRows() const
{
    return mNumRows;
}

template <typename Real> inline
int GMatrix<Real>::GetNumCols() const
{
    return mNumCols;
}

template <typename Real> inline
int GMatrix<Real>::GetNumElements() const
{
    return static_cast<int>(mElements.size());
}

template <typename Real> inline
Real const& GMatrix<Real>::operator()(int r, int c) const
{
#if defined(GTE_ASSERT_ON_GMATRIX_INDEX_OUT_OF_RANGE)
    LogAssert(0 <= r && r < GetNumRows() && 0 <= c && c < GetNumCols(),
        "Invalid index.");
#endif

#if defined(GTE_USE_ROW_MAJOR)
    return mElements[c + mNumCols*r];
#else
    return mElements[r + mNumRows*c];
#endif
}

template <typename Real> inline
Real& GMatrix<Real>::operator()(int r, int c)
{
#if defined(GTE_ASSERT_ON_GMATRIX_INDEX_OUT_OF_RANGE)
    LogAssert(0 <= r && r < GetNumRows() && 0 <= c && c < GetNumCols(),
        "Invalid index.");
#endif

#if defined(GTE_USE_ROW_MAJOR)
    return mElements[c + mNumCols*r];
#else
    return mElements[r + mNumRows*c];
#endif
}

template <typename Real>
void GMatrix<Real>::SetRow(int r, GVector<Real> const& vec)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(vec.GetSize() == GetNumCols(), "Mismatched size.");
#endif
    for (int c = 0; c < mNumCols; ++c)
    {
        operator()(r, c) = vec[c];
    }
}

template <typename Real>
void GMatrix<Real>::SetCol(int c, GVector<Real> const& vec)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(vec.GetSize() == GetNumRows(), "Mismatched size.");
#endif
    for (int r = 0; r < mNumRows; ++r)
    {
        operator()(r, c) = vec[r];
    }
}

template <typename Real>
GVector<Real> GMatrix<Real>::GetRow(int r) const
{
    GVector<Real> vec(mNumCols);
    for (int c = 0; c < mNumCols; ++c)
    {
        vec[c] = operator()(r, c);
    }
    return vec;
}

template <typename Real>
GVector<Real> GMatrix<Real>::GetCol(int c) const
{
    GVector<Real> vec(mNumRows);
    for (int r = 0; r < mNumRows; ++r)
    {
        vec[r] = operator()(r, c);
    }
    return vec;
}

template <typename Real> inline
Real const& GMatrix<Real>::operator[](int i) const
{
    return mElements[i];
}

template <typename Real> inline
Real& GMatrix<Real>::operator[](int i)
{
    return mElements[i];
}

template <typename Real> inline
bool GMatrix<Real>::operator==(GMatrix const& mat) const
{
    return mNumRows == mat.mNumRows && mNumCols == mat.mNumCols
        && mElements == mat.mElements;
}

template <typename Real> inline
bool GMatrix<Real>::operator!=(GMatrix const& mat) const
{
    return !operator==(mat);
}

template <typename Real> inline
bool GMatrix<Real>::operator<(GMatrix const& mat) const
{
    return mNumRows == mat.mNumRows && mNumCols == mat.mNumCols
        && mElements < mat.mElements;
}

template <typename Real> inline
bool GMatrix<Real>::operator<=(GMatrix const& mat) const
{
    return mNumRows == mat.mNumRows && mNumCols == mat.mNumCols
        && mElements <= mat.mElements;
}

template <typename Real> inline
bool GMatrix<Real>::operator>(GMatrix const& mat) const
{
    return mNumRows == mat.mNumRows && mNumCols == mat.mNumCols
        && mElements > mat.mElements;
}

template <typename Real> inline
bool GMatrix<Real>::operator>=(GMatrix const& mat) const
{
    return mNumRows == mat.mNumRows && mNumCols == mat.mNumCols
        && mElements >= mat.mElements;
}

template <typename Real>
void GMatrix<Real>::MakeZero()
{
    std::fill(mElements.begin(), mElements.end(), (Real)0);
}

template <typename Real>
void GMatrix<Real>::MakeUnit(int r, int c)
{
    MakeZero();
    if (0 <= r && r < mNumRows && 0 <= c && c < mNumCols)
    {
        operator()(r, c) = (Real)1;
    }
}

template <typename Real>
void GMatrix<Real>::MakeIdentity()
{
    MakeZero();
    int const numDiagonal = (mNumRows <= mNumCols ? mNumRows : mNumCols);
    for (int i = 0; i < numDiagonal; ++i)
    {
        operator()(i, i) = (Real)1;
    }
}

template <typename Real>
GMatrix<Real> GMatrix<Real>::Zero(int numRows, int numCols)
{
    GMatrix<Real> M(numRows, numCols);
    M.MakeZero();
    return M;
}

template <typename Real>
GMatrix<Real> GMatrix<Real>::Unit(int numRows, int numCols, int r, int c)
{
    GMatrix<Real> M(numRows, numCols);
    M.MakeUnit(r, c);
    return M;
}

template <typename Real>
GMatrix<Real> GMatrix<Real>::Identity(int numRows, int numCols)
{
    GMatrix<Real> M(numRows, numCols);
    M.MakeIdentity();
    return M;
}



template <typename Real>
GMatrix<Real> operator+(GMatrix<Real> const& M)
{
    return M;
}

template <typename Real>
GMatrix<Real> operator-(GMatrix<Real> const& M)
{
    GMatrix<Real> result(M.GetNumRows(), M.GetNumCols());
    for (int i = 0; i < M.GetNumElements(); ++i)
    {
        result[i] = -M[i];
    }
    return result;
}

template <typename Real>
GMatrix<Real> operator+(GMatrix<Real> const& M0, GMatrix<Real> const& M1)
{
    GMatrix<Real> result = M0;
    return result += M1;
}

template <typename Real>
GMatrix<Real> operator-(GMatrix<Real> const& M0, GMatrix<Real> const& M1)
{
    GMatrix<Real> result = M0;
    return result -= M1;
}

template <typename Real>
GMatrix<Real> operator*(GMatrix<Real> const& M, Real scalar)
{
    GMatrix<Real> result = M;
    return result *= scalar;
}

template <typename Real>
GMatrix<Real> operator*(Real scalar, GMatrix<Real> const& M)
{
    GMatrix<Real> result = M;
    return result *= scalar;
}

template <typename Real>
GMatrix<Real> operator/(GMatrix<Real> const& M, Real scalar)
{
    GMatrix<Real> result = M;
    return result /= scalar;
}

template <typename Real>
GMatrix<Real>& operator+=(GMatrix<Real>& M0, GMatrix<Real> const& M1)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(M0.GetNumRows() == M1.GetNumRows() &&
        M0.GetNumCols() == M1.GetNumCols(), "Mismatched size.");
#endif

    for (int i = 0; i < M0.GetNumElements(); ++i)
    {
        M0[i] += M1[i];
    }
    return M0;
}

template <typename Real>
GMatrix<Real>& operator-=(GMatrix<Real>& M0, GMatrix<Real> const& M1)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(M0.GetNumRows() == M1.GetNumRows() &&
        M0.GetNumCols() == M1.GetNumCols(), "Mismatched size.");
#endif

    for (int i = 0; i < M0.GetNumElements(); ++i)
    {
        M0[i] -= M1[i];
    }
    return M0;
}

template <typename Real>
GMatrix<Real>& operator*=(GMatrix<Real>& M, Real scalar)
{
    for (int i = 0; i < M.GetNumElements(); ++i)
    {
        M[i] *= scalar;
    }
    return M;
}

template <typename Real>
GMatrix<Real>& operator/=(GMatrix<Real>& M, Real scalar)
{
    if (scalar != (Real)0)
    {
        Real invScalar = ((Real)1) / scalar;
        for (int i = 0; i < M.GetNumElements(); ++i)
        {
            M[i] *= invScalar;
        }
    }
    else
    {
        for (int i = 0; i < M.GetNumElements(); ++i)
        {
            M[i] = (Real)0;
        }
    }
    return M;
}

template <typename Real>
Real L1Norm(GMatrix<Real> const& M)
{
    Real sum = std::abs(M[0]);
    for (int i = 1; i < M.GetNumElements(); ++i)
    {
        sum += std::abs(M[i]);
    }
    return sum;
}

template <typename Real>
Real L2Norm(GMatrix<Real> const& M)
{
    Real sum = M[0] * M[0];
    for (int i = 1; i < M.GetNumElements(); ++i)
    {
        sum += M[i] * M[i];
    }
    return std::sqrt(sum);
}

template <typename Real>
Real LInfinityNorm(GMatrix<Real> const& M)
{
    Real maxAbsElement = M[0];
    for (int i = 1; i < M.GetNumElements(); ++i)
    {
        Real absElement = std::abs(M[i]);
        if (absElement > maxAbsElement)
        {
            maxAbsElement = absElement;
        }
    }
    return maxAbsElement;
}

template <typename Real>
GMatrix<Real> Inverse(GMatrix<Real> const& M, bool* reportInvertibility)
{
    GMatrix<Real> invM(M.GetNumRows(), M.GetNumCols());
    if (M.GetNumRows() == M.GetNumCols())
    {
        Real determinant;
        bool invertible = GaussianElimination<Real>()(M.GetNumRows(), &M[0],
            &invM[0], determinant, nullptr, nullptr, nullptr, 0, nullptr);
        if (reportInvertibility)
        {
            *reportInvertibility = invertible;
        }
    }
    else
    {
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
        LogError("Matrix must be square.");
#endif
        invM.MakeZero();
        if (reportInvertibility)
        {
            *reportInvertibility = false;
        }
    }
    return invM;
}

template <typename Real>
Real Determinant(GMatrix<Real> const& M)
{
    Real determinant;
    if (M.GetNumRows() == M.GetNumCols())
    {
        GaussianElimination<Real>()(M.GetNumRows(), &M[0], nullptr,
            determinant, nullptr, nullptr, nullptr, 0, nullptr);
    }
    else
    {
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
        LogError("Matrix must be square.");
#endif
        determinant = (Real)0;
    }
    return determinant;
}

template <typename Real>
GMatrix<Real> Transpose(GMatrix<Real> const& M)
{
    GMatrix<Real> result(M.GetNumCols(), M.GetNumRows());
    for (int r = 0; r < M.GetNumRows(); ++r)
    {
        for (int c = 0; c < M.GetNumCols(); ++c)
        {
            result(c, r) = M(r, c);
        }
    }
    return result;
}

template <typename Real>
GVector<Real> operator*(GMatrix<Real> const& M, GVector<Real> const& V)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(V.GetSize() == M.GetNumRows(), "Mismatched size.");
#endif
    GVector<Real> result(M.GetNumRows());
    for (int r = 0; r < M.GetNumRows(); ++r)
    {
        result[r] = (Real)0;
        for (int c = 0; c < M.GetNumCols(); ++c)
        {
            result[r] += M(r, c) * V[c];
        }
    }
    return result;
}

template <typename Real>
GVector<Real> operator*(GVector<Real> const& V, GMatrix<Real> const& M)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(V.GetSize() == M.GetNumCols(), "Mismatched size.");
#endif
    GVector<Real> result(M.GetNumCols());
    for (int c = 0; c < M.GetNumCols(); ++c)
    {
        result[c] = (Real)0;
        for (int r = 0; r < M.GetNumRows(); ++r)
        {
            result[c] += V[r] * M(r, c);
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> operator*(GMatrix<Real> const& A, GMatrix<Real> const& B)
{
    return MultiplyAB(A, B);
}

template <typename Real>
GMatrix<Real> MultiplyAB(GMatrix<Real> const& A, GMatrix<Real> const& B)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(A.GetNumCols() == B.GetNumRows(), "Mismatched size.");
#endif
    int const numCommon = A.GetNumCols();
    GMatrix<Real> result(A.GetNumRows(), B.GetNumCols());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < numCommon; ++i)
            {
                result(r, c) += A(r, i) * B(i, c);
            }
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> MultiplyABT(GMatrix<Real> const& A, GMatrix<Real> const& B)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(A.GetNumCols() == B.GetNumCols(), "Mismatched size.");
#endif
    int const numCommon = A.GetNumCols();
    GMatrix<Real> result(A.GetNumRows(), B.GetNumRows());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < numCommon; ++i)
            {
                result(r, c) += A(r, i) * B(c, i);
            }
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> MultiplyATB(GMatrix<Real> const& A, GMatrix<Real> const& B)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(A.GetNumRows() == B.GetNumRows(), "Mismatched size.");
#endif
    int const numCommon = A.GetNumRows();
    GMatrix<Real> result(A.GetNumCols(), B.GetNumCols());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < numCommon; ++i)
            {
                result(r, c) += A(i, r) * B(i, c);
            }
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> MultiplyATBT(GMatrix<Real> const& A, GMatrix<Real> const& B)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(A.GetNumRows() == B.GetNumCols(), "Mismatched size.");
#endif
    int const numCommon = A.GetNumRows();
    GMatrix<Real> result(A.GetNumCols(), B.GetNumRows());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < numCommon; ++i)
            {
                result(r, c) += A(i, r) * B(c, i);
            }
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> MultiplyMD(GMatrix<Real> const& M, GVector<Real> const& D)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(D.GetSize() == M.GetNumCols(), "Mismatched size.");
#endif
    GMatrix<Real> result(M.GetNumRows(), M.GetNumCols());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = M(r, c) * D[c];
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> MultiplyDM(GVector<Real> const& D, GMatrix<Real> const& M)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(D.GetSize() == M.GetNumRows(), "Mismatched size.");
#endif
    GMatrix<Real> result(M.GetNumRows(), M.GetNumCols());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = D[r] * M(r, c);
        }
    }
    return result;
}

template <typename Real>
GMatrix<Real> OuterProduct(GVector<Real> const& U, GVector<Real> const& V)
{
    GMatrix<Real> result(U.GetSize(), V.GetSize());
    for (int r = 0; r < result.GetNumRows(); ++r)
    {
        for (int c = 0; c < result.GetNumCols(); ++c)
        {
            result(r, c) = U[r] * V[c];
        }
    }
    return result;
}

template <typename Real>
void MakeDiagonal(GVector<Real> const& D, GMatrix<Real>& M)
{
#if defined(GTE_ASSERT_ON_GMATRIX_SIZE_MISMATCH)
    LogAssert(M.GetNumRows() == M.GetNumCols(), "Mismatched size.");
#endif
    int const N = M.GetNumRows();
    for (int i = 0; i < N*N; ++i)
    {
        M[i] = (Real)0;
    }

    for (int i = 0; i < N; ++i)
    {
        M(i, i) = D[i];
    }
}


}
