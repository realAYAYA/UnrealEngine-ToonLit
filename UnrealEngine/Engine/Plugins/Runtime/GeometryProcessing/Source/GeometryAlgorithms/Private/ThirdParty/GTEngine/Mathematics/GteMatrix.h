// David Eberly, Geometric Tools, Redmond WA 98052
// Copyright (c) 1998-2019
// Distributed under the Boost Software License, Version 1.0.
// http://www.boost.org/LICENSE_1_0.txt
// http://www.geometrictools.com/License/Boost/LICENSE_1_0.txt
// File Version: 3.0.2 (2018/10/05)

#pragma once

#include <ThirdParty/GTEngine/Mathematics/GteVector.h>
#include <ThirdParty/GTEngine/Mathematics/GteGaussianElimination.h>

namespace gte
{

template <int NumRows, int NumCols, typename Real>
class Matrix
{
public:
    // The table is initialized to zero.
    Matrix();

    // The table is fully initialized by the inputs.  The 'values' must be
    // specified in row-major order, regardless of the active storage scheme
    // (GTE_USE_ROW_MAJOR or GTE_USE_COL_MAJOR).
    Matrix(std::array<Real, NumRows*NumCols> const& values);

    // At most NumRows*NumCols are copied from the initializer list, setting
    // any remaining elements to zero.  The 'values' must be specified in
    // row-major order, regardless of the active storage scheme
    // (GTE_USE_ROW_MAJOR or GTE_USE_COL_MAJOR).  Create the zero matrix using
    // the syntax
    //   Matrix<NumRows,NumCols,Real> zero{(Real)0};
    // WARNING: The C++ 11 specification states that
    //   Matrix<NumRows,NumCols,Real> zero{};
    // will lead to a call of the default constructor, not the initializer
    // constructor!
    Matrix(std::initializer_list<Real> values);

    // For 0 <= r < NumRows and 0 <= c < NumCols, element (r,c) is 1 and all
    // others are 0.  If either of r or c is invalid, the zero matrix is
    // created.  This is a convenience for creating the standard Euclidean
    // basis matrices; see also MakeUnit(int,int) and Unit(int,int).
    Matrix(int r, int c);

    // The copy constructor, destructor, and assignment operator are generated
    // by the compiler.

    // Member access for which the storage representation is transparent.  The
    // matrix entry in row r and column c is A(r,c).  The first operator()
    // returns a const reference rather than a Real value.  This supports
    // writing via standard file operations that require a const pointer to
    // data.
    inline Real const& operator()(int r, int c) const;
    inline Real& operator()(int r, int c);

    // Member access by rows or by columns.
    void SetRow(int r, Vector<NumCols,Real> const& vec);
    void SetCol(int c, Vector<NumRows,Real> const& vec);
    Vector<NumCols,Real> GetRow(int r) const;
    Vector<NumRows,Real> GetCol(int c) const;

    // Member access by 1-dimensional index.  NOTE: These accessors are
    // useful for the manipulation of matrix entries when it does not
    // matter whether storage is row-major or column-major.  Do not use
    // constructs such as M[c+NumCols*r] or M[r+NumRows*c] that expose the
    // storage convention.
    inline Real const& operator[](int i) const;
    inline Real& operator[](int i);

    // Comparisons for sorted containers and geometric ordering.
    inline bool operator==(Matrix const& mat) const;
    inline bool operator!=(Matrix const& mat) const;
    inline bool operator< (Matrix const& mat) const;
    inline bool operator<=(Matrix const& mat) const;
    inline bool operator> (Matrix const& mat) const;
    inline bool operator>=(Matrix const& mat) const;

    // Special matrices.
    void MakeZero();  // All components are 0.
    void MakeUnit(int r, int c);  // Component (r,c) is 1, all others zero.
    void MakeIdentity();  // Diagonal entries 1, others 0, even when nonsquare
    static Matrix Zero();
    static Matrix Unit(int r, int c);
    static Matrix Identity();

protected:
    class Table
    {
    public:
        // Storage-order-independent element access as 2D array.
        inline Real const& operator()(int r, int c) const;
        inline Real& operator()(int r, int c);

        // Element access as 1D array.  Use this internally only when
        // the 2D storage order is not relevant.
        inline Real const& operator[](int i) const;
        inline Real& operator[](int i);

#if defined(GTE_USE_ROW_MAJOR)
        std::array<std::array<Real,NumCols>,NumRows> mStorage;
#else
        std::array<std::array<Real,NumRows>,NumCols> mStorage;
#endif
    };

    Table mTable;
};

// Unary operations.
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator+(Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator-(Matrix<NumRows,NumCols,Real> const& M);

// Linear-algebraic operations.
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator+(
    Matrix<NumRows,NumCols,Real> const& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator-(
    Matrix<NumRows,NumCols,Real> const& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator*(Matrix<NumRows,NumCols,Real> const& M, Real scalar);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator*(Real scalar, Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
operator/(Matrix<NumRows,NumCols,Real> const& M, Real scalar);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator+=(
    Matrix<NumRows,NumCols,Real>& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator-=(
    Matrix<NumRows,NumCols,Real>& M0,
    Matrix<NumRows,NumCols,Real> const& M1);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator*=(Matrix<NumRows,NumCols,Real>& M, Real scalar);

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>&
operator/=(Matrix<NumRows,NumCols,Real>& M, Real scalar);

// Geometric operations.
template <int NumRows, int NumCols, typename Real>
Real L1Norm(Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Real L2Norm(Matrix<NumRows,NumCols,Real> const& M);

template <int NumRows, int NumCols, typename Real>
Real LInfinityNorm(Matrix<NumRows,NumCols,Real> const& M);

template <int N, typename Real>
Matrix<N,N,Real> Inverse(Matrix<N,N,Real> const& M,
    bool* reportInvertibility = nullptr);

template <int N, typename Real>
Real Determinant(Matrix<N, N, Real> const& M);

// M^T
template <int NumRows, int NumCols, typename Real>
Matrix<NumCols,NumRows,Real>
Transpose(Matrix<NumRows,NumCols,Real> const& M);

// M*V
template <int NumRows, int NumCols, typename Real>
Vector<NumRows,Real>
operator*(
    Matrix<NumRows,NumCols,Real> const& M,
    Vector<NumCols,Real> const& V);

// V^T*M
template <int NumRows, int NumCols, typename Real>
Vector<NumCols,Real>
operator*(
    Vector<NumRows,Real> const& V,
    Matrix<NumRows,NumCols,Real> const& M);

// A*B
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
operator*(
    Matrix<NumRows,NumCommon,Real> const& A,
    Matrix<NumCommon,NumCols,Real> const& B);

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyAB(
    Matrix<NumRows,NumCommon,Real> const& A,
    Matrix<NumCommon,NumCols,Real> const& B);

// A*B^T
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyABT(
    Matrix<NumRows,NumCommon,Real> const& A,
    Matrix<NumCols,NumCommon,Real> const& B);

// A^T*B
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyATB(
    Matrix<NumCommon,NumRows,Real> const& A,
    Matrix<NumCommon,NumCols,Real> const& B);

// A^T*B^T
template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyATBT(
    Matrix<NumCommon,NumRows,Real> const& A,
    Matrix<NumCols,NumCommon,Real> const& B);

// M*D, D is diagonal NumCols-by-NumCols
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyMD(
    Matrix<NumRows,NumCols,Real> const& M,
    Vector<NumCols,Real> const& D);

// D*M, D is diagonal NumRows-by-NumRows
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
MultiplyDM(
    Vector<NumRows,Real> const& D,
    Matrix<NumRows,NumCols,Real> const& M);

// U*V^T, U is NumRows-by-1, V is Num-Cols-by-1, result is NumRows-by-NumCols.
template <int NumRows, int NumCols, typename Real>
Matrix<NumRows,NumCols,Real>
OuterProduct(Vector<NumRows, Real> const& U, Vector<NumCols, Real> const& V);

// Initialization to a diagonal matrix whose diagonal entries are the
// components of D.
template <int N, typename Real>
void MakeDiagonal(Vector<N, Real> const& D, Matrix<N, N, Real>& M);

// Create an (N+1)-by-(N+1) matrix H by setting the upper N-by-N block to the
// input N-by-N matrix and all other entries to 0 except for the last row
// and last column entry which is set to 1.
template <int N, typename Real>
Matrix<N + 1, N+1, Real> HLift(Matrix<N, N, Real> const& M);

// Extract the upper (N-1)-by-(N-1) block of the input N-by-N matrix.
template <int N, typename Real>
Matrix<N - 1, N - 1, Real> HProject(Matrix<N, N, Real> const& M);


template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix()
{
    MakeZero();
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix(
    std::array<Real, NumRows*NumCols> const& values)
{
    for (int r = 0, i = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c, ++i)
        {
            mTable(r, c) = values[i];
        }
    }
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix(std::initializer_list<Real> values)
{
    int const numValues = static_cast<int>(values.size());
    auto iter = values.begin();
    int r, c, i;
    for (r = 0, i = 0; r < NumRows; ++r)
    {
        for (c = 0; c < NumCols; ++c, ++i)
        {
            if (i < numValues)
            {
                mTable(r, c) = *iter++;
            }
            else
            {
                break;
            }
        }

        if (c < NumCols)
        {
            // Fill in the remaining columns of the current row with zeros.
            for (/**/; c < NumCols; ++c)
            {
                mTable(r, c) = (Real)0;
            }
            ++r;
            break;
        }
    }

    if (r < NumRows)
    {
        // Fill in the remain rows with zeros.
        for (/**/; r < NumRows; ++r)
        {
            for (c = 0; c < NumCols; ++c)
            {
                mTable(r, c) = (Real)0;
            }
        }
    }
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>::Matrix(int r, int c)
{
    MakeUnit(r, c);
}

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::operator()(int r, int c) const
{
    return mTable(r, c);
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::operator()(int r, int c)
{
    return mTable(r, c);
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::SetRow(int r,
    Vector<NumCols, Real> const& vec)
{
    for (int c = 0; c < NumCols; ++c)
    {
        mTable(r, c) = vec[c];
    }
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::SetCol(int c,
    Vector<NumRows, Real> const& vec)
{
    for (int r = 0; r < NumRows; ++r)
    {
        mTable(r, c) = vec[r];
    }
}

template <int NumRows, int NumCols, typename Real>
Vector<NumCols, Real> Matrix<NumRows, NumCols, Real>::GetRow(int r) const
{
    Vector<NumCols, Real> vec;
    for (int c = 0; c < NumCols; ++c)
    {
        vec[c] = mTable(r, c);
    }
    return vec;
}

template <int NumRows, int NumCols, typename Real>
Vector<NumRows, Real> Matrix<NumRows, NumCols, Real>::GetCol(int c) const
{
    Vector<NumRows, Real> vec;
    for (int r = 0; r < NumRows; ++r)
    {
        vec[r] = mTable(r, c);
    }
    return vec;
}

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::operator[](int i) const
{
    return mTable[i];
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::operator[](int i)
{
    return mTable[i];
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator==(Matrix const& mat) const
{
    return mTable.mStorage == mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator!=(Matrix const& mat) const
{
    return mTable.mStorage != mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator<(Matrix const& mat) const
{
    return mTable.mStorage < mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator<=(Matrix const& mat) const
{
    return mTable.mStorage <= mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator>(Matrix const& mat) const
{
    return mTable.mStorage > mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real> inline
bool Matrix<NumRows, NumCols, Real>::operator>=(Matrix const& mat) const
{
    return mTable.mStorage >= mat.mTable.mStorage;
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::MakeZero()
{
    Real const zero = (Real)0;
    for (int i = 0; i < NumRows * NumCols; ++i)
    {
        mTable[i] = zero;
    }
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::MakeUnit(int r, int c)
{
    MakeZero();
    if (0 <= r && r < NumRows && 0 <= c && c < NumCols)
    {
        mTable(r, c) = (Real)1;
    }
}

template <int NumRows, int NumCols, typename Real>
void Matrix<NumRows, NumCols, Real>::MakeIdentity()
{
    MakeZero();
    int const numDiagonal = (NumRows <= NumCols ? NumRows : NumCols);
    for (int i = 0; i < numDiagonal; ++i)
    {
        mTable(i, i) = (Real)1;
    }
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real> Matrix<NumRows, NumCols, Real>::Zero()
{
    Matrix M;
    M.MakeZero();
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real> Matrix<NumRows, NumCols, Real>::Unit(int r,
    int c)
{
    Matrix M;
    M.MakeUnit(r, c);
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real> Matrix<NumRows, NumCols, Real>::Identity()
{
    Matrix M;
    M.MakeIdentity();
    return M;
}



template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator+(Matrix<NumRows, NumCols, Real> const& M)
{
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator-(Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        result[i] = -M[i];
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator+(
    Matrix<NumRows, NumCols, Real> const& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    Matrix<NumRows, NumCols, Real> result = M0;
    return result += M1;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator-(
    Matrix<NumRows, NumCols, Real> const& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    Matrix<NumRows, NumCols, Real> result = M0;
    return result -= M1;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator*(Matrix<NumRows, NumCols, Real> const& M, Real scalar)
{
    Matrix<NumRows, NumCols, Real> result = M;
    return result *= scalar;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator*(Real scalar, Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumRows, NumCols, Real> result = M;
    return result *= scalar;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
operator/(Matrix<NumRows, NumCols, Real> const& M, Real scalar)
{
    Matrix<NumRows, NumCols, Real> result = M;
    return result /= scalar;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator+=(
    Matrix<NumRows, NumCols, Real>& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        M0[i] += M1[i];
    }
    return M0;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator-=(
    Matrix<NumRows, NumCols, Real>& M0,
    Matrix<NumRows, NumCols, Real> const& M1)
{
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        M0[i] -= M1[i];
    }
    return M0;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator*=(Matrix<NumRows, NumCols, Real>& M, Real scalar)
{
    for (int i = 0; i < NumRows*NumCols; ++i)
    {
        M[i] *= scalar;
    }
    return M;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>&
operator/=(Matrix<NumRows, NumCols, Real>& M, Real scalar)
{
    if (scalar != (Real)0)
    {
        Real invScalar = ((Real)1) / scalar;
        for (int i = 0; i < NumRows*NumCols; ++i)
        {
            M[i] *= invScalar;
        }
    }
    else
    {
        for (int i = 0; i < NumRows*NumCols; ++i)
        {
            M[i] = (Real)0;
        }
    }
    return M;
}

template <int NumRows, int NumCols, typename Real>
Real L1Norm(Matrix<NumRows, NumCols, Real> const& M)
{
    Real sum = std::abs(M[0]);
    for (int i = 1; i < NumRows*NumCols; ++i)
    {
        sum += std::abs(M[i]);
    }
    return sum;
}

template <int NumRows, int NumCols, typename Real>
Real L2Norm(Matrix<NumRows, NumCols, Real> const& M)
{
    Real sum = M[0] * M[0];
    for (int i = 1; i < NumRows*NumCols; ++i)
    {
        sum += M[i] * M[i];
    }
    return std::sqrt(sum);
}

template <int NumRows, int NumCols, typename Real>
Real LInfinityNorm(Matrix<NumRows, NumCols, Real> const& M)
{
    Real maxAbsElement = M[0];
    for (int i = 1; i < NumRows*NumCols; ++i)
    {
        Real absElement = std::abs(M[i]);
        if (absElement > maxAbsElement)
        {
            maxAbsElement = absElement;
        }
    }
    return maxAbsElement;
}

template <int N, typename Real>
Matrix<N, N, Real> Inverse(Matrix<N, N, Real> const& M,
    bool* reportInvertibility)
{
    Matrix<N, N, Real> invM;
    Real determinant;
    bool invertible = GaussianElimination<Real>()(N, &M[0], &invM[0],
        determinant, nullptr, nullptr, nullptr, 0, nullptr);
    if (reportInvertibility)
    {
        *reportInvertibility = invertible;
    }
    return invM;
}

template <int N, typename Real>
Real Determinant(Matrix<N, N, Real> const& M)
{
    Real determinant;
    GaussianElimination<Real>()(N, &M[0], nullptr, determinant, nullptr,
        nullptr, nullptr, 0, nullptr);
    return determinant;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumCols, NumRows, Real>
Transpose(Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumCols, NumRows, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(c, r) = M(r, c);
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Vector<NumRows, Real>
operator*(
    Matrix<NumRows, NumCols, Real> const& M,
    Vector<NumCols, Real> const& V)
{
    Vector<NumRows, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        result[r] = (Real)0;
        for (int c = 0; c < NumCols; ++c)
        {
            result[r] += M(r, c) * V[c];
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Vector<NumCols, Real> operator*(Vector<NumRows, Real> const& V,
    Matrix<NumRows, NumCols, Real> const& M)
{
    Vector<NumCols, Real> result;
    for (int c = 0; c < NumCols; ++c)
    {
        result[c] = (Real)0;
        for (int r = 0; r < NumRows; ++r)
        {
            result[c] += V[r] * M(r, c);
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
operator*(
    Matrix<NumRows, NumCommon, Real> const& A,
    Matrix<NumCommon, NumCols, Real> const& B)
{
    return MultiplyAB(A, B);
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyAB(
    Matrix<NumRows, NumCommon, Real> const& A,
    Matrix<NumCommon, NumCols, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(r, i) * B(i, c);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyABT(
    Matrix<NumRows, NumCommon, Real> const& A,
    Matrix<NumCols, NumCommon, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(r, i) * B(c, i);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyATB(
    Matrix<NumCommon, NumRows, Real> const& A,
    Matrix<NumCommon, NumCols, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(i, r) * B(i, c);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, int NumCommon, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyATBT(
    Matrix<NumCommon, NumRows, Real> const& A,
    Matrix<NumCols, NumCommon, Real> const& B)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = (Real)0;
            for (int i = 0; i < NumCommon; ++i)
            {
                result(r, c) += A(i, r) * B(c, i);
            }
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyMD(
    Matrix<NumRows, NumCols, Real> const& M,
    Vector<NumCols, Real> const& D)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = M(r, c) * D[c];
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
MultiplyDM(
    Vector<NumRows, Real> const& D,
    Matrix<NumRows, NumCols, Real> const& M)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = D[r] * M(r, c);
        }
    }
    return result;
}

template <int NumRows, int NumCols, typename Real>
Matrix<NumRows, NumCols, Real>
OuterProduct(Vector<NumRows, Real> const& U, Vector<NumCols, Real> const& V)
{
    Matrix<NumRows, NumCols, Real> result;
    for (int r = 0; r < NumRows; ++r)
    {
        for (int c = 0; c < NumCols; ++c)
        {
            result(r, c) = U[r] * V[c];
        }
    }
    return result;
}

template <int N, typename Real>
void MakeDiagonal(Vector<N, Real> const& D, Matrix<N, N, Real>& M)
{
    for (int i = 0; i < N*N; ++i)
    {
        M[i] = (Real)0;
    }

    for (int i = 0; i < N; ++i)
    {
        M(i, i) = D[i];
    }
}

template <int N, typename Real>
Matrix<N + 1, N + 1, Real> HLift(Matrix<N, N, Real> const& M)
{
    Matrix<N + 1, N + 1, Real> result;
    result.MakeIdentity();
    for (int r = 0; r < N; ++r)
    {
        for (int c = 0; c < N; ++c)
        {
            result(r, c) = M(r, c);
        }
    }
    return result;
}

// Extract the upper (N-1)-by-(N-1) block of the input N-by-N matrix.
template <int N, typename Real>
Matrix<N - 1, N - 1, Real> HProject(Matrix<N, N, Real> const& M)
{
    static_assert(N >= 2, "Invalid matrix dimension.");
    Matrix<N - 1, N - 1, Real> result;
    for (int r = 0; r < N - 1; ++r)
    {
        for (int c = 0; c < N - 1; ++c)
        {
            result(r, c) = M(r, c);
        }
    }
    return result;
}


// Matrix<N,C,Real>::Table

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::Table::operator()(int r, int c)
    const
{
#if defined(GTE_USE_ROW_MAJOR)
    return mStorage[r][c];
#else
    return mStorage[c][r];
#endif
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::Table::operator()(int r, int c)
{
#if defined(GTE_USE_ROW_MAJOR)
    return mStorage[r][c];
#else
    return mStorage[c][r];
#endif
}

template <int NumRows, int NumCols, typename Real> inline
Real const& Matrix<NumRows, NumCols, Real>::Table::operator[](int i) const
{
    Real const* elements = &mStorage[0][0];
    return elements[i];
}

template <int NumRows, int NumCols, typename Real> inline
Real& Matrix<NumRows, NumCols, Real>::Table::operator[](int i)
{
    Real* elements = &mStorage[0][0];
    return elements[i];
}


}
