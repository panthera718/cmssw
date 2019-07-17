///////////////////////////////////////////////////////////////////////////////
// File: HFShower.cc
// Description: Sensitive Detector class for calorimeters
///////////////////////////////////////////////////////////////////////////////

#include "SimG4CMS/Calo/interface/HFShower.h"
#include "SimG4CMS/Calo/interface/HFFibreFiducial.h"

#include "G4NavigationHistory.hh"
#include "G4Step.hh"
#include "G4Track.hh"
#include "G4VSolid.hh"
#include "Randomize.hh"
#include "CLHEP/Units/GlobalPhysicalConstants.h"
#include "CLHEP/Units/GlobalSystemOfUnits.h"

//#define EDM_ML_DEBUG

#include <iostream>

HFShower::HFShower(const std::string &name, const DDCompactView &cpv, edm::ParameterSet const &p, int chk)
    : cherenkov(nullptr), fibre(nullptr), chkFibre(chk) {
  edm::ParameterSet m_HF = p.getParameter<edm::ParameterSet>("HFShower");
  applyFidCut = m_HF.getParameter<bool>("ApplyFiducialCut");
  probMax = m_HF.getParameter<double>("ProbMax");

  edm::LogVerbatim("HFShower") << "HFShower:: Maximum probability cut off " << probMax << " Check flag " << chkFibre;

  cherenkov = new HFCherenkov(m_HF);
  fibre = new HFFibre(name, cpv, p);
}

HFShower::~HFShower() {
  if (cherenkov)
    delete cherenkov;
  if (fibre)
    delete fibre;
}

std::vector<HFShower::Hit> HFShower::getHits(const G4Step *aStep, double weight) {
  std::vector<HFShower::Hit> hits;
  int nHit = 0;

  double edep = weight * (aStep->GetTotalEnergyDeposit());
#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("HFShower") << "HFShower::getHits: energy " << aStep->GetTotalEnergyDeposit() << " weight " << weight
                               << " edep " << edep;
#endif
  double stepl = 0.;

  if (aStep->GetTrack()->GetDefinition()->GetPDGCharge() != 0.)
    stepl = aStep->GetStepLength();
  if ((edep == 0.) || (stepl == 0.)) {
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("HFShower") << "HFShower::getHits: Number of Hits " << nHit;
#endif
    return hits;
  }
  const G4Track *aTrack = aStep->GetTrack();
  const G4DynamicParticle *aParticle = aTrack->GetDynamicParticle();

  HFShower::Hit hit;
  double energy = aParticle->GetTotalEnergy();
  double momentum = aParticle->GetTotalMomentum();
  double pBeta = momentum / energy;
  double dose = 0.;
  int npeDose = 0;

  const G4ThreeVector &momentumDir = aParticle->GetMomentumDirection();
  const G4ParticleDefinition *particleDef = aTrack->GetDefinition();

  const G4StepPoint *preStepPoint = aStep->GetPreStepPoint();
  const G4ThreeVector &globalPos = preStepPoint->GetPosition();
  G4String name = preStepPoint->GetTouchable()->GetSolid(0)->GetName();
  //double        zv           = std::abs(globalPos.z()) - gpar[4] - 0.5*gpar[1];
  double zv = std::abs(globalPos.z()) - gpar[4];
  G4ThreeVector localPos = G4ThreeVector(globalPos.x(), globalPos.y(), zv);
  G4ThreeVector localMom = preStepPoint->GetTouchable()->GetHistory()->GetTopTransform().TransformAxis(momentumDir);
  // @@ Here the depth should be changed  (Fibers all long in Geometry!)
  int depth = 1;
  int npmt = 0;
  bool ok = true;
  if (zv < 0. || zv > gpar[1]) {
    ok = false;  // beyond the fiber in Z
  }
  if (ok && applyFidCut) {
    npmt = HFFibreFiducial::PMTNumber(globalPos);
    if (npmt <= 0) {
      ok = false;
    } else if (npmt > 24) {  // a short fibre
      if (zv > gpar[0]) {
        depth = 2;
      } else {
        ok = false;
      }
    }
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("HFShower") << "HFShower: npmt " << npmt << " zv " << std::abs(globalPos.z()) << ":" << gpar[4]
                                 << ":" << zv << ":" << gpar[0] << " ok " << ok << " depth " << depth;
#endif
  } else {
    depth = (preStepPoint->GetTouchable()->GetReplicaNumber(0)) % 10;  // All LONG!
  }
  G4ThreeVector translation = preStepPoint->GetTouchable()->GetVolume(1)->GetObjectTranslation();

  double u = localMom.x();
  double v = localMom.y();
  double w = localMom.z();
  double zCoor = localPos.z();
  double zFibre = (0.5 * gpar[1] - zCoor - translation.z());
  double tSlice = (aStep->GetPostStepPoint()->GetGlobalTime());
  double time = fibre->tShift(localPos, depth, chkFibre);

#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("HFShower") << "HFShower::getHits: in " << name << " Z " << zCoor << "(" << globalPos.z() << ") "
                               << zFibre << " Trans " << translation << " Time " << tSlice << " " << time
                               << "\n                  Direction " << momentumDir << " Local " << localMom;
#endif
  // Here npe should be 0 if there is no fiber (@@ M.K.)
  int npe = 1;
  std::vector<double> wavelength;
  std::vector<double> momz;
  if (!applyFidCut) {  // _____ Tmp close of the cherenkov function
    if (ok)
      npe = cherenkov->computeNPE(aStep, particleDef, pBeta, u, v, w, stepl, zFibre, dose, npeDose);
    wavelength = cherenkov->getWL();
    momz = cherenkov->getMom();
  }  // ^^^^^ End of Tmp close of the cherenkov function
  if (ok && npe > 0) {
    for (int i = 0; i < npe; ++i) {
      double p = 1.;
      if (!applyFidCut)
        p = fibre->attLength(wavelength[i]);
      double r1 = G4UniformRand();
      double r2 = G4UniformRand();
#ifdef EDM_ML_DEBUG
      edm::LogVerbatim("HFShower") << "HFShower::getHits: " << i << " attenuation " << r1 << ":" << exp(-p * zFibre)
                                   << " r2 " << r2 << ":" << probMax
                                   << " Survive: " << (r1 <= exp(-p * zFibre) && r2 <= probMax);
#endif
      if (applyFidCut || chkFibre < 0 || (r1 <= exp(-p * zFibre) && r2 <= probMax)) {
        hit.depth = depth;
        hit.time = tSlice + time;
        if (!applyFidCut) {
          hit.wavelength = wavelength[i];  // Tmp
          hit.momentum = momz[i];          // Tmp
        } else {
          hit.wavelength = 300.;  // Tmp
          hit.momentum = 1.;      // Tmp
        }
        hit.position = globalPos;
        hits.push_back(hit);
        nHit++;
      }
    }
  }

#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("HFShower") << "HFShower::getHits: Number of Hits " << nHit;
  for (int i = 0; i < nHit; ++i)
    edm::LogVerbatim("HFShower") << "HFShower::Hit " << i << " WaveLength " << hits[i].wavelength << " Time "
                                 << hits[i].time << " Momentum " << hits[i].momentum << " Position "
                                 << hits[i].position;
#endif
  return hits;
}

std::vector<HFShower::Hit> HFShower::getHits(const G4Step *aStep, bool forLibraryProducer, double zoffset) {
  std::vector<HFShower::Hit> hits;
  int nHit = 0;

  double edep = aStep->GetTotalEnergyDeposit();
  double stepl = 0.;

  if (aStep->GetTrack()->GetDefinition()->GetPDGCharge() != 0.)
    stepl = aStep->GetStepLength();
  if ((edep == 0.) || (stepl == 0.)) {
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("HFShower") << "HFShower::getHits: Number of Hits " << nHit;
#endif
    return hits;
  }
  const G4Track *aTrack = aStep->GetTrack();
  const G4DynamicParticle *aParticle = aTrack->GetDynamicParticle();

  HFShower::Hit hit;
  double energy = aParticle->GetTotalEnergy();
  double momentum = aParticle->GetTotalMomentum();
  double pBeta = momentum / energy;
  double dose = 0.;
  int npeDose = 0;

  const G4ThreeVector &momentumDir = aParticle->GetMomentumDirection();
  G4ParticleDefinition *particleDef = aTrack->GetDefinition();

  G4StepPoint *preStepPoint = aStep->GetPreStepPoint();
  const G4ThreeVector &globalPos = preStepPoint->GetPosition();
  G4String name = preStepPoint->GetTouchable()->GetSolid(0)->GetName();
  //double        zv           = std::abs(globalPos.z()) - gpar[4] - 0.5*gpar[1];
  //double        zv           = std::abs(globalPos.z()) - gpar[4];
  double zv = gpar[1] - (std::abs(globalPos.z()) - zoffset);
  G4ThreeVector localPos = G4ThreeVector(globalPos.x(), globalPos.y(), zv);
  G4ThreeVector localMom = preStepPoint->GetTouchable()->GetHistory()->GetTopTransform().TransformAxis(momentumDir);
  // @@ Here the depth should be changed  (Fibers all long in Geometry!)
  int depth = 1;
  int npmt = 0;
  bool ok = true;
  if (zv < 0. || zv > gpar[1]) {
    ok = false;  // beyond the fiber in Z
  }
  if (ok && applyFidCut) {
    npmt = HFFibreFiducial::PMTNumber(globalPos);
    if (npmt <= 0) {
      ok = false;
    } else if (npmt > 24) {  // a short fibre
      if (zv > gpar[0]) {
        depth = 2;
      } else {
        ok = false;
      }
    }
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("HFShower") << "HFShower: npmt " << npmt << " zv " << std::abs(globalPos.z()) << ":" << gpar[4]
                                 << ":" << zv << ":" << gpar[0] << " ok " << ok << " depth " << depth;
#endif
  } else {
    depth = (preStepPoint->GetTouchable()->GetReplicaNumber(0)) % 10;  // All LONG!
  }
  G4ThreeVector translation = preStepPoint->GetTouchable()->GetVolume(1)->GetObjectTranslation();

  double u = localMom.x();
  double v = localMom.y();
  double w = localMom.z();
  double zCoor = localPos.z();
  double zFibre = (0.5 * gpar[1] - zCoor - translation.z());
  double tSlice = (aStep->GetPostStepPoint()->GetGlobalTime());
  double time = fibre->tShift(localPos, depth, chkFibre);

#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("HFShower") << "HFShower::getHits: in " << name << " Z " << zCoor << "(" << globalPos.z() << ") "
                               << zFibre << " Trans " << translation << " Time " << tSlice << " " << time
                               << "\n                  Direction " << momentumDir << " Local " << localMom;
#endif
  // Here npe should be 0 if there is no fiber (@@ M.K.)
  int npe = 1;
  std::vector<double> wavelength;
  std::vector<double> momz;
  if (!applyFidCut) {  // _____ Tmp close of the cherenkov function
    if (ok)
      npe = cherenkov->computeNPE(aStep, particleDef, pBeta, u, v, w, stepl, zFibre, dose, npeDose);
    wavelength = cherenkov->getWL();
    momz = cherenkov->getMom();
  }  // ^^^^^ End of Tmp close of the cherenkov function
  if (ok && npe > 0) {
    for (int i = 0; i < npe; ++i) {
      double p = 1.;
      if (!applyFidCut)
        p = fibre->attLength(wavelength[i]);
      double r1 = G4UniformRand();
      double r2 = G4UniformRand();
#ifdef EDM_ML_DEBUG
      edm::LogVerbatim("HFShower") << "HFShower::getHits: " << i << " attenuation " << r1 << ":" << exp(-p * zFibre)
                                   << " r2 " << r2 << ":" << probMax
                                   << " Survive: " << (r1 <= exp(-p * zFibre) && r2 <= probMax);
#endif
      if (applyFidCut || chkFibre < 0 || (r1 <= exp(-p * zFibre) && r2 <= probMax)) {
        hit.depth = depth;
        hit.time = tSlice + time;
        if (!applyFidCut) {
          hit.wavelength = wavelength[i];  // Tmp
          hit.momentum = momz[i];          // Tmp
        } else {
          hit.wavelength = 300.;  // Tmp
          hit.momentum = 1.;      // Tmp
        }
        hit.position = globalPos;
        hits.push_back(hit);
        nHit++;
      }
    }
  }

#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("HFShower") << "HFShower::getHits: Number of Hits " << nHit;
  for (int i = 0; i < nHit; ++i)
    edm::LogVerbatim("HFShower") << "HFShower::Hit " << i << " WaveLength " << hits[i].wavelength << " Time "
                                 << hits[i].time << " Momentum " << hits[i].momentum << " Position "
                                 << hits[i].position;
#endif
  return hits;
}

std::vector<HFShower::Hit> HFShower::getHits(const G4Step *aStep, bool forLibrary) {
  std::vector<HFShower::Hit> hits;
  int nHit = 0;

  double edep = aStep->GetTotalEnergyDeposit();
  double stepl = 0.;

  if (aStep->GetTrack()->GetDefinition()->GetPDGCharge() != 0.)
    stepl = aStep->GetStepLength();
  if ((edep == 0.) || (stepl == 0.)) {
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("HFShower") << "HFShower::getHits: Number of Hits " << nHit;
#endif
    return hits;
  }

  const G4Track *aTrack = aStep->GetTrack();
  const G4DynamicParticle *aParticle = aTrack->GetDynamicParticle();

  HFShower::Hit hit;
  double energy = aParticle->GetTotalEnergy();
  double momentum = aParticle->GetTotalMomentum();
  double pBeta = momentum / energy;
  double dose = 0.;
  int npeDose = 0;

  const G4ThreeVector &momentumDir = aParticle->GetMomentumDirection();
  G4ParticleDefinition *particleDef = aTrack->GetDefinition();

  const G4StepPoint *preStepPoint = aStep->GetPreStepPoint();
  const G4ThreeVector &globalPos = preStepPoint->GetPosition();
  G4String name = preStepPoint->GetTouchable()->GetSolid(0)->GetName();
  double zv = std::abs(globalPos.z()) - gpar[4] - 0.5 * gpar[1];
  G4ThreeVector localPos = G4ThreeVector(globalPos.x(), globalPos.y(), zv);
  G4ThreeVector localMom = preStepPoint->GetTouchable()->GetHistory()->GetTopTransform().TransformAxis(momentumDir);
  // @@ Here the depth should be changed (Fibers are all long in Geometry!)
  int depth = 1;
  int npmt = 0;
  bool ok = true;
  if (zv < 0 || zv > gpar[1]) {
    ok = false;  // beyond the fiber in Z
  }
  if (ok && applyFidCut) {
    npmt = HFFibreFiducial::PMTNumber(globalPos);
    if (npmt <= 0) {
      ok = false;
    } else if (npmt > 24) {  // a short fibre
      if (zv > gpar[0]) {
        depth = 2;
      } else {
        ok = false;
      }
    }
#ifdef EDM_ML_DEBUG
    edm::LogVerbatim("HFShower") << "HFShower:getHits(SL): npmt " << npmt << " zv " << std::abs(globalPos.z()) << ":"
                                 << gpar[4] << ":" << zv << ":" << gpar[0] << " ok " << ok << " depth " << depth;
#endif
  } else {
    depth = (preStepPoint->GetTouchable()->GetReplicaNumber(0)) % 10;  // All LONG!
  }
  G4ThreeVector translation = preStepPoint->GetTouchable()->GetVolume(1)->GetObjectTranslation();

  double u = localMom.x();
  double v = localMom.y();
  double w = localMom.z();
  double zCoor = localPos.z();
  double zFibre = (0.5 * gpar[1] - zCoor - translation.z());
  double tSlice = (aStep->GetPostStepPoint()->GetGlobalTime());
  double time = fibre->tShift(localPos, depth, chkFibre);

#ifdef EDM_ML_DEBUG
  edm::LogVerbatim("HFShower") << "HFShower::getHits(SL): in " << name << " Z " << zCoor << "(" << globalPos.z() << ") "
                               << zFibre << " Trans " << translation << " Time " << tSlice << " " << time
                               << "\n                  Direction " << momentumDir << " Local " << localMom;
#endif
  // npe should be 0
  int npe = 0;
  if (ok)
    npe = cherenkov->computeNPE(aStep, particleDef, pBeta, u, v, w, stepl, zFibre, dose, npeDose);
  std::vector<double> wavelength = cherenkov->getWL();
  std::vector<double> momz = cherenkov->getMom();

  for (int i = 0; i < npe; ++i) {
    hit.time = tSlice + time;
    hit.wavelength = wavelength[i];
    hit.momentum = momz[i];
    hit.position = globalPos;
    hits.push_back(hit);
    nHit++;
  }

  return hits;
}

void HFShower::initRun(const HcalDDDSimConstants *hcons) {
  //Special Geometry parameters
  gpar = hcons->getGparHF();
  if (fibre) {
    fibre->initRun(hcons);
  }
}
