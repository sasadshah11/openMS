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

#include <OpenMS/CONCEPT/ClassTest.h>
#include <OpenMS/test_config.h>

///////////////////////////

#include <OpenMS/ANALYSIS/XLMS/OPXLHelper.h>

#include <OpenMS/ANALYSIS/XLMS/OPXLDataStructs.h>
#include <OpenMS/CHEMISTRY/TheoreticalSpectrumGeneratorXLMS.h>
#include <OpenMS/ANALYSIS/XLMS/OPXLSpectrumProcessingAlgorithms.h>
#include <OpenMS/FORMAT/FASTAFile.h>
#include <OpenMS/CHEMISTRY/ProteaseDigestion.h>
#include <OpenMS/DATASTRUCTURES/ListUtils.h>
#include <QStringList>

using namespace OpenMS;

START_TEST(OPXLHelper, "$Id$")


START_SECTION(static std::vector<ResidueModification> getModificationsFromStringList(StringList modNames))
  QStringList q_str_list1;
  QStringList q_str_list2;
  q_str_list1 << "Carbamidomethyl (C)" << "Carbamidomethyl (T)";
  q_str_list2 << "Oxidation (M)" << "Oxidation (Y)";
  StringList fixedModNames = StringListUtils::fromQStringList(q_str_list1);
  StringList varModNames = StringListUtils::fromQStringList(q_str_list2);
  std::vector<ResidueModification> fixed_modifications = OPXLHelper::getModificationsFromStringList(fixedModNames);
  std::vector<ResidueModification> variable_modifications = OPXLHelper::getModificationsFromStringList(varModNames);

  TEST_EQUAL(fixed_modifications[0].getFullId(), "Carbamidomethyl (C)")
  TEST_EQUAL(fixed_modifications[1].getFullId(), "Carbamidomethyl (T)")
  TEST_EQUAL(variable_modifications[0].getFullId(), "Oxidation (M)")
  TEST_EQUAL(variable_modifications[1].getFullId(), "Oxidation (Y)")
END_SECTION

// loading and building data structures required in several following tests
std::vector<FASTAFile::FASTAEntry> fasta_db;
FASTAFile file;
file.load(OPENMS_GET_TEST_DATA_PATH("FASTAFile_test.fasta"), fasta_db);


ProteaseDigestion digestor;
String enzyme_name = "Trypsin";
digestor.setEnzyme(enzyme_name);
digestor.setMissedCleavages(2);

Size min_peptide_length = 5;

QStringList q_str_list1;
QStringList q_str_list2;
q_str_list1 << "Carbamidomethyl (C)" << "Carbamidomethyl (T)";
q_str_list2 << "Oxidation (M)" << "Oxidation (Y)";
StringList fixedModNames = StringListUtils::fromQStringList(q_str_list1);
StringList varModNames = StringListUtils::fromQStringList(q_str_list2);
std::vector<ResidueModification> fixed_modifications = OPXLHelper::getModificationsFromStringList(fixedModNames);
std::vector<ResidueModification> variable_modifications = OPXLHelper::getModificationsFromStringList(varModNames);


QStringList q_str_list3;
QStringList q_str_list4;
q_str_list3 << "K" << "E";
q_str_list4 << "D" << "E" << "C-term";
StringList cross_link_residue1 = StringListUtils::fromQStringList(q_str_list3);
StringList cross_link_residue2 = StringListUtils::fromQStringList(q_str_list4);

Size max_variable_mods_per_peptide = 5;

START_SECTION(static std::vector<OPXLDataStructs::AASeqWithMass> digestDatabase(std::vector<FASTAFile::FASTAEntry> fasta_db, EnzymaticDigestion digestor, Size min_peptide_length, StringList cross_link_residue1, StringList cross_link_residue2, std::vector<ResidueModification> fixed_modifications, std::vector<ResidueModification> variable_modifications, Size max_variable_mods_per_peptide))

  std::vector<OPXLDataStructs::AASeqWithMass> peptides = OPXLHelper::digestDatabase(fasta_db, digestor, min_peptide_length, cross_link_residue1, cross_link_residue2, fixed_modifications, variable_modifications, max_variable_mods_per_peptide);

  TEST_EQUAL(peptides.size(), 880)
  TEST_EQUAL(peptides[5].peptide_mass > 5, true) // not an empty AASequence
  TEST_EQUAL(peptides[5].peptide_mass, peptides[5].peptide_seq.getMonoWeight())
  TEST_EQUAL(peptides[500].peptide_mass > 5, true) // not an empty AASequence
  TEST_EQUAL(peptides[500].peptide_mass, peptides[500].peptide_seq.getMonoWeight())
  TEST_EQUAL(peptides[668].position, OPXLDataStructs::C_TERM)
  TEST_EQUAL(peptides[778].position, OPXLDataStructs::N_TERM)
END_SECTION

// building more data structures required in several following tests
std::vector<OPXLDataStructs::AASeqWithMass> peptides = OPXLHelper::digestDatabase(fasta_db, digestor, min_peptide_length, cross_link_residue1, cross_link_residue2, fixed_modifications, variable_modifications, max_variable_mods_per_peptide);

std::sort(peptides.begin(), peptides.end(), OPXLDataStructs::AASeqWithMassComparator());

double cross_link_mass = 150.0;
double precursor_mass_tolerance = 10;
bool precursor_mass_tolerance_unit_ppm = true;

std::vector<String> mono_masses;
mono_masses.push_back("50.0");
DoubleList cross_link_mass_mono_link = ListUtils::create<double>(mono_masses);

std::vector< double > spectrum_precursors;
for (Size i = 0; i < 800; i++)
{
  spectrum_precursors.push_back(peptides[i].peptide_mass + peptides[i+1].peptide_mass + cross_link_mass);
  spectrum_precursors.push_back(peptides[i].peptide_mass + peptides[i+2].peptide_mass + cross_link_mass);
  spectrum_precursors.push_back(peptides[i].peptide_mass + peptides[i+3].peptide_mass + cross_link_mass);
}

START_SECTION(static std::vector<OPXLDataStructs::XLPrecursor> enumerateCrossLinksAndMasses(const std::vector<OPXLDataStructs::AASeqWithMass>&  peptides, double cross_link_mass_light, const DoubleList& cross_link_mass_mono_link, const StringList& cross_link_residue1, const StringList& cross_link_residue2, std::vector< double >& spectrum_precursors, vector< int >& precursor_correction_positions, double precursor_mass_tolerance, bool precursor_mass_tolerance_unit_ppm))

  std::cout << std::endl;
  std::vector< int > spectrum_precursor_correction_positions;
  std::vector<OPXLDataStructs::XLPrecursor> precursors = OPXLHelper::enumerateCrossLinksAndMasses(peptides, cross_link_mass, cross_link_mass_mono_link, cross_link_residue1, cross_link_residue2, spectrum_precursors, spectrum_precursor_correction_positions, precursor_mass_tolerance, precursor_mass_tolerance_unit_ppm);
  // std::sort(precursors.begin(), precursors.end(), OPXLDataStructs::XLPrecursorComparator());

  TOLERANCE_ABSOLUTE(1e-3)
  TEST_EQUAL(precursors.size(), 15990)
  TEST_EQUAL(spectrum_precursor_correction_positions.size(), 15990)
  // sample about 1/15 of the data, since a lot of precursors are generated

  for (Size i = 0; i < precursors.size(); i += 2000)
  {
    if (precursors[i].beta_index > peptides.size())
    {
      // mono-link
      TEST_REAL_SIMILAR(peptides[precursors[i].alpha_index].peptide_mass + cross_link_mass_mono_link[0], precursors[i].precursor_mass)
    }
    else
    {
      // cross-link
      double computed_precursor = peptides[precursors[i].alpha_index].peptide_mass + peptides[precursors[i].beta_index].peptide_mass + cross_link_mass;
      TEST_REAL_SIMILAR(computed_precursor, precursors[i].precursor_mass)
    }
  }

END_SECTION

// building more data structures required in the following test
std::cout << std::endl;
std::vector< int > spectrum_precursor_correction_positions;
std::vector<OPXLDataStructs::XLPrecursor> precursors = OPXLHelper::enumerateCrossLinksAndMasses(peptides, cross_link_mass, cross_link_mass_mono_link, cross_link_residue1, cross_link_residue2, spectrum_precursors, spectrum_precursor_correction_positions, precursor_mass_tolerance, precursor_mass_tolerance_unit_ppm);
std::sort(precursors.begin(), precursors.end(), OPXLDataStructs::XLPrecursorComparator());

START_SECTION(static std::vector <OPXLDataStructs::ProteinProteinCrossLink> buildCandidates(const std::vector< OPXLDataStructs::XLPrecursor > & candidates, const std::vector< int > precursor_corrections, std::vector< int >& precursor_correction_positions, const std::vector<OPXLDataStructs::AASeqWithMass> & peptide_masses, const StringList & cross_link_residue1, const StringList & cross_link_residue2, double cross_link_mass, const DoubleList & cross_link_mass_mono_link, std::vector< double >& spectrum_precursor_vector, std::vector< double >& allowed_error_vector, String cross_link_name))
  double precursor_mass = 10668.85060;
  double allowed_error = 0.1;
  String cross_link_name = "MyLinker";

  std::vector< OPXLDataStructs::XLPrecursor > filtered_precursors;

  // determine MS2 precursors that match to the current peptide mass
  std::vector< OPXLDataStructs::XLPrecursor >::const_iterator low_it;
  std::vector< OPXLDataStructs::XLPrecursor >::const_iterator up_it;

  low_it = std::lower_bound(precursors.begin(), precursors.end(), precursor_mass - allowed_error, OPXLDataStructs::XLPrecursorComparator());
  up_it = std::upper_bound(precursors.begin(), precursors.end(), precursor_mass + allowed_error, OPXLDataStructs::XLPrecursorComparator());

  if (low_it != up_it) // no matching precursor in data
  {
    for (; low_it != up_it; ++low_it)
    {
      filtered_precursors.push_back(*low_it);
    }
  }
  TEST_EQUAL(precursors.size(), 15990)
  TEST_EQUAL(filtered_precursors.size(), 35)
  std::vector< int > precursor_corrections(59, 0);
  std::vector< int > precursor_correction_positions(59, 0);
  std::vector< double > spectrum_precursor_vector(1, 0.0);
  std::vector< double > allowed_error_vector(1, allowed_error);

  std::vector <OPXLDataStructs::ProteinProteinCrossLink> spectrum_candidates = OPXLHelper::buildCandidates(filtered_precursors, precursor_corrections, precursor_correction_positions, peptides, cross_link_residue1, cross_link_residue2, cross_link_mass, cross_link_mass_mono_link, spectrum_precursor_vector, allowed_error_vector, cross_link_name);

  TEST_EQUAL(spectrum_candidates.size(), 1680)
  TEST_EQUAL(spectrum_candidates[50].cross_linker_name, "MyLinker")
  for (Size i = 0; i < spectrum_candidates.size(); i += 200)
  {
    TEST_REAL_SIMILAR(spectrum_candidates[i].alpha.getMonoWeight() + spectrum_candidates[i].beta.getMonoWeight() + spectrum_candidates[i].cross_linker_mass, precursor_mass)
  }

END_SECTION

START_SECTION(static void buildFragmentAnnotations(std::vector<PeptideHit::PeakAnnotation> & frag_annotations, const std::vector< std::pair< Size, Size > > & matching, const PeakSpectrum & theoretical_spectrum, const PeakSpectrum & experiment_spectrum))
  TheoreticalSpectrumGeneratorXLMS specGen;
  Param param = specGen.getParameters();
  param.setValue("add_isotopes", "false");
  param.setValue("add_metainfo", "true");
  param.setValue("add_first_prefix_ion", "false");
  param.setValue("add_a_ions", "false");
  param.setValue("add_losses", "false");
  param.setValue("add_precursor_peaks", "false");
  param.setValue("add_k_linked_ions", "false");
  specGen.setParameters(param);

  PeakSpectrum theo_spec, exp_spec;
  // Theoretical Spec with metainfo
  AASequence peptedi = AASequence::fromString("PEPTEDI");
  specGen.getLinearIonSpectrum(theo_spec, peptedi, 4, true);

  param.setValue("add_metainfo", "false");
  specGen.setParameters(param);

  // Theoretical Spec without metainfo (Pseudo experimental spectrum)
  AASequence peptide = AASequence::fromString("PEPTIDE");
  specGen.getLinearIonSpectrum(exp_spec, peptide, 3, true);
  std::vector <std::pair <Size, Size> > alignment;

  DataArrays::FloatDataArray dummy_array;
  DataArrays::IntegerDataArray dummy_charge_array;
  OPXLSpectrumProcessingAlgorithms::getSpectrumAlignmentFastCharge(alignment, 50, true, theo_spec, exp_spec, dummy_charge_array, dummy_charge_array, dummy_array);

  std::vector<PeptideHit::PeakAnnotation> frag_annotations;

  // test, that additional annotations are added and do not replace existing ones
  PeptideHit::PeakAnnotation frag_anno;
  frag_anno.annotation = "TEST";
  frag_anno.charge = 50;
  frag_anno.mz = 1.0;
  frag_anno.intensity = 5.0;
  frag_annotations.push_back(frag_anno);

  OPXLHelper::buildFragmentAnnotations(frag_annotations, alignment, theo_spec, exp_spec);

  // number of annotations should be equal to number of aligned peaks (+ 1 for manual "TEST" annotation)
  TEST_EQUAL(frag_annotations.size(), alignment.size() + 1)
  TEST_EQUAL(frag_annotations[0].charge, 50)
  TEST_EQUAL(frag_annotations[0].mz, 1.0)
  TEST_EQUAL(frag_annotations[0].intensity, 5.0)
  TEST_EQUAL(frag_annotations[0].annotation, "TEST")

  TEST_EQUAL(frag_annotations[1].charge, 1)
  TEST_REAL_SIMILAR(frag_annotations[1].mz, 98.06004)
  TEST_EQUAL(frag_annotations[1].intensity, 1)
  TEST_EQUAL(frag_annotations[1].annotation, "[alpha|ci$b1]")

  TEST_EQUAL(frag_annotations[3].charge, 1)
  TEST_REAL_SIMILAR(frag_annotations[3].mz, 324.15539)
  TEST_EQUAL(frag_annotations[3].intensity, 1)
  TEST_EQUAL(frag_annotations[3].annotation, "[alpha|ci$b3]")

END_SECTION

END_TEST
