string(APPEND CONFIG_ARGS " --host=cray")
if (COMP_NAME STREQUAL gptl)
  string(APPEND CPPDEFS " -DHAVE_NANOTIME -DBIT64 -DHAVE_SLASHPROC -DHAVE_GETTIMEOFDAY")
endif()
set(PIO_FILESYSTEM_HINTS "lustre")
if (NOT DEBUG)
  string(APPEND CFLAGS " -O2 -g")
endif()
if (NOT DEBUG)
  string(APPEND FFLAGS " -O2 -g")
endif()
set(MPICC "cc")
set(MPICXX "CC")
set(MPIFC "ftn")
set(SCC "gcc")
set(SCXX "g++")
set(SFC "gfortran")
