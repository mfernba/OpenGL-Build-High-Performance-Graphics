#include "Ezm.h"  
#include "3rdParty/pugi_xml/pugixml.hpp"
 
#include <iostream>



NVSHARE::MeshSystemContainer *msc = NULL;
NVSHARE::MeshImport *meshImportLibrary = NULL;

EzmLoader::EzmLoader() {
	//load meshes using MeshImport library
	meshImportLibrary = NVSHARE::loadMeshImporters(".");
}

EzmLoader::~EzmLoader() {
	if(meshImportLibrary)
		meshImportLibrary->releaseMeshSystemContainer(msc);
}

#include <fstream>
#include <sstream>

std::string trim(const std::string& str,
                 const std::string& whitespace = " \t")
{
    const auto strBegin = str.find_first_not_of(whitespace);
    if (strBegin == std::string::npos)
        return ""; // no content

    const auto strEnd = str.find_last_not_of(whitespace);
    const auto strRange = strEnd - strBegin + 1;

    return str.substr(strBegin, strRange);
}
 
bool EzmLoader::Load(const string& filename, vector<Bone>& skeleton, vector<NVSHARE::MeshAnimation>& animations, vector<SubMesh>& submeshes, vector<Vertex>& vertices, vector<unsigned short>& indices,	map<std::string, std::string>& materialNames, glm::vec3& min, glm::vec3& max) {
	  
	
	min=glm::vec3(1000.0f);
	max=glm::vec3(-1000.0f);
	

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(filename.c_str()); 
		
	//load materials from the EZMesh file using pugixml library
	pugi::xml_node mats = doc.child("MeshSystem").child("Materials");
	int totalMaterials = atoi(mats.attribute("count").value()); 
	pugi::xml_node material = mats.child("Material");
	std::cerr<<"Reading materials ...";
	
	//for all materials 
	for(int i=0;i<totalMaterials;i++) {
		//get the material name 
		std::string name = material.attribute("name").value();
		std::string metadata = material.attribute("meta_data").value();
		if(metadata.find("%") != std::string::npos) {
			int startGarbage = metadata.find_first_of("%20");
			int endGarbage = metadata.find_last_of("%20");
		
			if(endGarbage!= std::string::npos)
				metadata = metadata.erase(endGarbage-2, 3);
			if(startGarbage!= std::string::npos)
				metadata = metadata.erase(startGarbage, 3); 
		}
		int diffuseIndex = metadata.find("diffuse=");
		if( diffuseIndex != std::string::npos)
			metadata = metadata.erase(diffuseIndex, strlen("diffuse="));

		//if the metadata is not empty
		int len = metadata.length();
		if(len>0) 
		{
			//add the material to the material map 	
			string fullName="";
			 
			 
			//get filename only
			int index = metadata.find_last_of("\\");

			if(index == string::npos) {
				fullName.append(metadata);
			} else {
				std::string fileNameOnly = metadata.substr(index+1, metadata.length());
				fullName.append(fileNameOnly);
			}
			//see if the material exists already?			 
			bool exists = true;
			if(materialNames.find(name)==materialNames.end() ) 
				 exists = false;
			 
			if(!exists)
				materialNames[name] = (fullName);
		
		} else {
			//otherwise just store the material name with an empty string in it
			bool exists = true;
			if(materialNames.find(name)==materialNames.end() ) 
				 exists = false;
			 
			if(!exists)
				materialNames[name] = "";
		
		}
		//get the next material node
		material = material.next_sibling("Material");
	}
	std::cerr<<" Done."<<std::endl;

	//load the mesh information
	if ( meshImportLibrary )
	{
		ifstream infile(filename.c_str(), std::ios::in);
		//determine file size in bytes
		long beg=0,end=0;
		infile.seekg(0, std::ios::beg); 
		beg = (long)infile.tellg();     
		infile.seekg(0, std::ios::end);
		end = (long)infile.tellg();
		infile.seekg(0, std::ios::beg); 
		long fileSize = (end - beg);
		
		//create a buffer large enough to store the whole file
		string buffer(std::istreambuf_iterator<char>(infile), (std::istreambuf_iterator<char>()));		 
		infile.close();

		msc = meshImportLibrary->createMeshSystemContainer(filename.c_str(),buffer.c_str(),buffer.length(),0);
		if ( msc ) {
			//get the meshsystem object and then obtain the mesh's bounding box 
			NVSHARE::MeshSystem *ms = meshImportLibrary->getMeshSystem(msc);	
			min = glm::vec3(ms->mAABB.mMin[0], ms->mAABB.mMin[1], ms->mAABB.mMin[2]);
			max = glm::vec3(ms->mAABB.mMax[0], ms->mAABB.mMax[1], ms->mAABB.mMax[2]);
			
			//determine if it is a Yup or a Zup mesh
			float dy = fabs(max.y-min.y);
			float dz = fabs(max.z-min.z);
			bool bYup = (dy>dz);
			if(!bYup) {
				float tmp =  min.y;
				min.y =  min.z;
				min.z = -tmp;

				tmp = max.y;
				max.y = max.z;
				max.z = -tmp;
			}

			//load skeletons
			std::cerr<<"Reading skeletons ...";
			if(ms->mSkeletonCount>0) {
				NVSHARE::MeshSkeleton* pSkel = ms->mSkeletons[0];
				Bone b;
				
				//loop for all bones and determine their name, orientation, scale and 
				//position properties					
				for(int i=0;i<pSkel->GetBoneCount();i++) {
					const NVSHARE::MeshBone pBone = pSkel->mBones[i];
					const int s = strlen(pBone.mName);
								
					b.name = new char[s+1];
					memset(b.name, 0, sizeof(char)*(s+1));
					strncpy_s(b.name,sizeof(char)*(s+1), pBone.mName, s);
					b.orientation = glm::quat( pBone.mOrientation[3],
											   pBone.mOrientation[0],
											   pBone.mOrientation[1],
											   pBone.mOrientation[2]);
					b.position = glm::vec3( pBone.mPosition[0],
												pBone.mPosition[1],
												pBone.mPosition[2]);

					b.scale  = glm::vec3(pBone.mScale[0], pBone.mScale[1], pBone.mScale[2]);

					//handle Zup case
					if(!bYup) {
						float tmp = b.position.y;
						b.position.y = b.position.z;
						b.position.z = -tmp;

						tmp = b.orientation.y;
						b.orientation.y = b.orientation.z;
						b.orientation.z = -tmp;

						tmp = b.scale.y;
						b.scale.y = b.scale.z;
						b.scale.z = -tmp;
					}
					
					//get the new local transform
					glm::mat4 S = glm::scale(glm::mat4(1), b.scale);								
					glm::mat4 R = glm::toMat4(b.orientation);							
					glm::mat4 T = glm::translate(glm::mat4(1), b.position);												
					b.xform = T*R*S; 				

					//store the parent index
					b.parent = pBone.mParentIndex;														

					//add bone to the skeleton vector
					skeleton.push_back(b);
				}
			}
			std::cerr<<" Done. "<<std::endl;
			
			std::cerr<<"Reading animations ... ";
			//loop through all animations and pass to the animations vector
			for(size_t i=0;i<ms->mAnimationCount;i++) {
				NVSHARE::MeshAnimation* pAnim = (ms->mAnimations[i]);						 
				animations.push_back(*pAnim);
			}
			std::cerr<<" Done. "<<std::endl;

			std::cerr<<"Reading meshes and vertex attributes ...";
			int totalVertices = 0, lastSubMeshIndex = 0;

			//for all meshes
			for(size_t i=0;i<ms->mMeshCount;i++) {

				//get the current mesh
				NVSHARE::Mesh* pMesh = ms->mMeshes[i]; 
				
				//for all mesh vertices, get the position, normal, uv coordninates
				//blend weights and blend indices and store into the vertices vector
				for(size_t j=0;j<pMesh->mVertexCount;j++) {
					Vertex v;
					v.pos.x = pMesh->mVertices[j].mPos[0];
					if(bYup) {
						v.pos.y = pMesh->mVertices[j].mPos[1];
						v.pos.z = pMesh->mVertices[j].mPos[2];
					} else {
						v.pos.y = pMesh->mVertices[j].mPos[2];
						v.pos.z = -pMesh->mVertices[j].mPos[1];
					}
							
					v.normal.x = pMesh->mVertices[j].mNormal[0];
					if(bYup) {
						v.normal.y = pMesh->mVertices[j].mNormal[1];
						v.normal.z = pMesh->mVertices[j].mNormal[2];
					} else {
						v.normal.y = pMesh->mVertices[j].mNormal[2];
						v.normal.z = -pMesh->mVertices[j].mNormal[1];
					}
					v.uv.x = pMesh->mVertices[j].mTexel1[0];
					v.uv.y = pMesh->mVertices[j].mTexel1[1];

					v.blendWeights.x = pMesh->mVertices[j].mWeight[0];
					v.blendWeights.y = pMesh->mVertices[j].mWeight[1];
					v.blendWeights.z = pMesh->mVertices[j].mWeight[2];
					v.blendWeights.w = pMesh->mVertices[j].mWeight[3];
					v.blendIndices[0] = pMesh->mVertices[j].mBone[0];
					v.blendIndices[1] = pMesh->mVertices[j].mBone[1];
					v.blendIndices[2] = pMesh->mVertices[j].mBone[2];
					v.blendIndices[3] = pMesh->mVertices[j].mBone[3];

					vertices.push_back(v);
				}
				
				//for all mesh submeshes
				for(size_t j=0;j<pMesh->mSubMeshCount;j++) {
					SubMesh s;
					NVSHARE::SubMesh* pSubMesh = pMesh->mSubMeshes[j];
					
					//get the submesh material name 	
					s.materialName = pSubMesh->mMaterialName;
					//get submesh indices
					s.indices.resize(pSubMesh->mTriCount * 3);
					memcpy(&(s.indices[0]), pSubMesh->mIndices, sizeof(unsigned int) *  pSubMesh->mTriCount * 3);				

					//store the submesh into a vector
					submeshes.push_back(s);
				}

				//add the total number of vertices to offset the mesh indices
				//this is done here so that all of the meshes could be linked
				//into a single vector
				if(totalVertices!=0) {
					for(size_t m=lastSubMeshIndex;m<submeshes.size();m++) {
						for(size_t n=0;n<submeshes[m].indices.size();n++) {
							submeshes[m].indices[n] += totalVertices;	
						}
					}
				}

				//increment the total vertices and assign the submesh index to 
				//the total subemeshes
				totalVertices += pMesh->mVertexCount;
				lastSubMeshIndex = submeshes.size();
			}
			std::cerr<<" Done. "<<std::endl;
		}
	}

	return true;
}


