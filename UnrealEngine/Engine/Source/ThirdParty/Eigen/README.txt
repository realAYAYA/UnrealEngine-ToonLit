Macros.h.patch shows a local change which was made to file: Engine\Source\ThirdParty\Eigen\Eigen\src\Core\util\Macros.h to fix a compiler warning for PS4

Changes to the library are demarcated with the following comment format: 
//@UE BEGIN Comment
//@UE END Comment

Memory allocations have been modified to use the engine memory allocator in the following files:
    Engine\Source\ThirdParty\Eigen\Eigen\src\Core\util\Memory.h
    Engine\Source\ThirdParty\Eigen\Eigen\src\SparseCore\SparseMatrix.h
Using the macros EIGEN_STD_MALLOC, EIGEN_STD_REALLOC and EIGEN_STD_FREE. 
Memory allocations have NOT been modified in:
    Engine\Source\ThirdParty\Eigen\Eigen\src\SPQRSupport\SuiteSparseQRSupport.h
To maintain matching allocators with the Suite library if it is ever used. 