string(APPEND FFLAGS " -convert big_endian -assume byterecl -traceback -assume realloc_lhs -fp-model consistent")
if (compile_threaded)
  string(APPEND FFLAGS " -qopenmp")
endif()
if (NOT DEBUG)
  string(APPEND FFLAGS " -O2")
endif()
if (DEBUG)
  string(APPEND FFLAGS " -O0 -g -check uninit -check bounds -check pointers -fpe0 -check noarg_temp_created")
endif()
string(APPEND CFLAGS " -fp-model precise -std=gnu99 -traceback")
if (compile_threaded)
  string(APPEND CFLAGS " -qopenmp")
endif()
if (NOT DEBUG)
  string(APPEND CFLAGS " -O2")
endif()
if (DEBUG)
  string(APPEND CFLAGS " -O0 -g")
endif()
string(APPEND CXXFLAGS " -fp-model precise -traceback")
if (compile_threaded)
  string(APPEND CXXFLAGS " -qopenmp")
endif()
if (NOT DEBUG)
  string(APPEND CXXFLAGS " -O2")
endif()
if (DEBUG)
  string(APPEND CXXFLAGS " -O0 -g")
endif()
set(SUPPORTS_CXX "TRUE")
set(CXX_LINKER "FORTRAN")
string(APPEND CXX_LDFLAGS " -cxxlib")
string(APPEND CPPDEFS " -DFORTRANUNDERSCORE -DNO_R16 -DCPRINTEL -DHAVE_SLASHPROC")
string(APPEND FC_AUTO_R8 " -r8")
string(APPEND FFLAGS_NOOPT " -O0")
string(APPEND FIXEDFLAGS " -fixed -132")
string(APPEND FREEFLAGS " -free")
set(HAS_F2008_CONTIGUOUS "TRUE")
set(MPIFC "mpif90")
set(MPICC "mpicc")
set(MPICXX "mpicxx")
set(SCC "icc")
set(SCXX "icpc")
set(SFC "ifort")
if (compile_threaded)
  string(APPEND LDFLAGS " -qopenmp")
endif()
string(APPEND SLIBS " -mkl")
