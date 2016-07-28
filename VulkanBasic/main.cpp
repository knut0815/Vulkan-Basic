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
		
		std::cout << "Available Vulkan extensions:" << std::endl;
		for (const auto& extension : extensions)
		{
			std::cout << "\t" << extension.extensionName << std::endl;
		}

		
		for (size_t i = 0; i < glfwExtensionCount; ++i)
		{
			auto predicate = [&](const VkExtensionProperties &prop) { return strcmp(glfwExtensions[i], prop.extensionName); };
			
			if (std::find_if(extensions.begin(), extensions.end(), predicate) == extensions.end())
			{
				throw std::runtime_error("One or more extensions required by GLFW are not supported.");
			}
		}
		std::cout << "All extensions required by GLFW are supported on this device." << std::endl;

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

	GLFWwindow *mWindow;
	const int mWidth = 800;
	const int mHeight = 600;
	vk::Deleter<VkInstance> mInstance{vkDestroyInstance};
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