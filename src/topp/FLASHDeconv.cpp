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
//#include <OpenMS/TRANSFORMATIONS/RAW2PEAK/PeakPickerCWT.h>

using namespace OpenMS;
using namespace std;

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
// We do not want this class to show up in the docu:
// NEED to fill this part later


class TOPPFLASHDeconv :
    public TOPPBase
{
public:
  TOPPFLASHDeconv() :
      TOPPBase("FLASHDeconv", "Ultra-fast high-quality deconvolution enables online processing of top-down MS data",
               false)
  {
  }

protected:
  //typedef FLASHDeconvHelperStructs::Parameter Parameter;

  // this function will be used to register the tool parameters
  // it gets automatically called on tool execution
  void registerOptionsAndFlags_() override
  {

    registerInputFile_("in", "<file>", "", "Input file (mzML)");
    setValidFormats_("in", ListUtils::create<String>("mzML"));

    registerInputFile_("in_train", "<file>", "", "topPIC result *prsm.tsv file for QScore training", false, true);
    setValidFormats_("in_train", ListUtils::create<String>("tsv"));

    registerOutputFile_("out", "<file>", "",
                        "output file (tsv) - feature level deconvoluted masses (or ensemble spectrum level deconvluted mass if use_ensemble_spectrum is set to 1) ");
    setValidFormats_("out", ListUtils::create<String>("tsv"));

    registerOutputFile_("out_train", "<file>", "",
                        "train result csv file for QScore training", false, true);
    setValidFormats_("out_train", ListUtils::create<String>("csv"));


    registerOutputFileList_("out_spec", "<file for MS1, file for MS2, ...>", {""},
                            "output files (tsv) - spectrum level deconvoluted masses per ms level", false);
    setValidFormats_("out_spec", ListUtils::create<String>("tsv"));

    registerOutputFile_("out_mzml", "<file>", "",
                        "mzml format output file (mzML) - spectrum level deconvoluted masses per ms level", false);
    setValidFormats_("out_mzml", ListUtils::create<String>("mzML"));

    registerOutputFile_("out_promex", "<file>", "",
                        "promex format output file (ms1ft) - only MS1 deconvoluted masses are recorded", false);
    setValidFormats_("out_promex", ListUtils::create<String>("ms1ft"), false);

    registerOutputFileList_("out_topFD", "<file for MS1, file for MS2, ...>", {""},
                            "topFD format output files (msalign) - spectrum level deconvoluted masses per ms level. The file name for MSn should end with msn.msalign to be able to be recognized by TopPIC. "
                            "For example, -out_topFD [name]_ms1.msalign [name]_ms2.msalign", false);
    setValidFormats_("out_topFD", ListUtils::create<String>("msalign"), false);

    registerIntOption_("mzml_mass_charge",
                       "<0:uncharged 1: +1 charged -1: -1 charged>",
                       0,
                       "Charge status of deconvoluted masses in mzml output",
                       false);

    setMinInt_("mzml_mass_charge", -1);
    setMaxInt_("mzml_mass_charge", 1);

    /*registerDoubleList_("tol",
                        "<ms1_tol, ms2_tol, ...>",
                        {10.0, 10.0},
                        "ppm tolerance for MS1, 2, ...  "
                        "(e.g., -tol 10.0 15.0 to specify 10.0 and 15.0 ppm for MS1 and MS2, respectively)",
                        false);
*/
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
    fd_defaults.setValue("min_charge", 1);
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
    fd_defaults.setValue("min_peaks", IntList{3, 1});
    fd_defaults.addTag("min_peaks", "advanced");
    fd_defaults.setValue("min_intensity", .0, "intensity threshold");
    fd_defaults.addTag("min_intensity", "advanced");
    fd_defaults.setValue("min_isotope_cosine",
                         DoubleList{.75, .75},
                         "cosine threshold between avg. and observed isotope pattern for MS1, 2, ... (e.g., -min_isotope_cosine 0.8 0.6 to specify 0.8 and 0.6 for MS1 and MS2, respectively)");
    //fd_defaults.addTag("min_isotope_cosine_", "advanced");

    fd_defaults.setValue("max_mass_count",
                         IntList{-1, -1},
                         "maximum mass count per spec for MS1, 2, ... (e.g., -max_mass_count_ 100 50 to specify 100 and 50 for MS1 and MS2, respectively. -1 specifies unlimited)");
    fd_defaults.addTag("max_mass_count", "advanced");


    fd_defaults.setValue("RT_window", 20.0, "RT window for MS1 deconvolution");
    fd_defaults.addTag("RT_window", "advanced");

    fd_defaults.remove("max_mass_count");
    //fd_defaults.remove("min_mass_count");

    Param mf_defaults = MassFeatureTrace().getDefaults();
    mf_defaults.setValue("mass_error_da",
                         1.5,
                         "da tolerance for feature tracing. Due to frequent isotope errer, 1.5 Da is recommended.");
    mf_defaults.remove("mass_error_ppm"); // hide entry
    mf_defaults.remove("trace_termination_criterion");
    mf_defaults.remove("reestimate_mt_sd");
    mf_defaults.remove("noise_threshold_int");
    mf_defaults.remove("min_sample_rate");
    mf_defaults.remove("trace_termination_outliers"); // hide entry
    mf_defaults.setValue("min_trace_length", 10.0, "min feature trace length in second");//
    mf_defaults.setValue("quant_method", "area", "");
    mf_defaults.addTag("quant_method", "advanced"); // hide entry
    mf_defaults.setValue("min_isotope_cosine", -1.0, "if not set, controlled by -Algorithm:min_isotope_cosine_ option");
    mf_defaults.addTag("min_isotope_cosine", "advanced");

    Param combined;
    combined.insert("Algorithm:", fd_defaults);
    combined.insert("FeatureTracing:", mf_defaults);
    registerFullParam_(combined);
  }

  // the main_ function is called after all parameters are read
  ExitCodes main_(int, const char **) override
  {
    OPENMS_LOG_INFO << "Initializing ... " << endl;

    //-------------------------------------------------------------
    // parsing parameters
    //-------------------------------------------------------------

    String in_file = getStringOption_("in");
    String out_file = getStringOption_("out");
    String in_train_file = getStringOption_("in_train");
    String out_train_file = getStringOption_("out_train");
    auto out_spec_file = getStringList_("out_spec");
    String out_mzml_file = getStringOption_("out_mzml");
    String out_promex_file = getStringOption_("out_promex");
    auto out_topfd_file = getStringList_("out_topFD");

    bool use_RNA_averagine = getIntOption_("use_RNA_averagine") > 0;
    int max_ms_level = getIntOption_("max_MS_level");
    bool ensemble = getIntOption_("use_ensemble_spectrum") > 0;
    bool write_detail = getIntOption_("write_detail") > 0;
    int mzml_charge = getIntOption_("mzml_mass_charge");
    double min_rt = getDoubleOption_("Algorithm:min_rt");
    double max_rt = getDoubleOption_("Algorithm:max_rt");

    fstream out_stream, out_train_stream, out_promex_stream;
    std::vector<fstream> out_spec_streams, out_topfd_streams;

    // fstream fi_out;
    // fi_out.open(in_file+".txt", fstream::out); //

    out_stream.open(out_file, fstream::out);
    MassFeatureTrace::writeHeader(out_stream);

    if (!out_promex_file.empty())
    {
      out_promex_stream.open(out_promex_file, fstream::out);
    }
    if (!out_topfd_file.empty())
    {
      out_topfd_streams = std::vector<fstream>(out_topfd_file.size());
      for (int i = 0; i < out_topfd_file.size(); i++)
      {
        out_topfd_streams[i].open(out_topfd_file[i], fstream::out);
      }
    }
    if(!out_spec_file.empty()){
      out_spec_streams = std::vector<fstream>(out_spec_file.size());
      for(int i=0; i < out_spec_file.size(); i++){
        out_spec_streams[i].open(out_spec_file[i], fstream::out);
        DeconvolutedSpectrum::writeDeconvolutedMassesHeader(out_spec_streams[i], i + 1, write_detail);
      }
    }

    std::unordered_map<int, double> train_scan_numbers;
    std::unordered_map<int, String> train_scan_accessions;
    if (!in_train_file.empty() && !out_train_file.empty())
    {
      out_train_stream.open(out_train_file, fstream::out);
      QScore::writeAttHeader(out_train_stream);
      std::ifstream in_trainstream(in_train_file);
      String line;
      bool start = false;
      while (std::getline(in_trainstream, line))
      {
        if (line.rfind("Data file name", 0) == 0){
          start = true;
          continue;
        }
        if (!start)
        {
          continue;
        }
        vector<String> results;
        stringstream tmp_stream(line);
        String str;
        while (getline(tmp_stream, str, '\t'))
        {
          results.push_back(str);
        }
        String acc = results[13];
        int first = acc.find("|");
        int second = acc.find("|", first + 1);
        train_scan_accessions[std::stoi(results[4])] = acc.substr(first + 1, second - first - 1);
        //std::cout<<acc.substr(first+1, second - first)<<std::endl;
        train_scan_numbers[std::stoi(results[4])] = std::stod(results[9]);
      }
      in_trainstream.close();
    }


    /*std::map<int, std::unordered_map<double, int>> tmp_map; // ms 1 scan, m/z, charge
    std::map<int, std::unordered_map<double, double>> tmp_map2;// ms 1 scan, m/z, mono m/z
    if(false)
    {
      std::ifstream instream("/Users/kyowonjeong/Documents/A4B/Flash-2020-12-13-03-58-53.log");//Flash-2020-12-13-15-25-28  Flash-2020-12-13-03-58-53
      String line;
      int cms1 = 0;
      while (std::getline(instream, line))
      {
        if(line.hasPrefix("RECD FTMS MS1 Scan #")){
          auto st = line.find_first_of('#');
          auto ed = line.find_first_of(';');
          auto n = line.substr(st + 1, ed);
          cms1 = atoi(n.c_str());
          tmp_map[cms1] = std::unordered_map<double, int>();
       //   tmp_map2[cms1] = std::unordered_map<double, double>();
        }

        if(line.hasPrefix("ADD m/z")){
          //ADD m/z 1026.9316/3.59 (6+)
          auto st = 7;
          auto ed= line.find_first_of('/', st);
          auto n = line.substr(st + 1, ed);
          auto mz = atof(n.c_str());
          st = ed;
          ed = line.find_first_of('(', st);
          n = line.substr(st + 1, ed);
          auto width = atof(n.c_str());
          st = ed;
          ed = line.find_first_of('+', st);
          n = line.substr(st + 1, ed);
          auto c = atoi(n.c_str());
          tmp_map[cms1][mz] = c;
         // auto ma = (mz - (width/2.0) + 1.2);
       //   tmp_map2[cms1][mz] = ma;
        }
      }
      instream.close();
    }
*/
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
      //std::cout<<min_rt<<" " << it.getRT() <<" " << max_rt<<std::endl;
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
        pickParam.setValue("spacing_difference_gap", 1e-2);
        //pickParam.setValue("spacing_difference", .0001);
        //pickParam.setValue("missing", 0);

        pickerHiRes.setParameters(pickParam);
        auto tmp_spec = ensemble_map[i];
        pickerHiRes.pick(tmp_spec, ensemble_map[i]);

        //std::cout<<ensemble_map[i].size()<<std::endl;
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
    int const num_last_deconvoluted_spectra = 10;
    auto last_deconvoluted_spectra = std::unordered_map<UInt, std::vector<DeconvolutedSpectrum>>();
    //auto lastlast_deconvoluted_spectra = std::unordered_map<UInt, DeconvolutedSpectrum>();

    MSExperiment exp;

    auto fd = FLASHDeconvAlgorithm();
    Param fd_param = getParam_().copy("Algorithm:", true);
    //fd_param.setVa˘lue("tol", getParam_().getValue("tol"));
    if (ensemble)
    {
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
    mf_param.setValue("noise_threshold_int", .0, "");
    mf_param.setValue("reestimate_mt_sd", "false", "");
    mf_param.setValue("trace_termination_criterion", "outlier");
    mf_param.setValue("trace_termination_outliers", 20);
    //mf_param.setValue("min_charge_cosine", fd_param.getValue("min_charge_cosine"));
    if (((double)mf_param.getValue("min_isotope_cosine")) < 0)
    {
      mf_param.setValue("min_isotope_cosine", isotope_cosines[0]);
    }
    mass_tracer.setParameters(mf_param);
    //std::cout<<mass_tracer.getParameters()<<std::endl;

    OPENMS_LOG_INFO << "Running FLASHDeconv ... " << endl;

    for (auto it = map.begin(); it != map.end(); ++it)
    {
      scan_number = SpectrumLookup::extractScanNumber(it->getNativeID(),
                                                      map.getSourceFiles()[0].getNativeIDTypeAccession());
      if (it->empty())
      {
        continue;
      }

      int ms_level = it->getMSLevel();
      if (ms_level > current_max_ms_level)
      {
        continue;
      }

      if(ms_level == 1){
        // fi_out<<"Spec\t" <<it->getRT()<<"\n";
        for(auto &p : *it){
          if(p.getIntensity() <= 0){
            continue;
          }
          //  fi_out << p.getMZ() << "\t" << p.getIntensity()<<"\n";
        }
      }

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
      /*
            if(!tmp_map.empty() && ms_level > 1 ){
              auto pmz = it->getPrecursors()[0].getMZ();
              for(int sn = scan_number; sn > scan_number - 100;sn--){
                if(tmp_map.find(sn) == tmp_map.end()){
                  continue;
                }
                auto stmp = tmp_map[sn];// m/z, charge
                bool set = false;
                for(auto st : stmp){
                  if(abs(st.first - pmz) > 0.1){
                    continue;
                  }
                  it->getPrecursors()[0].setCharge(st.second);
                  //std::cout<<it->getPrecursors()[0].getCharge()<<std::endl;
                  set = true;
                  break;
                }
                if(set){
                  break;
                }
              }
            }
                    */        // per spec deconvolution
      auto deconvoluted_spectrum = fd.getDeconvolutedSpectrum(*it, precursor_specs, scan_number);

      if (it->getMSLevel() == 2 && !in_train_file.empty() && !out_train_file.empty()
        //&& !deconvoluted_spectrum.getPrecursorPeakGroup().empty()
          )
      {

        double pmz = deconvoluted_spectrum.getPrecursor().getMZ();
        double pmass =
            train_scan_numbers.find(scan_number) == train_scan_numbers.end() ? .0 : train_scan_numbers[scan_number];
        QScore::writeAttTsv(train_scan_accessions[scan_number],
                            deconvoluted_spectrum.getOriginalSpectrum().getRT(), pmass, pmz,
                            deconvoluted_spectrum.getPrecursorPeakGroup(),
                            deconvoluted_spectrum.getPrecursorCharge(),
                            train_scan_numbers.find(scan_number) != train_scan_numbers.end(), avg, out_train_stream);

      }
      if (!out_mzml_file.empty())
      {
        exp.addSpectrum(deconvoluted_spectrum.toSpectrum(mzml_charge));
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

        if(last_deconvoluted_spectra[ms_level].size() >= num_last_deconvoluted_spectra){
          last_deconvoluted_spectra.erase(last_deconvoluted_spectra.begin());
        }
        last_deconvoluted_spectra[ms_level].push_back(deconvoluted_spectrum);
      }

      if (deconvoluted_spectrum.empty())
      {
        continue;
      }
      //if (ms_level < current_max_ms_level)
      //{
      //  last_deconvoluted_spectra[ms_level] = deconvoluted_spectrum_; // to register precursor in the future..
      //}

      if (!ensemble)
      {
        mass_tracer.storeInformationFromDeconvolutedSpectrum(deconvoluted_spectrum);// add deconvoluted mass in mass_tracer
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
        deconvoluted_spectrum.writeTopFD(out_topfd_streams[ms_level - 1], scan_number, avg);
      }

      //deconvoluted_spectrum_.clearPeakGroupsChargeInfo();
      //deconvoluted_spectrum_.getPrecursorPeakGroup().clearChargeInfo();
      float progress = (float) (it - map.begin()) / map.size();
      if (progress > prev_progress + .01)
      {
        printProgress_(progress);
        prev_progress = progress;
      }
    }

    printProgress_(1); //
    std::cout << std::endl;

    // mass_tracer run
    if (!ensemble)
    {
      mass_tracer
          .findFeatures(in_file, !out_promex_file.empty(), feature_cntr, feature_index, out_stream, out_promex_stream, fd.getAveragine());
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
                      << " ms (CPU), " << 1000.0 * elapsed_deconv_wall_secs[j] / total_spec_cntr << " ms (Wall)] --" << endl;
    }

    // fi_out.close(); //

    out_stream.close();

    if(!out_promex_file.empty()){
      out_promex_stream.close();
    }
    if(!out_topfd_file.empty()){
      for(auto & out_topfd_stream : out_topfd_streams){
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
