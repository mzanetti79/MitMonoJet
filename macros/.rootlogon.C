{
  //gDebug=1;
  gROOT->Macro("$CMSSW_BASE/src/MitAna/macros/setRootEnv.C+");
  //setRootEnv();
  sayHello();
  loadmylib("MitPhysics","Mods");
  loadmylib("MitPhysics","SelMods");
  loadmylib("MitPhysics","Skim");
  loadmylib("MitPhysics","Validation");
  loadmylib("MitPlots",  "Style");
  loadmylib("MitPlots",  "Input");
  loadmylib("MitPlots",  "Plot");

  // Mono Photon things
  loadmylib("MitMonoJet",    "SelMods");
  loadmylib("MitMonoJet",    "Mods");

  // Mono Photon macros to compile etc.
  gSystem->AddIncludePath("-I/cvmfs/cms.cern.ch/slc5_amd64_gcc462/lcg/roofit/5.32.00/include");
  gSystem->AddIncludePath("-I$CMSSW_BASE/src/");
  gSystem->AddIncludePath("-I$CMSSW_RELEASE_BASE/src/");
  gSystem->AddIncludePath("-I$CMSSW_BASE/src/MitMonoJet/SelMods/interface");
  gSystem->AddIncludePath("-I$CMSSW_BASE/src/MitMonoJet/Mods/interface");
  gSystem->AddIncludePath("-I$CMSSW_BASE/src/MitHtt/Mods/interface");
  gSystem->AddIncludePath("-I$CMSSW_BASE/src/MitAna/macros");
  gInterpreter->AddIncludePath("/cvmfs/cms.cern.ch/slc5_amd64_gcc462/lcg/roofit/5.32.00/include");
  gInterpreter->AddIncludePath(TString(gSystem->Getenv("CMSSW_BASE"))+"/src/");
  gInterpreter->AddIncludePath(TString(gSystem->Getenv("CMSSW_RELEASE_BASE"))+"/src/");
  gInterpreter->AddIncludePath(TString(gSystem->Getenv("CMSSW_BASE"))+
			       "/src/MitAna/TreeMod/interface");

  gROOT->SetMacroPath(TString(gROOT->GetMacroPath())
                      +TString(gSystem->Getenv("CMSSW_BASE"))+"/src/MitAna/macros");


  gSystem->AddIncludePath("-I$CMSSW_BASE/src/MitHgg/macros");
  gSystem->AddIncludePath("-I$CMSSW_BASE/src/MitHtt/macros");
  gInterpreter->AddIncludePath((TString(":")+TString(gSystem->Getenv("CMSSW_BASE"))+
  				TString("/src/MitMonoJet/macros")).Data());
  gInterpreter->AddIncludePath((TString(":")+TString(gSystem->Getenv("CMSSW_BASE"))+
  				TString("/src/MitHtt/macros")).Data());
  gROOT       ->SetMacroPath(TString(gROOT->GetMacroPath()) + TString(":")
  			     +TString(gSystem->Getenv("CMSSW_BASE"))+"/src/MitMonoJet/macros");
  gROOT       ->SetMacroPath(TString(gROOT->GetMacroPath()) + TString(":")
  			     +TString(gSystem->Getenv("CMSSW_BASE"))+"/src/MitHtt/macros");
  //gROOT->LoadMacro("plot.C+");
  //plot();
}
