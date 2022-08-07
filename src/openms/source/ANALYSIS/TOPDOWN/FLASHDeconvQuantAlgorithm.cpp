// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2022.
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

#include <include/OpenMS/ANALYSIS/TOPDOWN/FLASHDeconvQuantAlgorithm.h>
#include <queue>
#include <include/OpenMS/DATASTRUCTURES/Matrix.h>
#include <include/OpenMS/TRANSFORMATIONS/FEATUREFINDER/EGHTraceFitter.h>
#include <include/OpenMS/MATH/MISC/NonNegativeLeastSquaresSolver.h>
#include <include/OpenMS/CONCEPT/LogStream.h>

namespace OpenMS
{
  FLASHDeconvQuantAlgorithm::FLASHDeconvQuantAlgorithm() :
      ProgressLogger(),
      DefaultParamHandler("FLASHDeconvQuantAlgorithm")
  {
    defaults_.setValue("local_rt_range", 15.0, "RT range where to look for coeluting mass traces", {"advanced"});
    defaults_.setValue("local_mz_range",
                       6.5,
                       "MZ range where to look for isotopic mass traces",
                       {"advanced"}); // (-> decides size of isotopes =(local_mz_range_ * lowest_charge))
    defaults_.setValue("charge_lower_bound", 5, "Lowest charge state to consider");
    defaults_.setValue("charge_upper_bound", 50, "Highest charge state to consider");
    defaults_.setValue("min_mass", 10000, "minimim mass");
    defaults_.setValue("max_mass", 70000, "maximum mass");
    defaults_.setValue("mz_tol", 20, "ppm tolerance for m/z");

    defaults_.setValue("use_smoothed_intensities",
                       "true",
                       "Use LOWESS intensities instead of raw intensities.",
                       {"advanced"});
    defaults_.setValidStrings("use_smoothed_intensities", {"false", "true"});

    defaultsToParam_();

    this->setLogType(CMD);
  }

  void FLASHDeconvQuantAlgorithm::updateMembers_()
  {
//    local_rt_range_ = (double) param_.getValue("local_rt_range");
    local_mz_range_ = (double) param_.getValue("local_mz_range");

    charge_lower_bound_ = (Size) param_.getValue("charge_lower_bound");
    charge_upper_bound_ = (Size) param_.getValue("charge_upper_bound");

    min_mass_ = (double) param_.getValue("min_mass");
    max_mass_ = (double) param_.getValue("max_mass");

    mz_tolerance_ = (double) param_.getValue("mz_tol");
    mz_tolerance_ *= 1e-6;

    use_smoothed_intensities_ = param_.getValue("use_smoothed_intensities").toBool();
  }

  Param FLASHDeconvQuantAlgorithm::getFLASHDeconvParams_() const
  {
    Param fd_defaults = FLASHDeconvAlgorithm().getDefaults();
    // overwrite algorithm default so we export everything (important for copying back MSstats results)
    fd_defaults.setValue("min_charge", (int) charge_lower_bound_);
    fd_defaults.setValue("max_charge", (int) charge_upper_bound_);
    fd_defaults.setValue("min_mass", min_mass_);
    fd_defaults.setValue("max_mass", max_mass_);
    fd_defaults.setValue("min_isotope_cosine", DoubleList{.8, .8});

    fd_defaults.setValue("tol", DoubleList{10.0, 10.0});

    return fd_defaults;
  }

  void FLASHDeconvQuantAlgorithm::run(std::vector<MassTrace> &input_mtraces, std::vector<FeatureGroup>& out_fgs)
  {
    // *********************************************************** //
    // Step 1 deconvolute mass traces
    // *********************************************************** //
//    getFLASHDeconvConsensusResult();

    if (Constants::C13C12_MASSDIFF_U * max_nr_traces_ / (int)charge_lower_bound_ < local_mz_range_)
    {
      local_mz_range_ = Constants::C13C12_MASSDIFF_U * (int)max_nr_traces_ / charge_lower_bound_;
    }

    // initialize input & output
    std::vector<FeatureSeed> input_seeds;
    input_seeds.reserve(input_mtraces.size());
    Size index = 0;
    double sum_intensity = 0;
    for (auto iter=input_mtraces.begin(); iter!=input_mtraces.end(); ++iter, ++index)
    {
      FeatureSeed tmp_seed(*iter);
      tmp_seed.setTraceIndex(index);
      input_seeds.push_back(tmp_seed);
      sum_intensity += tmp_seed.getIntensity();
    }
    // sort input mass traces in RT
    std::sort(input_seeds.begin(), input_seeds.end(), FLASHDeconvQuantHelper::CmpFeatureSeedByRT());
    total_intensity_ = sum_intensity;
    std::vector<FeatureGroup> features;
    features.reserve(input_mtraces.size());

    // run deconvolution
    buildMassTraceGroups_(input_seeds, features);
    features.shrink_to_fit();

    // *********************************************************** //
    // Step 2 mass artifact removal & post processing...
    // *********************************************************** //
    refineFeatureGroups_(features);
    OPENMS_LOG_INFO << "#Detected feature groups : " << features.size() << endl;

    // *********************************************************** //
    // Step 3 clustering features
    // *********************************************************** //
    clusterFeatureGroups_(features, input_mtraces);
    OPENMS_LOG_INFO << "#Final feature groups: " << features.size() << endl;

    // output
    setFeatureGroupMembersForResultWriting(features);
    out_fgs.swap(features);
    features.clear();
  }

   void FLASHDeconvQuantAlgorithm::makeMSSpectrum_(std::vector<FeatureSeed *> &local_traces, MSSpectrum &spec, const double &rt) const
  {
    for (auto &tmp_trace : local_traces)
    {
      spec.emplace_back(tmp_trace->getCentroidMz(), tmp_trace->getIntensity());
    }
    spec.setMSLevel(1);
    spec.setName("");
    spec.setRT(rt);
    spec.sortByPosition();
  }

  void FLASHDeconvQuantAlgorithm::setFeatureGroupMembersForResultWriting(std::vector<FeatureGroup> &f_groups) const
  {
    // this cannot be done in FeatureGroup (in FLASHDeconvQuantHelper) due to some methods to be used only in here

    for (auto &fgroup : f_groups)
    {
      // initialize
      auto per_charge_int = std::vector<float>(1 + fgroup.getMaxCharge(), .0);
      auto per_charge_cos = std::vector<float>(1 + fgroup.getMaxCharge(), .0);

      std::vector<std::vector<float>> per_cs_isos(1 + fgroup.getMaxCharge(), std::vector<float>(fgroup.getIsotopeIntensities().size(), .0));

      // iterate all FeatureSeeds to collect per feature values
      for(auto& seed : fgroup)
      {
        per_charge_int[seed.getCharge()] += seed.getIntensity();
        per_cs_isos[seed.getCharge()][seed.getIsotopeIndex()] += seed.getIntensity();
      }

      for(auto& cs : fgroup.getChargeSet())
      {
        auto &this_cs_isos = per_cs_isos[cs];
        int min_isotope_index = std::distance( this_cs_isos.begin(), std::find_if( this_cs_isos.begin(), this_cs_isos.end(), [](auto &x) { return x != 0; }));
        int max_isotope_index = std::distance( this_cs_isos.begin(), (std::find_if( this_cs_isos.rbegin(), this_cs_isos.rend(), [](auto &x) { return x != 0; }) + 1).base());

        auto iso_dist = iso_model_.get(fgroup.getMonoisotopicMass());
        float cos_score = FLASHDeconvAlgorithm::getCosine(per_cs_isos[cs], min_isotope_index, max_isotope_index + 1,
                                                          iso_dist, iso_dist.size(), 0);
        per_charge_cos[cs] = cos_score;
      }
      // calculate average mass
      auto avg_mass = iso_model_.getAverageMassDelta(fgroup.getMonoisotopicMass()) + fgroup.getMonoisotopicMass();

      // setting values
      fgroup.setPerChargeIntensities(per_charge_int);
      fgroup.setPerChargeCosineScore(per_charge_cos);
      fgroup.setAverageMass(avg_mass);
    }
  }

  void FLASHDeconvQuantAlgorithm::getFeatureFromSpectrum_(std::vector<FeatureSeed *> &local_traces,
                                                          std::vector<FeatureGroup> &local_fgroup,
                                                          const double &rt)
  {
     // convert local_traces_ to MSSpectrum
     MSSpectrum spec;
     makeMSSpectrum_(local_traces, spec, rt);

     // run deconvolution
     std::vector<DeconvolvedSpectrum> null_survey_scan; // empty one, since only MS1s are considered.
     const std::map<int, std::vector<std::vector<double>>> null_map; // empty one
     fd_.performSpectrumDeconvolution(spec, null_survey_scan, 0, false, null_map);
     DeconvolvedSpectrum deconv_spec = fd_.getDeconvolvedSpectrum();

     if (deconv_spec.empty()) // if no result was found
     {
       return;
     }

     // convert deconvolved result into FeatureGroup
     for (auto &deconv : deconv_spec)
     {
       // filter out if deconv results are not sufficient
       if (deconv.size() < min_nr_mtraces_)
       {
         continue;
       }

       FeatureGroup fg(deconv);
       fg.setMaxIsotopeIndex(iso_model_.get(deconv.getMonoMass()).size());

       // Add individual FeatureSeeds to FeatureGroup
       for (auto &peak: deconv)
       {
         // if isotope index of this peak is out of threshold, don't include this
         if (peak.isotopeIndex >= (int) fg.getMaxIsotopeIndex())
         {
           continue;
         }

         // find seed index
         auto it = std::find_if(local_traces.begin(), local_traces.end(),
                                [peak] (FeatureSeed* const& f) -> bool {
                                  return (f->getCentroidMz() == peak.mz); // intensity changes in performSpectrumDeconvolution, thus cannot be used for filtering
                                });
         FeatureSeed tmp_seed(**it);
         tmp_seed.setCharge(peak.abs_charge);
         tmp_seed.setIsotopeIndex(peak.isotopeIndex);
         tmp_seed.setMass(peak.mass);

         fg.push_back(tmp_seed);
       }
       local_fgroup.push_back(fg);
     }
  }

  double FLASHDeconvQuantAlgorithm::scoreMZ_(const MassTrace& tr1, const MassTrace& tr2, Size iso_pos, Size charge) const
  {
    double diff_mz(std::fabs(tr2.getCentroidMZ() - tr1.getCentroidMZ()));

    double mt_sigma1(tr1.getCentroidSD());
    double mt_sigma2(tr2.getCentroidSD());
    double mt_variances(std::exp(2 * std::log(mt_sigma1)) + std::exp(2 * std::log(mt_sigma2)));

    double mz_score(0.0);
    /// mz scoring by expected mean w/ C13
    double mu = (Constants::C13C12_MASSDIFF_U * (int)iso_pos) / charge; // using '1.0033548378'
    double sd = .0;
    double sigma_mult(3.0);

    //standard deviation including the estimated isotope deviation
    double score_sigma(std::sqrt(std::exp(2 * std::log(sd)) + mt_variances));

    if ((diff_mz < mu + sigma_mult * score_sigma) && (diff_mz > mu - sigma_mult * score_sigma))
    {
      double tmp_exponent((diff_mz - mu) / score_sigma);
      mz_score = std::exp(-0.5 * tmp_exponent * tmp_exponent);
    }

    return mz_score;
  }

  double FLASHDeconvQuantAlgorithm::scoreRT_(const MassTrace& tr1, const MassTrace& tr2) const
  {
    std::map<double, std::vector<double> > coinciding_rts;

    std::pair<Size, Size> tr1_fwhm_idx(tr1.getFWHMborders());
    std::pair<Size, Size> tr2_fwhm_idx(tr2.getFWHMborders());

    double tr1_length(tr1.getFWHM());
    double tr2_length(tr2.getFWHM());
    double max_length = (tr1_length > tr2_length) ? tr1_length : tr2_length;

    // Extract peak shape between FWHM borders for both peaks
    for (Size i = tr1_fwhm_idx.first; i <= tr1_fwhm_idx.second; ++i)
    {
      coinciding_rts[tr1[i].getRT()].push_back(tr1[i].getIntensity());
    }
    for (Size i = tr2_fwhm_idx.first; i <= tr2_fwhm_idx.second; ++i)
    {
      coinciding_rts[tr2[i].getRT()].push_back(tr2[i].getIntensity());
    }

    // Look at peaks at the same RT
    std::vector<double> x, y, overlap_rts;
    for (auto m_it = coinciding_rts.begin(); m_it != coinciding_rts.end(); ++m_it)
    {
      if (m_it->second.size() == 2)
      {
        x.push_back(m_it->second[0]);
        y.push_back(m_it->second[1]);
        overlap_rts.push_back(m_it->first);
      }
    }

    double overlap(0.0);
    if (!overlap_rts.empty())
    {
      double start_rt(*(overlap_rts.begin())), end_rt(*(overlap_rts.rbegin()));
      overlap = std::fabs(end_rt - start_rt);
    }

    double proportion(overlap / max_length);
    if (proportion < 0.7)
    {
      return 0.0;
    }
    return computeCosineSim_(x, y);
  }

  double FLASHDeconvQuantAlgorithm::computeCosineSim_(const std::vector<double>& x, const std::vector<double>& y) const
  {
    if (x.size() != y.size())
    {
      return 0.0;
    }

    double mixed_sum(0.0);
    double x_squared_sum(0.0);
    double y_squared_sum(0.0);

    for (Size i = 0; i < x.size(); ++i)
    {
      mixed_sum += x[i] * y[i];
      x_squared_sum += x[i] * x[i];
      y_squared_sum += y[i] * y[i];
    }

    double denom(std::sqrt(x_squared_sum) * std::sqrt(y_squared_sum));
    return (denom > 0.0) ? mixed_sum / denom : 0.0;
  }


  bool FLASHDeconvQuantAlgorithm::doFWHMbordersOverlap(const std::pair<double, double> &border1,
                                              const std::pair<double, double> &border2) const
  {
    if ((border1.first > border2.second) || (border2.first > border1.second))
      return false;

    const double overlap_length = std::min(border1.second, border2.second) - std::max(border1.first, border2.first);
    if ((overlap_length / (border1.second - border1.first) < 0.5) &&
        (overlap_length / (border2.second - border2.first) < 0.5))
      return false;

    return true;
  }

  bool FLASHDeconvQuantAlgorithm::doMassTraceIndicesOverlap(const FeatureGroup &fg1, const FeatureGroup &fg2) const
  {
    // get overlapping charge states
    int min_overlapping_charge = std::max(fg1.getMinCharge(), fg2.getMinCharge());
    int max_overlapping_charge = std::min(fg1.getMaxCharge(), fg2.getMaxCharge());

    if (min_overlapping_charge > max_overlapping_charge) // no overlapping charge
    {
      return true;
    }

    // collect possible overlapping mass_traces based on charges
    std::vector<Size> mt_indices_1;
    std::vector<Size> mt_indices_2;
    mt_indices_1.reserve(fg1.size());
    mt_indices_2.reserve(fg2.size());
    for (Size fg1_idx = 0; fg1_idx < fg1.size(); ++fg1_idx)
    {
      if (fg1[fg1_idx].getCharge() >= min_overlapping_charge && fg1[fg1_idx].getCharge() <= max_overlapping_charge)
      {
        mt_indices_1.push_back(fg1[fg1_idx].getTraceIndex());
      }
    }
    for (Size fg2_idx = 0; fg2_idx < fg2.size(); ++fg2_idx)
    {
      if (fg2[fg2_idx].getCharge() >= min_overlapping_charge && fg2[fg2_idx].getCharge() <= max_overlapping_charge)
      {
        mt_indices_2.push_back(fg2[fg2_idx].getTraceIndex());
      }
    }
    std::sort(mt_indices_1.begin(), mt_indices_1.end());
    std::sort(mt_indices_2.begin(), mt_indices_2.end());

    Size min_vec_size = std::min(mt_indices_1.size(), mt_indices_2.size());
    std::vector<Size> inters_vec;
    inters_vec.reserve(min_vec_size);
    //    std::vector<Size>::iterator  inters_it;
    std::set_intersection(mt_indices_1.begin(),
                          mt_indices_1.end(),
                          mt_indices_2.begin(),
                          mt_indices_2.end(),
                          std::back_inserter(inters_vec));

    //    double overlap_p = inters_vec.size() / min_vec_size;
    //    double overlap_percentage = static_cast<double>(inters_vec.size()) / static_cast<double>(min_vec_size);
    // TODO : change this to overlapping only major cs?
    if (inters_vec.size() < 1)
    {
      return false;
    }
    return true;
  }

  bool FLASHDeconvQuantAlgorithm::rescoreFeatureGroup_(FeatureGroup &fg, bool score_anyways) const
  {
    if ((!scoreAndFilterFeatureGroup_(fg)) && (!score_anyways) )
    {
      return false;
    }

    // update private members in FeatureGroup based on the changed FeatureSeeds
    fg.updateMembers();
    setFeatureGroupScore_(fg);
    return true;
  }

  bool FLASHDeconvQuantAlgorithm::scoreAndFilterFeatureGroup_(FeatureGroup &fg) const
  {
    /// based on: FLASHDeconvAlgorithm::scoreAndFilterPeakGroups_()
    /// return false when scoring is not done (filtered out)

    // update monoisotopic mass, isotope_intensities_ and charge vector to use for filtering
    fg.updateMembersForScoring();

    /// if this FeatureGroup is within the target, pass any filter
    bool isNotTarget = true;
    if (with_target_masses_ && isThisMassOneOfTargets(fg.getMonoisotopicMass(), fg.getRtOfMostAbundantMT()))
    {
      isNotTarget = false;
    }

    /// filter if the number of charges are not enough
    if (isNotTarget && (fg.getChargeSet().size() < min_nr_mtraces_))
    {
      return false;
    }

    /// filter if the mass is out of range
    if (isNotTarget && (fg.getMonoisotopicMass() < min_mass_ || fg.getMonoisotopicMass() > max_mass_))
    {
      return false;
    }

    /// isotope cosine calculation
    int offset = 0;
    int second_max_offset = -1000;
    float isotope_score =
        FLASHDeconvAlgorithm::getIsotopeCosineAndDetermineIsotopeIndex(fg.getMonoisotopicMass(), fg.getIsotopeIntensities(), offset, second_max_offset, iso_model_);
    fg.setIsotopeCosine(isotope_score);
    if ( isNotTarget && isotope_score < min_isotope_cosine_ )
    {
      return false;
    }

    return true;
  }

  void FLASHDeconvQuantAlgorithm::setFeatureGroupScore_(FeatureGroup &fg) const
  {
    /// setting per_isotope_score
    auto iso_dist = iso_model_.get(fg.getMonoisotopicMass());
    int iso_size = (int) iso_dist.size();

    // setting for feature group score
    double feature_score = .0;

    for (auto &abs_charge : fg.getChargeSet())
    {
      double max_intensity = .0;
      vector<FeatureSeed*> traces_in_this_charge;
      FeatureSeed* apex_trace;
      // find the apex trace in this charge
      for (auto &peak: fg)
      {
        if (peak.getCharge() != abs_charge)
        {
          continue;
        }

        if (peak.getIsotopeIndex() > iso_size)
        {
          continue;
        }

        if (max_intensity < peak.getIntensity())
        {
          max_intensity = peak.getIntensity();
          apex_trace = &peak;
        }
        traces_in_this_charge.push_back(&peak);
      }

      // if no trace is collected
      if (max_intensity == .0)
      {
        continue;
      }

      auto current_per_isotope_intensities = vector<float>(iso_model_.getMaxIsotopeIndex(), .0);

      int min_isotope_index = iso_model_.getMaxIsotopeIndex();
      int max_isotope_index = 0;
      double per_charge_score = .0;
      // loop over traces with this charge to collect scores
      for(auto &trace_ptr : traces_in_this_charge)
      {
        current_per_isotope_intensities[trace_ptr->getIsotopeIndex()] += trace_ptr->getIntensity();
        min_isotope_index = min_isotope_index < trace_ptr->getIsotopeIndex() ? min_isotope_index : trace_ptr->getIsotopeIndex();
        max_isotope_index = max_isotope_index < trace_ptr->getIsotopeIndex() ? trace_ptr->getIsotopeIndex() : max_isotope_index;

        // if apex trace, no further scoring
        if (trace_ptr == apex_trace)
        {
          per_charge_score += apex_trace->getIntensity() / fg.getIntensity();
          continue;
        }

        // score per pair between this trace and the apex
        double mz_score(scoreMZ_(trace_ptr->getMassTrace(), apex_trace->getMassTrace(),
                                 abs(trace_ptr->getIsotopeIndex()-apex_trace->getIsotopeIndex()), abs_charge));
        double rt_score(scoreRT_(trace_ptr->getMassTrace(), apex_trace->getMassTrace()));
        double inty_score(trace_ptr->getIntensity() / fg.getIntensity());
        double total_pair_score = exp(log(rt_score) + log(mz_score) + log(inty_score));

        per_charge_score += total_pair_score;
      }
      feature_score += per_charge_score;

//      // isotope cosine score for only this charge
//      double cos_score = FLASHDeconvAlgorithm::getCosine(current_per_isotope_intensities,
//                                                          min_isotope_index,
//                                                          max_isotope_index,
//                                                          iso_dist,
//                                                          iso_size,
//                                                          0);
//      fg.setChargeIsotopeCosine(abs_charge, cos_score);
    }

    fg.setFeatureGroupScore(feature_score);
  }

  void FLASHDeconvQuantAlgorithm::refineFeatureGroups_(std::vector<FeatureGroup> &in_features)
  {
    // change min, max charges based on built FeatureGroups (for later use in scoring)
    int min_abs_charge = INT_MAX;
    int max_abs_charge = INT_MIN;

    // output features
    std::vector<FeatureGroup> out_feature;
    out_feature.reserve(in_features.size());

    // sort features by masses
    std::sort(in_features.begin(), in_features.end());

    // set variables according to the detected features
    for (auto &f: in_features)
    {
      min_abs_charge =
          min_abs_charge < f.getMinCharge() ? min_abs_charge : f.getMinCharge();
      max_abs_charge =
          max_abs_charge > f.getMaxCharge() ? max_abs_charge : f.getMaxCharge();
    }
    charge_lower_bound_ = min_abs_charge;
    charge_upper_bound_ = max_abs_charge;

    Size initial_size = in_features.size();
    this->startProgress(0, initial_size, "refining feature groups");
    // insert FeatureGroup with the highest score to out_features, merge if other FeatureGroup exist within mass_tol
    while (!in_features.empty())
    {
      this->setProgress(initial_size - in_features.size());

      // get a feature with the highest Intensity
      auto candidate_fg = std::max_element(in_features.begin(), in_features.end(), FLASHDeconvQuantHelper::CmpFeatureGroupByScore());

      // get all features within mass_tol from candidate FeatureGroup
      std::vector<FeatureGroup>::iterator low_it, up_it;

      FeatureGroup lower_fg(candidate_fg->getMonoisotopicMass() - mass_tolerance_da_);
      FeatureGroup upper_fg(candidate_fg->getMonoisotopicMass() + mass_tolerance_da_);

      low_it = std::lower_bound(in_features.begin(), in_features.end(), lower_fg);
      up_it = std::upper_bound(in_features.begin(), in_features.end(), upper_fg);

      // no matching in features (found only candidate itself)
      if (up_it - low_it == 1)
      {
        // save it to out_features
        if (rescoreFeatureGroup_(*candidate_fg)) // rescoring is needed to set scores in FeatureGroup
        {
          out_feature.push_back(*candidate_fg);
        }
        // remove candidate from features
        in_features.erase(candidate_fg);
        continue;
      }

      // check if found features are overlapping with the candidate feature
      std::vector<int> v_indices_to_remove;
      v_indices_to_remove.reserve(up_it - low_it);
      std::set<Size> mt_indices_to_add;
      std::vector<FeatureSeed *> mts_to_add; // One unique mt can be included in different FGs -> different FeatureSeed
      mts_to_add.reserve((up_it - low_it) * candidate_fg->size());

      for (; low_it != up_it; ++low_it)
      {
        // if low_it is candidate feature, ignore
        if (candidate_fg == low_it)
        {
          v_indices_to_remove.push_back(low_it - in_features.begin());
          continue;
        }

        // check if fwhm overlaps
        if (!doFWHMbordersOverlap(low_it->getFwhmRange(), candidate_fg->getFwhmRange()))
        {
          continue;
        }

//        // check if masstrace overlaps
//        if (!doMassTraceIndicesOverlap(*low_it, *candidate_fg))
//        {
//          continue;
//        }

        // merge found feature to candidate feature
        auto trace_indices = candidate_fg->getTraceIndices();
        for (auto &new_mt: *low_it)
        {
          // if this mass trace is not used in candidate_fg
          if (std::find(trace_indices.begin(), trace_indices.end(), new_mt.getTraceIndex()) == trace_indices.end())
          {
            mt_indices_to_add.insert(new_mt.getTraceIndex());
            mts_to_add.push_back(&new_mt);
          }
        }
        // add index of found feature to "to_be_removed_vector"
        v_indices_to_remove.push_back(low_it - in_features.begin());
      }

      // sort mts_to_add by abundance
      std::sort(mts_to_add.begin(), mts_to_add.end(), FLASHDeconvQuantHelper::CmpFeatureSeedByIntensity());

      // add extra masstraces to candidate_feature
      FeatureGroup final_candidate_fg(*candidate_fg); // copy of candidate_feature
      for (auto &new_mt: mts_to_add)
      {
        // to skip duplicated masstraces that are included
        if (mt_indices_to_add.find(new_mt->getTraceIndex()) == mt_indices_to_add.end())
        {
          continue;
        }
        mt_indices_to_add.erase(new_mt->getTraceIndex());

        FeatureSeed *apex_lmt_in_this_cs = final_candidate_fg.getApexLMTofCharge(new_mt->getCharge());
        // if this mt is introducing new charge
        if (apex_lmt_in_this_cs == nullptr)
        {
          final_candidate_fg.push_back(*new_mt);
          continue;
        }

        /// re-calculate isotope index (from FLASHDeconvAlgorithm::getCandidatePeakGroup)
        double apex_lmt_mass = apex_lmt_in_this_cs->getUnchargedMass();
        double new_lmt_mass = new_mt->getUnchargedMass();

        int tmp_iso_idx = round((new_lmt_mass - apex_lmt_mass) / Constants::ISOTOPE_MASSDIFF_55K_U);
        if (abs(apex_lmt_mass - new_lmt_mass + Constants::ISOTOPE_MASSDIFF_55K_U * tmp_iso_idx) >
            apex_lmt_mass * mz_tolerance_)
        {
          continue;
        }
        if (apex_lmt_in_this_cs->getIsotopeIndex() + tmp_iso_idx < 0)
        {
          // if new iso_idx is not within iso_range (too small)
          continue;
        }
        if (apex_lmt_in_this_cs->getIsotopeIndex() + tmp_iso_idx >= (int) candidate_fg->getMaxIsotopeIndex())
        {
          // if new iso_idx is not within iso_range (too large)
          continue;
        }
        new_mt->setIsotopeIndex(apex_lmt_in_this_cs->getIsotopeIndex() + tmp_iso_idx);

        final_candidate_fg.push_back(*new_mt);
      }

      // don't merge when it failed to exceed filtering threshold
      if (!rescoreFeatureGroup_(final_candidate_fg))
      {
        if (rescoreFeatureGroup_(*candidate_fg))
        {
          out_feature.push_back(*candidate_fg);
        }
        // remove it from features
        in_features.erase(candidate_fg);
        continue;
      }

      // save it to out_features
      out_feature.push_back(final_candidate_fg);

      // remove candidate from features
      std::sort(v_indices_to_remove.begin(), v_indices_to_remove.end());

      std::vector<FeatureGroup> tmp_out_fgs;
      tmp_out_fgs.reserve(in_features.size() - v_indices_to_remove.size());
      for (Size i = 0; i < in_features.size(); ++i)
      {
        if (std::find(v_indices_to_remove.begin(), v_indices_to_remove.end(), i) == v_indices_to_remove.end())
        {
          tmp_out_fgs.push_back(in_features[i]);
        }
      }
      in_features.swap(tmp_out_fgs);
    }
    this->endProgress();

    in_features.swap(out_feature);
  }

  void FLASHDeconvQuantAlgorithm:: buildMassTraceGroups_(std::vector<FeatureSeed> &mtraces,
                                               std::vector<FeatureGroup> &features)
  {
    /// FLASHDeconvAlgorithm setting
    Param fd_defaults = getFLASHDeconvParams_();
    fd_.setParameters(fd_defaults);
    fd_.calculateAveragine(false);
    iso_model_ = fd_.getAveragine();
//    std::vector<double> target_masses_; // monoisotope
//    fd_.setTargetMasses(target_masses_, ms_level);

    /// group mass traces to spectrum
    std::vector<std::pair<double, FeatureSeed*>> mt_rt_starts;
    std::vector<std::pair<double, FeatureSeed*>> mt_rt_ends;
    mt_rt_starts.reserve(mtraces.size());
    mt_rt_ends.reserve(mtraces.size());
    int counter = 0;

    // collect rt information from mtraces to generate spectrum
    double min_fwhm_length = numeric_limits<double>::max();
    for (auto &trace: mtraces)
    {
      mt_rt_starts.push_back(std::make_pair(trace.getFwhmStart(), &trace));
      mt_rt_ends.push_back(std::make_pair(trace.getFwhmEnd(), &trace));
      if (trace.getMassTrace().getFWHM() < min_fwhm_length)
      {
        min_fwhm_length = trace.getMassTrace().getFWHM();
      }
    }

    if (min_fwhm_length > rt_window_)
    {
      rt_window_ = min_fwhm_length;
    }

    // sorting mass traces in rt
    std::sort(mt_rt_starts.begin(), mt_rt_starts.end());
    std::sort(mt_rt_ends.begin(), mt_rt_ends.end());

    std::vector<std::pair<double, FeatureSeed *>>::const_iterator rt_s_iter = mt_rt_starts.begin();
    std::vector<std::pair<double, FeatureSeed *>>::const_iterator rt_e_iter = mt_rt_ends.begin();
    auto end_of_iter = mt_rt_starts.end();
    double end_of_current_rt_window = mt_rt_starts[0].first;
    double last_rt = mt_rt_ends[mt_rt_ends.size() - 1].first;

    // mass traces to be added in a spectrum
    std::vector<FeatureSeed *> local_traces;
    local_traces.reserve(mtraces.size());

    int possible_spec_size = int((mt_rt_starts[mt_rt_starts.size() - 1].first - end_of_current_rt_window) / rt_window_);
    this->startProgress(0, possible_spec_size, "assembling mass traces to features");

    while (rt_s_iter != end_of_iter && end_of_current_rt_window < last_rt)
    {
      this->setProgress(counter);

      // initial rt binning is 1 sec (for generating spectrum)
      end_of_current_rt_window += rt_window_;

      // add mass traces within rt range
      bool is_new_mt_added = false;
      for (; rt_s_iter != end_of_iter && rt_s_iter->first <= end_of_current_rt_window; ++rt_s_iter)
      {
        local_traces.push_back(rt_s_iter->second);
        is_new_mt_added = true;
      }

      // if nothing is added, increase current_rt
      if (!is_new_mt_added)
      {
        continue;
      }

      // remove mass traces out of rt range
      for (; rt_e_iter != mt_rt_ends.end() && rt_e_iter->first < end_of_current_rt_window; ++rt_e_iter)
      {
        local_traces.erase(std::remove_if(local_traces.begin(), local_traces.end(),
                                          [&rt_e_iter](auto const &p) { return rt_e_iter->second == p; }));
      }
      if (local_traces.empty())
      {
        continue;
      }

      // sort local traces in mz
      sort(local_traces.begin(), local_traces.end(), FLASHDeconvQuantHelper::CmpFeatureSeedByMZ());

      std::vector<FeatureGroup> local_fgroup;
      getFeatureFromSpectrum_(local_traces, local_fgroup, end_of_current_rt_window);
      ++counter; // to track the number of generated spectra
      // no feature has been detected
      if (local_fgroup.size() == 0)
      {
        continue;
      }

      for (auto &tmp_fg: local_fgroup)
      {
        sort(tmp_fg.begin(), tmp_fg.end());

        tmp_fg.updateMembersForScoring();
        tmp_fg.updateMembers();
      }

      features.insert(features.end(), local_fgroup.begin(), local_fgroup.end());
    }

    this->endProgress();
    OPENMS_LOG_INFO << "# generated spec from mass traces : " << counter << endl;
    OPENMS_LOG_INFO << "# generated feature groups from mass traces : " << features.size() << endl;
  }

  /// cluster FeatureGroups with shared mass traces, and resolve the shared ones. If not, report as output
  void FLASHDeconvQuantAlgorithm::clusterFeatureGroups_(std::vector<FeatureGroup> &fgroups,
                                                        std::vector<MassTrace> &input_mtraces) const
  {
    // *********************************************************** //
    // Step 1 preparation for hypergraph : collect feature idx with shared mass traces
    // *********************************************************** //
    std::vector<std::vector<Size>> shared_m_traces(input_mtraces.size(), std::vector<Size>());
    for (Size fg_index = 0; fg_index < fgroups.size(); ++fg_index)
    {
      for (auto &mt_i: fgroups[fg_index].getTraceIndices())
      {
        shared_m_traces[mt_i].push_back(fg_index);
      }
    }
    /// only for writing purpose (for non-resolved masstrace drawing)
    writeMassTracesOfFeatureGroup(fgroups, shared_m_traces);

    // *********************************************************** //
    // Step 2 constructing hypergraph from featuregroups
    //        node = mass traces
    //        hyperedge = feature groups
    // *********************************************************** //
    Size num_nodes = shared_m_traces.size();
    std::vector<bool> bfs_visited;
    bfs_visited.resize(num_nodes, false);
    std::queue<Size> bfs_queue;
    Size search_pos = 0; // keeping track of mass trace index to look for seed

    std::vector<FeatureGroup> out_features;
    out_features.reserve(fgroups.size());

    // BFS
    while (true)
    {
      // finding a seed 'shared_mass_trace' to start with (for constructing a cluster)
      bool finished = true;
      for (Size i = search_pos; i < num_nodes; ++i)
      {
        if (!bfs_visited[i])
        {
          // check if this mass_trace is used to any FeatureGroup
          if (shared_m_traces[i].size() == 0)
          {
            bfs_visited[i] = true;
            continue;
          }

          bfs_queue.push(i);
          bfs_visited[i] = true;
          finished = false;
          search_pos = i + 1;
          break;
        }
      }
      if (finished) // if no possible seed is left
        break;

      std::set<Size> fg_indices_in_current_cluster;

      while (!bfs_queue.empty())
      {
        Size i = bfs_queue.front(); // index of seed
        bfs_queue.pop();

        // get FeatureGroup indices sharing this seed
        for (vector<Size>::const_iterator it = shared_m_traces[i].begin();
             it != shared_m_traces[i].end();
             ++it)
        {
          // if this FeatureGroup was visited before
          if (fg_indices_in_current_cluster.find(*it) != fg_indices_in_current_cluster.end())
          {
            continue;
          }

          fg_indices_in_current_cluster.insert(*it);
          FeatureGroup &current_fg = fgroups[*it];

          for (const auto &mt_index: current_fg.getTraceIndices())
          {
            if (!bfs_visited[mt_index])
            {
              bfs_queue.push(mt_index);
              bfs_visited[mt_index] = true;
            }
          }
        }
      }

      // this feature is not sharing any mass traces with others
      if (fg_indices_in_current_cluster.size() == 1)
      {
        out_features.push_back(fgroups[*(fg_indices_in_current_cluster.begin())]);
        continue;
      }

      // resolve the conflict among feature groups
      resolveConflictInCluster_(fgroups, input_mtraces, shared_m_traces, fg_indices_in_current_cluster, out_features);
    }

    out_features.shrink_to_fit();
    std::swap(out_features, fgroups);
  }

  void FLASHDeconvQuantAlgorithm::writeMassTracesOfFeatureGroup(const std::vector<FeatureGroup> &feature_groups,
                                                                const std::vector<std::vector<Size>> &shared_m_traces_indices) const
  {
    // writing mass traces (when no resolution is done)
    String out_path = output_file_path_.substr(0, output_file_path_.find_last_of(".")-1) + "groups.tsv";
    ofstream out;
    out.open(out_path, ios::out);
    out << "mono_mass\tcharge\tisotope_index\tquant_value\tshared\tcentroid_mzs\trts\tmzs\tintys\n"; // header

    for (const auto &feat: feature_groups)
    {
      auto mt_idxs = feat.getTraceIndices();

      for (const auto &mt: feat)
      {
        // check if current mt is shared with other features
        bool isShared = false;
        if (shared_m_traces_indices[mt.getTraceIndex()].size() > 1)
        {
          isShared = true;
        }

        stringstream rts;
        stringstream mzs;
        stringstream intys;

        for (const auto &peak: mt.getMassTrace())
        {
          mzs << peak.getMZ() << ",";
          rts << peak.getRT() << ",";
          intys << peak.getIntensity() << ",";
        }
        std::string peaks = rts.str();
        peaks.pop_back();
        peaks = peaks + "\t" + mzs.str();
        peaks.pop_back();
        peaks = peaks + "\t" + intys.str();
        peaks.pop_back();

//        quant += mt.getMassTrace()->computePeakArea();

        out << std::to_string(feat.getMonoisotopicMass()) << "\t"
            << mt.getCharge() << "\t"
            << mt.getIsotopeIndex() << "\t"
            << std::to_string(mt.getIntensity()) << "\t"
            << isShared << "\t" << std::to_string(mt.getCentroidMz()) << "\t"
            << peaks + "\n";
      }
    }
    out.close();
  }

  void FLASHDeconvQuantAlgorithm::resolveConflictInCluster_(std::vector<FeatureGroup> &feature_groups,
                                                            std::vector<MassTrace> &input_masstraces,
                                                            std::vector<std::vector<Size>> &shared_m_traces_indices,
                                                            const std::set<Size> &fg_indices_in_this_cluster,
                                                            std::vector<FeatureGroup> &out_featuregroups) const
  {
    /// conflict resolution is done in feature level (not feature group level), starting from the most abundant shared signal
    /// input masstraces needs to be changed, if any resolution is done per FeatureGroup -> add new input_masstrace with modified intensities, and change the pointer to it.

    /// 1. find shared MTs and corresponding "Features" (not FeatureGroup)
    std::unordered_map<Size, std::vector<std::pair<Size, int>>> shared_mt_N_features; // key: mt_idx, value: pair<feature_group_idx, charge>
    std::vector<std::pair<Size, double>> shared_mt_idx_inty_pairs; // first: mt_idx, second: mt intensity
    for (auto &fg_idx : fg_indices_in_this_cluster)
    {
      auto &fgroup = feature_groups[fg_idx];
      for (auto &seed_iter : fgroup)
      {
        auto trace_idx = seed_iter.getTraceIndex();
        // collect only shared one
        if (shared_m_traces_indices[trace_idx].size() == 1)
        {
          continue;
        }

        if (shared_mt_N_features.find(trace_idx) == shared_mt_N_features.end()) // if this seed is not added to the map
        {
          shared_mt_N_features.emplace(trace_idx, std::vector<std::pair<Size, int>>());
          shared_mt_idx_inty_pairs.push_back(std::make_pair(trace_idx, seed_iter.getIntensity()));
        }
        shared_mt_N_features[trace_idx].push_back(std::make_pair(fg_idx, seed_iter.getCharge()));
      }
    }

    /// 2. sort shared MT by intensity (descending)
    std::sort(shared_mt_idx_inty_pairs.begin(), shared_mt_idx_inty_pairs.end(), [](const auto &left, const auto &right) {
      return left.second > right.second;
    });

    /// 3. start from the highest intensity, iterate shared MT and get conflicting region (collecting conflicting Features)
    while(!shared_mt_idx_inty_pairs.empty())
    {
      /// 3.1 collecting all shared mts and features in this conflicting region
      std::unordered_map<Size, bool> shared_mts_in_this_region; // to track all detected shared mass traces. key = shared_mt_idx, value = visited?
      std::set<String> visited_features_in_this_region; // to track all detected features. key = [FeatGroupID]&[CS]
      std::vector<std::pair<Size, int>> feat_not_for_resolution; // key = [FeatGroupID]&[CS]
      std::vector<Feature> conflicting_features;
      std::vector<MassTrace> conflicting_mts; // collect shared traces for resolution (not FeatureSeed, but original MassTrace. FeatureSeed has conflicting information)
      std::vector<Size> conflicting_mt_indices; // trace_index of collect shared traces for resolution

      shared_mts_in_this_region.emplace(shared_mt_idx_inty_pairs[0].first, false); // add the first shared mt
      while(true)
      {
        // find unvisited shared mt
        auto found_mt = std::find_if(std::begin(shared_mts_in_this_region), std::end(shared_mts_in_this_region),
                               [](auto&& p) { return p.second == false; });
        // if no shared mt left
        if (found_mt == std::end(shared_mts_in_this_region))
        {
          break;
        }

        Size this_shared_mt_index = found_mt->first;
        shared_mts_in_this_region[this_shared_mt_index] = true; // mark as visited
        std::vector<String> feat_not_to_include;
        FeatureSeed *picked_seed; // save for later

        // per conflicting features, make Feature
        for (auto &feat : shared_mt_N_features[this_shared_mt_index])
        {
          FeatureGroup &org_featgroup = feature_groups[feat.first];
          int cs_of_feat = feat.second;

          // check if FeatureElement is not created yet
          String feat_label = feat.first + "&" + cs_of_feat;
          if (visited_features_in_this_region.find(feat_label) != visited_features_in_this_region.end())
          {
            continue;
          }
          visited_features_in_this_region.insert(feat_label); // mark as this Feature is visited

          // initialize Feature
          Feature new_feature;
          new_feature.prepareVectors(org_featgroup.size());
          new_feature.charge = cs_of_feat;
          new_feature.feature_group_index = feat.first;

          // add seeds in element
          for (auto &feat_seed: org_featgroup)
          {
            if (feat_seed.getCharge() != cs_of_feat)
            {
              continue;
            }

            if (shared_m_traces_indices[feat_seed.getTraceIndex()].size() > 1) // this seed is shared
            {
              new_feature.shared_traces.push_back(feat_seed);
              new_feature.shared_trace_indices.push_back(feat_seed.getTraceIndex());
            }
            else // this seed is not shared
            {
              new_feature.unique_traces.push_back(feat_seed);
              new_feature.unique_trace_indices.push_back(feat_seed.getTraceIndex());
            }
          }

          // check if this Feature is eligible for resolution
          if (!isEligibleFeatureForConflictResolution(new_feature, shared_m_traces_indices, org_featgroup))
          {
            // put this feature in the list
            feat_not_to_include.push_back(feat_label);
            feat_not_for_resolution.push_back(feat);
            continue;
          }
          new_feature.shrinkVectors();

          // if any seed is shared, mark for visiting in the next round
          for (auto &mt_idx : new_feature.shared_trace_indices)
          {
            if (shared_mts_in_this_region.find(mt_idx) == shared_mts_in_this_region.end()) // if this shared trace is not visited yet
            {
              shared_mts_in_this_region.emplace(mt_idx, false);
            }
          }
          conflicting_features.push_back(new_feature);
        }

        // check if this mt is still shared one
        if (shared_mt_N_features[this_shared_mt_index].size() - feat_not_to_include.size() < 2)
        {
          auto &shared_fs = shared_mt_N_features[this_shared_mt_index];
          for (auto &f : feat_not_to_include)
          {
            shared_fs.erase(std::remove_if(shared_fs.begin(), shared_fs.end(),
                                           [f](auto const &p) { auto s = p.first + "$" + p.second;
                                             return s == f;}));
          }

          if (shared_fs.empty())
          {
            OPENMS_LOG_INFO << "something's wrong. need to resolve this issue" << endl;
          }

          // mark this MT is not shared one
          Size fg_id = shared_fs[0].first;
          shared_m_traces_indices[this_shared_mt_index] = std::vector<Size>{fg_id};
          continue;
        }

        // add this shared mt as a conflicting one
        auto i = input_masstraces.begin() + this_shared_mt_index;
        conflicting_mts.push_back(*i);
        conflicting_mt_indices.push_back(this_shared_mt_index);
      }

      /// 3.2 set isotope probabilities for features (to be used as theoretical intensity when modeling a theoretical shape)
      for (auto &feat : conflicting_features)
      {
        auto &feat_group = feature_groups[feat.feature_group_index];
        auto theo_isodist = iso_model_.get(feat_group.getMonoisotopicMass());
        feat.isotope_probabilities.reserve(feat.unique_traces.size()); // isotope prob for each mass_trace, based on their iso position
        for (auto &lmt: feat.unique_traces)
        {
          // if this trace is taken from different Feature
          if (lmt.getCharge() != feat.charge)
          {
            feat.isotope_probabilities.push_back(0.1); // arbitrary small number. maybe change later... (should not be large)
          }

          // if isotope index of lmt exceed tmp_iso length, give 0 (to not use for the modeling)
          Size tmp_iso_idx = (Size) lmt.getIsotopeIndex();
          if (tmp_iso_idx < theo_isodist.size()) // To avoid bad access error. If index is larger than the isodist length, no intensity to use.
          {
            // give weight per masstrace based on their isotope position, not the real intensity.
            feat.isotope_probabilities.push_back(theo_isodist[tmp_iso_idx].getIntensity());
          }
          else
          {
            feat.isotope_probabilities.push_back(0.0);
          }
        }
      }

      /// 3.3 resolve the conflict in this region
      resolveConflictRegion_(conflicting_features, conflicting_mts, conflicting_mt_indices);

      /// 3.4 Update FeatureGroup (only FeatureSeed vectors, other members will be updated later)
      for (auto &feat : conflicting_features)
      {
        // get original FeatureGroup, and update it
        FeatureGroup &feature_group = feature_groups[feat.feature_group_index];

        for (Size t_index = 0; t_index < feat.shared_trace_indices.size(); ++t_index)
        {
          auto &updated_seed = feat.shared_traces[t_index];
          auto &index_of_updated_seed = feat.shared_trace_indices[t_index];

          // find corresponding FeatureSeed in FeatureGroup
          auto found_seed = find_if(feature_group.begin(), feature_group.end(),
                                    [index_of_updated_seed](auto &&x) { return x.getTraceIndex() == index_of_updated_seed; });

          // if updated seed is empty, add remove found seed
          if (updated_seed.getIntensity() == 0)
          {
            feature_group.erase(found_seed);
            continue;
          }

          *found_seed = updated_seed;
          found_seed->setTraceIndex(input_masstraces.size()); // add new trace index
          // add new FeatureSeed
          input_masstraces.push_back(updated_seed.getMassTrace());
        }
      }

      /// 3.5 remove all conflicting MTs from "look-up table"
      for (auto &shared : shared_mts_in_this_region)
      {
        shared_mt_idx_inty_pairs.erase(std::remove_if(shared_mt_idx_inty_pairs.begin(), shared_mt_idx_inty_pairs.end(),
                                                      [shared](auto const &p) {return p.first == shared.first;}));
      }

      // remove this feature out from Fg? // some features need to be deleted
      if (feat_not_for_resolution.size() > 0)
      {
        for (auto &feat_tag : feat_not_for_resolution)
        {
          auto &feat_group = feature_groups[feat_tag.first];
          auto seed_iter = feat_group.begin();
          while (seed_iter != feat_group.end()) {
            if (seed_iter->getCharge() == feat_tag.second) {
              // if this seed is from the "feat_not_for_resolution", remove it.
              seed_iter = feat_group.erase(seed_iter);
            }
            else {
              ++seed_iter;
            }
          }
//          feat_group.updateMembersForScoring();
//          feat_group.updateMembers();
        }
      }
    }


    /// 4. update FeatureGroup members and quantities
    for (auto &idx: fg_indices_in_this_cluster)
    {
      auto &fgroup = feature_groups[idx];

      // remove 0 intensity mass traces
      auto fg_iter = fgroup.begin();
      while (fg_iter != fgroup.end())
      {
        if (fg_iter->getIntensity() == 0)
        {
          fg_iter = fgroup.erase(fg_iter);
        }
        else
        {
          ++fg_iter;
        }
      }

      // check if enough number of mass traces are left
      if (fgroup.size() < min_nr_mtraces_)
      {
        continue;
      }

      // update TODO: check if "score_anyways" in rescoreFeatureGroup is needed
      if (rescoreFeatureGroup_(fgroup))
      {
        out_featuregroups.push_back(fgroup);
      }
    }
  }

  bool FLASHDeconvQuantAlgorithm::isEligibleFeatureForConflictResolution(Feature &feat, std::vector<std::vector<Size>> &shared_m_traces_indices, FeatureGroup &feat_group) const
  {
    // check if this feature is composed of only shared mass traces
    if (feat.unique_traces.size() > 0)
    {
      return true;
    }

    // get the most abundant mass trace from FeatureGroup (except recruited ones)
    FeatureSeed *most_abundant_mt = nullptr;
    getMostAbundantMassTraceFromFeatureGroup(feat_group, feat.charge, most_abundant_mt, shared_m_traces_indices);
    if (most_abundant_mt == nullptr)
    {
      return false;
    }

    // get the most abundant mass trace from FeatureGroup (except recruited ones)
    feat.unique_traces.push_back(*most_abundant_mt);
    feat.unique_trace_indices.push_back(most_abundant_mt->getTraceIndex());
    return true;
  }

//  void FLASHDeconvQuantAlgorithm::resolveConflictInCluster_(std::vector<FeatureGroup> &feature_groups,
//                                                            const std::vector<MassTrace> &input_masstraces,
//                                                            const std::vector<std::vector<Size> > &shared_m_traces_indices,
//                                                            const std::set<Size> &fg_indices_in_this_cluster,
//                                                            std::vector<FeatureGroup> &out_featuregroups) const
//  {
//    /// conflict resolution is done in feature level (not feature group level), starting from the most abundant one
//
//    // prepare nodes for hypergraph
//    std::vector<std::vector<Size>> mt_and_feature_idx(shared_m_traces_indices.size(), std::vector<Size>());
//    std::vector<FeatureElement> feature_candidates; // index : (fg_indices_in_this_cluster+1)*charge_idx
//    for (auto &fg_idx: fg_indices_in_this_cluster)
//    {
//      auto &fg = feature_groups[fg_idx];
//      OPENMS_LOG_DEBUG << fg_idx << "\t" << fg.getMonoisotopicMass() << endl;
//
//      // get charge vector
//      Size feature_idx_starts = feature_candidates.size();
//      std::set<int> current_fg_cs = fg.getChargeSet();
//      for (auto &tmp_cs : current_fg_cs)
//      {
//        FeatureElement tmp;
//        tmp.mass_traces.reserve(fg.size());
//        tmp.mass_trace_indices.reserve(fg.size());
//        tmp.charge = tmp_cs;
//        tmp.feature_group_index = fg_idx;
//        feature_candidates.push_back(tmp);
//      }
//
//      for (auto lmt_iter = fg.begin(); lmt_iter != fg.end(); ++lmt_iter)
//      {
//        Size f_idx = feature_idx_starts + std::distance(current_fg_cs.begin(),
//                                                        std::find(current_fg_cs.begin(), current_fg_cs.end(), lmt_iter->getCharge()));
//        mt_and_feature_idx[lmt_iter->getTraceIndex()].push_back(f_idx);
//        feature_candidates[f_idx].mass_traces.push_back(&(*lmt_iter));
//        feature_candidates[f_idx].mass_trace_indices.push_back(lmt_iter->getTraceIndex());
//      }
//    }
//
//    Size num_nodes = shared_m_traces_indices.size();
//    std::vector<bool> bfs_visited;
//    bfs_visited.resize(num_nodes, false);
//    std::queue<Size> bfs_queue;
//    Size search_pos = 0; // keeping track of mass trace index to look for seed
//
//    // BFS
//    while (true)
//    {
//      bool finished = true;
//
//      for (Size i = search_pos; i < num_nodes; ++i)
//      {
//        if (!bfs_visited[i])
//        {
//          // check if this mass trace is used in any feature
//          if (mt_and_feature_idx[i].size() == 0)
//          {
//            bfs_visited[i] = true;
//            continue;
//          }
//
//          bfs_queue.push(i);
//          bfs_visited[i] = true;
//          finished = false;
//          search_pos = i + 1;
//          break;
//        }
//      }
//      if (finished) // if no possible seed is left
//        break;
//
//      std::set<Size> feature_idx_in_current_conflict_region;
//      std::set<Size> conflicting_mt_indices;
//
//      while (!bfs_queue.empty())
//      {
//        Size i = bfs_queue.front(); // index of seed
//        bfs_queue.pop();
//
//        // get feature indices sharing this seed
//        for (std::vector<Size>::const_iterator it = mt_and_feature_idx[i].begin();
//             it != mt_and_feature_idx[i].end();
//             ++it)
//        {
//          // if this feature was visited before
//          if (feature_idx_in_current_conflict_region.find(*it) != feature_idx_in_current_conflict_region.end())
//          {
//            continue;
//          }
//
//          feature_idx_in_current_conflict_region.insert(*it);
//          FeatureElement &selected_feature = feature_candidates[*it];
//
//          for (const auto &mt_index: selected_feature.mass_trace_indices)
//          {
//            if (!bfs_visited[mt_index])
//            {
//              bfs_queue.push(mt_index);
//              bfs_visited[mt_index] = true;
//            }
//            else if (mt_and_feature_idx[mt_index].size() > 1)
//            {
//              // check if this visited mass trace is the shared one (because the seed mass trace can end up here as well)
//              conflicting_mt_indices.insert(mt_index);
//            }
//          }
//        }
//      }
//
//      // this feature is not sharing any mass traces with others
//      if (feature_idx_in_current_conflict_region.size() == 1)
//      {
//        continue;
//      }
//
//      // collect conflicting mass traces (not LogMassTrace, originals)
//      std::vector<const MassTrace *> conflicting_mts;
//      for (auto &mt_idx: conflicting_mt_indices)
//      {
//        auto i = input_masstraces.begin() + mt_idx;
//        conflicting_mts.push_back(&(*i));
//      }
//
//      // set isotope probabilities for feature_candidates && check if mass traces are only shared ones
//      vector<Size> features_not_for_resolution;
//      for (auto &feat_idx: feature_idx_in_current_conflict_region)
//      {
//        auto &feat = feature_candidates[feat_idx];
//        auto &feat_group = feature_groups[feat.feature_group_index];
//        auto tmp_iso = iso_model_.get(feat_group.getMonoisotopicMass());
//        feat.isotope_probabilities.reserve(feat.mass_traces.size());
//        for (auto &lmt: feat.mass_traces)
//        {
//          // if isotope index of lmt exceed tmp_iso length, give 0
//          int tmp_iso_idx = lmt->getIsotopeIndex();
//          if (tmp_iso_idx < (int) tmp_iso.size())
//          {
//            feat.isotope_probabilities.push_back(tmp_iso[tmp_iso_idx].getIntensity());
//          }
//          else
//          {
//            feat.isotope_probabilities.push_back(0.0);
//          }
//        }
//
//        // check if this feature has enough mass traces to get a component
//        if (feat.mass_traces.size() > conflicting_mts.size())
//        {
//          continue;
//        }
//
//        // check if this feature is composed of only shared mass traces
//        bool composed_of_only_conflicts = true;
//        for (auto &mt_idx: feat.mass_trace_indices)
//        {
//          if (shared_m_traces_indices[mt_idx].size() == 1)
//          {
//            composed_of_only_conflicts = false;
//            break;
//          }
//        }
//        if (!composed_of_only_conflicts)
//        {
//          continue;
//        }
//
//        // get the most abundant mass trace from FeatureGroup (except recruited ones)
//        FeatureSeed *most_abundant_mt = nullptr;
//        getMostAbundantMassTraceFromFeatureGroup(feat_group, feat.charge, most_abundant_mt, shared_m_traces_indices);
//        if (most_abundant_mt == nullptr)
//        {
//          features_not_for_resolution.push_back(feat_idx);
//          continue;
//        }
//
//        // get the most abundant mass trace from FeatureGroup (except recruited ones)
//        feat.mass_traces.push_back(most_abundant_mt);
//        feat.mass_trace_indices.push_back(most_abundant_mt->getTraceIndex());
//        feat.isotope_probabilities.push_back(0.2); // TODO: why 0.2?
//      }
//
//      // if any feature is not eligible for resolution
//      if (features_not_for_resolution.size() > 0)
//      {
//        for (Size &idx: features_not_for_resolution)
//        {
//          feature_idx_in_current_conflict_region.erase(idx);
//          for (auto &lmt: feature_candidates[idx].mass_traces)
//          {
//            lmt->setIntensity(0.0);
//          }
//        }
//
//        if (feature_idx_in_current_conflict_region.size() < 2)
//        {
//          continue;
//        }
//
//        // check if any shared mass trace is not shared anymore
//        vector<Size> is_mt_included(conflicting_mts.size(),
//                                    0); // element is equal to the size of feature_vec when it's shared
//        for (auto &feat_idx: feature_idx_in_current_conflict_region)
//        {
//          auto &lmts = feature_candidates[feat_idx].mass_traces;
//          for (auto &lmt: lmts)
//          {
//            for (Size mt_i = 0; mt_i < conflicting_mts.size(); ++mt_i)
//            {
//              if (conflicting_mts[mt_i] == lmt->getMassTrace())
//              {
//                is_mt_included[mt_i] += 1;
//                break;
//              }
//            }
//          }
//        }
//        for (int mt_i = conflicting_mts.size() - 1; mt_i >= 0; --mt_i)
//        {
//          if (is_mt_included[mt_i] != feature_idx_in_current_conflict_region.size())
//          {
//            conflicting_mts.erase(conflicting_mts.begin() + mt_i);
//          }
//        }
//      }
//
//      // convert feature_idx_in_current_conflict_region from set to vector (for accessing with indices later)
//      std::vector<Size> feature_indices_vec;
//      feature_indices_vec.reserve(feature_idx_in_current_conflict_region.size());
//      for (auto &i: feature_idx_in_current_conflict_region)
//        feature_indices_vec.push_back(i);
//
//      // resolve this conflict region
//      resolveConflictRegion_(feature_candidates, feature_indices_vec, conflicting_mts);
//    }
//
//    // update feature group quantities
//    for (auto &idx: fg_indices_in_this_cluster)
//    {
//      auto &fgroup = feature_groups[idx];
//
//      // remove 0 intensity mass traces
//      auto fg_iter = fgroup.begin();
//      while (fg_iter != fgroup.end())
//      {
//        if (fg_iter->getIntensity() == 0)
//        {
//          fg_iter = fgroup.erase(fg_iter);
//        }
//        else
//        {
//          ++fg_iter;
//        }
//      }
//
//      // check if enough number of mass traces are left
//      if (fgroup.size() < min_nr_mtraces_)
//      {
//        continue;
//      }
//
//      // update TODO: check if "score_anyways" in rescoreFeatureGroup is needed
//      if (rescoreFeatureGroup_(feature_groups[idx]))
//      {
//        out_featuregroups.push_back(feature_groups[idx]);
//      }
//    }
//  }

  void FLASHDeconvQuantAlgorithm::resolveConflictRegion_(std::vector<Feature> &conflicting_features,
                                                         const std::vector<MassTrace> &conflicting_mts,
                                                         const std::vector<Size> &conflicting_mt_indices) const
  {
    /// Prepare Components per features (excluding conflicting mts)
    std::vector<std::vector<double>> components;
    Matrix<int> pointer_to_components; // row : index of conflicting_mts , column : index of conflicting_features
    pointer_to_components.resize(conflicting_mts.size(), conflicting_features.size(), -1);
    for (Size i_of_f = 0; i_of_f < conflicting_features.size(); ++i_of_f)
    {
      auto &tmp_feat = conflicting_features[i_of_f];
      Size mass_traces_size = tmp_feat.unique_traces.size();

      // preparation for ElutionModelFit
      FeatureFinderAlgorithmPickedHelperStructs::MassTraces traces_for_fitting;
      traces_for_fitting.reserve(mass_traces_size);
      traces_for_fitting.max_trace = 0;
      vector<Peak1D> peaks;
      // reserve space once, to avoid copying and invalidating pointers:
      peaks.reserve(tmp_feat.getPeakSizes());

      // get theoretical information from unique mass traces
      for (Size idx = 0; idx < mass_traces_size; ++idx)
      {
        auto &mass_trace_ptr = tmp_feat.unique_traces[idx].getMassTrace();

        // prepare for fitting
        FeatureFinderAlgorithmPickedHelperStructs::MassTrace tmp_mtrace;
        Size m_trace_size = mass_trace_ptr.getSize();
        tmp_mtrace.peaks.reserve(m_trace_size);
        for (auto &p_2d: mass_trace_ptr)
        {
          auto tmp_inty = p_2d.getIntensity();
          if (tmp_inty > 0.0) // only use non-zero intensities for fitting
          {
            Peak1D peak;
            peak.setMZ(p_2d.getMZ());
            peak.setIntensity(tmp_inty);
            peaks.push_back(peak);
            tmp_mtrace.peaks.emplace_back(p_2d.getRT(), &peaks.back());
          }
        }
        tmp_mtrace.updateMaximum();
        tmp_mtrace.theoretical_int = tmp_feat.isotope_probabilities[idx];
        traces_for_fitting.push_back(tmp_mtrace);
      }

      // TODO: check if this is necessary. (Padding 0s)
      if (traces_for_fitting.size() == 1 && traces_for_fitting[0].peaks.size() < 4)
      {
        while (traces_for_fitting[0].peaks.size() < 4)
        {
          auto &tmp_trace = traces_for_fitting[0];
          Peak1D peak;
          peak.setMZ(tmp_trace.peaks[0].second->getMZ());
          peak.setIntensity(0.0);
          peaks.push_back(peak);
          double region_start = tmp_trace.peaks.front().second->getPos();
          double region_end = tmp_trace.peaks.back().second->getPos();
          double offset = 0.2 * (region_end - region_start);
          tmp_trace.peaks.emplace_back(region_end + offset, &peaks.back());
        }
      }

      // ElutionModelFit
      EGHTraceFitter *fitter = new EGHTraceFitter();
      // TODO : is this necessary? giving isotope probability as a weight
      Param params = fitter->getDefaults();
      params.setValue("weighted", "true");
      fitter->setParameters(params);
      runElutionModelFit_(traces_for_fitting, fitter);

      // store Component information (Component = calculated model of the trace)
      for (Size row = 0; row < conflicting_mts.size(); ++row)
      {
        // if the feature doesn't own this conflicting_mt, skip
        auto &trace_indices = tmp_feat.shared_trace_indices;
        if (trace_indices.end() == std::find(trace_indices.begin(), trace_indices.end(), conflicting_mt_indices[row]))
        {
          continue;
        }

        // normalize fitted value
        std::vector<double> fit_intensities;
        double summed_intensities = .0;
        for (auto &peak: conflicting_mts[row])
        {
          double rt = peak.getRT();
          double fitted_value = fitter->getValue(rt);
          summed_intensities += fitted_value;
          fit_intensities.push_back(fitted_value);
        }

        // save normalized intensities into component
        std::vector<double> component;
        for (auto &inty: fit_intensities)
        {
          component.push_back(inty/summed_intensities);
        }

        pointer_to_components.setValue(row, i_of_f, components.size()); // set the index to this component
        components.push_back(component);
      }
    }

    // reconstruction of observed XICs (per conflicting mt)
    for (Size row = 0; row < conflicting_mts.size(); ++row)
    {
      Size org_index_of_this_trace = conflicting_mt_indices[row];
      auto &obs_masstrace = conflicting_mts[row];

      auto components_indices = pointer_to_components.row(row);
      Size column_size =
          components_indices.size() - std::count(components_indices.begin(), components_indices.end(), -1); // number of features involved with this mt

      // prepare observed XIC vector
      Size mt_size = obs_masstrace.getSize();
      double obs_total_intensity = .0;
      for (auto &peak : obs_masstrace)
      {
        obs_total_intensity += peak.getIntensity();
      }
      Matrix<double> obs;
      obs.resize(mt_size, 1);
      for (Size i = 0; i < mt_size; ++i)
      {
        obs.setValue(i, 0, obs_masstrace[i].getIntensity()/obs_total_intensity); // save normalized value
      }

      // prepare theoretical matrix (include only related features)
      Matrix<double> theo_matrix;
      theo_matrix.resize(mt_size, column_size);
      Size col = 0;
      for (Size comp_idx = 0; comp_idx < components_indices.size(); ++comp_idx)
      {
        // skipping no-feature column
        if (components_indices[comp_idx] == -1)
          continue;

        auto &temp_comp = components[pointer_to_components.getValue(row, comp_idx)];
        for (Size tmp_r = 0; tmp_r < temp_comp.size(); ++tmp_r)
        {
          theo_matrix.setValue(tmp_r, col, temp_comp[tmp_r]);
        }
        col++;
      }

      Matrix<double> out_quant;
      out_quant.resize(column_size, 1);
      NonNegativeLeastSquaresSolver::solve(theo_matrix, obs, out_quant);

      OPENMS_LOG_DEBUG << "-------------------------" << endl;
      OPENMS_LOG_DEBUG << "observed m/z : " << std::to_string(obs_masstrace.getCentroidMZ()) << endl;

      for (auto &peak: conflicting_mts[row])
      {
        OPENMS_LOG_DEBUG << std::to_string(peak.getRT()) << "\t" << std::to_string(peak.getIntensity()) << "\n";
      }

      // if any out_quant is zero, give the other group all.
      vector<double> calculated_ratio;
      Size zero_ratio_counter = 0;
      for (Size i = 0; i < out_quant.rows(); ++i)
      {
        calculated_ratio.push_back(out_quant.getValue(i, 0));
        if (out_quant.getValue(i, 0) == 0)
        {
          ++zero_ratio_counter;
        }
      }
      if (calculated_ratio.size() - zero_ratio_counter == 1) // only one out_quant was non-zero
      {
        for (auto &ratio:calculated_ratio)
        {
          if (ratio != 0)
          {
            ratio = 1;
          }
        }
      }

      /// update Features based on the calculated ratio
      col = 0;
      for (Size i_of_f = 0; i_of_f < components_indices.size(); ++i_of_f) // iterate feature
      {
        // skipping not-this-feature column
        if (components_indices[i_of_f] == -1)
          continue;

        auto &feat = conflicting_features[i_of_f];
        auto lmt_ptr = std::find_if(feat.shared_traces.begin(), feat.shared_traces.end(),
                                    [org_index_of_this_trace](auto &&x) { return x.getTraceIndex() == org_index_of_this_trace; });
        OPENMS_LOG_DEBUG << "feature " << col << " -> \t" << calculated_ratio[col]
                        << "\t" << lmt_ptr->getMass() << "\t" << lmt_ptr->getCharge() << std::endl;

//        auto temp_component = components[pointer_to_components.getValue(row, i_of_f)];
//        double theo_intensity = std::accumulate(temp_component.begin(), temp_component.end(), 0.0);

        // Kyowon's advice! ratio should be applied to real intensity, not theoretical one
        lmt_ptr->setIntensity(calculated_ratio[col] * obs_total_intensity);

        OPENMS_LOG_DEBUG << "--- theo[" << col << "]--- " << std::to_string(calculated_ratio[col]) << "\t"
                         << std::to_string(lmt_ptr->getIntensity()) << "\n";
        for (auto &m: theo_matrix.col(col))
        {
          OPENMS_LOG_DEBUG << to_string(m) << ", ";
        }
        OPENMS_LOG_DEBUG << std::endl;
//        OPENMS_LOG_INFO << std::to_string(lmt_ptr->getMass());

        /// Update MassTrace itself
        MassTrace updated = updateMassTrace(lmt_ptr->getMassTrace(), calculated_ratio[col]);
        lmt_ptr->setMassTrace(updated);

        col++;
      }
    }
    components.clear();
  }

  // update MassTrace
  MassTrace FLASHDeconvQuantAlgorithm::updateMassTrace(const MassTrace& ref_trace, const double &ratio) const
  {
    /// reference = ElutionPeakDetection::detectElutionPeaks_ (step3)
    // if ratio = 0, return empty MassTrace
    if (ratio == 0)
    {
      return MassTrace();
    }

    std::vector<PeakType> tmp_mt;
    std::vector<double> smoothed_tmp;
    for (Size p_index = 0; p_index < ref_trace.getSize(); ++p_index)
    {
      auto &peak = ref_trace[p_index];
      PeakType new_peak(peak);
      new_peak.setIntensity(peak.getIntensity() * ratio);
      tmp_mt.push_back(new_peak);
      smoothed_tmp.push_back(ref_trace.getSmoothedIntensities()[p_index]);
    }

    // create new mass trace
    MassTrace new_trace(tmp_mt);
    new_trace.setSmoothedIntensities(smoothed_tmp);
    // set label as if it's a sub-trace
    new_trace.setLabel(ref_trace.getLabel() + "_" + String(ratio));
    new_trace.updateSmoothedMaxRT();
    new_trace.updateWeightedMeanMZ();
    new_trace.updateWeightedMZsd();
    new_trace.setQuantMethod(ref_trace.getQuantMethod());
    new_trace.estimateFWHM(use_smoothed_intensities_);

    return new_trace;
  }

  // from ElutionModelFitter.
  void FLASHDeconvQuantAlgorithm::runElutionModelFit_(FeatureFinderAlgorithmPickedHelperStructs::MassTraces &m_traces,
                                                      EGHTraceFitter *fitter) const
  {
    // find the trace with maximal intensity:
    Size max_trace = 0;
    double max_intensity = 0;
    for (Size i = 0; i < m_traces.size(); ++i)
    {
      if (m_traces[i].max_peak->getIntensity() > max_intensity)
      {
        max_trace = i;
        max_intensity = m_traces[i].max_peak->getIntensity();
      }
    }
    m_traces.max_trace = max_trace;
    m_traces.baseline = 0.0;

    // TODO : add zeros?


    // Fitting
    try
    {
      fitter->fit(m_traces);
    }
    catch (Exception::UnableToFit &except)
    {
      OPENMS_LOG_ERROR << "Error fitting model to feature '"
                       << except.getName()
                       << " - " << except.getMessage() << endl;
    }

    // record model parameters:
    //    double center = fitter->getCenter(), height = fitter->getHeight();
    //    double sigma = fitter->getSigma();
    //    double tau = fitter->getTau();
    //    double width = sigma * 0.6266571 + abs(tau);
    //    double asymmetry = abs(tau) / sigma;
    //
    //    double lower_rt_bound = fitter->getLowerRTBound();
    //    double upper_rt_bound = fitter->getUpperRTBound();

  }

  void FLASHDeconvQuantAlgorithm::getMostAbundantMassTraceFromFeatureGroup(const FeatureGroup &fgroup,
                                                                  const int &skip_this_charge,
                                                                  FeatureSeed *&most_abundant_mt_ptr,
                                                                  const std::vector<std::vector<Size>> &shared_m_traces) const
  {
    // get intensities
    double max_intensity = .0; // maximum mt intensity in this feature group

    for (auto lmt = fgroup.begin(); lmt != fgroup.end(); ++lmt)
    {
      if (skip_this_charge > 0 && lmt->getCharge() == skip_this_charge)
      {
        continue;
      }

      // check if this mt is shared with other FeatureGroup
      if (shared_m_traces[lmt->getTraceIndex()].size() > 1)
      {
        continue;
      }

      if (lmt->getIntensity() > max_intensity)
      {
        max_intensity = lmt->getIntensity();
        most_abundant_mt_ptr = (FeatureSeed *) &(*lmt);
      }
    }
  }

  void FLASHDeconvQuantAlgorithm::getFLASHDeconvConsensusResult()
  {
    with_target_masses_ = true;
    std::fstream fin;
    fin.open("/Users/jeek/Documents/A4B/FDQ/Kiel-Human/FDQ_target_from_rep9.csv", std::ios::in);
//    fin.open("/Users/jeek/Documents/A4B/FDQ/Kiel-Human/ProSightPD_concensus.csv", std::ios::in);

    std::string line;
    std::getline(fin, line); // skip the first line
    while (std::getline(fin, line))
    {
      Size comma_loc = line.find(',');
      target_masses_.push_back(std::make_pair(stod(line.substr(0, comma_loc)), stod(line.substr(comma_loc + 1))));
    }

    std::sort(target_masses_.begin(), target_masses_.end());
    fin.close();
  }

  bool FLASHDeconvQuantAlgorithm::isThisMassOneOfTargets(const double &candi_mass, const double &candi_rt) const
  {
    auto low_it = std::lower_bound(target_masses_.begin(), target_masses_.end(), std::make_pair(candi_mass - 1.5, .0));
    auto up_it = std::upper_bound(target_masses_.begin(), target_masses_.end(), std::make_pair(candi_mass + 1.5, .0));

    bool is_it_in_the_list = false;
    // if only one mass is found
    if (low_it == up_it)
    {
      // check if any mass is within range
      if (abs(low_it->first-candi_mass) <= 1.5 && abs(low_it->second-candi_rt) < 180)
      {
        return true;
      }
    }
    for (auto tmp_it = low_it; tmp_it != up_it; ++tmp_it)
    {
      // check if any mass is within range
      if (abs(tmp_it->first-candi_mass) <= 1.5 && abs(tmp_it->second-candi_rt) < 180)
      {
        is_it_in_the_list = true;
      }
    }
    return is_it_in_the_list;
  }
}