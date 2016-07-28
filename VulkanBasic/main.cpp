#include "deleter.h"
#include "vulkan.h"
// glfw headers
#include "glfw3.h"

// glm headers
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "glm/vec4.hpp"
#include "glm/mat4x4.hpp"

// other
#include <iostream>
#include <stdexcept>
#include <vector>
#include <algorithm>

class BasicApp
{

public:

void run()
{
	initWindow();
	initVulkan();
	mainLoop();
}

~BasicApp()
{
	glfwDestroyWindow(mWindow);
	glfwTerminate();
}

private:

	void initWindow()
	{
		glfwInit();
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		mWindow = glfwCreateWindow(mWidth, mHeight, "Vulkan App", nullptr, nullptr);
	}

	void initVulkan()
	{
		createInstance();
		setupDebugCallback();
		pickPhysicalDevice();
		createLogicalDevice();
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(mWindow))
		{
			glfwPollEvents();
		}
	}

	//! a struct for determining which queue families a physical device supports
	struct QueueFamilyIndices
	{
		int graphicsFamily = -1;
		bool isComplete() { return graphicsFamily >= 0; }
	};

	//! create an instance, which includes extensions and optional validation layers
	void createInstance()
	{
		if (mEnableValidationLayers && !checkValidationLayerSupport())
		{
			throw std::runtime_error("One or more validation layers specified by this application are not supported.");
		}
		
		/*

		In Vulkan, an instance is the connection between your application and the Vulkan library
		and creating it involves specifying some details about your application to the driver.
		To create an instance, we first fill in a struct with some information about our application.
		This step is technically optional, but it may provide some useful information to the driver
		to optimize for our specific application.

		A lot of information in Vulkan is passed through structs instead of function parameters.

		The VkInstanceCreateInfo struct is not optional - it tells the Vulkan driver which global
		extensions and validation layers we want to use. Here, "global" means that they apply to the
		entire program and not a specific device.

		*/

		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.pApplicationName = "Hello Triangle";
		appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.pEngineName = "No Engine";
		appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		appInfo.apiVersion = VK_API_VERSION_1_0;

		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;

		/*

		The next two members of the VkInstanceCreateInfo struct specify the desired global
		extensions. Since Vulkan is platform agnostic, we need a special extension to interface with
		the windowing system. GLFW has a built-in function that returns the extension(s) it needs to
		do so.

		The last member (enabledLayerCount) of the struct determines the global validation layers to enable. 
		We leave that empty for now.

		*/

		auto extensions = getRequiredExtensions();
		createInfo.enabledExtensionCount = extensions.size();
		createInfo.ppEnabledExtensionNames = extensions.data();

		if (mEnableValidationLayers)
		{
			createInfo.enabledLayerCount = mValidationLayers.size();
			createInfo.ppEnabledLayerNames = mValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		/*

		We can now create our VkInstance. The general pattern that object creation function parameters
		in Vulkan follow is:

			1. Pointer to struct with creation info
			2. Pointer to custom allocator callbacks (always nullptr in this application)
			3. Pointer to the variable that stores the handle to the new object

		*/

		if(vkCreateInstance(&createInfo, nullptr, &mInstance) != VK_SUCCESS) // returns a VkResult
		{
			throw std::runtime_error("Failed to create VkInstance.");
		}

	}

	//! check if all of the request validation layers are installed on this system
	bool checkValidationLayerSupport()
	{
		/*
		
		The Vulkan API is designed around the idea of minimal driver overhead. As a result, there is very 
		limited error checking enabled by default. Validation layers are optional components that hook into
		Vulkan function calls to apply additional operations (i.e. checking the values of function parameters
		against the specification to detect misuse, tracking the creation and destruction of objects to find
		resource leaks, etc.).

		Vulkan does not come with any validation layers built-in, but the LunarG SDK provides a nice set of
		layers that will check common errors. Validation layers can only be used if they are installed on
		the system. A layer is enabled by specifying its name.

		We want to check if all of the requested validation layers are available on this system. To do
		this, we call vkEnumerateInstanceLayerProperties.

		*/

		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		std::vector<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char* layerName : mValidationLayers)
		{
			auto predicate = [&](const VkLayerProperties &prop) { return strcmp(layerName, prop.layerName); };
			if (std::find_if(availableLayers.begin(), availableLayers.end(), predicate) == availableLayers.end())
			{
				return false;
			}
		}
		return true;
	}

	//! retrieve the list of extensions required by GLFW
	std::vector<const char*> getRequiredExtensions()
	{
		/*
		
		Get the list of extensions that are required by GLFW. We optionally add the VK_EXT_debug_report extension,
		which lets us receive debug messages from Vulkan.

		*/

		std::vector<const char*> extensions;
		unsigned int glfwExtensionCount = 0;
		const char** glfwExtensions;
		glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		std::cout << "Adding extensions required by GLFW:" << std::endl;

		for (unsigned int i = 0; i < glfwExtensionCount; ++i)
		{
			std::cout << "\t" << glfwExtensions[i] << std::endl;
			extensions.push_back(glfwExtensions[i]);
		}

		if (mEnableValidationLayers)
		{
			extensions.push_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);
		}

		return extensions;
	}

	//! retrieve the list of all supported extensions on this system
	std::vector<VkExtensionProperties> getAvailableExtensions()
	{
		/*

		We might want to check for optional (non-essential) extensions. To retrieve a list of supported
		extensions before creating an instance, we call vkEnumerateInstanceExtensionProperties, which
		takes a pointer to a variable that stores the number of extensions and an array of VkExtensionProperties
		to store the details of the extensions. It also takes an optional first parameter that allows
		us to filter extensions by a specific validation layer, which we ignore for now.

		*/

		uint32_t extensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr); // find number of supported extensions

		std::vector<VkExtensionProperties> extensions(extensionCount);

		vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data()); // fill vector with extension data

		return extensions;
	}

	//! creates a callback object
	void setupDebugCallback()
	{	
		/*
		
		Even a debug callback in Vulkan is managed by a handle that needs to be explicitly created and 
		destroyed. First, we need to fill in a structure with details about the callback.

		The flags field allows you to filter which types of messages you would like to receive. The pfnCallback
		field specifies the pointer to the callback function. You can optionally pass a pointer to the pUserData
		field which will be passed along to the callback function via the userData parameter. You could use 
		this to pass a pointer to the application class, for example.

		This struct should be passed to the vkCreateDebugReportCallbackEXT function to create the VkDebugReportCallbackEXT
		object. Unfortunately, because this function is an extension function, it is not automatically loaded. We 
		have to look up its address ourselves using vkGetInstanceProcAddr. We use our own proxy function called
		CreateDebugReportCallbackEXT that handles this in the background.

		*/

		if (!mEnableValidationLayers) return;

		VkDebugReportCallbackCreateInfoEXT createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
		createInfo.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT;
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)sDebugCallback;

		if (CreateDebugReportCallbackEXT(mInstance, &createInfo, nullptr, &mCallback) != VK_SUCCESS) // calls CreateDebugReportCallbackEXT
		{
			throw std::runtime_error("Failed to setup debug callback.");
		}
	}

	//! explicitly loads a function from the Vulkan extension for creating a VkDebugReportCallbackEXT object
	static VkResult CreateDebugReportCallbackEXT(
		VkInstance instance,
		const VkDebugReportCallbackCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugReportCallbackEXT* pCallback)
	{
		auto func = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugReportCallbackEXT");
		if (func != nullptr)
		{
			return func(instance, pCreateInfo, pAllocator, pCallback);
		}
		else
		{
			return VK_ERROR_EXTENSION_NOT_PRESENT;
		}
	}

	//! explicitly loads a function from the Vulkan extension for cleaning up a VkDebugReportCallbackEXT object
	static void DestroyDebugReportCallbackEXT(
		VkInstance instance,
		VkDebugReportCallbackEXT callback,
		const VkAllocationCallbacks* pAllocator
		)
	{
		auto func = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugReportCallbackEXT");
		if (func != nullptr)
		{
			func(instance, callback, pAllocator);
		}
	}

	//! choose a physical device (GPU) to use for rendering
	void pickPhysicalDevice()
	{
		/*
		
		We need to look for and select a graphics card in the system that supports all of the features
		we need for rendering. We can select more than one graphics card, but in this example, we'll
		stick to the first one found.

		We call vkEnumeratePhysicalDevices twice: once to count the number of available devices (GPUs) 
		and once to fill a vector with all available VkPhysicalDevice handles. Then, we loop through
		each device to see if it is suitable for the operations we want to perform, since not all 
		graphics cards are created equal!

		*/

		uint32_t deviceCount = 0;
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);

		if (deviceCount == 0)
		{
			throw std::runtime_error("Failed to find GPUs with Vulkan support.");
		}

		std::vector<VkPhysicalDevice> devices(deviceCount);
		vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

		for (const auto& device : devices)
		{
			if (isDeviceSuitable(device))
			{
				mPhysicalDevice = device; // simply select the first suitable device
				break;
			}
		}

		if (mPhysicalDevice == VK_NULL_HANDLE)
		{
			throw std::runtime_error("Failed to find a suitable GPU.");
		}
	}

	//! given a device, determine whether it is suitable for rendering (mostly a placeholder at this point)
	bool isDeviceSuitable(VkPhysicalDevice device)
	{
		/*
		
		Basic device properties (name, type, and supported Vulkan version) can be queried using
		vkGetPhysicalDeviceProperties. The support for optional features (i.e. texture compression,
		64-bit floats, multi-viewport rendering for VR, etc.) can be queried with vkGetPhysicalDeviceFeatures.
		
		In a more advanced application, we could give each device a score and pick the highest one.
		That way we could favor a dedicated graphics card by giving it a high score. For now, we always 
		return true.

		*/
		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);
		
		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		// as an example, we test to see if we're using a discrete graphics card that supports geometry shaders
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU && deviceFeatures.geometryShader)
		{
			std::cout << "Geometry shaders are supported on this graphics card." << std::endl;
		}

		// make sure this device supports the graphics queue family
		QueueFamilyIndices indices = findQueueFamilies(device);

		return indices.isComplete();
	}

	//! determines which queue families the specified physical device supports
	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
	{
		/*
		
		Commands in Vulkan need to be submitted to a queue. There are different types of queues that 
		originate from different queue families, and each family of queues allows only a certain subset
		of commands. For example, there could be a queue family that only allows processing of compute
		commands or one that only allows memory transfer related commands.

		We need to check which queue families are supported by the device and which one of these supports
		the commands that we want to use. Right now, we'll only look for a queue that supports graphics
		commands.

		The VkQueueFamilyProperties struct contains some details about the queue family, including the
		type of operations that are supported and the number of queues that can be created based on that
		family.
			
		*/

		QueueFamilyIndices indices;

		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

		std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);

		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		int i = 0;
		for (const auto& queueFamily : queueFamilies)
		{
			if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
			{
				indices.graphicsFamily = i;
			}
			if (indices.isComplete())
			{
				break;
			}
			i++;
		}

		return indices;
	}

	void createLogicalDevice()
	{
		/*
		
		The creation of a logical device involves specifying a bunch of details in structs. VkDeviceQueueCreateInfo
		describes the number of queues we want for a single queue family. Right now we're only interested
		in a queue with graphics capabilities.

		Current drivers will only allow you to create a low number of queues within each queue family, but
		you don't really need more than one because you can create all of the command buffers on multiple
		threads and then submit them all at once on the main thread with a single low-overhead call. Vulkan
		also lets you assign priorities to queues to influence the scheduling of command buffer execution using
		floating point numbers between 0.0f and 1.0f. This is required even if there is only a single queue.
		
		Next, we need to specify the set of device features that we'll be using. These are the features
		that we queried support for with vkGetPhysicalDeviceFeatures. Right now, we can simply define it and
		leave everything to VK_FALSE.

		The rest of the VkDeviceCreateInfo struct looks similar to the VkInstanceCreateInfo struct and 
		requires you to specify extensions and validation layers. This time, however, they are device
		specific. An example of a device specific extension is VK_KHR_swapchain, which allows you to 
		present rendered images from that device to windows. It is possible that there are Vulkan devices 
		in the system that lack this ability, for example because they only support compute operations.

		*/

		QueueFamilyIndices indices = findQueueFamilies(mPhysicalDevice);

		float queuePriority = 1.0f;

		VkDeviceQueueCreateInfo queueCreateInfo = {};
		queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queueCreateInfo.queueFamilyIndex = indices.graphicsFamily;
		queueCreateInfo.queueCount = 1;
		queueCreateInfo.pQueuePriorities = &queuePriority;

		VkPhysicalDeviceFeatures deviceFeatures = {};

		VkDeviceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		createInfo.pQueueCreateInfos = &queueCreateInfo;
		createInfo.queueCreateInfoCount = 1;
		createInfo.pEnabledFeatures = &deviceFeatures;
		
		createInfo.enabledExtensionCount = 0;
		
		if (mEnableValidationLayers)
		{
			createInfo.enabledLayerCount = mValidationLayers.size();
			createInfo.ppEnabledLayerNames = mValidationLayers.data();
		}
		else
		{
			createInfo.enabledLayerCount = 0;
		}

		if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mDevice) != VK_SUCCESS)
		{
			throw std::runtime_error("Failed to create logical device.");
		}

		// pass the logical device, queue family, queue index, and a pointer to the variable where we'll store the queue handle
		// the index is 0 because we are only using one queue from this family
		vkGetDeviceQueue(mDevice, indices.graphicsFamily, 0, &mGraphicsQueue);
	}

	GLFWwindow *mWindow;
	const int mWidth = 800;
	const int mHeight = 600;
	vk::Deleter<VkInstance> mInstance{ vkDestroyInstance };
	vk::Deleter<VkDebugReportCallbackEXT> mCallback{ mInstance, DestroyDebugReportCallbackEXT };
	VkPhysicalDevice mPhysicalDevice{ VK_NULL_HANDLE };	// implicitly destroyed when the VkInstance is destroyed, so we don't need to add a delete wrapper
	vk::Deleter<VkDevice> mDevice{ vkDestroyDevice }; // needs to be declared below the VkInstance, since it must be destroyed before the instance is cleaned up
	VkQueue mGraphicsQueue; // automatically created and destroyed alongside the logical device

	static VkBool32 sDebugCallback (
		VkDebugReportFlagsEXT flags,			// type of the message, i.e. VK_DEBUG_REPORT_INFORMATION_BIT_EXT, VK_DEBUG_REPORT_WARNING_BIT_EXT, etc.
		VkDebugReportObjectTypeEXT objectType,	// type of the object that is the subject of the message, i.e. VK_DEBUG_REPORT_OBJECT_TYPE_DEVICE_EXT if object is a VkPhysicalDevice
		uint64_t object,						// handle to the actual Vulkan object, i.e. a VkPhysicalDevice
		size_t location,
		int32_t code,
		const char* layerPrefix,
		const char* messsage,					// the debug message
		void* userData)							// you can pass your own userData to the callback
	{
		std::cerr << "Validation layer: " << messsage << std::endl;
		return VK_FALSE;
	}

	const std::vector<const char*> mValidationLayers{ "VK_LAYER_LUNARG_standard_validation" }; // only active if we're in debug mode

#ifdef NDEBUG
	const bool mEnableValidationLayers = false;
#else
	const bool mEnableValidationLayers = true;
#endif

};

int main()
{
	BasicApp app;

	try
	{
		app.run();
	} 
	catch (const std::runtime_error &e)
	{
		std::cerr << e.what() << std::endl;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;

	uint32_t extensionCount = 0;
	vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
	std::cout << extensionCount << " extensions supported." << std::endl;

	return 0;
}	