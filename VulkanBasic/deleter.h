#pragma once

#include "vulkan.h"

#include <functional>

/*

Every Vulkan object needs to be explicitly destroyed with a function call when it is no longer needed.
This class takes advantage of the RAII principle to manage the lifetime of Vulkan resources. As an example, 
consider the case where we want to store a VkInstance object that should be destroyed with vkDestroyInstance. 
We would write:

	vk::Deleter<VkInstance> instance{vkDestroyInstance};

*/
namespace vk
{
	template<typename T>
	class Deleter
	{
	public:
		Deleter() : Deleter([](T t) {}) {}

		//! used for vkDestroyXXX(object, callbacks) variants
		Deleter(std::function<void(T, VkAllocationCallbacks*)> deletef) 
		{
			mObject = VK_NULL_HANDLE;
			mDeleter = [=](T obj) { deletef(obj, nullptr); };
		}

		//! used for vkDestroyXXX(instance, object, callbacks) variants
		Deleter(const Deleter<VkInstance>& instance, std::function<void(VkInstance, T, VkAllocationCallbacks*)> deletef) 
		{
			mObject = VK_NULL_HANDLE;
			mDeleter = [&instance, deletef](T obj) { deletef(instance, obj, nullptr); };
		}

		//! used for vkDestroyXXX(device, object, callbacks) variants
		Deleter(const Deleter<VkDevice>& device, std::function<void(VkDevice, T, VkAllocationCallbacks*)> deletef) 
		{
			mObject = VK_NULL_HANDLE;
			mDeleter = [&device, deletef](T obj) { deletef(device, obj, nullptr); };
		}

		~Deleter() 
		{
			cleanup();
		}

		T* operator &() 
		{
			cleanup();
			return &mObject;
		}

		operator T() const 
		{
			return mObject;
		}

	private:
		T mObject;
		std::function<void(T)> mDeleter;

		void cleanup()
		{
			if (mObject != VK_NULL_HANDLE)
			{
				mDeleter(mObject);
			}
			mObject = VK_NULL_HANDLE;
		}
	};
}