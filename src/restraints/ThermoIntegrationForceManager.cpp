/*
 * Copyright (c) 2005 The University of Notre Dame. All Rights Reserved.
 *
 * The University of Notre Dame grants you ("Licensee") a
 * non-exclusive, royalty free, license to use, modify and
 * redistribute this software in source and binary code form, provided
 * that the following conditions are met:
 *
 * 1. Acknowledgement of the program authors must be made in any
 *    publication of scientific results based in part on use of the
 *    program.  An acceptable form of acknowledgement is citation of
 *    the article in which the program was described (Matthew
 *    A. Meineke, Charles F. Vardeman II, Teng Lin, Christopher
 *    J. Fennell and J. Daniel Gezelter, "OOPSE: An Object-Oriented
 *    Parallel Simulation Engine for Molecular Dynamics,"
 *    J. Comput. Chem. 26, pp. 252-271 (2005))
 *
 * 2. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 3. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the
 *    distribution.
 *
 * This software is provided "AS IS," without a warranty of any
 * kind. All express or implied conditions, representations and
 * warranties, including any implied warranty of merchantability,
 * fitness for a particular purpose or non-infringement, are hereby
 * excluded.  The University of Notre Dame and its licensors shall not
 * be liable for any damages suffered by licensee as a result of
 * using, modifying or distributing the software or its
 * derivatives. In no event will the University of Notre Dame or its
 * licensors be liable for any lost revenue, profit or data, or for
 * direct, indirect, special, consequential, incidental or punitive
 * damages, however caused and regardless of the theory of liability,
 * arising out of the use of or inability to use software, even if the
 * University of Notre Dame has been advised of the possibility of
 * such damages.
 */
 

#include "restraints/ThermoIntegrationForceManager.hpp"

#ifdef IS_MPI
#include <mpi.h>
#endif

namespace oopse {
  
  ThermoIntegrationForceManager::ThermoIntegrationForceManager(SimInfo* info): 
    RestraintForceManager(info){
    currSnapshot_ = info_->getSnapshotManager()->getCurrentSnapshot();
    simParam = info_->getSimParams();
    
    if (simParam->haveThermodynamicIntegrationLambda()){
      tIntLambda_ = simParam->getThermodynamicIntegrationLambda();
    }
    else{
      tIntLambda_ = 1.0;
      sprintf(painCave.errMsg,
              "ThermoIntegration error: the transformation parameter\n"
              "\t(lambda) was not specified. OOPSE will use a default\n"
              "\tvalue of %f. To set lambda, use the \n"
              "\tthermodynamicIntegrationLambda variable.\n",
              tIntLambda_);
      painCave.isFatal = 0;
      simError();
    }
    
    if (simParam->haveThermodynamicIntegrationK()){
      tIntK_ = simParam->getThermodynamicIntegrationK();
    }
    else{
      tIntK_ = 1.0;
      sprintf(painCave.errMsg,
              "ThermoIntegration Warning: the tranformation parameter\n"
              "\texponent (k) was not specified. OOPSE will use a default\n"
              "\tvalue of %f. To set k, use the thermodynamicIntegrationK\n"
              "\tvariable.\n",
              tIntK_);
      painCave.isFatal = 0;
      simError();      
    }
    
    // build the scaling factor used to modulate the forces and torques
    factor_ = pow(tIntLambda_, tIntK_);
  }
  
  ThermoIntegrationForceManager::~ThermoIntegrationForceManager(){
  }
  
  void ThermoIntegrationForceManager::calcForces(bool needPotential, 
                                                 bool needStress){
    Snapshot* curSnapshot;
    SimInfo::MoleculeIterator mi;
    Molecule* mol;
    Molecule::IntegrableObjectIterator ii;
    StuntDouble* integrableObject;
    Vector3d frc;
    Vector3d trq;
    Mat3x3d tempTau;
    
    // perform the standard calcForces first
    ForceManager::calcForces(needPotential, needStress);
    
    curSnapshot = info_->getSnapshotManager()->getCurrentSnapshot();

    // now scale forces and torques of all the integrableObjects
      
    for (mol = info_->beginMolecule(mi); mol != NULL; 
         mol = info_->nextMolecule(mi)) {
      for (integrableObject = mol->beginIntegrableObject(ii); 
           integrableObject != NULL; 
           integrableObject = mol->nextIntegrableObject(ii)) {
        frc = integrableObject->getFrc();
        frc *= factor_;
        integrableObject->setFrc(frc);
        
        if (integrableObject->isDirectional()){
          trq = integrableObject->getTrq();
          trq *= factor_;
          integrableObject->setTrq(trq);
        }
      }
    }
    
    // set vraw to be the unmodulated potential
    lrPot_ = curSnapshot->statData[Stats::LONG_RANGE_POTENTIAL];
    curSnapshot->statData[Stats::VRAW] = lrPot_;
    
    // modulate the potential and update the snapshot
    lrPot_ *= factor_;
    curSnapshot->statData[Stats::LONG_RANGE_POTENTIAL] = lrPot_;
    
    // scale the pressure tensor
    tempTau = curSnapshot->statData.getTau();
    tempTau *= factor_;
    curSnapshot->statData.setTau(tempTau);

    // now, on to the applied restraining potentials (if needed):
    RealType restPot_local = 0.0;
    RealType vHarm_local = 0.0;
    
    if (simParam->getUseRestraints()) {
      // do restraints from RestraintForceManager:
      //restPot_local = doRestraints(1.0 - factor_);     
      restPot_local = doRestraints(1.0 - factor_);      
      vHarm_local = getUnscaledPotential();
    }
      
#ifdef IS_MPI
    RealType restPot;
    MPI::COMM_WORLD.Allreduce(&restPot_local, &restPot, 1, 
                              MPI::REALTYPE, MPI::SUM);
    MPI::COMM_WORLD.Allreduce(&vHarm_local, &vHarm_, 1, 
                              MPI::REALTYPE, MPI::SUM);         
    lrPot_ += restPot;
#else
    lrPot_ += restPot_local;
    vHarm_ = vHarm_local;
#endif

    // give the final values to stats
    curSnapshot->statData[Stats::LONG_RANGE_POTENTIAL] = lrPot_;
    curSnapshot->statData[Stats::VHARM] = vHarm_;
  }  
}
