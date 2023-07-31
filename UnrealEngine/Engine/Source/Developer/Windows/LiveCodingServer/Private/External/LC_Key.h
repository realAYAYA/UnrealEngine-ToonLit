// Copyright 2011-2020 Molecular Matters GmbH, all rights reserved.

#pragma once

// BEGIN EPIC MOD
#include "CoreTypes.h"
// END EPIC MOD

namespace input
{
	class Key
	{
	public:
		explicit Key(int vkCode);

		void AssignCode(int vkCode);

		void Clear(void);
		void Update(void);
		bool IsPressed(void) const;
		bool WentDown(void) const;

	private:
		int m_vkCode;
		bool m_isPressed;
		bool m_wasPressed;
	};
}
