#include "fastjet/PseudoJet.hh"
#include "fastjet/contrib/Njettiness.hh"
#include "fastjet/contrib/Nsubjettiness.hh"
#include "MitCommon/MathTools/interface/MathUtils.h"
#include "MitAna/DataTree/interface/MCParticleFwd.h"
#include "MitPhysics/Init/interface/ModNames.h"
#include "MitMonoJet/Mods/interface/BoostedVTreeWriter.h"

using namespace mithep;

ClassImp(mithep::BoostedVTreeWriter)

//--------------------------------------------------------------------------------------------------
BoostedVTreeWriter::BoostedVTreeWriter(const char *name, const char *title) : 
  BaseMod                (name,title),
  fIsData                (kTRUE),
  fMcPartsName           (Names::gkMCPartBrn),
  fMcParts               (0),
  fTriggerObjsName       ("HltObjsMonoJet"),
  fTrigObjs              (0),
  fJetsName              (Names::gkPFJetBrn),
  fJetsFromBranch        (kTRUE),
  fJets                  (0),
  fPFCandidatesName      (Names::gkPFCandidatesBrn),
  fPFCandidatesFromBranch(kTRUE),
  fPFCandidates          (0),
  fJetTriggerObjs        (0),
  fConeSize              (0.8),
  fNAnalyzed             (0),
  fHistNPtBins           (100),
  fHistNEtaBins          (100),
  fHistMinPt             (0.),
  fHistMaxPt             (300.),
  fHistMinEta            (-3.),
  fHistMaxEta            (3.),
  fHistTau1Bins          (100),
  fHistTau2Bins          (100),
  fHistTau3Bins          (100),
  fHistT2ovrT1Bins       (100),
  fHistT3ovrT2Bins       (100),
  fHistMinTau1           (0.),
  fHistMinTau2           (0.),
  fHistMinTau3           (0.),
  fHistMinT2ovrT1        (0.),
  fHistMinT3ovrT2        (0.),
  fHistMaxTau1           (3.),
  fHistMaxTau2           (3.),
  fHistMaxTau3           (3.),
  fHistMaxT2ovrT1        (3.),
  fHistMaxT3ovrT2        (3.),
  fOutputName            ("BoostedVNtuple.root"),
  fOutputFile            (0)
{
  // Constructor.
}

BoostedVTreeWriter::~BoostedVTreeWriter()
{
  // Destructor
  fOutputFile->Close();
}

//--------------------------------------------------------------------------------------------------
void BoostedVTreeWriter::Process()
{
  // Load the branches we want to work with
  LoadEventObject(fJetsName,fJets,fJetsFromBranch);
  LoadEventObject(fPFCandidatesName,fPFCandidates,fPFCandidatesFromBranch);
  // Extract the jet trigger objects from all trigger objects
  GetJetTriggerObjs();

  // Initializes all variables
  fMitGPTree.InitVariables();

  // After tree is initialized we can start with MC if applicable
  if (! fIsData)
    ProcessMc();
 
  // Keep track of events analyzed
  fNAnalyzed++;

  // Loop over jets and perform Nsubjettiness analysis (for now just stick with the first jet)
  std::vector<fastjet::PseudoJet> lFjParts;
  for (UInt_t i=0; i<fJets->GetEntries(); ++i) {      
    const PFJet *jet = dynamic_cast<const PFJet*>(fJets->At(i));
    if (! jet) {
      printf(" BoostedVTreeWriter::Process() - ERROR - jets provided are not PFJets.");
      break;
    }
    
    // Figure out whether the jet is matched with one of the triggers
    fTrigObjs = GetHLTObjects(fTriggerObjsName);
    if (! fTrigObjs)
      printf(" BoostedVTreeWriter::Process() - ERROR - TriggerObjectCol not found\n");
    else {
      // loop through the stored trigger objects and find corresponding trigger name
      for (UInt_t j=0; j<fTrigObjs->GetEntries();++j) {
    	const TriggerObject *to = fTrigObjs->At(j);
    	TString trName = to->TrigName();
    	// default MonoJet
    	if (trName.Contains("MonoCentralPFJet80_PFMETnoMu"))
    	  fMitGPTree.trigger_ |= 1 << 0;
	if (trName.Contains("HLT_MET120_HBHENoiseCleaned_v"))
    	  fMitGPTree.trigger_ |= 1 << 1;
      }
    }
    
    // Push all particle flow candidates into fastjet particle collection
    for (UInt_t j=0; j<jet->NPFCands(); ++j) {      
      const PFCandidate *pfCand = jet->PFCand(j);
      lFjParts.push_back(fastjet::PseudoJet(pfCand->Px(),pfCand->Py(),pfCand->Pz(),pfCand->E()));
      lFjParts.back().set_user_index(j);
      
      // Fill the PFCand histograms
      fPFCandidatesPt ->Fill(pfCand->Pt());
      fPFCandidatesEta->Fill(pfCand->Eta());
      if (j==0) {
	fMitGPTree.eta_ = pfCand->Eta();
	fMitGPTree.phi_ = pfCand->Phi();
	fMitGPTree.pt_ =  pfCand->Pt();
      }
    }	
    break; // this is for now just considering the first jet (consider all in the future)
  }

  // ---- Fastjet is ready ----

  // Setup the cluster for fastjet
  fastjet::ClusterSequenceArea *lClustering =
    new fastjet::ClusterSequenceArea(lFjParts,*fCAJetDef,*fAreaDefinition);

  // Produce a new set of jets based on the fastjet particle collection and the defined clustering
  std::vector<fastjet::PseudoJet> lOutJets = sorted_by_pt(lClustering->inclusive_jets(0.0));
 
  // Access the 2 hardest jets as applicable
  fastjet::PseudoJet lJet1, lJet2;
  if (lOutJets.size() > 0)
    lJet1 = (*fPruner)(lOutJets[0]);
  if (lOutJets.size() > 1)
    lJet2 = (*fPruner)(lOutJets[1]);

  // ---- Fastjet is already done ----

  // Things to monitor before cuts
  fMitGPTree.nParts_ = fPFCandidates->GetEntries();
  fMitGPTree.numJets_ = lOutJets.size();

  // Skip event if no jets are found
  if (lOutJets.size() == 0) {
    if (lClustering)
      delete lClustering;
    return;
  }
    
  // Fill the CA jet 1 histograms
  fCAJetPt ->Fill(lJet1.pt());
  fCAJetEta->Fill(lJet1.eta());

  // Remove events where hardest jet is lower than 100 GeV
  if (lJet1.pt() < 100) {
    if (lClustering)
      delete lClustering;
    return;
  }
  
  // Fill up the tree
  fMitGPTree.jet1_.SetPxPyPzE( lJet1.px(), lJet1.py(), lJet1.pz(), lJet1.e() );
  fMitGPTree.jet1Pt_ = lJet1.pt();
  fMitGPTree.jet1Eta_ = lJet1.eta();
  fMitGPTree.jet1Phi_ = lJet1.phi();
  fMitGPTree.jet1M_ = lJet1.m();
  fMitGPTree.jet1Tau1_ = GetTau(lJet1,1,1);
  fMitGPTree.jet1Tau2_ = GetTau(lJet1,2,1);
  fMitGPTree.jet1Tau3_ = GetTau(lJet1,3,1);
  fMitGPTree.jet1MinTrigDr_ = MinTriggerDeltaR(fMitGPTree.jet1_);

  // Only if there is a second jet
  if (lOutJets.size() > 1) {
    fMitGPTree.jet2_.SetPxPyPzE(lJet2.px(),lJet2.py(),lJet2.pz(),lJet2.e());
    fMitGPTree.jet2Pt_ = lJet2.pt();
    fMitGPTree.jet2Eta_ = lJet2.eta();
    fMitGPTree.jet2Phi_ = lJet2.phi();
    fMitGPTree.jet2M_ = lJet2.m();
    fMitGPTree.jet2Tau1_ = GetTau(lJet2,1,1);
    fMitGPTree.jet2Tau2_ = GetTau(lJet2,2,1);
    fMitGPTree.jet2Tau3_ = GetTau(lJet2,3,1);
    fMitGPTree.jet2MinTrigDr_ = MinTriggerDeltaR(fMitGPTree.jet2_);
  }
  
  // Called once when all is done
  fMitGPTree.tree_->Fill();

  // Always cleanup
  if (lClustering)
    delete lClustering;
}

//--------------------------------------------------------------------------------------------------
void BoostedVTreeWriter::SlaveBegin()
{
  // Run startup code on the computer (slave) doing the actual analysis. Here, we just request the
  // particle flow collection branch.

  if (! fIsData) {
    ReqEventObject(fMcPartsName,fMcParts,kTRUE);
  }
  ReqEventObject(fJetsName,fJets,fJetsFromBranch);
  ReqEventObject(fPFCandidatesName,fPFCandidates,fPFCandidatesFromBranch);

  // Default pruning parameters
  fPruner          = new fastjet::Pruner(fastjet::cambridge_algorithm, 0.1, 0.5);    // CMS Default
  
  // CA constructor (fConeSize = 0.8 for CA8)
  fCAJetDef       = new fastjet::JetDefinition(fastjet::cambridge_algorithm, fConeSize);
  
  // Initialize area caculation (done with ghost particles)
  int    activeAreaRepeats = 1;
  double ghostArea         = 0.01;
  double ghostEtaMax       = 7.0;
  fActiveArea      = new fastjet::GhostedAreaSpec(ghostEtaMax,activeAreaRepeats,         ghostArea);
  fAreaDefinition  = new fastjet::AreaDefinition(fastjet::active_area_explicit_ghosts, *fActiveArea);

  // Histograms definitions
  fPFCandidatesPt  = new TH1D("hPFCandPt" ,"Hist of pf Pt"      ,fHistNPtBins ,   fHistMinPt     ,fHistMaxPt);
  fPFCandidatesEta = new TH1D("hPFCandEta","Hist of pf Eta"     ,fHistNEtaBins,   fHistMinEta    ,fHistMaxEta);
  fCAJetPt         = new TH1D("hCAJetPt"  ,"Hist of CA jets Pt" ,fHistNPtBins ,   fHistMinPt     ,fHistMaxPt);
  fCAJetEta        = new TH1D("hCAJetEta" ,"Hist of CA jets Eta",fHistNEtaBins,   fHistMinEta    ,fHistMaxEta);
  fCATau1          = new TH1D("hCATau1"   ,"Tau 1"              ,fHistTau1Bins,   fHistMinTau1   ,fHistMaxTau1);
  fCATau2          = new TH1D("hCATau2"   ,"Tau 2"              ,fHistTau2Bins,   fHistMinTau2   ,fHistMaxTau2);
  fCATau3          = new TH1D("hCATau3"   ,"Tau 3"              ,fHistTau3Bins,   fHistMinTau3   ,fHistMaxTau3);
  fCAT2ovrT1       = new TH1D("hCAT2ovrT1","Tau 2 over Tau 1"   ,fHistT2ovrT1Bins,fHistMinT2ovrT1,fHistMaxT2ovrT1);
  fCAT3ovrT2       = new TH1D("hCAT3ovrT2","Tau 3 over Tau 2"   ,fHistT3ovrT2Bins,fHistMinT3ovrT2,fHistMaxT3ovrT2);

  // Create Ntuple Tree
  fOutputFile = TFile::Open(fOutputName,"RECREATE");
  fMitGPTree.CreateTree();
  fMitGPTree.tree_->SetAutoSave(300e9);
  fMitGPTree.tree_->SetDirectory(fOutputFile);
  AddOutput(fMitGPTree.tree_);
  fMitGPTree.CreateTree(0);
  AddOutput(fMitGPTree.tree_);
}

//--------------------------------------------------------------------------------------------------
void BoostedVTreeWriter::SlaveTerminate()
{
  // Say how many events were analyzed
  printf("\n BoostedVTreeWriter::Terminate - Events analyzed: %d\n\n",fNAnalyzed);

  // Save Histograms 
  AddOutput(fPFCandidatesPt);
  AddOutput(fPFCandidatesEta);
  AddOutput(fCAJetPt);
  AddOutput(fCAJetEta);
  AddOutput(fCATau1);
  AddOutput(fCATau2);
  AddOutput(fCATau3);
  AddOutput(fCAT2ovrT1);
  AddOutput(fCAT3ovrT2);

  // Save the ntuple file
  fOutputFile->WriteTObject(fMitGPTree.tree_,fMitGPTree.tree_->GetName());
}

//--------------------------------------------------------------------------------------------------
float BoostedVTreeWriter::GetTau(fastjet::PseudoJet &iJet,int iN, float iKappa)
{
  // Calculate the tau variable for the given pseudojet
  fastjet::contrib::Nsubjettiness nSubNKT(iN,fastjet::contrib::Njettiness::onepass_kt_axes,
					  iKappa,fConeSize,fConeSize);
  
  return nSubNKT(iJet);
}

//--------------------------------------------------------------------------------------------------
void BoostedVTreeWriter::ProcessMc()
{
  // We only get here for MC, so no more checking - we will perfrom the analysis on generator level
  // and take car of filling the tree

  //printf(" BoostedVTreeWriter::ProcessMc() - entered\n");

  LoadEventObject(fMcPartsName,fMcParts);

  // Fill Fastjet with the MC particles (no pileup included here)

  int nParts = 0;
  std::vector<fastjet::PseudoJet> lFjParts;
  for (UInt_t i=0; i<fMcParts->GetEntries(); ++i) {
    const MCParticle *p = fMcParts->At(i);
    
    if (p->Status() == 1) { // just the particles that go to the detector
      nParts++;
      // Push all particles that make detector entries into fastjet particle collection
      lFjParts.push_back(fastjet::PseudoJet(p->Px(),p->Py(),p->Pz(),p->E()));
      lFjParts.back().set_user_index(i);
    }
  }

  //printf(" BoostedVTreeWriter::ProcessMc() - found particles: %d\n",nParts);

  // ---- Fastjet is ready ----

  // Setup the cluster for fastjet
  fastjet::ClusterSequenceArea *lClustering =
    new fastjet::ClusterSequenceArea(lFjParts,*fCAJetDef,*fAreaDefinition);

  // Produce a new set of jets based on the fastjet particle collection and the defined clustering
  std::vector<fastjet::PseudoJet> lOutJets = sorted_by_pt(lClustering->inclusive_jets(0.0));
 
  // Attach jets 1 and 2
  fastjet::PseudoJet lJet1, lJet2;
  if (lOutJets.size() > 0)
    lJet1 = (*fPruner)(lOutJets[0]);
  if (lOutJets.size() > 1)
    lJet2 = (*fPruner)(lOutJets[1]);

  // ---- Fastjet is already done ----

  // Things to store before cuts
  fMitGPTree.nGenParts_ = nParts;
  fMitGPTree.numGenJets_ = lOutJets.size();

  // Skip event if no jets are found
  if (lOutJets.size() == 0) {
    if (lClustering)
      delete lClustering;
    return;
  }

  // Basic cut on the first jet
  if (lJet1.pt() < 100) {
    if (lClustering)
      delete lClustering;
    return;
  }
  
  // Fill up the tree
  fMitGPTree.genJet1_.SetPxPyPzE(lJet1.px(),lJet1.py(),lJet1.pz(),lJet1.e());
  fMitGPTree.genJet1Pt_=lJet1.pt();
  fMitGPTree.genJet1Eta_=lJet1.eta();
  fMitGPTree.genJet1Phi_=lJet1.phi();
  fMitGPTree.genJet1M_=lJet1.m();
  fMitGPTree.genJet1Tau1_ = GetTau(lJet1,1,1);
  fMitGPTree.genJet1Tau2_ = GetTau(lJet1,2,1);
  fMitGPTree.genJet1Tau3_ = GetTau(lJet1,3,1);
  fMitGPTree.genJet1MinTrigDr_ = MinTriggerDeltaR(fMitGPTree.genJet1_);

  if (lOutJets.size() > 1) {
    fMitGPTree.genJet2_.SetPxPyPzE(lJet2.px(),lJet2.py(),lJet2.pz(),lJet2.e());
    fMitGPTree.genJet2Pt_=lJet2.pt();
    fMitGPTree.genJet2Eta_=lJet2.eta();
    fMitGPTree.genJet2Phi_=lJet2.phi();
    fMitGPTree.genJet2M_=lJet2.m();
    fMitGPTree.genJet2Tau1_ = GetTau(lJet2,1,1);
    fMitGPTree.genJet2Tau2_ = GetTau(lJet2,2,1);
    fMitGPTree.genJet2Tau3_ = GetTau(lJet2,3,1);
    fMitGPTree.genJet2MinTrigDr_ = MinTriggerDeltaR(fMitGPTree.genJet2_);
  }

  if (lClustering)
    delete lClustering;

  return;
}

//--------------------------------------------------------------------------------------------------
void BoostedVTreeWriter::GetJetTriggerObjs()
{
  // Pick only the jet trigger objects out of all our trigger objects

  // Make sure to initialize
  fJetTriggerObjs.clear();

  // Prepare loop
  fTrigObjs = GetHLTObjects(fTriggerObjsName);
  if (! fTrigObjs)
    printf(" BoostedVTreeWriter::Process() - ERROR - TriggerObjectCol not found\n");
  else {
    // loop through the stored trigger objects and find corresponding trigger name
    for (UInt_t j=0; j<fTrigObjs->GetEntries();++j) {
      const TriggerObject *to = fTrigObjs->At(j);
      TString trName = to->TrigName();
      // default MonoJet
      if (trName.Contains("MonoCentralPFJet80_PFMETnoMu"))
	fJetTriggerObjs.push_back(to);
    }
  }

  return;
}

//--------------------------------------------------------------------------------------------------
Double_t BoostedVTreeWriter::MinTriggerDeltaR(LorentzVector jet)
{
  Double_t dR = 999;
  for (UInt_t i=0; i<fJetTriggerObjs.size(); i++) {
    Double_t dRTest = MathUtils::DeltaR(jet,*fJetTriggerObjs[i]);
    if (dRTest<dR)
      dR = dRTest;
  }

  return dR;
}
