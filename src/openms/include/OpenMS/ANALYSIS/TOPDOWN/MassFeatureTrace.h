//--------------------------------------------------------------------------
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

#pragma once

#include <OpenMS/FILTERING/DATAREDUCTION/MassTraceDetection.h>
#include "boost/dynamic_bitset.hpp"
#include <iostream>
#include <iomanip>
#include <OpenMS/ANALYSIS/TOPDOWN/FLASHDeconvHelperStructs.h>
#include <OpenMS/ANALYSIS/TOPDOWN/DeconvolutedSpectrum.h>
#include <OpenMS/ANALYSIS/TOPDOWN/PeakGroup.h>
#include <OpenMS/DATASTRUCTURES/DefaultParamHandler.h>

namespace OpenMS
{
  /**
  @brief Feature trace in mass dimension for FLASHDeconv
  @ingroup Topdown
  */
  class OPENMS_DLLAPI MassFeatureTrace :
      public DefaultParamHandler
  {
  public:
    typedef FLASHDeconvHelperStructs::PrecalculatedAveragine PrecalculatedAveragine;
    typedef FLASHDeconvHelperStructs::LogMzPeak LogMzPeak;

    /// constructor
    MassFeatureTrace();

    /// default destructor
    ~MassFeatureTrace() override;

    /// copy constructor
    MassFeatureTrace(const MassFeatureTrace& ) = default;

    /// move constructor
    MassFeatureTrace(MassFeatureTrace&& other) = default;

    /// assignment operator
    MassFeatureTrace& operator=(const MassFeatureTrace& fd) = default;

    /// Obtain information from deconvoluted_spectrum per (necessary information for mass tracing afterwards)
    void storeInformationFromDeconvolutedSpectrum(DeconvolutedSpectrum& deconvoluted_spectrum);

    /**
       @brief Find features and write features in output files. Probably seprate the writing part soon.
       @param file_name input spectrum file name
       @param promex_out whether promex format output should be generated
       @param feature_cntr total number of features, updated in this function
       @param feature_index index to features, updated in this function
       @param fsf file stream for regular output
       @param fsp file stream for promex output
       @param averagine precalculated averagine for cosine calculation
       */
    void findFeatures(const String& file_name,
                      const bool promex_out,
                      int& feature_cntr,
                      int& feature_index,
                      std::fstream& fsf,
                      std::fstream& fsp,
                      const PrecalculatedAveragine& averagine);

    /// write header line for regular file output
    static void writeHeader(std::fstream& fs);

    /// write header lien for promex file output
    static void writePromexHeader(std::fstream& fs);

  protected:
    void updateMembers_() override;

  private:
    /// MS1 tolerance
    //double tol_;
    /// cosine thresholds for scoring and filtering
    double min_isotope_cosine_;
    /// peak group information is stored in here for traicing
    std::unordered_map<double, std::unordered_map<double, PeakGroup>> peak_group_map_; // rt , mono mass, peakgroup
  };
}
