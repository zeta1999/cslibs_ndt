#ifndef CSLIBS_NDT_MATCHING_CERES_GRIDMAP_COST_FUNCTOR_HPP
#define CSLIBS_NDT_MATCHING_CERES_GRIDMAP_COST_FUNCTOR_HPP

#include <cslibs_ndt/map/map.hpp>
#include <cslibs_ndt/matching/ceres/map/scan_match_cost_functor.hpp>

#include <ceres/cubic_interpolation.h>
#include <type_traits>

namespace cslibs_ndt {
namespace matching {
namespace ceres {

template <cslibs_ndt::map::tags::option option_t,
          std::size_t Dim,
          typename _T,
          template <typename, typename, typename...> class backend_t,
          template <typename, typename, typename...> class dynamic_backend_t>
class ScanMatchCostFunctor<
        cslibs_ndt::map::Map<option_t,Dim,cslibs_ndt::Distribution,_T,backend_t,dynamic_backend_t>,
        Flag::DIRECT>
{
    using ndt_t = cslibs_ndt::map::Map<option_t,Dim,cslibs_ndt::Distribution,_T,backend_t,dynamic_backend_t>;

    using point_t = typename ndt_t::point_t;
    using bundle_t = typename ndt_t::distribution_bundle_t;

protected:
    explicit inline ScanMatchCostFunctor(const ndt_t& map) :
        map_(map)
    {
    }

    template <int _D>
    inline void Evaluate(const Eigen::Matrix<double,_D,1>& q, double* const value) const
    {
        Eigen::Matrix<_T,Dim,1> p = Eigen::Matrix<_T,Dim,1>::Zero();
        for (std::size_t i=0; i<std::min(_D,static_cast<int>(Dim)); ++i)
            p(i) = q(i);

        *value = 1.0 - map_.sampleNonNormalized(point_t(p));
    }

    template <typename JetT, int _D>
    inline void Evaluate(const Eigen::Matrix<JetT,_D,1>& q, JetT* const value) const
    {
        point_t pt;
        Eigen::Matrix<JetT,Dim,1> p = Eigen::Matrix<JetT,Dim,1>::Zero();
        for (std::size_t i=0; i<std::min(_D,static_cast<int>(Dim)); ++i) {
            p(i)  = q(i);
            pt(i) = q(i).a;
        }

        const bundle_t* bundle = map_.get(pt);

        JetT retval(1);
        if (bundle) {
            for (std::size_t i=0; i<ndt_t::bin_count; ++i) {
                if (const auto& bi = bundle->at(i)) {
                    if (const auto& di = bi->getDistribution()) {
                        if (!di->valid())
                            continue;
                        auto sample = [&p,&di]() {
                            const auto &mean_tmp = di->getMean();
                            const auto &inf_tmp  = di->getInformationMatrix();

                            Eigen::Matrix<JetT, Dim, 1> mean;
                            Eigen::Matrix<JetT, Dim, Dim> inf;
                            for (std::size_t x=0; x<Dim; ++x) {
                                mean(x) = JetT(mean_tmp(x));
                                for (std::size_t y=0; y<Dim; ++y)
                                    inf(x,y) = JetT(inf_tmp(x,y));
                            }
                            const Eigen::Matrix<JetT, Dim, 1> diff = p - mean;
                            const JetT exponent = -JetT(0.5) * diff.transpose() * inf * diff;
                            return ::ceres::exp(exponent);
                        };
                        retval -= JetT(ndt_t::div_count) * sample();
                    }
                }
            }
        }
        *value = retval;
    }

private:
    const ndt_t& map_;
};

// only possible for maps of dimension 2
template <cslibs_ndt::map::tags::option option_t,
          typename _T,
          template <typename, typename, typename...> class backend_t,
          template <typename, typename, typename...> class dynamic_backend_t>
class ScanMatchCostFunctor<
        cslibs_ndt::map::Map<option_t,2,cslibs_ndt::Distribution,_T,backend_t,dynamic_backend_t>,
        Flag::INTERPOLATION>
{
    using ndt_t = cslibs_ndt::map::Map<option_t,2,cslibs_ndt::Distribution,_T,backend_t,dynamic_backend_t>;
    using point_t = typename ndt_t::point_t;

    template <typename>
    friend class ::ceres::BiCubicInterpolator;

    static constexpr int DATA_DIMENSION = 1;

protected:
    explicit inline ScanMatchCostFunctor(const ndt_t& map,
                                         const double& sampling_resolution) :
        map_(map),
        sampling_resolution_(sampling_resolution),
        interpolator_(*this)
    {
    }

    template <typename T, int _D>
    inline void Evaluate(const Eigen::Matrix<T,_D,1>& q, T* const value) const
    {
        interpolator_.Evaluate(q(0) / sampling_resolution_,
                               q(1) / sampling_resolution_,
                               value);
    }

private:
    inline void GetValue(const int row, const int column, double* const value) const
    {
        *value = 1.0 - map_.sampleNonNormalized(
                    point_t(row * sampling_resolution_,
                            column * sampling_resolution_));
    }

    const ndt_t& map_;
    const double sampling_resolution_;
    const ::ceres::BiCubicInterpolator<ScanMatchCostFunctor<ndt_t,Flag::INTERPOLATION>> interpolator_;
};

}
}
}

#endif // CSLIBS_NDT_MATCHING_CERES_GRIDMAP_COST_FUNCTOR_HPP
