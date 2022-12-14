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
// $Maintainer: Eugen Netz $
// $Authors: Eugen Netz $
// --------------------------------------------------------------------------

#include <OpenMS/ANALYSIS/XLMS/OPXLSpectrumProcessingAlgorithms.h>

#include <OpenMS/CONCEPT/LogStream.h>

// preprocessing and filtering
#include <OpenMS/FILTERING/TRANSFORMERS/ThresholdMower.h>
#include <OpenMS/FILTERING/TRANSFORMERS/Normalizer.h>
#include <OpenMS/FILTERING/TRANSFORMERS/NLargest.h>

using namespace std;

namespace OpenMS
{

  PeakSpectrum OPXLSpectrumProcessingAlgorithms::mergeAnnotatedSpectra(PeakSpectrum & first_spectrum, PeakSpectrum & second_spectrum)
  {
    // merge peaks: create new spectrum, insert peaks from first and then from second spectrum
    PeakSpectrum resulting_spectrum;
    resulting_spectrum.insert(resulting_spectrum.end(), first_spectrum.begin(), first_spectrum.end());
    resulting_spectrum.insert(resulting_spectrum.end(), second_spectrum.begin(), second_spectrum.end());

    // merge DataArrays in a similar way
    for (Size i = 0; i < first_spectrum.getFloatDataArrays().size(); i++)
    {
      // TODO instead of this "if", get second array by name if available.  would not be dependent on order.
      if (second_spectrum.getFloatDataArrays().size() > i)
      {
        PeakSpectrum::FloatDataArray float_array;
        float_array.insert(float_array.end(), first_spectrum.getFloatDataArrays()[i].begin(), first_spectrum.getFloatDataArrays()[i].end());
        float_array.insert(float_array.end(), second_spectrum.getFloatDataArrays()[i].begin(), second_spectrum.getFloatDataArrays()[i].end());
        resulting_spectrum.getFloatDataArrays().push_back(float_array);
        resulting_spectrum.getFloatDataArrays()[i].setName(first_spectrum.getFloatDataArrays()[i].getName());
      }
    }

    for (Size i = 0; i < first_spectrum.getStringDataArrays().size(); i++)
    {
      if (second_spectrum.getStringDataArrays().size() > i)
      {
        PeakSpectrum::StringDataArray string_array;
        string_array.insert(string_array.end(), first_spectrum.getStringDataArrays()[i].begin(), first_spectrum.getStringDataArrays()[i].end());
        string_array.insert(string_array.end(), second_spectrum.getStringDataArrays()[i].begin(), second_spectrum.getStringDataArrays()[i].end());
        resulting_spectrum.getStringDataArrays().push_back(string_array);
        resulting_spectrum.getStringDataArrays()[i].setName(first_spectrum.getStringDataArrays()[i].getName());
      }
    }

    for (Size i = 0; i < first_spectrum.getIntegerDataArrays().size(); i++)
    {
      if (second_spectrum.getIntegerDataArrays().size() > i)
      {
        PeakSpectrum::IntegerDataArray integer_array;
        integer_array.insert(integer_array.end(), first_spectrum.getIntegerDataArrays()[i].begin(), first_spectrum.getIntegerDataArrays()[i].end());
        integer_array.insert(integer_array.end(), second_spectrum.getIntegerDataArrays()[i].begin(), second_spectrum.getIntegerDataArrays()[i].end());
        resulting_spectrum.getIntegerDataArrays().push_back(integer_array);
        resulting_spectrum.getIntegerDataArrays()[i].setName(first_spectrum.getIntegerDataArrays()[i].getName());
      }
    }

    // Spectra were simply concatenated, so they are not sorted by position anymore
    resulting_spectrum.sortByPosition();
    return resulting_spectrum;
  }

  PeakMap OPXLSpectrumProcessingAlgorithms::preprocessSpectra(PeakMap& exp, double fragment_mass_tolerance, bool fragment_mass_tolerance_unit_ppm, Size peptide_min_size, Int min_precursor_charge, Int max_precursor_charge, bool deisotope, bool labeled)
  {
    // filter MS2 map
    // remove 0 intensities
    ThresholdMower threshold_mower_filter;
    threshold_mower_filter.filterPeakMap(exp);

    Normalizer normalizer;
    normalizer.filterPeakMap(exp);

    // sort by rt
    exp.sortSpectra(false);
    LOG_DEBUG << "Deisotoping and filtering spectra." << endl;

    PeakMap filtered_spectra;

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (SignedSize exp_index = 0; exp_index < static_cast<SignedSize>(exp.size()); ++exp_index)
    {
      vector<Precursor> precursor = exp[exp_index].getPrecursors();
      // for labeled experiments, the pairs of heavy and light spectra are linked by spectra indices from the consensusXML, so the returned number of spectra has to be equal to the input
      bool process_this_spectrum = labeled;
      if (precursor.size() == 1 && exp[exp_index].size() >= peptide_min_size * 2)
      {
        int precursor_charge = precursor[0].getCharge();
        if (precursor_charge >= min_precursor_charge && precursor_charge <= max_precursor_charge)
        {
          process_this_spectrum = true;
        }
      }

      if (!process_this_spectrum)
      {
        continue;
      }
      exp[exp_index].sortByPosition();

      if (deisotope)
      {
        PeakSpectrum deisotoped = OPXLSpectrumProcessingAlgorithms::deisotopeAndSingleChargeMSSpectrum(exp[exp_index], 1, 7, fragment_mass_tolerance, fragment_mass_tolerance_unit_ppm, false, 3, 10, false);

        // only consider spectra, that have at least as many peaks as two times the minimal peptide size after deisotoping
        if (deisotoped.size() > peptide_min_size * 2 || labeled)
        {
          deisotoped.sortByPosition();

#ifdef _OPENMP
#pragma omp critical
#endif
          {
            filtered_spectra.addSpectrum(deisotoped);
          }
        }
      }
      else
      {
        PeakSpectrum filtered = exp[exp_index];
        if (!labeled) // this kind of filtering is not necessary for labeled cross-links, since they area filtered by comparing heavy and light spectra later
        {
          NLargest nfilter(500);
          nfilter.filterSpectrum(filtered);
        }

        // only consider spectra, that have at least as many peaks as two times the minimal peptide size after filtering
        if (filtered.size() > peptide_min_size * 2 || labeled)
        {
          filtered.sortByPosition();

#ifdef _OPENMP
#pragma omp critical
#endif
          {
            filtered_spectra.addSpectrum(filtered);
          }
        }
      }
    }
    return filtered_spectra;
  }

  void OPXLSpectrumProcessingAlgorithms::getSpectrumAlignmentFastCharge(
    std::vector<std::pair<Size, Size> > & alignment, double fragment_mass_tolerance,
    bool fragment_mass_tolerance_unit_ppm,
    const PeakSpectrum& theo_spectrum,
    const PeakSpectrum& exp_spectrum,
    const DataArrays::IntegerDataArray& theo_charges,
    const DataArrays::IntegerDataArray& exp_charges,
    DataArrays::FloatDataArray& ppm_error_array,
    double intensity_cutoff)
  {
    OPENMS_PRECONDITION(exp_spectrum.isSorted(), "Spectrum needs to be sorted.");
    OPENMS_PRECONDITION(theo_spectrum.isSorted(), "Spectrum needs to be sorted.");
    OPENMS_PRECONDITION((alignment.empty() == true), "Alignment result vector needs to be empty.");
    OPENMS_PRECONDITION((ppm_error_array.empty() == true), "ppm error result vector needs to be empty.");

    const Size n_t(theo_spectrum.size());
    const Size n_e(exp_spectrum.size());
    const bool has_charge = !(exp_charges.empty() || theo_charges.empty());

    if (n_t == 0 || n_e == 0) { return; }

    Size t(0), e(0);
    alignment.reserve(theo_spectrum.size());
    ppm_error_array.reserve(theo_spectrum.size());

    while (t < n_t && e < n_e)
    {
      const double theo_mz = theo_spectrum[t].getMZ();
      const double exp_mz = exp_spectrum[e].getMZ();

      int tz(0), ez(0);
      if (has_charge)
      {
        tz = theo_charges[t];
        ez = exp_charges[e];
      }
      const bool tz_matches_ez = (ez == tz || !ez || !tz);

      double ti = theo_spectrum[t].getIntensity();
      double ei = exp_spectrum[e].getIntensity();
      const bool initial_intensity_matches = ( std::min(ti, ei) / std::max(ti, ei) ) > intensity_cutoff;


      double d = exp_mz - theo_mz;
      const double max_dist_dalton = fragment_mass_tolerance_unit_ppm ? theo_mz * fragment_mass_tolerance * 1e-6 : fragment_mass_tolerance;

      if (fabs(d) <= max_dist_dalton) // match in tolerance window?
      {
        // get first peak with matching charge in tolerance window
        if (!tz_matches_ez || !initial_intensity_matches)
        {
          Size e_candidate(e);
          while (e_candidate < n_e-1)
          {
            ++e_candidate;
            double new_ez = has_charge ? exp_charges[e_candidate] : 0;
            double new_ei = exp_spectrum[e_candidate].getIntensity();
            const bool charge_matches = (new_ez == tz || !new_ez || !tz);
            const bool intensity_matches = ( std::min(ti, new_ei) / std::max(ti, new_ei) ) > intensity_cutoff;
            double new_d = exp_spectrum[e].getMZ() - theo_mz;
            if (charge_matches && new_d <= max_dist_dalton && intensity_matches)
            { // found a match
              break;
            }
            else if (new_d > max_dist_dalton)
            { // no match found
              e_candidate = e;
              break;
            }
          }
          if (e == e_candidate)
          { // no match found continue with next theo. peak
            ++t;
            continue;
          }
          else
          { // match found
            e = e_candidate;
          }
        }

        // Invariant: e now points to the first peak in tolerance window, that matches in charge and intensity

        // last peak? there can't be a better one in this tolerance window
        if (e >= n_e - 1)
        {
          // add match
          alignment.emplace_back(std::make_pair(t, e));
          // add ppm error
          double ppm_error = (exp_spectrum[e].getMZ() - theo_mz) / theo_mz * 1e6;
          ppm_error_array.emplace_back(ppm_error);
          return;
        }

        Size closest_exp_peak(e);

        // Invariant: closest_exp_peak always point to best match

        double new_ez(0);
        double best_d = exp_spectrum[closest_exp_peak].getMZ() - theo_mz;

        do // check for better match in tolerance window
        {
          // advance to next exp. peak
          ++e;

          // determine distance of next peak
          double new_d = exp_spectrum[e].getMZ() - theo_mz;
          const bool in_tolerance_window = (fabs(new_d) < max_dist_dalton);

          if (!in_tolerance_window) { break; }

          // Invariant: e is in tolerance window

          // check if charge and intensity of next peak matches
          if (has_charge) { new_ez = exp_charges[e]; }
          const bool charge_matches = (new_ez == tz || !new_ez || !tz);
          double new_ei = exp_spectrum[e].getIntensity();
          const bool intensity_matches = ( std::min(ti, new_ei) / std::max(ti, new_ei) ) > intensity_cutoff;
          if (!charge_matches || !intensity_matches) { continue; }

          // Invariant: charge and intensity matches

          const bool better_distance = (fabs(new_d) <= fabs(best_d));

          // better distance (and matching charge)? better match found
          if (better_distance)
          { // found a better match
            closest_exp_peak = e;
            best_d = new_d;
          }
          else
          { // distance got worse -> no additional matches!
            break;
          }
        }
        while (e < n_e - 1);

        // search in tolerance window for an experimental peak closer to theoretical one
        alignment.emplace_back(std::make_pair(t, closest_exp_peak));

        // add ppm error for this match
        double ppm_error = (exp_spectrum[closest_exp_peak].getMZ() - theo_mz) / theo_mz * 1e6;
        ppm_error_array.emplace_back(ppm_error);


        e = closest_exp_peak + 1;  // advance experimental peak to 1-after the best match
        ++t; // advance theoretical peak
      }
      else if (d < 0) // exp. peak is left of theo. peak (outside of tolerance window)
      {
        ++e;
      }
      else if (d > 0) // theo. peak is left of exp. peak (outside of tolerance window)
      {
        ++t;
      }
    }
  }

  PeakSpectrum OPXLSpectrumProcessingAlgorithms::deisotopeAndSingleChargeMSSpectrum(PeakSpectrum& old_spectrum, Int min_charge, Int max_charge, double fragment_tolerance, bool fragment_tolerance_unit_ppm, bool keep_only_deisotoped, Size min_isopeaks, Size max_isopeaks, bool make_single_charged)
  {
    PeakSpectrum out;
    PeakSpectrum::IntegerDataArray charge_array;
    PeakSpectrum::IntegerDataArray num_iso_peaks;
    charge_array.setName("Charges");
    num_iso_peaks.setName("NumIsoPeaks");

    vector<Size> mono_isotopic_peak(old_spectrum.size(), 0);
    vector<double> mono_iso_peak_intensity(old_spectrum.size(), 0);
    vector<Size> iso_peak_count(old_spectrum.size(), 1);
    if (old_spectrum.empty())
    {
      return out;
    }

    // determine charge seeds and extend them
    vector<Int> features(old_spectrum.size(), -1);
    Int feature_number = 0;

    for (Size current_peak = 0; current_peak != old_spectrum.size(); ++current_peak)
    {
      double current_mz = old_spectrum[current_peak].getMZ();
      mono_iso_peak_intensity[current_peak] = old_spectrum[current_peak].getIntensity();

      for (Int q = max_charge; q >= min_charge; --q)   // important: test charge hypothesis from high to low
      {
        // try to extend isotopes from mono-isotopic peak
        // if extension larger then min_isopeaks possible:
        //   - save charge q in mono_isotopic_peak[]
        //   - annotate all isotopic peaks with feature number
        if (features[current_peak] == -1)   // only process peaks which have no assigned feature number
        {
          bool has_min_isopeaks = true;
          vector<Size> extensions;
          for (Size i = 0; i < max_isopeaks; ++i)
          {
            double expected_mz = current_mz + i * Constants::C13C12_MASSDIFF_U / q;
            Size p = old_spectrum.findNearest(expected_mz);
            double tolerance_dalton = fragment_tolerance_unit_ppm ? fragment_tolerance * old_spectrum[p].getMZ() * 1e-6 : fragment_tolerance;
            if (fabs(old_spectrum[p].getMZ() - expected_mz) > tolerance_dalton)   // test for missing peak
            {
              if (i < min_isopeaks)
              {
                has_min_isopeaks = false;
              }
              break;
            }
            else
            {
              // TODO: include proper averagine model filtering. assuming the intensity gets lower for heavier peaks does not work for the high masses of cross-linked peptides
//               Size n_extensions = extensions.size();
//               if (n_extensions != 0)
//               {
//                 if (old_spectrum[p].getIntensity() > old_spectrum[extensions[n_extensions - 1]].getIntensity())
//                 {
//                   if (i < min_isopeaks)
//                   {
//                     has_min_isopeaks = false;
//                   }
//                   break;
//                 }
//               }

              // averagine check passed
              extensions.push_back(p);
              mono_iso_peak_intensity[current_peak] += old_spectrum[p].getIntensity();
              iso_peak_count[current_peak] = i + 1;
            }
          }

          if (has_min_isopeaks)
          {
            mono_isotopic_peak[current_peak] = q;
            for (Size i = 0; i != extensions.size(); ++i)
            {
              features[extensions[i]] = feature_number;
            }
            feature_number++;
          }
        }
      }
    }


    // creating PeakSpectrum containing charges
    for (Size i = 0; i != old_spectrum.size(); ++i)
    {
      Int z = mono_isotopic_peak[i];
      if (keep_only_deisotoped)
      {
        if (z == 0)
        {
          continue;
        }

        // add the number of peaks in the isotopic pattern to the data array
        num_iso_peaks.push_back(iso_peak_count[i]);

        // if already single charged or no decharging selected keep peak as it is
        if (!make_single_charged)
        {
          Peak1D p;
          p.setMZ(old_spectrum[i].getMZ());
          p.setIntensity(mono_iso_peak_intensity[i]);
          charge_array.push_back(z);
          out.push_back(p);
        }
        else
        {
          Peak1D p;
          p.setIntensity(mono_iso_peak_intensity[i]);
          p.setMZ(old_spectrum[i].getMZ() * z - (z - 1) * Constants::PROTON_MASS_U);
          charge_array.push_back(1);
          out.push_back(p);
        }
      }
      else
      {
        // keep all unassigned peaks
        if (features[i] < 0)
        {
          Peak1D p;
          p.setMZ(old_spectrum[i].getMZ());
          p.setIntensity(old_spectrum[i].getIntensity());
          charge_array.push_back(0);
          // add the number of peaks in the isotopic pattern to the data array
          num_iso_peaks.push_back(iso_peak_count[i]);
          out.push_back(p);
          continue;
        }

        // convert mono-isotopic peak with charge assigned by deisotoping
        if (z != 0)
        {
          // add the number of peaks in the isotopic pattern to the data array
          num_iso_peaks.push_back(iso_peak_count[i]);

          if (!make_single_charged)
          {
            Peak1D p;
            p.setMZ(old_spectrum[i].getMZ());
            p.setIntensity(mono_iso_peak_intensity[i]);
            charge_array.push_back(z);
            out.push_back(p);
          }
          else
          {
            Peak1D p;
            p.setMZ(old_spectrum[i].getMZ() * z - (z - 1) * Constants::PROTON_MASS_U);
            p.setIntensity(mono_iso_peak_intensity[i]);
            charge_array.push_back(z);
            out.push_back(p);
          }
        }
      }
    }
    out.setPrecursors(old_spectrum.getPrecursors());
    out.setRT(old_spectrum.getRT());

    out.setNativeID(old_spectrum.getNativeID());
    out.setInstrumentSettings(old_spectrum.getInstrumentSettings());
    out.setAcquisitionInfo(old_spectrum.getAcquisitionInfo());
    out.setSourceFile(old_spectrum.getSourceFile());
    out.setDataProcessing(old_spectrum.getDataProcessing());
    out.setType(old_spectrum.getType());
    out.setMSLevel(old_spectrum.getMSLevel());
    out.setName(old_spectrum.getName());

    out.getIntegerDataArrays().push_back(charge_array);
    out.getIntegerDataArrays().push_back(num_iso_peaks);
    return out;
  }
}
