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
// $Maintainer: Jihyung Kim $
// $Authors: Jihyung Kim $
// --------------------------------------------------------------------------

#include <OpenMS/VISUAL/DIALOGS/FLASHTabWidget.h>
#include <ui_FLASHTabWidget.h>

#include <OpenMS/CONCEPT/LogStream.h>
#include <OpenMS/FORMAT/FileHandler.h>
#include <OpenMS/FORMAT/ParamXMLFile.h>
#include <OpenMS/SYSTEM/File.h>
#include <OpenMS/VISUAL/DIALOGS/TOPPASInputFilesDialog.h>
#include <OpenMS/VISUAL/MISC/GUIHelpers.h>

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QMessageBox>
#include <QProcess>
#include <QProgressDialog>
#include <QSignalBlocker>

#include <algorithm>

using namespace std;

namespace OpenMS
{
  namespace Internal
  {

    FLASHGUILock::FLASHGUILock(FLASHTabWidget* ftw)
      :
      ftw_(ftw),
      old_(ftw->currentWidget()),
      glock_(ftw)
    {
      ftw->setCurrentWidget(ftw->ui->tab_log);
    }

    FLASHGUILock::~FLASHGUILock()
    {
      ftw_->setCurrentWidget(old_);
    }

    String getFLASHDeconvExe()
    {
      return File::findSiblingTOPPExecutable("FLASHDeconv");
    }

    QString getFDDefaultOutDir()
    {
      auto dir = QDir::homePath().append("/FLASHDeconvOut");
      if (!QDir().exists(dir)) QDir().mkpath(dir);
      return dir;
    }

    FLASHTabWidget::FLASHTabWidget(QWidget* parent) :
        QTabWidget(parent),
        ui(new Ui::FLASHTabWidget),
        ep_([&](const String& out) {writeLog_(out.toQString());},
            [&](const String& out) {writeLog_(out.toQString());})
    {
      ui->setupUi(this);

      writeLog_(QString("Welcome to the Wizard!"), Qt::darkGreen, true);

      // keep the group of input widgets in sync with respect to their current-working-dir, when browsing for new files
      connect(ui->input_mzMLs, &InputFileList::updatedCWD, this, &FLASHTabWidget::broadcastNewCWD_);

      // param setting
      setWidgetsfromFDDefaultParam_();

      ui->out_dir->setDirectory(getFDDefaultOutDir());
    }

    FLASHTabWidget::~FLASHTabWidget()
    {
      delete ui;
    }

    String infileToFDoutput(const String& infile)
    {
      return FileHandler::swapExtension(File::basename(infile), FileTypes::TSV);
    }

    StringList FLASHTabWidget::getMzMLInputFiles() const
    {
      return ui->input_mzMLs->getFilenames();
    }

    void FLASHTabWidget::on_run_fd_clicked()
    {
      if (!checkFDInputReady_()) return;
      
      FLASHGUILock lock(this); // forbid user interaction

      // get parameter
      Param tmp_param;
      tmp_param.insert("FLASHDeconv:1:", flashdeconv_param_);
      String tmp_ini = File::getTemporaryFile();
      ParamXMLFile().store(tmp_ini, tmp_param);

      StringList in_mzMLs = getMzMLInputFiles();
      writeLog_(QString("Starting FLASHDeconv with %1 mzML file(s)").arg(in_mzMLs.size()), Qt::darkGreen, true);
      
      QProgressDialog progress("Running FLASHDeconv ", "Abort ...", 0, (int)in_mzMLs.size(), this);
      progress.setWindowModality(Qt::ApplicationModal);
      progress.setMinimumDuration(0); // show immediately
      progress.setValue(0);
      int step = 0;

      for (const auto& mzML : in_mzMLs)
      {
        auto r = ep_.run(this,
                         getFLASHDeconvExe().toQString(),
                         QStringList() << "-ini" << tmp_ini.toQString()
                                            << "-in" << mzML.toQString()
                                            << "-out" << getCurrentOutDir_() + "/" + infileToFDoutput(mzML).toQString(),
//                                       << "-out_chrom" << getCurrentOutDir_() + "/" + infileToChrom(mzML).toQString(),
                         "",
                         true);
        if (r != ExternalProcess::RETURNSTATE::SUCCESS) break;
        if (progress.wasCanceled()) break;
        progress.setValue(++step);
      } // mzML loop
      
      progress.close();
    }

    void FLASHTabWidget::on_edit_advanced_parameters_clicked()
    {
      // refresh 'flashdeconv_param_' from data within the Wizards controls
//      updateFLASHDeconvParamFromWidgets_();
      ui->list_editor->store();
      Param tmp_param = flashdeconv_param_;

      // show the parameters to the user
      String executable = File::getExecutablePath() + "INIFileEditor";
      String tmp_file = File::getTemporaryFile();
      ParamXMLFile().store(tmp_file, tmp_param);
      QProcess qp;
      qp.start(executable.toQString(), QStringList() << tmp_file.toQString());
      ui->tab_run->setEnabled(false); // grey out the Wizard until INIFileEditor returns...
      qp.waitForFinished(-1);
      ui->tab_run->setEnabled(true);
      ParamXMLFile().load(tmp_file, tmp_param);
      flashdeconv_param_.update(tmp_param, false);
    }

    void FLASHTabWidget::setWidgetsfromFDDefaultParam_()
    {
      // create a default INI of FLASHDeconv
      String tmp_file = File::getTemporaryFile();
      if (ep_.run(this, getFLASHDeconvExe().toQString(), QStringList() << "-write_ini" << tmp_file.toQString(), "", true) != ExternalProcess::RETURNSTATE::SUCCESS)
      {
        exit(1);
      }
      ParamXMLFile().load(tmp_file, flashdeconv_param_);
      flashdeconv_param_ = flashdeconv_param_.copy("FLASHDeconv:1:", true);

      // parameters to show in default mode : flashdeconv_param_wizard_
      flashdeconv_param_.remove("log");
      flashdeconv_param_.remove("no_progress");
      flashdeconv_param_.remove("debug");
      flashdeconv_param_.remove("in");
      flashdeconv_param_.remove("out");

      // parameters for different output format
      StringList out_params = {"out_spec", "out_mzml", "out_promex", "out_topFD", "out_topFD_feature"};
      for (const auto& name : out_params) flashdeconv_param_outputs_.setValue(name, ""); // create a dummy param, just so we can use ::copySubset
      flashdeconv_param_outputs_ = flashdeconv_param_.copySubset(flashdeconv_param_outputs_);

      // remove output format params from global parameter set
      for (const auto& name : out_params) flashdeconv_param_.remove(name);

      ui->list_editor->load(flashdeconv_param_);
    }

    QString FLASHTabWidget::getCurrentOutDir_() const
    {
      QString out_dir(ui->out_dir->dirNameValid() ?
        ui->out_dir->getDirectory() :
        getFDDefaultOutDir());
      return out_dir;
    }

    void FLASHTabWidget::writeLog_(const QString& text, const QColor& color, bool new_section)
    {
      QColor tc = ui->log_text->textColor();
      if (new_section)
      {
        ui->log_text->setTextColor(Qt::darkBlue);
        ui->log_text->append(QString(10, '#').append(QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")).append(QString(10, '#')).append("\n"));
        ui->log_text->setTextColor(tc);
      }

      ui->log_text->setTextColor(color);
      ui->log_text->append(text);
      ui->log_text->setTextColor(tc); // restore old color
    }

    void FLASHTabWidget::writeLog_(const String& text, const QColor& color, bool new_section)
    {
      writeLog_(text.toQString(), color, new_section);
    }
    
    bool FLASHTabWidget::checkFDInputReady_()
    {
      if (ui->input_mzMLs->getFilenames().empty())
      {
        QMessageBox::critical(this, "Error", "Input mzML file(s) are missing! Please provide at least one!");
        return false;
      }

      return true;
    }

    void FLASHTabWidget::broadcastNewCWD_(const QString& new_cwd)
    {
      // RAII to avoid infinite loop (setCWD signals updatedCWD which is connected to slot broadcastNewCWD_)
      QSignalBlocker blocker1(ui->input_mzMLs);
      ui->input_mzMLs->setCWD(new_cwd);
    }

    /// custom arguments to allow for looping calls
    struct Args
    {
      QStringList loop_arg; ///< list of arguments to insert; one for every loop
      size_t insert_pos;       ///< where to insert in the target argument list (index is 0-based)
    };
    
    typedef std::vector<Args> ArgLoop;

    /// Allows running an executable with arguments
    /// Multiple execution in a loop is supported by the ArgLoop argument
    /// e.g. running 'ls -la .' and 'ls -la ..'
    /// uses Command("ls", QStringList() << "-la" << "%1", ArgLoop{ Args {QStringList() << "." << "..", 1 } })
    /// All lists in loop[i].loop_arg should have the same size (i.e. same number of loops)
    struct Command
    {
      String exe;
      QStringList args;
      ArgLoop loop;

      Command(const String& e, const QStringList& a, const ArgLoop& l) :
        exe(e),
        args(a),
        loop(l) {}

      /// how many loops can we make according to the ArgLoop provided?
      /// if ArgLoop is empty, we just do a single invokation
      size_t getLoopCount() const
      {
        if (loop.empty()) return 1;
        size_t common_size = loop[0].loop_arg.size();
        for (const auto& l : loop)
        {
          if (l.loop_arg.size() != (int)common_size) throw Exception::Precondition(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "Internal error. Not all loop arguments support the same number of loops!");
          if ((int)l.insert_pos >= args.size()) throw Exception::Precondition(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "Internal error. Loop argument wants to insert after end of template arguments!");
        }
        return common_size;
      }
      /// for a given loop, return the substituted arguments
      /// @p loop_number of 0 is always valid, i.e. no loop args, just use the unmodified args provided
      QStringList getArgs(const int loop_number) const
      {
        if (loop_number >= (int)getLoopCount())
        {
          throw Exception::Precondition(__FILE__, __LINE__, OPENMS_PRETTY_FUNCTION, "Internal error. The loop number you requested is too high!");
        }
        if (loop.empty()) return args; // no looping available

        QStringList arg_l = args;
        for (const auto& largs : loop) // replace all args for the current round
        {
          arg_l[largs.insert_pos] = args[largs.insert_pos].arg(largs.loop_arg[loop_number]);
        }
        return arg_l;
      }
    };

  }   //namespace Internal
} //namspace OpenMS



