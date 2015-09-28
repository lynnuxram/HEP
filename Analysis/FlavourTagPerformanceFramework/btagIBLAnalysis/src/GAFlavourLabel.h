#include "xAODJet/Jet.h"

namespace xAOD {

  // return a "basic" jet flavour label:
  //    5:             b jet
  //    4:             c jet
  //   15:           tau jet
  //    0: light-flavour jet
  //
  // The three different methods correspond to the different labeling schemes:
  // using the weakly decaying hadrons, using the hadrons resulting from fragmentation,
  // or using the partons just before fragmentation

  int GAFinalHadronFlavourLabel (const xAOD::Jet* jet);
  int GAInitialHadronFlavourLabel (const xAOD::Jet* jet);
  int GAFinalPartonFlavourLabel (const xAOD::Jet* jet);
}
