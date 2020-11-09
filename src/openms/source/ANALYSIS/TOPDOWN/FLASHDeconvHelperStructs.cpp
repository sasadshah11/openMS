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

#include <OpenMS/ANALYSIS/TOPDOWN/FLASHDeconvHelperStructs.h>

namespace OpenMS
{
  FLASHDeconvHelperStructs::PrecalculatedAveragine::PrecalculatedAveragine()
  {
  }

  FLASHDeconvHelperStructs::PrecalculatedAveragine::PrecalculatedAveragine(double m,
                                                                           double M,
                                                                           double delta,
                                                                           CoarseIsotopePatternGenerator *generator,
                                                                           bool useRNAavg)
      :
      massInterval(delta), minMass(m)
  {
    int i = 0;
    while (true)
    {
      double a = i * massInterval;
      i++;
      if (a < m)
      {
        continue;
      }
      if (a > M)
      {
        break;
      }
      auto iso = useRNAavg ? generator->estimateFromRNAWeight(a) : generator->estimateFromPeptideWeight(a);
      auto factor = .01;
      iso.trimRight(factor * iso.getMostAbundant().getIntensity());

      double norm = .0;
      Size mostAbundantIndex = 0;
      double mostAbundantInt = 0;

      for (Size k = 0; k < iso.size(); k++)
      {
        norm += iso[k].getIntensity() * iso[k].getIntensity();
        if (mostAbundantInt >= iso[k].getIntensity())
        {
          continue;
        }
        mostAbundantInt = iso[k].getIntensity();
        mostAbundantIndex = k;
      }

      Size leftIndex = mostAbundantIndex;
      for (Size k = 0; k <= mostAbundantIndex; k++)
      {
        if (iso[k].getIntensity() > mostAbundantInt * factor)
        {
          break;
        }
        norm -= iso[k].getIntensity() * iso[k].getIntensity();
        leftIndex--;
        iso[k].setIntensity(0);
      }

      Size rightIndex = iso.size() - 1 - mostAbundantIndex;
      for (Size k = iso.size() - 1; k >= mostAbundantIndex; k--)
      {
        if (iso[k].getIntensity() > mostAbundantInt * factor)
        {
          break;
        }
        norm -= iso[k].getIntensity() * iso[k].getIntensity();
        rightIndex--;
        iso[k].setIntensity(0);
      }

      rightIndices.push_back(rightIndex);
      leftIndices.push_back(leftIndex);
      averageMassDelta.push_back(iso.averageMass() - iso[0].getMZ());
      norms.push_back(norm);
      isotopes.push_back(iso);
    }
  }

  IsotopeDistribution FLASHDeconvHelperStructs::PrecalculatedAveragine::get(double mass)
  {
    Size i = (Size) (.5 + (mass - minMass) / massInterval);
    i = i >= isotopes.size() ? isotopes.size() - 1 : i;
    return isotopes[i];
  }

  double FLASHDeconvHelperStructs::PrecalculatedAveragine::getNorm(double mass)
  {
    Size i = (Size) (.5 + (mass - minMass) / massInterval);
    i = i >= isotopes.size() ? isotopes.size() - 1 : i;
    return norms[i];
  }

  Size FLASHDeconvHelperStructs::PrecalculatedAveragine::getLeftIndex(double mass)
  {
    Size i = (Size) (.5 + (mass - minMass) / massInterval);
    i = i >= isotopes.size() ? isotopes.size() - 1 : i;
    return leftIndices[i];
  }

  double FLASHDeconvHelperStructs::PrecalculatedAveragine::getAverageMassDelta(double mass)
  {
    Size i = (Size) (.5 + (mass - minMass) / massInterval);
    i = i >= isotopes.size() ? isotopes.size() - 1 : i;
    return averageMassDelta[i];
  }

  Size FLASHDeconvHelperStructs::PrecalculatedAveragine::getRightIndex(double mass)
  {
    Size i = (Size) (.5 + (mass - minMass) / massInterval);
    i = i >= isotopes.size() ? isotopes.size() - 1 : i;
    return rightIndices[i];
  }

  FLASHDeconvHelperStructs::LogMzPeak::LogMzPeak() :
      mz(0),
      intensity(0),
      logMz(-1000),
      charge(0),
      isotopeIndex(0)
  {
  }

  FLASHDeconvHelperStructs::LogMzPeak::LogMzPeak(Peak1D &peak, bool positive) :
      mz(peak.getMZ()),
      intensity(peak.getIntensity()),
      logMz(getLogMz(peak.getMZ(), positive)),
      charge(0),
      isotopeIndex(0)
  {
  }

  FLASHDeconvHelperStructs::LogMzPeak::LogMzPeak(double mz, bool positive) :
      mz(mz), logMz(getLogMz(mz, positive))
  {
  }

  FLASHDeconvHelperStructs::LogMzPeak::LogMzPeak(LogMzPeak &peak, int c, int i) :
      mz(peak.mz),
      intensity(peak.intensity),
      logMz(peak.logMz),
      charge(c),
      isotopeIndex(i)
  //index(peak.index)
  {
  }

  FLASHDeconvHelperStructs::LogMzPeak::~LogMzPeak()
  {
  }


  double FLASHDeconvHelperStructs::LogMzPeak::getUnchargedMass()
  {
    if (mass <= 0)
    {
      mass = (mz - getChargeMass(charge > 0)) * abs(charge);
    }
    return mass;
  }

  bool FLASHDeconvHelperStructs::LogMzPeak::operator<(const LogMzPeak &a) const
  {
    return this->logMz < a.logMz;
  }

  bool FLASHDeconvHelperStructs::LogMzPeak::operator>(const LogMzPeak &a) const
  {
    return this->logMz > a.logMz;
  }

  bool FLASHDeconvHelperStructs::LogMzPeak::operator==(const LogMzPeak &a) const
  {
    return this->logMz == a.logMz;
  }

  class LogMzPeakHashFunction
  {
  public:

    size_t operator()(const FLASHDeconvHelperStructs::LogMzPeak &p) const
    {
      return std::hash<double>()(p.mz);
    }
  };

  FLASHDeconvHelperStructs::PrecalculatedAveragine FLASHDeconvHelperStructs::calculateAveragines(double maxMass,
                                                                                                 bool useRNAavg)
  {
    auto generator = new CoarseIsotopePatternGenerator();

    auto maxIso = useRNAavg ?
                  generator->estimateFromRNAWeight(maxMass) :
                  generator->estimateFromPeptideWeight(maxMass);
    maxIso.trimRight(0.01 * maxIso.getMostAbundant().getIntensity());

    generator->setMaxIsotope(maxIso.size());
    auto avg = FLASHDeconvHelperStructs::PrecalculatedAveragine(50, maxMass, 20, generator, useRNAavg);
    avg.maxIsotopeCount = maxIso.size() - 1;
    return avg;
  }

  double FLASHDeconvHelperStructs::getChargeMass(bool positive)
  {
    return (positive ? Constants::PROTON_MASS_U : Constants::ELECTRON_MASS_U);
  }


  double FLASHDeconvHelperStructs::getLogMz(double mz, bool positive)
  {
    return std::log(mz - getChargeMass(positive));
  }

}
