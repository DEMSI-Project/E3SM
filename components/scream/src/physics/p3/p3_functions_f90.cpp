#include "p3_functions_f90.hpp"

#include "share/scream_assert.hpp"
#include "share/util/scream_utils.hpp"
#include "share/util/scream_kokkos_utils.hpp"
#include "p3_f90.hpp"

#include <random>

using scream::Real;
using scream::Int;

//
// A C++ interface to micro_p3 fortran calls and vice versa
//

extern "C" {

void p3_init_a_c(Real* itab, Real* itabcol);

void find_lookuptable_indices_1a_c(Int* dumi, Int* dumjj, Int* dumii, Int* dumzz,
                                   Real* dum1, Real* dum4, Real* dum5, Real* dum6,
                                   Real qitot, Real nitot, Real qirim, Real rhop);

void find_lookuptable_indices_1b_c(Int* dumj, Real* dum3, Real qr, Real nr);

void access_lookup_table_c(Int dumjj, Int dumii, Int dumi, Int index,
                           Real dum1, Real dum4, Real dum5, Real* proc);

void access_lookup_table_coll_c(Int dumjj, Int dumii, Int dumj, Int dumi, Int index,
                                Real dum1, Real dum3, Real dum4, Real dum5, Real* proc);

void get_cloud_dsd2_c(Real qc, Real* nc, Real* mu_c, Real rho, Real* nu, Real* lamc,
                      Real* cdist, Real* cdist1, Real lcldm);

void get_rain_dsd2_c(Real qr, Real* nr, Real* mu_r, Real* lamr, Real* cdistr, Real* logn0r, Real rcldm);

void calc_first_order_upwind_step_c(Int kts, Int kte, Int kdir, Int kbot, Int k_qxtop, Real dt_sub, Real* rho, Real* inv_rho, Real* inv_dzq, Int num_arrays, Real** fluxes, Real** vs, Real** qnx);

}

namespace scream {
namespace p3 {

//
// In all C++ -> Fortran bridge functions you should see p3_init(true). P3 needs
// to be initialized since most of its function depend on global tables to be
// populated. The 'true' argument is to set p3 to use its fortran implementations
// instead of calling back to C++. We want this behavior since it doesn't make much
// sense for C++ to bridge over to fortran only to have fortran bridge back to C++.
// If the client wanted the C++ implementation, they should just call it directly.
//

void p3_init_a(P3InitAFortranData& d)
{
  p3_init(true); // need to initialize p3 first so that tables are loaded
  p3_init_a_c(d.itab.data(), d.itabcol.data());
}

void find_lookuptable_indices_1a(LookupIceData& d)
{
  p3_init(true); // need to initialize p3 first so that tables are loaded
  find_lookuptable_indices_1a_c(&d.dumi, &d.dumjj, &d.dumii, &d.dumzz,
                                &d.dum1, &d.dum4, &d.dum5, &d.dum6,
                                d.qitot, d.nitot, d.qirim, d.rhop);
}

void find_lookuptable_indices_1b(LookupIceDataB& d)
{
  p3_init(true);
  find_lookuptable_indices_1b_c(&d.dumj, &d.dum3, d.qr, d.nr);
}

void access_lookup_table(AccessLookupTableData& d)
{
  p3_init(true); // need to initialize p3 first so that tables are loaded
  access_lookup_table_c(d.lid.dumjj, d.lid.dumii, d.lid.dumi, d.index,
                        d.lid.dum1, d.lid.dum4, d.lid.dum5, &d.proc);
}

void access_lookup_table_coll(AccessLookupTableCollData& d)
{
  p3_init(true);
  access_lookup_table_coll_c(d.lid.dumjj, d.lid.dumii, d.lidb.dumj, d.lid.dumi, d.index,
                             d.lid.dum1, d.lidb.dum3, d.lid.dum4, d.lid.dum5, &d.proc);
}

void get_cloud_dsd2(GetCloudDsd2Data& d)
{
  p3_init(true);
  Real nc_in = d.nc_in;
  get_cloud_dsd2_c(d.qc, &nc_in, &d.mu_c, d.rho, &d.nu, &d.lamc, &d.cdist, &d.cdist1, d.lcldm);
  d.nc_out = nc_in;
}

void get_rain_dsd2(GetRainDsd2Data& d)
{
  p3_init(true);
  Real nr_in = d.nr_in;
  get_rain_dsd2_c(d.qr, &nr_in, &d.mu_r, &d.lamr, &d.cdistr, &d.logn0r, d.rcldm);
  d.nr_out = nr_in;
}

CalcUpwindData::CalcUpwindData(
  Int kts_, Int kte_, Int kdir_, Int kbot_, Int k_qxtop_, Int num_arrays_, Real dt_sub_,
  std::pair<Real, Real> rho_range, std::pair<Real, Real> inv_dzq_range,
  std::pair<Real, Real> vs_range, std::pair<Real, Real> qnx_range) :
  kts(kts_), kte(kte_), kdir(kdir_), kbot(kbot_), k_qxtop(k_qxtop_), num_arrays(num_arrays_), dt_sub(dt_sub_),
  m_nk((kte - kts) + 1),
  m_data( (3 + num_arrays*4) * m_nk)
{
  Int offset = 0;

  rho     = m_data.data();
  inv_rho = rho + (offset+=m_nk);
  inv_dzq = inv_rho + (offset+=m_nk);

  for (Int i = 0; i < num_arrays; ++i) {
    fluxes[i]  = m_data.data() + (offset+=m_nk);
    vs[i]      = m_data.data() + (offset+=m_nk);
    qnx_in[i]  = m_data.data() + (offset+=m_nk);
    qnx_out[i] = m_data.data() + (offset+=m_nk);
  }

  std::default_random_engine generator;
  std::uniform_real_distribution<Real>
    rho_dist(rho_range.first, rho_range.second),
    inv_dzq_dist(inv_dzq_range.first, inv_dzq_range.second),
    vs_dist(vs_range.first, vs_range.second),
    qnx_dist(qnx_range.first, qnx_range.second);

  for (Int k = 0; k < m_nk; ++k) {
    rho[k]     = rho_dist(generator);
    inv_rho[k] = 1 / rho[k];
    inv_dzq[k] = inv_dzq_dist(generator);

    for (Int i = 0; i < num_arrays; ++i) {
      fluxes [i][k] = 0.0;
      qnx_out[i][k] = 0.0;
      vs     [i][k] = vs_dist(generator);
      qnx_in [i][k] = qnx_dist(generator);
    }
  }
}

void calc_first_order_upwind_step(CalcUpwindData& d)
{
  p3_init(true);
  std::vector<std::vector<Real> > qnx_in(d.num_arrays, std::vector<Real>(d.nk()));
  for (Int i = 0; i < d.num_arrays; ++i) {
    std::copy(d.qnx_in[i], d.qnx_in[i] + d.nk(), qnx_in[i].begin());
  }

  calc_first_order_upwind_step_c(d.kts, d.kte, d.kdir, d.kbot, d.k_qxtop, d.dt_sub, d.rho, d.inv_rho, d.inv_dzq, d.num_arrays, d.fluxes, d.vs, d.qnx_in);

  for (Int i = 0; i < d.num_arrays; ++i) {
    std::copy(qnx_in[i].begin(), qnx_in[i].end(), d.qnx_out[i]);
  }
}

std::shared_ptr<P3GlobalForFortran::Views> P3GlobalForFortran::s_views;

const P3GlobalForFortran::Views& P3GlobalForFortran::get()
{
  if (!P3GlobalForFortran::s_views) {
    P3GlobalForFortran::s_views = std::make_shared<Views>();
    P3F::init_kokkos_ice_lookup_tables(s_views->m_itab, s_views->m_itabcol);
    P3F::init_kokkos_tables(s_views->m_vn_table, s_views->m_vm_table, s_views->m_mu_r_table, s_views->m_dnu);
  }
  return *P3GlobalForFortran::s_views;
}

void P3GlobalForFortran::deinit()
{
  P3GlobalForFortran::s_views = nullptr;
}

void find_lookuptable_indices_1a_f(Int* dumi, Int* dumjj, Int* dumii, Int* dumzz,
                                   Real* dum1, Real* dum4, Real* dum5, Real* dum6,
                                   Real qitot_, Real nitot_, Real qirim_, Real rhop_)
{
  using P3F = Functions<Real, DefaultDevice>;
  using TableIce = typename P3F::TableIce;

  typename P3F::Smask qiti_gt_small(qitot_ > P3F::C::QSMALL);
  typename P3F::Spack qitot(qitot_), nitot(nitot_), qirim(qirim_), rhop(rhop_);
  typename P3F::view_1d<TableIce> t_d("t_h", 1);
  auto t_h = Kokkos::create_mirror_view(t_d);
  Kokkos::parallel_for(1, KOKKOS_LAMBDA(const Int&) {
    P3F::lookup_ice(qiti_gt_small, qitot, nitot, qirim, rhop, t_d(0));
  });
  Kokkos::deep_copy(t_h, t_d);
  auto& t = t_h(0);

  // adjust for 1-based indexing
  *dumi  = t.dumi[0]  + 1;
  *dumjj = t.dumjj[0] + 1;
  *dumii = t.dumii[0] + 1;
  *dumzz = t.dumzz[0] + 1;

  *dum1 = t.dum1[0];
  *dum4 = t.dum4[0];
  *dum5 = t.dum5[0];
  *dum6 = t.dum6[0];
}

void find_lookuptable_indices_1b_f(Int* dumj, Real* dum3, Real qr_, Real nr_)
{
  using P3F = Functions<Real, DefaultDevice>;
  using TableRain = typename P3F::TableRain;

  // we can assume fortran would not be calling this routine if qiti_gt_small was not true
  typename P3F::Smask qiti_gt_small(true);

  typename P3F::Spack qr(qr_), nr(nr_);
  typename P3F::view_1d<TableRain> t_d("t_h", 1);
  auto t_h = Kokkos::create_mirror_view(t_d);
  Kokkos::parallel_for(1, KOKKOS_LAMBDA(const Int&) {
    P3F::lookup_rain(qiti_gt_small, qr, nr, t_d(0));
  });
  Kokkos::deep_copy(t_h, t_d);
  auto& t = t_h(0);

  // adjust for 1-based indexing
  *dumj = t.dumj[0] + 1;

  *dum3 = t.dum3[0];
}

void access_lookup_table_f(Int dumjj, Int dumii, Int dumi, Int index,
                           Real dum1, Real dum4, Real dum5, Real* proc)
{
  using P3F = Functions<Real, DefaultDevice>;

  // we can assume fortran would not be calling this routine if qiti_gt_small was not true
  typename P3F::Smask qiti_gt_small(true);
  typename P3F::TableIce t;

  // Adjust for 0-based indexing
  t.dumi  = dumi  - 1;
  t.dumjj = dumjj - 1;
  t.dumii = dumii - 1;

  int adjusted_index = index - 1;

  t.dum1 = dum1;
  t.dum4 = dum4;
  t.dum5 = dum5;

  auto itab = P3GlobalForFortran::itab();
  Real result;
  Kokkos::parallel_reduce(1, KOKKOS_LAMBDA(const Int&, Real& value) {
    value = P3F::apply_table_ice(qiti_gt_small, adjusted_index, itab, t)[0];
  }, result);
  *proc = result;
}

void access_lookup_table_coll_f(Int dumjj, Int dumii, Int dumj, Int dumi, Int index,
                                Real dum1, Real dum3, Real dum4, Real dum5, Real* proc)
{
  using P3F = Functions<Real, DefaultDevice>;

  // we can assume fortran would not be calling this routine if qiti_gt_small was not true
  typename P3F::Smask qiti_gt_small(true);

  typename P3F::TableIce ti;
  typename P3F::TableRain tr;

  // Adjust for 0-based indexing
  ti.dumi  = dumi  - 1;
  ti.dumjj = dumjj - 1;
  ti.dumii = dumii - 1;
  tr.dumj  = dumj  - 1;

  int adjusted_index = index - 1;

  ti.dum1 = dum1;
  ti.dum4 = dum4;
  ti.dum5 = dum5;
  tr.dum3 = dum3;

  auto itabcol = P3GlobalForFortran::itabcol();
  Real result;
  Kokkos::parallel_reduce(1, KOKKOS_LAMBDA(const Int&, Real& value) {
    value = P3F::apply_table_coll(qiti_gt_small, adjusted_index, itabcol, ti, tr)[0];
  }, result);
  *proc = result;
}

void get_cloud_dsd2_f(Real qc_, Real* nc_, Real* mu_c_, Real rho_, Real* nu_, Real* lamc_,
                      Real* cdist_, Real* cdist1_, Real lcldm_)
{
  using P3F = Functions<Real, DefaultDevice>;

  typename P3F::Smask qc_gt_small(qc_ > P3F::C::QSMALL);
  typename P3F::view_1d<Real> t_d("t_h", 6);
  auto t_h = Kokkos::create_mirror_view(t_d);

  Real local_nc = *nc_;
  auto dnu = P3GlobalForFortran::dnu();
  Kokkos::parallel_for(1, KOKKOS_LAMBDA(const Int&) {
    typename P3F::Spack qc(qc_), nc(local_nc), rho(rho_), lcldm(lcldm_);
    typename P3F::Spack mu_c, nu, lamc, cdist, cdist1;

    P3F::get_cloud_dsd2(qc_gt_small, qc, nc, mu_c, rho, nu, dnu, lamc, cdist, cdist1, lcldm);

    t_d(0) = nc[0];
    t_d(1) = mu_c[0];
    t_d(2) = nu[0];
    t_d(3) = lamc[0];
    t_d(4) = cdist[0];
    t_d(5) = cdist1[0];
  });
  Kokkos::deep_copy(t_h, t_d);

  *nc_     = t_h(0);
  *mu_c_   = t_h(1);
  *nu_     = t_h(2);
  *lamc_   = t_h(3);
  *cdist_  = t_h(4);
  *cdist1_ = t_h(5);
}

void get_rain_dsd2_f(Real qr_, Real* nr_, Real* mu_r_, Real* lamr_, Real* cdistr_, Real* logn0r_, Real rcldm_)
{
  using P3F = Functions<Real, DefaultDevice>;

  typename P3F::Smask qr_gt_small(qr_ > P3F::C::QSMALL);
  typename P3F::view_1d<Real> t_d("t_d", 5);
  auto t_h = Kokkos::create_mirror_view(t_d);
  Real local_nr = *nr_;

  Kokkos::parallel_for(1, KOKKOS_LAMBDA(const Int&) {
    typename P3F::Spack qr(qr_), rcldm(rcldm_), nr(local_nr);
    typename P3F::Spack lamr, mu_r, cdistr, logn0r;

    P3F::get_rain_dsd2(qr_gt_small, qr, nr, mu_r, lamr, cdistr, logn0r, rcldm);

    t_d(0) = nr[0];
    t_d(1) = mu_r[0];
    t_d(2) = lamr[0];
    t_d(3) = cdistr[0];
    t_d(4) = logn0r[0];
  });
  Kokkos::deep_copy(t_h, t_d);

  *nr_     = t_h(0);
  *mu_r_   = t_h(1);
  *lamr_   = t_h(2);
  *cdistr_ = t_h(3);
  *logn0r_ = t_h(4);
}

template <int N>
void calc_first_order_upwind_step_f_impl(
  Int kts, Int kte, Int kdir, Int kbot, Int k_qxtop, Real dt_sub,
  Real* rho, Real* inv_rho, Real* inv_dzq,
  Real** fluxes, Real** vs, Real** qnx)
{
  using P3F  = Functions<Real, DefaultDevice>;
  using P3FH = Functions<Real, HostDevice>;

  using Spack = typename P3F::Spack;
  using view_1d = typename P3F::view_1d<Spack>;
  using view_1dh = typename P3FH::view_1d<Spack>;
  using KT = typename P3F::KT;
  using ExeSpace = typename KT::ExeSpace;
  using MemberType = typename P3F::MemberType;
  using view_1d_ptr_array = typename P3F::view_1d_ptr_array<Spack, N>;
  using uview_1d = typename P3F::uview_1d<Spack>;

  // Setup views
  const Int nk = (kte - kts) + 1;
  const Int npack = (nk + Spack::n - 1) / Spack::n;

  view_1d rho_d("rho_d", npack), inv_rho_d("inv_rho_d", npack), inv_dzq_d("inv_dzq_d", npack);
  auto rho_h     = Kokkos::create_mirror_view(rho_d);
  auto inv_rho_h = Kokkos::create_mirror_view(inv_rho_d);
  auto inv_dzq_h = Kokkos::create_mirror_view(inv_dzq_d);
  Kokkos::Array<view_1d, N> fluxes_d, vs_d, qnx_d;
  Kokkos::Array<view_1dh, N> fluxes_h, vs_h, qnx_h;

  // Populate views. Considered using Unmanaged views to wrap input ptrs but
  // I was worried about running off the ends of arrays if npack does not evenly
  // divide nk;
  for (Int k = 0; k < npack; ++k) {
    const Int scalar_offset = k*Spack::n;
    for (Int s = 0; s < Spack::n && scalar_offset+s < nk; ++s) {
      rho_h(k)[s]     = rho    [scalar_offset + s];
      inv_rho_h(k)[s] = inv_rho[scalar_offset + s];
      inv_dzq_h(k)[s] = inv_dzq[scalar_offset + s];
    }
  }

  for (Int i = 0; i < N; ++i) {
    fluxes_d[i] = view_1d("fluxes_d", npack);
    vs_d[i]     = view_1d("vs_d",     npack);
    qnx_d[i]    = view_1d("qnx_d",    npack);

    fluxes_h[i] = Kokkos::create_mirror_view(fluxes_d[i]);
    vs_h[i]     = Kokkos::create_mirror_view(vs_d[i]);
    qnx_h[i]    = Kokkos::create_mirror_view(qnx_d[i]);

    for (Int k = 0; k < npack; ++k) {
      const Int scalar_offset = k*Spack::n;
      for (Int s = 0; s < Spack::n && scalar_offset+s < nk; ++s) {
        fluxes_h[i](k)[s] = fluxes[i][scalar_offset + s];
        vs_h    [i](k)[s] = vs    [i][scalar_offset + s];
        qnx_h   [i](k)[s] = qnx   [i][scalar_offset + s];
      }
    }
  }

  // Sync views
  Kokkos::deep_copy(rho_d, rho_h);
  Kokkos::deep_copy(inv_rho_d, inv_rho_h);
  Kokkos::deep_copy(inv_dzq_d, inv_dzq_h);
  for (Int i = 0; i < N; ++i) {
    Kokkos::deep_copy(fluxes_d[i], fluxes_h[i]);
    Kokkos::deep_copy(vs_d[i],     vs_h[i]);
    Kokkos::deep_copy(qnx_d[i],    qnx_h[i]);
  }

  auto policy = util::ExeSpaceUtils<ExeSpace>::get_default_team_policy(1, nk);
  Kokkos::parallel_for(policy, KOKKOS_LAMBDA(const MemberType& team) {
    view_1d_ptr_array fluxes_ptr, vs_ptr, qnx_ptr;
    for (int i = 0; i < N; ++i) {
      fluxes_ptr[i] = (uview_1d*)(&fluxes_d[i]);
      vs_ptr[i]     = (uview_1d*)(&vs_d[i]);
      qnx_ptr[i]    = (uview_1d*)(&qnx_d[i]);
    }
    uview_1d urho_d(rho_d), uinv_rho_d(inv_rho_d), uinv_dzq_d(inv_dzq_d);
    P3F::calc_first_order_upwind_step<N>(urho_d, uinv_rho_d, uinv_dzq_d, team, nk, kbot, k_qxtop, kdir, dt_sub, fluxes_ptr, vs_ptr, qnx_ptr);
  });

  // Sync back to host
  for (Int i = 0; i < N; ++i) {
    Kokkos::deep_copy(fluxes_h[i], fluxes_d[i]);
    Kokkos::deep_copy(vs_h[i],     vs_d[i]);
    Kokkos::deep_copy(qnx_h[i],    qnx_d[i]);

    for (Int k = 0; k < npack; ++k) {
      const Int scalar_offset = k*Spack::n;
      for (Int s = 0; s < Spack::n && scalar_offset+s < nk; ++s) {
        fluxes[i][scalar_offset + s] = fluxes_h[i](k)[s];
        vs    [i][scalar_offset + s] = vs_h    [i](k)[s];
        qnx   [i][scalar_offset + s] = qnx_h   [i](k)[s];
      }
    }
  }
}

void calc_first_order_upwind_step_f(
  Int kts, Int kte, Int kdir, Int kbot, Int k_qxtop, Real dt_sub,
  Real* rho, Real* inv_rho, Real* inv_dzq,
  Int num_arrays, Real** fluxes, Real** vs, Real** qnx)
{
  if (num_arrays == 1) {
    calc_first_order_upwind_step_f_impl<1>(kts, kte, kdir, kbot, k_qxtop, dt_sub, rho, inv_rho, inv_dzq, fluxes, vs, qnx);
  }
  else if (num_arrays == 2) {
    calc_first_order_upwind_step_f_impl<2>(kts, kte, kdir, kbot, k_qxtop, dt_sub, rho, inv_rho, inv_dzq, fluxes, vs, qnx);
  }
  else if (num_arrays == 4) {
    calc_first_order_upwind_step_f_impl<4>(kts, kte, kdir, kbot, k_qxtop, dt_sub, rho, inv_rho, inv_dzq, fluxes, vs, qnx);
  }
  else {
    scream_require_msg(false, "Unsupported num arrays in bridge calc_first_order_upwind_step_f: " << num_arrays);
  }
}

// Cuda implementations of std math routines are not necessarily BFB
// with the host.
template <typename ScalarT, typename DeviceT>
struct CudaWrap
{
  using Scalar = ScalarT;

  static Scalar cxx_pow(Scalar base, Scalar exp)
  {
    Scalar result;
    Kokkos::parallel_reduce(1, KOKKOS_LAMBDA(const Int&, Scalar& value) {
        value = std::pow(base, exp);
    }, result);

    return result;
  }

#define cuda_wrap_single_arg(wrap_name, func_call)      \
static Scalar wrap_name(Scalar input) {                 \
  Scalar result;                                        \
  Kokkos::parallel_reduce(1, KOKKOS_LAMBDA(const Int&, Scalar& value) { \
    value = func_call(input);                                         \
  }, result);                                                         \
  return result;                                                      \
}

  cuda_wrap_single_arg(cxx_gamma, std::tgamma)
  cuda_wrap_single_arg(cxx_cbrt, std::cbrt)
  cuda_wrap_single_arg(cxx_log, std::log)
  cuda_wrap_single_arg(cxx_log10, std::log10)
  cuda_wrap_single_arg(cxx_exp, std::exp)

#undef cuda_wrap_single_arg
};

Real cxx_pow(Real base, Real exp)
{
#ifdef KOKKOS_ENABLE_CUDA
  return CudaWrap<Real, DefaultDevice>::cxx_pow(base, exp);
#else
  return std::pow(base, exp);
#endif
}

Real cxx_gamma(Real input)
{
#ifdef KOKKOS_ENABLE_CUDA
  return CudaWrap<Real, DefaultDevice>::cxx_gamma(input);
#else
  return std::tgamma(input);
#endif
}

Real cxx_cbrt(Real input)
{
#ifdef KOKKOS_ENABLE_CUDA
  return CudaWrap<Real, DefaultDevice>::cxx_cbrt(input);
#else
  return std::cbrt(input);
#endif
}

Real cxx_log(Real input)
{
#ifdef KOKKOS_ENABLE_CUDA
  return CudaWrap<Real, DefaultDevice>::cxx_log(input);
#else
  return std::log(input);
#endif
}

Real cxx_log10(Real input)
{
#ifdef KOKKOS_ENABLE_CUDA
  return CudaWrap<Real, DefaultDevice>::cxx_log10(input);
#else
  return std::log10(input);
#endif
}

Real cxx_exp(Real input)
{
#ifdef KOKKOS_ENABLE_CUDA
  return CudaWrap<Real, DefaultDevice>::cxx_exp(input);
#else
  return std::exp(input);
#endif
}

} // namespace p3
} // namespace scream
