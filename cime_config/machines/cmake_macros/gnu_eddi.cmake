if (COMP_NAME STREQUAL gptl)
  string(APPEND CPPDEFS " -DHAVE_VPRINTF -DHAVE_GETTIMEOFDAY -DHAVE_BACKTRACE")
endif()
set(NETCDF_PATH "$ENV{NETCDF_HOME}")
if (NOT DEBUG)
  string(APPEND FFLAGS " -fno-unsafe-math-optimizations")
endif()
if (DEBUG)
  string(APPEND FFLAGS " -g -fbacktrace -fbounds-check -ffpe-trap=invalid,zero,overflow")
endif()
string(APPEND SLIBS " -L$ENV{NETCDF_HOME}/lib/ -lnetcdff -lnetcdf -lcurl -llapack -lblas")
