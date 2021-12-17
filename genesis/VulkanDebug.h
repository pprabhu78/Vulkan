#pragma once

#include "GenAssert.h"

#include <vulkan/vulkan.h>

#include <string>
#include <iostream>

// Default fence timeout in nanoseconds
#define DEFAULT_FENCE_TIMEOUT 100000000000

namespace genesis
{
	class VulkanErrorToString
	{
	public:
		static std::string toString(VkResult result);
	};
}

#ifdef VK_CHECK_RESULT
#undef VK_CHECK_RESULT
#endif

#define VK_CHECK_RESULT(f)																		\
{																										\
	VkResult res = (f);																			\
	if (res != VK_SUCCESS)																		\
	{																									\
		std::cout << "Fatal : VkResult is \""												\
					<< genesis::VulkanErrorToString::toString(res) << "\" in "		\
					<< __FILE__ << " at line " << __LINE__ << "\n";						\
		assert(res == VK_SUCCESS);																\
	}																									\
}