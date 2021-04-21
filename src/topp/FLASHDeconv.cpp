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

#include <OpenMS/APPLICATIONS/TOPPBase.h>
#include <OpenMS/ANALYSIS/TOPDOWN/FLASHDeconvAlgorithm.h>
#include <OpenMS/ANALYSIS/TOPDOWN/MassFeatureTrace.h>
#include <OpenMS/ANALYSIS/TOPDOWN/DeconvolutedSpectrum.h>
#include <QFileInfo>
#include <OpenMS/FORMAT/FileTypes.h>
#include <OpenMS/FORMAT/MzMLFile.h>
#include <OpenMS/METADATA/SpectrumLookup.h>
#include <OpenMS/ANALYSIS/TOPDOWN/QScore.h>
#include <OpenMS/TRANSFORMATIONS/RAW2PEAK/PeakPickerHiRes.h>

using namespace OpenMS;
using namespace std;

#define DEBUG_EXTRA_PARAMTER

//-------------------------------------------------------------
// Doxygen docu
//-------------------------------------------------------------
/**
  @page TOPP_FLASHDeconv TOPP_FLASHDeconv
  (Need to be modified)

  @brief  @ref
  @code
  @endcode
  @verbinclude
  @htmlinclude
*/


class TOPPFLASHDeconv :
    public TOPPBase
{
public:
  TOPPFLASHDeconv() :
      TOPPBase("FLASHDeconv", "Ultra-fast high-quality deconvolution enables online processing of top-down MS data")
  {
  }


protected:
  // this function will be used to register the tool parameters
  // it gets automatically called on tool execution
  void registerOptionsAndFlags_() override
  {

    registerInputFile_("in", "<file>", "", "Input file (mzML)");
    setValidFormats_("in", ListUtils::create<String>("mzML"));

    #ifdef DEBUG_EXTRA_PARAMTER
    registerInputFile_("in_train", "<file>", "", "topPIC result *prsm.tsv file for QScore training", false, true);
    setValidFormats_("in_train", ListUtils::create<String>("tsv"));
    registerOutputFile_("out_train", "<file>", "","train result csv file for QScore training", false, true);
    setValidFormats_("out_train", ListUtils::create<String>("csv"));
    #endif

    registerInputFile_("in_log",
                       "<file>",
                       "",
                       "log file generated by FLASHIda. Only generated by real-time acquisition",
                       false,
                       true);
    setValidFormats_("in_log", ListUtils::create<String>("log"), false);

    registerOutputFile_("out",
                        "<file>",
                        "",
                        "output file (tsv) - feature level deconvoluted masses (or ensemble spectrum level deconvluted mass if use_ensemble_spectrum is set to 1) ");
    setValidFormats_("out", ListUtils::create<String>("tsv"));

    registerOutputFileList_("out_spec", "<file for MS1, file for MS2, ...>", {""},
                            "output files (tsv) - spectrum level deconvoluted masses per ms level", false);
    setValidFormats_("out_spec", ListUtils::create<String>("tsv"));

    registerOutputFile_("out_mzml", "<file>", "",
                        "mzml format output file (mzML) - spectrum level deconvoluted masses per ms level", false);
    setValidFormats_("out_mzml", ListUtils::create<String>("mzML"));

    registerOutputFile_("out_promex", "<file>", "",
                        "promex format output file (ms1ft) - only MS1 deconvoluted masses are recorded", false);
    setValidFormats_("out_promex", ListUtils::create<String>("ms1ft"), false);

    registerOutputFileList_("out_topFD",
                            "<file for MS1, file for MS2, ...>",
                            {""},
                            "topFD format output files (msalign) - spectrum level deconvoluted masses per ms level. The file name for MSn should end with msn.msalign to be able to be recognized by TopPIC. "
                            "For example, -out_topFD [name]_ms1.msalign [name]_ms2.msalign",
                            false);

      registerOutputFileList_("out_topFD_feature", "<file  for MS1, file for MS2, ...>", {""},
                          "topFD format output feature file (feature) format per MS level", false);
      setValidFormats_("out_topFD_feature", ListUtils::create<String>("feature"), false);

    setValidFormats_("out_topFD", ListUtils::create<String>("msalign"), false);

    registerIntOption_("mzml_mass_charge",
                       "<0:uncharged 1: +1 charged -1: -1 charged>",
                       0,
                       "Charge status of deconvoluted masses in mzml output",
                       false);

    setMinInt_("mzml_mass_charge", -1);
    setMaxInt_("mzml_mass_charge", 1);

    registerIntOption_("preceding_MS1_count",
                       "<number>",
                       1,
                       "Specifies the number of preceding MS1 spectra for MS2 precursor determination. In TDP, some precursor peaks in MS2 are not part of "
                       "the deconvoluted masses in MS1 immediately preceding the MS2. In this case, increasing this parameter allows for the search in further preceding "
                       "MS1 spectra and helps determine exact precursor masses.",
                       false,
                       false);

    setMinInt_("preceding_MS1_count", 1);

    registerIntOption_("write_detail",
                       "<1:true 0:false>",
                       0,
                       "to write peak info per deconvoluted mass in detail or not in [prefix]_MSn_spec.tsv files. If set to 1, all peak information (m/z, intensity, charge, and isotope index) per mass is reported.",
                       false,
                       false);

    setMinInt_("write_detail", 0);
    setMaxInt_("write_detail", 1);

    registerIntOption_("max_MS_level", "", 2, "maximum MS level (inclusive) for deconvolution", false, true);
    setMinInt_("max_MS_level", 1);

    registerIntOption_("use_ensemble_spectrum",
                       "",
                       0,
                       "if set to 1, all spectra will add up to a single ensemble spectrum (per MS level) for which the deconvolution is performed. out_spec should specify the output spectrum level deconvolution tsv files.",
                       false,
                       false);

    setMinInt_("use_ensemble_spectrum", 0);
    setMaxInt_("use_ensemble_spectrum", 1);

    registerIntOption_("use_RNA_averagine", "", 0, "if set to 1, RNA averagine model is used", false, true);
    setMinInt_("use_RNA_averagine", 0);
    setMaxInt_("use_RNA_averagine", 1);

    Param fd_defaults = FLASHDeconvAlgorithm().getDefaults();
    // overwrite algorithm default so we export everything (important for copying back MSstats results)
    fd_defaults.setValue("tol", DoubleList{10.0, 10.0}, "ppm tolerance");
    fd_defaults.setValue("min_charge", 2);
    fd_defaults.setValue("max_charge", 100);
    fd_defaults.setValue("min_mz", -1.0);
    fd_defaults.addTag("min_mz", "advanced");
    fd_defaults.setValue("max_mz", -1.0);
    fd_defaults.addTag("max_mz", "advanced");
    fd_defaults.setValue("min_rt", -1.0);
    fd_defaults.addTag("min_rt", "advanced");
    fd_defaults.setValue("max_rt", -1.0);
    fd_defaults.addTag("max_rt", "advanced");
    fd_defaults.setValue("min_mass", 50.0);
    fd_defaults.setValue("max_mass", 100000.0);
    //fd_defaults.addTag("tol", "advanced"); // hide entry
    fd_defaults.setValue("min_peaks", IntList{2, 1});
    fd_defaults.addTag("min_peaks", "advanced");
    fd_defaults.setValue("min_intensity", .0, "intensity threshold");
    fd_defaults.addTag("min_intensity", "advanced");
    fd_defaults.setValue("min_isotope_cosine",
                         DoubleList{.8, .9},
                         "cosine threshold between avg. and observed isotope pattern for MS1, 2, ... (e.g., -min_isotope_cosine 0.8 0.6 to specify 0.8 and 0.6 for MS1 and MS2, respectively)");
    //fd_defaults.addTag("min_isotope_cosine_", "advanced");

    fd_defaults.setValue("max_mass_count",
                         IntList{-1, -1},
                         "maximum mass count per spec for MS1, 2, ... (e.g., -max_mass_count_ 100 50 to specify 100 and 50 for MS1 and MS2, respectively. -1 specifies unlimited)");
    fd_defaults.addTag("max_mass_count", "advanced");


    fd_defaults.setValue("rt_window", 180.0, "RT window for MS1 deconvolution");
    fd_defaults.addTag("rt_window", "advanced");

    fd_defaults.remove("max_mass_count");
    //fd_defaults.remove("min_mass_count");

    Param mf_defaults = MassFeatureTrace().getDefaults();
    mf_defaults.setValue("min_isotope_cosine",
                         -1.0,
                         "Cosine threshold between avg. and observed isotope pattern for mass features. if not set, controlled by -Algorithm:min_isotope_cosine_ option");
    mf_defaults.addTag("min_isotope_cosine", "advanced");
    mf_defaults.remove("noise_threshold_int");
    mf_defaults.remove("reestimate_mt_sd");
    mf_defaults.remove("trace_termination_criterion");
    mf_defaults.remove("trace_termination_outliers");
    mf_defaults.remove("chrom_peak_snr");

    mf_defaults.remove("mass_error_ppm"); // hide entry
    //mf_defaults.remove("min_sample_rate");

    Param combined;
    combined.insert("Algorithm:", fd_defaults);
    combined.insert("FeatureTracing:", mf_defaults);
    registerFullParam_(combined);
  }

  // the main_ function is called after all parameters are read
  ExitCodes main_(int, const char **) override
  {
    OPENMS_LOG_INFO << "Initializing ... " << endl;
    const bool write_detail_qscore_att = false;

    //-------------------------------------------------------------
    // parsing parameters
    //-------------------------------------------------------------

    String in_file = getStringOption_("in");
    String out_file = getStringOption_("out");
    String in_train_file = "";//getStringOption_("in_train");
    String in_log_file = getStringOption_("in_log");
    String out_train_file = "";//getStringOption_("out_train");
    auto out_spec_file = getStringList_("out_spec");
    String out_mzml_file = getStringOption_("out_mzml");
    String out_promex_file = getStringOption_("out_promex");
    auto out_topfd_file = getStringList_("out_topFD");
    auto out_topfd_feature_file = getStringList_("out_topFD_feature");
    //double topFD_qscore_threshold = getDoubleOption_("topFD_qscore_threshold");
    bool use_RNA_averagine = getIntOption_("use_RNA_averagine") > 0;
    int max_ms_level = getIntOption_("max_MS_level");
    bool ensemble = getIntOption_("use_ensemble_spectrum") > 0;
    bool write_detail = getIntOption_("write_detail") > 0;
    int mzml_charge = getIntOption_("mzml_mass_charge");
    double min_rt = getDoubleOption_("Algorithm:min_rt");
    double max_rt = getDoubleOption_("Algorithm:max_rt");

    #ifdef DEBUG_EXTRA_PARAMTER
    in_train_file = getStringOption_("in_train");
    out_train_file = getStringOption_("out_train");
    fstream fi_out;
    fi_out.open(in_file + ".txt", fstream::out); //
    fstream fi_m;
      auto fi_m_str(in_file);
    std::replace( fi_m_str.begin(), fi_m_str.end(), '_', 't');
      std::replace( fi_m_str.begin(), fi_m_str.end(), '.', 'd');
    fi_m.open(fi_m_str + ".m", fstream::out);
    #endif

    fstream out_stream, out_train_stream, out_promex_stream;
    std::vector<fstream> out_spec_streams, out_topfd_streams, out_topfd_feature_streams;

    out_stream.open(out_file, fstream::out);
    MassFeatureTrace::writeHeader(out_stream);

    if (!out_promex_file.empty())
    {
      out_promex_stream.open(out_promex_file, fstream::out);
      MassFeatureTrace::writePromexHeader(out_promex_stream);
    }

  if (!out_topfd_feature_file.empty())
  {
      out_topfd_feature_streams = std::vector<fstream>(out_topfd_feature_file.size());
      for (int i = 0; i < out_topfd_feature_file.size(); i++)
      {
          out_topfd_feature_streams[i].open(out_topfd_feature_file[i], fstream::out);
      }
      MassFeatureTrace::writeTopFDFeatureHeader(out_topfd_feature_streams);
  }

    if (!out_topfd_file.empty())
    {
      out_topfd_streams = std::vector<fstream>(out_topfd_file.size());
      for (int i = 0; i < out_topfd_file.size(); i++)
      {
        out_topfd_streams[i].open(out_topfd_file[i], fstream::out);
      }
    }
    if (!out_spec_file.empty())
    {
      out_spec_streams = std::vector<fstream>(out_spec_file.size());
      for (int i = 0; i < out_spec_file.size(); i++)
      {
        out_spec_streams[i].open(out_spec_file[i], fstream::out);
        DeconvolutedSpectrum::writeDeconvolutedMassesHeader(out_spec_streams[i], i + 1, write_detail);
      }
    }

    std::unordered_map<int, FLASHDeconvHelperStructs::TopPicItem> top_pic_map;

    if (!in_train_file.empty() && !out_train_file.empty())
    {
      out_train_stream.open(out_train_file, fstream::out);
      QScore::writeAttHeader(out_train_stream, write_detail_qscore_att);
      std::ifstream in_trainstream(in_train_file);
      String line;
      bool start = false;
      while (std::getline(in_trainstream, line))
      {
        if (line.rfind("Data file name", 0) == 0)
        {
          start = true;
          continue;
        }
        if (!start)
        {
          continue;
        }

        auto tp = FLASHDeconvHelperStructs::TopPicItem(line);
        top_pic_map[tp.scan_] = tp;
      }
      in_trainstream.close();
    }


    std::map<int, std::vector<std::vector<double>>> precursor_map_for_real_time_acquisition; // ms1 scan -> mass, charge ,score, mz range, precursor int, mass int, color
    if (!in_log_file.empty()) // TODO fix this
    {
      std::ifstream instream(in_log_file);
      if (instream.good())
      {
        String line;
        int scan;
        double mass, charge, w1, w2, qscore, pint, mint, color;
        while (std::getline(instream, line))
        {
          if (line.find("0 targets") != line.npos)
          {
            continue;
          }
          if (line.hasPrefix("MS1"))
          {
            Size st = line.find("MS1 Scan# ") + 10;
            Size ed = line.find(' ', st);
            String n = line.substr(st, ed);
            scan = atoi(n.c_str());
            precursor_map_for_real_time_acquisition[scan] = std::vector<std::vector<double>>();//// ms1 scan -> mass, charge ,score, mz range, precursor int, mass int, color
          }
          if (line.hasPrefix("Mass"))
          {
            Size st = 5;
            Size ed = line.find('\t');
            String n = line.substr(st, ed);
            mass = atof(n.c_str());

            st = line.find("Z=") + 2;
            ed = line.find('\t', st);
            n = line.substr(st, ed);
            charge = atof(n.c_str());

            st = line.find("Score=") + 6;
            ed = line.find('\t', st);
            n = line.substr(st, ed);
            qscore = atof(n.c_str());

            st = line.find("[") + 1;
            ed = line.find('-', st);
            n = line.substr(st, ed);
            w1 = atof(n.c_str());

            st = line.find('-', ed) + 1;
            ed = line.find(']', st);
            n = line.substr(st, ed);
            w2 = atof(n.c_str());

            st = line.find("PrecursorIntensity=", ed) + 19;
            ed = line.find('\t', st);
            n = line.substr(st, ed);
            pint = atof(n.c_str());

            st = line.find("PrecursorMassIntensity=", ed) + 23;
            ed = line.find('\t', st);
            n = line.substr(st, ed);
            mint = atof(n.c_str());

            st = line.find("Color=", ed) + 6;
            //ed = line.find(' ', st);
            n = line.substr(st, st + 1);
            if (n.hasPrefix("B"))
            {
              color = 1.0;
            }
            else if (n.hasPrefix("R"))
            {
              color = 2.0;
            }
            else if (n.hasPrefix("b"))
            {
              color = 3.0;
            }
            else if (n.hasPrefix("r"))
            {
              color = 4.0;
            }
            else
            {
              color = 5.0;
            }

            std::vector<double> e(8);
            e[0] = mass;
            e[1] = charge;
            e[2] = qscore;
            e[3] = w1;
            e[4] = w2;
            e[5] = pint;
            e[6] = mint;
            e[7] = color;

            precursor_map_for_real_time_acquisition[scan].push_back(e);
          }
        }
//precursor_map_for_real_time_acquisition[scan] = std::vector<std::vector<double>>();//// ms1 scan -> mass, charge ,score, mz range, precursor int, mass int, color


        instream.close();
      }
      else
      {
        std::cout << in_log_file << " not found\n";
      }
    }
    int current_max_ms_level = 0;

    auto spec_cntr = std::vector<int>(max_ms_level, 0);
    // spectrum number with at least one deconvoluted mass per ms level per input file
    auto qspec_cntr = std::vector<int>(max_ms_level, 0);
    // mass number per ms level per input file
    auto mass_cntr = std::vector<int>(max_ms_level, 0);
    // feature number per input file
    int feature_cntr = 0;

    // feature index written in the output file
    int feature_index = 1;

    MSExperiment ensemble_map;
    // generate ensemble spectrum if param.ensemble is set
    if (ensemble)
    {
      for (int i = 0; i < max_ms_level; i++)
      {
        auto spec = MSSpectrum();
        spec.setMSLevel(i + 1);
        std::ostringstream name;
        name << "ensemble MS" << (i + 1) << " spectrum";
        spec.setName(name.str());
        ensemble_map.addSpectrum(spec);
      }
    }

    //-------------------------------------------------------------
    // reading input
    //-------------------------------------------------------------

    MSExperiment map;
    MzMLFile mzml;

    // all for measure elapsed cup wall time
    double elapsed_cpu_secs = 0, elapsed_wall_secs = 0;
    auto elapsed_deconv_cpu_secs = std::vector<double>(max_ms_level, .0);
    auto elapsed_deconv_wall_secs = std::vector<double>(max_ms_level, .0);

    auto begin = clock();
    auto t_start = chrono::high_resolution_clock::now();

    OPENMS_LOG_INFO << "Processing : " << in_file << endl;

    mzml.setLogType(log_type_);
    mzml.load(in_file, map);

    //      double rtDuration = map[map.size() - 1].getRT() - map[0].getRT();
    int ms1_cntr = 0;
    double ms2_cntr = .0; // for debug...
    current_max_ms_level = 0;

    // read input dataset once to count spectra and generate ensemble spectrum if necessary
    for (auto &it : map)
    {
      if (it.empty())
      {
        continue;
      }
      if (it.getMSLevel() > max_ms_level)
      {
        continue;
      }

      int ms_level = it.getMSLevel();
      current_max_ms_level = current_max_ms_level < ms_level ? ms_level : current_max_ms_level;

      if (min_rt > 0 && it.getRT() < min_rt)
      {
        continue;
      }
      if (max_rt > 0 && it.getRT() > max_rt)
      {
        break;
      }
      if (ensemble)
      {
        auto &espec = ensemble_map[it.getMSLevel() - 1];
        for (auto &p : it)
        {
          espec.push_back(p);
        }
      }

      if (it.getMSLevel() == 1)
      {
        ms1_cntr++;
      }
      if (it.getMSLevel() == 2)
      {
        ms2_cntr++;
      }
    }

    // Max MS Level is adjusted according to the input dataset
    current_max_ms_level = current_max_ms_level > max_ms_level ? max_ms_level : current_max_ms_level;
    // if an ensemble spectrum is analyzed, replace the input dataset with the ensemble one
    if (ensemble)
    {
      for (int i = 0; i < current_max_ms_level; ++i)
      {
        ensemble_map[i].sortByPosition();
        //spacing_difference_gap
        PeakPickerHiRes pickerHiRes;
        auto pickParam = pickerHiRes.getParameters();
        //pickParam.setValue("spacing_difference_gap", 1e-1);
        //pickParam.setValue("spacing_difference", 2.0);
        //pickParam.setValue("missing", 0);

        pickerHiRes.setParameters(pickParam);
        auto tmp_spec = ensemble_map[i];
        pickerHiRes.pick(tmp_spec, ensemble_map[i]);

        // PeakPickerCWT picker;
        // tmp_spec = ensemble_map[i];
        //picker.pick(tmp_spec, ensemble_map[i]);
      }
      //MzMLFile mzml_file;
      //mzml_file.store("/Users/kyowonjeong/Documents/A4B/Results/ensemble.mzML", ensemble_map);//

      map = ensemble_map;
    }
      // Run FLASHDeconv here

      int scan_number = 0;
      float prev_progress = .0;
      int num_last_deconvoluted_spectra = getIntOption_("preceding_MS1_count");
      const int max_num_last_deconvoluted_spectra = std::max(10, num_last_deconvoluted_spectra * 2);
      auto last_deconvoluted_spectra = std::unordered_map<UInt, std::vector<DeconvolutedSpectrum>>();
      //auto lastlast_deconvoluted_spectra = std::unordered_map<UInt, DeconvolutedSpectrum>();
#ifdef DEBUG_EXTRA_PARAMTER
      // TODO
      std::set<int> m_scans{6314, 6326, 6404, 6626, 6596, 6236, 6572, 6296, 6410, 6578, 6212, 6344, 6326, 6248, 6530,
                            6332, 6374, 6338, 6314, 6356, 6476, 6482, 6368, 6542, 6356, 6457, 6534, 6541, 6709, 6493,
                            6691, 6601, 6493, 6673, 6534, 6450, 6481, 6457, 6367, 6667, 6487, 6463, 6619, 6439, 6355,
                            6577, 6511, 6457, 6478, 6262, 6400, 6394, 6292, 6664, 6430, 6441, 6430, 6262, 6634, 6412,
                            6622, 6358, 6640, 6532, 6502, 6502, 6358, 6460, 6478, 6364, 6532, 6394, 6604, 6370, 6238,
                            9940, 10129, 10050, 9886, 10176, 10008, 10008, 10199, 9995, 9776, 10134, 9885, 10068, 9948,
                            9940, 10092, 10074, 9912, 10026, 9900, 9894, 10057, 10109, 10117, 9960, 9818, 10200, 9918,
                            10170, 9948, 9908, 10141, 10177, 10058, 10195, 10069, 9942, 10141, 10201, 10129, 9896, 9925,
                            10099, 9730, 10147, 9800, 10147, 11144, 11310, 10964, 10964, 11096, 11238, 11020, 11032,
                            11322, 11032, 11280, 10997, 11146, 11134, 11134, 11056, 10912, 11322, 11232, 11249, 11050,
                            10890, 11044, 11308, 11179, 11143, 10898, 11099, 11191, 11279, 10993, 11024, 8426, 8486,
                            7892, 7982, 7850, 8444, 8042, 8012, 8576, 7946, 8102, 8353, 8395, 8210, 8383, 7970, 7658,
                            8617, 8011, 8611, 8106, 8575, 8377, 8106, 8095, 7813, 8311, 8155, 7789, 8623, 8683, 8539,
                            8344, 8416, 7900, 8447, 8134, 8507, 8471, 7894, 7906, 7810, 7954, 8110, 7660, 7996, 6536,
                            15689, 16039, 15565, 16902, 15745, 15675, 15547, 6272, 6554, 15358, 16404, 6572, 15774,
                            16685, 15968, 16051, 15236, 16122, 15985, 15511, 15529, 15713, 15630, 16206, 6781, 15469,
                            15258, 6391, 15962, 15529, 6586, 15479, 15919, 16593, 15492, 15900, 15617, 16179, 15444,
                            16348, 6286, 15931, 15162, 16236, 15997, 16000};
#endif
      MSExperiment exp;

      auto fd = FLASHDeconvAlgorithm();
      Param fd_param = getParam_().copy("Algorithm:", true);
      //fd_param.setVa˘lue("tol", getParam_().getValue("tol"));
      if (ensemble) {
          fd_param.setValue("min_rt", .0);
          fd_param.setValue("max_rt", .0);
    }
    fd.setParameters(fd_param);
    fd.calculateAveragine(use_RNA_averagine);
    auto avg = fd.getAveragine();
    auto mass_tracer = MassFeatureTrace();
    Param mf_param = getParam_().copy("FeatureTracing:", true);
    DoubleList isotope_cosines = fd_param.getValue("min_isotope_cosine");
    //mf_param.setValue("mass_error_ppm", ms1tol);
    mf_param.setValue("noise_threshold_int", .0);
    mf_param.setValue("reestimate_mt_sd", "false");
    mf_param.setValue("trace_termination_criterion", "outlier");
    mf_param.setValue("trace_termination_outliers", 20);
    mf_param.setValue("chrom_peak_snr", .0);
    if (((double) mf_param.getValue("min_isotope_cosine")) < 0)
    {
      mf_param.setValue("min_isotope_cosine", isotope_cosines[0]);
    }
    mass_tracer.setParameters(mf_param);

    unordered_map<int, PeakGroup> precursor_peak_groups; // MS2 scan number, peak group

    OPENMS_LOG_INFO << "Running FLASHDeconv ... " << endl;

    for (auto it = map.begin(); it != map.end(); ++it)
    {
      scan_number = SpectrumLookup::extractScanNumber(it->getNativeID(),
                                                      map.getSourceFiles()[0].getNativeIDTypeAccession());
      if (it->empty())
      {
        continue;
      }

        float progress = (float) (it - map.begin()) / map.size();
        if (progress > prev_progress + .01)
        {
            printProgress_(progress);
            prev_progress = progress;
        }

      int ms_level = it->getMSLevel();
      if (ms_level > current_max_ms_level)
      {
        continue;
      }
#ifdef DEBUG_EXTRA_PARAMTER
      if(ms_level == 1){

        fi_out << "Spec\t" << it->getRT() << "\n";

        for(auto &p : *it){
          if(p.getIntensity() <= 0){
            continue;
          }

          fi_out << p.getMZ() << "\t" << p.getIntensity() << "\n";
        }
      }
     // continue;
#endif
      spec_cntr[ms_level - 1]++;
      auto deconv_begin = clock();
      auto deconv_t_start = chrono::high_resolution_clock::now();

      //auto deconvoluted_spectrum = DeconvolutedSpectrum(*it, scan_number);
      // for MS>1 spectrum, register precursor
      std::vector<DeconvolutedSpectrum> precursor_specs;

      if (ms_level > 1 && last_deconvoluted_spectra.find(ms_level - 1) != last_deconvoluted_spectra.end())
      {
        precursor_specs = (last_deconvoluted_spectra[ms_level - 1]);
      }

      std::vector<Precursor> triggeredPeaks;
      /*
      if (ms_level < current_max_ms_level)
      {
        auto tit = it + 1;
        for (; tit != map.end(); ++tit)
        {
          if (tit->getMSLevel() == ms_level)
          {
            break;
          }

          for (auto &peak: tit->getPrecursors())
          {
            triggeredPeaks.push_back(peak);
          }
        }
      }*/

      auto deconvoluted_spectrum = fd.getDeconvolutedSpectrum(*it,
                                                              triggeredPeaks,
                                                              precursor_specs,
                                                              scan_number,
                                                              num_last_deconvoluted_spectra,
                                                              precursor_map_for_real_time_acquisition);

      if(it->getMSLevel() > 1&& !deconvoluted_spectrum.getPrecursorPeakGroup().empty()){
          precursor_peak_groups[scan_number] = deconvoluted_spectrum.getPrecursorPeakGroup();
      }

      if (it->getMSLevel() == 2 && !in_train_file.empty() && !out_train_file.empty()
          && !deconvoluted_spectrum.getPrecursorPeakGroup().empty()
          )
      {
        double pmz = deconvoluted_spectrum.getPrecursor().getMZ();
        auto color = deconvoluted_spectrum.getPrecursor().getMetaValue("color");
        double pmass =
                top_pic_map[scan_number].proteform_id_ < 0 ? .0 : top_pic_map[scan_number].precursor_mass_;
        double precursor_intensity = deconvoluted_spectrum.getPrecursor().getIntensity();
        auto pg = deconvoluted_spectrum.getPrecursorPeakGroup();
        int fr = top_pic_map[scan_number].first_residue_;
        int lr = top_pic_map[scan_number].last_residue_;

          QScore::writeAttTsv(scan_number, top_pic_map[scan_number].protein_acc_, top_pic_map[scan_number].proteform_id_,
                            deconvoluted_spectrum.getOriginalSpectrum().getRT(),
                            deconvoluted_spectrum.getPrecursorScanNumber(),
                            pmass, pmz, color,
                            top_pic_map[scan_number].intensity_,
                            pg, fr,lr,
                            deconvoluted_spectrum.getPrecursorCharge(),
                            precursor_intensity, top_pic_map[scan_number].unexp_mod_,
                            top_pic_map[scan_number].proteform_id_ >= 0,
                            top_pic_map[scan_number].e_value_,
                            avg, out_train_stream, write_detail_qscore_att);

      }


      if (!out_mzml_file.empty())
      {
        if (it->getMSLevel() == 1 || !deconvoluted_spectrum.getPrecursorPeakGroup().empty())
        {
          exp.addSpectrum(deconvoluted_spectrum.toSpectrum(mzml_charge));
        }
      }
      elapsed_deconv_cpu_secs[ms_level - 1] += double(clock() - deconv_begin) / CLOCKS_PER_SEC;
      elapsed_deconv_wall_secs[ms_level - 1] += chrono::duration<double>(
          chrono::high_resolution_clock::now() - deconv_t_start).count();

      if (ms_level < current_max_ms_level)
      {
        //if(last_deconvoluted_spectra.find(ms_level) != last_deconvoluted_spectra.end())
        //{
        //  lastlast_deconvoluted_spectra[ms_level] = last_deconvoluted_spectra[ms_level];
        //}

          if (last_deconvoluted_spectra[ms_level].size() >= max_num_last_deconvoluted_spectra) {
              last_deconvoluted_spectra.erase(last_deconvoluted_spectra.begin());
          }
        last_deconvoluted_spectra[ms_level].push_back(deconvoluted_spectrum);
      }


      //if (ms_level < current_max_ms_level)
      //{
      //  last_deconvoluted_spectra[ms_level] = deconvoluted_spectrum_; // to register precursor in the future..
      //}

      if (!ensemble)
      {
        mass_tracer.storeInformationFromDeconvolutedSpectrum(deconvoluted_spectrum);// add deconvoluted mass in mass_tracer
      }

        if (deconvoluted_spectrum.empty())
        {
            continue;
        }

      qspec_cntr[ms_level - 1]++;
      mass_cntr[ms_level - 1] += deconvoluted_spectrum.size();
      if(out_spec_streams.size() > ms_level - 1)
      {
        deconvoluted_spectrum
            .writeDeconvolutedMasses(out_spec_streams[ms_level - 1], in_file, avg, write_detail);
      }
      if (out_topfd_streams.size() > ms_level - 1)
      {
        deconvoluted_spectrum.writeTopFD(out_topfd_streams[ms_level - 1], avg);
        //deconvoluted_spectrum.writeTopFD(out_topfd_streams[ms_level - 1], scan_number + 100000, avg, 2);
        //deconvoluted_spectrum.writeTopFD(out_topfd_streams[ms_level - 1], scan_number + 200000, avg, .5);
        //double precursor_offset = ((double) rand() / (RAND_MAX)) * 90 + 10; // 10 - 100
        //precursor_offset = ((double) rand() / (RAND_MAX))>.5? precursor_offset : -precursor_offset;
        //deconvoluted_spectrum.writeTopFD(out_topfd_streams[ms_level - 1], scan_number + 300000, avg, 1, precursor_offset);
      }

      //deconvoluted_spectrum_.clearPeakGroupsChargeInfo();
      //deconvoluted_spectrum_.getPrecursorPeakGroup().clearChargeInfo();

    }

    printProgress_(1); //
    std::cout << std::endl;

    // mass_tracer run
    if (!ensemble)
    {
      mass_tracer
          .findFeatures(in_file, !out_promex_file.empty(), !out_topfd_feature_file.empty(), precursor_peak_groups,
                        feature_cntr, feature_index, out_stream, out_promex_stream, out_topfd_feature_streams, fd.getAveragine());
    }
    if (!out_mzml_file.empty())
    {
      MzMLFile mzml_file;
      mzml_file.store(out_mzml_file, exp);
    }

    for (int j = 0; j < (int) current_max_ms_level; j++)
    {
      if (spec_cntr[j] == 0)
      {
        continue;
      }

      if (ensemble)
      {
        OPENMS_LOG_INFO << "So far, FLASHDeconv found " << mass_cntr[j] << " masses in the ensemble MS"
                        << (j + 1) << " spectrum" << endl;

      }
      else
      {
        OPENMS_LOG_INFO << "So far, FLASHDeconv found " << mass_cntr[j] << " masses in " << qspec_cntr[j]
                        << " MS" << (j + 1) << " spectra out of "
                        << spec_cntr[j] << endl;
      }
    }
    if (feature_cntr > 0)
    {
      OPENMS_LOG_INFO << "Mass tracer found " << feature_cntr << " features" << endl;
    }

    auto t_end = chrono::high_resolution_clock::now();
    auto end = clock();

    elapsed_cpu_secs = double(end - begin) / CLOCKS_PER_SEC;
    elapsed_wall_secs = chrono::duration<double>(t_end - t_start).count();

    OPENMS_LOG_INFO << "-- done [took " << elapsed_cpu_secs << " s (CPU), " << elapsed_wall_secs
                    << " s (Wall)] --"
                    << endl;

    int total_spec_cntr = 0;
    for (int j = 0; j < (int) current_max_ms_level; j++)
    {
      total_spec_cntr += spec_cntr[j];

      OPENMS_LOG_INFO << "-- deconv per MS" << (j + 1) << " spectrum (except spec loading, feature finding) [took "
                      << 1000.0 * elapsed_deconv_cpu_secs[j] / total_spec_cntr
                      << " ms (CPU), " << 1000.0 * elapsed_deconv_wall_secs[j] / total_spec_cntr << " ms (Wall)] --"
                      << endl;
    }

#ifdef DEBUG_EXTRA_PARAMTER

      for (auto it = map.begin(); it != map.end(); ++it) {

          scan_number = SpectrumLookup::extractScanNumber(it->getNativeID(),
                                                          map.getSourceFiles()[0].getNativeIDTypeAccession());

          if(m_scans.find(scan_number) == m_scans.end()){
              continue;
          }
          fi_m << "Spec{" << scan_number << "}=[";
          for (auto &p : *it) {
              fi_m << p.getMZ() << "," << std::setprecision(2)<< p.getIntensity() <<std::setprecision(-1)<< "\n";
          }
          fi_m << "];";

      }

      fi_out.close(); //
    fi_m.close();
#endif
    out_stream.close();

    if (!out_promex_file.empty())
    {
      out_promex_stream.close();
    }
      if (!out_topfd_feature_file.empty())
      {
          for (auto &out_topfd_feature_stream : out_topfd_feature_streams)
          {
              out_topfd_feature_stream.close();
          }
      }


    if (!out_topfd_file.empty())
    {
      for (auto &out_topfd_stream : out_topfd_streams)
      {
        out_topfd_stream.close();
      }
    }
    if(!out_spec_file.empty()){
      for(auto & out_spec_stream : out_spec_streams){
        out_spec_stream.close();
      }
    }

    if (!out_train_file.empty())
    {
      out_train_stream.close();
    }


    return EXECUTION_OK;
  }

  static void printProgress_(float progress)
  {
    float bar_width = 70;
    std::cout << "[";
    int pos = (int) (bar_width * progress);
    for (int i = 0; i < bar_width; ++i)
    {
      if (i < pos)
      {
        std::cout << "=";
      }
      else if (i == pos)
      {
        std::cout << ">";
      }
      else
      {
        std::cout << " ";
      }
    }
    std::cout << "] " << int(progress * 100.0) << " %\r";
    std::cout.flush();
  }
};

// the actual main function needed to create an executable
int main(int argc, const char **argv)
{
  TOPPFLASHDeconv tool;
  return tool.main(argc, argv);
}
