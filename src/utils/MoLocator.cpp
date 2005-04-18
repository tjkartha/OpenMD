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
 
#include <iostream>

#include <cstdlib>
#include <cmath>

#include "utils/simError.h"
#include "utils/MoLocator.hpp"
#include "types/AtomType.hpp"

namespace oopse {
  MoLocator::MoLocator( MoleculeStamp* theStamp, ForceField* theFF){
    
    myStamp = theStamp;
    myFF = theFF;
    nIntegrableObjects = myStamp->getNIntegrable();
    calcRef();
  }
  
  void MoLocator::placeMol( const Vector3d& offset, const Vector3d& ort, Molecule* mol){
    Vector3d newCoor;
    Vector3d curRefCoor;  
    RotMat3x3d rotMat = latVec2RotMat(ort);
    
    if(mol->getNIntegrableObjects() != nIntegrableObjects){
      sprintf( painCave.errMsg,
               "MoLocator error.\n"
               "  The number of integrable objects of MoleculeStamp is not the same as  that of Molecule\n");
      painCave.isFatal = 1;
      simError();
    }
    
    Molecule::IntegrableObjectIterator ii;
    StuntDouble* integrableObject;
    int i;
    for (integrableObject = mol->beginIntegrableObject(ii), i = 0; integrableObject != NULL; 
         integrableObject = mol->nextIntegrableObject(ii), ++i) { 
      
      newCoor = rotMat * refCoords[i];
      newCoor += offset;
      
      integrableObject->setPos( newCoor);
      integrableObject->setVel(V3Zero);
      
      if(integrableObject->isDirectional()){
        integrableObject->setA(rotMat * integrableObject->getA());
        integrableObject->setJ(V3Zero);  
      }        
    }
  }
  
  void MoLocator::calcRef( void ){
    AtomStamp* currAtomStamp;
    RigidBodyStamp* rbStamp;
    int nAtoms; 
    int nRigidBodies;
    std::vector<double> mass;
    Vector3d coor;
    Vector3d refMolCom;  
    int nAtomsInRb;
    double totMassInRb;
    double currAtomMass;
    double molMass;
    
    nAtoms= myStamp->getNAtoms();
    nRigidBodies = myStamp->getNRigidBodies();
    
    for(size_t i=0; i<nAtoms; i++){
      
      currAtomStamp = myStamp->getAtom(i);
      
      if( !currAtomStamp->havePosition() ){
        sprintf( painCave.errMsg,
                 "MoLocator error.\n"
                 "  Component %s, atom %s does not have a position specified.\n"
                 "  This means MoLocator cannot initalize it's position.\n",
                 myStamp->getID(),
                 currAtomStamp->getType() );
        
        painCave.isFatal = 1;
        simError();
      }
      
      //if atom belongs to rigidbody, just skip it
      if(myStamp->isAtomInRigidBody(i))
        continue;
      //get mass and the reference coordinate 
      else{
        currAtomMass = getAtomMass(currAtomStamp->getType(), myFF);   
        mass.push_back(currAtomMass);
        coor.x() = currAtomStamp->getPosX();
        coor.y() = currAtomStamp->getPosY();
        coor.z() = currAtomStamp->getPosZ();
        refCoords.push_back(coor);
        
      }
    }
    
    for(int i = 0; i < nRigidBodies; i++){
      
      rbStamp = myStamp->getRigidBody(i);
      nAtomsInRb = rbStamp->getNMembers();
      
      coor.x() = 0.0;
      coor.y() = 0.0;
      coor.z() = 0.0;
      totMassInRb = 0.0;
      
      for(int j = 0; j < nAtomsInRb; j++){
        
        currAtomStamp = myStamp->getAtom(rbStamp->getMember(j));
        currAtomMass = getAtomMass(currAtomStamp->getType(), myFF);
        totMassInRb +=  currAtomMass;
        
        coor.x() += currAtomStamp->getPosX() * currAtomMass;
        coor.y() += currAtomStamp->getPosY() * currAtomMass;
        coor.z() += currAtomStamp->getPosZ() * currAtomMass;
      }
      
      mass.push_back(totMassInRb);
      coor /= totMassInRb;
      refCoords.push_back(coor);
    }
    
    
    //calculate the reference center of mass
    molMass = 0;
    refMolCom.x() = 0;
    refMolCom.y() = 0;
    refMolCom.z() = 0;
    
    for(int i = 0; i < nIntegrableObjects; i++){
      refMolCom += refCoords[i] * mass[i];
      molMass += mass[i];
    }
    
    refMolCom /= molMass;
    
    //move the reference center of mass to (0,0,0) and adjust the reference coordinate 
    //of the integrabel objects
    for(int i = 0; i < nIntegrableObjects; i++)
      refCoords[i] -= refMolCom;
  }
  
  double getAtomMass(const std::string& at, ForceField* myFF) {
    double mass;
    AtomType* atomType= myFF->getAtomType(at);
    if (atomType != NULL) {
      mass =     atomType->getMass();
    } else {
      mass = 0.0;
      std::cerr << "Can not find AtomType: " << at << std::endl;
    }
    return mass;
  }
  
  double getMolMass(MoleculeStamp *molStamp, ForceField *myFF) {
    int nAtoms;
    double totMass = 0;
    nAtoms = molStamp->getNAtoms();
    
    for(size_t i = 0; i < nAtoms; i++) {
      AtomStamp *currAtomStamp = molStamp->getAtom(i);
      totMass += getAtomMass(currAtomStamp->getType(), myFF);         
    }
    return totMass;
  }
  RotMat3x3d latVec2RotMat(const Vector3d& lv){
    
    double theta =acos(lv[2]);
    double phi = atan2(lv[1], lv[0]);
    double psi = 0;
    
    return RotMat3x3d(phi, theta, psi);
    
  }
}
