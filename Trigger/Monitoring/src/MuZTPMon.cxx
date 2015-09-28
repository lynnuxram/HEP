/**    @Afile HLTMuonMonTool.cxx
 *   
 *    authors: Marilyn Marx (marx@cern.ch)
 */

#include "GaudiKernel/IJobOptionsSvc.h"
#include "AthenaMonitoring/AthenaMonManager.h"
#include "AthenaMonitoring/ManagedMonitorToolTest.h"
#include "EventInfo/EventInfo.h"
#include "EventInfo/EventID.h"
#include "TrigMuonEvent/MuonFeature.h"
#include "TrigMuonEvent/CombinedMuonFeature.h"
#include "TrigMuonEvent/CombinedMuonFeatureContainer.h"
#include "TrigMuonEvent/TrigMuonEFContainer.h"
#include "TrigMuonEvent/TrigMuonEFInfoContainer.h"
#include "TrigMuonEvent/TrigMuonEFInfoTrackContainer.h"
#include "TrigMuonEvent/TrigMuonEF.h"
#include "TrigMuonEvent/TrigMuonEFInfo.h"
#include "TrigMuonEvent/TrigMuonEFTrack.h"
#include "TrigMuonEvent/TrigMuonEFCbTrack.h"
#include "TrigDecisionTool/TrigDecisionTool.h"
#include "TrigDecisionTool/ChainGroup.h"
#include "TrigDecisionTool/FeatureContainer.h"
#include "TrigDecisionTool/Feature.h"
#include "TrigDecisionTool/TDTUtilities.h"
#include "TrigObjectMatching/TrigMatchTool.h"
#include "VxVertex/VxContainer.h"
#include "AnalysisTriggerEvent/LVL1_ROI.h"
#include "AnalysisTriggerEvent/Muon_ROI.h"
#include "muonEvent/Muon.h"
#include "muonEvent/MuonContainer.h"

#include "TROOT.h"
#include "TH1I.h"
#include "TH1F.h"
#include "TH2I.h"
#include "TH2F.h"
#include "TGraphAsymmErrors.h"

#include "CLHEP/Units/PhysicalConstants.h"
#include "CLHEP/Units/SystemOfUnits.h"
#include <vector>
#include <iostream>
#include <fstream>
#include <cmath>

#include "TrigMuonMonitoring/HLTMuonMonTool.h"
#include "GaudiKernel/ToolHandle.h"

//for GetKalmanUpdator
#include "GaudiKernel/ListItem.h"

class TrigMatchTool;

using namespace std;


bool CheckMuonTriggerMatch(float off_eta, float off_phi, std::vector<float> on_eta, std::vector<float> on_phi);
float CalcdeltaR(float off_eta, float off_phi,float on_eta, float on_phi);


StatusCode HLTMuonMonTool::initMuZTPDQA()
{
  return StatusCode::SUCCESS;
}

StatusCode HLTMuonMonTool::bookMuZTPDQA( bool isNewEventsBlock, bool isNewLumiBlock, bool isNewRun )
{
  if( isNewRun ){

    for(std::map<std::string, std::string>::iterator itmap=m_ztpmap.begin();itmap!=m_ztpmap.end();++itmap){
      
      std::string histdirmuztp="HLT/MuonMon/MuZTP/"+itmap->first;
      double xbins[3] = {0.,1.05,2.7};
      //mass

      addHistogram( new TH1F(("muZTP_invmass_nomasswindow_" + itmap->first).c_str(), "Invariant mass", 20, 0.0, 120000.0 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_invmass_" + itmap->first).c_str(), "Invariant mass", 20, 0.0, 120000.0 ), histdirmuztp );
      //all probes
      addHistogram( new TH1F(("muZTP_Pt_" + itmap->first).c_str(), "Probe muon p_{T}", 20, 0.0, 100.0 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Pt_EC_" + itmap->first).c_str(), "Probe muon p_{T} EC", 20, 0.0, 100.0 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Pt_B_" + itmap->first).c_str(), "Probe muon p_{T} B", 20, 0.0, 100.0 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Eta_" + itmap->first).c_str(), "Probe muon #eta", 20, -2.7, 2.7 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Eta_1bin_" + itmap->first).c_str(), "Probe muon #eta", 1, 0.0, 2.7 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Eta_2bins_" + itmap->first).c_str(), "Probe muon #eta", 2, xbins), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Eta_1bin_cut_" + itmap->first).c_str(), "Probe muon #eta", 1, 0.0, 2.7 ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Eta_2bins_cut_" + itmap->first).c_str(), "Probe muon #eta", 2, xbins), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Phi_" + itmap->first).c_str(), "Probe muon #phi", 20, -CLHEP::pi, CLHEP::pi), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Phi_EC_" + itmap->first).c_str(), "Probe muon #phi", 20, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
      addHistogram( new TH1F(("muZTP_Phi_B_" + itmap->first).c_str(), "Probe muon #phi", 20, -CLHEP::pi, CLHEP::pi), histdirmuztp );
      //fired probes
      std::vector<std::string> level;
      level.push_back("L1");
      level.push_back("L2");
      level.push_back("EF");
      for(unsigned int j=0;j<level.size();j++){
	addHistogram( new TH1F(("muZTP_Pt_"+level[j]+"fired_" + itmap->first).c_str(), ("p_{T} (fired "+level[j]+")").c_str(), 20, 0.0, 100.0 ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Pt_EC_"+level[j]+"fired_" + itmap->first).c_str(), ("p_{T} EC (fired "+level[j]+")").c_str(), 20, 0.0, 100.0 ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Pt_B_"+level[j]+"fired_" + itmap->first).c_str(), ("p_{T} B (fired "+level[j]+")").c_str(), 20, 0.0, 100.0 ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Eta_"+level[j]+"fired_" + itmap->first).c_str(), ("#eta (fired "+level[j]+")").c_str(), 20, -2.7, 2.7 ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Eta_1bin_"+level[j]+"fired_" + itmap->first).c_str(), ("#eta (fired "+level[j]+")").c_str(), 1, 0.0, 2.7 ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Eta_2bins_"+level[j]+"fired_" + itmap->first).c_str(), ("#eta (fired "+level[j]+")").c_str(), 2, xbins), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Eta_1bin_cut_"+level[j]+"fired_" + itmap->first).c_str(), ("#eta (fired "+level[j]+") with p_{T} cut").c_str(), 1, 0.0, 2.7 ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Eta_2bins_cut_"+level[j]+"fired_" + itmap->first).c_str(), ("#eta (fired "+level[j]+")with p_{T} cut").c_str(), 2, xbins), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Phi_"+level[j]+"fired_" + itmap->first).c_str(), ("#phi (fired "+level[j]+")").c_str(), 20, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Phi_EC_"+level[j]+"fired_" + itmap->first).c_str(), ("#phi EC (fired "+level[j]+")").c_str(), 20, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
	addHistogram( new TH1F(("muZTP_Phi_B_"+level[j]+"fired_" + itmap->first).c_str(), ("#phi B (fired "+level[j]+")").c_str(), 20, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
      }
      //2D eta-phi
      addHistogram( new TH2F(("muZTP_EtaPhi_all_" + itmap->first).c_str(), "Eta/phi all", 16, -2.7, 2.7, 16, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
      addHistogram( new TH2F(("muZTP_EtaPhi_L1_" + itmap->first).c_str(), "Eta/phi L1", 16, -2.7, 2.7, 16, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
      addHistogram( new TH2F(("muZTP_EtaPhi_L2_" + itmap->first).c_str(), "Eta/phi L2", 16, -2.7, 2.7, 16, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
      addHistogram( new TH2F(("muZTP_EtaPhi_EF_" + itmap->first).c_str(), "Eta/phi EF", 16, -2.7, 2.7, 16, -CLHEP::pi, CLHEP::pi ), histdirmuztp );
      //2D eta-phi efficiency
      TH2F *muZTP_eff_EtaPhi_L1 = (TH2F *)hist2(("muZTP_EtaPhi_L1_" + itmap->first).c_str(), histdirmuztp)->Clone();
      muZTP_eff_EtaPhi_L1->SetName(("muZTP_eff_EtaPhi_L1_"+ itmap->first).c_str());
      muZTP_eff_EtaPhi_L1->Sumw2();
      addHistogram(muZTP_eff_EtaPhi_L1, histdirmuztp );
      TH2F *muZTP_eff_EtaPhi_L2 = (TH2F *)hist2(("muZTP_EtaPhi_L2_" + itmap->first).c_str(), histdirmuztp)->Clone();
      muZTP_eff_EtaPhi_L2->SetName(("muZTP_eff_EtaPhi_L2_"+ itmap->first).c_str());
      muZTP_eff_EtaPhi_L2->Sumw2();
      addHistogram(muZTP_eff_EtaPhi_L2, histdirmuztp );
      TH2F *muZTP_eff_EtaPhi_EF = (TH2F *)hist2(("muZTP_EtaPhi_EF_" + itmap->first).c_str(), histdirmuztp)->Clone();
      muZTP_eff_EtaPhi_EF->SetName(("muZTP_eff_EtaPhi_EF_"+ itmap->first).c_str());
      muZTP_eff_EtaPhi_EF->Sumw2();
      addHistogram(muZTP_eff_EtaPhi_EF, histdirmuztp );
      //BayesDivide
      std::vector<std::string> var;
      var.push_back("_Pt_");
      var.push_back("_Pt_EC_");
      var.push_back("_Pt_B_");
      var.push_back("_Eta_");
      var.push_back("_Phi_");
      var.push_back("_Phi_EC_");
      var.push_back("_Phi_B_");
      var.push_back("_Eta_1bin_");
      var.push_back("_Eta_2bins_");
      var.push_back("_Eta_1bin_cut_");
      var.push_back("_Eta_2bins_cut_");
      for(unsigned int k=0;k<var.size();k++){
	for(unsigned int l=0;l<level.size();l++){
	  TGraphAsymmErrors* g = new TGraphAsymmErrors();
	  g->SetName(("muZTP_eff_"+level[l]+var[k]+itmap->first).c_str());
	  g->SetMarkerStyle(22);
	  g->SetMinimum(0.0);
	  g->SetMaximum(1.05);
	  addGraph( g, histdirmuztp );
	}//level
      }
      for(int m=0;m<7;m++){
	std::vector<std::string> ratio;
	ratio.push_back("EFwrtL2");
	ratio.push_back("EFwrtL1");
	ratio.push_back("L2wrtL1");
	for(int n=0;n<3;n++){
	  TGraphAsymmErrors* g = new TGraphAsymmErrors();
	  g->SetName(("muZTP_eff_"+ratio[n]+var[m]+itmap->first).c_str());
	  g->SetMarkerStyle(22);
	  g->SetMinimum(0.0);
	  g->SetMaximum(1.05);
	  addGraph( g, histdirmuztp );
	}//ratio
      }//var
    }//trigger vector
    
  }else if( isNewLumiBlock ){
  }else if( isNewEventsBlock ){
  }
  return StatusCode::SUCCESS;
}

StatusCode HLTMuonMonTool::fillMuZTPDQA()
{
  hist("Common_Counter", histdir )->Fill((float)MUZTP);

  //RETRIEVE Vertex Container
  const VxContainer* VertexContainer=0;
  StatusCode sc_ztp = m_storeGate->retrieve(VertexContainer,"VxPrimaryCandidate");
  if(sc_ztp.isFailure()) {
    ATH_MSG_ERROR("VxPrimaryCandidate" << " Container Unavailable");
    return StatusCode::SUCCESS;
  }

  //REQUIREMENTS FOR GOOD PRIMARY VERTEX   
  bool HasGoodPV = false;
  VxContainer::const_iterator vertexIter;
  for(vertexIter=VertexContainer->begin(); vertexIter!=VertexContainer->end(); ++vertexIter){
  if ((*vertexIter)->vxTrackAtVertex()->size()>2 && fabs((*vertexIter)->recVertex().position().z()) < 150) HasGoodPV = true;                                             
  }
  if (HasGoodPV == false){
    return StatusCode::SUCCESS;
  }

  // ---------------------------------------------------------------

  ATH_MSG_DEBUG(" ===== START HLTMuon muZTP MonTool ===== ");
  
  // ---------------------------------------------------------------
  
  //LOOP OVER ALL TRIGGER CHAINS
  for(std::map<std::string, std::string>::iterator itmap=m_ztpmap.begin();itmap!=m_ztpmap.end();++itmap){

    std::string histdirmuztp="HLT/MuonMon/MuZTP/"+itmap->first;
    double m_ptcut=999999.;
    map<std::string, double>::iterator itptcut = m_ztpptcut.find(itmap->first);
    if(itptcut!=m_ztpptcut.end())m_ptcut=itptcut->second;
    
    bool isMGchain=false;
    bool isMSonlychain = false;
    size_t found;
    found = itmap->first.find("MSonly");
    if(found != string::npos) isMSonlychain = true;
    found = itmap->first.find("MG");
    if(found != string::npos) isMGchain = true;
    
    //CHECK IF TRIGGER HAS FIRED EVENT TO START
    bool isTriggered_L1 = false;
    const Trig::ChainGroup* ChainGroupL1 = getTDT()->getChainGroup(itmap->second);
    isTriggered_L1 = ChainGroupL1->isPassed();
    if (isTriggered_L1 == false){
      return StatusCode::SUCCESS;
    } 
    bool isTriggered_L2 = false;
    const Trig::ChainGroup* ChainGroupL2 = getTDT()->getChainGroup("L2_"+itmap->first);
    isTriggered_L2 = ChainGroupL2->isPassed();
    
    bool isTriggered_EF = false;
    const Trig::ChainGroup* ChainGroupEF = getTDT()->getChainGroup("EF_"+itmap->first);
    isTriggered_EF = ChainGroupEF->isPassed();

    

    ///////////////////////////////////// get ONLINE Objects //////////////////////////////////////////

    Trig::FeatureContainer fL1 = getTDT()->features(itmap->second);
    Trig::FeatureContainer fL2 = getTDT()->features("L2_"+itmap->first);
    Trig::FeatureContainer fEF = getTDT()->features("EF_"+itmap->first);
    
    std::vector<Trig::Combination>::const_iterator jL1;    
    std::vector<Trig::Combination>::const_iterator jL2;    
    std::vector<Trig::Combination>::const_iterator jEF;    
        
    int iroiL1=1;
    int iroiL2=1;
    int iroiEF=1;

    std::vector<float> EFCbpt, EFCbeta, EFCbphi;
    std::vector<float> EFExpt, EFExeta, EFExphi;
    std::vector<float> L2Cbpt, L2Cbeta, L2Cbphi;
    std::vector<float> L2Expt, L2Exeta, L2Exphi;
    std::vector<float> L1pt, L1eta, L1phi;

    /////// L1 /////////////
    for(jL1=fL1.getCombinations().begin() ; jL1!=fL1.getCombinations().end() ; ++jL1, ++iroiL1){
      std::vector<Trig::Feature<Muon_ROI> >  muonL1Feature = jL1->get<Muon_ROI>();
      const Muon_ROI* muonL1 = muonL1Feature.at(0).cptr(); 
      if(muonL1Feature.size()!=1){
	ATH_MSG_DEBUG( "Vector of InfoContainers size is not 1" );
      }
      if(!muonL1){
	  ATH_MSG_DEBUG( "No L1Muon track found" );
	}else{
	  ATH_MSG_DEBUG( " L1Muon exists " );	 
	  L1pt.push_back(muonL1->pt());
	  L1eta.push_back(muonL1->eta());
	  L1phi.push_back(muonL1->phi());
      }
    }

    /////// L2 /////////////
    for(jL2=fL2.getCombinations().begin() ; jL2!=fL2.getCombinations().end() ; ++jL2, ++iroiL2){
      if(!isMSonlychain){
	//muComb
	std::vector<Trig::Feature<CombinedMuonFeature> > muCombL2Feature = jL2->get<CombinedMuonFeature>(); 
	const CombinedMuonFeature* muCombL2 = muCombL2Feature.at(0).cptr();
	if(!muCombL2){
	  ATH_MSG_DEBUG( "No muComb track found" );
	}else{
	  ATH_MSG_DEBUG( " muComb muon exists " );	
	  L2Cbpt.push_back(muCombL2->pt());
	  L2Cbeta.push_back(muCombL2->eta());
	  L2Cbphi.push_back(muCombL2->phi());
	}
      }
      //muFast
      std::vector<Trig::Feature<MuonFeature> > muFastL2Feature = jL2->get<MuonFeature>(); 
      const MuonFeature* muFastL2 = muFastL2Feature.at(0).cptr();
      if(!muFastL2){
	ATH_MSG_DEBUG( "No mufast track found" );
      }else{
	ATH_MSG_DEBUG( " muFast muon exists " );
	L2Expt.push_back(muFastL2->pt());
	L2Exeta.push_back(muFastL2->eta());
	L2Exphi.push_back(muFastL2->phi());
      }
    }

    /////// EF /////////////
    for(jEF=fEF.getCombinations().begin() ; jEF!=fEF.getCombinations().end() ; ++jEF, ++iroiEF){
      std::vector<Trig::Feature<TrigMuonEFInfoContainer> > muef_info_all = jEF->get<TrigMuonEFInfoContainer>();
      const TrigMuonEFInfoContainer* thecont = muef_info_all.at(0).cptr(); 
      if(muef_info_all.size()!=1) ATH_MSG_DEBUG( "Vector of InfoContainers size is not 1" );

      //fill Object with all the info you need
      int iObj=1;
      for ( TrigMuonEFInfoContainer::const_iterator mit = thecont->begin(); mit != thecont->end(); ++mit, ++iObj) { 	
	const TrigMuonEFInfoTrackContainer* muonEFInfoTrackCont = (*mit)->TrackContainer();
	TrigMuonEFInfoTrackContainer::const_iterator trit;       
	int iTrack    = 0; // all tracks
	for ( trit = muonEFInfoTrackCont->begin(); trit != muonEFInfoTrackCont->end(); ++trit, ++iTrack ) {
	  //EXTRAPOLATED TRACK	  
	  if((*trit)->hasExtrapolatedTrack()){
	    TrigMuonEFTrack*  muonEFTrack=(*trit)->ExtrapolatedTrack();
	    if(!muonEFTrack){
	      ATH_MSG_DEBUG( "No MuonEF Extrapolated track found" );
	    }else{
	      ATH_MSG_DEBUG( " ExtrapolatedTrack exists " );	    
	      if (muonEFTrack->iPt()!=0.) {
		EFExpt.push_back(muonEFTrack->pt());
		EFExphi.push_back(muonEFTrack->phi());
		EFExeta.push_back(muonEFTrack->eta());
	      }
	    }
	  }//extrapolated
	  //COMBINED TRACK
	  if ((*trit)->hasCombinedTrack()) {
	    TrigMuonEFCbTrack*   muonEFCbTrack = (*trit)->CombinedTrack();
	    if (!muonEFCbTrack) {
	      ATH_MSG_DEBUG( "No MuonEF Combined track found" );
	    }else{
	      ATH_MSG_DEBUG( " CombinedTrack exists " );
	      if (muonEFCbTrack->iPt()!=0.) {
		EFCbpt.push_back(muonEFCbTrack->pt());
		EFCbphi.push_back(muonEFCbTrack->phi());
		EFCbeta.push_back(muonEFCbTrack->eta());	    	      
	      }
	    }
	  }//combined
	}//TrackContainer Loop
      }// end of TrigMuonEFInfoContainer loop      
    }//roi loop
 


    ///////////////////////////////////// get OFFLINE Objects //////////////////////////////////////////
   
    int n_RecCBmuon=0;
    std::vector<float> RecCBpt, RecCBeta, RecCBphi;
    std::vector<float> RecCBpx, RecCBpy, RecCBpz, RecCBe, RecCBcharge;
    std::vector<bool> passedchainL1;
    std::vector<bool> passedSAchainL2;
    std::vector<bool> passedCBchainL2;
    std::vector<bool> passedSAchainEF;
    std::vector<bool> passedCBchainEF;

    // Retrieve Muon Offline Reco MuidMuonCollection
    const Analysis::MuonContainer* muonCont=0;
    
    std::string muonKey = "MuidMuonCollection";
    StatusCode sc = m_storeGate->retrieve( muonCont, muonKey);
    if(sc.isFailure() || !muonCont){
      ATH_MSG_WARNING( "Container of muon particle with key " << muonKey << " not found in Store Gate with size " );
      return StatusCode::SUCCESS;
    } else {
      ATH_MSG_DEBUG(  "Container of muon particle with key found in Store Gate with size " << muonCont->size() );
      Analysis::MuonContainer::const_iterator contItr  = muonCont->begin();
      Analysis::MuonContainer::const_iterator contItrE = muonCont->end();
      
      for (; contItr != contItrE; contItr++){
	
	const Analysis::Muon* recMuon = *contItr;
	float pt  = - 999999.;
	float px  = - 999999.;
	float py  = - 999999.;
	float pz  = - 999999.;
	float e = -999.;
	float eta = -999.;
	float phi = -999.;
	float charge = -999.;
	
	ATH_MSG_DEBUG(  "check CB muon " );
	if ((*contItr)->combinedMuonTrackParticle()!=0 ){
	  
	  ATH_MSG_DEBUG(  "CB muon found" );
	  
	  n_RecCBmuon++;
	  const Rec::TrackParticle* muidCBMuon = recMuon->combinedMuonTrackParticle();
	  pt = muidCBMuon->pt()/GeV;
	  eta = muidCBMuon->eta();
	  phi = muidCBMuon->phi();
	  charge = muidCBMuon->charge();
	  e = muidCBMuon->e();
	  px = muidCBMuon->px();
	  py = muidCBMuon->py();
	  pz = muidCBMuon->pz();

	  if(pt < 10.0) continue;

	  RecCBpt.push_back(pt);
	  RecCBpx.push_back(px);
	  RecCBpy.push_back(py);
	  RecCBpz.push_back(pz);
	  RecCBe.push_back(e);
	  RecCBcharge.push_back(charge);
	  RecCBeta.push_back(eta);
	  RecCBphi.push_back(phi);
	
	  //match offline object to online trigger object
	  passedchainL1.push_back(CheckMuonTriggerMatch(eta,phi, L1eta, L1phi));  
	  passedSAchainL2.push_back(CheckMuonTriggerMatch(eta,phi, L2Exeta, L2Exphi));
	  passedCBchainL2.push_back(CheckMuonTriggerMatch(eta,phi, L2Cbeta, L2Cbphi));
	  passedSAchainEF.push_back(CheckMuonTriggerMatch(eta,phi, EFExeta, EFExphi));
	  passedCBchainEF.push_back(CheckMuonTriggerMatch(eta,phi, EFCbeta, EFCbphi));
	}
      }	      
    }///offline loop
    
    if(n_RecCBmuon<2) continue;
    
    //loop over TAG muons       
    for(unsigned int tag=0;tag<RecCBpt.size();++tag){
      if(!passedCBchainEF[tag]) continue;

      //loop over PROBE muons
      for(unsigned int probe=0;probe<RecCBpt.size();++probe){
	if(tag==probe) continue;
	if(RecCBcharge[tag]==RecCBcharge[probe]) continue;
	
	float probept = RecCBpt[probe];
	float probeeta = RecCBeta[probe];
	float fabsprobeeta = fabs(probeeta);
	float probephi = RecCBphi[probe];
	float pxSum = RecCBpx[tag] + RecCBpx[probe]; 
	float pySum =  RecCBpy[tag] + RecCBpy[probe]; 
	float pzSum =  RecCBpz[tag] + RecCBpz[probe]; 
	float eSum =  RecCBe[tag] + RecCBe[probe]; 
	float invMass = sqrt(eSum*eSum - pxSum*pxSum - pySum*pySum - pzSum*pzSum);
	hist(("muZTP_invmass_nomasswindow_" + itmap->first).c_str(), histdirmuztp)->Fill(invMass);

	bool isEndcap = false;
	if(fabs(probeeta) > 1.05) isEndcap = true;
	bool masswindow = false;
	if(fabs(invMass - 91187.6) < 12000.0) masswindow = true;
	
	if(masswindow){
	  //all probes
	  hist(("muZTP_invmass_" + itmap->first).c_str(), histdirmuztp)->Fill(invMass);
	  hist(("muZTP_Pt_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	  if( isEndcap ) hist(("muZTP_Pt_EC_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	  if( !isEndcap ) hist(("muZTP_Pt_B_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	  hist(("muZTP_Eta_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta);
	  hist(("muZTP_Eta_1bin_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	  hist(("muZTP_Eta_2bins_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	  if(RecCBpt[probe] > m_ptcut){
	    hist(("muZTP_Eta_1bin_cut_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    hist(("muZTP_Eta_2bins_cut_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	  }	 
	  hist(("muZTP_Phi_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	  if( isEndcap ) hist(("muZTP_Phi_EC_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	  if( !isEndcap ) hist(("muZTP_Phi_B_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	  hist2(("muZTP_EtaPhi_all_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta, probephi);
	  //L1
	  if(passedchainL1[probe]) {
	    hist(("muZTP_Pt_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( isEndcap ) hist(("muZTP_Pt_EC_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( !isEndcap ) hist(("muZTP_Pt_B_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    hist(("muZTP_Eta_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta);
	    hist(("muZTP_Eta_1bin_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    hist(("muZTP_Eta_2bins_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    if(RecCBpt[probe] > m_ptcut){
	      hist(("muZTP_Eta_1bin_cut_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	      hist(("muZTP_Eta_2bins_cut_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    }
	    hist(("muZTP_Phi_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( isEndcap ) hist(("muZTP_Phi_EC_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( !isEndcap ) hist(("muZTP_Phi_B_L1fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    hist2(("muZTP_EtaPhi_L1_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta, probephi);
	  }
	  //L2
	  if(isTriggered_L2 && isMSonlychain && passedSAchainL2[probe]) { //muFast
	    hist(("muZTP_Pt_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( isEndcap ) hist(("muZTP_Pt_EC_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( !isEndcap ) hist(("muZTP_Pt_B_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    hist(("muZTP_Eta_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta);
	    hist(("muZTP_Eta_1bin_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    hist(("muZTP_Eta_2bins_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    if(RecCBpt[probe] > m_ptcut){
	      hist(("muZTP_Eta_1bin_cut_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	      hist(("muZTP_Eta_2bins_cut_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    }
	    hist(("muZTP_Phi_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( isEndcap ) hist(("muZTP_Phi_EC_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( !isEndcap ) hist(("muZTP_Phi_B_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    hist2(("muZTP_EtaPhi_L2_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta, probephi);
	  }
	  if(isTriggered_L2 && !isMSonlychain && passedCBchainL2[probe]) { //muComb
	    hist(("muZTP_Pt_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( isEndcap ) hist(("muZTP_Pt_EC_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( !isEndcap ) hist(("muZTP_Pt_B_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    hist(("muZTP_Eta_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta);
	    hist(("muZTP_Eta_1bin_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    hist(("muZTP_Eta_2bins_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    if(RecCBpt[probe] > m_ptcut){
	      hist(("muZTP_Eta_1bin_cut_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	      hist(("muZTP_Eta_2bins_cut_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    }
	    hist(("muZTP_Phi_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( isEndcap ) hist(("muZTP_Phi_EC_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( !isEndcap ) hist(("muZTP_Phi_B_L2fired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    hist2(("muZTP_EtaPhi_L2_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta, probephi);
	  }
	  //EF
	  if(isTriggered_EF && isMSonlychain && passedSAchainEF[probe]){
	    hist(("muZTP_Pt_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( isEndcap ) hist(("muZTP_Pt_EC_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( !isEndcap ) hist(("muZTP_Pt_B_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    hist(("muZTP_Eta_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta);
	    hist(("muZTP_Eta_1bin_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    hist(("muZTP_Eta_2bins_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    if(RecCBpt[probe] > m_ptcut){
	      hist(("muZTP_Eta_1bin_cut_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	      hist(("muZTP_Eta_2bins_cut_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    }
	    hist(("muZTP_Phi_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( isEndcap ) hist(("muZTP_Phi_EC_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( !isEndcap ) hist(("muZTP_Phi_B_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    hist2(("muZTP_EtaPhi_EF_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta, probephi);
	  }
	  if(isTriggered_EF && !isMSonlychain && passedCBchainEF[probe]) {
	    hist(("muZTP_Pt_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( isEndcap ) hist(("muZTP_Pt_EC_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    if( !isEndcap ) hist(("muZTP_Pt_B_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probept);
	    hist(("muZTP_Eta_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta);
	    hist(("muZTP_Eta_1bin_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    hist(("muZTP_Eta_2bins_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    if(RecCBpt[probe] > m_ptcut){
	      hist(("muZTP_Eta_1bin_cut_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	      hist(("muZTP_Eta_2bins_cut_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(fabsprobeeta);
	    }
	    hist(("muZTP_Phi_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( isEndcap ) hist(("muZTP_Phi_EC_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    if( !isEndcap ) hist(("muZTP_Phi_B_EFfired_" + itmap->first).c_str(), histdirmuztp)->Fill(probephi);
	    hist2(("muZTP_EtaPhi_EF_" + itmap->first).c_str(), histdirmuztp)->Fill(probeeta, probephi);
	  }
	} // masswindow
      } // loop over probes
    } // loop over tags 
  } // loop over trigger vector 

  return StatusCode::SUCCESS;
} // end of fillMuZTPDQA

StatusCode HLTMuonMonTool::procMuZTPDQA( bool isEndOfEventsBlock, bool isEndOfLumiBlock, bool isEndOfRun )
{
  if( isEndOfRun ){

    for(std::map<std::string, std::string>::iterator itmap=m_ztpmap.begin();itmap!=m_ztpmap.end();++itmap){

      std::string histdirmuztp="HLT/MuonMon/MuZTP/"+itmap->first;

      //efficiency histograms
      std::vector<std::string> var;
      var.push_back("_Pt_");
      var.push_back("_Pt_EC_");
      var.push_back("_Pt_B_");
      var.push_back("_Eta_");
      var.push_back("_Phi_");
      var.push_back("_Phi_EC_");
      var.push_back("_Phi_B_");
      for(unsigned int k=0;k<var.size();k++){
	std::vector<std::string> level;
	level.push_back("L1");
	level.push_back("L2");
	level.push_back("EF");
	for(unsigned int j=0;j<level.size();j++){
	  //ABSOLUTE
	  dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_"+level[j]+var[k]+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP"+var[k]+level[j]+"fired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP"+var[k]+itmap->first).c_str(), histdirmuztp));
	  dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_"+level[j]+"_Eta_1bin_"+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP_Eta_1bin_"+level[j]+"fired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP_Eta_1bin_"+itmap->first).c_str(), histdirmuztp));
	  dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_"+level[j]+"_Eta_2bins_"+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP_Eta_2bins_"+level[j]+"fired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP_Eta_2bins_"+itmap->first).c_str(), histdirmuztp));
	  dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_"+level[j]+"_Eta_1bin_cut_"+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP_Eta_1bin_cut_"+level[j]+"fired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP_Eta_1bin_cut_"+itmap->first).c_str(), histdirmuztp));
	  dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_"+level[j]+"_Eta_2bins_cut_"+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP_Eta_2bins_cut_"+level[j]+"fired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP_Eta_2bins_cut_"+itmap->first).c_str(), histdirmuztp));
	  //2D ETA_PHI
	  hist2(("muZTP_eff_EtaPhi_"+level[j]+"_" + itmap->first).c_str(), histdirmuztp)->Divide( hist2(("muZTP_EtaPhi_"+level[j]+"_" + itmap->first).c_str(), histdirmuztp), hist2(("muZTP_EtaPhi_all_"+ itmap->first).c_str(), histdirmuztp), 1, 1, "B");
	}//level
	//RELATIVE
	dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_EFwrtL2"+var[k]+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP"+var[k]+"EFfired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP"+var[k]+"L2fired_"+itmap->first).c_str(), histdirmuztp));
	dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_EFwrtL1"+var[k]+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP"+var[k]+"EFfired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP"+var[k]+"L1fired_"+itmap->first).c_str(), histdirmuztp));
	dynamic_cast<TGraphAsymmErrors*>( graph(("muZTP_eff_L2wrtL1"+var[k]+itmap->first).c_str(), histdirmuztp))->BayesDivide(hist(("muZTP"+var[k]+"L2fired_"+itmap->first).c_str(), histdirmuztp), hist(("muZTP"+var[k]+"L1fired_"+itmap->first).c_str(), histdirmuztp));
      }//var
    }
    
  }else if( isEndOfLumiBlock ){
  }else if( isEndOfEventsBlock ){
  }
  return StatusCode::SUCCESS;
}


///////////////////////////////////// FUNCTION DEFINITIONS //////////////////////////////////////////

bool CheckMuonTriggerMatch(float off_eta, float off_phi, std::vector<float> v_on_eta, std::vector<float> v_on_phi)
{

  float deltaRcut = 0.15;
  float deltaR= 999999.;
  for(unsigned int i=0; i< v_on_eta.size();++i){
    float dr = CalcdeltaR(off_eta,off_phi,v_on_eta[i],v_on_phi[i]);
    if(dr<deltaR){
      deltaR=dr;
    }
  }
  
  if(deltaR<deltaRcut)return true;
  else return false;
}
///////////////////////////////////////////////////////////////////////////////

float CalcdeltaR(float off_eta, float off_phi,float on_eta, float on_phi){

  float deta = off_eta - on_eta;
  float dphi = off_phi - on_phi;
  float dR=0;
  if (dphi > acos(-1.)) dphi -= 2.*acos(-1.);
  if (dphi < -acos(-1.)) dphi += 2.*acos(-1.);
  dR = sqrt( deta*deta + dphi*dphi ) ;

  return dR;
}
///////////////////////////////////////////////////////////////////////////////
