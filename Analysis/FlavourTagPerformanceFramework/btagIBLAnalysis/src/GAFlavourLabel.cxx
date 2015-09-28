#include "GAFlavourLabel.h"

#include <vector>
#include <string>

using xAOD::IParticle;

int GAFinalHadronFlavourLabel (const xAOD::Jet* jet);
int GAInitialHadronFlavourLabel (const xAOD::Jet* jet);
int GAFinalPartonFlavourLabel (const xAOD::Jet* jet);


///////////////////////////////////////////////////////////////////////////////////

int
xAOD::GAFinalHadronFlavourLabel (const xAOD::Jet* jet) {

  const std::string labelB = "GhostBHadronsFinal";
  const std::string labelC = "GhostCHadronsFinal";
  const std::string labelTau = "GhostTausFinal";

  std::vector<const IParticle*> ghostB;
  if (jet->getAssociatedObjects<IParticle>(labelB, ghostB) && ghostB.size() > 0) return 5;
  std::vector<const IParticle*> ghostC;
  if (jet->getAssociatedObjects<IParticle>(labelC, ghostC) && ghostC.size() > 0) return 4;
  std::vector<const IParticle*> ghostTau;
  if (jet->getAssociatedObjects<IParticle>(labelTau, ghostTau) && ghostTau.size() > 0) return 15;
  return 0;
}

int
xAOD::GAInitialHadronFlavourLabel (const xAOD::Jet* jet) {

  const std::string labelB = "GhostBHadronsInitial";
  const std::string labelC = "GhostCHadronsInitial";
  const std::string labelTau = "GhostTausFinal";

  std::vector<const IParticle*> ghostB;
  if (jet->getAssociatedObjects<IParticle>(labelB, ghostB) && ghostB.size() > 0) return 5;
  std::vector<const IParticle*> ghostC;
  if (jet->getAssociatedObjects<IParticle>(labelC, ghostC) && ghostC.size() > 0) return 4;
  std::vector<const IParticle*> ghostTau;
  if (jet->getAssociatedObjects<IParticle>(labelTau, ghostTau) && ghostTau.size() > 0) return 15;
  return 0;
}

int
xAOD::GAFinalPartonFlavourLabel (const xAOD::Jet* jet) {

  const std::string labelB = "GhostBQuarksFinal";
  const std::string labelC = "GhostCQuarksFinal";
  const std::string labelTau = "GhostTausFinal";

  std::vector<const IParticle*> ghostB;
  if (jet->getAssociatedObjects<IParticle>(labelB, ghostB) && ghostB.size() > 0) return 5;
  std::vector<const IParticle*> ghostC;
  if (jet->getAssociatedObjects<IParticle>(labelC, ghostC) && ghostC.size() > 0) return 4;
  std::vector<const IParticle*> ghostTau;
  if (jet->getAssociatedObjects<IParticle>(labelTau, ghostTau) && ghostTau.size() > 0) return 15;
  return 0;
}
