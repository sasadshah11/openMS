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
// $Maintainer: Chris Bielow$
// $Authors: Valentin Noske, Vincent Musch$
// --------------------------------------------------------------------------


#include <fstream>
#include <OpenMS/KERNEL/Feature.h>
#include <OpenMS/KERNEL/FeatureMap.h>
#include <OpenMS/MATH/MISC/MathFunctions.h>
#include <OpenMS/QC/MQEvidenceExporter.h>
#include <OpenMS/SYSTEM/File.h>
#include <QtCore/QDir>

using namespace OpenMS;

MQEvidence::MQEvidence(const String &p)
{
    if(p.empty())
    {
        return;
    }
    String filename = p +"/evidence.txt";
    try
    {
        QString path = QString::fromStdString(p);
        QDir().mkpath(path);



        file_ = std::fstream(filename, std::fstream::out);
    }
    catch(...)
    {
        OPENMS_LOG_FATAL_ERROR << filename << " wasn’t created" << std::endl;
        throw Exception::FileNotWritable(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "out_evd");
    }
    exportHeader();
    id_ = 0;
}

MQEvidence::~MQEvidence()
{
    file_.close();
}

/*
 * Description:
 * Checks if it is possible to add text to the stream
 */
bool MQEvidence::isValid()
{
    return !!file_;
}

/*
 * Description:
 * Writes the header of the MQEvidence file
 */
void MQEvidence::exportHeader()
{
    file_ << "id" << "\t";
    file_ << "Sequence" << "\t";
    file_ << "Length" << "\t";
    file_ << "Acetyl (Protein N-term)" << "\t";
    file_ << "Oxidation (M)" << "\t";
    file_ << "Modification" << "\t";
    file_ << "Modified Sequence" << "\t";
    file_ << "Mass" << "\t";
    file_ << "Score" << "\t";
    file_ << "Protein" << "\t";
    file_ << "Protein group IDs" << "\t";
    file_ << "Charge" << "\t";
    file_ << "M/Z" << "\t";
    file_ << "Retention Time" << "\t";
    file_ << "Retention Length" << "\t";
    file_ << "Intensity" << "\t";
    file_ << "Resolution" << "\t";
    file_ << "Potential contaminant" << "\t";
    file_ << "Type" << "\t";
    file_ << "Missed cleavages" << "\t";
    file_ << "Mass error [ppm]" << "\t";
    file_ << "Uncalibrated Mass error [ppm]" << "\t";
    file_ << "Mass error [Da]" << "\t";
    file_ << "Uncalibrated Mass error [Da]" << "\t";
    file_ << "Uncalibrated - Calibrated m/z [ppm]" << "\t";
    file_ << "Uncalibrated - Calibrated m/z [Da]" << "\t";
    file_ << "Calibrated retention time start" << "\t";
    file_ << "Calibrated retention time end" << "\t";
    file_ << "Calibrated Retention Time" << "\t";
    file_ << "Retention time calibration" << "\t";
    file_ << "MS/MS count" << "\t";
    file_ << "Match time difference" << "\t";
    file_ << "Match m/z difference" << "\t";
    file_ << "Raw file" << "\t";
    file_ << "\n";
}

UInt64 MQEvidence::proteinGroupID(const String &protein)
{
    auto it = protein_id_.find(protein);
    if(it == protein_id_.end())
    {
        protein_id_.emplace(protein, protein_id_.size()+1);
        return protein_id_.size();
    }
    else
    {
        return it -> second;
    }
}

std::map<UInt64, Size> MQEvidence::makeFeatureUIDtoConsensusMapIndex(const ConsensusMap & cmap)
{
    std::map<UInt64, Size> f_to_ci;
    for(Size i = 0; i < cmap.size(); ++i)
    {
        for(const auto & fh : cmap[i].getFeatures())
        {
            auto [it, was_created_newly] = f_to_ci.emplace(fh.getUniqueId(), i);
            if (!was_created_newly) throw Exception::Precondition(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "FeatureHandle exists twice in ConsensusMap!");
            f_to_ci[fh.getUniqueId()] = i;
        }
    }
    return f_to_ci;
}

bool MQEvidence::hasValidPepID(
        const Feature & f,
        const Int64 c_feature_number,
        const std::multimap<OpenMS::String, std::pair<OpenMS::Size,OpenMS::Size>> &UIDs,
        const ProteinIdentification::Mapping &mp_f)
{
    const std::vector<PeptideIdentification> &pep_ids_f = f.getPeptideIdentifications();
    if(pep_ids_f.empty())
    {
        return false;
    }
    const PeptideIdentification & best_pep_id = pep_ids_f[0]; // PeptideIdentifications are sorted
    String best_uid = PeptideIdentification::buildUIDFromPepID(best_pep_id, mp_f.identifier_to_msrunpath);
    const auto range = UIDs.equal_range(best_uid);
    for (auto it_pep = range.first; it_pep != range.second; ++it_pep)
    {
        if(c_feature_number == it_pep->second.first)
        {
            return true;
        }
    }
    return false;
}

bool MQEvidence::hasPeptideIdentifications(const ConsensusFeature& cf)
{
    const std::vector<PeptideIdentification> &pep_ids_c = cf.getPeptideIdentifications();
    if(pep_ids_c.empty()){
        return false;
    }
    return true;

}

void MQEvidence::exportRowFromFeature(
        const Feature &f,
        const ConsensusMap &cmap,
        const Int64 c_feature_number,
        const String &raw_file,
        const std::multimap<String, std::pair<Size, Size>> &UIDs,
        const ProteinIdentification::Mapping &mp_f)
{
    const PeptideHit * pep_hits_max; // the best hit referring to score
    const ConsensusFeature & cf = cmap[c_feature_number];
    UInt64 pep_ids_size = 0;
    String type;
    if(hasValidPepID(f, c_feature_number, UIDs, mp_f))
    {
        for (Size i = 1; i < f.getPeptideIdentifications().size(); ++i)
        {
            if (f.getPeptideIdentifications()[i].getHits()[0].getSequence() == f.getPeptideIdentifications()[0].getHits()[0].getSequence()) ++pep_ids_size;
            else break;
        }
        type = "MULTI-MSMS";
        pep_hits_max = &f.getPeptideIdentifications()[0].getHits()[0];
    }
    else if(hasPeptideIdentifications(cf))
    {
        type = "MULTI-MATCH";
        pep_hits_max = &cf.getPeptideIdentifications()[0].getHits()[0];
    }
    else
    {
        return;
    }

    const double & max_score = pep_hits_max->getScore();
    const AASequence &pep_seq = pep_hits_max->getSequence();

    if (pep_seq.empty())
    {
        return;
    }
    file_ << id_ << "\t";
    ++id_;
    file_ << pep_seq.toUnmodifiedString() << "\t"; // Sequence
    file_ << pep_seq.size() << "\t"; // Length

    std::map<String, int> modifications;
    if(pep_seq.hasNTerminalModification())
    {
        const String &n_terminal_modification = pep_seq.getNTerminalModificationName();
        modifications.emplace(std::make_pair(n_terminal_modification, 1));
        n_terminal_modification.hasSubstring("Acetyl") ? file_ << 1 << "\t" : file_ << 0 << "\t"; // Acetyl (Protein N-term)
    }
    else
    {
        file_ << 0 << "\t"; // Acetyl (Protein N-term)
    }
    if(pep_seq.hasCTerminalModification()) {
        modifications.emplace(std::make_pair(pep_seq.getCTerminalModificationName(),1));
    }
    for (uint i = 0; i < pep_seq.size(); ++i) {

        if(pep_seq.getResidue(i).isModified())
        {
            ++modifications[pep_seq.getResidue(i).getModificationName()]; //TODO::Check
        }
    }
    file_ <<  modifications["Oxidation [M]"] << "\t"; // Oxidation (M)
    if(modifications.empty())
    {
        file_ << "Unmodified" << "\t";
    }
    else
    {
        for (const auto &m : modifications)
        {
            file_ << m.first << ";"; // Modification
        }
        file_ << "\t";
    }
    file_ << "_" << pep_seq << "_" << "\t"; // Modified Sequence
    file_ << pep_seq.getMonoWeight() << "\t"; // Mass
    file_ << max_score << "\t"; // Score
    const std::set<String> &accessions = pep_hits_max->extractProteinAccessionsSet();
    for (const String &p : accessions) {
        file_ << p << ";"; // Protein
    }
    file_ << "\t";
    for (const String &p : accessions) {
        file_ << proteinGroupID(p) << ";"; // Protein group ids
    }

    file_ << "\t";
    file_ << f.getCharge() << "\t"; // Charge
    file_ << f.getMZ() << "\t"; // MZ
    file_ << f.getRT()/60 << "\t"; // Retention time in min.
    file_ << (double(f.getMetaValue("rt_raw_end")) - double(f.getMetaValue("rt_raw_start")))/60 << "\t"; //Retention length in min.
    file_ << f.getIntensity() << "\t"; // Intensity
    file_ << f.getWidth()/60 << "\t";  // Resolution in min.

    file_ << (pep_hits_max->getMetaValue("is_contaminant") == "1" ? '+' : '-') << '\t'; // Potential Contaminant

    file_<< type << "\t"; // Type
    file_ << pep_hits_max->getMetaValue("missed_cleavages", "NA") << "\t"; // missed cleavages

    const double & uncalibrated_mz_error_ppm = pep_hits_max->getMetaValue("uncalibrated_mz_error_ppm", NAN);
    const double & calibrated_mz_error_ppm = pep_hits_max->getMetaValue("calibrated_mz_error_ppm", NAN);

    if(isnan(uncalibrated_mz_error_ppm) && isnan(calibrated_mz_error_ppm))
    {
        file_ << "NA" << "\t"; // Mass error [ppm]
        file_ << "NA" << "\t"; // Uncalibrated Mass error [ppm]
        file_ << "NA" << "\t"; // Mass error [Da]
        file_ << "NA" << "\t"; // Uncalibrated Mass error [Da]
        file_ << "NA" << "\t"; // Uncalibrated - Calibrated m/z [ppm]
        file_ << "NA"  << "\t"; // Uncalibrated - Calibrated m/z [mDa]
    }
    else if(isnan(calibrated_mz_error_ppm))
    {
        file_ << "NA" << "\t"; // Mass error [ppm]
        file_ << uncalibrated_mz_error_ppm << "\t"; // Uncalibrated Mass error [ppm]
        file_ << "NA" << "\t"; // Mass error [Da]
        file_ <<  OpenMS::Math::ppmToMass(uncalibrated_mz_error_ppm,f.getMZ()) << "\t"; // Uncalibrated Mass error [Da]
        file_ << "NA" << "\t"; // Uncalibrated - Calibrated m/z [ppm]
        file_ << "NA"  << "\t"; // Uncalibrated - Calibrated m/z [mDa]


    }
    else if(isnan(uncalibrated_mz_error_ppm))
    {
        file_ << calibrated_mz_error_ppm << "\t"; // Mass error [ppm]
        file_ << "NA" << "\t"; // Uncalibrated Mass error [ppm]
        file_ << OpenMS::Math::ppmToMass(calibrated_mz_error_ppm,f.getMZ()) << "\t"; // Mass error [Da]
        file_ << "NA" << "\t"; // Uncalibrated Mass error [Da]
        file_ << "NA" << "\t"; // Uncalibrated - Calibrated m/z [ppm]
        file_ << "NA"  << "\t"; // Uncalibrated - Calibrated m/z [mDa]
    }
    else
    {
        file_ << calibrated_mz_error_ppm << "\t";   //Mass error [ppm]
        file_ << uncalibrated_mz_error_ppm << "\t"; // Uncalibrated Mass error [ppm]
        file_ << OpenMS::Math::ppmToMass(calibrated_mz_error_ppm,f.getMZ()) << "\t"; // Mass error [Da]
        file_ << OpenMS::Math::ppmToMass(uncalibrated_mz_error_ppm,f.getMZ()) << "\t"; // Uncalibrated Mass error [Da]
        file_ << uncalibrated_mz_error_ppm - calibrated_mz_error_ppm << "\t"; // Uncalibrated - Calibrated m/z [ppm]
        file_ << OpenMS::Math::ppmToMass((uncalibrated_mz_error_ppm - calibrated_mz_error_ppm),f.getMZ())  << "\t"; // Uncalibrated - Calibrated m/z [Da]
    }
    f.metaValueExists("rt_align_start") ? file_ << double(f.getMetaValue("rt_align_start"))/60 << "\t" : file_ << "NA" << "\t"; //  Calibrated retention time start
    f.metaValueExists("rt_align_end") ? file_ << double(f.getMetaValue("rt_align_end"))/60 << "\t" : file_ << "NA" << "\t"; // Calibrated retention time end
    if(f.metaValueExists("rt_align"))
    {
        file_ << double(f.getMetaValue("rt_align"))/60 << "\t"; // Calibrated Retention Time
        file_ << (f.getRT() - double(f.getMetaValue("rt_align")))/60 << "\t"; // Retention time calibration
        file_ << double(f.getMetaValue("rt_align"))- cmap[c_feature_number].getRT() << "\t"; // Match time diff
    }
    else
    {
        file_ << "NA" << "\t"; // calibrated retention time
        file_ << "NA" << "\t"; // Retention time calibration
    }
    file_ << pep_ids_size<<"\t"; // MS/MS count
    if(type == "MULTI-MSMS")
    {
        file_ << "NA" << "\t"; // Match time diff
        file_ << "NA" << "\t"; // Match mz diff
    }
    else
    {
        file_ << f.getRT() - cmap[c_feature_number].getRT() << "\t";    //Match time diff
        file_ << f.getMZ() - cmap[c_feature_number].getMZ() << "\t";    //Match mz diff
    }
    file_ << raw_file << "\t"; // Raw File
    file_ << "\n";
}

void MQEvidence::exportFeatureMapTotxt(
        const FeatureMap& feature_map,
        const ConsensusMap& cmap)
{
    if(!isValid())
    {
        OpenMS_Log_error << "MqEvidence object is not valid." << std::endl;
        throw Exception::FileNotWritable(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "MqEvidence object is not valid.");
    }
    const std::map<UInt64,Size> & fTc = makeFeatureUIDtoConsensusMapIndex(cmap);
    StringList spectra_data;
    feature_map.getPrimaryMSRunPath(spectra_data);
    String raw_file = File::basename(spectra_data.empty() ? feature_map.getLoadedFilePath() : spectra_data[0]);

    ProteinIdentification::Mapping mp_f;
    mp_f.create(feature_map.getProteinIdentifications());

    std::multimap<String, std::pair<Size, Size>>UIDs = PeptideIdentification::fillConsensusPepIDMap(cmap);

    for (const Feature &f : feature_map)
    {
        const UInt64 &f_id = f.getUniqueId();
        const auto &c_id = fTc.find(f_id);
        if(c_id != fTc.end()) {
            exportRowFromFeature(f, cmap, c_id -> second, raw_file, UIDs, mp_f);
        }
        else
        {
            throw Exception::MissingInformation(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "Feature in FeatureMap has no associated ConsensusFeature.");
        }
    }
}
