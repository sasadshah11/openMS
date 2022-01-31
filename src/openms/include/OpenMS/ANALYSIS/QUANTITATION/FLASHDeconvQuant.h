// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2020.
//
// This software is released under a three-clause BSD license:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of any author or any participating institution
//    may be used to endorse or promote products derived from this software
//    without specific prior written permission.
// For a full list of authors, refer to the file AUTHORS.
// --------------------------------------------------------------------------
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL ANY OF THE AUTHORS OR THE CONTRIBUTING
// INSTITUTIONS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
// OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
// WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
// OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
// ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// --------------------------------------------------------------------------
// $Maintainer: Jihyung Kim $
// $Authors: Jihyung Kim $
// --------------------------------------------------------------------------

#pragma once

#include <OpenMS/CONCEPT/ProgressLogger.h>
#include <OpenMS/KERNEL/MassTrace.h>
#include <OpenMS/CHEMISTRY/ISOTOPEDISTRIBUTION/IsotopeDistribution.h>
#include <OpenMS/CHEMISTRY/ISOTOPEDISTRIBUTION/CoarseIsotopePatternGenerator.h>
#include <algorithm>
#include <OpenMS/DATASTRUCTURES/DefaultParamHandler.h>
#include <OpenMS/DATASTRUCTURES/Matrix.h>
#include "boost/dynamic_bitset.hpp"
#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/FeatureFinderAlgorithmPickedHelperStructs.h>
#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/TraceFitter.h>
#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/EGHTraceFitter.h>
#include <fstream>

using namespace std;
namespace OpenMS
{
  /**
   * @brief Internal structure to store MassTrace and its additional information
   */
  class OPENMS_DLLAPI LogMassTrace
  {
  public:
    /// default constructor
    LogMassTrace() :
        mass_trace_(),
        log_centroid_mz_(),
        centroid_mz_(),
        charge_(),
        fwhm_start_(),
        fwhm_end_(),
        intensity_(),
        isotope_index_(),
        mass_(),
        trace_index_()
    {
    }

    /// default destructor
    ~LogMassTrace()
    {
    }

    /// copy constructor
    LogMassTrace(const LogMassTrace &lmt)
    {
      mass_trace_ = lmt.mass_trace_;
      log_centroid_mz_ = lmt.log_centroid_mz_;
      centroid_mz_ = lmt.centroid_mz_;
      fwhm_start_ = lmt.fwhm_start_;
      fwhm_end_ = lmt.fwhm_end_;
      charge_ = lmt.charge_;
      intensity_ = lmt.intensity_;
      isotope_index_ = lmt.isotope_index_;
      mass_ = lmt.mass_;
      trace_index_ = lmt.trace_index_;
    }

    /// assignment operator
    LogMassTrace &operator=(const LogMassTrace &lmt)
    {
      if (this == &lmt)
        return *this;

      mass_trace_ = lmt.mass_trace_;
      log_centroid_mz_ = lmt.log_centroid_mz_;
      centroid_mz_ = lmt.centroid_mz_;
      charge_ = lmt.charge_;
      fwhm_start_ = lmt.fwhm_start_;
      fwhm_end_ = lmt.fwhm_end_;
      intensity_ = lmt.intensity_;
      isotope_index_ = lmt.isotope_index_;
      mass_ = lmt.mass_;
      trace_index_ = lmt.trace_index_;

      return *this;
    }

    /// constructor with mass trace
    LogMassTrace(MassTrace &mt)
    {
      mass_trace_ = &mt;
      log_centroid_mz_ = std::log(mt.getCentroidMZ() - Constants::PROTON_MASS_U);
      centroid_mz_ = mt.getCentroidMZ();
      charge_ = 0;
      auto fwhm = mt.getFWHMborders();
      fwhm_start_ = mt[fwhm.first].getRT();
      fwhm_end_ = mt[fwhm.second].getRT();
      intensity_ = mt.getIntensity(false); // TODO: smoothed?
      isotope_index_ = -1;

      /// deteremined mass after deconvolution. NOT monoisotopic but only decharged
      mass_ = 0;

      // index of current trace
      trace_index_ = 0;
    }

    /// comparison operator (ascending order)
    bool operator<(const LogMassTrace &fh) const
    {
      return (log_centroid_mz_ < fh.log_centroid_mz_);
    }

    /// getter & setter
    MassTrace *getMassTrace() const
    {
      return mass_trace_;
    }

    double getLogCentroidMz() const
    {
      return log_centroid_mz_;
    }

    double getCentroidMz() const
    {
      return centroid_mz_;
    }

    int getCharge() const
    {
      return charge_;
    }

    double getFwhmStart() const
    {
      return fwhm_start_;
    }

    double getFwhmEnd() const
    {
      return fwhm_end_;
    }

    double getIntensity() const
    {
      return intensity_;
    }

    int getIsotopeIndex() const
    {
      return isotope_index_;
    }

    Size getTraceIndex() const
    {
      return trace_index_;
    }

    void setMassTrace(MassTrace *mt)
    {
      mass_trace_ = mt;
    }

    void setLogCentroidMz(double &mz)
    {
      log_centroid_mz_ = mz;
    }

    void setCentroidMz(double &mz)
    {
      centroid_mz_ = mz;
    }

    void setCharge(int cs)
    {
      charge_ = cs;
    }

    void setFwhmStart(double fwhm_s)
    {
      fwhm_start_ = fwhm_s;
    }

    void setFwhmEnd(double fwhm_e)
    {
      fwhm_end_ = fwhm_e;
    }

    void setIntensity(double inty)
    {
      intensity_ = inty;
    }

    void setIsotopeIndex(double idx)
    {
      isotope_index_ = idx;
    }

    void setTraceIndex(Size i)
    {
      trace_index_ = i;
    }

    double getUnchargedMass()
    {
      if (charge_ == 0)
      {
        return .0;
      }
      if (mass_ <= 0)
      {
        mass_ = (centroid_mz_ - Constants::PROTON_MASS_U) * charge_;
      }
      return mass_;
    }

  private:
    MassTrace *mass_trace_;

    // log transformed centroid mz
    double log_centroid_mz_;
    double centroid_mz_;
    int charge_;
    double fwhm_start_; // from mass trace, in sec
    double fwhm_end_;
    double intensity_;
    int isotope_index_;
    double mass_;

    Size trace_index_;
  };

  // vector class for mass traces from same molecule, different charges and isotope indices
  class OPENMS_DLLAPI FeatureGroup :
      private std::vector<LogMassTrace>
  {
  public :
    using std::vector<LogMassTrace>::push_back;
    using std::vector<LogMassTrace>::empty;
    using std::vector<LogMassTrace>::begin;
    using std::vector<LogMassTrace>::end;
    using std::vector<LogMassTrace>::size;
    using std::vector<LogMassTrace>::reserve;
    using std::vector<LogMassTrace>::swap;
    using std::vector<LogMassTrace>::shrink_to_fit;
    using std::vector<LogMassTrace>::erase;
    using std::vector<LogMassTrace>::operator[];

    /// default constructor
    FeatureGroup() = default;

    /// default destructor
    ~FeatureGroup() = default;

    /// comparison operators
    bool operator<(const FeatureGroup &a) const
    {
      if (this->monoisotopic_mass_ == a.monoisotopic_mass_)
      {
        return this->intensity_ < a.intensity_;
      }
      return this->monoisotopic_mass_ < a.monoisotopic_mass_;
    }

    bool operator>(const FeatureGroup &a) const
    {
      if (this->monoisotopic_mass_ == a.monoisotopic_mass_)
      {
        return this->intensity_ > a.intensity_;
      }
      return this->monoisotopic_mass_ > a.monoisotopic_mass_;
    }

    bool operator==(const FeatureGroup &a) const
    {
      return
          this->monoisotopic_mass_ == a.monoisotopic_mass_
          && this->intensity_ == a.intensity_;
    }

    /// assignment operator
    FeatureGroup &operator=(const FeatureGroup &t) = default;

    /// copy constructor
    FeatureGroup(const FeatureGroup &) = default;

    /// move constructor
    FeatureGroup(FeatureGroup &&other) = default;


    FeatureGroup(const int &min_cs, const int &max_cs) :
        min_abs_charge_(min_cs),
        max_abs_charge_(max_cs)
    {
    }

    // for lower_bound / upper_bound
    FeatureGroup(const double mass) :
        monoisotopic_mass_(mass)
    {
      intensity_ = 0;
    }

    void updateMassesAndIntensity(const int offset = 0, const int max_isotope_index = 0)
    {
      if (offset != 0)
      {
        std::vector<LogMassTrace> tmpPeaks;
        tmpPeaks.swap(*this);
        reserve(tmpPeaks.size());

        for (auto &p: tmpPeaks)
        {
          p.setIsotopeIndex(p.getIsotopeIndex() - offset);
          if (p.getIsotopeIndex() < 0 || p.getIsotopeIndex() >= max_isotope_index)
          {
            continue;
          }
          push_back(p);
        }
      }

      intensity_ = .0;
      double nominator = .0;

      for (auto &p: *this)
      {
        double pi = p.getIntensity() + 1;
        intensity_ += pi;
        nominator += pi * (p.getUnchargedMass() - p.getIsotopeIndex() * Constants::ISOTOPE_MASSDIFF_55K_U);
      }
      monoisotopic_mass_ = nominator / intensity_;
    }

    void updateIntensity()
    {
      //      intensity_ = .0;
      double tmp_inty = .0;
      for (auto &p: *this)
      {
        tmp_inty += p.getIntensity();
      }
      intensity_ = tmp_inty;
    }

    void updateSNR()
    {
      float cos_squred = isotope_cosine_score_ * isotope_cosine_score_;
      float signal = 0, noise = 0;
      per_charge_snr_ = std::vector<float>(1 + max_abs_charge_, .0);

      for (int c = min_abs_charge_; c <= std::min((int) per_charge_signal_pwr_.size(), max_abs_charge_); ++c)
      {
        float per_charge_cos_squared = per_charge_cos_[c] * per_charge_cos_[c];
        float signal_pwr =
            per_charge_signal_pwr_[c] < per_charge_pwr_[c] ? per_charge_signal_pwr_[c] : per_charge_pwr_[c];
        float nom = per_charge_cos_squared * signal_pwr;
        float denom = per_charge_pwr_[c] - signal_pwr
                      + (1 - per_charge_cos_squared) * signal_pwr;

        per_charge_snr_[c] = denom <= 0 ? .0 : nom / denom;

        signal += per_charge_signal_pwr_[c];
        noise += per_charge_pwr_[c] - signal_pwr;
      }

      float t_nom = cos_squred * signal;
      float t_denom = noise
                      + (1 - cos_squred) * signal;
      snr_ = t_denom <= 0 ? .0 : t_nom / t_denom;
    }

    double getMonoisotopicMass() const
    {
      return monoisotopic_mass_;
    }

    float getSNR() const
    {
      return snr_;
    }

    std::tuple<int, int> getChargeRange() const
    {
      return std::tuple<int, int>{min_abs_charge_, max_abs_charge_};
    }

    double getIntensity() const
    {
      return intensity_;
    }

    float getIsotopeCosine() const
    {
      return isotope_cosine_score_;
    }

    float getChargeScore() const
    {
      return charge_score_;
    }

    float getFeatureGroupScore() const
    {
      return total_score_;
    }

    std::tuple<double, double> getMzRange(int abs_charge) const
    {
      double mz_start = -1;
      double mz_end = -10;
      if (abs_charge > max_abs_charge_ || abs_charge < min_abs_charge_)
      {

      }
      else
      {
        for (auto &tmp_p: *this)
        {
          if (tmp_p.getCharge() != abs_charge)
          {
            continue;
          }
          if (mz_start < 0)
          {
            mz_start = tmp_p.getCentroidMz();
          }
          else
          {
            mz_start = mz_start < tmp_p.getCentroidMz() ? mz_start : tmp_p.getCentroidMz();
          }
          mz_end = mz_end > tmp_p.getCentroidMz() ? mz_end : tmp_p.getCentroidMz();
        }
      }
      return std::tuple<double, double>{mz_start, mz_end};

    }

    std::pair<double, double> getFwhmRange() const
    {
      return fwhm_range_;
    }

    std::vector<Size> getTraceIndices() const
    {
      return ltrace_indices_;
    }

    float getIsotopeCosineOfCharge(const int &abs_charge) const
    {
      return per_charge_cos_[abs_charge];
    }

    double getCentroidRtOfApices() const
    {
      return centroid_rt_of_apices;
    }

    double getRtOfApex() const
    {
      return rt_of_apex;
    }

    LogMassTrace* getApexLMTofCharge(int charge) const
    {
      LogMassTrace* apex_lmt = nullptr;
      double max_intensity = 0.0;

      for (auto lmt_iter = this->begin(); lmt_iter != this->end(); ++lmt_iter)
      {
        if (lmt_iter->getCharge() == charge && lmt_iter->getIntensity() > max_intensity)
        {
          max_intensity = lmt_iter->getIntensity();
          apex_lmt = (LogMassTrace*) &(*lmt_iter);
        }
      }
      return apex_lmt;
    }

    std::pair<double, double> getSummedIntensityOfMostAbundantMTperCS() const
    {
      auto per_cs_max_area = std::vector<double>(1 + max_abs_charge_, .0);
      auto per_cs_max_inty = std::vector<double>(1 + max_abs_charge_, .0);

      for (auto &lmt: *this)
      {
        double sum_inty = 0;
        for (auto &p : *(lmt.getMassTrace()))
        {
          sum_inty += p.getIntensity();
        }
        if(per_cs_max_inty[lmt.getCharge()] < sum_inty)
        {
          per_cs_max_inty[lmt.getCharge()] = sum_inty;
        }

        if (per_cs_max_area[lmt.getCharge()] < lmt.getIntensity())
        {
          per_cs_max_area[lmt.getCharge()] = lmt.getIntensity();
        }
      }

      return std::make_pair(std::accumulate(per_cs_max_area.begin(), per_cs_max_area.end(), .0),
                            std::accumulate(per_cs_max_inty.begin(), per_cs_max_inty.end(), .0));
    }

    double getAvgFwhmLength() const
    {
      std::vector<double> fwhm_len_arr;
      fwhm_len_arr.reserve(this->size());

      for (auto &l_trace: *this)
      {
        double tmp_fwhm_len = l_trace.getFwhmEnd() - l_trace.getFwhmStart();
        fwhm_len_arr.push_back(tmp_fwhm_len);
      }
      return accumulate(fwhm_len_arr.begin(), fwhm_len_arr.end(), 0.0) / (this->size());
    }

    void setChargeRange(const int min_c, const int max_c)
    {
      min_abs_charge_ = min_c;
      max_abs_charge_ = max_c;
    }

    void setChargeScore(const float score)
    {
      charge_score_ = score;
    }

    void setIsotopeCosine(const float cos)
    {
      isotope_cosine_score_ = cos;
    }

    void setFeatureGroupScore(const float score)
    {
      total_score_ = score;
    }

    void setChargeIsotopeCosine(const int abs_charge, const float cos)
    {
      if (max_abs_charge_ < abs_charge)
      {
        return;
      }
      if (per_charge_cos_.empty())
      {
        per_charge_cos_ = std::vector<float>(1 + max_abs_charge_, .0);
      }
      per_charge_cos_[abs_charge] = cos;
    }

    void setChargeIntensity(const int abs_charge, const float intensity)
    {
      if (max_abs_charge_ < abs_charge)
      {
        return;
      }
      if (per_charge_int_.empty())
      {
        per_charge_int_ = std::vector<float>(1 + max_abs_charge_, .0);
      }
      per_charge_int_[abs_charge] = intensity;
    }

    void setChargePower(const int abs_charge, const double pwr)
    {
      if (max_abs_charge_ < abs_charge)
      {
        return;
      }
      if (per_charge_pwr_.empty())
      {
        per_charge_pwr_ = std::vector<float>(1 + max_abs_charge_, .0);
      }
      per_charge_pwr_[abs_charge] = pwr;
    }

    void setChargeSignalPower(const int abs_charge, const double pwr)
    {
      if (max_abs_charge_ < abs_charge)
      {
        return;
      }
      if (per_charge_signal_pwr_.empty())
      {
        per_charge_signal_pwr_ = std::vector<float>(1 + max_abs_charge_, .0);
      }
      per_charge_signal_pwr_[abs_charge] = pwr;
    }

    void setFwhmRange()
    {
      double min_fwhm(numeric_limits<double>::max());
      double max_fwhm(0.0);

      for (auto &l_trace: *this)
      {
        std::pair<double, double> tmp_fwhm(l_trace.getFwhmStart(), l_trace.getFwhmEnd());

        if (tmp_fwhm.first < min_fwhm)
          min_fwhm = tmp_fwhm.first;
        if (tmp_fwhm.second > max_fwhm)
          max_fwhm = tmp_fwhm.second;
      }
      fwhm_range_ = std::make_pair(min_fwhm, max_fwhm);
    }

    void initializePerChargeVectors()
    {
      per_charge_cos_.clear();
      per_charge_int_.clear();
      per_charge_cos_ = std::vector<float>(1 + max_abs_charge_, .0);
      per_charge_int_ = std::vector<float>(1 + max_abs_charge_, .0);
      // other vectors (per_charge_pwr_, per_charge_signal_pwr_, per_charge_snr_) don't need to be initialized
    }

    void setTraceIndices()
    {
      ltrace_indices_.clear();
      ltrace_indices_.reserve(this->size());
      for (auto &l_trace: *this)
      {
        ltrace_indices_.push_back(l_trace.getTraceIndex());
      }
      std::sort(ltrace_indices_.begin(), ltrace_indices_.end());
    }

    void updateFwhmBorder(std::pair<double, double> new_fwhm)
    {
      // it is guaranteed new_fwhm overlaps with current fwhm_border;
      if (new_fwhm.first < fwhm_range_.first)
      {
        fwhm_range_.first = new_fwhm.first;
      }
      if (new_fwhm.second > fwhm_range_.second)
      {
        fwhm_range_.second = new_fwhm.second;
      }
    }

    void filterMassTracesWithLowIntensities()
    {
      // get maximum intensity
      double max_intensity = .0;
      for (const auto &lmt: *this)
      {
        if (lmt.getIntensity() > max_intensity)
        {
          max_intensity = lmt.getIntensity();
        }
      }

      // filter out mass traces with intensity lower than threshold
      double threshold = max_intensity * 0.2;
      std::vector<LogMassTrace> tmpPeaks;
      tmpPeaks.swap(*this);
      reserve(tmpPeaks.size());

      for (const auto &p: tmpPeaks)
      {
        if (p.getIntensity() >= threshold)
        {
          push_back(p);
        }
      }
    }

    void filterIsoMassTracesWithLowIntensities()
    {
      // get maximum intensity per charge
      std::vector<double> max_inty_per_charge(1 + max_abs_charge_, .0);
      for (const auto &lmt: *this)
      {
        if (lmt.getIntensity() > max_inty_per_charge[lmt.getCharge()])
        {
          max_inty_per_charge[lmt.getCharge()] = lmt.getIntensity();
        }
      }

      // filter out mass traces with intensity lower than threshold
      //      double threshold = max_intensity * 0.2;
      for (auto &i: max_inty_per_charge)
        i *= .2;

      std::vector<LogMassTrace> tmpPeaks;
      tmpPeaks.swap(*this);
      reserve(tmpPeaks.size());

      for (const auto &p: tmpPeaks)
      {
        if (p.getIntensity() >= max_inty_per_charge[p.getCharge()])
        {
          push_back(p);
        }
      }
    }

    void setCentroidRtOfApices()
    {
      double tmp_rt = .0;
      double max_inty = .0;
      double apex_rt = .0;

      for (const auto &lmt: *this)
      {
        Size max_idx = lmt.getMassTrace()->findMaxByIntPeak(false);
        auto tmp_max_peak = (*lmt.getMassTrace())[max_idx];
        tmp_rt += tmp_max_peak.getRT();
        if (max_inty < tmp_max_peak.getIntensity())
        {
          max_inty = tmp_max_peak.getIntensity();
          apex_rt = tmp_max_peak.getRT();
        }
      }
      tmp_rt /= this->size();
      centroid_rt_of_apices = tmp_rt;
      rt_of_apex = apex_rt;
    }


  private:
    /// information on the deconvouted mass
    double monoisotopic_mass_;
    /// charge range
    int min_abs_charge_, max_abs_charge_; // absolute charge states.
    double intensity_;
    float charge_score_;
    float isotope_cosine_score_;
    float snr_;
    float total_score_;

    std::pair<double, double> fwhm_range_;
    std::vector<Size> ltrace_indices_;

    std::vector<float> per_charge_cos_;
    std::vector<float> per_charge_int_;
    std::vector<float> per_charge_signal_pwr_;
    std::vector<float> per_charge_pwr_;
    std::vector<float> per_charge_snr_;

    double centroid_rt_of_apices;
    double rt_of_apex; // Apex : apex of most abundant mass traces

    //    int most_intense_charge_;
    //    double most_intense_mt_intensity;
    //    std::vector<int> most_intense_charges;
    //    int most_intense_iso_of_most_intense_charge_;
  };

  class OPENMS_DLLAPI CmpLogMassTraceByRT
  {
  public:
    bool operator()(const LogMassTrace& x, const LogMassTrace& y) const
    {
      return x.getFwhmStart() < y.getFwhmStart();
    }
  };

  class OPENMS_DLLAPI CmpLogMassTraceByMZ
  {
  public:
    bool operator()(const LogMassTrace* x, const LogMassTrace* y) const
    {
      return x->getLogCentroidMz() < y->getLogCentroidMz();
    }
  };

  class OPENMS_DLLAPI CmpLogMassTraceByIntensity
  {
  public:
    bool operator()(const LogMassTrace* x, const LogMassTrace* y) const
    {
      // descending order
      return x->getIntensity() > y->getIntensity();
    }
  };

  class OPENMS_DLLAPI CmpFeatureGroupByScore
  {
  public:
    bool operator()(const FeatureGroup& x, const FeatureGroup& y) const
    {
      // intensity
      if(x.getIntensity() == y.getIntensity()){
        return x.getIsotopeCosine() < y.getIsotopeCosine();
      }

      return x.getIntensity() < y.getIntensity();

//      if(x.getIsotopeCosine() == y.getIsotopeCosine()){
//        return x.getIntensity() < x.getIntensity();
//      }
//
//      return x.getIsotopeCosine() < y.getIsotopeCosine();
    }

//    bool operator()(const FeatureGroup* x, const FeatureGroup* y) const
//    {
//      // descending order
//      if(x->getIsotopeCosine() == y->getIsotopeCosine()){
//        return x->getIntensity() < y->getIntensity();
//      }
//
//      return x->getIsotopeCosine() < y->getIsotopeCosine();
//    }

  };

  /// TODO: replace this with FLASHDeconv
  /// This struct contains the averagine patterns precalulated for speed up. Other variables are also calculated for fast cosine calculation
  struct OPENMS_DLLAPI PrecalculatedAveragine
  {
  private:
    /// isotope distributions for different (binned) masses
    std::vector<IsotopeDistribution> isotopes_;
    /// L2 norms for masses
    std::vector<double> norms_;

    /// mass differences between average mass and monoisotopic mass
    std::vector<double> average_mono_mass_difference_;
    /// Isotope start indices: isotopes of the indices less than them have very low intensities
    std::vector<int> left_count_from_apex_;
    /// Isotope end indices: isotopes of the indices larger than them have very low intensities
    std::vector<int> right_count_from_apex_;
    /// most abundant isotope index
    std::vector<Size> apex_index_;
    /// max isotope index
    Size max_isotope_index_;

    /// mass interval for calculation
    double mass_interval_;
    /// min mass for calculation
    double min_mass_;
  public:
    /// default constructor
    PrecalculatedAveragine() = default;

    /**
     @brief constructor with parameters such as mass ranges and interval ( delta ).
     @param m minMass
     @param M maxMass
     @param delta mass interval between m and M
     @param generator generator by which the calulation is done
  */
    PrecalculatedAveragine(const double min_mass,
                           const double max_mass,
                           const double delta,
                           CoarseIsotopePatternGenerator *generator)
        :mass_interval_(delta), min_mass_(min_mass)
    {
      int i = 0;
      while (true)
      {
        double mass = i * mass_interval_;
        i++;
        if (mass < min_mass)
        {
          continue;
        }
        if (mass > max_mass)
        {
          break;
        }
        auto iso = generator->estimateFromPeptideWeight(mass);

        const double min_pwr = .999;
        const Size min_iso_length = 5;
        double total_pwr = .0;
        int most_abundant_index_ = 0;
        double most_abundant_int = 0;

        for (Size k = 0; k < iso.size(); k++) {
          total_pwr += iso[k].getIntensity() * iso[k].getIntensity();
          if (most_abundant_int >= iso[k].getIntensity())
          {
            continue;
          }
          most_abundant_int = iso[k].getIntensity();
          most_abundant_index_ = k;
        }

        int left_count = 0;
        int right_count = iso.size() - 1;
        int trim_count = 0;
        while (iso.size() - trim_count > min_iso_length)
        {
          double lint = iso[left_count].getIntensity();
          double rint = iso[right_count].getIntensity();
          double pwr;
          bool trim_left = true;
          if (lint < rint)
          {
            pwr = lint * lint;
          }
          else
          {
            pwr = rint * rint;
            trim_left = false;
          }
          if (total_pwr - pwr < total_pwr * min_pwr)
          {
            break;
          }
          total_pwr -= pwr;
          trim_count++;
          if (trim_left)
          {
            iso[left_count].setIntensity(0);
            left_count++;
          }
          else
          {
            iso[right_count].setIntensity(0); // for trimming
            right_count--;
          }
        }
        left_count = most_abundant_index_ - left_count;
        right_count = right_count - most_abundant_index_;
        iso.trimRight(1e-10);

        for (Size k = 0; k < iso.size(); k++)
        {
          double ori_int = iso[k].getIntensity();
          iso[k].setIntensity(ori_int / sqrt(total_pwr));
        }

        apex_index_.push_back(most_abundant_index_);
        right_count_from_apex_.push_back(right_count + 1);
        left_count_from_apex_.push_back(left_count + 1);
        average_mono_mass_difference_.push_back(iso.averageMass() - iso[0].getMZ());
        isotopes_.push_back(iso);
      }
    }

    /// get distribution for input mass
    IsotopeDistribution get(const double mass) const
    {
      Size i = (Size) (.5 + std::max(.0, mass - min_mass_) / mass_interval_);
      i = i >= isotopes_.size() ? isotopes_.size() - 1 : i;
      return isotopes_[i];
    }

    /// get max isotope index
    int getMaxIsotopeIndex() const
    {
      return max_isotope_index_;
    }

    /// get isotope start index
    Size getLeftCountFromApex(const double mass) const
    {
      Size i = (Size) (.5 + std::max(.0, mass - min_mass_) / mass_interval_);
      i = i >= isotopes_.size() ? isotopes_.size() - 1 : i;
      return (Size) left_count_from_apex_[i];
    }

    /// get isotope end index
    Size getRightCountFromApex(const double mass) const
    {
      Size i = (Size) (.5 + std::max(.0, mass - min_mass_) / mass_interval_);
      i = i >= isotopes_.size() ? isotopes_.size() - 1 : i;
      return (Size) right_count_from_apex_[i];
    }

    /// get mass difference between avg and mono masses
    double getAverageMassDelta(const double mass) const
    {
      Size i = (Size) (.5 + std::max(.0, mass - min_mass_) / mass_interval_);
      i = i >= isotopes_.size() ? isotopes_.size() - 1 : i;
      return average_mono_mass_difference_[i];
    }

    Size getApexIndex(const double mass) const
    {
      Size i = (Size) (.5 + std::max(.0, mass - min_mass_) / mass_interval_);
      i = i >= isotopes_.size() ? isotopes_.size() - 1 : i;
      return apex_index_[i];
    }

    /// set max isotope index
    void setMaxIsotopeIndex(const int index)
    {
      max_isotope_index_ = index;
    }
  };

  struct OPENMS_DLLAPI FeatureElement
  {
  public:
    std::vector<LogMassTrace*> mass_traces;
    std::vector<Size> mass_trace_indices; // index to input shared_m_traces_indices
    std::vector<double> isotope_probabilities;
//    LogMassTrace* most_abundant_mt_in_fg = nullptr;

    int charge;
    Size feature_group_index;

    /// default constructor
    FeatureElement() = default;

    Size getPeakSizes() const
    {
      Size total_peaks_size = 0;
      for (auto &lmt : mass_traces)
      {
        total_peaks_size += lmt->getMassTrace()->getSize();
      }
      return total_peaks_size;
    }
   };

  class OPENMS_DLLAPI FLASHDeconvQuant :
      public ProgressLogger,
      public DefaultParamHandler
{
  public:
    /// Default constructor
    FLASHDeconvQuant();

    /// Default destructor
    ~FLASHDeconvQuant() override;

    /// main method of FeatureFindingMetabo
    void run(std::vector<MassTrace> &input_mtraces, FeatureMap &output_featmap);

    String outfile_path;

  protected:
    void updateMembers_() override;

  private:
    void logTransformMassTraces_(std::vector<MassTrace> &input_mtraces, std::vector<LogMassTrace> &log_mtraces);

    void setFilters_(); // from FLASHDeconv

    void setAveragineModel_();

    double getBinValue_(const Size &bin, const double &min_value) const;

    Size getBinNumber_(const double &value, const double &min_value) const;

    void updateMzBins_(std::vector<LogMassTrace*> &local_traces, const Size &bin_number,
                       const double& mz_bin_min, std::vector<float> &mz_bin_intensities);

    void unionPrevMassBins_();

    Matrix<int> updateMassBins_(const std::vector<float> &mz_intensities);

    void getCandidatePeakGroups_(const std::vector<LogMassTrace*> &log_mtraces,
                                 const Matrix<int> &per_mass_abs_charge_ranges,
                                 std::vector<FeatureGroup> &fgroup);

    void updateCandidateMassBins_(std::vector<float> &mass_intensitites,
                                                    const std::vector<float> &mz_intensities);
    Matrix<int> filterMassBins_(const std::vector<float> &mass_intensities);

    // equivalent to FLASHDeconvAlgorithm::generatePeakGroupsFromSpectrum_
    void getFeatureFromSpectrum_(std::vector<LogMassTrace*> &local_traces, std::vector<FeatureGroup> &local_fgroup);

    void buildMassTraceGroups_(std::vector<LogMassTrace> &log_mtraces, std::vector<FeatureGroup>& features);

    bool scoreFeatureGroup_(FeatureGroup& fg, double given_iso_score = .0) const;

    void scoreAndFilterPeakGroups_(std::vector<FeatureGroup> &local_fgroup);

    void calculatePerChargeIsotopeIntensity_(std::vector<double> &per_isotope_intensity,
                                                        std::vector<double> &per_charge_intensity,
                                                        const int max_isotope_count,
                                                        FeatureGroup &fg) const;

    double getChargeFitScore_(const std::vector<double> &per_charge_intensity) const;

    bool checkChargeDistribution_(const std::vector<double> &per_charge_intensity) const;

    double getIsotopeCosineAndDetermineIsotopeIndex(const double mono_mass,
                                                    const std::vector<double> &per_isotope_intensities,
                                                    int &offset) const;

    double getCosine_(const std::vector<double> &a,
                      const int &a_start,
                      const int &a_end,
                      const IsotopeDistribution &b,
                      const int &b_size,
                      const int offset) const;

    double getShapeDiff_(const std::vector<double> &a,
                         const int &a_start,
                         const int &a_end,
                         const IsotopeDistribution &b,
                         const int &b_size,
                         const int max_b_index,
                         const int offset) const;

    void removeHarmonicsPeakGroups_(std::vector<FeatureGroup> &local_fgroup) const;

    void removeOverlappingPeakGroups_(std::vector<FeatureGroup> &local_fgroup) const;

    void refineFeatureGroups_(std::vector<FeatureGroup>& features);

    bool rescoreFeatureGroup_(FeatureGroup& fg) const;

    bool doFWHMbordersOverlap(const std::pair<double, double>& border1, const std::pair<double, double>& border2) const;

    bool doMassTraceIndicesOverlap(const FeatureGroup& fg1, const FeatureGroup& fg2) const;

    void clusterFeatureGroups_(std::vector<FeatureGroup>& fgroups,
                               std::vector<MassTrace>& input_mtraces) const;

    void resolveSharedMassTraces(std::vector<FeatureGroup>& fgroups, std::vector<std::vector<Size>>& shared_m_traces,
                                 std::vector<MassTrace>& input_mtraces) const;

    void resolveSharedMassTraces_simple(std::vector<FeatureGroup>& fgroups, std::vector<std::vector<Size>>& shared_m_traces,
                                        std::vector<MassTrace>& input_mtraces) const;

    void resolveConflictInCluster_(std::vector<FeatureGroup>& feature_groups,
                                   const std::vector<MassTrace> & input_masstraces,
                                   const std::vector<std::vector<Size> >& shared_m_traces_indices,
                                   const std::set<Size>& hypo_indices,
                                   std::vector<FeatureGroup>& out_features) const;

    void writeMassTracesOfFeatureGroup(const std::vector<FeatureGroup>& featgroups,
                                       const std::vector<std::vector<Size> >& shared_m_traces_indices,
                                       std::fstream& out) const;

    void writeFeatureGroupsInFile(std::vector<FeatureGroup>& feat, std::vector<MassTrace>& input_mtraces);

    void resolveConflictRegion_(std::vector<FeatureElement> &feat,
                                const std::vector<Size> &feature_idx_in_current_conflict_region,
                                const std::vector<const MassTrace*> &conflicting_mts) const;

    void runElutionModelFit_(FeatureFinderAlgorithmPickedHelperStructs::MassTraces &m_traces, EGHTraceFitter* fitter) const;

    void getMostAbundantMassTraceFromFeatureGroup(const FeatureGroup &fgroup,
                                                  const int &ignore_this_charge,
                                                  LogMassTrace* &most_abundant_mt_ptr,
                                                  const std::vector<std::vector<Size>>& shared_m_traces) const;

    /// parameter stuff
    double local_rt_range_;
    double local_mz_range_;
    Size charge_lower_bound_;
    Size charge_upper_bound_;
    int charge_range_;
    double min_mass_;
    double max_mass_;
    double mz_tolerance_; // ppm

    double total_intensity_;

    const double mass_tolerance_da_ = 3; // Da, for feature mass collection
//    const double mass_tolerance_ppm_ = 20;

    // advanced parameter?
    Size min_nr_mtraces_ = 3; // minimum number of consecutive bridges among mass traces to support feature
    bool use_smoothed_intensities_;
    double rt_window_ = 1; // TODO : remove?

    /// variables for internal use (not for user input)
    double lower_bound_mz_;
    double upper_bound_mz_;
    PrecalculatedAveragine iso_model_;
    Size max_nr_traces_; // calculated from iso_model_ (setAveragineModel())
    double mz_bin_width_;
    double mass_bin_min_value_;
    double mz_bin_min_value_;

    /// high and low charges are differently deconvoluted. This vale determines the (includisve) threshold for low charge.
    const int low_charge_ = 8; // inclusive

    /// cosine threshold between observed and theoretical isotope patterns for MS1
    double min_isotope_cosine_ = 0.90;

    /// This stores the "universal pattern"
    std::vector<double> filter_;
    /// This stores the patterns for harmonic reduction
    Matrix<double> harmonic_filter_matrix_;

    /// This stores the "universal pattern" in binned dimension
    std::vector<int> bin_offsets_;
    /// This stores the patterns for harmonic reduction in binned dimension
    Matrix<int> harmonic_bin_offset_matrix_;

    /// mass_bins_ stores the selected bins for this spectrum + overlapped spectrum (previous a few spectra).
    boost::dynamic_bitset<> mass_bins_;
    /// mz_bins_ stores the binned log mz peaks
    boost::dynamic_bitset<> mz_bins_;
    /// mz_bins_for_edge_effect_ stores the bins to consider edge effect of log mz peak binning
    boost::dynamic_bitset<> mz_bins_for_edge_effect_;

    /// The data structures for spectra overlapping.
    // TODO: need to study this, if it can be removed?
    std::vector<std::vector<Size>> prev_mass_bin_vector_;

    /// harmonic charge factors that will be considered for harmonic mass reduction. For example, 2 is for 1/2 charge harmonic component reduction
    const std::vector<int> harmonic_charges_{2, 3, 5};

  };
}