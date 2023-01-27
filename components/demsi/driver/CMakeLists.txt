set( CMAKE_VERBOSE_MAKEFILE on )

message("C++ compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message("Fortran compiler: ${CMAKE_Fortran_COMPILER_ID} ${CMAKE_Fortran_COMPILER_VERSION}")

# Source CIME-generated Macros
include(${CASEROOT}/Macros.cmake)
# Load machine/compiler specific settings
set(COMPILER_SPECIFIC_DEPENDS ${CASEROOT}/Depends.${COMPILER}.cmake)
set(MACHINE_SPECIFIC_DEPENDS ${CASEROOT}/Depends.${MACH}.cmake)
set(PLATFORM_SPECIFIC_DEPENDS ${CASEROOT}/Depends.${MACH}.${COMPILER}.cmake)
set(TRY_TO_LOAD ${COMPILER_SPECIFIC_DEPENDS} ${MACHINE_SPECIFIC_DEPENDS} ${PLATFORM_SPECIFIC_DEPENDS})
foreach(ITEM IN LISTS TRY_TO_LOAD)
  if (EXISTS ${ITEM})
    include(${ITEM})
  endif()
endforeach()

if (USE_ESMF_LIB)
  set(ESMFDIR "esmf")
else()
  set(ESMFDIR "noesmf")
endif()

set(INCLUDES "${INSTALL_SHAREDPATH}/include" "${INSTALL_SHAREDPATH}/${COMP_INTERFACE}/${ESMFDIR}/${NINST_VALUE}/csm_share")

add_library(ice
    ${CMAKE_BINARY_DIR}/demsi/driver/ice_comp_mct.f90)

target_include_directories(ice PRIVATE ${INCLUDES})

add_custom_command(
  OUTPUT ${CMAKE_BINARY_DIR}/demsi/driver/ice_comp_mct.f90
  COMMAND cpp -P -traditional ${CMAKE_CURRENT_SOURCE_DIR}/ice_comp_mct.F > ${CMAKE_BINARY_DIR}/demsi/driver/ice_comp_mct.f90
  DEPENDS ice_comp_mct.F)

add_dependencies(ice demsilib)

message("DEMSI_BUILD_LOC: ${DEMSI_BUILD_LOC}")

target_include_directories(ice PUBLIC "${DEMSI_BUILD_LOC}/model/src/")

find_package(OpenMP)
message("OpenMP_CXX_FLAGS: ${OpenMP_CXX_FLAGS}")
target_link_libraries(ice PRIVATE "${OpenMP_CXX_FLAGS}")
target_compile_options(ice PRIVATE "${OpenMP_CXX_FLAGS}")

#target_link_libraries(ice PUBLIC "gfortran")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/src-install/lib/libdemsilib${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/permon-install/lib/libpermon${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/petsc-install/lib/libpetsc${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/petsc-install/lib/libflapack${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/petsc-install/lib/libfblas${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/compadre-install/lib64/libcompadre${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/LAMMPS-install/lib64/liblammps${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/kokkos-kernels-install/lib64/libkokkoskernels${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/LAMMPS-install/lib64/libkokkoscontainers${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/LAMMPS-install/lib64/libkokkoscore${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/Icepack-install/lib/libIcepack${DEMSI_LIBRARY_SUFFIX}")
target_link_libraries(ice PUBLIC "${DEMSI_BUILD_LOC}/model/build/tinyxml2-install/lib/libtinyxml2${DEMSI_LIBRARY_SUFFIX}")


include_directories(${CMAKE_BINARY_DIR}/demsi/driver/)
link_directories(${CMAKE_BINARY_DIR}/demsi/driver/)
