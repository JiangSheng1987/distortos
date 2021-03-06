/**
 * \file
 * \brief startScheduling() implementation for ARMv6-M and ARMv7-M
 *
 * \author Copyright (C) 2014-2016 Kamil Szczygiel http://www.distortec.com http://www.freddiechopin.info
 *
 * \par License
 * This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0. If a copy of the MPL was not
 * distributed with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

#include "distortos/architecture/startScheduling.hpp"

#include "distortos/chip/clocks.hpp"
#include "distortos/chip/CMSIS-proxy.h"

namespace distortos
{

namespace architecture
{

/*---------------------------------------------------------------------------------------------------------------------+
| global functions
+---------------------------------------------------------------------------------------------------------------------*/

void startScheduling()
{
	// SysTick and PendSV - lowest possible priority
	NVIC_SetPriority(SysTick_IRQn, 0xff);
	NVIC_SetPriority(PendSV_IRQn, 0xff);

	// SVCall - high priority
#ifdef CONFIG_ARCHITECTURE_ARMV7_M_KERNEL_BASEPRI
	constexpr uint32_t svcallPriority {CONFIG_ARCHITECTURE_ARMV7_M_KERNEL_BASEPRI};
#else	// !def CONFIG_ARCHITECTURE_ARMV7_M_KERNEL_BASEPRI
	constexpr uint32_t svcallPriority {};
#endif	// !def CONFIG_ARCHITECTURE_ARMV7_M_KERNEL_BASEPRI
	NVIC_SetPriority(SVCall_IRQn, svcallPriority);

	// configure SysTick timer as the tick timer
	constexpr uint32_t period {chip::ahbFrequency / CONFIG_TICK_FREQUENCY};
	constexpr uint32_t periodDividedBy8 {period / 8};
	constexpr uint32_t maxSysTickPeriod {1 << 24};
	constexpr bool divideBy8 {period > maxSysTickPeriod};
	// at least one of the periods must be valid
	static_assert(period <= maxSysTickPeriod || periodDividedBy8 <= maxSysTickPeriod, "Invalid SysTick configuration!");
	SysTick->LOAD = (divideBy8 == false ? period : periodDividedBy8) - 1;
	SysTick->VAL = 0;
	SysTick->CTRL = (divideBy8 == true ? 0 : SysTick_CTRL_CLKSOURCE_Msk) | SysTick_CTRL_ENABLE_Msk |
			SysTick_CTRL_TICKINT_Msk;
}

}	// namespace architecture

}	// namespace distortos
