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
// $Maintainer: Chris Bielow $
// $Authors: Svetlana Kutuzova, Douglas McCloskey $
// --------------------------------------------------------------------------
 
#pragma once

#include <OpenMS/CONCEPT/ClassTest.h>
#include <OpenMS/FORMAT/FeatureXMLFile.h>
#include <OpenMS/TRANSFORMATIONS/FEATUREFINDER/PeakWidthEstimator.h>
#include <OpenMS/TRANSFORMATIONS/RAW2PEAK/PeakPickerCWT.h>
#include <OpenMS/ANALYSIS/OPENSWATH/TargetedSpectraExtractor.h>
#include <OpenMS/ANALYSIS/ID/AccurateMassSearchEngine.h>
#include <OpenMS/FORMAT/MzTabFile.h>
#include <OpenMS/FORMAT/MzTab.h>
#include <OpenMS/FILTERING/NOISEESTIMATION/SignalToNoiseEstimatorMedianRapid.h>
#include <OpenMS/FILTERING/BASELINE/MorphologicalFilter.h>
#include <OpenMS/KERNEL/SpectrumHelper.h>
#include <OpenMS/ANALYSIS/OPENSWATH/SpectrumAddition.h>
#include <OpenMS/FILTERING/SMOOTHING/SavitzkyGolayFilter.h>
#include <OpenMS/FORMAT/MzMLFile.h>

namespace OpenMS
{
  /**
    description
  */
  class OPENMS_DLLAPI FIAMSDataProcessor  :
    public DefaultParamHandler
  {
public:
    /// Constructor
    FIAMSDataProcessor();

    /// Default desctructor
    ~FIAMSDataProcessor() = default;

    /// Copy constructor
    FIAMSDataProcessor(const FIAMSDataProcessor& cp);

    /// Assignment
    FIAMSDataProcessor& operator=(const FIAMSDataProcessor& fdp);

    /// Process the given file
    void run(const MSExperiment & experiment, const float & n_seconds, OpenMS::MzTab & output);

    /// Cut the spectra for time
    void cutForTime(const MSExperiment & experiment, std::vector<MSSpectrum> & output, const float & n_seconds=6000);

    /// Merge spectra from different retention times into one
    MSSpectrum mergeAlongTime(const std::vector<OpenMS::MSSpectrum> & input);

    /// Pick peaks from merged spectrum and return as featureMap with the corresponding polarity
    MSSpectrum extractPeaks(const MSSpectrum & input);

    /// Convert a spectrum to a feature map with the corresponding polarity
    FeatureMap convertToFeatureMap(const MSSpectrum & input);

    /// Estimate noise for each peak
    MSSpectrum trackNoise(const MSSpectrum & input);

    /// Perform accurate mass search
    void runAccurateMassSearch(FeatureMap & input, OpenMS::MzTab & output);

    /// Get mass-to-charge ratios to base the sliding window upon
    const std::vector<float> getMZs();

    /// Get the sliding bin sizes
    const std::vector<float> getBinSizes();

protected:
    void updateMembers_() override;

private:
    void storeSpectrum_(const MSSpectrum & input, String filename);

    std::vector<float> mzs_; 
    std::vector<float> bin_sizes_;
    SavitzkyGolayFilter sgfilter_;
    MorphologicalFilter morph_filter_;
    PeakPickerHiRes picker_;
  };
} // namespace OpenMS

