if (NOT DEBUG)
  string(APPEND CFLAGS " -O2")
  string(APPEND FFLAGS " -O2")
endif()
if (COMP_NAME STREQUAL gptl)
  string(APPEND CPPDEFS " -DHAVE_SLASHPROC")
endif()
string(APPEND CPPDEFS " -DTHRUST_IGNORE_CUB_VERSION_CHECK")
set(MPICXX "mpiCC")
#string(APPEND CUDA_FLAGS " -O3 -arch sm_70 --use_fast_math")
set(USE_CUDA "TRUE")
