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
	}

	void mainLoop()
	{
		while (!glfwWindowShouldClose(mWindow))
		{
			glfwPollEvents();
		}
	}

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
		createInfo.pfnCallback = (PFN_vkDebugReportCallbackEXT)debugCallback;

		if (CreateDebugReportCallbackEXT(mInstance, &createInfo, nullptr, &mCallback) != VK_SUCCESS) // calls CreateDebugReportCallbackEXT
		{
			throw std::runtime_error("Failed to setup debug callback.");
		}
	}

	//! explicitly loads a function from the Vulkan extension for use in our program
	VkResult CreateDebugReportCallbackEXT(
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

	GLFWwindow *mWindow;
	const int mWidth = 800;
	const int mHeight = 600;
	vk::Deleter<VkInstance> mInstance{vkDestroyInstance};
	vk::Deleter<VkDebugReportCallbackEXT> mCallback{ mInstance, DestroyDebugReportCallbackEXT };
	
	static VkBool32 debugCallback (
		VkDebugReportFlagsEXT flags,			// type of the message, i.e. VK_DEBUG_REPORT_INFORMATION_BIT_EXT
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