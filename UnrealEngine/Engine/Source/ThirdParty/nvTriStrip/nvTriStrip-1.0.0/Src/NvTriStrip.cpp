
#pragma warning(disable : 4018)		// warning C4018: '<' : signed/unsigned mismatch

#include "NvTriStripObjects.h"
#include "NvTriStrip.h"

////////////////////////////////////////////////////////////////////////////////////////
//private data
static unsigned int cacheSize    = CACHESIZE_GEFORCE1_2;
static bool bStitchStrips        = true;
static unsigned int minStripSize = 0;
static bool bListsOnly           = false;

////////////////////////////////////////////////////////////////////////////////////////
// SetListsOnly()
//
// If set to true, will return an optimized list, with no strips at all.
//
// Default value: false
//
void SetListsOnly(const bool _bListsOnly)
{
	bListsOnly = _bListsOnly;
}

////////////////////////////////////////////////////////////////////////////////////////
// SetCacheSize()
//
// Sets the cache size which the stripfier uses to optimize the data.
// Controls the length of the generated individual strips.
// This is the "actual" cache size, so 24 for GeForce3 and 16 for GeForce1/2
// You may want to play around with this number to tweak performance.
//
// Default value: 16
//
void SetCacheSize(const unsigned int _cacheSize)
{
	cacheSize = _cacheSize;
}


////////////////////////////////////////////////////////////////////////////////////////
// SetStitchStrips()
//
// bool to indicate whether to stitch together strips into one huge strip or not.
// If set to true, you'll get back one huge strip stitched together using degenerate
//  triangles.
// If set to false, you'll get back a large number of separate strips.
//
// Default value: true
//
void SetStitchStrips(const bool _bStitchStrips)
{
	bStitchStrips = _bStitchStrips;
}


////////////////////////////////////////////////////////////////////////////////////////
// SetMinStripSize()
//
// Sets the minimum acceptable size for a strip, in triangles.
// All strips generated which are shorter than this will be thrown into one big, separate list.
//
// Default value: 0
//
void SetMinStripSize(const unsigned int _minStripSize)
{
	minStripSize = _minStripSize;
}

////////////////////////////////////////////////////////////////////////////////////////
// GenerateStrips()
//
// in_indices: input index list, the indices you would use to render
// in_numIndices: number of entries in in_indices
// primGroups: array of optimized/stripified PrimitiveGroups
// numGroups: number of groups returned
//
// Be sure to call delete[] on the returned primGroups to avoid leaking mem
//
void GenerateStrips(const unsigned int* in_indices, const unsigned int in_numIndices,
					PrimitiveGroup** primGroups, unsigned int* numGroups)
{
	//put data in format that the stripifier likes
	UIntVec tempIndices;
	tempIndices.resize(in_numIndices);
	unsigned int maxIndex = 0;
	for(int i = 0; i < in_numIndices; i++)
	{
		tempIndices[i] = in_indices[i];
		if(in_indices[i] > maxIndex)
			maxIndex = in_indices[i];
	}
	NvStripInfoVec tempStrips;
	NvFaceInfoVec tempFaces;

	NvStripifier stripifier;
	
	//do actual stripification
	stripifier.Stripify(tempIndices, cacheSize, minStripSize, maxIndex, tempStrips, tempFaces);

	//stitch strips together
	IntVec stripIndices;
	unsigned int numSeparateStrips = 0;
	//int i = 0;

	if(bListsOnly)
	{
		//if we're outputting only lists, we're done
		*numGroups = 1;
		(*primGroups) = new PrimitiveGroup[*numGroups];
		PrimitiveGroup* primGroupArray = *primGroups;

		//count the total number of indices
		unsigned int numIndices = 0;
		for(int i = 0; i < tempStrips.size(); i++)
		{
			numIndices += (int)(tempStrips[i]->m_faces.size()) * 3;
		}

		//add in the list
		numIndices += (int)(tempFaces.size()) * 3;

		primGroupArray[0].type       = PT_LIST;
		primGroupArray[0].numIndices = numIndices;
		primGroupArray[0].indices    = new unsigned int[numIndices];

		//do strips
		unsigned int indexCtr = 0;
		for(int i = 0; i < tempStrips.size(); i++)
		{
			for(int j = 0; j < tempStrips[i]->m_faces.size(); j++)
			{
				//degenerates are of no use with lists
				if(!NvStripifier::IsDegenerate(tempStrips[i]->m_faces[j]))
				{
					primGroupArray[0].indices[indexCtr++] = tempStrips[i]->m_faces[j]->m_v0;
					primGroupArray[0].indices[indexCtr++] = tempStrips[i]->m_faces[j]->m_v1;
					primGroupArray[0].indices[indexCtr++] = tempStrips[i]->m_faces[j]->m_v2;
				}
				else
				{
					//we've removed a tri, reduce the number of indices
					primGroupArray[0].numIndices -= 3;
				}
			}
		}

		//do lists
		for(int i = 0; i < tempFaces.size(); i++)
		{
			primGroupArray[0].indices[indexCtr++] = tempFaces[i]->m_v0;
			primGroupArray[0].indices[indexCtr++] = tempFaces[i]->m_v1;
			primGroupArray[0].indices[indexCtr++] = tempFaces[i]->m_v2;
		}
	}
	else
	{
		stripifier.CreateStrips(tempStrips, stripIndices, bStitchStrips, numSeparateStrips);

		//if we're stitching strips together, we better get back only one strip from CreateStrips()
		assert( (bStitchStrips && (numSeparateStrips == 1)) || !bStitchStrips);
		
		//convert to output format
		*numGroups = numSeparateStrips; //for the strips
		if(tempFaces.size() != 0)
			(*numGroups)++;  //we've got a list as well, increment
		(*primGroups) = new PrimitiveGroup[*numGroups];
		
		PrimitiveGroup* primGroupArray = *primGroups;
		
		//first, the strips
		int startingLoc = 0;
		for(int stripCtr = 0; stripCtr < numSeparateStrips; stripCtr++)
		{
			int stripLength = 0;

			if(!bStitchStrips)
			{
				//if we've got multiple strips, we need to figure out the correct length
				int i;
				for(i = startingLoc; i < stripIndices.size(); i++)
				{
					if(stripIndices[i] == -1)
						break;
				}
				
				stripLength = i - startingLoc;
			}
			else
				stripLength = (int)(stripIndices.size());
			
			primGroupArray[stripCtr].type       = PT_STRIP;
			primGroupArray[stripCtr].indices    = new unsigned int[stripLength];
			primGroupArray[stripCtr].numIndices = stripLength;
			
			int indexCtr = 0;
			for(int i = startingLoc; i < stripLength + startingLoc; i++)
				primGroupArray[stripCtr].indices[indexCtr++] = stripIndices[i];

			//we add 1 to account for the -1 separating strips
			//this doesn't break the stitched case since we'll exit the loop
			startingLoc += stripLength + 1; 
		}
		
		//next, the list
		if(tempFaces.size() != 0)
		{
			int faceGroupLoc = (*numGroups) - 1;    //the face group is the last one
			primGroupArray[faceGroupLoc].type       = PT_LIST;
			primGroupArray[faceGroupLoc].indices    = new unsigned int[tempFaces.size() * 3];
			primGroupArray[faceGroupLoc].numIndices = (unsigned int)(tempFaces.size()) * 3;
			int indexCtr = 0;
			for(int i = 0; i < tempFaces.size(); i++)
			{
				primGroupArray[faceGroupLoc].indices[indexCtr++] = tempFaces[i]->m_v0;
				primGroupArray[faceGroupLoc].indices[indexCtr++] = tempFaces[i]->m_v1;
				primGroupArray[faceGroupLoc].indices[indexCtr++] = tempFaces[i]->m_v2;
			}
		}
	}

	//clean up everything

	//delete strips
	for(int i = 0; i < tempStrips.size(); i++)
	{
		for(int j = 0; j < tempStrips[i]->m_faces.size(); j++)
		{
			delete tempStrips[i]->m_faces[j];
			tempStrips[i]->m_faces[j] = NULL;
		}
		delete tempStrips[i];
		tempStrips[i] = NULL;
	}

	//delete faces
	for(int i = 0; i < tempFaces.size(); i++)
	{
		delete tempFaces[i];
		tempFaces[i] = NULL;
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// RemapIndices()
//
// Function to remap your indices to improve spatial locality in your vertex buffer.
//
// in_primGroups: array of PrimitiveGroups you want remapped
// numGroups: number of entries in in_primGroups
// numVerts: number of vertices in your vertex buffer, also can be thought of as the range
//  of acceptable values for indices in your primitive groups.
// remappedGroups: array of remapped PrimitiveGroups
//
// Note that, according to the remapping handed back to you, you must reorder your 
//  vertex buffer.
//
void RemapIndices(const PrimitiveGroup* in_primGroups, const unsigned int numGroups,
				  const unsigned int numVerts, PrimitiveGroup** remappedGroups)
{
	(*remappedGroups) = new PrimitiveGroup[numGroups];

	//caches oldIndex --> newIndex conversion
	int *indexCache;
	indexCache = new int[numVerts];
	memset(indexCache, -1, sizeof(int)*numVerts);
	
	//loop over primitive groups
	unsigned int indexCtr = 0;
	for(int i = 0; i < numGroups; i++)
	{
		unsigned int numIndices = in_primGroups[i].numIndices;

		//init remapped group
		(*remappedGroups)[i].type       = in_primGroups[i].type;
		(*remappedGroups)[i].numIndices = numIndices;
		(*remappedGroups)[i].indices    = new unsigned int[numIndices];

		for(int j = 0; j < numIndices; j++)
		{
			int cachedIndex = indexCache[in_primGroups[i].indices[j]];
			if(cachedIndex == -1) //we haven't seen this index before
			{
				//point to "last" vertex in VB
				(*remappedGroups)[i].indices[j] = indexCtr;

				//add to index cache, increment
				indexCache[in_primGroups[i].indices[j]] = indexCtr++;
			}
			else
			{
				//we've seen this index before
				(*remappedGroups)[i].indices[j] = cachedIndex;
			}
		}
	}

	delete[] indexCache;
}