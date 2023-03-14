// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MuR/Platform.h"

#include "MuR/MutableMath.h"
#include "MuR/Ptr.h"


#define MAX_DIVISIONS 5
#define MAX_VECTORS 20

namespace mu
{
	struct VertexInfo
	{
		vec3f vector;
		int32 index;
	};

	class Octree
	{
	public:

		Octree(vec3f _minCorner, vec3f _maxCorner, float _padding = 0.0f,int div = 0)
		{
			minCorner = _minCorner;
			maxCorner = _maxCorner;
			padding = _padding;
			division = div;

			for (int i = 0; i < 8; ++i)
			{
				children[i] = nullptr;
			}
		}
		
		~Octree()
		{
			for (int i = 0; i < 8; ++i)
			{
				if (children[i])
				{
					delete children[i];
					children[i] = nullptr;
				}	
			}
		}

		void InsertElement(vec3f v, int idx)
		{
			if (!children[0])
			{
				elements.Add({ v, idx });

				if (elements.Num() > MAX_VECTORS && division < MAX_DIVISIONS)
				{
					Divide();
				}
			}
			else
			{
				for (int i = 0; i < 8; ++i)
				{
					if (children[i] != nullptr)
					{
						if (Collides(children[i], v))
						{
							children[i]->InsertElement(v, idx);
						}
					}
				}
			}
		}

		void Divide()
		{
			float sizeX = maxCorner[0] - minCorner[0];
			float sizeY = maxCorner[1] - minCorner[1];
			float sizeZ = maxCorner[2] - minCorner[2];

			vec3f diagonal = { sizeX / 2.0f, sizeY / 2.0f , sizeZ / 2.0f };

			for (int i = 0; i < 2; ++i)
			{
				float height = (sizeY*i) / 2.0f;

				vec3f nMin = minCorner + vec3f(0.0f, height, 0.0f);
				vec3f nMax = nMin + diagonal;
				children[0 + i * 4] = new Octree(nMin, nMax, padding, division + 1);

				nMin = minCorner + vec3f(0.0f, height, sizeZ / 2.0f);
				nMax = nMin + diagonal;
				children[1 + i * 4] = new Octree(nMin, nMax, padding, division + 1);

				nMin = minCorner + vec3f(sizeX / 2.0f, height, 0.0f);
				nMax = nMin + diagonal;
				children[2 + i * 4] = new Octree(nMin, nMax, padding, division + 1);

				nMin = minCorner + vec3f(sizeX / 2.0f, height, sizeZ / 2.0f);
				nMax = nMin + diagonal;
				children[3 + i * 4] = new Octree(nMin, nMax, padding, division + 1);
			}

            for (size_t i = 0; i < elements.Num(); ++i)
			{
				InsertElement(elements[i].vector, elements[i].index);
			}
		            
			elements.Empty();
		}

		bool Collides(Octree* child, vec3f v)
		{
			bool ret = false;

			if (child)
			{
				if (v[0] >= child->minCorner[0] - padding && v[1] >= child->minCorner[1] - padding && v[2] >= child->minCorner[2] - padding)
				{
					if (v[0] <= child->maxCorner[0] + padding && v[1] <= child->maxCorner[1] + padding && v[2] <= child->maxCorner[2] + padding)
					{
						ret = true;
					}
				}
			}

			return ret;
		}

		int GetNearests(vec3f v, float dist, int idx)
		{
			int ret = idx;

			if (!children[0])
			{
                for (size_t i = 0; i < elements.Num(); ++i)
				{
					vec3f r = elements[i].vector - v;
					
					if (idx > elements[i].index && dot(r,r) <= dist*dist)
					{
						ret = elements[i].index;
						break;
					}
				}
				return ret;
			}

			for (int i = 0; i < 8; ++i)
			{
				if (Collides(children[i], v))
				{
					int result = children[i]->GetNearests(v, dist, idx);
					if (result < idx && result < ret)
					{
						ret = result;
					}
				}
			}

			return ret;
		}

	private:
		
		int division = 0;

		vec3f minCorner;
		vec3f maxCorner;

		float padding = 0.0f;

		TArray<VertexInfo> elements;

		Octree* children[8];
	};
}
