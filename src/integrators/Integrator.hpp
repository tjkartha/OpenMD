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
 
/**
 * @file Integrator.hpp
 * @author tlin
 * @date 11/08/2004
 * @time 13:25am
 * @version 1.0
 */

#ifndef INTEGRATORS_INTEGRATOR_HPP
#define INTEGRATORS_INTEGRATOR_HPP

#include "brains/ForceManager.hpp"
#include "restraints/ThermoIntegrationForceManager.hpp"
#include "io/DumpWriter.hpp"
#include "io/StatWriter.hpp"
#include "integrators/Velocitizer.hpp"
#include "integrators/RNEMD.hpp"

namespace oopse {


  /**
   * @class Integrator Integrator.hpp "integrators/Integrator.hpp"
   * @brief Base class of Integrator
   * @todo document
   */
  class Integrator {
  public:

    virtual ~Integrator();

    //avoid public virtual function        
    void integrate() {
      doIntegrate();
    }

    void update() {
      doUpdate();
    }

    void setForceManager(ForceManager* forceMan) {

      if (forceMan_ != forceMan && forceMan_  != NULL) {
	delete forceMan_;
      }
      forceMan_ = forceMan;
    }

    void setVelocitizer(Velocitizer* velocitizer) {
      if (velocitizer_ != velocitizer && velocitizer_  != NULL) {
	delete velocitizer_;
      }
      velocitizer_  = velocitizer;
    }

    void setRNEMD(RNEMD* rnemd) {
      if (rnemd_ != rnemd && rnemd_  != NULL) {
	delete rnemd_;
      }
      rnemd_  = rnemd;
    }
        
  protected:

    Integrator(SimInfo* info);

    virtual void doIntegrate() = 0;

    virtual void doUpdate() {}
        
    void saveConservedQuantity() {
      currentSnapshot_->statData[Stats::CONSERVED_QUANTITY] = calcConservedQuantity();
    }
        
    SimInfo* info_;
    Globals* simParams;
    ForceManager* forceMan_;
    bool needPotential;
    bool needStress;
    bool needReset;    
    Velocitizer* velocitizer_;
    RNEMD* rnemd_;
    bool needVelocityScaling;
    RealType targetScalingTemp;

    bool useRNEMD;    
    
    DumpWriter* dumpWriter;
    StatWriter* statWriter;
    Thermo thermo;

    RealType runTime;
    RealType sampleTime;
    RealType statusTime;
    RealType thermalTime;
    RealType resetTime;
    RealType RNEMD_swapTime;
    RealType dt;

    Snapshot* currentSnapshot_; //During the integration, the address of currentSnapshot Will not change

        
  private:
        
    virtual RealType calcConservedQuantity() = 0;
        
    virtual DumpWriter* createDumpWriter() = 0;

    virtual StatWriter* createStatWriter() = 0;
  };

    
}
#endif //INTEGRATORS_INTEGRATOR_HPP
