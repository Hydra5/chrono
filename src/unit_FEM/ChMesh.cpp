//
// PROJECT CHRONO - http://projectchrono.org
//
// Copyright (c) 2013 Project Chrono
// All rights reserved.
//
// Use of this source code is governed by a BSD-style license that can be 
// found in the LICENSE file at the top level of the distribution
// and at http://projectchrono.org/license-chrono.txt.
//
// File authors: Andrea Favali, Alessandro Tasora


#include "core/ChMath.h"
#include "physics/ChObject.h"
#include "ChMesh.h"
// for the TetGen parsing:
#include "ChNodeFEMxyz.h"
#include "ChElementTetra_4.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <algorithm>
#include <functional> 


using namespace std;


namespace chrono 
{
namespace fem
{



void ChMesh::SetupInitial()
{
	n_dofs = 0;

	for (unsigned int i=0; i< vnodes.size(); i++)
	{
			//    - count the degrees of freedom 
		n_dofs += vnodes[i]->Get_ndof();
	}

	for (unsigned int i=0; i< velements.size(); i++)
	{
			//    - precompute matrices, such as the [Kl] local stiffness of each element, if needed, etc.
		velements[i]->SetupInitial();
	}

}


void ChMesh::Relax ()
{
	for (unsigned int i=0; i< vnodes.size(); i++)
	{
			//    - "relaxes" the structure by setting all X0 = 0, and null speeds
		vnodes[i]->Relax();
	}
}


void ChMesh::SetNoSpeedNoAcceleration()
{ 
	for (unsigned int i=0; i< vnodes.size(); i++)
	{
			//    -  set null speeds, null accelerations
		vnodes[i]->SetNoSpeedNoAcceleration();
	}
}


void ChMesh::AddNode ( ChSharedPtr<ChNodeFEMbase> m_node)
{
	this->vnodes.push_back(m_node);
}

void ChMesh::AddElement ( ChSharedPtr<ChElementBase> m_elem)
{
	this->velements.push_back(m_elem);
}

void ChMesh::ClearElements ()
{
	velements.clear();
}

void ChMesh::ClearNodes ()
{
	velements.clear();
	vnodes.clear();
}





// Updates all time-dependant variables, if any...
// Ex: maybe the elasticity can increase in time, etc.

void ChMesh::Update (double m_time)
{
	// Parent class update
	ChIndexedNodes::Update(m_time);
	
	for (unsigned int i=0; i< velements.size(); i++)
	{
			//    - update auxiliary stuff, ex. update element's rotation matrices if corotational..
		velements[i]->Update();
	}

}


void ChMesh::LoadFromTetGenFile(char* filename_node, char* filename_ele, ChSharedPtr<ChContinuumMaterial> my_material)
{
	int totnodes = 0;
	int nodes_offset = this->GetNnodes();
	int added_nodes = 0;

	// Load .node TetGen file
	{
		bool parse_header = true;
		bool parse_nodes = false;
		
		fstream fin(filename_node);
		if (!fin.good())
			throw ChException("ERROR opening TetGen .node file: " + std::string(filename_node) + "\n");

		int nnodes = 0;
		int ndims = 0;
		int nattrs = 0;
		int nboundarymark = 0;

		string line;
		while(getline(fin, line)) 
		{
			//trims white space from the beginning of the string
			line.erase(line.begin(), find_if(line.begin(), line.end(), not1(ptr_fun<int, int>(isspace)))); 

			if(line[0] == '#') continue; // skip comment
			if(line[0] == 0) continue; // skip empty lines

			if (parse_header)
			{
				stringstream(line) >> nnodes >> ndims >> nattrs >> nboundarymark;
				if (ndims != 3)
					throw ChException("ERROR in TetGen .node file. Only 3 dimensional nodes supported: \n"+ line);
				if (nattrs != 0)
					throw ChException("ERROR in TetGen .node file. Only nodes with 0 attrs supported: \n"+ line);
				if (nboundarymark != 0)
					throw ChException("ERROR in TetGen .node file. Only nodes with 0 markers supported: \n"+ line);
				parse_header = false;
				parse_nodes = true;
				totnodes = nnodes;
				continue;
			}

			int idnode = 0;
			double x = -10e30;
			double y = -10e30;
			double z = -10e30;

			if (parse_nodes)
			{
				stringstream(line) >> idnode >> x >> y >> z;
				++added_nodes;
				if (idnode <= 0 || idnode > nnodes)
					throw ChException("ERROR in TetGen .node file. Node ID not in range: \n"+ line +"\n");
				if (idnode != added_nodes)
					throw ChException("ERROR in TetGen .node file. Nodes IDs must be sequential (1 2 3 ..): \n"+ line+"\n");
				if (x == -10e30 || y == -10e30 || z == -10e30 )
					throw ChException("ERROR in TetGen .node file, in parsing x,y,z coordinates of node: \n"+ line+"\n");
				
				if (my_material.IsType<ChContinuumElastic>() )
				{
					ChSharedPtr<ChNodeFEMxyz> mnode( new ChNodeFEMxyz(ChVector<>(x,y,z)) );
					this->AddNode(mnode);
				}
				else if (my_material.IsType<ChContinuumPoisson3D>() )
				{
					ChSharedPtr<ChNodeFEMxyzP> mnode( new ChNodeFEMxyzP(ChVector<>(x,y,z)) );
					this->AddNode(mnode);
				}
				else throw ChException("ERROR in TetGen generation. Material type not supported. \n");

			}

		} // end while
        
    }// end .node file

	// Load .ele TetGen file
	{
		bool parse_header = true;
		bool parse_tet  = false;

		fstream fin(filename_ele);
		if (!fin.good())
			throw ChException("ERROR opening TetGen .node file: " + std::string(filename_node) + "\n");

		int ntets, nnodespertet, nattrs = 0;

		string line;
		while(getline(fin, line)) 
		{
			//trims white space from the beginning of the string
			line.erase(line.begin(), find_if(line.begin(), line.end(), not1(ptr_fun<int, int>(isspace)))); 

			if(line[0] == '#') continue; // skip comment
			if(line[0] == 0) continue; // skip empty lines

			if (parse_header)
			{
				stringstream(line) >> ntets >> nnodespertet >> nattrs;
				if (nnodespertet != 4)
					throw ChException("ERROR in TetGen .ele file. Only 4 -nodes per tes supported: \n"+ line+"\n");
				if (nattrs != 0)
					throw ChException("ERROR in TetGen .ele file. Only tets with 0 attrs supported: \n"+ line+"\n");
				parse_header = false;
				parse_tet = true;
				continue;
			}

			int idtet = 0;
			int n1,n2,n3,n4;

			if (parse_tet)
			{
				stringstream(line) >> idtet >> n1 >> n2 >> n3 >> n4;
				if (idtet <= 0 || idtet > ntets)
					throw ChException("ERROR in TetGen .node file. Tetahedron ID not in range: \n"+ line+"\n");
				if (n1 > totnodes)
					throw ChException("ERROR in TetGen .node file, ID of 1st node is out of range: \n"+ line+"\n");
				if (n2 > totnodes)
					throw ChException("ERROR in TetGen .node file, ID of 2nd node is out of range: \n"+ line+"\n");
				if (n3 > totnodes)
					throw ChException("ERROR in TetGen .node file, ID of 3rd node is out of range: \n"+ line+"\n");
				if (n4 > totnodes)
					throw ChException("ERROR in TetGen .node file, ID of 4th node is out of range: \n"+ line+"\n");
				
				if (my_material.IsType<ChContinuumElastic>() )
				{
					ChSharedPtr<ChElementTetra_4> mel( new ChElementTetra_4 );
					mel->SetNodes(
						this->GetNode(nodes_offset + n1-1), 
						this->GetNode(nodes_offset + n3-1), 
						this->GetNode(nodes_offset + n2-1), 
						this->GetNode(nodes_offset + n4-1) );
					mel->SetMaterial(my_material);
					this->AddElement(mel);
				} 
				else if (my_material.IsType<ChContinuumPoisson3D>() )
				{
					ChSharedPtr<ChElementTetra_4_P> mel( new ChElementTetra_4_P );
					mel->SetNodes(
						this->GetNode(nodes_offset + n1-1), 
						this->GetNode(nodes_offset + n3-1), 
						this->GetNode(nodes_offset + n2-1), 
						this->GetNode(nodes_offset + n4-1) );
					mel->SetMaterial(my_material);
					this->AddElement(mel);
				}
				else throw ChException("ERROR in TetGen generation. Material type not supported. \n");

			}

		} // end while
        
    }// end .ele file
}




void ChMesh::LoadFromAbaqusFile(char* filename, 
								ChSharedPtr<ChContinuumMaterial> my_material, 
								std::vector< std::vector< ChSharedPtr<ChNodeFEMbase> > >& node_sets)
{
	node_sets.resize(0);

	int totnodes = 0;
	unsigned int nodes_offset = this->GetNnodes();
	int added_nodes = 0;
	int added_elements = 0;
	//std::vector< ChSharedPtr<ChNodeFEMbase> >* current_nodeset = 0;

	enum eChAbaqusParserSection {
				E_PARSE_UNKNOWN = 0,
				E_PARSE_NODES_XYZ,
				E_PARSE_TETS_10,
				E_PARSE_NODESET
	} e_parse_section = E_PARSE_UNKNOWN;
	
	fstream fin(filename);
	if (!fin.good())
		throw ChException("ERROR opening Abaqus .inp file: " + std::string(filename) + "\n");

	int nnodes = 0;
	int ndims =  0;
	int nattrs = 0;

	string line;
	while(getline(fin, line)) 
	{
		//trims white space from the beginning of the string
		line.erase(line.begin(), find_if(line.begin(), line.end(), not1(ptr_fun<int, int>(isspace)))); 

		if(line[0] == 0) continue; // skip empty lines

		if(line[0] == '*') 
		{
			e_parse_section  = E_PARSE_UNKNOWN;

			if (line.find("*NODE") == 0)
			{
				int nse = line.find("NSET=");
				if (nse>0)
				{
					int ncom = line.find(",",nse);
					string s_node_set = line.substr(nse+5,ncom-(nse+5));
					GetLog() << "Parsing: nodes " << s_node_set << "\n";
				}
				e_parse_section  = E_PARSE_NODES_XYZ;
			}
			if (line.find("*ELEMENT") == 0)
			{
				int nty = line.find("TYPE=");
				if (nty>0)
				{
					int ncom = line.find(",",nty);
					string s_ele_type = line.substr(nty+5,ncom-(nty+5));
					if (s_ele_type != "C3D10" &&
						s_ele_type != "DC3D10" )
						throw ChException("ERROR in .inp file, TYPE=" + s_ele_type + " (only C3D10 or DC3D10 tetahedrons supported) see: \n" + line + "\n");
				}
				int nse = line.find("ELSET=");
				if (nse>0)
				{
					int ncom = line.find(",",nse);
					string s_ele_set = line.substr(nse+6,ncom-(nse+6));
					GetLog() << "Parsing: element set: " << s_ele_set << "\n";
				}
				e_parse_section  = E_PARSE_TETS_10;
			}
			if (line.find("*NSET") == 0)
			{
				GetLog() << "Parsing: nodeset.. ";
				int nse = line.find("NSET=",5);
				if (nse>0)
				{
					int ncom = line.find(",",nse);
					string s_node_set = line.substr(nse+5,ncom-(nse+5));
					GetLog() << "Parsing: nodeset: " << s_node_set << "\n";
					
					std::vector< ChSharedPtr<ChNodeFEMbase> > empty_set;
					node_sets.push_back(empty_set);
				}
				e_parse_section  = E_PARSE_NODESET;
			}

			continue; // skip 
		}
		

		if (e_parse_section == E_PARSE_NODES_XYZ)
		{
			int idnode = 0;
			double x = -10e30;
			double y = -10e30;
			double z = -10e30;
			double tokenvals[20];
			int ntoken = 0;

			string token;
			std::istringstream ss(line);
			while(getline(ss, token,',') && ntoken < 20) 
			{
				std::istringstream stoken(token);
				stoken >> tokenvals[ntoken]; 
				++ntoken;
			}
			++added_nodes;

			if (ntoken != 4)
				throw ChException("ERROR in .inp file, nodes require ID and three x y z coords, see line:\n"+ line+"\n");
			idnode = (int) tokenvals[0];
			if (idnode != added_nodes)
				throw ChException("ERROR in .inp file. Nodes IDs must be sequential (1 2 3 ..): \n"+ line+"\n");
			x = tokenvals[1];
			y = tokenvals[2];
			z = tokenvals[3];
			if (x == -10e30 || y == -10e30 || z == -10e30 )
				throw ChException("ERROR in in .inp file, in parsing x,y,z coordinates of node: \n"+ line+"\n");
			
			if (my_material.IsType<ChContinuumElastic>() )
			{
				ChSharedPtr<ChNodeFEMxyz> mnode( new ChNodeFEMxyz(ChVector<>(x,y,z)) );
				this->AddNode(mnode);
			}
			else if (my_material.IsType<ChContinuumPoisson3D>() )
			{
				ChSharedPtr<ChNodeFEMxyzP> mnode( new ChNodeFEMxyzP(ChVector<>(x,y,z)) );
				this->AddNode(mnode);
			}
			else throw ChException("ERROR in .inp generation. Material type not supported. \n");

		}


		if (e_parse_section == E_PARSE_TETS_10)
		{
			int idelem = 0;
			unsigned int tokenvals[20];
			int ntoken = 0;

			string token;
			std::istringstream ss(line);
			while(std::getline(ss, token, ',') && ntoken < 20) 
			{
				std::istringstream stoken(token);
				stoken >> tokenvals[ntoken]; 
				++ntoken;
			}
			++added_elements;

			if (ntoken != 11)
				throw ChException("ERROR in .inp file, tetahedrons require ID and 10 node IDs, see line:\n"+ line+"\n");
			idelem = (int) tokenvals[0];
			if (idelem != added_elements)
				throw ChException("ERROR in .inp file. Element IDs must be sequential (1 2 3 ..): \n"+ line+"\n");
			for (int in = 0; in<10; ++in)
				if (tokenvals[in+1] == -10e30)
					throw ChException("ERROR in in .inp file, in parsing IDs of tetahedron: \n"+ line+"\n");
			
			if (my_material.IsType<ChContinuumElastic>() )
			{
				ChSharedPtr<ChElementTetra_4> mel( new ChElementTetra_4 );
				mel->SetNodes(
					this->GetNode(nodes_offset + tokenvals[1]-1), 
					this->GetNode(nodes_offset + tokenvals[3]-1), 
					this->GetNode(nodes_offset + tokenvals[2]-1), 
					this->GetNode(nodes_offset + tokenvals[4]-1) );
				mel->SetMaterial(my_material);
				this->AddElement(mel);
			} 
			else if (my_material.IsType<ChContinuumPoisson3D>() )
			{
				ChSharedPtr<ChElementTetra_4_P> mel( new ChElementTetra_4_P );
				mel->SetNodes(
					this->GetNode(nodes_offset + tokenvals[1]-1), 
					this->GetNode(nodes_offset + tokenvals[3]-1), 
					this->GetNode(nodes_offset + tokenvals[2]-1), 
					this->GetNode(nodes_offset + tokenvals[4]-1) );
				mel->SetMaterial(my_material);
				this->AddElement(mel);
			}
			else throw ChException("ERROR in TetGen generation. Material type not supported. \n");

		}

		if (e_parse_section == E_PARSE_NODESET)
		{
			int idelem = 0;

			unsigned int tokenvals[100];
			int ntoken = 0;

			string token;
			std::istringstream ss(line);
			while(std::getline(ss, token, ',') && ntoken < 100) 
			{
				std::istringstream stoken(token);
				stoken >> tokenvals[ntoken]; 
				++ntoken;
			}

			for (int nt = 0; nt< ntoken; ++nt)
			{
				int idnode = (int) tokenvals[nt];
				node_sets.back().push_back( this->GetNode(nodes_offset + idnode -1) );
			}

		}


	} // end while
        
}




void ChMesh::InjectKRMmatrices(ChLcpSystemDescriptor& mdescriptor) 
{
	for (unsigned int ie = 0; ie < this->velements.size(); ie++)
		this->velements[ie]->InjectKRMmatrices(mdescriptor);
}

void ChMesh::KRMmatricesLoad(double Kfactor, double Rfactor, double Mfactor)
{
	for (unsigned int ie = 0; ie < this->velements.size(); ie++)
		this->velements[ie]->KRMmatricesLoad(Kfactor, Rfactor, Mfactor);
}

void ChMesh::VariablesFbReset()
{
	for (unsigned int ie = 0; ie < this->vnodes.size(); ie++)
		this->vnodes[ie]->VariablesFbReset();
}

void ChMesh::VariablesFbLoadForces(double factor)
{
	// applied nodal forces
	for (unsigned int in = 0; in < this->vnodes.size(); in++)
		this->vnodes[in]->VariablesFbLoadForces(factor);

	// internal forces
	for (unsigned int ie = 0; ie < this->velements.size(); ie++)
		this->velements[ie]->VariablesFbLoadInternalForces(factor);
}

void ChMesh::VariablesQbLoadSpeed() 
{
	for (unsigned int ie = 0; ie < this->vnodes.size(); ie++)
		this->vnodes[ie]->VariablesQbLoadSpeed();
}

void ChMesh::VariablesFbIncrementMq() 
{
	// nodal masses
	for (unsigned int ie = 0; ie < this->vnodes.size(); ie++)
		this->vnodes[ie]->VariablesFbIncrementMq();

	// internal masses
	for (unsigned int ie = 0; ie < this->velements.size(); ie++)
		this->velements[ie]->VariablesFbIncrementMq();
}

void ChMesh::VariablesQbSetSpeed(double step) 
{
	for (unsigned int ie = 0; ie < this->vnodes.size(); ie++)
		this->vnodes[ie]->VariablesQbSetSpeed(step);
}

void ChMesh::VariablesQbIncrementPosition(double step)
{
	for (unsigned int ie = 0; ie < this->vnodes.size(); ie++)
		this->vnodes[ie]->VariablesQbIncrementPosition(step);
}

void ChMesh::InjectVariables(ChLcpSystemDescriptor& mdescriptor)
{
	for (unsigned int ie = 0; ie < this->vnodes.size(); ie++)
		mdescriptor.InsertVariables(&this->vnodes[ie]->Variables());
}






} // END_OF_NAMESPACE____
} // END_OF_NAMESPACE____

