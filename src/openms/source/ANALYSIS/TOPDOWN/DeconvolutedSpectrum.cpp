// --------------------------------------------------------------------------
//                   OpenMS -- Open-Source Mass Spectrometry
// --------------------------------------------------------------------------
// Copyright The OpenMS Team -- Eberhard Karls University Tuebingen,
// ETH Zurich, and Freie Universitaet Berlin 2002-2018.
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

#include "include/OpenMS/ANALYSIS/TOPDOWN/DeconvolutedSpectrum.h"
#include <OpenMS/ANALYSIS/TOPDOWN/FLASHDeconvAlgorithm.h>

namespace OpenMS
{
  DeconvolutedSpectrum::DeconvolutedSpectrum(const MSSpectrum &s, int n) :
      scanNumber(n)
  {
    spec = s;
  }

  bool DeconvolutedSpectrum::empty() const
  {
    return peakGroups.empty();
  }

  MSSpectrum DeconvolutedSpectrum::toSpectrum()
  {
    auto outSpec = MSSpectrum(spec);
    outSpec.clear(false);
    for (auto &pg : peakGroups)
    {
      if (pg.empty())
      {
        continue;
      }
      outSpec.emplace_back(pg.monoisotopicMass, pg.intensity);
    }
    if (precursorPeakGroup != nullptr && !spec.getPrecursors().empty())
    {
      Precursor precursor(spec.getPrecursors()[0]);
      precursor.setCharge(precursorPeakGroup->maxQScoreCharge);
      precursor.setMZ(precursorPeakGroup->monoisotopicMass);
      precursor.setIntensity(precursorPeakGroup->intensity);

      outSpec.getPrecursors().emplace_back(precursor);
    }
    return outSpec;
  }

  void DeconvolutedSpectrum::writeDeconvolutedMasses(std::fstream &fs,
                                                     const String &fileName,
                                                     bool writeDetail)//, fstream &fsm, fstream &fsp)
  {
    if (empty())
    {
      return;
    }

    for (auto &pg : peakGroups)
    {
      if (pg.empty())
      {
        continue;
      }
      const double &m = pg.monoisotopicMass;
      const double &am = pg.avgMass;
      const double &intensity = pg.intensity;

      fs << pg.massIndex << "\t" << pg.specIndex << "\t" << fileName << "\t" << pg.scanNumber << "\t"
         << std::to_string(spec.getRT()) << "\t"
         << peakGroups.size() << "\t"
         << std::to_string(am) << "\t" << std::to_string(m) << "\t" << intensity << "\t"
         << pg.minCharge << "\t" << pg.maxCharge << "\t"
         << pg.size() << "\t";

      if (writeDetail)
      {
        fs << std::fixed << std::setprecision(2);
        for (auto &p : pg)
        {
          fs << p.mz << ";";
        }
        fs << "\t";

        fs << std::fixed << std::setprecision(1);
        for (auto &p : pg)
        {
          fs << p.intensity << ";";
        }
        fs << "\t";
        fs << std::setprecision(-1);

        for (auto &p : pg)
        {
          fs << p.charge << ";";
        }
        fs << "\t";
        for (auto &p : pg)
        {
          fs << p.getUnchargedMass() << ";";
        }
        fs << "\t";
        for (auto &p : pg)
        {
          fs << p.isotopeIndex << ";";
        }
        fs << "\t";

        for (auto &p : pg)
        {
          auto tm = pg.monoisotopicMass + p.isotopeIndex * Constants::ISOTOPE_MASSDIFF_55K_U;
          auto diff = (tm / abs(p.charge) + FLASHDeconvHelperStructs::getChargeMass(p.charge > 0) - p.mz) / p.mz;
          fs << 1e6 * diff << ";";
        }
        fs << "\t";
      }
      if (spec.getMSLevel() > 1)
      {
        //PrecursorScanNum	PrecursorMz	PrecursorIntensity PrecursorCharge	PrecursorMonoMass		PrecursorQScore
        fs << precursorScanNumber << "\t" << std::to_string(precursorPeak.getMZ()) << "\t"
           << precursorPeak.getIntensity() << "\t"
           << precursorPeak.getCharge()
           << "\t";

        if (precursorPeakGroup == nullptr)
        {
          fs << "N/A\tN/A\t";
        }
        else
        {
          fs << std::to_string(precursorPeakGroup->monoisotopicMass) << "\t"
             << precursorPeakGroup->qScore << "\t";
        }
      }
      fs << pg.isotopeCosineScore << "\t";

      fs << pg.totalSNR << "\t"
         << pg.maxQScoreCharge << "\t" << std::to_string(pg.maxQScoreMzStart) << "\t"
         << std::to_string(pg.maxQScoreMzEnd) << "\t"
         << pg.qScore << "\t" << std::setprecision(-1); //

      for (int i = pg.minCharge; i <= pg.maxCharge; i++)
      {
        if (pg.perChargeInfo.find(i) == pg.perChargeInfo.end())
        {
          fs << 0;
        }
        else
        {
          fs << pg.perChargeInfo[i][2];
        }
        if (i < pg.maxCharge)
        {
          fs << ";";
        }
      }
      fs << "\t";
      int isoEndIndex = 0;

      for (auto &p : pg)
      {
        isoEndIndex = isoEndIndex < p.isotopeIndex ? p.isotopeIndex : isoEndIndex;
      }
      auto perIsotopeIntensity = new double[isoEndIndex + 1];
      std::fill_n(perIsotopeIntensity, isoEndIndex + 1, .0);
      for (auto &p : pg)
      {
        perIsotopeIntensity[p.isotopeIndex] += p.intensity;
      }

      for (int i = 0; i <= isoEndIndex; i++)
      {
        fs << perIsotopeIntensity[i];
        if (i < isoEndIndex)
        {
          fs << ";";
        }
      }

      fs << "\n";
    }
  }


  void DeconvolutedSpectrum::writeDeconvolutedMassesHeader(std::fstream &fs, int &n, bool detail)
  {
    if (detail)
    {
      if (n == 1)
      {
        fs
            << "MassIndex\tSpecIndex\tFileName\tScanNum\tRetentionTime\tMassCountInSpec\tAverageMass\tMonoisotopicMass\t"
               "SumIntensity\tMinCharge\tMaxCharge\t"
               "PeakCount\tPeakMZs\tPeakIntensities\tPeakCharges\tPeakMasses\tPeakIsotopeIndices\tPeakPPMErrors\t"
               "IsotopeCosine\tMassSNR\tMaxQScoreCharge\tMaxQScoreMzStart\tMaxQScoreMzEnd\tQScore\tPerChargeIntensity\tPerIsotopeIntensity\n";
      }
      else
      {
        fs
            << "MassIndex\tSpecIndex\tFileName\tScanNum\tRetentionTime\tMassCountInSpec\tAverageMass\tMonoisotopicMass\t"
               "SumIntensity\tMinCharge\tMaxCharge\t"
               "PeakCount\tPeakMZs\tPeakIntensities\tPeakCharges\tPeakMasses\tPeakIsotopeIndices\tPeakPPMErrors\t"
               "PrecursorScanNum\tPrecursorMz\tPrecursorIntensity\tPrecursorCharge\tPrecursorMonoisotopicMass\tPrecursorQScore\t"
               "IsotopeCosine\tMassSNR\tMaxQScoreCharge\tMaxQScoreMzStart\tMaxQScoreMzEnd\tQScore\tPerChargeIntensity\tPerIsotopeIntensity\n";
      }
    }
    else
    {
      if (n == 1)
      {
        fs
            << "MassIndex\tSpecIndex\tFileName\tScanNum\tRetentionTime\tMassCountInSpec\tAverageMass\tMonoisotopicMass\t"
               "SumIntensity\tMinCharge\tMaxCharge\t"
               "PeakCount\t"
               //"PeakMZs\tPeakCharges\tPeakMasses\tPeakIsotopeIndices\tPeakPPMErrors\tPeakIntensities\t"
               "IsotopeCosine\tMassSNR\tMaxQScoreCharge\tMaxQScoreMzStart\tMaxQScoreMzEnd\tQScore\tPerChargeIntensity\tPerIsotopeIntensity\n";

      }
      else
      {
        fs
            << "MassIndex\tSpecIndex\tFileName\tScanNum\tRetentionTime\tMassCountInSpec\tAverageMass\tMonoisotopicMass\t"
               "SumIntensity\tMinCharge\tMaxCharge\t"
               "PeakCount\t"
               "PrecursorScanNum\tPrecursorMz\tPrecursorIntensity\tPrecursorCharge\tPrecursorMonoisotopicMass\tPrecursorQScore\t"
               "IsotopeCosine\tMassSNR\tMaxQScoreCharge\tMaxQScoreMzStart\tMaxQScoreMzEnd\tQScore\tPerChargeIntensity\tPerIsotopeIntensity\n";
      }
    }
  }

  /*  void DeconvolutedSpectrum::writeAttCsvHeader(std::fstream &fs)
    {
      fs
          << "ScanNumber,RetentionTime,PrecursorScanNumber,Charge,ChargeSNR,PeakIntensity,EnvIntensity,EnvIsotopeCosine,PeakMz,"
             "MonoMass,MassSNR,IsotopeCosine,MassIntensity,QScore,Class\n";
    }*/

  void DeconvolutedSpectrum::writeThermoInclusionHeader(std::fstream &fs)
  {
    fs << "Compound,Formula,Adduct,m/z,z,t start (min),t stop (min),Isolation Window (m/z),Normalized AGC Target (%)\n";
  }


  void DeconvolutedSpectrum::clearPeakGroupsChargeInfo()
  {
    for (auto &pg: peakGroups)
    {
      pg.clearChargeInfo();
    }
  }

  void DeconvolutedSpectrum::writeTopFD(std::fstream &fs, int id)//, fstream &fsm, fstream &fsp)
  {
    auto msLevel = spec.getMSLevel();

    fs << std::fixed << std::setprecision(2);
    fs << "BEGIN IONS\n"
       << "ID=" << id << "\n"
       << "SCANS=" << scanNumber << "\n"
       << "RETENTION_TIME=" << spec.getRT() << "\n";

    fs << "ACTIVATION=" << activationMethod << "\n";

    if (msLevel > 1)
    {
      if (precursorPeakGroup != nullptr)
      {
        fs << "MS_ONE_ID=" << precursorPeakGroup->specIndex << "\n"
           << "MS_ONE_SCAN=" << precursorPeakGroup->scanNumber << "\n"
           << "PRECURSOR_MZ="
           << std::to_string(precursorPeak.getMZ()) << "\n"
           << "PRECURSOR_CHARGE=" << precursorPeak.getCharge() << "\n"
           << "PRECURSOR_MASS=" << std::to_string(precursorPeakGroup->monoisotopicMass) << "\n"
           << "PRECURSOR_INTENSITY=" << precursorPeak.getIntensity() << "\n";
      }
      else
      {
        fs << "MS_ONE_ID=" << 0 << "\n"
            << "MS_ONE_SCAN=" << 0 << "\n"
            << "PRECURSOR_MZ="
            << std::to_string(precursorPeak.getMZ()) << "\n"
            << "PRECURSOR_CHARGE=" << precursorPeak.getCharge() << "\n"
            << "PRECURSOR_MASS=" << std::to_string(precursorPeak.getMZ() * precursorPeak.getCharge()) << "\n"
            << "PRECURSOR_INTENSITY=" << precursorPeak.getIntensity() << "\n";
      }
    }
    fs << std::setprecision(-1);

    double scoreThreshold = 0;
    std::vector<double> scores;

    if (peakGroups.size() > 500)// max peak count for TopPic
    {
      scores.reserve(peakGroups.size());
      for (auto &pg : peakGroups)
      {
        scores.push_back(pg.isotopeCosineScore);
      }
      std::sort(scores.begin(), scores.end());
      scoreThreshold = scores[scores.size() - 500];
      std::vector<double>().swap(scores);
    }

    int size = 0;
    for (auto &pg : peakGroups)
    {
      if (pg.isotopeCosineScore < scoreThreshold)
      {
        continue;
      }
      if (size >= 500)
      {
        break;
      }

      size++;
      fs << std::fixed << std::setprecision(2);
      fs << std::to_string(pg.monoisotopicMass) << "\t" << pg.intensity << "\t" << pg.maxQScoreCharge
                                                                                //  << "\t" << log10(pg.precursorSNR+1e-10) << "\t" << log10(pg.precursorTotalSNR+1e-10)
                                                                                //  << "\t" << log10(pg.maxSNR + 1e-10) << "\t" << log10(pg.totalSNR + 1e-10)
                                                                                << "\n";
      fs << std::setprecision(-1);
    }

    fs << "END IONS\n\n";
  }

  bool DeconvolutedSpectrum::registerPrecursor(DeconvolutedSpectrum &precursorSpectrum)
  {
    //precursorSpectrum.updatePeakGroupMap();
    for (auto &p: spec.getPrecursors())
    {
      for (auto &act :  p.getActivationMethods())
      {
        activationMethod = Precursor::NamesOfActivationMethodShort[act];
        break;
      }
      precursorPeak = p;
      precursorScanNumber = precursorSpectrum.scanNumber;
      auto startMz = p.getIsolationWindowLowerOffset() > 100.0 ?
                     p.getIsolationWindowLowerOffset() :
                     -p.getIsolationWindowLowerOffset() + p.getMZ();
      auto endMz = p.getIsolationWindowUpperOffset() > 100.0 ?
                   p.getIsolationWindowUpperOffset() :
                   p.getIsolationWindowUpperOffset() + p.getMZ();

      double maxSumIntensity = 0.0;
      for (auto &pg: (precursorSpectrum.peakGroups))
      {
        if (pg[0].mz > endMz || pg[pg.size() - 1].mz < startMz)
        {
          continue;
        }

        double sumIntensity = .0;
        double maxIntensity = .0;
        LogMzPeak *tmp = nullptr;
        for (auto &pt:pg)
        {
          if (pt.mz < startMz)
          {
            continue;
          }
          if (pt.mz > endMz)
          {
            break;
          }
          sumIntensity += pt.intensity;

          if (pt.intensity < maxIntensity)
          {
            continue;
          }
          maxIntensity = pt.intensity;
          precursorPeak.setCharge(pt.charge);
          tmp = &pt;
        }

        if (sumIntensity <= maxSumIntensity || tmp == nullptr)
        {
          continue;
        }

        maxSumIntensity = sumIntensity;
        precursorPeakGroup = &pg;
      }
    }

    return precursorPeakGroup != nullptr;
  }

  void DeconvolutedSpectrum::setPeakGroups(std::vector<PeakGroup> &pg)
  {
    peakGroups = pg;
  }

  MSSpectrum &DeconvolutedSpectrum::getOriginalSpectrum()
  {
    return spec;
  }

  std::__wrap_iter<std::vector<PeakGroup, std::allocator<PeakGroup>>::pointer> DeconvolutedSpectrum::begin()
  {
    return peakGroups.begin();
  }

  std::__wrap_iter<std::vector<PeakGroup, std::allocator<PeakGroup>>::pointer> DeconvolutedSpectrum::end()
  {
    return peakGroups.end();
  }

  PeakGroup DeconvolutedSpectrum::getPrecursorPeakGroup()
  {
    if (precursorPeakGroup == nullptr)
    {
      return PeakGroup();
    }
    return *precursorPeakGroup;
  }

  int DeconvolutedSpectrum::getPrecursorCharge()
  {
    return precursorPeak.getCharge();
  }

  int DeconvolutedSpectrum::size()
  {
    return peakGroups.size();
  }

  double DeconvolutedSpectrum::getCurrentMaxMass(double maxMass)
  {
    if (spec.getMSLevel() == 1 || precursorPeakGroup == nullptr || precursorPeakGroup->empty())
    {
      return maxMass;
    }
    return precursorPeakGroup->monoisotopicMass;
  }

  int DeconvolutedSpectrum::getCurrentMaxCharge(int maxCharge)
  {
    if (spec.getMSLevel() == 1 || precursorPeakGroup == nullptr || precursorPeakGroup->empty())
    {
      return maxCharge;
    }
    return precursorPeak.getCharge();
  }
}