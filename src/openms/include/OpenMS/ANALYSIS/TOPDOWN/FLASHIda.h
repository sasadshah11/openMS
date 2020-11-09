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
// $Maintainer: Kyowon Jeong $
// $Authors: Kyowon Jeong $
// --------------------------------------------------------------------------

#pragma once

#include <OpenMS/ANALYSIS/TOPDOWN/PeakGroup.h>
#include <OpenMS/ANALYSIS/TOPDOWN/FLASHDeconvHelperStructs.h>

namespace OpenMS
{
  // class for real time flashdeconv..
  class OPENMS_DLLAPI FLASHIda
  {
  public:
    typedef FLASHDeconvHelperStructs::PrecalculatedAveragine PrecalculatedAveragine;
    typedef FLASHDeconvHelperStructs::LogMzPeak LogMzPeak;

    FLASHIda(char *arg);

    ~FLASHIda();

    int getPeakGroups(double *mzs,
                      double *ints,
                      int length,
                      double rt,
                      int msLevel,
                      char *name,
                      double qScoreThreshold);

    void getIsolationWindows(double *wstart, double *wend, double *qScores, int *charges, double *avgMasses);

  protected:
    std::map<int, std::vector<double>> selected; // int mass, rt, qscore
    PrecalculatedAveragine avg;

    std::vector<std::vector<Size>> prevMassBinVector;
    std::vector<double> prevMinBinLogMassVector;
    std::vector<PeakGroup> peakGroups;

    void filterPeakGroupsUsingMassExclusion(MSSpectrum &spec, int msLevel,
                                            double qScoreThreshold);

    MSSpectrum makeMSSpectrum(double *mzs, double *ints, int length, double rt, int msLevel, char *name);

    void deepClearSpectrum(MSSpectrum &spec);

  private:
    double qScoreThreshold;
    int minCharge;
    int currentChargeRange;
    int chargeRange;
    double minMass;
    double currentMaxMass;
    double maxMass;
    DoubleList tolerance, binWidth;
    double RTwindow;
    IntList maxMassCount;
    int numOverlappedScans;
    // all information to keep track of
    // parameter
    // averagine results
    // exclusion list mass
    // refer to the deconv'd spectrum. two exclusion durations
    // https://stackoverflow.com/questions/31417688/passing-a-vector-array-from-unmanaged-c-to-c-sharp

  };
}
