/**********************************************************************
Copyright (C) 2000 by OpenEye Scientific Software, Inc.
Some portions Copyright (C) 2001-2005 by Geoffrey R. Hutchison
Some portions Copyright (C) 2004 by Chris Morley
Some portions Copyright (C) 2008 by J. Daniel Gezelter
 
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation version 2 of the License.
 
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.
***********************************************************************/

#include <openbabel/babelconfig.h>
#include <openbabel/obmolecformat.h>
#include <openbabel/obiter.h>
#include <openbabel/mol.h>
#include <openbabel/chains.h>
#include <fstream>

#include "utils/StringUtils.hpp"

using namespace std;
namespace OpenBabel
{
  
  class OOPSEFormat : public OBMoleculeFormat
  {
  public:
    //Register this format type ID
    OOPSEFormat() 
    {      
      OBConversion::RegisterFormat("md",this);
    }
    
    virtual const char* Description() //required
    {
      return
        "OOPSE combined meta-data / cartesian coordinates format\nNo comments yet\n";
    };
    
    virtual const char* SpecificationURL()
    {return "http://www.oopse.org";}; //optional
    
    virtual const char* GetMIMEType() 
    {return "chemical/x-md"; };
    
    virtual unsigned int Flags() 
    { 
      return NOTREADABLE | WRITEONEONLY;
    }
    
    //*** This section identical for most OBMol conversions ***
    ////////////////////////////////////////////////////
    /// The "API" interface functions
    virtual bool WriteMolecule(OBBase* pOb, OBConversion* pConv);

  private:
    bool AreSameFragments(OBMol& mol, vector<int>& frag1, vector<int>& frag2);
    //void findAngles(OBMol& mol);
    OBMol* createMolFromFragment(OBMol& mol, vector<int>& fragment);
    void WriteMDFile(vector<OBMol*> mols, vector<int> numMols, ostream& os, OBMol& mol, vector<int>& indices);
  };

  //Make an instance of the format class
  OOPSEFormat theOOPSEFormat;
  
  /////////////////////////////////////////////////////////////////


  bool OOPSEFormat::WriteMolecule(OBBase* pOb, OBConversion* pConv)
  {
    OBMol* pmol = dynamic_cast<OBMol*>(pOb);
    if(pmol==NULL)
      return false;
    
    vector<vector<int> > fragmentLists;
    pmol->ContigFragList(fragmentLists);
    OBBitVec unused;
    vector<bool> used(fragmentLists.size(), 0);
    vector<vector<int> > molecules;
    vector<int> indices;
    for(int i =0; i < used.size(); ++i) {
      if (used[i])
        {
          continue;
        }
      used[i] = true;
      vector<int> sameMolTypes;
      sameMolTypes.push_back(i);
      indices.insert(indices.end(), fragmentLists[i].begin(), 
                     fragmentLists[i].end());
      for (int j = i + 1;j < used.size(); ++j)
        {
          if (used[j])
            {
              continue;
            }
          
          if (AreSameFragments(*pmol, fragmentLists[i], fragmentLists[j]))
            {
              sameMolTypes.push_back(j);
              indices.insert(indices.end(), fragmentLists[j].begin(), 
                             fragmentLists[j].end());
              used[j]=true;
            }
        }
      molecules.push_back(sameMolTypes);
      
    }
    
    //
    vector<OBMol*> mdMols;    
    vector<int> numMols;
    for(vector<vector<int> >::iterator  i = molecules.begin(); 
        i != molecules.end(); ++i) 
      {
        mdMols.push_back(createMolFromFragment(*pmol, 
                                               fragmentLists[i->front()]));
        numMols.push_back((*i).size());
      }
    
    string OutputFileName = pConv->GetInFilename();
    size_t pos = OutputFileName.rfind(".");
    if(pos!=string::npos)
      OutputFileName = OutputFileName.substr(0, pos) + ".md";       
    else
      OutputFileName += ".md";

    ofstream ofs(OutputFileName.c_str());
    if(!ofs)
      {
        cerr << "Cannot write to " << OutputFileName <<endl;
        return false;
      }
    
    WriteMDFile(mdMols, numMols, ofs, *pmol, indices);
    
    for(vector<OBMol*>::iterator  i = mdMols.begin(); i != mdMols.end(); ++i) 
      {
        delete *i;
      }
    
    //    
    
    return(true);
  }
  
  bool OOPSEFormat::AreSameFragments(OBMol& mol, vector<int>& frag1, 
                                     vector<int>& frag2)
  {
    if (frag1.size() != frag2.size())
      {
        return false;
      }
    
    //exact graph matching is a NP complete problem
    /** @todo using sparse matrix to store the connectivities*/
    for (unsigned int i =0 ; i < frag1.size(); ++i)
      {
        OBAtom* atom1 = mol.GetAtom(frag1[i]);
        OBAtom* atom2 = mol.GetAtom(frag2[i]);
        
        if (atom1->GetAtomicNum() != atom2->GetAtomicNum())
          {
            return false;
          }
      }
    return true;
  }
  
  struct SameAngle
  {
    bool operator()(const triple<OBAtom*,OBAtom*,OBAtom*> t1, 
                    const triple<OBAtom*,OBAtom*,OBAtom*> t2) const
    {
      return (t1.second == t2.second) && ( (t1.first == t2.first && t1.third == t2.third) || (t1.first == t2.third && t1.third == t2.first));
    }
  };

  /*  
    void OOPSEFormat::findAngles(OBMol& mol) {

    //if already has data return
    if(mol.HasData(OBGenericDataType::AngleData))
    return;
    
    vector<OBEdgeBase*>::iterator bi1,bi2;
    OBBond* bond;
    OBAtom *a,*b,*c;
    
    set<triple<OBAtom*,OBAtom*,OBAtom*>, SameAngle> uniqueAngles;
    //loop through all bonds generating torsions
    for(bond = mol.BeginBond(bi1);bond;bond = mol.NextBond(bi1))
    {
        b = bond->GetBeginAtom();
        c = bond->GetEndAtom();
        if(b->IsHydrogen())
            continue;

        for(a = b->BeginNbrAtom(bi2);a;a = b->NextNbrAtom(bi2))
        {
            if(a == c)
                continue;          
            
            uniqueAngles.insert(triple<OBAtom*,OBAtom*,OBAtom*>(a, b, c));
        }
    }

    //get new data and attach it to molecule
    OBAngleData *angles = new OBAngleData;
    mol.SetData(angles);
    set<triple<OBAtom*,OBAtom*,OBAtom*>, SameAngle>::iterator i;

    for (i = uniqueAngles.begin(); i != uniqueAngles.end(); ++i) {
        OBAngle angle;
        angle.SetAtoms(i->first, i->second, i->second);
        angles->SetData(angle);
    }

  }
  */
  
  OBMol* OOPSEFormat::createMolFromFragment(OBMol& mol, vector<int>& fragment) {
    
    OBMol* newMol = new OBMol();
    OBChainsParser* chainParser = new OBChainsParser();
    newMol->ReserveAtoms(fragment.size());
    newMol->BeginModify();
    for(vector<int>::iterator i = fragment.begin(); i != fragment.end(); ++i) {
      OBAtom* newAtom = newMol->NewAtom();
      *newAtom = *mol.GetAtom(*i);
    }
    newMol->EndModify();
    newMol->ConnectTheDots();
    newMol->PerceiveBondOrders();
    newMol->FindAngles();
    newMol->FindTorsions();
    //chainParser->PerceiveChains(*newMol, false);

    return newMol;
  }
  
  void OOPSEFormat::WriteMDFile(vector<OBMol*> mols, vector<int> numMols, ostream& os, OBMol& mol, vector<int>& indices) {
    std::string indentLevel1("  ");
    std::string indentLevel2("    ");
    std::string molPrefix("MolName");
    std::string resName;
    unsigned int i;
    const int BUFFLEN = 1024;
    char buffer[BUFFLEN];
    string str, str1;
    OBAtom *a, *b, *c, *d;    
    OBChainsParser* chainParser = new OBChainsParser();
    OBResidue *r;
    int resKey;
    char type_name[10];
    char *element_name;
    int res_num;
    
    std::cerr << "yo\n";
    os << "<OOPSE version=4>" << endl;
    os << "  <MetaData>" << endl << endl;
    
    for(i = 0; i < mols.size(); ++i) {
      OBMol* pmol = mols[i];
      std::cerr << "yo1\n";
      pmol->ConnectTheDots();
      pmol->PerceiveBondOrders();
      chainParser->PerceiveChains(*pmol, false);
      //pmol->FindSSSR();
      //pmol->SetAromaticPerceived();
      //pmol->Kekulize();
      
      map<OBAtom*, int> atomMap;
      os << "molecule {\n";
      sprintf(buffer, "%d", i);
      os << indentLevel1 << "name = " << "\"" << molPrefix << buffer << "\"" << ";\n";
      

      //atom
      
      int ai = 0;
      FOR_RESIDUES_OF_MOL(res, *pmol) {

        std::cerr << "found residue" << res->GetName() << "\n";
      }
      
//       FOR_RESIDUES_OF_MOL(res, *pmol) {
	
// 	resName = res->GetName();
// 	resKey = res->GetResKey();

	

// 	std::cerr << "found residue " << resName << "\n";
// 	std::cerr << "  num = " << res->GetNum() << "\n";
// 	std::cerr << "  numAtoms = " << res->GetNumAtoms() << "\n";
// 	std::cerr << "  num = " << res->GetNum() << "\n";
// 	std::cerr << "  chain = " << res->GetChain() << "\n";
// 	std::cerr << "  chainnum = " << res->GetChainNum() << "\n";
// 	std::cerr << "  idx = " << res->GetIdx() << "\n";
// 	std::cerr << "  key = " << res->GetResKey() << "\n";


// 	FOR_ATOMS_OF_RESIDUE(atom, &*res) {
// 	  str = atom->GetType();
// 	  ttab.SetFromType("INT");
// 	  ttab.SetToType("INT");
// 	  ttab.Translate(str1,str);
// 	  os << indentLevel1 << "atom[" << ai << "] {\n";
// 	  os << indentLevel2 << "type = " << "\"" << resName << "-" << str1 << "\"" << ";\n";
// 	  os << indentLevel1 << "}\n";
// 	  atomMap[&(*atom)] = ai++;
// 	}        
// 	os << "\n";
	
//       }      

      ai = 0;
      FOR_ATOMS_OF_MOL(atom, *pmol ) {
        str = atom->GetType();
	r = atom->GetResidue();
	
	if (r == NULL) 
	  resName = "NULL";
	else 
	  resName = r->GetName();
		
	if (resName.compare("NULL") ==0 || resName.compare("LIG") == 0 || resName.compare("UNK") == 0) {
	  // Either couldn't find a residue at all or couldn't find a reasonable 
	  // residue name to use.  We'll punt and use OpenBabel's internal atom typing:
	  ttab.SetFromType("INT");
	  ttab.SetToType("INT");
	  ttab.Translate(str1,str);
	} else {
	  	

	  if (resName.compare("HOH") == 0) {
	    // HOH is a special residue name for water, and the standard atom types
	    // are OW and HW, so just append W to the string for the atom type:
	    ttab.SetFromType("INT");
	    ttab.SetToType("XYZ");
	    ttab.Translate(str1,str);
	    str1 += "W";
	  } else {

	    std::cerr << "found residue " << resName << "\n";

	    // If we know what residue we've got, the specific atom name can
	    // be used to help specify partial charges. 

	    str = r->GetAtomID(&*atom);
	    size_t startpos = str.find_first_not_of(" ");
	    size_t endpos = str.find_last_not_of(" ");
	    str = str.substr( startpos, endpos-startpos+1 );
	    str1 = resName + "-" + str;
	  }

	}

//         os << indentLevel1 << "atom[" << ai << "] {\n";
//         os << indentLevel2 << "type = " << "\"" << str1 << "\"" << ";\n";
//         os << indentLevel1 << "}\n";
        os << "atom[" << ai << "] { ";
        os << "type = " << "\"" << str1 << "\"" << "; ";
        os << "}\n";
        atomMap[&(*atom)] = ai++;
      }        
      os << "\n";
      
      //bond
      FOR_BONDS_OF_MOL(bond, *pmol ) {
//         os << indentLevel1 << "bond {\n";
//         os << indentLevel2 << "members(" << atomMap[bond->GetBeginAtom()] <<  ", " << atomMap[bond->GetEndAtom()] << ");\n";
//         os << indentLevel1 << "}\n";
        os << "bond { ";
        os << "members(" << atomMap[bond->GetBeginAtom()] <<  ", " << atomMap[bond->GetEndAtom()] << "); ";
        os << "}\n";
      } 

      FOR_ANGLES_OF_MOL(angle, *pmol) {

	// OpenBabel's getAtoms returns the 3 atom pointer for the
	// angle with the vertex first.  These need to be reordered
	// for vertex-second ordering for OOPSE.

         b = pmol->GetAtom((*angle)[0] + 1);
         a = pmol->GetAtom((*angle)[1] + 1);
         c = pmol->GetAtom((*angle)[2] + 1);

// 	 os << indentLevel1 << "bend {\n";
// 	 os << indentLevel2 << "members(" << atomMap[a] <<  ", " << atomMap[b] << ", " << atomMap[c] << ");\n";
// 	 os << indentLevel1 << "}\n";
	 os << "bend { ";
	 os << "members(" << atomMap[a] <<  ", " << atomMap[b] << ", " << atomMap[c] << "); ";
	 os << "}\n";
      } 

      FOR_TORSIONS_OF_MOL(torsion, *pmol) {

	// OpenBabel's getAtoms returns the 3 atom pointer for the
	// angle with the vertex first.  These need to be reordered
	// for vertex-second ordering for OOPSE.

         a = pmol->GetAtom((*torsion)[0] + 1);
         b = pmol->GetAtom((*torsion)[1] + 1);
         c = pmol->GetAtom((*torsion)[2] + 1);
         d = pmol->GetAtom((*torsion)[3] + 1);

//         os << indentLevel1 << "torsion {\n";
//         os << indentLevel2 << "members(" << atomMap[a] <<  ", " << atomMap[b] << ", " << atomMap[c] << ", " << atomMap[d] << ");\n";
//         os << indentLevel1 << "}\n";

        os << "torsion { ";
        os << "members(" << atomMap[a] <<  ", " << atomMap[b] << ", " << atomMap[c] << ", " << atomMap[d] << "); ";
        os << "}\n";
      } 

      /*
      //bend
      OBGenericData* pGenericData = pmol->GetData(OBGenericDataType::AngleData);
      OBAngleData* pAngleData = dynamic_cast<OBAngleData*>(pGenericData);
      vector<OBAngle> angles = pAngleData->GetData();
      
      os << indentLevel1 << "nBends = " << angles.size() << ";\n";        
      int bendIndex = 0;
      for (vector<OBAngle>::iterator ti = angles.begin(); ti != angles.end(); ++ti)
      {
      triple<OBAtom*, OBAtom*, OBAtom*> bendAtoms = ti->getAtoms();
      os << indentLevel1 << "bend[" << bendIndex++ << "] {\n";
          os << indentLevel2 << "member(" << atomMap[bendAtoms.first] <<  ", " << atomMap[bendAtoms.second] << atomMap[bendAtoms.third] <<");\n";
          os << indentLevel1 << "}\n";            
          }
          
          //torsion
          pGenericData = pmol->GetData(OBGenericDataType::TorsionData);
          OBTorsionData* pTorsionData = dynamic_cast<OBTorsionData*>(pGenericData);
          vector<OBTorsion> torsions = pTorsionData->GetData();
          vector<quad<OBAtom*,OBAtom*,OBAtom*,OBAtom*> > torsionArray;
          for (vector<OBTorsion>::iterator ti = torsions.begin(); ti != torsions.end(); ++ti)
          {
          vector<quad<OBAtom*,OBAtom*,OBAtom*,OBAtom*> > tmpTorsions = ti->getTorsions();
          torsionArray.insert(torsionArray.end(), tmpTorsions.begin(), tmpTorsions.end());            
          }
          
          os << indentLevel1 << "nTorsions = " << torsionArray.size() << ";\n";
          int torsionIndex = 0;
          for (vector<quad<OBAtom*,OBAtom*,OBAtom*,OBAtom*> >::iterator ti = torsionArray.begin(); ti != torsionArray.end(); ++ti)
          {
          os << indentLevel1 << "torsion[" << torsionIndex++ << "] {\n";
          os << indentLevel2 << "member(" << atomMap[ti->first] <<  ", " << atomMap[ti->second] <<", " << atomMap[ti->third] <<", " << atomMap[ti->forth] << ");\n";
          os << indentLevel1 << "}\n";          
          }
      */
      os << "}" << endl;
      os << endl;
      
    }
    
    os << endl;
    
    
    for(i=0; i < mols.size(); ++i) {      
      os << "component{" << endl;
      sprintf(buffer, "%d", i);
      os << indentLevel1 << "type = " << molPrefix << buffer << ";" << endl;
      os << indentLevel1 << "nMol = " << numMols[i]<< ";" << endl;
      os << "}" << endl;
    }
    
    os << "  </MetaData>" << endl;
    os << "  <Snapshot>" << endl;
    os << "    <FrameData>" << endl;
    
    sprintf(buffer, "        Time: %.10g", 0.0);
    
    os << buffer << endl;
    
    sprintf(buffer, "        Hmat: {{ %.10g, %.10g, %.10g }, { %.10g, %.10g, %.10g }, { %.10g, %.10g, %.10g }}", 100.0, 0.0, 0.0, 0.0, 100.0, 0.0, 0.0, 0.0, 100.0);
    
    os << buffer << endl;
    os << "    </FrameData>" << endl;
    os << "    <StuntDoubles>" << endl;
    
    OBAtom *atom;
    
    for(vector<int>::iterator i = indices.begin();i != indices.end(); ++i) {     
      atom = mol.GetAtom(*i);
      sprintf(buffer, "%10d %7s %18.10g %18.10g %18.10g %13e %13e %13e", *i - 1, "pv", atom->GetX(), atom->GetY(), atom->GetZ(), 0.0, 0.0, 0.0);
      os << buffer << endl;
    }
    os << "    </StuntDoubles>" << endl;
    os << "  </Snapshot>" << endl;
    os << "</OOPSE>" << endl;
  }
  
} //namespace OpenBabel

