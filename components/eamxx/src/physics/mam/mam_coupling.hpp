#ifndef MAM_COUPLING_HPP
#define MAM_COUPLING_HPP

#include <mam4xx/mam4.hpp>
#include <mam4xx/conversions.hpp>
#include <ekat/kokkos/ekat_subview_utils.hpp>
#include <share/atm_process/ATMBufferManager.hpp>
#include <share/util/scream_common_physics_functions.hpp>

// These data structures and functions are used to move data between EAMxx
// and mam4xx. This file must be adjusted whenever the aerosol modes and
// species are modified.

namespace scream::mam_coupling {

using KT            = ekat::KokkosTypes<ekat::DefaultDevice>;

// views for single- and multi-column data
using view_1d       = typename KT::template view_1d<Real>;
using view_2d       = typename KT::template view_2d<Real>;
using const_view_1d = typename KT::template view_1d<const Real>;
using const_view_2d = typename KT::template view_2d<const Real>;

// Kokkos thread team (league member)
using Team = Kokkos::TeamPolicy<KT::ExeSpace>::member_type;

// unmanaged views (for buffer and workspace manager)
using uview_1d = typename ekat::template Unmanaged<typename KT::template view_1d<Real>>;
using uview_2d = typename ekat::template Unmanaged<typename KT::template view_2d<Real>>;

using PF = scream::PhysicsFunctions<DefaultDevice>;

// number of constituents in gas chemistry "work arrays"
KOKKOS_INLINE_FUNCTION
constexpr int gas_pcnst() {
  constexpr int gas_pcnst_ = mam4::gas_chemistry::gas_pcnst;
  return gas_pcnst_;
}

// number of aerosol/gas species tendencies
KOKKOS_INLINE_FUNCTION
constexpr int nqtendbb() { return 4; }

// returns the number of distinct aerosol modes
KOKKOS_INLINE_FUNCTION
constexpr int num_aero_modes() {
  return mam4::AeroConfig::num_modes();
}

// returns the number of distinct aerosol species
KOKKOS_INLINE_FUNCTION
constexpr int num_aero_species() {
  return mam4::AeroConfig::num_aerosol_ids();
}

// returns the number of distinct aerosol-related gases
KOKKOS_INLINE_FUNCTION
constexpr int num_aero_gases() {
  return mam4::AeroConfig::num_gas_ids();
}

// returns the total number of aerosol tracers (i.e. the total number of
// distinct valid mode-species pairs)
KOKKOS_INLINE_FUNCTION
constexpr int num_aero_tracers() {
  // see mam4::mode_aero_species() for valid per-mode aerosol species
  // (in mam4xx/aero_modes.hpp)
  return 7 + 4 + 7 + 3;
}

// Given a MAM aerosol mode index, returns a string denoting the symbolic
// name of the mode.
KOKKOS_INLINE_FUNCTION
const char* aero_mode_name(const int mode) {
  static const char *mode_names[num_aero_modes()] = {
    "1",
    "2",
    "3",
    "4",
  };
  return mode_names[mode];
}

// Given a MAM aerosol species ID, returns a string denoting the symbolic
// name of the species.
KOKKOS_INLINE_FUNCTION
const char* aero_species_name(const int species_id) {
  static const char *species_names[num_aero_species()] = {
    "soa",
    "so4",
    "pom",
    "bc",
    "nacl",
    "dst",
    "mom",
  };
  return species_names[species_id];
}

// Given a MAM aerosol-related gas ID, returns a string denoting the symbolic
// name of the gas species.
KOKKOS_INLINE_FUNCTION
const char* gas_species_name(const int gas_id) {
  static const char *species_names[num_aero_gases()] = {
    "o3",
    "h2o2",
    "h2so4",
    "so2",
    "dms",
    "soag"
  };
  return species_names[gas_id];
}

// here we provide storage for names of fields generated by the functions below
namespace {

#define MAX_FIELD_NAME_LEN 128

KOKKOS_INLINE_FUNCTION
size_t gpu_strlen(const char* s) {
  size_t l = 0;
  while (s[l]) ++l;
  return l;
}

KOKKOS_INLINE_FUNCTION
void concat_2_strings(const char *s1, const char *s2, char *concatted) {
  size_t len1 = gpu_strlen(s1);
  for (size_t i = 0; i < len1; ++i)
    concatted[i] = s1[i];
  size_t len2 = gpu_strlen(s2);
  for (size_t i = 0; i < len2; ++i)
    concatted[i + len1] = s2[i];
  concatted[len1+len2] = 0;
}

KOKKOS_INLINE_FUNCTION
void concat_3_strings(const char *s1, const char *s2, const char *s3, char *concatted) {
  size_t len1 = gpu_strlen(s1);
  for (size_t i = 0; i < len1; ++i)
    concatted[i] = s1[i];
  size_t len2 = gpu_strlen(s2);
  for (size_t i = 0; i < len2; ++i)
    concatted[i + len1] = s2[i];
  size_t len3 = gpu_strlen(s3);
  for (size_t i = 0; i < len3; ++i)
    concatted[i + len1 + len2] = s3[i];
  concatted[len1+len2+len3] = 0;
}

KOKKOS_INLINE_FUNCTION
char* int_aero_nmr_names(int mode) {
  static char int_aero_nmr_names_[num_aero_modes()][MAX_FIELD_NAME_LEN] = {};
  return int_aero_nmr_names_[mode];
}

KOKKOS_INLINE_FUNCTION
char* cld_aero_nmr_names(int mode) {
  static char cld_aero_nmr_names_[num_aero_modes()][MAX_FIELD_NAME_LEN] = {};
  return cld_aero_nmr_names_[mode];
}

KOKKOS_INLINE_FUNCTION
char* int_aero_mmr_names(int mode, int species) {
  static char int_aero_mmr_names_[num_aero_modes()][num_aero_species()][MAX_FIELD_NAME_LEN] = {};
  return int_aero_mmr_names_[mode][species];
}

KOKKOS_INLINE_FUNCTION
char* cld_aero_mmr_names(int mode, int species) {
  static char cld_aero_mmr_names_[num_aero_modes()][num_aero_species()][MAX_FIELD_NAME_LEN] = {};
  return cld_aero_mmr_names_[mode][species];
}

KOKKOS_INLINE_FUNCTION
char* gas_mmr_names(int gas_id) {
  static char gas_mmr_names_[num_aero_gases()][MAX_FIELD_NAME_LEN] = {};
  return gas_mmr_names_[gas_id];
}

} // end anonymous namespace

// Given a MAM aerosol mode index, returns the name of the related interstitial
// modal number mixing ratio field in EAMxx ("num_a<1-based-mode-index>")
KOKKOS_INLINE_FUNCTION
const char* int_aero_nmr_field_name(const int mode) {
  if (!int_aero_nmr_names(mode)[0]) {
    concat_2_strings("num_a", aero_mode_name(mode), int_aero_nmr_names(mode));
  }
  return const_cast<const char*>(int_aero_nmr_names(mode));
}

// Given a MAM aerosol mode index, returns the name of the related cloudborne
// modal number mixing ratio field in EAMxx ("num_c<1-based-mode-index>>")
KOKKOS_INLINE_FUNCTION
const char* cld_aero_nmr_field_name(const int mode) {
  if (!cld_aero_nmr_names(mode)[0]) {
    concat_2_strings("num_c", aero_mode_name(mode), cld_aero_nmr_names(mode));
  }
  return const_cast<const char*>(cld_aero_nmr_names(mode));
}

// Given a MAM aerosol mode index and the index of the MAM aerosol species
// within it, returns the name of the relevant interstitial mass mixing ratio
// field in EAMxx. The form of the field name is "<species>_a<1-based-mode-index>".
// If the desired species is not present within the desire mode, returns a blank
// string ("").
KOKKOS_INLINE_FUNCTION
const char* int_aero_mmr_field_name(const int mode, const int species) {
  if (!int_aero_mmr_names(mode, species)[0]) {
    const auto aero_id = mam4::mode_aero_species(mode, species);
    if (aero_id != mam4::AeroId::None) {
      concat_3_strings(aero_species_name(static_cast<int>(aero_id)),
                       "_a", aero_mode_name(mode),
                       int_aero_mmr_names(mode, species));
    }
  }
  return const_cast<const char*>(int_aero_mmr_names(mode, species));
};

// Given a MAM aerosol mode index and the index of the MAM aerosol species
// within it, returns the name of the relevant cloudborne mass mixing ratio
// field in EAMxx. The form of the field name is "<species>_c<1-based-mode-index>".
// If the desired species is not present within the desire mode, returns a blank
// string ("").
KOKKOS_INLINE_FUNCTION
const char* cld_aero_mmr_field_name(const int mode, const int species) {
  if (!cld_aero_mmr_names(mode, species)[0]) {
    const auto aero_id = mam4::mode_aero_species(mode, species);
    if (aero_id != mam4::AeroId::None) {
      concat_3_strings(aero_species_name(static_cast<int>(aero_id)),
                       "_c", aero_mode_name(mode),
                       int_aero_mmr_names(mode, species));
    }
  }
  return const_cast<const char*>(cld_aero_mmr_names(mode, species));
};

// Given a MAM aerosol-related gas identifier, returns the name of its mass
// mixing ratio field in EAMxx ("aero_gas_mmr_<gas>")
KOKKOS_INLINE_FUNCTION
const char* gas_mmr_field_name(const int gas) {
  if (!gas_mmr_names(gas)[0]) {
    concat_2_strings("aero_gas_mmr_", gas_species_name(gas), gas_mmr_names(gas));
  }
  return const_cast<const char*>(gas_mmr_names(gas));
}

// This type stores multi-column views related specifically to the wet
// atmospheric state used by EAMxx.
struct WetAtmosphere {
  const_view_2d qv;      // wet water vapor specific humidity [kg vapor / kg moist air]
  const_view_2d qc;      // wet cloud liquid water mass mixing ratio [kg cloud water/kg moist air]
  const_view_2d nc;      // wet cloud liquid water number mixing ratio [# / kg moist air]
  const_view_2d qi;      // wet cloud ice water mass mixing ratio [kg cloud ice water / kg moist air]
  const_view_2d ni;      // wet cloud ice water number mixing ratio [# / kg moist air]
  const_view_2d omega;   // vertical pressure velocity [Pa/s]
};

// This type stores multi-column views related to the dry atmospheric state
// used by MAM.
struct DryAtmosphere {
  Real          z_surf;    // height of bottom of atmosphere [m]
  const_view_2d T_mid;     // temperature at grid midpoints [K]
  const_view_2d p_mid;     // total pressure at grid midpoints [Pa]
  view_2d       qv;        // dry water vapor mixing ratio [kg vapor / kg dry air]
  view_2d       qc;        // dry cloud liquid water mass mixing ratio [kg cloud water/kg dry air]
  view_2d       nc;        // dry cloud liquid water number mixing ratio [# / kg dry air]
  view_2d       qi;        // dry cloud ice water mass mixing ratio [kg cloud ice water / kg dry air]
  view_2d       ni;        // dry cloud ice water number mixing ratio [# / kg dry air]
  view_2d       z_mid;     // height at layer midpoints [m]
  view_2d       z_iface;   // height at layer interfaces [m]
  view_2d       dz;        // layer thickness [m]
  const_view_2d p_del;     // hydrostatic "pressure thickness" at grid interfaces [Pa]
  const_view_2d cldfrac;   // cloud fraction [-]
  view_2d       w_updraft; // updraft velocity [m/s]
  const_view_1d pblh;      // planetary boundary layer height [m]
  const_view_1d phis;      // surface geopotential [m2/s2]
};

// This type stores aerosol number and mass mixing ratios evolved by MAM. It
// can be used to represent wet and dry aerosols. When you declare an
// AerosolState, you must decide whether it's a dry or wet aerosol state (with
// mixing ratios in terms of dry or wet parcels of air, respectively).
// These mixing ratios are organized by mode (and species, for mass mixing ratio)
// in the same way as they are in mam4xx, and indexed using mam4::AeroConfig.
struct AerosolState {
  view_2d int_aero_nmr[num_aero_modes()]; // modal interstitial aerosol number mixing ratios [# / kg air]
  view_2d cld_aero_nmr[num_aero_modes()]; // modal cloudborne aerosol number mixing ratios [# / kg air]
  view_2d int_aero_mmr[num_aero_modes()][num_aero_species()]; // interstitial aerosol mass mixing ratios [kg aerosol / kg air]
  view_2d cld_aero_mmr[num_aero_modes()][num_aero_species()]; // cloudborne aerosol mass mixing ratios [kg aerosol / kg air]
  view_2d gas_mmr[num_aero_gases()]; // gas mass mixing ratios [kg gas / kg air]
};

// storage for variables used within MAM atmosphere processes, initialized with
// ATMBufferManager
struct Buffer {
  // ======================
  // column midpoint fields
  // ======================

  // number of local fields stored at column midpoints
  static constexpr int num_2d_mid = 8 + // number of dry atm fields
                                    2 * (num_aero_modes() + num_aero_tracers()) +
                                    num_aero_gases();

  // (dry) atmospheric state
  uview_2d z_mid;     // height at midpoints
  uview_2d dz;        // layer thickness
  uview_2d qv_dry;    // dry water vapor mixing ratio (dry air)
  uview_2d qc_dry;    // dry cloud water mass mixing ratio
  uview_2d nc_dry;    // dry cloud water number mixing ratio
  uview_2d qi_dry;    // cloud ice mass mixing ratio
  uview_2d ni_dry;    // dry cloud ice number mixing ratio
  uview_2d w_updraft; // vertical wind velocity

  // aerosol dry interstitial/cloudborne number/mass mixing ratios
  // (because the number of species per mode varies, not all of these will
  //  be used)
  uview_2d dry_int_aero_nmr[num_aero_modes()];
  uview_2d dry_cld_aero_nmr[num_aero_modes()];
  uview_2d dry_int_aero_mmr[num_aero_modes()][num_aero_species()];
  uview_2d dry_cld_aero_mmr[num_aero_modes()][num_aero_species()];

  // aerosol-related dry gas mass mixing ratios
  uview_2d dry_gas_mmr[num_aero_gases()];

  // =======================
  // column interface fields
  // =======================

  // number of local fields stored at column interfaces
  static constexpr int num_2d_iface = 1;

  uview_2d z_iface; // height at interfaces

  // storage
  Real* wsm_data;
};

// ON HOST, returns the number of bytes of device memory needed by the above
// Buffer type given the number of columns and vertical levels
inline size_t buffer_size(const int ncol, const int nlev) {
  return sizeof(Real) *
    (Buffer::num_2d_mid * ncol * nlev +
     Buffer::num_2d_iface * ncol * (nlev+1));
}

// ON HOST, initializeѕ the Buffer type with sufficient memory to store
// intermediate (dry) quantities on the given number of columns with the given
// number of vertical levels. Returns the number of bytes allocated.
inline size_t init_buffer(const ATMBufferManager &buffer_manager,
                          const int ncol, const int nlev,
                          Buffer &buffer) {
  Real* mem = reinterpret_cast<Real*>(buffer_manager.get_memory());

  // set view pointers for midpoint fields
  uview_2d* view_2d_mid_ptrs[Buffer::num_2d_mid] = {
    &buffer.z_mid,
    &buffer.dz,
    &buffer.qv_dry,
    &buffer.qc_dry,
    &buffer.nc_dry,
    &buffer.qi_dry,
    &buffer.ni_dry,
    &buffer.w_updraft,

    // aerosol modes
    &buffer.dry_int_aero_nmr[0],
    &buffer.dry_int_aero_nmr[1],
    &buffer.dry_int_aero_nmr[2],
    &buffer.dry_int_aero_nmr[3],
    &buffer.dry_cld_aero_nmr[0],
    &buffer.dry_cld_aero_nmr[1],
    &buffer.dry_cld_aero_nmr[2],
    &buffer.dry_cld_aero_nmr[3],

    // the following requires knowledge of mam4's mode-species layout
    // (see mode_aero_species() in mam4xx/aero_modes.hpp)

    // accumulation mode
    &buffer.dry_int_aero_mmr[0][0],
    &buffer.dry_int_aero_mmr[0][1],
    &buffer.dry_int_aero_mmr[0][2],
    &buffer.dry_int_aero_mmr[0][3],
    &buffer.dry_int_aero_mmr[0][4],
    &buffer.dry_int_aero_mmr[0][5],
    &buffer.dry_int_aero_mmr[0][6],
    &buffer.dry_cld_aero_mmr[0][0],
    &buffer.dry_cld_aero_mmr[0][1],
    &buffer.dry_cld_aero_mmr[0][2],
    &buffer.dry_cld_aero_mmr[0][3],
    &buffer.dry_cld_aero_mmr[0][4],
    &buffer.dry_cld_aero_mmr[0][5],
    &buffer.dry_cld_aero_mmr[0][6],

    // aitken mode
    &buffer.dry_int_aero_mmr[1][0],
    &buffer.dry_int_aero_mmr[1][1],
    &buffer.dry_int_aero_mmr[1][2],
    &buffer.dry_int_aero_mmr[1][3],
    &buffer.dry_cld_aero_mmr[1][0],
    &buffer.dry_cld_aero_mmr[1][1],
    &buffer.dry_cld_aero_mmr[1][2],
    &buffer.dry_cld_aero_mmr[1][3],

    // coarse mode
    &buffer.dry_int_aero_mmr[2][0],
    &buffer.dry_int_aero_mmr[2][1],
    &buffer.dry_int_aero_mmr[2][2],
    &buffer.dry_int_aero_mmr[2][3],
    &buffer.dry_int_aero_mmr[2][4],
    &buffer.dry_int_aero_mmr[2][5],
    &buffer.dry_int_aero_mmr[2][6],
    &buffer.dry_cld_aero_mmr[2][0],
    &buffer.dry_cld_aero_mmr[2][1],
    &buffer.dry_cld_aero_mmr[2][2],
    &buffer.dry_cld_aero_mmr[2][3],
    &buffer.dry_cld_aero_mmr[2][4],
    &buffer.dry_cld_aero_mmr[2][5],
    &buffer.dry_cld_aero_mmr[2][6],

    // primary carbon mode
    &buffer.dry_int_aero_mmr[3][0],
    &buffer.dry_int_aero_mmr[3][1],
    &buffer.dry_int_aero_mmr[3][2],
    &buffer.dry_cld_aero_mmr[3][0],
    &buffer.dry_cld_aero_mmr[3][1],
    &buffer.dry_cld_aero_mmr[3][2],

    // aerosol gases
    &buffer.dry_gas_mmr[0],
    &buffer.dry_gas_mmr[1],
    &buffer.dry_gas_mmr[2],
    &buffer.dry_gas_mmr[3],
    &buffer.dry_gas_mmr[4],
    &buffer.dry_gas_mmr[5]
  };

  for (int i = 0; i < Buffer::num_2d_mid; ++i) {
    *view_2d_mid_ptrs[i] = view_2d(mem, ncol, nlev);
    mem += view_2d_mid_ptrs[i]->size();
  }

  // set view pointers for interface fields
  uview_2d* view_2d_iface_ptrs[Buffer::num_2d_iface] = {
    &buffer.z_iface
  };
  for (int i = 0; i < Buffer::num_2d_iface; ++i) {
    *view_2d_iface_ptrs[i] = view_2d(mem, ncol, nlev+1);
    mem += view_2d_iface_ptrs[i]->size();
  }

  // WSM data
  buffer.wsm_data = mem;

  /* FIXME: this corresponds to the FIXME in the above function
  // Compute workspace manager size to check used memory
  // vs. requested memory
  const auto policy      = ekat::ExeSpaceUtils<KT::ExeSpace>::get_default_team_policy(ncol_, nlev_);
  const int n_wind_slots = ekat::npack<Spack>(2)*Spack::n;
  const int n_trac_slots = ekat::npack<Spack>(m_num_tracers+3)*Spack::n;
  const int wsm_size     = WSM::get_total_bytes_needed(nlevi_packs, 13+(n_wind_slots+n_trac_slots), policy)/sizeof(Spack);
  mem += wsm_size;
  */

  // return the number of bytes allocated
  return (mem - buffer_manager.get_memory())*sizeof(Real);
}

// Given a dry atmosphere state, creates a haero::Atmosphere object for the
// column with the given index. This object can be provided to mam4xx for the
// column.
KOKKOS_INLINE_FUNCTION
haero::Atmosphere atmosphere_for_column(const DryAtmosphere& dry_atm,
                                        const int column_index) {
  EKAT_KERNEL_ASSERT_MSG(dry_atm.T_mid.data() != nullptr,
    "T_mid not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.p_mid.data() != nullptr,
    "p_mid not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.qv.data() != nullptr,
    "qv not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.qc.data() != nullptr,
    "qc not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.nc.data() != nullptr,
    "nc not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.qi.data() != nullptr,
    "qi not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.ni.data() != nullptr,
    "ni not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.z_mid.data() != nullptr,
    "z_mid not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.p_del.data() != nullptr,
    "p_del not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.cldfrac.data() != nullptr,
    "cldfrac not defined for dry atmosphere state!");
  EKAT_KERNEL_ASSERT_MSG(dry_atm.w_updraft.data() != nullptr,
    "w_updraft not defined for dry atmosphere state!");
  return haero::Atmosphere(mam4::nlev,
                           ekat::subview(dry_atm.T_mid, column_index),
                           ekat::subview(dry_atm.p_mid, column_index),
                           ekat::subview(dry_atm.qv, column_index),
                           ekat::subview(dry_atm.qc, column_index),
                           ekat::subview(dry_atm.nc, column_index),
                           ekat::subview(dry_atm.qi, column_index),
                           ekat::subview(dry_atm.ni, column_index),
                           ekat::subview(dry_atm.z_mid, column_index),
                           ekat::subview(dry_atm.p_del, column_index),
                           ekat::subview(dry_atm.cldfrac, column_index),
                           ekat::subview(dry_atm.w_updraft, column_index),
                           dry_atm.pblh(column_index));
}

// Given an AerosolState with views for dry aerosol quantities, creates a
// mam4::Prognostics object for the column with the given index with
// ONLY INTERSTITIAL AEROSOL VIEWS DEFINED. This object can be provided to
// mam4xx for the column.
KOKKOS_INLINE_FUNCTION
mam4::Prognostics interstitial_aerosols_for_column(const AerosolState& dry_aero,
                                                   const int column_index) {
  constexpr int nlev = mam4::nlev;
  mam4::Prognostics progs(nlev);
  for (int m = 0; m < num_aero_modes(); ++m) {
    EKAT_KERNEL_ASSERT_MSG(dry_aero.int_aero_nmr[m].data(),
      "int_aero_nmr not defined for dry aerosol state!");
    progs.n_mode_i[m] = ekat::subview(dry_aero.int_aero_nmr[m], column_index);
    for (int a = 0; a < num_aero_species(); ++a) {
      if (dry_aero.int_aero_mmr[m][a].data()) {
        progs.q_aero_i[m][a] = ekat::subview(dry_aero.int_aero_mmr[m][a], column_index);
      }
    }
  }
  for (int g = 0; g < num_aero_gases(); ++g) {
    EKAT_KERNEL_ASSERT_MSG(dry_aero.gas_mmr[g].data(),
      "gas_mmr not defined for dry aerosol state!");
    progs.q_gas[g] = ekat::subview(dry_aero.gas_mmr[g], column_index);
  }
  return progs;
}

// Given a dry aerosol state, creates a mam4::Prognostics object for the column
// with the given index with interstitial and cloudborne aerosol views defined.
// This object can be provided to mam4xx for the column.
KOKKOS_INLINE_FUNCTION
mam4::Prognostics aerosols_for_column(const AerosolState& dry_aero,
                                      const int column_index) {
  auto progs = interstitial_aerosols_for_column(dry_aero, column_index);
  for (int m = 0; m < num_aero_modes(); ++m) {
    EKAT_KERNEL_ASSERT_MSG(dry_aero.cld_aero_nmr[m].data(),
      "dry_cld_aero_nmr not defined for aerosol state!");
    progs.n_mode_c[m] = ekat::subview(dry_aero.cld_aero_nmr[m], column_index);
    for (int a = 0; a < num_aero_species(); ++a) {
      if (dry_aero.cld_aero_mmr[m][a].data()) {
        progs.q_aero_c[m][a] = ekat::subview(dry_aero.cld_aero_mmr[m][a], column_index);
      }
    }
  }
  return progs;
}

// Given a thread team and a dry atmosphere state, dispatches threads from the
// team to compute vertical layer heights and interfaces for the column with
// the given index.
KOKKOS_INLINE_FUNCTION
void compute_vertical_layer_heights(const Team& team,
                                    const DryAtmosphere& dry_atm,
                                    const int column_index) {
  EKAT_KERNEL_ASSERT_MSG(column_index == team.league_rank(),
    "Given column index does not correspond to given team!");

  const auto dz = ekat::subview(dry_atm.dz, column_index);
  auto z_iface  = ekat::subview(dry_atm.z_iface, column_index);
  auto z_mid    = ekat::subview(dry_atm.z_mid, column_index);
  PF::calculate_z_int(team, mam4::nlev, dz, dry_atm.z_surf, z_iface);
  team.team_barrier(); // likely necessary to have z_iface up to date
  PF::calculate_z_mid(team, mam4::nlev, z_iface, z_mid);
}


// Given a thread team and wet and dry atmospheres, dispatches threads from the
// team to compute the vertical updraft velocity for the column with the given
// index.
KOKKOS_INLINE_FUNCTION
void compute_updraft_velocities(const Team& team,
                                const WetAtmosphere& wet_atm,
                                const DryAtmosphere& dry_atm,
                                const int column_index) {
  EKAT_KERNEL_ASSERT_MSG(column_index == team.league_rank(),
    "Given column index does not correspond to given team!");

  constexpr int nlev = mam4::nlev;
  int i = column_index;
  Kokkos::parallel_for(Kokkos::TeamVectorRange(team, nlev), [&] (const int k) {
    const auto rho = PF::calculate_density(dry_atm.p_del(i,k), dry_atm.dz(i,k));
    dry_atm.w_updraft(i,k) = PF::calculate_vertical_velocity(wet_atm.omega(i,k), rho);
  });
}

// Given a thread team and a wet atmosphere state, dispatches threads
// from the team to compute mixing ratios for a dry atmosphere state in th
// column with the given index.
KOKKOS_INLINE_FUNCTION
void compute_dry_mixing_ratios(const Team& team,
                               const WetAtmosphere& wet_atm,
                               const DryAtmosphere& dry_atm,
                               const int column_index) {
  EKAT_KERNEL_ASSERT_MSG(column_index == team.league_rank(),
    "Given column index does not correspond to given team!");

  constexpr int nlev = mam4::nlev;
  int i = column_index;
  Kokkos::parallel_for(Kokkos::TeamVectorRange(team, nlev), [&] (const int k) {
    const auto qv_ik = wet_atm.qv(i,k);
    dry_atm.qv(i,k) = PF::calculate_drymmr_from_wetmmr(wet_atm.qv(i,k), qv_ik);
    dry_atm.qc(i,k) = PF::calculate_drymmr_from_wetmmr(wet_atm.qc(i,k), qv_ik);
    dry_atm.nc(i,k) = PF::calculate_drymmr_from_wetmmr(wet_atm.nc(i,k), qv_ik);
    dry_atm.qi(i,k) = PF::calculate_drymmr_from_wetmmr(wet_atm.qi(i,k), qv_ik);
    dry_atm.ni(i,k) = PF::calculate_drymmr_from_wetmmr(wet_atm.ni(i,k), qv_ik);
  });
}

// Given a thread team and wet atmospheric and aerosol states, dispatches threads
// from the team to compute mixing ratios for the given dry interstitial aerosol
// state for the column with the given index.
KOKKOS_INLINE_FUNCTION
void compute_dry_mixing_ratios(const Team& team,
                               const WetAtmosphere& wet_atm,
                               const AerosolState& wet_aero,
                               const AerosolState& dry_aero,
                               const int column_index) {
  EKAT_KERNEL_ASSERT_MSG(column_index == team.league_rank(),
    "Given column index does not correspond to given team!");

  constexpr int nlev = mam4::nlev;
  int i = column_index;
  Kokkos::parallel_for(Kokkos::TeamVectorRange(team, nlev), [&] (const int k) {
    const auto qv_ik = wet_atm.qv(i,k);
    for (int m = 0; m < num_aero_modes(); ++m) {
      dry_aero.int_aero_nmr[m](i,k) = PF::calculate_drymmr_from_wetmmr(wet_aero.int_aero_nmr[m](i,k), qv_ik);
      if (dry_aero.cld_aero_nmr[m].data()) {
        dry_aero.cld_aero_nmr[m](i,k) = PF::calculate_drymmr_from_wetmmr(wet_aero.cld_aero_nmr[m](i,k), qv_ik);
      }
      for (int a = 0; a < num_aero_species(); ++a) {
        if (dry_aero.int_aero_mmr[m][a].data()) {
          dry_aero.int_aero_mmr[m][a](i,k) = PF::calculate_drymmr_from_wetmmr(wet_aero.int_aero_mmr[m][a](i,k), qv_ik);
        }
        if (dry_aero.cld_aero_mmr[m][a].data()) {
          dry_aero.cld_aero_mmr[m][a](i,k) = PF::calculate_drymmr_from_wetmmr(wet_aero.cld_aero_mmr[m][a](i,k), qv_ik);
        }
      }
    }
    for (int g = 0; g < num_aero_gases(); ++g) {
      dry_aero.gas_mmr[g](i,k) = PF::calculate_drymmr_from_wetmmr(wet_aero.gas_mmr[g](i,k), qv_ik);
    }
  });
}

// Given a thread team and dry atmospheric and aerosol states, dispatches threads
// from the team to compute mixing ratios for the given wet interstitial aerosol
// state for the column with the given index.
KOKKOS_INLINE_FUNCTION
void compute_wet_mixing_ratios(const Team& team,
                               const DryAtmosphere& dry_atm,
                               const AerosolState& dry_aero,
                               const AerosolState& wet_aero,
                               const int column_index) {
  EKAT_KERNEL_ASSERT_MSG(column_index == team.league_rank(),
    "Given column index does not correspond to given team!");

  constexpr int nlev = mam4::nlev;
  int i = column_index;
  Kokkos::parallel_for(Kokkos::TeamVectorRange(team, nlev), [&] (const int k) {
    const auto qv_ik = dry_atm.qv(i,k);
    for (int m = 0; m < num_aero_modes(); ++m) {
      wet_aero.int_aero_nmr[m](i,k) = PF::calculate_wetmmr_from_drymmr(dry_aero.int_aero_nmr[m](i,k), qv_ik);
      if (wet_aero.cld_aero_nmr[m].data()) {
        wet_aero.cld_aero_nmr[m](i,k) = PF::calculate_wetmmr_from_drymmr(dry_aero.cld_aero_nmr[m](i,k), qv_ik);
      }
      for (int a = 0; a < num_aero_species(); ++a) {
        if (wet_aero.int_aero_mmr[m][a].data()) {
          wet_aero.int_aero_mmr[m][a](i,k) = PF::calculate_wetmmr_from_drymmr(dry_aero.int_aero_mmr[m][a](i,k), qv_ik);
        }
        if (wet_aero.cld_aero_mmr[m][a].data()) {
          wet_aero.cld_aero_mmr[m][a](i,k) = PF::calculate_wetmmr_from_drymmr(dry_aero.cld_aero_mmr[m][a](i,k), qv_ik);
        }
      }
    }
    for (int g = 0; g < num_aero_gases(); ++g) {
      wet_aero.gas_mmr[g](i,k) = PF::calculate_wetmmr_from_drymmr(dry_aero.gas_mmr[g](i,k), qv_ik);
    }
  });
}

// Because CUDA C++ doesn't allow us to declare and use constants outside of
// KOKKOS_INLINE_FUNCTIONS, we define this macro that allows us to (re)define
// these constants where needed within two such functions so we don't define
// them inconsistently. Yes, it's the 21st century and we're still struggling
// with these basic things.
#define DECLARE_PROG_TRANSFER_CONSTANTS \
  /* mapping of constituent indices to aerosol modes */ \
  const auto Accum = mam4::ModeIndex::Accumulation; \
  const auto Aitken = mam4::ModeIndex::Aitken; \
  const auto Coarse = mam4::ModeIndex::Coarse; \
  const auto PC = mam4::ModeIndex::PrimaryCarbon; \
  const auto NoMode = mam4::ModeIndex::None; \
  static const mam4::ModeIndex mode_for_cnst[gas_pcnst()] = { \
    NoMode, NoMode, NoMode, NoMode, NoMode, NoMode, /* gases (not aerosols) */ \
    Accum, Accum, Accum, Accum, Accum, Accum, Accum, Accum,         /* 7 aero species + NMR */ \
    Aitken, Aitken, Aitken, Aitken, Aitken,                         /* 4 aero species + NMR */ \
    Coarse, Coarse, Coarse, Coarse, Coarse, Coarse, Coarse, Coarse, /* 7 aero species + NMR */ \
    PC, PC, PC, PC,                                                 /* 3 aero species + NMR */ \
  }; \
  /* mapping of constituent indices to aerosol species */ \
  const auto SOA = mam4::AeroId::SOA; \
  const auto SO4 = mam4::AeroId::SO4; \
  const auto POM = mam4::AeroId::POM; \
  const auto BC = mam4::AeroId::BC; \
  const auto NaCl = mam4::AeroId::NaCl; \
  const auto DST = mam4::AeroId::DST; \
  const auto MOM = mam4::AeroId::MOM; \
  const auto NoAero = mam4::AeroId::None; \
  static const mam4::AeroId aero_for_cnst[gas_pcnst()] = { \
    NoAero, NoAero, NoAero, NoAero, NoAero, NoAero, /* gases (not aerosols) */ \
    SO4, POM, SOA, BC, DST, NaCl, MOM, NoAero,      /* accumulation mode */ \
    SO4, SOA, NaCl, MOM, NoAero,                    /* aitken mode */ \
    DST, NaCl, SO4, BC, POM, SOA, MOM, NoAero,      /* coarse mode */ \
    POM, BC, MOM, NoAero,                           /* primary carbon mode */ \
  }; \
  /* mapping of constituent indices to gases */ \
  const auto O3 = mam4::GasId::O3; \
  const auto H2O2 = mam4::GasId::H2O2; \
  const auto H2SO4 = mam4::GasId::H2SO4; \
  const auto SO2 = mam4::GasId::SO2; \
  const auto DMS = mam4::GasId::DMS; \
  const auto SOAG = mam4::GasId::SOAG; \
  const auto NoGas = mam4::GasId::None; \
  static const mam4::GasId gas_for_cnst[gas_pcnst()] = { \
    O3, H2O2, H2SO4, SO2, DMS, SOAG, \
    NoGas, NoGas, NoGas, NoGas, NoGas, NoGas, NoGas, NoGas, \
    NoGas, NoGas, NoGas, NoGas, NoGas, \
    NoGas, NoGas, NoGas, NoGas, NoGas, NoGas, NoGas, NoGas, \
    NoGas, NoGas, NoGas, NoGas, \
  };

// Given a Prognostics object, transfers data for interstitial aerosols to the
// chemistry work array q, and cloudborne aerosols to the chemistry work array
// qqcw, both at vertical level k. The input quantities in progs are stored as
// number/mass mixing ratios, whereas the output quantities in q and qqcw are
// stored as volume mixing ratios (VMRs), which are sometimes referred to as
// tracer mixing ratios (TMRs).
// NOTE: this mapping is chemistry-mechanism-specific (see mo_sim_dat.F90
// NOTE: in the relevant preprocessed chemical mechanism)
// NOTE: see mam4xx/aero_modes.hpp to interpret these mode/aerosol/gas
// NOTE: indices
KOKKOS_INLINE_FUNCTION
void transfer_prognostics_to_work_arrays(const mam4::Prognostics &progs,
                                         const int k,
                                         Real q[gas_pcnst()],
                                         Real qqcw[gas_pcnst()]) {
  DECLARE_PROG_TRANSFER_CONSTANTS

  // copy number/mass mixing ratios from progs to q and qqcw at level k,
  // converting them to VMR
  for (int i = 0; i < gas_pcnst(); ++i) {
    auto mode_index = mode_for_cnst[i];
    auto aero_id = aero_for_cnst[i];
    auto gas_id = gas_for_cnst[i];
    if (gas_id != NoGas) { // constituent is a gas
      int g = static_cast<int>(gas_id);
      const Real mw = mam4::gas_species(g).molecular_weight;
      q[i] = mam4::conversions::vmr_from_mmr(progs.q_gas[g](k), mw);
      qqcw[i] = mam4::conversions::vmr_from_mmr(progs.q_gas[g](k), mw);
    } else {
      int m = static_cast<int>(mode_index);
      if (aero_id != NoAero) { // constituent is an aerosol species
        int a = aerosol_index_for_mode(mode_index, aero_id);
        const Real mw = mam4::aero_species(a).molecular_weight;
        q[i] = mam4::conversions::vmr_from_mmr(progs.q_aero_i[m][a](k), mw);
        qqcw[i] = mam4::conversions::vmr_from_mmr(progs.q_aero_c[m][a](k), mw);
      } else { // constituent is a modal number mixing ratio
        int m = static_cast<int>(mode_index);
        // FIXME: do we just use number mixing ratios in q/qqcw?
        q[i] = progs.n_mode_i[m](k);
        qqcw[i] = progs.n_mode_c[m](k);
      }
    }
  }
}

// Given work arrays with interstitial and cloudborne aerosol data, transfers
// them to the given Prognostics object at the kth vertical level. This is the
// "inverse operator" for transfer_prognostics_to_work_arrays, above.
KOKKOS_INLINE_FUNCTION
void transfer_work_arrays_to_prognostics(const Real q[gas_pcnst()],
                                         const Real qqcw[gas_pcnst()],
                                         mam4::Prognostics &progs, const int k) {
  DECLARE_PROG_TRANSFER_CONSTANTS

  // copy number/mass mixing ratios from progs to q and qqcw at level k,
  // converting them to VMR
  for (int i = 0; i < gas_pcnst(); ++i) {
    auto mode_index = mode_for_cnst[i];
    auto aero_id = aero_for_cnst[i];
    auto gas_id = gas_for_cnst[i];
    if (gas_id != NoGas) { // constituent is a gas
      int g = static_cast<int>(gas_id);
      const Real mw = mam4::gas_species(g).molecular_weight;
      progs.q_gas[g](k) = mam4::conversions::mmr_from_vmr(q[i], mw);
      progs.q_gas[g](k) = mam4::conversions::mmr_from_vmr(qqcw[i], mw);
    } else {
      int m = static_cast<int>(mode_index);
      if (aero_id != NoAero) { // constituent is an aerosol species
        int a = aerosol_index_for_mode(mode_index, aero_id);
        const Real mw = mam4::aero_species(a).molecular_weight;
        progs.q_aero_i[m][a](k) = mam4::conversions::mmr_from_vmr(q[i], mw);
        progs.q_aero_c[m][a](k) = mam4::conversions::mmr_from_vmr(qqcw[i], mw);
      } else { // constituent is a modal number mixing ratio
        int m = static_cast<int>(mode_index);
        // FIXME: do we just use number mixing ratios in q/qqcw?
        progs.n_mode_i[m](k) = q[i];
        progs.n_mode_c[m](k) = qqcw[i];
      }
    }
  }
}

} // namespace scream::mam_coupling

#endif
