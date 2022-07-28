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
// $Maintainer: Kyowon Jeong, Jihyung Kim $
// $Authors: Kyowon Jeong, Jihyung Kim $
// --------------------------------------------------------------------------

#include <OpenMS/ANALYSIS/TOPDOWN/DeconvolvedSpectrum.h>

namespace OpenMS
{
  DeconvolvedSpectrum::DeconvolvedSpectrum(const MSSpectrum& spectrum, const int scan_number) : scan_number_(scan_number)
  {
    spec_ = spectrum;
  }

  MSSpectrum DeconvolvedSpectrum::toSpectrum(const int to_charge, bool retain_undeconvolved)
  {
    auto out_spec = MSSpectrum(spec_);
    out_spec.clear(false);
    if (spec_.getMSLevel() > 1 && precursor_peak_group_.empty())
    {
      return out_spec;
    }
    double charge_mass_offset = abs(to_charge) * FLASHDeconvHelperStructs::getChargeMass(to_charge >= 0);
    std::unordered_set<double> deconvolved_mzs;

    for (auto& pg : *this)
    {
      if (pg.empty())
      {
        continue;
      }
      out_spec.emplace_back(pg.getMonoMass() + charge_mass_offset, pg.getIntensity());
      if (retain_undeconvolved)
      {
        for (auto& p : pg)
        {
          deconvolved_mzs.insert(p.mz);
        }
      }
    }
    if (retain_undeconvolved)
    {
      for (auto& p : spec_)
      {
        if (deconvolved_mzs.find(p.getMZ()) != deconvolved_mzs.end()) // if p is deconvolved
        {
          continue;
        }
        out_spec.emplace_back(p.getMZ() + charge_mass_offset - FLASHDeconvHelperStructs::getChargeMass(to_charge >= 0), p.getIntensity());
      }
    }
    out_spec.sortByPosition();
    if (!precursor_peak_group_.empty() && !precursor_peak_.empty())
    {
      Precursor precursor(spec_.getPrecursors()[0]);
      // precursor.setCharge((precursor_peak_group_.isPositive() ?
      //                      precursor_peak_group_.getRepAbsCharge() :
      //                      -precursor_peak_group_.getRepAbsCharge()));//getChargeMass
      precursor.setCharge(to_charge);
      precursor.setMZ(precursor_peak_group_.getMonoMass() + charge_mass_offset);
      precursor.setIntensity(precursor_peak_group_.getIntensity());
      out_spec.getPrecursors().clear();
      out_spec.getPrecursors().emplace_back(precursor);
    }
    return out_spec;
  }


  const MSSpectrum& DeconvolvedSpectrum::getOriginalSpectrum() const
  {
    return spec_;
  }

  const PeakGroup& DeconvolvedSpectrum::getPrecursorPeakGroup() const
  {
    if (precursor_peak_group_.empty())
    {
      return *(new PeakGroup());
    }
    return precursor_peak_group_;
  }

  int DeconvolvedSpectrum::getPrecursorCharge() const
  {
    return precursor_peak_.getCharge();
  }

  double DeconvolvedSpectrum::getCurrentMaxMass(const double max_mass) const
  {
    if (spec_.getMSLevel() == 1 || precursor_peak_group_.empty())
    {
      return max_mass;
    }
    return precursor_peak_group_.getMonoMass();
  }

  double DeconvolvedSpectrum::getCurrentMinMass(const double min_mass) const
  {
    if (spec_.getMSLevel() == 1)
    {
      return min_mass;
    }
    return 50.0;
  }

  int DeconvolvedSpectrum::getCurrentMaxAbsCharge(const int max_abs_charge) const
  {
    if (spec_.getMSLevel() == 1 || precursor_peak_group_.empty())
    {
      return max_abs_charge;
    }
    return abs(precursor_peak_.getCharge());
  }

  const Precursor& DeconvolvedSpectrum::getPrecursor() const
  {
    return precursor_peak_;
  }

  int DeconvolvedSpectrum::getScanNumber() const
  {
    return scan_number_;
  }

  int DeconvolvedSpectrum::getPrecursorScanNumber() const
  {
    return precursor_scan_number_;
  }


  const String& DeconvolvedSpectrum::getActivationMethod() const
  {
    return activation_method_;
  }


  void DeconvolvedSpectrum::setPrecursor(const Precursor& precursor)
  {
    precursor_peak_ = precursor;
  }

  void DeconvolvedSpectrum::setPrecursorIntensity(const double i)
  {
    precursor_peak_.setIntensity(i);
  }

  void DeconvolvedSpectrum::setActivationMethod(const String& method)
  {
    activation_method_ = method;
  }

  void DeconvolvedSpectrum::setPrecursorPeakGroup(const PeakGroup& pg)
  {
    precursor_peak_group_ = pg;
  }

  void DeconvolvedSpectrum::setPrecursorScanNumber(const int scan_number)
  {
    precursor_scan_number_ = scan_number;
  }

  std::vector<PeakGroup>::const_iterator DeconvolvedSpectrum::begin() const noexcept
  {
    return peak_groups.begin();
  }
  std::vector<PeakGroup>::const_iterator DeconvolvedSpectrum::end() const noexcept
  {
    return peak_groups.end();
  }

  std::vector<PeakGroup>::iterator DeconvolvedSpectrum::begin() noexcept
  {
    return peak_groups.begin();
  }
  std::vector<PeakGroup>::iterator DeconvolvedSpectrum::end() noexcept
  {
    return peak_groups.end();
  }

  const PeakGroup& DeconvolvedSpectrum::operator[](const Size i) const
  {
    return peak_groups[i];
  }

  void DeconvolvedSpectrum::push_back(const PeakGroup& pg)
  {
    peak_groups.push_back(pg);
  }
  Size DeconvolvedSpectrum::size() const noexcept
  {
    return peak_groups.size();
  }
  void DeconvolvedSpectrum::clear()
  {
    peak_groups.clear();
  }
  void DeconvolvedSpectrum::reserve(Size n)
  {
    peak_groups.reserve(n);
  }
  bool DeconvolvedSpectrum::empty() const
  {
    return peak_groups.empty();
  }
  void DeconvolvedSpectrum::swap(std::vector<PeakGroup>& x)
  {
    peak_groups.swap(x);
  }

  void DeconvolvedSpectrum::sort()
  {
    std::sort(peak_groups.begin(), peak_groups.end());
  }

  void DeconvolvedSpectrum::sortByQScore()
  {
    std::sort(peak_groups.begin(), peak_groups.end(), [](const PeakGroup& p1, const PeakGroup& p2) { return p1.getQScore() > p2.getQScore(); });
  }

  void DeconvolvedSpectrum::updatePeakGroupQvalues(std::vector<DeconvolvedSpectrum>& deconvolved_spectra, std::vector<DeconvolvedSpectrum>& deconvolved_decoy_spectra) // per ms level + precursor update as well.
  {

    std::map<int, std::vector<float>> tscore_map; // per ms level

    std::map<int, std::vector<float>> dscore_iso_decoy_map;
    std::map<int, std::map<float, float>> qscore_iso_decoy_map; // maps for isotope decoy only qvalues

    std::map<int, std::vector<float>> dscore_noise_decoy_map;
    std::map<int, std::map<float, float>> qscore_noise_decoy_map; // maps for noise decoy only qvalues

    std::map<int, std::vector<float>> dscore_charge_decoy_map;
    std::map<int, std::map<float, float>> qscore_charge_decoy_map; // maps for charge decoy only qvalues

    for (auto& deconvolved_spectrum : deconvolved_spectra)
    {
      int ms_level = deconvolved_spectrum.getOriginalSpectrum().getMSLevel();
      if(tscore_map.find(ms_level) == tscore_map.end())
      {
        tscore_map[ms_level] = std::vector<float>();
        dscore_noise_decoy_map[ms_level] = std::vector<float>();
        qscore_noise_decoy_map[ms_level] = std::map<float, float>();
        dscore_iso_decoy_map[ms_level] = std::vector<float>();
        qscore_iso_decoy_map[ms_level] = std::map<float, float>();
        dscore_charge_decoy_map[ms_level] = std::vector<float>();
        qscore_charge_decoy_map[ms_level] = std::map<float, float>();
      }
      for (auto& pg : deconvolved_spectrum)
      {
        tscore_map[ms_level].push_back(pg.getQScore());
      }
    }
    for (auto& decoy_deconvolved_spectrum : deconvolved_decoy_spectra)
    {
      int ms_level = decoy_deconvolved_spectrum.getOriginalSpectrum().getMSLevel();
      for (auto& pg : decoy_deconvolved_spectrum)
      {
        //target (0), an isotope decoy (1), a noise (2), or a charge decoy (3)
        if(pg.getDecoyIndex() == 3)
        {
          dscore_iso_decoy_map[ms_level].push_back(pg.getQScore());
        }else if(pg.getDecoyIndex() == 2)
        {
          dscore_noise_decoy_map[ms_level].push_back(pg.getQScore());
        }else if(pg.getDecoyIndex() == 1)
        {
          dscore_charge_decoy_map[ms_level].push_back(pg.getQScore());
        }
      }
    }

    for(auto& titem: tscore_map)
    {
      int ms_level = titem.first;
      auto& tscore = titem.second;
      auto& dscore_iso = dscore_iso_decoy_map[ms_level];
      auto& dscore_charge = dscore_charge_decoy_map[ms_level];
      auto& dscore_noise = dscore_noise_decoy_map[ms_level];

      std::sort(tscore.begin(), tscore.end());
      std::sort(dscore_iso.begin(), dscore_iso.end());
      std::sort(dscore_noise.begin(), dscore_noise.end());
      std::sort(dscore_charge.begin(), dscore_charge.end());

      auto& map_charge = qscore_charge_decoy_map[ms_level];
      float tmp_q_charge = 1;
      for (int i = 0; i < tscore.size(); i++)
      {
        float ts = tscore[i];
        int dindex = std::distance(std::lower_bound(dscore_charge.begin(), dscore_charge.end(), ts), dscore_charge.end());
        int tindex = tscore.size() - i;

        tmp_q_charge = std::min(tmp_q_charge, ((float)dindex / (dindex + tindex)));
        map_charge[ts] = tmp_q_charge;
      }

      auto& map_iso = qscore_iso_decoy_map[ms_level];
      float tmp_q_iso = 1;
      for (int i = 0; i < tscore.size(); i++)
      {
        float ts = tscore[i];
        int dindex = std::distance(std::lower_bound(dscore_iso.begin(), dscore_iso.end(), ts), dscore_iso.end());
        int tindex = tscore.size() - i;

        tmp_q_iso = std::min(tmp_q_iso, ((float)dindex / (dindex + tindex) * (1 - map_charge[ts])));
        map_iso[ts] = tmp_q_iso;
      }

      auto& map_noise = qscore_noise_decoy_map[ms_level];
      float tmp_q_noise = 1;

      for (int i = 0; i < tscore.size(); i++)
      {
        float ts = tscore[i];
        int dindex = std::distance(std::lower_bound(dscore_noise.begin(), dscore_noise.end(), ts), dscore_noise.end());
        int tindex = tscore.size() - i;

        tmp_q_noise = std::min(tmp_q_noise, ((float)dindex / (dindex + tindex)));
        map_noise[ts] = tmp_q_noise;
      }
    }

    for(auto& titem: tscore_map)
    {
      int ms_level = titem.first;
      for (auto& deconvolved_spectrum : deconvolved_spectra)
      {
        if(deconvolved_spectrum.getOriginalSpectrum().getMSLevel() != ms_level)
        {
          continue;
        }
        auto& map_iso = qscore_iso_decoy_map[ms_level];
        auto& map_noise = qscore_noise_decoy_map[ms_level];
        auto& map_charge = qscore_charge_decoy_map[ms_level];

        for (auto& pg : deconvolved_spectrum)
        {
          //
          pg.setQvalueWithChargeDecoyOnly(map_charge[pg.getQScore()]);
          pg.setQvalueWithNoiseDecoyOnly(map_noise[pg.getQScore()]);
          pg.setQvalueWithIsotopeDecoyOnly(map_iso[pg.getQScore()]);

          pg.setQvalue(pg.getQvalueWithChargeDecoyOnly() + pg.getQvalueWithIsotopeDecoyOnly() + pg.getQvalueWithNoiseDecoyOnly());

          if(deconvolved_spectrum.getOriginalSpectrum().getMSLevel() > 1 && !deconvolved_spectrum.getPrecursorPeakGroup().empty())
          {
            double qs = deconvolved_spectrum.getPrecursorPeakGroup().getQScore();
            auto& pmap_iso = qscore_iso_decoy_map[ms_level - 1];
            auto& pmap_charge = qscore_charge_decoy_map[ms_level - 1];
            auto& pmap_noise = qscore_noise_decoy_map[ms_level - 1];
            deconvolved_spectrum.setPrecursorPeakGroupQvalue(pmap_iso[qs], pmap_noise[qs], pmap_charge[qs]);
          }
        }
      }
    }
  }

  void DeconvolvedSpectrum::setPrecursorPeakGroupQvalue(const double qvalue_with_iso_decoy_only , const double qvalue_with_noise_decoy_only, const double qvalue_with_charge_decoy_only)
  {
    precursor_peak_group_.setQvalue(qvalue_with_charge_decoy_only+ qvalue_with_iso_decoy_only+ qvalue_with_noise_decoy_only);
    precursor_peak_group_.setQvalueWithChargeDecoyOnly(qvalue_with_charge_decoy_only);
    precursor_peak_group_.setQvalueWithIsotopeDecoyOnly(qvalue_with_iso_decoy_only);
    precursor_peak_group_.setQvalueWithNoiseDecoyOnly(qvalue_with_noise_decoy_only);
  }
} // namespace OpenMS
