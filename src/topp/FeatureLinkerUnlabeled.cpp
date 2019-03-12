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
// $Maintainer: Timo Sachsenberg $
// $Authors: Marc Sturm, Clemens Groepl, Steffen Sass $
// --------------------------------------------------------------------------
#include <OpenMS/ANALYSIS/MAPMATCHING/FeatureGroupingAlgorithmUnlabeled.h>

#include "FeatureLinkerBase.cpp"

#include <iomanip>     // setw

using namespace OpenMS;
using namespace std;

//-------------------------------------------------------------
// Doxygen docu
//-------------------------------------------------------------

/**
    @page TOPP_FeatureLinkerUnlabeled FeatureLinkerUnlabeled

    @brief Groups corresponding features from multiple maps.

<CENTER>
    <table>
        <tr>
            <td ALIGN = "center" BGCOLOR="#EBEBEB"> potential predecessor tools </td>
            <td VALIGN="middle" ROWSPAN=4> \f$ \longrightarrow \f$ FeatureLinkerUnlabeled \f$ \longrightarrow \f$</td>
            <td ALIGN = "center" BGCOLOR="#EBEBEB"> potential successor tools </td>
        </tr>
        <tr>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_FeatureFinderCentroided @n (or another feature detection algorithm) </td>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_ProteinQuantifier </td>
        </tr>
        <tr>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=2> @ref TOPP_MapAlignerPoseClustering @n (or another map alignment algorithm) </td>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_TextExporter </td>
        </tr>
        <tr>
            <td VALIGN="middle" ALIGN = "center" ROWSPAN=1> @ref TOPP_SeedListGenerator </td>
        </tr>
    </table>
</CENTER>


    This tool provides an algorithm for grouping corresponding features in multiple runs of label-free experiments. For more details and algorithm-specific parameters (set in the INI file) see "Detailed Description" in the @ref OpenMS::FeatureGroupingAlgorithmUnlabeled "algorithm documentation"
  or the INI file table below.

    FeatureLinkerUnlabeled takes several feature maps (featureXML files) and stores the corresponding features in a consensus map (consensusXML file). Feature maps can be created from MS experiments (peak data) using one of the FeatureFinder TOPP tools.

  Advanced users can convert the consensusXML generated by this tool to EDTA using @subpage TOPP_FileConverter and plot the distribution of distances in RT (or m/z) between different input files (can be done in Excel).
  The distribution should be Gaussian-like with very few points  beyond the tails. Points far away from the Gaussian indicate a too wide tolerance. A Gaussian with its left/right tail trimmed indicates a too narrow tolerance.

    @see @ref TOPP_FeatureLinkerUnlabeledQT @ref TOPP_FeatureLinkerLabeled


    <B>The command line parameters of this tool are:</B>
    @verbinclude TOPP_FeatureLinkerUnlabeled.cli
    <B>INI file documentation of this tool:</B>
    @htmlinclude TOPP_FeatureLinkerUnlabeled.html
*/

// We do not want this class to show up in the docu:
/// @cond TOPPCLASSES

class TOPPFeatureLinkerUnlabeled :
  public TOPPFeatureLinkerBase
{

public:
  TOPPFeatureLinkerUnlabeled() :
    TOPPFeatureLinkerBase("FeatureLinkerUnlabeled", "Groups corresponding features from multiple maps.")
  {
  }

protected:
  void registerOptionsAndFlags_() override
  {
    TOPPFeatureLinkerBase::registerOptionsAndFlags_();
    registerSubsection_("algorithm", "Algorithm parameters section");
  }

  Param getSubsectionDefaults_(const String & /*section*/) const override
  {
    FeatureGroupingAlgorithmUnlabeled algo;
    Param p = algo.getParameters();
    return p;
  }

  ExitCodes main_(int, const char **) override
  {
    FeatureGroupingAlgorithmUnlabeled * algorithm = new FeatureGroupingAlgorithmUnlabeled();

    //-------------------------------------------------------------
    // parameter handling
    //-------------------------------------------------------------
    StringList ins;
    ins = getStringList_("in");
    String out = getStringOption_("out");

    //-------------------------------------------------------------
    // check for valid input
    //-------------------------------------------------------------
    // check if all input files have the correct type
    FileTypes::Type file_type = FileHandler::getType(ins[0]);
    for (Size i = 0; i < ins.size(); ++i)
    {
      if (FileHandler::getType(ins[i]) != file_type)
      {
        writeLog_("Error: All input files must be of the same type!");
        return ILLEGAL_PARAMETERS;
      }
    }

    //-------------------------------------------------------------
    // set up algorithm
    //-------------------------------------------------------------
    Param algorithm_param = getParam_().copy("algorithm:", true);
    writeDebug_("Used algorithm parameters", algorithm_param, 3);
    algorithm->setParameters(algorithm_param);

    Size reference_index(0);
    //-------------------------------------------------------------
    // perform grouping
    //-------------------------------------------------------------
    // load input
    ConsensusMap out_map;
    StringList ms_run_locations;
    if (file_type == FileTypes::FEATUREXML)
    {
      // use map with highest number of features as reference:
      Size max_count(0);
      FeatureXMLFile f;
      for (Size i = 0; i < ins.size(); ++i)
      {
        Size s = f.loadSize(ins[i]);
        if (s > max_count)
        {
          max_count = s;
          reference_index = i;
        }
      }

      // Load reference map and input it to the algorithm
      UInt64 ref_id;
      Size ref_size;
      std::vector<PeptideIdentification> ref_pepids;
      std::vector<ProteinIdentification> ref_protids;
      {
        FeatureMap map_ref;
        FeatureXMLFile f_fxml_tmp;
        f_fxml_tmp.getOptions().setLoadConvexHull(false);
        f_fxml_tmp.getOptions().setLoadSubordinates(false);
        f_fxml_tmp.load(ins[reference_index], map_ref);
        algorithm->setReference(reference_index, map_ref);
        ref_id = map_ref.getUniqueId();
        ref_size = map_ref.size();
        ref_pepids = map_ref.getUnassignedPeptideIdentifications();
        ref_protids = map_ref.getProteinIdentifications();
      }

      ConsensusMap dummy;
      // go through all input files and add them to the result one by one
      for (Size i = 0; i < ins.size(); ++i)
      {

        FeatureXMLFile f_fxml_tmp;
        FeatureMap tmp_map;
        f_fxml_tmp.getOptions().setLoadConvexHull(false);
        f_fxml_tmp.getOptions().setLoadSubordinates(false);
        f_fxml_tmp.load(ins[i], tmp_map);

        // copy over information on the primary MS run
        StringList ms_runs;
        tmp_map.getPrimaryMSRunPath(ms_runs);
        ms_run_locations.insert(ms_run_locations.end(), ms_runs.begin(), ms_runs.end());

        if (i != reference_index)
        {
          algorithm->addToGroup(i, tmp_map);

          // store some meta-data about the maps in the "dummy" object -> try to
          // keep the same order as they were given in the input independent of
          // which map is the reference.

          dummy.getColumnHeaders()[i].filename = ins[i];
          dummy.getColumnHeaders()[i].size = tmp_map.size();
          dummy.getColumnHeaders()[i].unique_id = tmp_map.getUniqueId();

          // add protein identifications to result map
          dummy.getProteinIdentifications().insert(
            dummy.getProteinIdentifications().end(),
            tmp_map.getProteinIdentifications().begin(),
            tmp_map.getProteinIdentifications().end());

          // add unassigned peptide identifications to result map
          dummy.getUnassignedPeptideIdentifications().insert(
            dummy.getUnassignedPeptideIdentifications().end(),
            tmp_map.getUnassignedPeptideIdentifications().begin(),
            tmp_map.getUnassignedPeptideIdentifications().end());
        }
        else
        {
          // copy the meta-data from the refernce map
          dummy.getColumnHeaders()[i].filename = ins[i];
          dummy.getColumnHeaders()[i].size = ref_size;
          dummy.getColumnHeaders()[i].unique_id = ref_id;

          // add protein identifications to result map
          dummy.getProteinIdentifications().insert(
            dummy.getProteinIdentifications().end(),
            ref_protids.begin(),
            ref_protids.end());

          // add unassigned peptide identifications to result map
          dummy.getUnassignedPeptideIdentifications().insert(
            dummy.getUnassignedPeptideIdentifications().end(),
            ref_pepids.begin(),
            ref_pepids.end());
        }
      }

      // get the resulting map
      out_map = algorithm->getResultMap();

      //
      // Copy back meta-data (Protein / Peptide ids / File descriptions)
      //

      // add protein identifications to result map
      out_map.getProteinIdentifications().insert(
        out_map.getProteinIdentifications().end(),
        dummy.getProteinIdentifications().begin(),
        dummy.getProteinIdentifications().end());

      // add unassigned peptide identifications to result map
      out_map.getUnassignedPeptideIdentifications().insert(
        out_map.getUnassignedPeptideIdentifications().end(),
        dummy.getUnassignedPeptideIdentifications().begin(),
        dummy.getUnassignedPeptideIdentifications().end());

      out_map.setColumnHeaders(dummy.getColumnHeaders());

      // canonical ordering for checking the results, and the ids have no real meaning anyway
      // the way this was done in DelaunayPairFinder and StablePairFinder
      // -> the same ordering as FeatureGroupingAlgorithmUnlabeled::group applies!
      out_map.sortByMZ();
      out_map.updateRanges();
    }
    else
    {
      vector<ConsensusMap> maps(ins.size());
      ConsensusXMLFile f;
      for (Size i = 0; i < ins.size(); ++i)
      {
        f.load(ins[i], maps[i]);
        StringList ms_runs;
        maps[i].getPrimaryMSRunPath(ms_runs);
        ms_run_locations.insert(ms_run_locations.end(), ms_runs.begin(), ms_runs.end());
      }
      // group
      algorithm->FeatureGroupingAlgorithm::group(maps, out_map);

      // set file descriptions:
      bool keep_subelements = getFlag_("keep_subelements");
      if (!keep_subelements)
      {
        for (Size i = 0; i < ins.size(); ++i)
        {
          out_map.getColumnHeaders()[i].filename = ins[i];
          out_map.getColumnHeaders()[i].size = maps[i].size();
          out_map.getColumnHeaders()[i].unique_id = maps[i].getUniqueId();
        }
      }
      else
      {
        // components of the output map are not the input maps themselves, but
        // the components of the input maps:
        algorithm->transferSubelements(maps, out_map);
      }
    }

    // assign unique ids
    out_map.applyMemberFunction(&UniqueIdInterface::setUniqueId);

    // annotate output with data processing info
    addDataProcessing_(out_map, getProcessingInfo_(DataProcessing::FEATURE_GROUPING));

    out_map.setPrimaryMSRunPath(ms_run_locations);
    // write output
    ConsensusXMLFile().store(out, out_map);

    // some statistics
    map<Size, UInt> num_consfeat_of_size;
    for (ConsensusMap::const_iterator cmit = out_map.begin(); cmit != out_map.end(); ++cmit)
    {
      ++num_consfeat_of_size[cmit->size()];
    }

    LOG_INFO << "Number of consensus features:" << endl;
    for (map<Size, UInt>::reverse_iterator i = num_consfeat_of_size.rbegin(); i != num_consfeat_of_size.rend(); ++i)
    {
      LOG_INFO << "  of size " << setw(2) << i->first << ": " << setw(6) << i->second << endl;
    }
    LOG_INFO << "  total:      " << setw(6) << out_map.size() << endl;

    delete algorithm;

    return EXECUTION_OK;
  }

};


int main(int argc, const char ** argv)
{
  TOPPFeatureLinkerUnlabeled tool;
  return tool.main(argc, argv);
}

/// @endcond
